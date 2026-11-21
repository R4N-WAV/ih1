// V4 feedback round: ten new sound-design processors (Phase 2). Original IH1 designs. Product manuals and papers
// listed in the V4 research supplement were read as workflow / DSP references only; no code, presets or artwork
// were copied. Time-domain modules are in this file, the three STFT modules in V4Spectral.cpp.
//
//   Slice Shuffler     a sampler-style bar reconstruction: the previous bar (or beat / 2 bars) is cut into slices and
//                      replayed in a deterministic permutation (reverse order, shuffle, strides, palindrome, pairs,
//                      random walk), each slice with its own direction, pitch and gate. Crossfaded 2-voice playback.
//   Attack / Body      a transient mask splits the signal into an attack layer and a body layer that sum back to the
//                      input exactly; the attack can be delayed and repeated (flams, rolls), the body reversed, held
//                      (frozen between hits) or smeared.
//   Echo Matrix        an 8-step tap grid over a tempo cycle: each tap has its own level (pattern), pitch (rotating-
//                      head shifter), pan and darkening; the cycle feeds back through a filter and a saturator.
//   Reverse Pitch Echo overlapping reversed grains read at a pitch ratio, inside a feedback loop: every pass returns
//                      backwards and another interval higher / lower (cascading reverse-shift echoes).
//   Dispersion         a cascade of up to 64 second-order all-pass sections spread across the spectrum: pure phase
//                      (no magnitude change at 100 % mix), frequency-dependent delay that turns attacks into chirps.
//   Modal Resonator    up to 24 tuned two-pole modes with material ratio sets (bar, bell, glass, plate, membrane...):
//                      the input (or only its transients) strikes a synthetic object.
//   Texture Imprint    source-following particulate texture (dust, crackle, static bursts, friction, grit); silent
//                      input = silent texture, deterministic per seed.
//
// All modules: allocation only in onPrepare, bounded voices and feedback (soft-limited), every boundary windowed or
// crossfaded (V4 Phase 4), deterministic randomness from a preset-owned seed, no reported latency (the delays here are
// intentional echoes / capture time, not plug-in latency).
#include <array>
#include <cmath>

#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

inline float softLimit(float x, float knee = 1.2f) {
    const float a = std::fabs(x);
    return a <= knee ? x : std::copysign(knee + std::tanh(a - knee), x);
}
inline float rcos(float u) { return 0.5f - 0.5f * std::cos(dsp::kPi * std::clamp(u, 0.0f, 1.0f)); }  // 0 -> 1, zero slope at both ends
inline float hann(float u) { return 0.5f - 0.5f * std::cos(dsp::kTwoPi * u); }                        // 0..1..0 over u in [0,1]

inline uint32_t mix32(uint32_t a, uint32_t b) {
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    h ^= h >> 16; h *= 0x85EBCA6Bu; h ^= h >> 13; h *= 0xC2B2AE35u; h ^= h >> 16;
    return h;
}
inline float unit(uint32_t h) { return static_cast<float>(h & 0xFFFFFF) / 16777216.0f; }

