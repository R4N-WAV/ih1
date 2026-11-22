// V5 feedback round: four atmospheric processors (Phase 2). Original IH1 designs; the V5 research supplement's
// product manuals and FDN papers were read as workflow / DSP references only. Nothing was copied.
//
//   Cloud Chamber        a 16-line feedback delay network with an orthogonal (normalised Hadamard) mixing matrix and a
//                        THREE-BAND absorption per line: low, mid and high frequencies decay at their own T60, so the
//                        lows can inhabit a long space while the highs die quickly (or the reverse). Density morphs the
//                        input from sparse discrete reflections to a dense wash; Bloom adds long input all-passes so the
//                        wet field swells in; Drift slowly modulates four lines; Freeze is a stable, lossless hold.
//   Reflection Sculptor  early reflections as an instrument: up to 24 taps from a pattern (room, wall, corridor, slap
//                        cluster, scatter, ramp, rhythmic, metal box), each with its own level, pan and distance
//                        darkening, slow tap motion and optional diffusion. A pure FIR: no recursion at all.
//   Reverse Bloom        the classic studio reverse-reverb swell, live: the input excites an internal reverb, and the
//                        reverb's tail is played backwards one chunk later, so every phrase is preceded by its own
//                        rising, inhaled tail. No recursion other than the bounded internal reverb.
//   Swell                volume-pedal blooms: each detected attack is removed (3 ms look-ahead, reported) and the sound
//                        fades in over the Rise time along a chosen curve, optionally into a diffuse all-pass wash.
//
// Stability (V5 Phase 5): Cloud Chamber's per-line absorption filter is normalised so that its magnitude never
// exceeds the largest band gain (< 1), and freeze crossfades toward an exact identity (|H| = 1) with the input
// muted: an orthogonal matrix with |H| <= 1 cannot gain energy. Reverse Bloom's internal reverb is a Householder FDN
// with absorption < 1. Swell's wash is a cascade of all-passes (|H| = 1). Reflection Sculptor has no loop.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

#include "ih1/dsp/DelayLine.h"
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

// In-place normalised Hadamard transform of 16 values (orthogonal: preserves energy exactly).
inline void hadamard16(float* x) {
    for (int len = 1; len < 16; len <<= 1)
        for (int i = 0; i < 16; i += len << 1)
            for (int j = i; j < i + len; ++j) {
                const float a = x[j], b = x[j + len];
                x[j] = a + b;
                x[j + len] = a - b;
            }
    for (int i = 0; i < 16; ++i) x[i] *= 0.25f;
}

// ============================================================================================ Cloud Chamber
enum { ccSize, ccDecay, ccLowDecay, ccHighDecay, ccDensity, ccBloom, ccDrift, ccPredelay, ccLowCut, ccWidth, ccFreeze, ccMix };

