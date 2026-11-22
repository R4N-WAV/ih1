// V5 feedback round: rhythm, drum and hybrid processors (Phase 3). Original IH1 designs; product manuals in the V5
// research supplement (transient designers, parallel drum compressors, rhythmic volume shapers) were read as
// workflow references only. Every processor acts on the composite signal it receives: none of them claims to
// separate kick, snare or hats inside a mixed loop.
//
//   Pulse Shaper     tremolo as a groove engine: a 16-step amplitude pattern (16 rhythms) on the host grid or a free
//                    clock, with swing, accents and ghost steps, per-bar seeded probability, humanised timing, step
//                    attack / gate / release, stereo modes (linked, offset, ping-pong, mirror), band focus (only the
//                    lows, mids or highs move) and an optional reset of the pattern on every detected hit.
//   Drum Crush       a parallel drum-bus crusher: a fast peak compressor whose gain reduction also drives a saturator
//                    ("the harder it squashes, the dirtier it gets"), a tilt tone stage, a body shelf, and a blend
//                    over the untouched (time-aligned) dry transient.
//   Drum Space       a short room excited by two paths: the transient layer (bright) and the body layer (dark) of
//                    the input, split by an exact transient mask (the two layers sum back to the input). Early
//                    reflections plus an 8-line Householder room, five characters, optional gated tail.
//   Band Transients  attack / sustain shaping in three bands (Linkwitz-Riley low and high splits, the mid band is the
//                    exact remainder, so neutral settings null): e.g. more kick attack with less cymbal sustain.
//   Hit Melody       hybrid: every detected hit advances a note pattern in a key / scale, and the hit strikes a pair of
//                    tuned resonator voices (fundamental + two partials) at that note. Drums become melodic.
//
// Stability (V5 Phase 5): Pulse Shaper, Drum Crush and Band Transients have no recursion. Drum Space's room is a
// Householder FDN with absorption < 1 and one-pole damping (|H| <= 1). Hit Melody's resonators are two-pole sections
// with r < 1 normalised to unity peak gain. Every gain edge is a raised-cosine or exponential ramp (no steps).
#include <algorithm>
#include <array>
#include <cmath>

#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Nonlinear.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

inline float rcos(float u) { return 0.5f - 0.5f * std::cos(dsp::kPi * std::clamp(u, 0.0f, 1.0f)); }
inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
inline float hashU(uint32_t a, uint32_t b) { return static_cast<float>(hash32(a * 0x9E3779B1u ^ hash32(b)) >> 8) * (1.0f / 16777216.0f); }

// Onset detector: fast vs slow envelope with a refractory period (cymbal wash and room tails do not re-trigger).
struct Onset {
    dsp::EnvelopeFollower fast, slow;
    int refr = 0, minGap = 2400;
    void prepare(double sr, float minGapMs) {
        fast.prepare(sr, 0.4f, 30.0f);
        slow.prepare(sr, 30.0f, 400.0f);
        minGap = static_cast<int>(minGapMs * 0.001 * sr);
    }
    void reset() { fast.reset(); slow.reset(); refr = 0; }
    // ratio: how far the fast envelope must rise above the slow one
    bool process(float a, float ratio, float floor = 0.003f) {
        const float f = fast.process(a), s = slow.process(a);
        if (refr > 0) { --refr; return false; }
        if (f > floor && f > ratio * s) { refr = minGap; return true; }
        return false;
    }
};

// ============================================================================================ Pulse Shaper
enum { psPattern, psDivision, psSteps, psDepth, psAttack, psGate, psRelease, psSwing, psAccent, psChance, psHumanize, psStereo, psBand, psReset, psSeed, psMix };

// 16-step patterns: 0 = rest, 1 = ghost, 2 = normal, 3 = accent
constexpr uint8_t kPulse[16][16] = {
    {3, 0, 2, 0, 3, 0, 2, 0, 3, 0, 2, 0, 3, 0, 2, 0},  // Eighths
    {3, 1, 2, 1, 3, 1, 2, 1, 3, 1, 2, 1, 3, 1, 2, 1},  // Sixteenths
    {0, 0, 3, 0, 0, 0, 3, 0, 0, 0, 3, 0, 0, 0, 3, 0},  // Off-beats
    {3, 0, 0, 2, 0, 0, 2, 0, 3, 0, 0, 2, 0, 0, 2, 0},  // Dotted
    {3, 0, 0, 2, 0, 0, 2, 0, 3, 0, 0, 2, 0, 0, 2, 0},  // Tresillo (3+3+2 per half bar)
    {3, 0, 0, 2, 0, 0, 2, 0, 0, 0, 3, 0, 2, 0, 0, 0},  // Clave
    {3, 0, 2, 2, 3, 0, 2, 2, 3, 0, 2, 2, 3, 0, 2, 2},  // Gallop
    {3, 0, 2, 0, 0, 2, 0, 2, 3, 0, 2, 0, 0, 2, 2, 0},  // Skip
    {3, 2, 0, 0, 2, 0, 1, 2, 0, 3, 0, 1, 2, 0, 2, 0},  // Broken
    {3, 0, 0, 0, 2, 0, 0, 0, 3, 2, 2, 2, 3, 3, 3, 3},  // Stutter Run
    {3, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 1, 0},  // Half-time
    {3, 0, 2, 3, 0, 2, 3, 0, 2, 3, 0, 2, 3, 0, 2, 0},  // Triplet feel
    {3, 1, 1, 2, 1, 1, 3, 1, 1, 2, 1, 1, 3, 1, 2, 1},  // Ghost Notes
    {0, 2, 0, 3, 0, 2, 2, 0, 0, 2, 0, 3, 0, 0, 2, 2},  // Syncopa
    {3, 3, 0, 3, 3, 0, 3, 0, 3, 3, 0, 3, 3, 0, 3, 3},  // Machine
    {3, 2, 2, 2, 3, 2, 2, 2, 3, 2, 2, 2, 3, 2, 2, 2},  // Seeded (rewritten per bar from the seed)
};