// Absolute-position ring buffer (power of two) with Hermite reads.
struct Ring {
    std::vector<float> b;
    int64_t mask = 0;
    void prepare(double seconds, double sr) {
        int64_t size = 1;
        while (size < static_cast<int64_t>(seconds * sr) + 8) size <<= 1;
        b.assign(static_cast<size_t>(size), 0.0f);
        mask = size - 1;
    }
    void clear() { std::fill(b.begin(), b.end(), 0.0f); }
    void put(int64_t pos, float x) { b[static_cast<size_t>(pos & mask)] = x; }
    float at(double pos) const {
        const double fl = std::floor(pos);
        const int64_t i = static_cast<int64_t>(fl);
        const float f = static_cast<float>(pos - fl);
        const float y0 = b[static_cast<size_t>((i - 1) & mask)], y1 = b[static_cast<size_t>(i & mask)];
        const float y2 = b[static_cast<size_t>((i + 1) & mask)], y3 = b[static_cast<size_t>((i + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
    double span() const { return static_cast<double>(mask) - 8.0; }
};

// Tempo grid position in samples (host timeline while it plays, a free clock otherwise).
inline double gridSamples(const ProcessContext& ctx, int64_t freeClock, int i, double sr) {
    if (ctx.ppqValid && ctx.hostPlaying) return (ctx.ppqPosition * 60.0 / std::clamp(ctx.bpm, 20.0, 999.0)) * sr + i;
    return static_cast<double>(freeClock + i);
}

// ====================================================================================================== Slice Shuffler
enum { ssLength, ssSlices, ssPattern, ssEvolve, ssSeed, ssReverse, ssPitch, ssPitchAmt, ssGate, ssSwing, ssFade, ssMix };

class SliceShuffler final : public FxBase {
    static constexpr int kMaxSlices = 32;
    struct Voice {
        bool on = false, live = false;
        double src = 0.0;   // absolute ring position of the slice start
        double t = 0.0;     // samples since the voice started
        double len = 1.0;   // slice length (samples)
        float rate = 1.0f;
        bool rev = false;
        float gateLen = 1.0f;  // samples of the slice that sound before the gate closes
        float fadeIn = 1.0f, fadeOut = 0.0f;  // fadeOut > 0: releasing over that many samples
        float fadeLen = 64.0f;                // edge fade latched when the voice starts (a control change never rescales it)
        float relT = 0.0f;
    };
    void onPrepare() override {
        ring_[0].prepare(17.0, sr_);
        ring_[1].prepare(17.0, sr_);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
    }
    void reset() override {
        for (auto& r : ring_) r.clear();
        abs_ = 0;
        clock_ = 0;
        lastStep_ = INT64_MIN;
        cur_ = Voice{};
        old_ = Voice{};
        cur_.on = true;
        cur_.live = true;
        lastBar_ = -1.0;
        mix_.reset();
    }
    void applyParams(bool snap) override { mix_.setMix(pct(ssMix), snap); }
    double tailSeconds() const override { return 0.2; }

    int sourceSlice(int step, int n, uint32_t seed) const {
        switch (ip(ssPattern)) {
            case 0: return n - 1 - step;                                   // reverse order
            case 1: {                                                      // seeded shuffle (Fisher-Yates over n)
                std::array<int, kMaxSlices> p{};
                for (int i = 0; i < n; ++i) p[static_cast<size_t>(i)] = i;
                for (int i = n - 1; i > 0; --i) std::swap(p[static_cast<size_t>(i)], p[static_cast<size_t>(mix32(seed, static_cast<uint32_t>(i)) % static_cast<uint32_t>(i + 1))]);
                return p[static_cast<size_t>(step)];
            }
            case 2: return (step * 3) % n;                                 // stride 3
            case 3: return (step * 5) % n;                                 // stride 5
            case 4: return step < n / 2 ? step : n - 1 - step;             // palindrome: there and back
            case 5: return step - (step % 2);                              // pairs: every slice twice
            case 6: {                                                      // random walk
                int s = 0;
                for (int i = 1; i <= step; ++i) {
                    const float r = unit(mix32(seed, static_cast<uint32_t>(i) * 31u));
                    s += r < 0.35f ? -1 : (r < 0.7f ? 1 : (r < 0.85f ? 2 : -2));
                    s = ((s % n) + n) % n;
                }
                return s;
            }
            case 7: return (step / 4) * 4 + ((step % 4) == 3 ? 3 : 0);    // stutter: hold the first of each four
            default: return step;
        }
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        static const double kBeats[4] = {1.0, 2.0, 4.0, 8.0};
        static const int kSl[4] = {4, 8, 16, 32};
        const double bpm = std::clamp(ctx.bpm, 40.0, 300.0);
        const double barLen = kBeats[std::clamp(ip(ssLength), 0, 3)] * 60.0 / bpm * sr_;
        const int N = kSl[std::clamp(ip(ssSlices), 0, 3)];
        const double sliceLen = barLen / N;
        const float fade = std::max(16.0f, std::min(fp(ssFade) * 0.001f * srf(), static_cast<float>(0.25 * sliceLen)));
        const float swing = pct(ssSwing) * 0.5f;
        const uint32_t seedBase = static_cast<uint32_t>(ip(ssSeed)) * 7919u + 17u;
        const bool evolve = bp(ssEvolve);
        static const float kPitchSets[5][4] = {{0, 0, 0, 0}, {12, -12, 12, -12}, {7, -5, 12, -12}, {3, 7, -2, -5}, {5, -7, 11, -13}};
        const int pset = std::clamp(ip(ssPitch), 0, 4);
        for (int i = 0; i < n; ++i) {
            const float inL = ch[0][i], inR = ch[1][i];
            ring_[0].put(abs_, inL);
            ring_[1].put(abs_, inR);
            const double g = gridSamples(ctx, clock_, i, sr_);
            const double bar = std::floor(g / barLen);
            const double inBar = g - bar * barLen;
            // swing: odd steps start later by swing x half a slice
            int step = static_cast<int>(inBar / sliceLen);
            step = std::clamp(step, 0, N - 1);
            if ((step % 2) == 1 && inBar - step * sliceLen < swing * sliceLen) step -= 1;
            const int64_t stepKey = static_cast<int64_t>(bar) * kMaxSlices + step;
            if (stepKey != lastStep_) {
                lastStep_ = stepKey;
                // a full previous bar must exist in the ring (after a reset / at start: pass the input through)
                const double barStartAbs = static_cast<double>(abs_) - inBar;
                const bool havePrev = barStartAbs - barLen >= 8.0 && barLen < ring_[0].span() * 0.45;
                const uint32_t seed = evolve ? mix32(seedBase, static_cast<uint32_t>(static_cast<int64_t>(bar))) : seedBase;
                old_ = cur_;
                if (old_.on && old_.fadeOut <= 0.0f) { old_.fadeOut = old_.fadeLen; old_.relT = 0.0f; }  // an already-releasing voice keeps its release
                cur_ = Voice{};
                cur_.on = true;
                cur_.fadeIn = 0.0f;
                cur_.fadeLen = fade;
                if (!havePrev) {
                    cur_.live = true;
                } else {
                    const int src = sourceSlice(step, N, seed);
                    cur_.src = barStartAbs - barLen + src * sliceLen;
                    cur_.len = sliceLen;
                    const uint32_t h = mix32(seed ^ 0xA5u, static_cast<uint32_t>(step));
                    cur_.rev = unit(h) < pct(ssReverse);
                    const bool pitched = pset > 0 && unit(mix32(h, 3u)) < pct(ssPitchAmt);
                    cur_.rate = pitched ? std::exp2(kPitchSets[pset][(h >> 7) & 3] / 12.0f) : 1.0f;
                    // the slice may start a little late (swing), it still plays its own length
                    cur_.gateLen = std::max(2.0f * fade, static_cast<float>(sliceLen) * std::max(0.05f, pct(ssGate)));
                }
            }
            float yl = 0.0f, yr = 0.0f;
            auto render = [&](Voice& v, float& ol, float& orr) {
                if (!v.on) { ol = orr = 0.0f; return; }
                float gEnv = rcos(v.fadeIn / v.fadeLen);
                if (v.fadeIn < v.fadeLen) v.fadeIn += 1.0f;
                if (v.fadeOut > 0.0f) {
                    gEnv *= 1.0f - rcos(v.relT / v.fadeOut);
                    v.relT += 1.0f;
                    if (v.relT >= v.fadeOut) v.on = false;
                } else if (!v.live && static_cast<float>(v.t) >= v.gateLen) {
                    v.fadeOut = v.fadeLen;  // the step's gate closes (raised-cosine release)
                    v.relT = 0.0f;
                }
                if (v.live) { ol = inL * gEnv; orr = inR * gEnv; return; }
                double p = v.rev ? v.src + v.len - 1.0 - v.t * v.rate : v.src + v.t * v.rate;
                p = std::min(p, static_cast<double>(abs_) - 4.0);  // never the future
                ol = ring_[0].at(p) * gEnv;
                orr = ring_[1].at(p) * gEnv;
                v.t += 1.0;
            };
            float a, b;
            render(cur_, a, b);
            yl += a; yr += b;
            render(old_, a, b);
            yl += a; yr += b;
            ch[0][i] = yl;
            ch[1][i] = yr;
            ++abs_;
        }
        clock_ += n;
        mix_.apply(ch, n);
    }
    Ring ring_[2];
    int64_t abs_ = 0, clock_ = 0, lastStep_ = INT64_MIN;
    double lastBar_ = -1.0;
    Voice cur_, old_;
    MixStage mix_;
};

// ===================================================================================================== Attack / Body
enum { abSens, abLength, abAttGain, abAttDelay, abRepeats, abSpacing, abRepDecay, abBodyGain, abBodyMode, abBodyLen, abBodyTone, abMix };

class AttackBody final : public FxBase {
    void onPrepare() override {
        for (auto& r : att_) r.prepare(0.9, sr_);
        for (auto& r : body_) r.prepare(4.5, sr_);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        attG_.prepare(sr_, 25.0f);
        bodyG_.prepare(sr_, 25.0f);
        dlyS_.prepare(sr_, 40.0f);
        spS_.prepare(sr_, 40.0f);
        repS_.prepare(sr_, 60.0f);
        static const float ms[4] = {13.7f, 23.1f, 37.9f, 51.3f};
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k) ap_[c][k].prepare(static_cast<int>(ms[k] * 0.001f * srf()));  // identical L/R: mono-safe
    }
    void reset() override {
        for (auto& r : att_) r.clear();
        for (auto& r : body_) r.clear();
        for (auto& c : ap_) for (auto& a : c) a.reset();
        for (auto& f : tone_) f.reset();
        fast_ = slow_ = mask_ = 0.0f;
        abs_ = 0;
        holdPos_ = -1.0;
        gPh_ = 0.0f;
        holdAmt_ = 0.0f;
        modeAmt_.fill(0.0f);
        modeAmt_[0] = 1.0f;
        rph_[0] = rph_[1] = 0.0;
        rlen_[0] = rlen_[1] = -1.0;
        repS_.snap(std::clamp(fp(abRepeats), 0.0f, 6.0f));
        mix_.reset();
        lastHit_ = -1000000;
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(abMix), snap);
        attG_.setTarget(dsp::dbToGain(fp(abAttGain)));
        bodyG_.setTarget(dsp::dbToGain(fp(abBodyGain)));
        dlyS_.setTarget(fp(abAttDelay) * 0.001f * srf());
        spS_.setTarget(fp(abSpacing) * 0.001f * srf());
        if (snap) { attG_.snap(attG_.target()); bodyG_.snap(bodyG_.target()); dlyS_.snap(dlyS_.target()); spS_.snap(spS_.target()); }
        for (auto& f : tone_) f.setup(dsp::Svf::Lowpass, std::min(fp(abBodyTone), 0.45f * srf()), 0.6f, 0.0f, sr_);
        toneAmt_ = std::clamp((20000.0f - fp(abBodyTone)) / 1000.0f, 0.0f, 1.0f);
    }
    double tailSeconds() const override { return 0.9 + 4.0; }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float aF = 1.0f - std::exp(-1.0f / (0.0003f * srf())), rF = 1.0f - std::exp(-1.0f / (0.008f * srf()));
        const float aS = 1.0f - std::exp(-1.0f / (0.015f * srf())), rS = 1.0f - std::exp(-1.0f / (0.12f * srf()));
        const float ratio = 1.1f + 2.9f * (1.0f - pct(abSens));        // fast must exceed slow by this factor
        const float rel = 1.0f - std::exp(-1.0f / (std::max(2.0f, fp(abLength)) * 0.001f * srf()));
        const float att = 1.0f - std::exp(-1.0f / (0.0002f * srf()));
        repS_.setTarget(std::clamp(fp(abRepeats), 0.0f, 6.0f));
        const float repDecay = pct(abRepDecay);
        const int mode = std::clamp(ip(abBodyMode), 0, 3);  // 0 natural, 1 reversed, 2 held, 3 smeared
        const double chunk = std::max(0.03, fp(abBodyLen) * 0.001) * sr_;
        const float grain = static_cast<float>(std::min(chunk, 0.12 * sr_));
        for (int i = 0; i < n; ++i) {
            const float x0 = ch[0][i], x1 = ch[1][i];
            const float m = 0.5f * (std::fabs(x0) + std::fabs(x1));
            fast_ += (m > fast_ ? aF : rF) * (m - fast_);
            slow_ += (m > slow_ ? aS : rS) * (m - slow_);
            const bool hit = fast_ > ratio * slow_ + 1e-4f;
            // mask: rises in ~0.2 ms on a transient, releases over Attack Length (sums back to the input exactly)
            mask_ += (hit ? att : rel) * ((hit ? 1.0f : 0.0f) - mask_);
            if (hit && abs_ - lastHit_ > static_cast<int64_t>(0.03 * sr_)) { lastHit_ = abs_; holdPos_ = static_cast<double>(abs_) - 0.02 * sr_; }
            const float a0 = x0 * mask_, a1 = x1 * mask_;
            const float b0 = x0 - a0, b1 = x1 - a1;
            att_[0].put(abs_, a0);
            att_[1].put(abs_, a1);
            body_[0].put(abs_, b0);
            body_[1].put(abs_, b1);
            // ---- attack layer: delayed, repeated
            const float dly = dlyS_.next(), sp = std::max(1.0f, spS_.next());
            const float ga = attG_.next();
            float al = 0.0f, ar = 0.0f, g = 1.0f;
            const float reps = repS_.next();  // continuous: a new repeat fades in instead of appearing
            for (int r = 0; r <= 6; ++r) {
                const float w = std::clamp(reps - static_cast<float>(r) + 1.0f, 0.0f, 1.0f);
                if (w <= 0.0f) break;
                const double back = static_cast<double>(dly) + r * sp;
                if (back < 2.0) {
                    // (near) zero delay: the current sample itself, blended into the ring read by 2 samples (exact at 0)
                    const float u = static_cast<float>(back / 2.0);
                    const double p = static_cast<double>(abs_) - 2.0;
                    al += w * g * ((1.0f - u) * a0 + u * (p >= 4.0 ? att_[0].at(p) : 0.0f));
                    ar += w * g * ((1.0f - u) * a1 + u * (p >= 4.0 ? att_[1].at(p) : 0.0f));
                } else {
                    const double p = static_cast<double>(abs_) - back;
                    if (p < 4.0) break;
                    al += w * g * att_[0].at(p);
                    ar += w * g * att_[1].at(p);
                }
                g *= repDecay;
            }
            // ---- body layer: every mode runs, each blended by its own 20 ms ramp (mode changes never step)
            const float bstep = 1.0f / (0.02f * srf());
            for (int k = 0; k < 4; ++k) modeAmt_[static_cast<size_t>(k)] = k == mode ? std::min(1.0f, modeAmt_[static_cast<size_t>(k)] + bstep) : std::max(0.0f, modeAmt_[static_cast<size_t>(k)] - bstep);
            float bl = modeAmt_[0] * b0, br = modeAmt_[0] * b1;
            gPh_ += 1.0f / grain;
            if (gPh_ >= 1.0f) gPh_ -= 1.0f;
            // reversed chunks: two half-overlapped backwards readers, each latching the chunk length at its own wrap
            for (int v = 0; v < 2; ++v) {
                double& ph = rph_[v];
                if (rlen_[v] <= 0.0) { rlen_[v] = chunk; ph = v * 0.5 * chunk; }
                if (modeAmt_[1] > 0.0f) {
                    const float w = hann(static_cast<float>(ph / rlen_[v]));
                    const double p = static_cast<double>(abs_) - 2.0 * ph - 2.0;
                    if (p >= 4.0) {
                        bl += modeAmt_[1] * w * body_[0].at(p);
                        br += modeAmt_[1] * w * body_[1].at(p);
                    }
                }
                ph += 1.0;
                if (ph >= rlen_[v]) { ph -= rlen_[v]; rlen_[v] = chunk; }
            }
            if (modeAmt_[2] > 0.0f && holdPos_ > 4.0) {  // held: grains loop around the last hit's body (frozen sustain)
                for (int v = 0; v < 2; ++v) {
                    float ph = gPh_ + 0.5f * v;
                    ph -= std::floor(ph);
                    const float w = hann(ph);
                    const double p = std::min(holdPos_ + 0.02 * sr_ + ph * grain, static_cast<double>(abs_) - 4.0);
                    bl += modeAmt_[2] * w * body_[0].at(p) * 1.2f;
                    br += modeAmt_[2] * w * body_[1].at(p) * 1.2f;
                }
            }
            if (modeAmt_[3] > 0.0f) {  // smeared: a long all-pass diffuser
                float d0 = b0, d1 = b1;
                for (auto& a : ap_[0]) d0 = a.process(d0, 0.68f);
                for (auto& a : ap_[1]) d1 = a.process(d1, 0.68f);
                bl += modeAmt_[3] * d0;
                br += modeAmt_[3] * d1;
            } else {
                // keep the diffuser warm but silent so it can blend in without a cold start
                for (auto& a : ap_[0]) a.process(0.0f, 0.68f);
                for (auto& a : ap_[1]) a.process(0.0f, 0.68f);
            }
            const float gb = bodyG_.next();
            // tone: fully out at 20 kHz (the default), blended in below 19 kHz, so the neutral split stays exact
            const float tl = tone_[0].process(bl), tr = tone_[1].process(br);
            bl = (bl + toneAmt_ * (tl - bl)) * gb;
            br = (br + toneAmt_ * (tr - br)) * gb;
            ch[0][i] = softLimit(al * ga + bl, 2.0f);
            ch[1][i] = softLimit(ar * ga + br, 2.0f);
            ++abs_;
        }
        for (auto& f : tone_) f.flush();
        mix_.apply(ch, n);
    }
    Ring att_[2], body_[2];
    dsp::Allpass ap_[2][4];
    dsp::Svf tone_[2];
    dsp::Smoother attG_, bodyG_, dlyS_, spS_, repS_;
    double rph_[2] = {0, 0}, rlen_[2] = {-1, -1};
    float toneAmt_ = 0.0f;
    float fast_ = 0, slow_ = 0, mask_ = 0, gPh_ = 0, holdAmt_ = 0;
    std::array<float, 4> modeAmt_{};
    int64_t abs_ = 0, lastHit_ = -1000000;
    double holdPos_ = -1.0;
    MixStage mix_;
};

// ======================================================================================================= Echo Matrix
enum { emDiv, emPattern, emDecay, emInterval, emPitchMode, emSwing, emSpread, emTone, emDarken, emFeedback, emPitchLoop, emDrive, emMix };
constexpr int kTaps = 8;

class EchoMatrix final : public FxBase {
    void onPrepare() override {
        ring_[0].prepare(9.0, sr_);
        ring_[1].prepare(9.0, sr_);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
        fbS_.prepare(sr_, 30.0f);
        win_ = static_cast<float>(0.045 * sr_);  // pitch-shifter head window
        smoothA_ = 1.0f - std::exp(-1.0f / (0.005f * srf()));
    }
    void reset() override {
        for (auto& r : ring_) r.clear();
        abs_ = 0;
        step_ = -1.0;
        for (auto& p : ph_) p = 0.0f;
        for (auto& z : lp_) z = 0.0f;
        for (auto& z : fbLp_) z = 0.0f;
        for (auto& z : fbHp_) z = 0.0f;
        fbState_[0] = fbState_[1] = 0.0f;
        lvlInit_ = false;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        if (snap) lvlInit_ = false;
        mix_.setMix(pct(emMix), snap);
        fbS_.setTarget(std::min(0.92f, pct(emFeedback) * 0.92f));
        if (snap) fbS_.snap(fbS_.target());
    }
    double tailSeconds() const override { return 8.0 + 20.0 * pct(emFeedback); }
    float level(int pattern, int tap, uint32_t seed) const {
        static const float kP[7][kTaps] = {
            {1, 1, 1, 1, 1, 1, 1, 1},          // straight
            {0, 0, 1, 0, 0, 1, 0, 1},          // dotted / 3-3-2 feel
            {0, 1, 0, 1, 0, 1, 0, 1},          // off-beats
            {1, 0, 1, 1, 0, 1, 1, 0},          // gallop
            {1, 0, 0, 1, 0, 0, 1, 0},          // clave-ish triplet grid
            {0, 1, 0, 0, 0, 0, 1, 0},          // sparse
            {1, 0.8f, 0.6f, 0.45f, 0.35f, 0.25f, 0.18f, 0.12f},  // cascade
        };
        if (pattern >= 7) return unit(mix32(seed, static_cast<uint32_t>(tap))) < 0.5f ? 0.0f : 0.6f + 0.4f * unit(mix32(seed, 99u + tap));
        return kP[std::clamp(pattern, 0, 6)][tap];
    }
    float pitchOf(int mode, int tap, float interval) const {
        switch (mode) {
            case 1: return interval * static_cast<float>(tap + 1);                      // climb
            case 2: return (tap % 2 ? -1.0f : 1.0f) * interval;                         // zigzag
            case 3: return (tap % 2) ? 12.0f : 0.0f;                                     // octave on odd taps
            case 4: return (tap % 3 == 2) ? -12.0f : ((tap % 3 == 1) ? interval : 0.0f);  // shadow
            default: return 0.0f;
        }
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const double stepT = std::min(1.0, dsp::divisionSeconds(ip(emDiv), ctx.bpm)) * sr_;
        if (step_ < 0.0) step_ = stepT;
        const int pattern = ip(emPattern);
        const uint32_t seed = 0xE40u + static_cast<uint32_t>(pattern);
        const float decay = pct(emDecay);
        const float interval = fp(emInterval);
        const int pmode = ip(emPitchMode);
        const float swing = pct(emSwing) * 0.5f;
        const float spread = pct(emSpread);
        const float darken = pct(emDarken);
        const float tone = fp(emTone);
        const float driveT = 1.0f + 5.0f * pct(emDrive);
        const bool pitchLoop = bp(emPitchLoop);
        float tapLvl[kTaps], tapPan[kTaps][2], tapRate[kTaps], tapA[kTaps];
        int lastTap = 0;
        for (int t = 0; t < kTaps; ++t) {
            tapLvl[t] = level(pattern, t, seed) * std::pow(decay, static_cast<float>(t));
            if (tapLvl[t] > 0.0f) lastTap = t;  // the cycle's feedback source: fixed by the pattern, never by history
            const float pan = (t % 2 ? 1.0f : -1.0f) * spread * (0.4f + 0.6f * t / 7.0f);
            dsp::panGains(pan, tapPan[t][0], tapPan[t][1]);
            tapPan[t][0] *= 1.41421356f;
            tapPan[t][1] *= 1.41421356f;
            tapRate[t] = std::exp2(pitchOf(pmode, t, interval) / 12.0f);
            const float fc = std::max(200.0f, tone * std::pow(1.0f - 0.6f * darken, static_cast<float>(t)));
            tapA[t] = 1.0f - std::exp(-dsp::kTwoPi * std::min(fc, 0.45f * srf()) / srf());
        }
        if (!lvlInit_) {
            lvlInit_ = true;
            for (int t = 0; t < kTaps; ++t) { lvlCur_[t] = tapLvl[t]; panCur_[t][0] = tapPan[t][0]; panCur_[t][1] = tapPan[t][1]; }
            driveCur_ = driveT;
        }
        const float fbA = 1.0f - std::exp(-dsp::kTwoPi * std::min(tone, 0.45f * srf()) / srf());
        const float hpA = 1.0f - std::exp(-dsp::kTwoPi * 80.0f / srf());
        for (int i = 0; i < n; ++i) {
            // the step length slews (<= 0.5 samples / sample): tempo or division changes bend, never jump
            step_ += std::clamp(stepT - step_, -0.5 / kTaps, 0.5 / kTaps);
            const float fb = fbS_.next();
            // per-sample smoothing of tap levels, pans and drive (~5 ms): pattern-independent controls glide
            const float sa = smoothA_;
            for (int t = 0; t < kTaps; ++t) {
                lvlCur_[t] += sa * (tapLvl[t] - lvlCur_[t]);
                panCur_[t][0] += sa * (tapPan[t][0] - panCur_[t][0]);
                panCur_[t][1] += sa * (tapPan[t][1] - panCur_[t][1]);
            }
            driveCur_ += sa * (driveT - driveCur_);
            const float drive = driveCur_;
            for (int c = 0; c < 2; ++c) ring_[c].put(abs_, ch[c][i] + fbState_[c]);
            float out[2] = {0.0f, 0.0f}, last[2] = {0.0f, 0.0f}, lastRaw[2] = {0.0f, 0.0f};
            for (int t = 0; t < kTaps; ++t) {
                const double D = step_ * (t + 1) + ((t % 2) == 0 ? swing * step_ : 0.0);
                if (tapLvl[t] <= 0.0f && t != lastTap) { for (int h = 0; h < 1; ++h) { ph_[t] += (1.0f - tapRate[t]) / win_; ph_[t] -= std::floor(ph_[t]); } continue; }
                // rotating two-head pitch shifter around the tap (heads crossfaded with a Hann window)
                const float r = tapRate[t];
                float s[2] = {0.0f, 0.0f}, raw[2] = {0.0f, 0.0f};
                {
                    for (int h = 0; h < 2; ++h) {
                        float p = ph_[t] + 0.5f * h;
                        p -= std::floor(p);
                        const float w = hann(p);
                        const double d = D + static_cast<double>(p) * win_;
                        for (int c = 0; c < 2; ++c) s[c] += w * ring_[c].at(static_cast<double>(abs_) - d);
                    }
                    for (int c = 0; c < 2; ++c) raw[c] = ring_[c].at(static_cast<double>(abs_) - D);
                    // Pitch Pattern Off uses the plain read; any pitched pattern always uses the shifter (no switching
                    // mid-signal, even when a tap passes through 0 st)
                    if (pmode == 0) for (int c = 0; c < 2; ++c) s[c] = raw[c];
                    ph_[t] += (1.0f - r) / win_;
                    ph_[t] -= std::floor(ph_[t]);
                }
                for (int c = 0; c < 2; ++c) {
                    float& z = lp_[t * 2 + c];
                    z += tapA[t] * (s[c] - z);
                    out[c] += lvlCur_[t] * panCur_[t][c] * z;
                }
                if (t == lastTap) { last[0] = s[0]; last[1] = s[1]; lastRaw[0] = raw[0]; lastRaw[1] = raw[1]; }
            }
            // feedback: the last sounding tap of the cycle (pitched or not), filtered and saturated
            for (int c = 0; c < 2; ++c) {
                float f = pitchLoop ? last[c] : lastRaw[c];
                fbLp_[c] += fbA * (f - fbLp_[c]);
                fbHp_[c] += hpA * (fbLp_[c] - fbHp_[c]);
                f = fbLp_[c] - fbHp_[c];
                f = std::tanh(f * drive) / drive;
                fbState_[c] = softLimit(f * fb, 1.0f);
                ch[c][i] = softLimit(out[c], 2.0f);
            }
            ++abs_;
        }
        mix_.apply(ch, n);
    }
    Ring ring_[2];
    int64_t abs_ = 0;
    double step_ = -1.0;
    float win_ = 2000.0f;
    std::array<float, kTaps> ph_{};
    std::array<float, kTaps * 2> lp_{};
    float fbLp_[2] = {0, 0}, fbHp_[2] = {0, 0}, fbState_[2] = {0, 0};
    float lvlCur_[kTaps] = {}, panCur_[kTaps][2] = {}, driveCur_ = 1.0f, smoothA_ = 0.01f;
    bool lvlInit_ = false;
    dsp::Smoother fbS_;
    MixStage mix_;
};

// ================================================================================================ Reverse Pitch Echo
enum { rpInterval, rpFine, rpSync, rpLength, rpDiv, rpDelay, rpDirection, rpFeedback, rpTone, rpLowCut, rpSpread, rpMix };

class ReversePitchEcho final : public FxBase {
    struct Grain { bool on = false; double start = 0.0; float t = 0.0f, len = 1.0f, rate = 1.0f; bool rev = true; };
    void onPrepare() override {
        ring_[0].prepare(9.0, sr_);
        ring_[1].prepare(9.0, sr_);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
        fbS_.prepare(sr_, 30.0f);
    }
    void reset() override {
        for (auto& r : ring_) r.clear();
        for (auto& c : g_) for (auto& g : c) g = Grain{};
        abs_ = 0;
        next_[0] = next_[1] = 0.0;
        count_ = 0;
        for (int c = 0; c < 2; ++c) { lp_[c].reset(); hp_[c].reset(); fb_[c] = 0.0f; }
        mix_.reset();
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(rpMix), snap);
        fbS_.setTarget(std::min(0.9f, pct(rpFeedback) * 0.9f));
        if (snap) fbS_.snap(fbS_.target());
        for (int c = 0; c < 2; ++c) {
            lp_[c].setup(dsp::Svf::Lowpass, fp(rpTone), 0.6f, 0.0f, sr_);
            hp_[c].setup(dsp::Svf::Highpass, fp(rpLowCut), 0.6f, 0.0f, sr_);
        }
    }
    double tailSeconds() const override { return 4.0 + 20.0 * pct(rpFeedback); }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const double len = std::clamp(syncedSeconds(bp(rpSync), fp(rpLength), ip(rpDiv), ctx.bpm), 0.03, 2.0) * sr_;
        const float rate = std::exp2((fp(rpInterval) + fp(rpFine) * 0.01f) / 12.0f);
        const double pre = fp(rpDelay) * 0.001 * sr_;
        const int dir = ip(rpDirection);  // 0 reverse, 1 forward, 2 alternate
        const float spread = pct(rpSpread);
        for (int i = 0; i < n; ++i) {
            fbS_.next();
            for (int c = 0; c < 2; ++c) ring_[c].put(abs_, ch[c][i] + fb_[c]);
            float out[2] = {0.0f, 0.0f};
            for (int c = 0; c < 2; ++c) {
                // a new grain every half grain length (two overlapping Hann grains = constant overlap-add)
                if (static_cast<double>(abs_) >= next_[c]) {
                    const double off = c == 1 ? spread * 0.25 * len : 0.0;
                    next_[c] = static_cast<double>(abs_) + 0.5 * len;
                    // four slots; a sounding grain is never stolen (a shorter Length can overlap three grains briefly)
                    Grain* slot = nullptr;
                    for (auto& gg : g_[c]) if (!gg.on) { slot = &gg; break; }
                    Grain g;
                    g.on = true;
                    g.len = static_cast<float>(len);
                    g.rate = rate * (c == 1 ? std::exp2(spread * 0.15f / 12.0f) : 1.0f);
                    g.rev = dir == 0 || (dir == 2 && (count_ % 2) == 0);
                    // reverse: start at "now - pre-delay" and walk back; forward: start far enough back to never
                    // reach the write head
                    const double reach = static_cast<double>(g.rate) * len;
                    g.start = static_cast<double>(abs_) - pre - off - (g.rev ? 2.0 : reach + 2.0);
                    if (slot && g.start - reach > 4.0) *slot = g;
                    if (c == 1) ++count_;
                }
                for (auto& g : g_[c]) {
                    if (!g.on) continue;
                    const float u = g.t / g.len;
                    const double p = g.rev ? g.start - static_cast<double>(g.t) * g.rate : g.start + static_cast<double>(g.t) * g.rate;
                    out[c] += hann(u) * ring_[c].at(std::min(p, static_cast<double>(abs_) - 2.0));
                    g.t += 1.0f;
                    if (g.t >= g.len) g.on = false;
                }
            }
            for (int c = 0; c < 2; ++c) {
                const float f = hp_[c].process(lp_[c].process(out[c]));
                fb_[c] = softLimit(f * fbS_.current(), 1.0f);
                ch[c][i] = softLimit(out[c], 2.0f);
            }
            ++abs_;
        }
        for (int c = 0; c < 2; ++c) { lp_[c].flush(); hp_[c].flush(); }
        mix_.apply(ch, n);
    }
    Ring ring_[2];
    Grain g_[2][4];
    int64_t abs_ = 0, count_ = 0;
    double next_[2] = {0.0, 0.0};
    dsp::Svf lp_[2], hp_[2];
    float fb_[2] = {0.0f, 0.0f};
    dsp::Smoother fbS_;
    MixStage mix_;
};