class CloudChamber final : public FxBase {
    static constexpr int N = 16;
    static constexpr float kLowX = 300.0f, kHighX = 3500.0f;
    void onPrepare() override {
        static const float ms[N] = {29.7f, 33.1f, 37.9f, 41.3f, 45.7f, 49.1f, 53.9f, 58.3f, 62.9f, 67.7f, 71.3f, 77.9f, 83.1f, 89.3f, 94.7f, 101.9f};
        for (int j = 0; j < N; ++j) {
            baseMs_[j] = ms[j];
            line_[j].prepare(static_cast<int>(ms[j] * 1.7f * 0.001f * srf()) + 128);
        }
        static const float dms[4] = {4.7f, 6.1f, 8.3f, 11.9f};
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < 4; ++k) diff_[c][k].prepare(static_cast<int>(dms[k] * (c ? 1.13f : 1.0f) * 0.001f * srf()) + 2);
            for (int k = 0; k < 3; ++k) bloom_[c][k].prepare(static_cast<int>((61.0f + 37.0f * k) * (c ? 1.07f : 1.0f) * 0.001f * srf() * 1.7f) + 2);
            pre_[c].prepare(static_cast<int>(0.31 * sr_) + 4);
        }
        for (int k = 0; k < 4; ++k) {
            lfo_[k].prepare(sr_, 0xC1D0u + static_cast<uint32_t>(k));
            lfo_[k].setRate(0.07f + 0.031f * static_cast<float>(k));
        }
        aLow_ = 1.0f - std::exp(-dsp::kTwoPi * kLowX / srf());
        aHigh_ = 1.0f - std::exp(-dsp::kTwoPi * kHighX / srf());
        freezeS_.prepare(sr_, 90.0f);
        inGate_.prepare(sr_, 60.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
    }
    void reset() override {
        for (auto& l : line_) l.reset();
        for (auto& c : diff_) for (auto& a : c) a.reset();
        for (auto& c : bloom_) for (auto& a : c) a.reset();
        for (auto& p : pre_) p.reset();
        for (auto& h : hp_) h.reset();
        for (int j = 0; j < N; ++j) zl_[j] = zh_[j] = 0.0f;
        freezeS_.snap(bp(ccFreeze) ? 1.0f : 0.0f);
        inGate_.snap(bp(ccFreeze) ? 0.0f : 1.0f);
        mix_.reset();
    }
    void applyParams(bool snap) override {
        const float size = 0.35f + 1.3f * pct(ccSize);
        const float t60 = std::clamp(fp(ccDecay), 0.1f, 60.0f);
        const float tLow = std::clamp(t60 * fp(ccLowDecay), 0.1f, 60.0f), tHigh = std::clamp(t60 * fp(ccHighDecay), 0.05f, 60.0f);
        for (int j = 0; j < N; ++j) {
            delay_[j] = std::max(8.0f, baseMs_[j] * size * 0.001f * srf());
            const float sec = delay_[j] / srf();
            gL_[j] = std::pow(10.0f, -3.0f * sec / tLow);
            gM_[j] = std::pow(10.0f, -3.0f * sec / t60);
            gH_[j] = std::pow(10.0f, -3.0f * sec / tHigh);
            normaliseLine(j);
        }
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 3; ++k) bloom_[c][k].setDelay(std::max(1, static_cast<int>((61.0f + 37.0f * k) * (c ? 1.07f : 1.0f) * 0.001f * srf() * size)));
        for (auto& h : hp_) h.setup(dsp::Svf::Highpass, fp(ccLowCut), 0.6f, 0, sr_);
        freezeS_.setTarget(bp(ccFreeze) ? 1.0f : 0.0f);
        inGate_.setTarget(bp(ccFreeze) ? 0.0f : 1.0f);
        if (snap) { freezeS_.snap(freezeS_.target()); inGate_.snap(inGate_.target()); }
        mix_.setMix(pct(ccMix), snap);
    }
    // Per-line 3-band absorption H = gL A + gM (B - A) + gH (1 - B) with one-pole low-passes A (300 Hz) and B (3.5 kHz).
    // The band split is not exactly magnitude-complementary, so |H| can exceed the largest band gain where the bands
    // overlap; the three gains are scaled together so that max |H| over the spectrum <= max(gL, gM, gH) < 1.
    void normaliseLine(int j) {
        const float gmax = std::max({gL_[j], gM_[j], gH_[j]});
        float peak = 0.0f;
        for (int k = 0; k <= 40; ++k) {
            const float f = 20.0f * std::pow(1000.0f, static_cast<float>(k) / 40.0f);
            const float w = dsp::kTwoPi * std::min(f, 0.49f * srf()) / srf();
            const std::complex<float> z1 = std::polar(1.0f, -w);
            const std::complex<float> A = aLow_ / (1.0f - (1.0f - aLow_) * z1), B = aHigh_ / (1.0f - (1.0f - aHigh_) * z1);
            const std::complex<float> H = gL_[j] * A + gM_[j] * (B - A) + gH_[j] * (1.0f - B);
            peak = std::max(peak, std::abs(H));
        }
        const float s = peak > gmax ? gmax / peak * 0.999f : 1.0f;
        gL_[j] *= s; gM_[j] *= s; gH_[j] *= s;
    }
    double tailSeconds() const override {
        if (p_[ccFreeze] >= 0.5f) return 600.0;
        return std::min(60.0, 1.2 * static_cast<double>(p_[ccDecay]) * std::max(1.0f, p_[ccLowDecay]) + 0.4);
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float dens = pct(ccDensity), bloom = pct(ccBloom);
        const float dg = 0.25f + 0.5f * dens;
        const float modD = pct(ccDrift) * 0.0016f * srf();
        const float pre = std::max(0.0f, fp(ccPredelay) * 0.001f * srf());
        const float width = pct(ccWidth);
        constexpr float kOut = 0.42f;
        for (int i = 0; i < n; ++i) {
            const float fz = freezeS_.next(), ig = inGate_.next();
            float in[2];
            for (int c = 0; c < 2; ++c) {
                pre_[c].push(hp_[c].process(ch[c][i]));
                float x = pre_[c].readLinear(std::max(1.0f, pre)) * ig;
                float d = x;
                for (auto& a : diff_[c]) d = a.process(d, dg);
                x = (1.0f - dens) * x + dens * d;  // sparse (discrete) -> dense (diffused) onset
                if (bloom > 1e-4f) {
                    float b = x;
                    for (auto& a : bloom_[c]) b = a.process(b, 0.55f);
                    x = (1.0f - bloom) * x + bloom * b;  // the wash swells in over ~0.1-0.4 s
                }
                in[c] = x;
            }
            float s[N];
            for (int j = 0; j < N; ++j) {
                // Drift fades out while frozen: the frozen field must be exactly lossless (integer reads, |H| = 1)
                const float d = delay_[j] + (j < 4 ? (1.0f - fz) * modD * (0.5f + 0.5f * lfo_[j].next()) : 0.0f);
                const float v = (j < 4 && fz < 1.0f) ? line_[j].readLinear(d) : line_[j].tap(static_cast<int>(d));
                // three-band absorption (normalised, |H| < 1), crossfaded to identity by Freeze (|.| <= 1)
                zl_[j] += aLow_ * (v - zl_[j]);
                zh_[j] += aHigh_ * (v - zh_[j]);
                const float a = gL_[j] * zl_[j] + gM_[j] * (zh_[j] - zl_[j]) + gH_[j] * (v - zh_[j]);
                s[j] = (1.0f - fz) * a + fz * v;
            }
            float outL = 0.0f, outR = 0.0f;
            for (int j = 0; j < N; ++j) {
                const float sg = ((j * 7) & 4) ? -1.0f : 1.0f;
                if (j & 1) outR += sg * s[j]; else outL += sg * s[j];
            }
            hadamard16(s);
            for (int j = 0; j < N; ++j) line_[j].push(s[j] + 0.35f * in[j & 1] * (((j >> 1) & 1) ? -1.0f : 1.0f));
            outL *= kOut;
            outR *= kOut;
            const float mid = 0.5f * (outL + outR), side = 0.5f * (outL - outR) * width;
            ch[0][i] = mid + side;
            ch[1][i] = mid - side;
        }
        for (int j = 0; j < N; ++j) { zl_[j] = dsp::flushTiny(zl_[j]); zh_[j] = dsp::flushTiny(zh_[j]); }
        for (auto& h : hp_) h.flush();
        mix_.apply(ch, n);
    }
    dsp::DelayLine line_[N], pre_[2];
    dsp::Allpass diff_[2][4], bloom_[2][3];
    dsp::Lfo lfo_[4];
    dsp::Svf hp_[2];
    dsp::Smoother freezeS_, inGate_;
    float baseMs_[N] = {}, delay_[N] = {}, gL_[N] = {}, gM_[N] = {}, gH_[N] = {}, zl_[N] = {}, zh_[N] = {};
    float aLow_ = 0.04f, aHigh_ = 0.37f;
    MixStage mix_;
};

