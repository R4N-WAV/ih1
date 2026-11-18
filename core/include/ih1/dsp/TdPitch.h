// Time-domain dual-tap pitch shifter (delay-line "rotating head"): two read taps sweep a window with
// equal-power sine/cosine crossfades. Pitch ratio r = 1 - d'(t). Cheap; best for small shifts
// (doubling, MicroPitch) and for shimmer feedback paths.
#pragma once

#include "ih1/dsp/DelayLine.h"

namespace ih1::dsp {

class TdPitchShifter {
public:
    void prepare(double sr, float windowMs, float maxExtraDelayMs) {
        sr_ = sr;
        window_ = windowMs * 0.001f * static_cast<float>(sr);
        line_.prepare(static_cast<int>(window_ + maxExtraDelayMs * 0.001f * static_cast<float>(sr)) + 16);
    }
    void reset() { line_.reset(); phase_ = 0.0f; }
    // ratio: pitch ratio; extraDelay: base delay in samples added to both taps
    float process(float x, float ratio, float extraDelay) {
        line_.push(x);
        phase_ += (1.0f - ratio) / window_;
        phase_ -= std::floor(phase_);
        float p2 = phase_ + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;
        const float d1 = 2.0f + extraDelay + phase_ * window_;
        const float d2 = 2.0f + extraDelay + p2 * window_;
        const float g1 = std::sin(kPi * phase_), g2 = std::sin(kPi * p2);
        return g1 * line_.readCubic(d1) + g2 * line_.readCubic(d2);
    }

private:
    double sr_ = 48000.0;
    float window_ = 1000.0f, phase_ = 0.0f;
    DelayLine line_;
};

}  // namespace ih1::dsp