// ========================================================================================================= Dispersion
enum { dpStages, dpCenter, dpSpread, dpCurve, dpQ, dpModDepth, dpModRate, dpFeedback, dpStereo, dpMix };
constexpr int kMaxStages = 64;

class Dispersion final : public FxBase {
    void onPrepare() override {
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        centerS_.prepare(sr_, 60.0f);
        spreadS_.prepare(sr_, 60.0f);
        fbS_.prepare(sr_, 30.0f);
        qS_.prepare(sr_, 60.0f);
        curveS_.prepare(sr_, 60.0f);
        stS_.prepare(sr_, 60.0f);
        lfo_.prepare(sr_, 0xD15u);
        lfo_.setShape(0);
    }
    void reset() override {
        for (auto& c : ap_) for (auto& a : c) a.reset();
        fb_[0] = fb_[1] = 0.0f;
        dc_[0] = dc_[1] = 0.0f;
        lfo_.setPhase(0.0f);
        ctr_ = 0;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(dpMix), snap);
        centerS_.setTarget(std::log2(fp(dpCenter)));
        spreadS_.setTarget(fp(dpSpread));
        fbS_.setTarget(std::min(0.75f, pct(dpFeedback) * 0.75f));
        qS_.setTarget(std::log(fp(dpQ)));
        curveS_.setTarget(fp(dpCurve) * 0.01f);
        stS_.setTarget(pct(dpStereo) * 0.5f);
        lfo_.setRate(fp(dpModRate));
        if (snap) { centerS_.snap(centerS_.target()); spreadS_.snap(spreadS_.target()); fbS_.snap(fbS_.target()); qS_.snap(qS_.target()); curveS_.snap(curveS_.target()); stS_.snap(stS_.target()); updateCoefs(); }
    }
    double tailSeconds() const override { return 0.5 + 2.0 * pct(dpFeedback); }
    void updateCoefs() {
        static const int kSt[6] = {4, 8, 16, 32, 48, 64};
        stages_ = kSt[std::clamp(ip(dpStages), 0, 5)];
        const float mod = lfoVal_ * fp(dpModDepth);
        const float c = centerS_.current() + mod, sp = spreadS_.current();
        const float curve = curveS_.current();  // -1: more sections low (lows late), +1: more high
        const float q = std::exp(qS_.current());
        const float st = stS_.current();
        for (int k = 0; k < stages_; ++k) {
            float u = stages_ > 1 ? static_cast<float>(k) / static_cast<float>(stages_ - 1) : 0.5f;
            // warp the distribution of section centres: curve < 0 packs them low, > 0 packs them high
            u = curve >= 0.0f ? std::pow(u, 1.0f / (1.0f + 3.0f * curve)) : std::pow(u, 1.0f - 3.0f * curve);
            for (int ch = 0; ch < 2; ++ch) {
                // stereo: a very small centre offset (long high-Q cascades turn tiny offsets into large phase
                // differences, so the range is kept narrow; 0 = mono-identical)
                const float oct = c + sp * (u - 0.5f) + (ch ? st : -st) * 0.05f;
                const float f = std::clamp(std::exp2(oct), 20.0f, 0.45f * srf());
                ap_[ch][static_cast<size_t>(k)].setup(dsp::Svf::Allpass, f, q, 0.0f, sr_);
            }
        }
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float dcA = 1.0f - std::exp(-dsp::kTwoPi * 12.0f / srf());
        for (int i = 0; i < n; ++i) {
            const float l = lfo_.next();
            centerS_.next(); spreadS_.next(); qS_.next(); curveS_.next(); stS_.next();
            const bool moving = !centerS_.settled() || !spreadS_.settled() || !qS_.settled() || !curveS_.settled() || !stS_.settled();
            // coefficients every 16 samples (every 4 while a control moves); the TPT form is modulation-safe
            if ((ctr_++ & (moving ? 3u : 15u)) == 0) { lfoVal_ = l; updateCoefs(); }
            const float fb = fbS_.next();
            for (int c2 = 0; c2 < 2; ++c2) {
                float y = ch[c2][i] + fb * fb_[c2];
                for (int k = 0; k < stages_; ++k) y = ap_[c2][static_cast<size_t>(k)].process(y);
                dc_[c2] += dcA * (y - dc_[c2]);
                y -= dc_[c2];
                fb_[c2] = softLimit(y, 1.0f);
                ch[c2][i] = softLimit(y, 2.0f);
            }
        }
        for (auto& c : ap_) for (int k = 0; k < stages_; ++k) c[static_cast<size_t>(k)].flush();
        mix_.apply(ch, n);
    }
    std::array<dsp::Svf, kMaxStages> ap_[2];
    int stages_ = 16;
    uint32_t ctr_ = 0;
    float lfoVal_ = 0.0f, fb_[2] = {0, 0}, dc_[2] = {0, 0};
    dsp::Smoother centerS_, spreadS_, fbS_, qS_, curveS_, stS_;
    dsp::Lfo lfo_;
    MixStage mix_;
};