// ============================================================================================ Reflection Sculptor
enum { rsPattern, rsTime, rsTaps, rsSpread, rsDecay, rsTone, rsMotion, rsDiffusion, rsPredelay, rsSeed, rsMix };

class ReflectionSculptor final : public FxBase {
    static constexpr int kTaps = 24;
    void onPrepare() override {
        for (auto& l : line_) l.prepare(static_cast<int>(0.56 * sr_) + 64);
        for (int c = 0; c < 2; ++c) {
            ap_[c][0].prepare(static_cast<int>((c ? 3.7f : 3.1f) * 0.001f * srf()) + 2);
            ap_[c][1].prepare(static_cast<int>((c ? 9.3f : 7.9f) * 0.001f * srf()) + 2);
        }
        for (int k = 0; k < kTaps; ++k) {
            lfo_[k].prepare(sr_, 0x5EF0u + static_cast<uint32_t>(k));
            lfo_[k].setRate(0.05f + 0.013f * static_cast<float>(k));
            lfo_[k].setShape(dsp::Lfo::Drift);
        }
        timeS_.prepare(sr_, 80.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
    }
    void reset() override {
        for (auto& l : line_) l.reset();
        for (auto& c : ap_) for (auto& a : c) a.reset();
        for (auto& z : lp_) z = 0.0f;
        for (auto& g : gainS_) g = -1.0f;
        timeS_.snap(fp(rsTime));
        mix_.reset();
    }
    void applyParams(bool snap) override {
        build();
        timeS_.setTarget(fp(rsTime));
        if (snap) timeS_.snap(fp(rsTime));
        mix_.setMix(pct(rsMix), snap);
    }
    // Deterministic tap layout per pattern: position 0..1 of the span, level, pan.
    void build() {
        const int pat = std::clamp(ip(rsPattern), 0, 7);
        const int taps = std::clamp(static_cast<int>(std::lround(fp(rsTaps))), 2, kTaps);
        const uint32_t seed = static_cast<uint32_t>(std::lround(fp(rsSeed))) * 977u + static_cast<uint32_t>(pat) * 131u;
        const float spread = pct(rsSpread), decay = pct(rsDecay);
        float pw = 0.0f;
        for (int k = 0; k < kTaps; ++k) {
            float pos = 0.0f, lvl = 0.0f, pan = 0.0f;
            if (k < taps) {
                const float u = static_cast<float>(k + 1) / static_cast<float>(taps);
                const float r = hashU(seed, static_cast<uint32_t>(k)), r2 = hashU(seed + 7u, static_cast<uint32_t>(k));
                switch (pat) {
                    case 0: pos = std::pow(u, 1.6f) * (0.85f + 0.3f * r); pan = r2 * 2.0f - 1.0f; break;            // Room
                    case 1: pos = 0.08f + 0.12f * r + (k > taps / 2 ? 0.6f : 0.0f) * u; pan = (k & 1) ? 0.7f : -0.7f; break;  // Studio Wall
                    case 2: pos = u; pan = (k & 1) ? 0.35f : -0.35f; break;                                          // Corridor
                    case 3: pos = 0.75f + 0.25f * u * r; pan = r2 * 2.0f - 1.0f; break;                               // Slap Cluster
                    case 4: pos = r; pan = r2 * 2.0f - 1.0f; break;                                                   // Scatter
                    case 5: pos = u; pan = (k & 1) ? u : -u; break;                                                   // Ramp Up
                    case 6: pos = std::round(u * 8.0f) / 8.0f; pan = (static_cast<int>(std::round(u * 8.0f)) & 1) ? 0.8f : -0.8f; break;  // Rhythmic
                    default: pos = 0.05f + 0.3f * u + 0.02f * r; pan = std::sin(7.0f * u) * 0.6f; break;             // Metal Box
                }
                pos = std::clamp(pos, 0.004f, 1.0f);
                // level: farther taps are quieter (Decay); Ramp Up rises instead
                lvl = pat == 5 ? 0.25f + 0.75f * u : std::pow(1.0f - 0.92f * decay * pos, 2.0f);
                lvl *= 0.75f + 0.25f * hashU(seed + 13u, static_cast<uint32_t>(k));
            }
            pos_[k] = pos;
            lvl_[k] = lvl;
            pan_[k] = std::clamp(pan * spread, -1.0f, 1.0f);
            pw += lvl * lvl;
        }
        // loudness: the summed power of the taps is normalised, so the tap count changes texture, not level
        norm_ = 0.9f / std::sqrt(std::max(pw, 1e-3f));
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float motion = pct(rsMotion) * 0.003f * srf();
        const float pre = fp(rsPredelay) * 0.001f * srf();
        const float diff = pct(rsDiffusion);
        // distance darkening: the far end of the span is low-passed down to Tone; near taps stay open
        const float toneA = 1.0f - std::exp(-dsp::kTwoPi * fp(rsTone) / srf());
        const float cap = static_cast<float>(line_[0].capacity() - 4);
        for (int i = 0; i < n; ++i) {
            const float span = timeS_.next() * 0.001f * srf();
            line_[0].push(ch[0][i]);
            line_[1].push(ch[1][i]);
            float out[2] = {0.0f, 0.0f};
            for (int k = 0; k < kTaps; ++k) {
                if (lvl_[k] <= 0.0f && gainS_[k] <= 0.0f) continue;
                // per-tap level glides (pattern / tap-count changes never step)
                float& g = gainS_[k];
                if (g < 0.0f) g = lvl_[k];
                g += 0.002f * (lvl_[k] - g);
                const float m = motion > 0.0f ? motion * lfo_[k].next() : 0.0f;
                const float d = std::clamp(pre + pos_[k] * span + m, 1.0f, cap);
                const float a = toneA + (1.0f - toneA) * (1.0f - pos_[k]);
                float gl, gr;
                dsp::panGains(pan_[k], gl, gr);
                for (int c = 0; c < 2; ++c) {
                    const float v = line_[c].readLinear(d);
                    float& z = lp_[k * 2 + c];
                    z += a * (v - z);
                    out[c] += g * z * (c ? gr : gl) * 1.41421356f;
                }
            }
            for (int c = 0; c < 2; ++c) {
                float y = out[c] * norm_;
                if (diff > 1e-4f) {
                    float dd = y;
                    for (auto& ap : ap_[c]) dd = ap.process(dd, 0.35f + 0.35f * diff);
                    y = (1.0f - diff) * y + diff * dd;
                }
                ch[c][i] = y;
            }
        }
        for (auto& z : lp_) z = dsp::flushTiny(z);
        mix_.apply(ch, n);
    }
    dsp::DelayLine line_[2];
    dsp::Allpass ap_[2][2];
    dsp::Lfo lfo_[kTaps];
    dsp::Smoother timeS_;
    float pos_[kTaps] = {}, lvl_[kTaps] = {}, pan_[kTaps] = {}, gainS_[kTaps] = {}, lp_[kTaps * 2] = {};
    float norm_ = 1.0f;
    MixStage mix_;
};

// ============================================================================================ Reverse Bloom
enum { rbSync, rbLength, rbDivision, rbTail, rbTone, rbDensity, rbWidth, rbMix };

class ReverseBloom final : public FxBase {
    static constexpr int N = 8;
    static constexpr double kMaxLen = 4.2;
    void onPrepare() override {
        static const float ms[N] = {23.3f, 29.9f, 35.3f, 41.9f, 47.1f, 53.7f, 61.1f, 67.3f};
        for (int j = 0; j < N; ++j) { line_[j].prepare(static_cast<int>(ms[j] * 0.001f * srf()) + 8); len_[j] = static_cast<int>(ms[j] * 0.001f * srf()); }
        for (int c = 0; c < 2; ++c) {
            ring_[c].prepare(static_cast<int>(2.0 * kMaxLen * sr_) + 64);
            for (int k = 0; k < 3; ++k) diff_[c][k].prepare(static_cast<int>((5.3f + 4.1f * k) * (c ? 1.11f : 1.0f) * 0.001f * srf()) + 2);
        }
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
    }
    void reset() override {
        for (auto& l : line_) l.reset();
        for (auto& r : ring_) r.reset();
        for (auto& c : diff_) for (auto& a : c) a.reset();
        for (auto& z : damp_) z = 0.0f;
        for (auto& v : ph_) v = 0.0;
        started_ = false;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        const float t60 = std::clamp(fp(rbTail), 0.2f, 20.0f);
        for (int j = 0; j < N; ++j) g_[j] = std::pow(10.0f, -3.0f * (static_cast<float>(len_[j]) / srf()) / t60);
        dampA_ = 1.0f - std::exp(-dsp::kTwoPi * fp(rbTone) / srf());
        mix_.setMix(pct(rbMix), snap);
    }
    double tailSeconds() const override { return 2.0 * kMaxLen + static_cast<double>(p_[rbTail]); }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const double L = std::clamp(syncedSeconds(bp(rbSync), fp(rbLength), ip(rbDivision), ctx.bpm), 0.08, kMaxLen) * sr_;
        const float dg = 0.3f + 0.45f * pct(rbDensity);
        const float width = pct(rbWidth);
        if (!started_) { started_ = true; for (int v = 0; v < 2; ++v) { ph_[v] = v * 0.5 * L; vl_[v] = L; } }
        for (int i = 0; i < n; ++i) {
            // 1) the input excites a bounded Householder FDN (absorption g < 1, one-pole damping |H| <= 1)
            const float x = 0.5f * (ch[0][i] + ch[1][i]);
            float s[N], sum = 0.0f;
            for (int j = 0; j < N; ++j) {
                float v = line_[j].tap(len_[j]);
                damp_[j] += dampA_ * (v - damp_[j]);
                s[j] = g_[j] * damp_[j];
                sum += s[j];
            }
            const float h = 2.0f / N * sum;
            for (int j = 0; j < N; ++j) line_[j].push(s[j] - h + x * ((j & 1) ? -0.4f : 0.4f));
            float wet[2] = {(s[0] - s[2] + s[4] - s[6]) * 0.5f, (s[1] - s[3] + s[5] - s[7]) * 0.5f};
            for (int c = 0; c < 2; ++c) {
                for (auto& a : diff_[c]) wet[c] = a.process(wet[c], dg);
                ring_[c].push(wet[c]);
            }
            // 2) two half-overlapped voices play the recorded tail backwards (Hann windows: sum = 1). Each voice
            //    latches the chunk length at its window zero, so a tempo / length change never jumps a read point.
            float out[2] = {0.0f, 0.0f};
            for (int v = 0; v < 2; ++v) {
                const double pc = ph_[v];
                const float w = 0.5f - 0.5f * std::cos(dsp::kTwoPi * static_cast<float>(pc / vl_[v]));
                const float d = static_cast<float>(std::min(2.0 * pc + 1.0, static_cast<double>(ring_[0].capacity() - 4)));
                for (int c = 0; c < 2; ++c) out[c] += w * ring_[c].readCubic(std::max(1.0f, d));
                ph_[v] += 1.0;
                if (ph_[v] >= vl_[v]) { ph_[v] -= vl_[v]; vl_[v] = L; }
            }
            const float mid = 0.5f * (out[0] + out[1]), side = 0.5f * (out[0] - out[1]) * width;
            ch[0][i] = 1.3f * (mid + side);
            ch[1][i] = 1.3f * (mid - side);
        }
        for (auto& z : damp_) z = dsp::flushTiny(z);
        mix_.apply(ch, n);
    }
    dsp::DelayLine line_[N], ring_[2];
    dsp::Allpass diff_[2][3];
    int len_[N] = {};
    float g_[N] = {}, damp_[N] = {}, dampA_ = 0.5f;
    double ph_[2] = {0, 0}, vl_[2] = {1, 1};
    bool started_ = false;
    MixStage mix_;
};

