// Fractional delay line (power-of-two ring). Interpolators per KADAL KNW-M8Q0G3M7NM:
// cubic (Hermite) / 3rd-order Lagrange for modulated reads, linear for cheap LFO-rate reads.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace ih1::dsp {

class DelayLine {
public:
    // maxDelaySamples: largest delay that will be read (plus interpolator margin is added here).
    void prepare(int maxDelaySamples) {
        int size = 1;
        while (size < maxDelaySamples + 8) size <<= 1;
        buf_.assign(static_cast<size_t>(size), 0.0f);
        mask_ = size - 1;
        w_ = 0;
    }
    void reset() { std::fill(buf_.begin(), buf_.end(), 0.0f); }
    int capacity() const { return mask_ - 4; }

    void push(float x) {
        w_ = (w_ + 1) & mask_;
        buf_[static_cast<size_t>(w_)] = x;
    }
    // delay 0 = most recently pushed sample
    float tap(int d) const { return buf_[static_cast<size_t>((w_ - d) & mask_)]; }

    float readLinear(float d) const {
        d = clampDelay(d);
        const int i = static_cast<int>(d);
        const float f = d - static_cast<float>(i);
        const float a = tap(i), b = tap(i + 1);
        return a + f * (b - a);
    }
    // 4-point Hermite (Catmull-Rom). Needs d >= 1.
    float readCubic(float d) const {
        d = clampDelay(std::max(d, 1.0f));
        const int i = static_cast<int>(d);
        const float f = d - static_cast<float>(i);
        const float xm1 = tap(i - 1), x0 = tap(i), x1 = tap(i + 1), x2 = tap(i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    // 3rd-order Lagrange (flatter magnitude than Hermite at mid fractions).
    float readLagrange(float d) const {
        d = clampDelay(std::max(d, 1.0f));
        const int i = static_cast<int>(d) - 1;
        const float f = d - static_cast<float>(i);  // in [1,2)
        const float x0 = tap(i), x1 = tap(i + 1), x2 = tap(i + 2), x3 = tap(i + 3);
        const float d1 = f - 1.0f, d2 = f - 2.0f, d3 = f - 3.0f;
        return -d1 * d2 * d3 / 6.0f * x0 + f * d2 * d3 * 0.5f * x1 - f * d1 * d3 * 0.5f * x2 + f * d1 * d2 / 6.0f * x3;
    }

private:
    float clampDelay(float d) const {
        if (!(d >= 0.0f)) d = 0.0f;
        const float mx = static_cast<float>(mask_ - 4);
        return d > mx ? mx : d;
    }
    std::vector<float> buf_{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int mask_ = 7;
    int w_ = 0;
};

// Schroeder allpass with integer delay (diffusers for reverb / granular cloud).
class Allpass {
public:
    void prepare(int delaySamples) { line_.prepare(delaySamples + 2); d_ = delaySamples; }
    void setDelay(int d) { d_ = std::clamp(d, 1, line_.capacity()); }
    void reset() { line_.reset(); }
    float process(float x, float g) {
        const float z = line_.tap(d_ - 1);
        const float v = x + g * z;
        line_.push(v);
        return z - g * v;
    }
    // modulated read (fractional delay)
    float processMod(float x, float g, float d) {
        const float z = line_.readCubic(d - 1.0f);
        const float v = x + g * z;
        line_.push(v);
        return z - g * v;
    }

private:
    DelayLine line_;
    int d_ = 1;
};

}  // namespace ih1::dsp