// =================================================================================================== Modal Resonator
enum { mdMaterial, mdNote, mdOctave, mdFine, mdModes, mdDecay, mdDamping, mdInharm, mdBright, mdExcite, mdSpread, mdMix };
constexpr int kMaxModes = 24;

class ModalResonator final : public FxBase {
    struct Mode { float a1 = 0, a2 = 0, b0 = 0, y1[2] = {0, 0}, y2[2] = {0, 0}, x1[2] = {0, 0}, x2[2] = {0, 0}, amp = 0, panL = 1, panR = 1; bool on = false; };
    void onPrepare() override {
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        pitchS_.prepare(sr_, 40.0f);
        decayS_.prepare(sr_, 60.0f);
    }
    void reset() override {
        for (auto& m : m_) { m.y1[0] = m.y1[1] = m.y2[0] = m.y2[1] = m.x1[0] = m.x1[1] = m.x2[0] = m.x2[1] = 0.0f; }
        fast_ = slow_ = 0.0f;
        ctr_ = 0;
        outG_ = 1.0f;
        inEnv_ = outEnv_ = 0.0f;
        guard_ = 1.0f;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(mdMix), snap);
        const float midi = static_cast<float>(ip(mdNote) + 12 * (ip(mdOctave) + 1)) + fp(mdFine) * 0.01f;
        pitchS_.setTarget(midi);
        decayS_.setTarget(std::log(fp(mdDecay)));
        if (snap) { pitchS_.snap(midi); decayS_.snap(decayS_.target()); updateModes(); }
    }
    double tailSeconds() const override { return std::min(14.0, static_cast<double>(fp(mdDecay)) * 1.2); }
    static float ratio(int material, int k, float stretch) {
        static const float bar[] = {1.0f, 2.756f, 5.404f, 8.933f, 13.345f, 18.638f, 24.81f, 31.87f};
        static const float bell[] = {0.5f, 1.0f, 1.183f, 1.506f, 2.0f, 2.514f, 2.662f, 3.011f, 4.166f, 5.433f, 6.796f, 8.215f};
        static const float glass[] = {1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.6f, 16.3f, 20.4f};
        static const float membrane[] = {1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f, 3.6f, 4.06f, 4.154f, 4.601f};
        float r;
        switch (material) {
            case 1: r = k < 8 ? bar[k] : bar[7] * std::pow(1.3f, static_cast<float>(k - 7)); break;
            case 2: r = k < 12 ? bell[k] : bell[11] * std::pow(1.18f, static_cast<float>(k - 11)); break;
            case 3: r = k < 8 ? glass[k] : glass[7] * std::pow(1.25f, static_cast<float>(k - 7)); break;
            case 4: {  // plate: sqrt(m^2 + n^2) family, sorted roughly by index
                const int m = 1 + k % 4, nn = 1 + k / 4;
                r = std::sqrt(static_cast<float>(m * m + nn * nn)) / std::sqrt(2.0f);
                break;
            }
            case 5: r = k < 12 ? membrane[k] : membrane[11] * std::pow(1.12f, static_cast<float>(k - 11)); break;
            default: r = static_cast<float>(k + 1);  // harmonic
        }
        return r * std::pow(std::max(r, 1e-3f), stretch);  // stretch > 0 spreads upper modes (stiffness), < 0 squeezes
    }
    void updateModes() {
        const int material = ip(mdMaterial);
        const int count = std::clamp(static_cast<int>(std::lround(fp(mdModes))), 1, kMaxModes);
        const float f0 = 440.0f * std::exp2((pitchS_.current() - 69.0f) / 12.0f);
        const float t60 = std::exp(decayS_.current());
        const float damp = pct(mdDamping) * 1.5f;
        const float stretch = fp(mdInharm) * 0.01f * 0.5f;
        const float bright = fp(mdBright) * 0.01f;
        const float spread = pct(mdSpread);
        float norm = 0.0f;
        for (int k = 0; k < kMaxModes; ++k) {
            Mode& m = m_[static_cast<size_t>(k)];
            const float r = ratio(material, k, stretch);
            const float f = f0 * r;
            m.on = k < count && f < 0.45f * srf() && f > 20.0f;
            if (!m.on) continue;
            const float tk = std::max(0.02f, t60 * std::pow(std::max(r, 0.25f), -damp));
            const float rad = std::exp(-6.9078f / (tk * srf()));
            const float w = dsp::kTwoPi * f / srf();
            m.a1 = 2.0f * rad * std::cos(w);
            m.a2 = -rad * rad;
            // excitation scaled with sqrt(1 - r): a strike rings at a similar loudness whatever the decay (unity peak
            // gain would make long modes inaudible to transients). Sustained input at a mode frequency is held by the
            // level guard in process().
            m.b0 = 0.3f * std::sqrt(1.0f - rad);
            m.amp = std::pow(std::max(r, 0.25f), -0.6f + 0.9f * bright);
            const float pan = (k % 2 ? 1.0f : -1.0f) * spread;
            dsp::panGains(pan, m.panL, m.panR);
            m.panL *= 1.41421356f;
            m.panR *= 1.41421356f;
            norm += m.amp;
        }
        const float g = 1.0f / std::max(1.0f, std::sqrt(norm));
        for (auto& m : m_) m.amp *= g;
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const int excite = ip(mdExcite);  // 0 input, 1 transients, 2 hybrid
        const float aF = 1.0f - std::exp(-1.0f / (0.0005f * srf())), rF = 1.0f - std::exp(-1.0f / (0.01f * srf()));
        const float aS = 1.0f - std::exp(-1.0f / (0.02f * srf())), rS = 1.0f - std::exp(-1.0f / (0.15f * srf()));
        for (int i = 0; i < n; ++i) {
            pitchS_.next();
            decayS_.next();
            if ((ctr_++ & 7) == 0 && (!pitchS_.settled() || !decayS_.settled())) updateModes();
            float x[2] = {ch[0][i], ch[1][i]};
            const float m = 0.5f * (std::fabs(x[0]) + std::fabs(x[1]));
            fast_ += (m > fast_ ? aF : rF) * (m - fast_);
            slow_ += (m > slow_ ? aS : rS) * (m - slow_);
            const float tr = std::clamp((fast_ - 1.3f * slow_) / std::max(fast_, 1e-5f), 0.0f, 1.0f);
            const float eg = excite == 0 ? 1.0f : (excite == 1 ? tr : 0.35f + 0.65f * tr);
            float y[2] = {0.0f, 0.0f};
            for (auto& md : m_) {
                if (!md.on) continue;
                for (int c = 0; c < 2; ++c) {
                    const float in = x[c] * eg;
                    const float v = md.b0 * (in - md.x2[c]) + md.a1 * md.y1[c] + md.a2 * md.y2[c];
                    md.x2[c] = md.x1[c];
                    md.x1[c] = in;
                    md.y2[c] = md.y1[c];
                    md.y1[c] = v;
                }
                y[0] += md.amp * md.panL * md.y1[0];
                y[1] += md.amp * md.panR * md.y1[1];
            }
            // level guard: the resonance may exceed the input by at most +9 dB (fast attack, slow release), so a
            // sustained tone sitting on a mode cannot build up without bound
            const float yi = 0.5f * (std::fabs(y[0]) + std::fabs(y[1]));
            inEnv_ += (m > inEnv_ ? 0.01f : 0.0005f) * (m - inEnv_);
            outEnv_ += (yi > outEnv_ ? 0.01f : 0.0005f) * (yi - outEnv_);
            const float allow = 2.8f * std::max(inEnv_, 0.02f);
            const float tgt = outEnv_ > allow ? allow / outEnv_ : 1.0f;
            guard_ += (tgt < guard_ ? 0.02f : 0.0005f) * (tgt - guard_);
            ch[0][i] = softLimit(y[0] * guard_, 1.5f);
            ch[1][i] = softLimit(y[1] * guard_, 1.5f);
        }
        for (auto& md : m_)
            for (int c = 0; c < 2; ++c) {
                if (std::fabs(md.y1[c]) < 1e-20f) md.y1[c] = 0.0f;
                if (std::fabs(md.y2[c]) < 1e-20f) md.y2[c] = 0.0f;
            }
        mix_.apply(ch, n);
    }
    std::array<Mode, kMaxModes> m_{};
    dsp::Smoother pitchS_, decayS_;
    float fast_ = 0, slow_ = 0, outG_ = 1, inEnv_ = 0, outEnv_ = 0, guard_ = 1;
    uint32_t ctr_ = 0;
    MixStage mix_;
};

