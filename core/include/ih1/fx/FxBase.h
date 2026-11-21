// Common base for IH1 FX modules: parameter storage + latency-aligned smoothed wet/dry stage.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/dsp/Denormals.h"
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
        glideLeft_ = 0;
        onPrepare();
        if (hasParams_) applyParams(true);
        reset();
    }
    void setLatencyMode(LatencyMode m) final { latMode_ = m; }
    // V4 Phase 4 (parameter glide): a non-snapped change of a continuous parameter no longer lands in one step.
    // Discrete parameters (choices, toggles) switch at once (the engine crossfades the staged effect around them);
    // continuous ones glide linearly over kGlideMs while process() runs in kGlideSub-sample sub-blocks, so a jump of
    // e.g. a modulation depth or a read offset becomes ~45 small steps that each effect's own smoothers absorb.
    void setParams(const ParamValues& v, bool snap) final {
        hasParams_ = true;
        if (snap || !glideReady_) {
            p_ = v;
            target_ = v;
            glideLeft_ = 0;
            glideReady_ = true;
            applyParams(snap);
            return;
        }
        target_ = v;
        bool any = false;
        for (int i = 0; i < kMaxParams; ++i) {
            const size_t k = static_cast<size_t>(i);
            if (!isContinuous(i)) { p_[k] = v[k]; continue; }
            if (p_[k] != v[k]) any = true;
        }
        if (any) {
            glideLeft_ = std::max(1, static_cast<int>(kGlideMs * 0.001 * sr_));
            for (int i = 0; i < kMaxParams; ++i) glideStep_[static_cast<size_t>(i)] = (target_[static_cast<size_t>(i)] - p_[static_cast<size_t>(i)]) / static_cast<float>(glideLeft_);
        }
        applyParams(false);
    }
    void process(float* const* ch, int n, const ProcessContext& ctx) final {
        if (glideLeft_ <= 0 || ctx.numMidi > 0) {
            if (glideLeft_ > 0) finishGlide();  // MIDI-driven effects: no sub-blocks (event offsets are per block)
            processBlock(ch, n, ctx);
            return;
        }
        int off = 0;
        while (off < n) {
            const int m = glideLeft_ > 0 ? std::min(kGlideSub, n - off) : n - off;
            if (glideLeft_ > 0) {
                const int adv = std::min(m, glideLeft_);
                glideLeft_ -= adv;
                if (glideLeft_ <= 0) {
                    finishGlide();
                } else {
                    for (int i = 0; i < kMaxParams; ++i)
                        if (isContinuous(i)) p_[static_cast<size_t>(i)] += glideStep_[static_cast<size_t>(i)] * static_cast<float>(adv);
                    applyParams(false);
                }
            }
            float* sub[2] = {ch[0] + off, ch[1] + off};
            ProcessContext c2 = ctx;
            c2.blockOffset = ctx.blockOffset + off;
            if (ctx.ppqValid) c2.ppqPosition = ctx.ppqPosition + off / sr_ * ctx.bpm / 60.0;
            processBlock(sub, m, c2);
            off += m;
        }
    }

protected:
    virtual void onPrepare() = 0;
    virtual void applyParams(bool snap) = 0;
    // The effect's audio callback (FxProcessor::process wraps it with the parameter glide).
    virtual void processBlock(float* const* ch, int n, const ProcessContext& ctx) = 0;
    // Continuous parameter? Effects declare their discrete parameter indices through discreteMask_ (set by the
    // registry from the descriptor when the processor is created); unknown = continuous.
    bool isContinuous(int i) const { return i < 64 && !((discreteMask_ >> i) & 1ull); }
    void finishGlide() {
        glideLeft_ = 0;
        p_ = target_;
        applyParams(false);
    }
public:
    void setDiscreteMask(uint64_t m) { discreteMask_ = m; }
protected:
    static constexpr float kGlideMs = 30.0f;
    static constexpr int kGlideSub = 32;
    ParamValues target_{}, glideStep_{};
    int glideLeft_ = 0;
    bool glideReady_ = false;
    uint64_t discreteMask_ = 0;

    float srf() const { return static_cast<float>(sr_); }
    int ip(int i) const { return static_cast<int>(std::lround(p_[static_cast<size_t>(i)])); }
    float fp(int i) const { return p_[static_cast<size_t>(i)]; }
    bool bp(int i) const { return p_[static_cast<size_t>(i)] >= 0.5f; }
    float pct(int i) const { return p_[static_cast<size_t>(i)] * 0.01f; }

    double sr_ = 48000.0;
    int maxBlock_ = 512;
    ParamValues p_{};
    bool hasParams_ = false;
    LatencyMode latMode_ = LatencyMode::Quality;  // already resolved for this type by the Session
};

// Tempo-synced or free time helper: returns seconds.
inline double syncedSeconds(bool sync, float ms, int division, double bpm) {
    return sync ? dsp::divisionSeconds(division, bpm) : ms * 0.001;
}
std::vector<std::string> divisionLabels();

}  // namespace ih1::fx