// ============================================================================================ Swell
enum { swRise, swCurve, swSensitivity, swRetrigger, swDepth, swWash, swTone, swMix };

class Swell final : public FxBase {
    static constexpr float kLookMs = 3.0f;
    void onPrepare() override {
        // Full Quality: a 3 ms look-ahead (reported latency) closes the gain before the attack is heard. Low / Zero:
        // no look-ahead, 0 samples, and the first ~1 ms of each attack passes before the swell closes.
        look_ = latMode_ == LatencyMode::Quality ? std::max(1, static_cast<int>(kLookMs * 0.001f * srf())) : 0;
        for (auto& l : delay_) l.prepare(look_ + 8);
        static const float ms[4] = {23.0f, 37.0f, 53.0f, 71.0f};
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k) wash_[c][k].prepare(static_cast<int>(ms[k] * (c ? 1.09f : 1.0f) * 0.001f * srf()) + 2);
        fast_.prepare(sr_, 0.3f, 25.0f);
        slow_.prepare(sr_, 25.0f, 350.0f);
        mix_.prepare(sr_, maxBlock_, look_, MixLaw::Linear);
    }
    int latencySamples() const override { return look_; }
    void reset() override {
        for (auto& l : delay_) l.reset();
        for (auto& c : wash_) for (auto& a : c) a.reset();
        for (auto& t : tone_) t.reset();
        fast_.reset();
        slow_.reset();
        gain_ = 1.0f;
        rise_ = 1.0f;
        drop_ = 0.0f;
        refr_ = 0;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        for (auto& t : tone_) t.setup(dsp::Svf::Lowpass, fp(swTone), 0.6f, 0, sr_);
        mix_.setMix(pct(swMix), snap);
    }
    double tailSeconds() const override { return 0.5; }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float riseSamples = std::max(1.0f, fp(swRise) * 0.001f * srf());
        const float curve = pct(swCurve);
        const float ratio = 1.4f + 4.0f * (1.0f - pct(swSensitivity));
        const int retrig = static_cast<int>(fp(swRetrigger) * 0.001f * srf());
        const float floor = 1.0f - pct(swDepth);
        const float wash = pct(swWash);
        const float dropStep = 1.0f / std::max(1.0f, 1.5f * 0.001f * srf());  // 1.5 ms raised-cosine close
        for (int i = 0; i < n; ++i) {
            // detection on the undelayed input: the look-ahead lets the gain close before the attack is heard
            const float a = std::max(std::fabs(ch[0][i]), std::fabs(ch[1][i]));
            const float f = fast_.process(a), s = slow_.process(a);
            if (refr_ > 0) --refr_;
            if (refr_ == 0 && f > 0.003f && f > ratio * s) {
                refr_ = std::max(retrig, static_cast<int>(0.02f * srf()));
                drop_ = 1.0f;   // close (from wherever the gain is now)
                rise_ = 0.0f;
                dropFrom_ = gain_;
            }
            if (drop_ > 0.0f) {
                drop_ = std::max(0.0f, drop_ - dropStep);
                gain_ = floor + (dropFrom_ - floor) * rcos(drop_);
            } else if (rise_ < 1.0f) {
                rise_ = std::min(1.0f, rise_ + 1.0f / riseSamples);
                // curve 0 = linear fade, 100 % = slow-then-fast (exponential-like) bloom; zero slope at the top
                const float lin = rise_, expo = rise_ * rise_ * rise_;
                const float shaped = (1.0f - curve) * lin + curve * expo;
                gain_ = floor + (1.0f - floor) * (0.5f * shaped + 0.5f * rcos(shaped));
            }
            for (int c = 0; c < 2; ++c) {
                delay_[c].push(ch[c][i]);
                float y = delay_[c].tap(look_) * gain_;
                if (wash > 1e-4f) {
                    float w = tone_[c].process(y);
                    for (auto& ap : wash_[c]) w = ap.process(w, 0.62f);
                    y = y + wash * 0.9f * w;
                }
                ch[c][i] = y;
            }
        }
        for (auto& t : tone_) t.flush();
        mix_.apply(ch, n);
    }
    dsp::DelayLine delay_[2];
    dsp::Allpass wash_[2][4];
    dsp::Svf tone_[2];
    dsp::EnvelopeFollower fast_, slow_;
    int look_ = 144, refr_ = 0;
    float gain_ = 1.0f, rise_ = 1.0f, drop_ = 0.0f, dropFrom_ = 1.0f;
    MixStage mix_;
};

}  // namespace

