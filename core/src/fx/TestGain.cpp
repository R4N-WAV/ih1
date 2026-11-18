// Hidden test effect: smoothed gain + one-pole low-pass + optional fixed latency.
// Used only by the test-suite to verify commit / reorder / solo / mute semantics.
#include <algorithm>
#include <cmath>
#include <vector>

#include "ih1/fx/AllFx.h"

namespace ih1::fx {
namespace {

class TestGain final : public FxProcessor {
public:
    void prepare(double sr, int) override {
        sr_ = sr;
        const int lat = static_cast<int>(latency_);
        for (auto& b : delay_) b.assign(static_cast<size_t>(lat + 1), 0.0f);
        pos_ = 0;
        gain_ = target_;
    }
    void reset() override {
        for (auto& b : delay_) std::fill(b.begin(), b.end(), 0.0f);
        lp_[0] = lp_[1] = 0.0f;
    }
    void setParams(const ParamValues& v, bool snap) override {
        target_ = std::pow(10.0f, v[0] / 20.0f);
        cutoff_ = v[1];
        if (snap) gain_ = target_;
    }
    void process(float* const* ch, int n, const ProcessContext&) override {
        const float a = 1.0f - std::exp(-2.0f * 3.14159265f * cutoff_ / static_cast<float>(sr_));
        const float step = 1.0f / (0.01f * static_cast<float>(sr_));
        for (int i = 0; i < n; ++i) {
            gain_ += std::clamp(target_ - gain_, -step, step);
            for (int c = 0; c < 2; ++c) {
                float x = ch[c][i] * gain_;
                if (cutoff_ < 19999.0f) { lp_[c] += a * (x - lp_[c]); x = lp_[c]; }
                auto& d = delay_[c];
                if (d.size() > 1) {
                    d[static_cast<size_t>(pos_)] = x;
                    const int rd = (pos_ + 1) % static_cast<int>(d.size());
                    x = d[static_cast<size_t>(rd)];
                }
                ch[c][i] = x;
            }
            if (delay_[0].size() > 1) pos_ = (pos_ + 1) % static_cast<int>(delay_[0].size());
        }
    }
    int latencySamples() const override { return static_cast<int>(latency_); }

    float latency_ = 0.0f;

private:
    double sr_ = 48000.0;
    float target_ = 1.0f, gain_ = 1.0f, cutoff_ = 20000.0f;
    float lp_[2] = {0, 0};
    std::vector<float> delay_[2];
    int pos_ = 0;
};

}  // namespace

FxDescriptor describeTestGain() {
    FxDescriptor d;
    d.typeId = "test_gain";
    d.name = "Test Gain";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Engine test effect.";
    d.experimental = true;
    d.params = {
        P::flt("gain_db", "Gain", "dB", -60.0f, 12.0f, 0.0f, "Level change."),
        P::logf("cutoff", "Cutoff", "Hz", 20.0f, 20000.0f, 20000.0f, "Low-pass cutoff."),
    };
    d.create = [] { return std::make_unique<TestGain>(); };
    return d;
}

// Variant with 64 samples of latency to test compensation.
FxDescriptor describeTestLatency() {
    FxDescriptor d = describeTestGain();
    d.typeId = "test_latency";
    d.name = "Test Latency";
    d.create = [] {
        auto p = std::make_unique<TestGain>();
        p->latency_ = 64.0f;
        return p;
    };
    return d;
}

}  // namespace ih1::fx