// =================================================================================================== Texture Imprint
enum { txType, txAmount, txDensity, txBand, txWidth, txFollow, txTransient, txCorrelation, txSeed, txMix };

class TextureImprint final : public FxBase {
    void onPrepare() override {
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        amtS_.prepare(sr_, 40.0f);
    }
    void reset() override {
        seeded_ = ip(txSeed);
        rngC_.seed_(0x7E57u + static_cast<uint32_t>(ip(txSeed)) * 131u);
        rngL_.seed_(0x1E57u + static_cast<uint32_t>(ip(txSeed)) * 137u);
        rngR_.seed_(0x2E57u + static_cast<uint32_t>(ip(txSeed)) * 139u);
        env_ = fast_ = slow_ = 0.0f;
        for (int c = 0; c < 2; ++c) { bp_[c].reset(); hp_[c].reset(); click_[c] = 0.0f; sh_[c] = 0.0f; }
        burst_ = 0.0f;
        burstG_ = 0.0f;
        shT_ = 0.0f;
        presence_ = 0.0f;
        presenceHold_ = 0;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        if (ip(txSeed) != seeded_) {  // a new seed restarts the (repeatable) random streams
            seeded_ = ip(txSeed);
            rngC_.seed_(0x7E57u + static_cast<uint32_t>(seeded_) * 131u);
            rngL_.seed_(0x1E57u + static_cast<uint32_t>(seeded_) * 137u);
            rngR_.seed_(0x2E57u + static_cast<uint32_t>(seeded_) * 139u);
        }
        mix_.setMix(pct(txMix), snap);
        amtS_.setTarget(dsp::dbToGain(fp(txAmount)));
        if (snap) amtS_.snap(amtS_.target());
        const float q = 0.5f + 6.0f * (1.0f - pct(txWidth));
        for (int c = 0; c < 2; ++c) {
            bp_[c].setup(dsp::Svf::Bandpass, fp(txBand) * (c ? 1.06f : 1.0f), q, 0.0f, sr_);
            hp_[c].setup(dsp::Svf::Highpass, 40.0f, 0.7f, 0.0f, sr_);
        }
    }
    double tailSeconds() const override { return 0.1; }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const int type = ip(txType);  // 0 dust, 1 crackle, 2 static, 3 friction, 4 grit
        const float dens = pct(txDensity);
        const float follow = pct(txFollow);
        const float trans = pct(txTransient);
        const float corr = pct(txCorrelation);
        const float aE = 1.0f - std::exp(-1.0f / (0.002f * srf())), rE = 1.0f - std::exp(-1.0f / (0.08f * srf()));
        const float aF = 1.0f - std::exp(-1.0f / (0.0005f * srf())), rF = 1.0f - std::exp(-1.0f / (0.01f * srf()));
        const float aS = 1.0f - std::exp(-1.0f / (0.02f * srf())), rS = 1.0f - std::exp(-1.0f / (0.15f * srf()));
        const float rate = (5.0f + 1500.0f * dens * dens) / srf();  // events per sample
        const float clickDecay = std::exp(-1.0f / (0.0006f * srf()));
        for (int i = 0; i < n; ++i) {
            const float m = 0.5f * (std::fabs(ch[0][i]) + std::fabs(ch[1][i]));
            env_ += (m > env_ ? aE : rE) * (m - env_);
            fast_ += (m > fast_ ? aF : rF) * (m - fast_);
            slow_ += (m > slow_ ? aS : rS) * (m - slow_);
            const float onset = std::clamp((fast_ - 1.4f * slow_) / std::max(fast_, 1e-5f), 0.0f, 1.0f);
            // presence: open while there was sound in the last ~1.5 s, then a slow close (silence stays silent)
            if (env_ > 1e-4f) presenceHold_ = static_cast<int>(1.5f * srf());
            else if (presenceHold_ > 0) --presenceHold_;
            presence_ += ((presenceHold_ > 0 ? 1.0f : 0.0f) - presence_) * (presenceHold_ > 0 ? 0.002f : 0.0002f);
            // level the texture rides on: follows the input envelope (Follow 100 % = silent input, silent texture)
            const float lvl = (follow * std::min(env_ * 2.5f, 1.0f) + (1.0f - follow) * 0.25f * presence_) * (1.0f + 3.0f * trans * onset);
            const float a = amtS_.next() * lvl;
            float tex[2] = {0.0f, 0.0f};
            const float common = rngC_.bipolar();
            const float nz[2] = {corr * common + (1.0f - corr) * rngL_.bipolar(), corr * common + (1.0f - corr) * rngR_.bipolar()};
            switch (type) {
                case 0:  // dust: sparse filtered impulses
                case 1: {  // crackle: dust plus larger pops and a faint hiss
                    for (int c = 0; c < 2; ++c) {
                        const float u = (c == 0 || corr > 0.5f) ? rngC_.uniform() : rngR_.uniform();
                        if (u < rate * (1.0f + 4.0f * trans * onset)) click_[c] += (rngL_.bipolar() > 0.0f ? 1.0f : -1.0f) * (type == 1 && rngC_.uniform() < 0.08f ? 3.0f : 1.0f);
                        click_[c] *= clickDecay;
                        tex[c] = click_[c] + (type == 1 ? 0.03f * nz[c] : 0.0f);
                    }
                    break;
                }
                case 2: {  // static: bursts of band noise
                    if (burst_ <= 0.0f && rngC_.uniform() < rate * 0.05f * (1.0f + 6.0f * trans * onset)) burst_ = (0.005f + 0.04f * rngC_.uniform()) * srf();
                    const float g = burst_ > 0.0f ? 1.0f : 0.05f;
                    burstG_ += (g - burstG_) * 0.02f;  // short ramps: the bursts never click on or off
                    if (burst_ > 0.0f) burst_ -= 1.0f;
                    for (int c = 0; c < 2; ++c) tex[c] = burstG_ * nz[c];
                    break;
                }
                case 3: {  // friction: band noise amplitude-modulated by the input itself (a bowed / scraped grain)
                    for (int c = 0; c < 2; ++c) tex[c] = nz[c] * std::fabs(ch[c][i]) * 4.0f;
                    break;
                }
                default: {  // grit: sample-and-hold noise imprinted onto the signal (granular AM)
                    shT_ += rate * 40.0f;
                    if (shT_ >= 1.0f) { shT_ -= 1.0f; for (int c = 0; c < 2; ++c) shTarget_[c] = nz[c]; }
                    for (int c = 0; c < 2; ++c) {
                        sh_[c] += 0.05f * (shTarget_[c] - sh_[c]);
                        tex[c] = sh_[c] * ch[c][i] * 4.0f;
                    }
                }
            }
            for (int c = 0; c < 2; ++c) {
                float t = type == 3 || type == 4 ? tex[c] : bp_[c].process(tex[c]) * 2.0f;
                if (type == 3) t = bp_[c].process(t) * 2.0f;
                t = hp_[c].process(t);
                ch[c][i] = ch[c][i] + softLimit(a * t, 1.0f);
            }
        }
        for (int c = 0; c < 2; ++c) { bp_[c].flush(); hp_[c].flush(); }
        mix_.apply(ch, n);
    }
    dsp::Rng rngC_, rngL_, rngR_;
    int seeded_ = -1;
    dsp::Svf bp_[2], hp_[2];
    dsp::Smoother amtS_;
    float env_ = 0, fast_ = 0, slow_ = 0, click_[2] = {0, 0}, burst_ = 0, burstG_ = 0, sh_[2] = {0, 0}, shTarget_[2] = {0, 0}, shT_ = 0, presence_ = 0;
    int presenceHold_ = 0;
    MixStage mix_;
};

}  // namespace

