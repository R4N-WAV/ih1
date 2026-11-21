// IH1 RateReducer (V3 Phase 5): one reusable lo-fi resampling stage, used as its own FX (Degrade) and as an
// optional character stage inside other FX (Delay repeats, Grain Engine feedback, Glitch Scatter, Fold & Crush).
//
// The terms are kept apart on purpose (research Part II 7.2):
//   * sample-rate REDUCTION / sample-and-hold: the held value updates `rate` times a second. With the anti-alias
//     filter Off, content above rate/2 folds back as inharmonic aliases (the gritty sampler sound) -- deliberate.
//   * anti-alias pre-filter (Soft = 2-pole, Strong = 4-pole low-pass at 0.45 * rate): the correct decimation path
//     when aliasing should be rejected; the result is darker and cleaner.
//   * reconstruction / smoothing post-filter (Soft / Strong): softens the hold steps (images above rate/2), like a
//     sampler's output filter. Off = raw staircase.
//   * bit-depth REDUCTION is a separate amplitude quantiser (bits, optional TPDF dither), not part of the rate.
//   * clock jitter: the hold period wanders randomly (broken / unstable converter clock).
// This is never oversampling; nothing here is used to reduce aliasing of another stage.
//
// Real-time safe: no allocation; all state is a handful of floats. `Off` (rate >= host rate, bits >= 24, filters
// Off) is an exact bypass.
#pragma once

#include <algorithm>
#include <cmath>

#include "ih1/dsp/Basics.h"
#include "ih1/dsp/Denormals.h"
#include "ih1/dsp/Svf.h"

namespace ih1::dsp {

class RateReducer {
public:
    enum Filter { FilterOff = 0, FilterSoft = 1, FilterStrong = 2 };
    struct Settings {
        float rateHz = 48000.0f;  // effective sample rate of the hold stage
        float bits = 24.0f;       // amplitude resolution (>= 24 = off)
        float jitter = 0.0f;      // 0..1 clock instability
        int antiAlias = FilterOff;
        int reconstruct = FilterOff;
        bool dither = false;
        bool linked = true;       // both channels share one clock (false: independent, slightly offset clocks)
    };

    void prepare(double sr, uint32_t seed = 1) {
        sr_ = sr > 0 ? sr : 48000.0;
        seed_ = seed ? seed : 1u;
        reset();
    }
    void reset() {
        rng_.seed_(seed_);
        for (int c = 0; c < 2; ++c) {
            phase_[c] = 0.0f;
            held_[c] = 0.0f;
            for (auto& f : pre_[c]) f.reset();
            for (auto& f : post_[c]) f.reset();
        }
        period_[0] = period_[1] = 1.0f;
        lastRate_ = -1.0f;
        lastAa_ = lastRc_ = -1;
    }
    bool bypassed(const Settings& s) const {
        return s.rateHz >= static_cast<float>(sr_) * 0.999f && s.bits >= 23.99f && s.antiAlias == FilterOff && s.reconstruct == FilterOff;
    }
    // In place, stereo. Settings are expected to be smoothed by the caller where they move continuously.
    void process(float* L, float* R, int n, const Settings& s) {
        if (bypassed(s)) return;
        const float sr = static_cast<float>(sr_);
        const float rate = std::clamp(s.rateHz, 50.0f, sr);
        if (rate != lastRate_ || s.antiAlias != lastAa_ || s.reconstruct != lastRc_) {
            const float fc = std::min(0.45f * rate, 0.45f * sr);
            for (int c = 0; c < 2; ++c) {
                for (auto& f : pre_[c]) f.setup(Svf::Lowpass, fc, 0.6f, 0, sr_);
                for (auto& f : post_[c]) f.setup(Svf::Lowpass, fc, 0.6f, 0, sr_);
            }
            lastRate_ = rate;
            lastAa_ = s.antiAlias;
            lastRc_ = s.reconstruct;
        }
        const float step = rate / sr;  // phase advance per host sample (hold updates when phase wraps)
        const bool quant = s.bits < 23.99f;
        const float levels = quant ? std::exp2(std::clamp(s.bits, 1.0f, 24.0f) - 1.0f) : 1.0f;
        const float jit = std::clamp(s.jitter, 0.0f, 1.0f) * 0.9f;
        float* ch[2] = {L, R};
        const int nAa = s.antiAlias == FilterStrong ? 2 : (s.antiAlias == FilterSoft ? 1 : 0);
        const int nRc = s.reconstruct == FilterStrong ? 2 : (s.reconstruct == FilterSoft ? 1 : 0);
        for (int i = 0; i < n; ++i) {
            for (int c = 0; c < 2; ++c) {
                float x = ch[c][i];
                for (int k = 0; k < nAa; ++k) x = pre_[c][k].process(x);
                const int clk = s.linked ? 0 : c;
                if (c == 0 || !s.linked) {
                    phase_[clk] += step;
                    tick_[clk] = phase_[clk] >= period_[clk];
                    if (tick_[clk]) {
                        phase_[clk] -= period_[clk];
                        if (phase_[clk] > 1.0f) phase_[clk] = 0.0f;
                        period_[clk] = 1.0f + jit * rng_.bipolar() + (s.linked ? 0.0f : 0.013f * static_cast<float>(c));
                    }
                }
                if (tick_[clk]) {
                    float h = x;
                    if (quant) {
                        if (s.dither) h += (rng_.uniform() - rng_.uniform()) / levels;
                        h = std::round(std::clamp(h, -1.5f, 1.5f) * levels) / levels;
                    }
                    held_[c] = h;
                }
                float y = held_[c];
                for (int k = 0; k < nRc; ++k) y = post_[c][k].process(y);
                ch[c][i] = y;
            }
        }
        for (int c = 0; c < 2; ++c) {
            for (auto& f : pre_[c]) f.flush();
            for (auto& f : post_[c]) f.flush();
        }
    }

private:
    double sr_ = 48000.0;
    uint32_t seed_ = 1;
    Rng rng_;
    float phase_[2] = {0, 0}, period_[2] = {1, 1}, held_[2] = {0, 0};
    bool tick_[2] = {false, false};
    Svf pre_[2][2], post_[2][2];
    float lastRate_ = -1.0f;
    int lastAa_ = -1, lastRc_ = -1;
};

}  // namespace ih1::dsp