class PulseShaper final : public FxBase {
    void onPrepare() override {
        onset_.prepare(sr_, 90.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
    }
    void reset() override {
        for (int c = 0; c < 2; ++c) { env_[c] = 0.0f; atk_[c] = 1.0f; from_[c] = 0.0f; lvl_[c] = 0.0f; hold_[c] = 0; lp_[c].reset(); hp_[c].reset(); lp2_[c].reset(); hp2_[c].reset(); }
        freeBeats_ = 0.0;
        lastSlot_ = -1;
        onset_.reset();
        mix_.reset();
    }
    void applyParams(bool snap) override {
        for (int c = 0; c < 2; ++c) {
            lp_[c].setup(dsp::Svf::Lowpass, 200.0f, 0.7071f, 0, sr_);
            lp2_[c].setup(dsp::Svf::Lowpass, 200.0f, 0.7071f, 0, sr_);
            hp_[c].setup(dsp::Svf::Highpass, 3000.0f, 0.7071f, 0, sr_);
            hp2_[c].setup(dsp::Svf::Highpass, 3000.0f, 0.7071f, 0, sr_);
        }
        mix_.setMix(pct(psMix), snap);
    }
    // level of step k (0..steps-1) of bar `bar` for channel c: 0..1 (0 = rest)
    float stepLevel(int k, int64_t bar, int c) const {
        const int steps = std::clamp(static_cast<int>(std::lround(fp(psSteps))), 2, 16);
        const int pat = std::clamp(ip(psPattern), 0, 15);
        const int st = ip(psStereo);
        int kk = k;
        if (c == 1 && st == 1) kk = (k + 1) % steps;  // offset by one step
        const uint32_t seed = static_cast<uint32_t>(std::lround(fp(psSeed)));
        int v;
        if (pat == 15) {
            const float u = hashU(seed * 31u + 7u, static_cast<uint32_t>(kk) + 97u * static_cast<uint32_t>(bar & 0xffff));
            v = kk == 0 ? 3 : (u < 0.35f ? 0 : u < 0.55f ? 1 : u < 0.85f ? 2 : 3);
        } else {
            v = kPulse[pat][kk % 16];
        }
        if (c == 1 && st == 3) v = v == 0 ? 2 : 0;  // mirror: the right side plays the gaps
        if (st == 2 && v > 0) {
            // ping-pong: active steps alternate between the sides
            int nth = 0;
            for (int j = 0; j < kk; ++j) nth += kPulse[pat][j % 16] > 0 ? 1 : 0;
            if ((nth & 1) != c) v = 0;
        }
        if (v == 0) return 0.0f;
        // probability: a deterministic coin per step and bar (the same bar always plays the same way)
        const float chance = pct(psChance);
        if (chance < 0.999f && kk != 0 && hashU(seed * 131u + 1u, static_cast<uint32_t>(kk) * 7919u + static_cast<uint32_t>(bar & 0xffff)) > chance) return 0.0f;
        const float acc = pct(psAccent);
        return v == 3 ? 1.0f : v == 2 ? 1.0f - 0.35f * acc : 1.0f - 0.8f * acc;
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const double stepBeats = dsp::kDivisions[std::clamp(ip(psDivision), 0, dsp::kNumDivisions - 1)].beats;
        const int steps = std::clamp(static_cast<int>(std::lround(fp(psSteps))), 2, 16);
        const double beatsPerSample = std::clamp(ctx.bpm, 20.0, 999.0) / 60.0 / sr_;
        const bool host = ctx.ppqValid && ctx.hostPlaying;
        const float depth = pct(psDepth), floor = 1.0f - depth;
        const float atkS = std::max(1.0f, fp(psAttack) * 0.001f * srf());
        const float relC = 1.0f - std::exp(-1.0f / std::max(1.0f, fp(psRelease) * 0.001f * srf()));
        const float gate = 0.05f + 0.95f * pct(psGate);
        const float swing = pct(psSwing) * 0.5f;           // up to half a step late on the off steps
        const float human = pct(psHumanize) * 0.006f * srf();  // up to +-6 ms
        const int band = ip(psBand);
        const bool resetOnHit = bp(psReset);
        const uint32_t seed = static_cast<uint32_t>(std::lround(fp(psSeed)));
        for (int i = 0; i < n; ++i) {
            if (resetOnHit && onset_.process(std::max(std::fabs(ch[0][i]), std::fabs(ch[1][i])), 2.2f)) {
                // re-anchor the pattern on the hit (free clock only: on a playing host the grid wins)
                if (!host) freeBeats_ = 0.0;
            }
            const double beats = host ? ctx.ppqPosition + i * beatsPerSample : freeBeats_;
            if (!host) freeBeats_ += beatsPerSample;
            const double stepPos = beats / stepBeats;
            const int64_t absStep = static_cast<int64_t>(std::floor(stepPos));
            const int k = static_cast<int>(((absStep % steps) + steps) % steps);
            const int64_t bar = (absStep - k) / steps;
            // swing and humanise delay a step's start inside its own slot
            double startInSlot = (k & 1) ? swing : 0.0;
            if (human > 0.0f) startInSlot += (hashU(seed + 17u, static_cast<uint32_t>(absStep & 0xffffff)) * 2.0f - 1.0f) * human / (stepBeats / beatsPerSample);
            startInSlot = std::clamp(startInSlot, 0.0, 0.9);
            const double frac = stepPos - std::floor(stepPos);
            const bool trigger = frac >= startInSlot && lastSlot_ != absStep;
            if (trigger) lastSlot_ = absStep;
            const int holdSamples = static_cast<int>((gate * (1.0 - startInSlot)) * stepBeats / beatsPerSample);
            for (int c = 0; c < 2; ++c) {
                if (trigger) {
                    const float l = stepLevel(k, bar, c);
                    if (l > 0.0f) { lvl_[c] = l; from_[c] = env_[c]; atk_[c] = 0.0f; hold_[c] = holdSamples; }
                }
                // open with a raised-cosine attack from wherever the envelope is (continuous), hold, then release
                if (atk_[c] < 1.0f) {
                    atk_[c] = std::min(1.0f, atk_[c] + 1.0f / atkS);
                    env_[c] = from_[c] + (lvl_[c] - from_[c]) * rcos(atk_[c]);
                } else if (hold_[c] > 0) {
                    --hold_[c];
                } else {
                    env_[c] += relC * (0.0f - env_[c]);
                }
                const float g = floor + (1.0f - floor) * env_[c];
                const float x = ch[c][i];
                if (band == 0) {
                    ch[c][i] = x * g;
                } else {
                    const float low = lp2_[c].process(lp_[c].process(x));
                    const float high = hp2_[c].process(hp_[c].process(x));
                    const float sel = band == 1 ? low : band == 3 ? high : x - low - high;
                    ch[c][i] = x + (g - 1.0f) * sel;  // only the chosen band moves; gain 1 = exact input
                }
            }
        }
        for (int c = 0; c < 2; ++c) { lp_[c].flush(); lp2_[c].flush(); hp_[c].flush(); hp2_[c].flush(); }
        mix_.apply(ch, n);
    }
    Onset onset_;
    dsp::Svf lp_[2], lp2_[2], hp_[2], hp2_[2];
    float env_[2] = {0, 0}, atk_[2] = {1, 1}, from_[2] = {0, 0}, lvl_[2] = {0, 0};
    int hold_[2] = {0, 0};
    double freeBeats_ = 0.0;
    int64_t lastSlot_ = -1;
    MixStage mix_;
};

// ============================================================================================ Drum Crush
enum { dcCrush, dcDrive, dcCharacter, dcRelease, dcReact, dcTone, dcBody, dcBlend, dcOut };

class DrumCrush final : public FxBase {
    void onPrepare() override {
        for (auto& d : dc_) d.prepare(sr_, 8.0f);
        outS_.prepare(sr_, 30.0f);
    }
    void reset() override {
        env_ = 0.0f;
        for (int c = 0; c < 2; ++c) { sh_[c].reset(); dc_[c].reset(); tilt_[c].reset(); body_[c].reset(); }
        outS_.snap(dsp::dbToGain(fp(dcOut)));
    }
    void applyParams(bool snap) override {
        static const dsp::Shape shapes[3] = {dsp::Shape::Tanh, dsp::Shape::SoftCubic, dsp::Shape::Asym};
        for (auto& s : sh_) s.setShape(shapes[std::clamp(ip(dcCharacter), 0, 2)]);
        for (int c = 0; c < 2; ++c) {
            tilt_[c].setLowpass(1200.0f, sr_);
            body_[c].setup(dsp::Svf::LowShelf, 110.0f, 0.7f, fp(dcBody), sr_);
        }
        static const float rel[4] = {40.0f, 140.0f, 450.0f, 0.0f};
        relMs_ = rel[std::clamp(ip(dcRelease), 0, 3)];
        outS_.setTarget(dsp::dbToGain(fp(dcOut)));
        if (snap) outS_.snap(outS_.target());
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        const float crush = pct(dcCrush);
        const float thr = dsp::dbToGain(-6.0f - 34.0f * crush);  // -6 .. -40 dBFS
        const float ratio = 3.0f + 17.0f * crush;
        const float drive = dsp::dbToGain(fp(dcDrive));
        const float react = pct(dcReact);
        const float tone = fp(dcTone) * 0.01f;  // -1 dark .. +1 bright
        const float blend = pct(dcBlend);
        // nominal saturator input: the compressed peak (~ threshold) maps to 0.5, so Drive really saturates
        const float inScale = 0.5f / thr;
        for (int i = 0; i < n; ++i) {
            const float a = std::max(std::fabs(ch[0][i]), std::fabs(ch[1][i]));
            // release: fixed, or Auto (fast after short hits, slow on sustained material)
            float rms = relMs_;
            if (rms <= 0.0f) rms = 60.0f + 500.0f * std::clamp(sustain_ * 4.0f, 0.0f, 1.0f);
            const float rel = 1.0f - std::exp(-1.0f / (rms * 0.001f * srf()));
            // instant attack: the gain reduction is right from the first sample of a hit (no makeup-amplified
            // overshoot); the crushed layer is a parallel effect, so its shaped attack is part of its character
            env_ = a > env_ ? a : env_ + rel * (a - env_);
            sustain_ += 0.0005f * (std::min(env_, 1.0f) - sustain_);
            float gr = 1.0f;  // feed-forward compressor: no recursion
            if (env_ > thr) gr = std::pow(env_ / thr, 1.0f / ratio - 1.0f);
            const float grDb = -20.0f * std::log10(std::max(gr, 1e-6f));
            // the harder it squashes, the harder the saturator is driven (up to +12 dB more at 24 dB reduction)
            const float dyn = drive * dsp::dbToGain(react * std::min(grDb, 24.0f) * 0.5f);
            // output: the shaper's level at the nominal peak is normalised to 0.5 (-6 dBFS), whatever the drive
            const float norm = 0.5f / std::max(0.05f, std::tanh(0.5f * dyn));
            const float og = outS_.next();
            for (int c = 0; c < 2; ++c) {
                const float x = ch[c][i];
                float w = sh_[c].process(x * gr * inScale * dyn) * norm;
                // tilt: dark (more low-pass) ... bright (more of the high remainder)
                const float lo = tilt_[c].lp(w), hi = w - lo;
                w = tone < 0.0f ? lo + (1.0f + tone) * hi : (1.0f - 0.5f * tone) * lo + (1.0f + tone) * hi;
                w = body_[c].process(w);
                w = dc_[c].process(w);
                ch[c][i] = og * ((1.0f - 0.5f * blend) * x + blend * w);
            }
        }
        for (auto& b : body_) b.flush();
        for (auto& t : tilt_) t.flush();
    }
    dsp::AdaaShaper sh_[2];
    dsp::DcBlocker dc_[2];
    dsp::OnePole tilt_[2];
    dsp::Svf body_[2];
    dsp::Smoother outS_;
    float env_ = 0.0f, sustain_ = 0.0f, relMs_ = 140.0f;
};

// ============================================================================================ Drum Space
enum { dsMode, dsSize, dsDecay, dsAttackSend, dsBodySend, dsGate, dsTone, dsWidth, dsMix };

class DrumSpace final : public FxBase {
    static constexpr int N = 8, kEr = 8;
    void onPrepare() override {
        static const float ms[N] = {7.1f, 9.7f, 12.3f, 14.9f, 17.3f, 20.9f, 23.7f, 27.1f};
        for (int j = 0; j < N; ++j) { base_[j] = ms[j]; line_[j].prepare(static_cast<int>(ms[j] * 3.2f * 0.001f * srf()) + 8); }
        for (auto& l : er_) l.prepare(static_cast<int>(0.09 * sr_) + 8);
        fastE_.prepare(sr_, 0.3f, 12.0f);
        slowE_.prepare(sr_, 12.0f, 120.0f);
        onset_.prepare(sr_, 60.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
    }
    void reset() override {
        for (auto& l : line_) l.reset();
        for (auto& l : er_) l.reset();
        for (auto& z : damp_) z = 0.0f;
        for (int c = 0; c < 2; ++c) { bright_[c].reset(); dark_[c].reset(); }
        fastE_.reset(); slowE_.reset(); onset_.reset();
        gate_ = 1.0f;
        gateHold_ = 0;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        const int mode = std::clamp(ip(dsMode), 0, 4);
        static const float sizeScale[5] = {0.6f, 1.6f, 1.2f, 0.45f, 2.2f};
        static const float dampHz[5] = {7000.0f, 9000.0f, 8000.0f, 12000.0f, 2600.0f};
        const float size = (0.4f + 1.2f * pct(dsSize)) * sizeScale[mode];
        const float t60 = std::clamp(fp(dsDecay), 0.05f, 6.0f);
        for (int j = 0; j < N; ++j) {
            len_[j] = std::max(4, static_cast<int>(base_[j] * size * 0.001f * srf()));
            len_[j] = std::min(len_[j], line_[j].capacity() - 2);
            g_[j] = std::pow(10.0f, -3.0f * (static_cast<float>(len_[j]) / srf()) / t60);
        }
        const float tone = fp(dsTone) * 0.01f;
        dampA_ = 1.0f - std::exp(-dsp::kTwoPi * std::clamp(dampHz[mode] * std::pow(2.0f, 1.5f * tone), 800.0f, 0.45f * srf()) / srf());
        static const float erMs[kEr] = {3.1f, 5.3f, 7.9f, 11.3f, 14.7f, 19.1f, 23.9f, 29.3f};
        for (int k = 0; k < kEr; ++k) erD_[k] = std::max(1.0f, erMs[k] * size * 0.001f * srf());
        for (int c = 0; c < 2; ++c) {
            bright_[c].setup(dsp::Svf::Highpass, 400.0f, 0.6f, 0, sr_);
            dark_[c].setup(dsp::Svf::Lowpass, 2500.0f, 0.6f, 0, sr_);
        }
        mix_.setMix(pct(dsMix), snap);
    }
    double tailSeconds() const override { return std::min(8.0, 1.2 * static_cast<double>(p_[dsDecay]) + 0.1); }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float aSend = pct(dsAttackSend) * 1.4f, bSend = pct(dsBodySend);
        const bool gated = ip(dsMode) == 2 || fp(dsGate) > 0.5f;
        const int holdS = static_cast<int>(std::max(fp(dsGate), ip(dsMode) == 2 ? 180.0f : 0.0f) * 0.001f * srf());
        const float closeStep = 1.0f / (0.035f * srf()), openStep = 1.0f / (0.002f * srf());
        const float width = pct(dsWidth);
        for (int i = 0; i < n; ++i) {
            const float a = std::max(std::fabs(ch[0][i]), std::fabs(ch[1][i]));
            const float f = fastE_.process(a), s = slowE_.process(a);
            // transient mask in [0,1]: the attack layer is x*m, the body layer x*(1-m): they sum to x exactly
            const float m = f > 1e-6f ? std::clamp((f - s) / f * 1.6f, 0.0f, 1.0f) : 0.0f;
            if (gated) {
                if (onset_.process(a, 1.8f)) gateHold_ = holdS;
                if (gateHold_ > 0) { --gateHold_; gate_ = std::min(1.0f, gate_ + openStep); }
                else gate_ = std::max(0.0f, gate_ - closeStep);
            } else {
                gate_ = 1.0f;
            }
            float exc[2];
            for (int c = 0; c < 2; ++c) {
                const float x = ch[c][i];
                exc[c] = aSend * bright_[c].process(x * m) + bSend * dark_[c].process(x * (1.0f - m));
                er_[c].push(exc[c]);
            }
            // early reflections (alternating sides), then the room
            float erOut[2] = {0.0f, 0.0f};
            for (int k = 0; k < kEr; ++k) {
                const float gk = 0.55f * (1.0f - 0.07f * k);
                erOut[k & 1] += gk * er_[k & 1].readLinear(erD_[k]);
                erOut[(k + 1) & 1] += 0.35f * gk * er_[(k + 1) & 1].readLinear(erD_[k] * 1.07f);
            }
            float sv[N], sum = 0.0f;
            for (int j = 0; j < N; ++j) {
                const float v = line_[j].tap(len_[j]);
                damp_[j] += dampA_ * (v - damp_[j]);
                sv[j] = g_[j] * damp_[j];
                sum += sv[j];
            }
            const float h = 2.0f / N * sum;
            for (int j = 0; j < N; ++j) line_[j].push(sv[j] - h + 0.4f * (j & 1 ? exc[1] + 0.5f * erOut[1] : exc[0] + 0.5f * erOut[0]));
            float wl = 0.6f * erOut[0] + 0.5f * (sv[0] - sv[2] + sv[4] - sv[6]);
            float wr = 0.6f * erOut[1] + 0.5f * (sv[1] - sv[3] + sv[5] - sv[7]);
            const float gg = rcos(gate_);
            const float mid = 0.5f * (wl + wr) * gg, side = 0.5f * (wl - wr) * width * gg;
            ch[0][i] = mid + side;
            ch[1][i] = mid - side;
        }
        for (auto& z : damp_) z = dsp::flushTiny(z);
        for (int c = 0; c < 2; ++c) { bright_[c].flush(); dark_[c].flush(); }
        mix_.apply(ch, n);
    }
    dsp::DelayLine line_[N], er_[2];
    dsp::Svf bright_[2], dark_[2];
    dsp::EnvelopeFollower fastE_, slowE_;
    Onset onset_;
    float base_[N] = {}, g_[N] = {}, damp_[N] = {}, erD_[kEr] = {}, dampA_ = 0.5f, gate_ = 1.0f;
    int len_[N] = {}, gateHold_ = 0;
    MixStage mix_;
};

// ============================================================================================ Band Transients
enum { btLowAttack, btLowSustain, btMidAttack, btMidSustain, btHighAttack, btHighSustain, btLowX, btHighX, btSpeed, btClip, btOut, btMix };

class BandTransients final : public FxBase {
    void onPrepare() override {
        outS_.prepare(sr_, 30.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
    }
    void reset() override {
        for (int c = 0; c < 2; ++c) { lowX_[c].reset(); highX_[c].reset(); }
        for (int b = 0; b < 3; ++b) { fast_[b].reset(); slow_[b].reset(); g_[b] = 1.0f; }
        outS_.snap(dsp::dbToGain(fp(btOut)));
        mix_.reset();
    }
    void applyParams(bool snap) override {
        const float hx = std::max(fp(btHighX), fp(btLowX) * 2.0f);
        for (int c = 0; c < 2; ++c) { lowX_[c].setup(fp(btLowX), sr_); highX_[c].setup(hx, sr_); }
        const float sp = 1.6f - 1.2f * pct(btSpeed);  // speed scales every time constant
        static const float fa[3] = {1.5f, 0.6f, 0.25f}, sa[3] = {30.0f, 18.0f, 10.0f}, sr[3] = {260.0f, 180.0f, 120.0f};
        for (int b = 0; b < 3; ++b) {
            fast_[b].prepare(sr_, fa[b] * sp, sa[b] * sp);
            slow_[b].prepare(sr_, sa[b] * sp, sr[b] * sp);
        }
        outS_.setTarget(dsp::dbToGain(fp(btOut)));
        if (snap) outS_.snap(outS_.target());
        mix_.setMix(pct(btMix), snap);
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float att[3] = {fp(btLowAttack) * 0.01f, fp(btMidAttack) * 0.01f, fp(btHighAttack) * 0.01f};
        const float sus[3] = {fp(btLowSustain) * 0.01f, fp(btMidSustain) * 0.01f, fp(btHighSustain) * 0.01f};
        const bool clip = bp(btClip);
        const float gSm = 1.0f - std::exp(-1.0f / (0.0008f * srf()));
        for (int i = 0; i < n; ++i) {
            float band[3][2];
            for (int c = 0; c < 2; ++c) {
                const float x = ch[c][i];
                float lo, rest, hi, rest2;
                lowX_[c].process(x, lo, rest);
                highX_[c].process(x, rest2, hi);
                (void)rest; (void)rest2;
                band[0][c] = lo;
                band[2][c] = hi;
                band[1][c] = x - lo - hi;  // exact remainder: neutral settings reconstruct the input
            }
            float y[2] = {0.0f, 0.0f};
            for (int b = 0; b < 3; ++b) {
                const float a = std::max(std::fabs(band[b][0]), std::fabs(band[b][1]));  // linked
                const float f = fast_[b].process(a), s = slow_[b].process(a);
                const float r = std::log2(std::max(f, 1e-7f) / std::max(s, 1e-7f));
                const float tr = std::clamp(r, 0.0f, 1.0f), su = std::clamp(-r, 0.0f, 1.0f);
                const float gDb = 12.0f * att[b] * tr + 12.0f * sus[b] * su;
                g_[b] += gSm * (dsp::dbToGain(gDb) - g_[b]);
                y[0] += g_[b] * band[b][0];
                y[1] += g_[b] * band[b][1];
            }
            const float og = outS_.next();
            for (int c = 0; c < 2; ++c) {
                float v = y[c] * og;
                // peak safety: transparent below -1 dBFS, rounds peaks above it (attack boosts raise peaks)
                if (clip) { const float k = 0.89f; const float av = std::fabs(v); if (av > k) v = std::copysign(k + (1.0f - k) * std::tanh((av - k) / (1.0f - k)), v); }
                ch[c][i] = v;
            }
        }
        mix_.apply(ch, n);
    }
    dsp::Crossover lowX_[2], highX_[2];
    dsp::EnvelopeFollower fast_[3], slow_[3];
    float g_[3] = {1, 1, 1};
    dsp::Smoother outS_;
    MixStage mix_;
};

// ============================================================================================ Hit Melody
enum { hmKey, hmScale, hmPattern, hmRange, hmOctave, hmDecay, hmBright, hmSensitivity, hmGlide, hmSeed, hmMix };
constexpr int kHmScales[7][8] = {
    {0, 3, 5, 7, 10, -1, -1, -1}, {0, 2, 4, 7, 9, -1, -1, -1}, {0, 2, 3, 5, 7, 8, 10, -1}, {0, 2, 3, 5, 7, 9, 10, -1},
    {0, 1, 3, 5, 7, 8, 10, -1}, {0, 2, 4, 6, 8, 10, -1, -1}, {0, 7, -1, -1, -1, -1, -1, -1}};

class HitMelody final : public FxBase {
    struct Res {  // all-pole two-pole resonator: an impulse of area A rings at amplitude ~ A * k (b = k sin w), r < 1
        float b = 0, a1 = 0, a2 = 0, y1 = 0, y2 = 0;
        void set(float hz, float t60, float sr, float k) {
            const float w = dsp::kTwoPi * std::clamp(hz, 20.0f, 0.45f * sr) / sr;
            const float r = std::min(0.99995f, std::pow(10.0f, -3.0f / (std::max(t60, 0.02f) * sr)));
            b = k * std::sin(w);
            a1 = 2.0f * r * std::cos(w);
            a2 = -r * r;
        }
        float process(float x) {
            const float y = b * x + a1 * y1 + a2 * y2;
            y2 = y1; y1 = dsp::flushTiny(y);
            return y;
        }
        void reset() { y1 = y2 = 0.0f; }
    };
    void onPrepare() override {
        onset_.prepare(sr_, 70.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
    }
    void reset() override {
        for (auto& v : voice_) for (auto& r : v.res) r.reset();
        for (auto& v : voice_) { v.exc = 0.0f; v.hz = 220.0f; v.target = 220.0f; v.pending = false; v.fade = 0; v.dirty = true; }
        cur_ = 0;
        step_ = 0;
        walk_ = 0;
        excLp_ = 0.0f;
        onset_.reset();
        excEnv_ = 0.0f;
        mix_.reset();
    }
    void applyParams(bool snap) override { mix_.setMix(pct(hmMix), snap); }
    double tailSeconds() const override { return std::min(6.0, 1.3 * static_cast<double>(p_[hmDecay]) + 0.1); }
    float noteHz(int idx) const {
        const int scale = std::clamp(ip(hmScale), 0, 6);
        int deg = 0;
        while (deg < 8 && kHmScales[scale][deg] >= 0) ++deg;
        const int range = std::clamp(static_cast<int>(std::lround(fp(hmRange))), 1, 4);
        const int total = deg * range;
        idx = ((idx % total) + total) % total;
        const int semis = kHmScales[scale][idx % deg] + 12 * (idx / deg);
        const int midi = 12 * (4 + std::clamp(static_cast<int>(std::lround(fp(hmOctave))), -2, 2)) + std::clamp(ip(hmKey), 0, 11) + semis;
        return 440.0f * std::exp2((static_cast<float>(midi) - 69.0f) / 12.0f);
    }
    int nextIndex() {
        const int scale = std::clamp(ip(hmScale), 0, 6);
        int deg = 0;
        while (deg < 8 && kHmScales[scale][deg] >= 0) ++deg;
        const int total = deg * std::clamp(static_cast<int>(std::lround(fp(hmRange))), 1, 4);
        const uint32_t seed = static_cast<uint32_t>(std::lround(fp(hmSeed)));
        ++step_;
        switch (std::clamp(ip(hmPattern), 0, 4)) {
            case 0: return step_ % total;
            case 1: return total - 1 - step_ % total;
            case 2: { const int p = 2 * total - 2; const int k = p > 0 ? step_ % p : 0; return k < total ? k : p - k; }
            case 3: walk_ += hashU(seed, static_cast<uint32_t>(step_)) < 0.5f ? -1 : 1; walk_ = std::clamp(walk_, 0, total - 1); return walk_;
            default: return static_cast<int>(hashU(seed + 3u, static_cast<uint32_t>(step_ % 16)) * static_cast<float>(total)) % total;
        }
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float t60 = fp(hmDecay);
        const float bright = pct(hmBright);
        if (excA_ <= 0.0f) excA_ = 1.0f - std::exp(-dsp::kTwoPi * (300.0f + 12000.0f * bright * bright) / srf());
        const float ratio = 1.4f + 4.0f * (1.0f - pct(hmSensitivity));
        const float glideC = fp(hmGlide) > 0.5f ? 1.0f - std::exp(-1.0f / (fp(hmGlide) * 0.001f * srf())) : 1.0f;
        static const float partial[3] = {1.0f, 2.0f, 3.01f}, plev[3] = {1.0f, 0.5f, 0.3f};
        const int fadeLen = std::max(1, static_cast<int>(0.003f * srf()));
        for (int i = 0; i < n; ++i) {
            const float x = 0.5f * (ch[0][i] + ch[1][i]);
            float strike = 0.0f;
            if (onset_.process(std::fabs(x), ratio)) {
                // the new note goes to the next voice; the others keep ringing. A voice that is still ringing is faded
                // out over 3 ms before it is retuned (V5 Phase 6: retuning a ringing two-pole changes its amplitude
                // abruptly), so the strike waits for that fade
                cur_ = (cur_ + 1) % kVoices;
                auto& v = voice_[cur_];
                v.pendingHz = noteHz(nextIndex());
                v.pendingStrike = std::min(1.0f, 2.0f * onset_.fast.value());  // the hit's level strikes the note
                v.fade = std::fabs(v.res[0].y1) + std::fabs(v.res[0].y2) > 1e-4f ? fadeLen : 0;
                v.pending = true;
            }
            for (int k = 0; k < kVoices; ++k) {
                auto& v = voice_[k];
                if (!v.pending) continue;
                if (v.fade > 0) { --v.fade; continue; }
                for (auto& rr : v.res) rr.reset();
                v.target = v.pendingHz;
                if (glideC >= 1.0f || v.hz <= 0.0f) v.hz = v.target;
                v.dirty = true;
                v.pending = false;
                if (k == cur_) {
                    strike = v.pendingStrike;
                    // V5 Phase 6: the strike is band-limited to ~2x the note's highest partial, so even the top partial
                    // starts continuously (a wider-band impulse made a click-like step on smooth sources)
                    const float fc = std::min(300.0f + 12000.0f * bright * bright, 2.0f * 3.01f * v.target);
                    excA_ = 1.0f - std::exp(-dsp::kTwoPi * std::max(fc, 60.0f) / srf());
                }
            }
            // band-limited strike: a one-pole low-pass keeps the impulse's area (the fundamental rings at the same level),
            // Brightness decides how much the upper partials are struck
            excLp_ += excA_ * (strike - excLp_);
            (void)excEnv_;
            float out = 0.0f;
            for (int k = 0; k < kVoices; ++k) {
                auto& v = voice_[k];
                if (v.hz != v.target) {
                    v.hz += glideC * (v.target - v.hz);
                    if (std::fabs(v.hz - v.target) < 0.01f) v.hz = v.target;
                    v.dirty = true;
                }
                if (v.dirty || (i == 0 && v.t60 != t60)) {
                    for (int p = 0; p < 3; ++p) v.res[p].set(v.hz * partial[p], t60 / (1.0f + 0.6f * p), srf(), 0.5f);
                    v.t60 = t60;
                    v.dirty = false;
                }
                const float e = k == cur_ ? excLp_ : 0.0f;
                const float fg = v.pending && v.fade > 0 ? rcos(static_cast<float>(v.fade) / static_cast<float>(fadeLen)) : (v.pending ? 0.0f : 1.0f);
                for (int p = 0; p < 3; ++p) out += fg * plev[p] * v.res[p].process(e);
            }
            // a strike rings at ~0.5 x its level per partial (0.9 x for the three together); overlapping notes and
            // rolls are rounded above -1 dBFS (each resonator is stable on its own, r < 1)
            const float ao = std::fabs(out);
            if (ao > 0.9f) out = std::copysign(0.9f + 0.1f * std::tanh((ao - 0.9f) * 10.0f), out);
            ch[0][i] = out;
            ch[1][i] = out;
        }
        mix_.apply(ch, n);
    }
    static constexpr int kVoices = 3;
    struct Voice { Res res[3]; float hz = 220.0f, target = 220.0f, t60 = -1.0f, exc = 0.0f, pendingHz = 220.0f, pendingStrike = 0.0f; int fade = 0; bool dirty = true, pending = false; };
    Voice voice_[kVoices];
    Onset onset_;
    int cur_ = 0, step_ = 0, walk_ = 0;
    float excLp_ = 0.0f, excEnv_ = 0.0f, excA_ = -1.0f;
    MixStage mix_;
};

}  // namespace

FxDescriptor describePulseShaper() {
    FxDescriptor d;
    d.typeId = "pulse_shaper";
    d.name = "Pulse Shaper";
    d.family = FxFamily::Modulation;
    d.tooltip = "Tremolo as a groove: a 16-step volume pattern with swing, accents, ghost notes, probability and humanised timing, on the host grid. Can move only the lows, mids or highs, and can restart on every hit.";
    d.params = {
        P::choice("pattern", "Pattern", {"Eighths", "Sixteenths", "Off-beats", "Dotted", "Tresillo", "Clave", "Gallop", "Skip", "Broken", "Stutter Run", "Half-time", "Triplet Feel", "Ghost Notes", "Syncopa", "Machine", "Seeded"}, 1,
                  "The rhythm of the volume steps. Seeded writes a new pattern for every bar from the Seed (always the same for the same bar)."),
        P::choice("division", "Step", divisionLabels(), 4, "The length of one step (1/16 = sixteen steps per bar)."),
        P::flt("steps", "Steps", "", 2.0f, 16.0f, 16.0f, "How many steps before the pattern repeats (odd lengths drift against the bar)."),
        P::pct("depth", "Depth", 85.0f, "How far the volume drops between steps (100 % = to silence)."),
        P::flt("attack", "Attack", "ms", 0.2f, 60.0f, 1.5f, "How fast each step opens: short = sharp and percussive (still click-free), long = soft swells."),
        P::pct("gate", "Gate", 55.0f, "How long each step stays open before it releases."),
        P::flt("release", "Release", "ms", 1.0f, 600.0f, 45.0f, "How each step fades out after its gate: short = choppy, long = a breathing pulse."),
        P::pct("swing", "Swing", 0.0f, "Delays every second step for a shuffled, swung feel."),
        P::pct("accent", "Accent", 50.0f, "Contrast between accented, normal and ghost steps."),
        P::pct("chance", "Chance", 100.0f, "Probability that a step plays (the first step of a bar always does). Each bar repeats exactly on replay."),
        P::adv(P::pct("humanize", "Humanize", 0.0f, "Small, repeatable timing offsets per step (up to 6 ms).")),
        P::choice("stereo", "Stereo", {"Linked", "Offset", "Ping-Pong", "Mirror"}, 0, "Linked: both sides together. Offset: the right side one step later. Ping-Pong: steps alternate left / right. Mirror: the right side plays the gaps."),
        P::choice("band", "Band", {"Full", "Lows", "Mids", "Highs"}, 0, "Which part of the spectrum pulses (the rest stays steady), e.g. only the hats, or only the bass body."),
        P::adv(P::toggle("reset", "Reset on Hit", false, "Without a playing host, every detected hit restarts the pattern from step 1.")),
        P::adv(P::flt("seed", "Seed", "", 0.0f, 99.0f, 5.0f, "Chooses the random choices (Chance, Humanize, Seeded pattern).")),
        P::pct("mix", "Mix", 100.0f, "Blend of the steady and the pulsing sound."),
    };
    d.create = [] { return std::make_unique<PulseShaper>(); };
    return d;
}

FxDescriptor describeDrumCrush() {
    FxDescriptor d;
    d.typeId = "drum_crush";
    d.name = "Drum Crush";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "A parallel drum-bus crusher: heavy compression whose squash also drives the distortion, blended under the untouched attack. Big, dirty body with the hits still clear.";
    d.params = {
        P::pct("crush", "Crush", 55.0f, "How hard the drums are squashed (lower threshold, higher ratio)."),
        P::flt("drive", "Drive", "dB", 0.0f, 30.0f, 9.0f, "How hard the crushed layer is distorted."),
        P::choice("character", "Character", {"Warm", "Hard", "Fuzz"}, 0, "Warm: round saturation. Hard: a firmer, brighter clip. Fuzz: asymmetric, gritty."),
        P::choice("release", "Release", {"Fast", "Medium", "Slow", "Auto"}, 1, "How quickly the squash lets go: Fast pumps and breathes, Slow sits flat, Auto adapts to the material."),
        P::adv(P::pct("react", "React", 50.0f, "How much the squash increases the distortion (the dirt follows the hits).")),
        P::flt("tone", "Tone", "%", -100.0f, 100.0f, -15.0f, "Darker (left) or brighter (right) crushed layer."),
        P::flt("body", "Body", "dB", -6.0f, 9.0f, 2.0f, "Low-end weight of the crushed layer."),
        P::pct("blend", "Blend", 50.0f, "How much crushed layer sits under the dry drums (the dry attack stays intact)."),
        P::flt("out", "Output", "dB", -18.0f, 12.0f, 0.0f, "Output level."),
    };
    d.create = [] { return std::make_unique<DrumCrush>(); };
    return d;
}

FxDescriptor describeDrumSpace() {
    FxDescriptor d;
    d.typeId = "drum_space";
    d.name = "Drum Space";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "A short room built for drums: the attack of each hit excites a bright room and the body a darker one. Tight rooms, live rooms, booths, chambers and gated rooms.";
    d.params = {
        P::choice("mode", "Room", {"Tight Room", "Live Room", "Gated", "Booth", "Dark Chamber"}, 0, "The character of the space. Gated: the tail is cut shortly after every hit."),
        P::pct("size", "Size", 40.0f, "How big the room is."),
        P::logf("decay", "Decay", "s", 0.1f, 4.0f, 0.6f, "How long the room rings."),
        P::pct("attack_send", "Attack Send", 70.0f, "How much the hits' attacks excite the room (snap, crack, click)."),
        P::pct("body_send", "Body Send", 35.0f, "How much the hits' body excites the room (tone, sustain)."),
        P::flt("gate", "Gate Hold", "ms", 0.0f, 600.0f, 0.0f, "Above 0: the room closes this long after every hit (gated reverb)."),
        P::flt("tone", "Tone", "%", -100.0f, 100.0f, 0.0f, "Darker or brighter room."),
        P::flt("width", "Width", "%", 0.0f, 150.0f, 100.0f, "Stereo width of the room."),
        P::pct("mix", "Mix", 30.0f, "Blend of the dry drums and the room."),
    };
    d.create = [] { return std::make_unique<DrumSpace>(); };
    return d;
}

FxDescriptor describeBandTransients() {
    FxDescriptor d;
    d.typeId = "band_transients";
    d.name = "Band Transients";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Attack and sustain shaping in three bands: e.g. a harder kick attack with a shorter low tail, while the cymbals lose sustain. Works on the whole mixed signal.";
    d.params = {
        P::flt("low_attack", "Low Attack", "%", -100.0f, 100.0f, 0.0f, "Attack of the lows (kick punch)."),
        P::flt("low_sustain", "Low Sustain", "%", -100.0f, 100.0f, 0.0f, "Sustain of the lows (boom, tail)."),
        P::flt("mid_attack", "Mid Attack", "%", -100.0f, 100.0f, 0.0f, "Attack of the mids (snare crack, stick)."),
        P::flt("mid_sustain", "Mid Sustain", "%", -100.0f, 100.0f, 0.0f, "Sustain of the mids (body, room)."),
        P::flt("high_attack", "High Attack", "%", -100.0f, 100.0f, 0.0f, "Attack of the highs (hat tick, snap)."),
        P::flt("high_sustain", "High Sustain", "%", -100.0f, 100.0f, 0.0f, "Sustain of the highs (cymbal wash)."),
        P::adv(P::logf("low_x", "Low Split", "Hz", 60.0f, 500.0f, 180.0f, "Where the lows end.")),
        P::adv(P::logf("high_x", "High Split", "Hz", 1500.0f, 10000.0f, 4500.0f, "Where the highs begin.")),
        P::pct("speed", "Speed", 50.0f, "Detection speed: fast = only the sharpest attacks, slow = broader punches."),
        P::toggle("clip", "Peak Safety", true, "Rounds peaks above -1 dBFS (attack boosts raise peaks)."),
        P::flt("out", "Output", "dB", -18.0f, 12.0f, 0.0f, "Output level."),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and the shaped sound."),
    };
    d.create = [] { return std::make_unique<BandTransients>(); };
    return d;
}

FxDescriptor describeHitMelody() {
    FxDescriptor d;
    d.typeId = "hit_melody";
    d.name = "Hit Melody";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Turns hits into a melody: every detected attack strikes a tuned resonance at the next note of a pattern in your key and scale.";
    d.params = {
        P::choice("key", "Key", {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 9, "Root note."),
        P::choice("scale", "Scale", {"Minor Pentatonic", "Major Pentatonic", "Minor", "Dorian", "Phrygian", "Whole Tone", "Root + Fifth"}, 0, "The notes the hits can land on."),
        P::choice("pattern", "Pattern", {"Up", "Down", "Up / Down", "Random Walk", "Seeded"}, 0, "How each hit moves to the next note."),
        P::flt("range", "Range", "oct", 1.0f, 4.0f, 2.0f, "How many octaves the pattern spans."),
        P::flt("octave", "Octave", "", -2.0f, 2.0f, 0.0f, "Shifts the whole melody up or down."),
        P::logf("decay", "Ring", "s", 0.05f, 4.0f, 0.6f, "How long each note rings."),
        P::pct("bright", "Brightness", 50.0f, "How much of the hit's high end strikes the note (bright, plucky vs soft, mallet-like)."),
        P::pct("sensitivity", "Sensitivity", 60.0f, "How easily a sound counts as a new hit."),
        P::adv(P::flt("glide", "Glide", "ms", 0.0f, 200.0f, 0.0f, "Slides each voice into its new note.")),
        P::adv(P::flt("seed", "Seed", "", 0.0f, 99.0f, 4.0f, "Chooses the random walk / seeded note order.")),
        P::pct("mix", "Mix", 55.0f, "Blend of the drums and the melody."),
    };
    d.create = [] { return std::make_unique<HitMelody>(); };
    return d;
}

}  // namespace ih1::fx