// ======================================================================================================= descriptors
FxDescriptor describeSliceShuffler() {
    FxDescriptor d;
    d.typeId = "slice_shuffle";
    d.name = "Slice Shuffler";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Rebuilds your sound like a sampler: the previous bar is cut into slices and replayed in a new order, with reversed, pitched and gated slices.";
    d.params = {
        P::choice("length", "Capture", {"1 beat", "2 beats", "1 bar", "2 bars"}, 2, "How much of the recent past is sliced and rebuilt. It always replays the previous period, so the first period passes through untouched."),
        P::choice("slices", "Slices", {"4", "8", "16", "32"}, 2, "How many pieces the captured period is cut into."),
        P::choice("pattern", "Pattern", {"Reverse Order", "Shuffle", "Stride 3", "Stride 5", "Palindrome", "Pairs", "Random Walk", "Stutter"}, 1,
                  "The new order of the slices. Reverse Order plays them last-to-first; Shuffle is a fixed scramble (per Seed); Strides skip through; Palindrome goes there and back; Pairs plays each slice twice; Random Walk wanders; Stutter holds the first of every four."),
        P::toggle("evolve", "Evolve", false, "Off: the same rearrangement every period. On: a new (still repeatable) arrangement each period."),
        P::flt("seed", "Seed", "", 0.0f, 99.0f, 7.0f, "Chooses the scramble, the reversed slices and the pitched slices. The same seed always gives the same result."),
        P::pct("reverse", "Reverse", 25.0f, "Chance that a slice plays backwards."),
        P::choice("pitch", "Pitch Set", {"Off", "Octaves", "Octaves + Fifths", "Minor Steps", "Chaos"}, 0, "Intervals a pitched slice may jump to (it plays faster or slower, like a sampler)."),
        P::pct("pitch_amt", "Pitch Chance", 30.0f, "Chance that a slice is pitched (with a Pitch Set chosen)."),
        P::pct("gate", "Gate", 100.0f, "How much of each slice sounds before it is cut (with a short fade). Lower = choppy, staccato rebuilds."),
        P::pct("swing", "Swing", 0.0f, "Delays every second slice for a swung, shuffled feel."),
        P::flt("fade", "Edge Fade", "ms", 0.5f, 12.0f, 2.5f, "Fade at every slice edge. Short keeps attacks sharp; longer is smoother. Never zero, so slices do not click."),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and the rebuilt sound."),
    };
    for (int i : {ssEvolve, ssSwing, ssFade}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<SliceShuffler>(); };
    return d;
}