FxDescriptor describeCloudChamber() {
    FxDescriptor d;
    d.typeId = "cloud_chamber";
    d.name = "Cloud Chamber";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "A dense, evolving atmosphere in which lows, mids and highs each decay at their own speed. From sparse reflections to a slow-blooming fog, with a stable freeze.";
    d.params = {
        P::pct("size", "Size", 55.0f, "How large the space is (the spacing of its reflections). Not a level control."),
        P::logf("decay", "Decay", "s", 0.3f, 40.0f, 6.0f, "How long the midrange takes to fade by 60 dB."),
        P::flt("low_decay", "Low Decay", "x", 0.25f, 2.0f, 1.2f, "How long the lows last compared with the mids (above 1 = a deep, long low end; below 1 = a tight bottom)."),
        P::flt("high_decay", "High Decay", "x", 0.1f, 1.5f, 0.45f, "How long the highs last compared with the mids (low = dark, natural air absorption; above 1 = a glassy, bright tail)."),
        P::pct("density", "Density", 70.0f, "Sparse, discrete early reflections (low) to an immediately dense wash (high)."),
        P::pct("bloom", "Bloom", 30.0f, "Lets the field swell in slowly after each sound instead of starting at once."),
        P::pct("drift", "Drift", 35.0f, "Slow movement inside the space: a living, gently shifting tail."),
        P::flt("predelay", "Pre-delay", "ms", 0.0f, 300.0f, 15.0f, "A gap between the sound and its space, so attacks stay clear."),
        P::adv(P::logf("low_cut", "Low Cut", "Hz", 20.0f, 800.0f, 60.0f, "Keeps the lowest frequencies out of the space (cleaner low end).")),
        P::flt("width", "Width", "%", 0.0f, 150.0f, 100.0f, "Stereo width of the space. 0 % = mono."),
        P::toggle("freeze", "Freeze", false, "Holds the space as it is, forever, without new input. Turn off to let it decay again (smooth both ways)."),
        P::pct("mix", "Mix", 35.0f, "Blend of the dry sound and the space."),
    };
    d.create = [] { return std::make_unique<CloudChamber>(); };
    return d;
}

