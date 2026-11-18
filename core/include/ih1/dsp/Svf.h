// Trapezoidal-integrated state-variable filter (Simper/Cytomic topology; KADAL KNW-R8PP5DNBY0,
// IMP-W4CTPKMXBZ). Modulation-safe: coefficients may change every sample. Bell/shelf forms follow
// Simper's published linear-trapezoidal SVF mixing.
#pragma once

#include <algorithm>
#include <cmath>

namespace ih1::dsp {

class Svf {
public:
    enum Mode { Lowpass = 0, Bandpass, Highpass, Notch, Peak, Allpass, Bell, LowShelf, HighShelf };

    void setup(Mode mode, float fc, float q, float gainDb, double sr) {
        mode_ = mode;
        fc = std::clamp(fc, 5.0f, 0.49f * static_cast<float>(sr));
        q = std::max(q, 0.025f);
        const float A = std::pow(10.0f, gainDb / 40.0f);
        float g = std::tan(3.14159265358979f * fc / static_cast<float>(sr));
        float k = 1.0f / q;
        m0_ = m1_ = m2_ = 0.0f;
        switch (mode) {
            case Lowpass: m2_ = 1.0f; break;
            case Bandpass: m1_ = 1.0f; break;
            case Highpass: m0_ = 1.0f; m1_ = -k; m2_ = -1.0f; break;
            case Notch: m0_ = 1.0f; m1_ = -k; break;
            case Peak: m0_ = 1.0f; m1_ = -k; m2_ = -2.0f; break;
            case Allpass: m0_ = 1.0f; m1_ = -2.0f * k; break;
            case Bell: k = 1.0f / (q * A); m0_ = 1.0f; m1_ = k * (A * A - 1.0f); break;
            case LowShelf: g /= std::sqrt(A); m0_ = 1.0f; m1_ = k * (A - 1.0f); m2_ = A * A - 1.0f; break;
            case HighShelf: g *= std::sqrt(A); m0_ = A * A; m1_ = k * (1.0f - A) * A; m2_ = 1.0f - A * A; break;
        }
        a1_ = 1.0f / (1.0f + g * (g + k));
        a2_ = g * a1_;
        a3_ = g * a2_;
    }

    float process(float v0) {
        const float v3 = v0 - ic2_;
        const float v1 = a1_ * ic1_ + a2_ * v3;
        const float v2 = ic2_ + a2_ * ic1_ + a3_ * v3;
        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;
        return m0_ * v0 + m1_ * v1 + m2_ * v2;
    }
    // multi-output for filters that need LP/BP/HP from one state update
    void processMulti(float v0, float& lp, float& bp, float& hp, float k) {
        const float v3 = v0 - ic2_;
        const float v1 = a1_ * ic1_ + a2_ * v3;
        const float v2 = ic2_ + a2_ * ic1_ + a3_ * v3;
        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;
        lp = v2;
        bp = v1;
        hp = v0 - k * v1 - v2;
    }
    void reset() { ic1_ = ic2_ = 0.0f; }
    void flush() {
        if (std::fabs(ic1_) < 1e-20f) ic1_ = 0.0f;
        if (std::fabs(ic2_) < 1e-20f) ic2_ = 0.0f;
    }
    // magnitude response at frequency f (for tests / UI curves)
    float magnitudeAt(float f, double sr) const;

private:
    Mode mode_ = Lowpass;
    float a1_ = 0, a2_ = 0, a3_ = 0, m0_ = 0, m1_ = 0, m2_ = 1;
    float ic1_ = 0, ic2_ = 0;
};

// Linkwitz-Riley 4th-order crossover from two cascaded Butterworth SVF sections per band.
class Crossover {
public:
    void setup(float fc, double sr) {
        for (int i = 0; i < 2; ++i) {
            lp_[i].setup(Svf::Lowpass, fc, 0.70710678f, 0.0f, sr);
            hp_[i].setup(Svf::Highpass, fc, 0.70710678f, 0.0f, sr);
        }
    }
    void process(float x, float& low, float& high) {
        low = lp_[1].process(lp_[0].process(x));
        high = hp_[1].process(hp_[0].process(x));
    }
    void reset() { for (int i = 0; i < 2; ++i) { lp_[i].reset(); hp_[i].reset(); } }

private:
    Svf lp_[2], hp_[2];
};

}  // namespace ih1::dsp