FxDescriptor describeAttackBody() {
    FxDescriptor d;
    d.typeId = "transient_split";
    d.name = "Attack / Body";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Splits every sound into its attack and its body and treats them separately: flam and roll the hits, reverse, freeze or smear what rings after them.";
    d.params = {
        P::pct("sensitivity", "Sensitivity", 55.0f, "How readily a sudden rise counts as an attack."),
        P::logf("length", "Attack Length", "ms", 2.0f, 200.0f, 25.0f, "How long after each hit belongs to the attack layer."),
        P::flt("att_gain", "Attack Level", "dB", -60.0f, 12.0f, 0.0f, "Level of the attack layer (-60 dB removes the hits)."),
        P::flt("att_delay", "Attack Delay", "ms", 0.0f, 250.0f, 0.0f, "Moves the attacks later than the body: smeared, flammed or displaced hits."),
        P::flt("repeats", "Attack Repeats", "", 0.0f, 6.0f, 0.0f, "Extra copies of each attack (rolls, ratchets)."),
        P::logf("spacing", "Repeat Spacing", "ms", 8.0f, 250.0f, 45.0f, "Time between attack repeats."),
        P::pct("rep_decay", "Repeat Decay", 65.0f, "Each repeat is this much of the previous one."),
        P::flt("body_gain", "Body Level", "dB", -60.0f, 12.0f, 0.0f, "Level of the body (everything that is not an attack)."),
        P::choice("body_mode", "Body", {"Natural", "Reversed", "Held", "Smeared"}, 0, "Natural: untouched. Reversed: the body plays backwards in chunks. Held: the body after each hit is frozen and sustained until the next hit. Smeared: the body is diffused into a wash."),
        P::logf("body_len", "Body Chunk", "ms", 30.0f, 2000.0f, 250.0f, "Chunk length for Reversed, grain size for Held."),
        P::logf("body_tone", "Body Tone", "Hz", 500.0f, 20000.0f, 20000.0f, "Low-pass on the body layer (20 kHz = off)."),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and the recombined sound. With everything at its default the two are identical."),
    };
    for (int i : {abRepDecay, abBodyLen, abBodyTone}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<AttackBody>(); };
    return d;
}