FxDescriptor describeReflectionSculptor() {
    FxDescriptor d;
    d.typeId = "reflections";
    d.name = "Reflection Sculptor";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "Designs the first reflections of a space as a pattern: rooms, walls, corridors, slap clusters, rhythmic or metallic reflections. Depth without a long tail.";
    d.params = {
        P::choice("pattern", "Pattern", {"Room", "Studio Wall", "Corridor", "Slap Cluster", "Scatter", "Ramp Up", "Rhythmic", "Metal Box"}, 0,
                  "How the reflections are arranged in time and space. Corridor: evenly spaced. Slap Cluster: one dense group late in the span. Ramp Up: reflections grow louder (a reverse-like approach). Rhythmic: an eighth-note grid. Metal Box: tight, close, metallic."),
        P::logf("time", "Time", "ms", 5.0f, 400.0f, 60.0f, "The span of the reflections: small (close walls) to very large (distant surfaces, slap echoes)."),
        P::flt("taps", "Reflections", "", 2.0f, 24.0f, 12.0f, "How many reflections. Few = discrete and audible; many = a dense early field. The overall level stays the same."),
        P::pct("spread", "Spread", 70.0f, "How far left and right the reflections are placed."),
        P::pct("decay", "Fall-off", 60.0f, "How much quieter the distant reflections are."),
        P::logf("tone", "Distance Tone", "Hz", 1000.0f, 18000.0f, 7000.0f, "How dark the farthest reflections become (near ones stay bright)."),
        P::pct("motion", "Motion", 20.0f, "Slow, independent drift of every reflection: a moving, breathing space."),
        P::pct("diffusion", "Diffusion", 25.0f, "Softens each reflection into a short cloud instead of a sharp copy."),
        P::adv(P::flt("predelay", "Pre-delay", "ms", 0.0f, 100.0f, 0.0f, "A gap before the first reflection.")),
        P::adv(P::flt("seed", "Variation", "", 0.0f, 99.0f, 3.0f, "A different arrangement of the same pattern. The same value always gives the same layout.")),
        P::pct("mix", "Mix", 40.0f, "Blend of the dry sound and the reflections."),
    };
    d.create = [] { return std::make_unique<ReflectionSculptor>(); };
    return d;
}

