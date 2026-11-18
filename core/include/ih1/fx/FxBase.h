// Common base for IH1 FX modules: parameter storage + latency-aligned smoothed wet/dry stage.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/fx/FxProcessor.h"

namespace ih1::fx {

enum class MixLaw { Linear, EqualPower, Flat };  // Flat: both paths at unity at 50 %

// Captures the dry input, delays it by the FX latency, then blends with the wet output.
class MixStage {
public:
    void prepare(double sr, int maxBlock, int latency, MixLaw law) {
        law_ = law;
        latency_ = latency;
        int size = 1;
        while (size < latency + maxBlock + 4) size <<= 1;
        for (auto& b : ring_) b.assign(static_cast<size_t>(size), 0.0f);
        mask_ = size - 1;
        w_ = 0;
        mix_.prepare(sr, 25.0f);
        dry_.assign(static_cast<size_t>(maxBlock) * 2, 0.0f);
    }
    void reset() { for (auto& b : ring_) std::fill(b.begin(), b.end(), 0.0f); }
    void setMix(float m01, bool snap) { if (snap) mix_.snap(dsp::clamp01(m01)); else mix_.setTarget(dsp::clamp01(m01)); }
    // Call before processing the wet path.
    void capture(float* const* ch, int n) {
        const int base = w_;
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < n; ++i) ring_[c][static_cast<size_t>((base + i) & mask_)] = ch[c][i];
            for (int i = 0; i < n; ++i) dry_[static_cast<size_t>(c * (static_cast<int>(dry_.size()) / 2) + i)] = ring_[c][static_cast<size_t>((base + i - latency_) & mask_)];
        }
        w_ = (base + n) & mask_;
    }
    // Call after processing; blends the latency-aligned dry with the wet signal in ch.
    void apply(float* const* ch, int n) {
        const size_t half = dry_.size() / 2;
        for (int i = 0; i < n; ++i) {
            const float m = mix_.next();
            float d, w;
            gains(m, d, w);
            ch[0][i] = d * dry_[static_cast<size_t>(i)] + w * ch[0][i];
            ch[1][i] = d * dry_[half + static_cast<size_t>(i)] + w * ch[1][i];
        }
    }
    const float* dry(int c) const { return dry_.data() + (c ? dry_.size() / 2 : 0); }
    void gains(float m, float& d, float& w) const {
        switch (law_) {
            case MixLaw::EqualPower: dsp::equalPower(m, d, w); break;
            case MixLaw::Flat: d = std::min(1.0f, 2.0f * (1.0f - m)); w = std::min(1.0f, 2.0f * m); break;
            default: d = 1.0f - m; w = m;
        }
    }

private:
    MixLaw law_ = MixLaw::Linear;
    int latency_ = 0, mask_ = 0, w_ = 0;
    std::vector<float> ring_[2];
    std::vector<float> dry_;
    dsp::Smoother mix_;
};

class FxBase : public FxProcessor {
public:
    void prepare(double sampleRate, int maxBlock) final {
        sr_ = sampleRate > 0 ? sampleRate : 48000.0;
        maxBlock_ = std::max(1, maxBlock);
        onPrepare();
        if (hasParams_) applyParams(true);
        reset();
    }
    void setParams(const ParamValues& v, bool snap) final {
        p_ = v;
        hasParams_ = true;
        applyParams(snap);
    }

protected:
    virtual void onPrepare() = 0;
    virtual void applyParams(bool snap) = 0;

    float srf() const { return static_cast<float>(sr_); }
    int ip(int i) const { return static_cast<int>(std::lround(p_[static_cast<size_t>(i)])); }
    float fp(int i) const { return p_[static_cast<size_t>(i)]; }
    bool bp(int i) const { return p_[static_cast<size_t>(i)] >= 0.5f; }
    float pct(int i) const { return p_[static_cast<size_t>(i)] * 0.01f; }

    double sr_ = 48000.0;
    int maxBlock_ = 512;
    ParamValues p_{};
    bool hasParams_ = false;
};

// Tempo-synced or free time helper: returns seconds.
inline double syncedSeconds(bool sync, float ms, int division, double bpm) {
    return sync ? dsp::divisionSeconds(division, bpm) : ms * 0.001;
}
std::vector<std::string> divisionLabels();

}  // namespace ih1::fx