FxDescriptor describeEchoMatrix() {
    FxDescriptor d;
    d.typeId = "echo_matrix";
    d.name = "Echo Matrix";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "A rhythm of eight echoes, each with its own level, pitch, place in the stereo field and colour, cycling back through a filtered, driven feedback loop.";
    d.params = {
        P::choice("division", "Step", divisionLabels(), 4, "The time between the eight echo taps (tempo-synced)."),
        P::choice("pattern", "Pattern", {"Straight", "Dotted", "Off-beats", "Gallop", "Triplet Grid", "Sparse", "Cascade", "Random"}, 1, "Which of the eight taps sound."),
        P::pct("decay", "Tap Decay", 85.0f, "Each later tap is this much of the previous one."),
        P::flt("interval", "Interval", "st", -12.0f, 12.0f, 7.0f, "The pitch step used by the pitch pattern."),
        P::choice("pitch_mode", "Pitch Pattern", {"Off", "Climb", "Zigzag", "Octave Odd", "Shadow"}, 0, "How taps are pitched: climbing by the interval, alternating up and down, octave on odd taps, or an octave-down shadow."),
        P::pct("swing", "Swing", 0.0f, "Pushes every other tap late for a swung rhythm."),
        P::pct("spread", "Spread", 60.0f, "Taps alternate left and right, wider towards the end of the cycle."),
        P::logf("tone", "Tone", "Hz", 600.0f, 18000.0f, 7000.0f, "Brightness of the first tap and of the feedback loop."),
        P::pct("darken", "Darken", 35.0f, "Each later tap is darker than the last."),
        P::pct("feedback", "Feedback", 30.0f, "The whole cycle repeats. Safely limited."),
        P::toggle("pitch_loop", "Pitch in Loop", false, "On: the feedback carries the pitched tap, so every cycle climbs further."),
        P::pct("drive", "Loop Drive", 15.0f, "Saturation inside the feedback loop."),
        P::pct("mix", "Mix", 45.0f, "Blend of the original and the echoes."),
    };
    for (int i : {emSwing, emDarken, emPitchLoop, emDrive}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<EchoMatrix>(); };
    return d;
}

FxDescriptor describeReversePitchEcho() {
    FxDescriptor d;
    d.typeId = "reverse_echo";
    d.name = "Reverse Pitch Echo";
    d.family = FxFamily::PitchVoice;
    d.tooltip = "Echoes that come back backwards and pitched; with feedback each pass climbs (or falls) another interval into a glittering cascade.";
    d.params = {
        P::flt("interval", "Interval", "st", -24.0f, 24.0f, 12.0f, "Pitch of each pass (playback speed of the reversed grains)."),
        P::flt("fine", "Fine", "ct", -50.0f, 50.0f, 0.0f, "Detune: a few cents make the cascade shimmer and beat."),
        P::toggle("sync", "Sync", true, "Grain length locked to the song tempo."),
        P::logf("length_ms", "Length", "ms", 40.0f, 2000.0f, 300.0f, "Grain length when Sync is off."),
        P::choice("division", "Division", divisionLabels(), 7, "Grain length as a note value when Sync is on."),
        P::logf("delay_ms", "Pre-Delay", "ms", 1.0f, 1000.0f, 20.0f, "Gap before the reversed material returns."),
        P::choice("direction", "Direction", {"Reverse", "Forward", "Alternate"}, 0, "Reverse: every grain backwards. Forward: pitched forwards. Alternate: backwards and forwards in turn."),
        P::pct("feedback", "Feedback", 55.0f, "Feeds the pitched result back in: each pass is shifted again. Safely limited."),
        P::logf("tone", "Tone", "Hz", 800.0f, 18000.0f, 9000.0f, "Low-pass inside the loop: tames the top of the cascade."),
        P::logf("low_cut", "Low Cut", "Hz", 20.0f, 1200.0f, 120.0f, "High-pass inside the loop: keeps the cascade from getting muddy."),
        P::pct("spread", "Stereo", 50.0f, "Offsets and detunes the right channel's grains."),
        P::pct("mix", "Mix", 50.0f, "Blend of the original and the reversed echoes."),
    };
    for (int i : {rpFine, rpLowCut, rpSpread}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<ReversePitchEcho>(); };
    return d;
}

FxDescriptor describeDispersion() {
    FxDescriptor d;
    d.typeId = "dispersion";
    d.name = "Dispersion";
    d.family = FxFamily::Modulation;
    d.tooltip = "Delays different frequencies by different amounts without changing the tone: attacks smear into chirps, zaps and laser-like sweeps.";
    d.params = {
        P::choice("stages", "Stages", {"4", "8", "16", "32", "48", "64"}, 3, "Number of all-pass sections. More = a longer, more obvious chirp."),
        P::logf("center", "Center", "Hz", 60.0f, 8000.0f, 700.0f, "Where in the spectrum the delay is concentrated."),
        P::flt("spread", "Spread", "oct", 0.0f, 8.0f, 4.0f, "How wide a range of frequencies is spread out in time."),
        P::flt("curve", "Lows / Highs", "%", -100.0f, 100.0f, -40.0f, "Negative: low frequencies arrive later (a falling 'pew'). Positive: highs arrive later (a rising chirp)."),
        P::flt("q", "Sharpness", "", 0.3f, 4.0f, 1.2f, "Higher = each section delays a narrower band more strongly: more ringing, metallic chirps."),
        P::flt("mod_depth", "Motion", "oct", 0.0f, 2.0f, 0.0f, "Slowly moves the centre: the smear sweeps."),
        P::logf("mod_rate", "Motion Rate", "Hz", 0.02f, 8.0f, 0.2f, "Speed of the motion."),
        P::pct("feedback", "Feedback", 0.0f, "Recirculates the dispersed signal: springy, laser-like ringing. Safely limited."),
        P::pct("stereo", "Stereo", 0.0f, "Left and right are dispersed slightly differently (wider, but less mono-compatible with many stages)."),
        P::pct("mix", "Mix", 100.0f, "At 100 % only the timing changes. Mixing with the dry sound creates moving notches (phaser-like)."),
    };
    for (int i : {dpQ, dpModRate, dpStereo}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Dispersion>(); };
    return d;
}

FxDescriptor describeModalResonator() {
    FxDescriptor d;
    d.typeId = "modal";
    d.name = "Modal Resonator";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Strikes a synthetic object with your sound: bars, bells, glass, plates and drum skins ring at their own (often inharmonic) overtones.";
    d.params = {
        P::choice("material", "Material", {"Harmonic", "Bar", "Bell", "Glass", "Plate", "Membrane"}, 2, "The overtone pattern of the object: harmonic (string-like), metal bar, bell, glass, plate or drum membrane."),
        P::choice("note", "Note", {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0, "Fundamental pitch of the object."),
        P::choice("octave", "Octave", {"1", "2", "3", "4", "5", "6"}, 3, "Register of the object."),
        P::flt("fine", "Fine", "ct", -100.0f, 100.0f, 0.0f, "Fine tuning."),
        P::flt("modes", "Modes", "", 1.0f, 24.0f, 12.0f, "Number of resonant modes (overtones)."),
        P::logf("decay", "Decay", "s", 0.05f, 12.0f, 2.5f, "How long the object rings."),
        P::pct("damping", "Damping", 45.0f, "Upper modes die away faster than lower ones (a softer, woodier object)."),
        P::flt("inharm", "Inharmonicity", "%", -50.0f, 50.0f, 0.0f, "Stretches (+) or squeezes (-) the overtones: from tuned to clangorous."),
        P::flt("bright", "Brightness", "%", -100.0f, 100.0f, 0.0f, "Balance of low and high modes."),
        P::choice("excite", "Excite", {"Input", "Transients", "Hybrid"}, 0, "What strikes the object: the whole sound, only its attacks, or both."),
        P::pct("spread", "Spread", 40.0f, "Alternate modes are placed left and right."),
        P::pct("mix", "Mix", 50.0f, "Blend of the original and the ringing object."),
    };
    for (int i : {mdFine, mdDamping, mdBright, mdSpread}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<ModalResonator>(); };
    return d;
}

FxDescriptor describeTextureImprint() {
    FxDescriptor d;
    d.typeId = "texture";
    d.name = "Texture Imprint";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Imprints a physical texture onto the sound (dust, crackle, static, friction, grit) that follows its level and attacks.";
    d.params = {
        P::choice("type", "Texture", {"Dust", "Crackle", "Static", "Friction", "Grit"}, 0, "Dust: fine particles. Crackle: dust, pops and hiss. Static: bursts of interference. Friction: a scraped, bowed grain carried by the sound. Grit: the sound chopped by random granular noise."),
        P::flt("amount", "Amount", "dB", -40.0f, 6.0f, -12.0f, "Level of the texture relative to the sound."),
        P::pct("density", "Density", 40.0f, "How many particles or bursts."),
        P::logf("band", "Band", "Hz", 200.0f, 12000.0f, 3000.0f, "Where in the spectrum the texture sits."),
        P::pct("width", "Width", 50.0f, "Narrow (tonal, ringing) to wide (noisy)."),
        P::pct("follow", "Follow", 85.0f, "How much the texture follows the sound's level. 100 % = silence stays silent."),
        P::pct("transient", "Attack Bursts", 30.0f, "Extra texture on every attack."),
        P::pct("correlation", "Stereo Link", 30.0f, "100 % = the same texture in both ears; lower = wider."),
        P::flt("seed", "Seed", "", 0.0f, 99.0f, 1.0f, "Chooses the random pattern. The same seed always gives the same texture."),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and the imprinted sound."),
    };
    for (int i : {txWidth, txCorrelation, txSeed}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<TextureImprint>(); };
    return d;
}

}  // namespace ih1::fx