FxDescriptor describeReverseBloom() {
    FxDescriptor d;
    d.typeId = "reverse_bloom";
    d.name = "Reverse Bloom";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "A live reverse-reverb swell: every phrase's reverb tail is played backwards one chunk later, so sounds are preceded by their own rising, inhaled echo.";
    d.params = {
        P::toggle("sync", "Sync", true, "Locks the chunk length to the host tempo."),
        P::logf("length_ms", "Length", "ms", 100.0f, 4000.0f, 800.0f, "How long each reversed chunk is (the swell arrives one chunk after the sound)."),
        P::choice("division", "Division", divisionLabels(), 10, "Chunk length in note values when Sync is on."),
        P::logf("tail", "Tail", "s", 0.3f, 12.0f, 2.5f, "How long the reverb tail that gets reversed is: short = a quick inhale, long = a slow, wide swell."),
        P::logf("tone", "Tone", "Hz", 800.0f, 16000.0f, 6000.0f, "Darkens the reversed tail."),
        P::pct("density", "Density", 60.0f, "Grainy, discrete swell (low) to a smooth, dense one (high)."),
        P::flt("width", "Width", "%", 0.0f, 150.0f, 90.0f, "Stereo width of the swell."),
        P::pct("mix", "Mix", 50.0f, "Blend of the dry sound and the reversed swell."),
    };
    d.create = [] { return std::make_unique<ReverseBloom>(); };
    return d;
}

FxDescriptor describeSwell() {
    FxDescriptor d;
    d.typeId = "swell";
    d.name = "Swell";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "Volume-pedal blooms: every attack is removed and the sound fades in along a curve, optionally into a soft diffuse wash. Turns plucks, hits and words into pads.";
    d.params = {
        P::logf("rise", "Rise", "ms", 20.0f, 4000.0f, 450.0f, "How long each sound takes to fade in after its attack."),
        P::pct("curve", "Curve", 50.0f, "0 % = an even fade; higher = a slow start that blooms at the end."),
        P::pct("sensitivity", "Sensitivity", 60.0f, "How easily a new attack restarts the swell."),
        P::flt("retrigger", "Retrigger", "ms", 20.0f, 2000.0f, 150.0f, "The shortest time between two swells (rolls and flams count as one)."),
        P::pct("depth", "Depth", 100.0f, "How completely the attack is removed (100 % = from silence)."),
        P::pct("wash", "Wash", 25.0f, "Adds a soft, diffuse halo to the swelling sound."),
        P::adv(P::logf("tone", "Wash Tone", "Hz", 800.0f, 16000.0f, 6000.0f, "Brightness of the wash.")),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and the swelled sound."),
    };
    d.create = [] { return std::make_unique<Swell>(); };
    return d;
}

}  // namespace ih1::fx
