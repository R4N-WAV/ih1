// First-order antiderivative-antialiased waveshapers (KADAL KNW-F7DN55HDGE, IMP-X9AEKNDHGM pattern)
// and a linear-phase 2x FIR oversampler (KADAL KNW-JC0N8AGZ90 / KNW-MCXD9ZQP83: ADAA-1 + 2x is the
// default strategy; 4x = two cascaded stages).
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace ih1::dsp {

enum class Shape { Tanh = 0, HardClip, SoftCubic, Arctan, Fold, Asym };

// f(x) and its antiderivative F(x) for each shape.
inline float shapeF(Shape s, float x) {
    switch (s) {
        case Shape::HardClip: return std::clamp(x, -1.0f, 1.0f);
        case Shape::SoftCubic: { const float c = std::clamp(x, -1.0f, 1.0f); return 1.5f * (c - c * c * c / 3.0f); }
        case Shape::Arctan: return 0.63661977f * std::atan(1.5707963f * x);
        case Shape::Fold: return std::sin(x);
        case Shape::Asym: return std::tanh(x + 0.35f) - 0.33637554f;  // tanh(0.35)
        default: return std::tanh(x);
    }
}
inline double shapeAD(Shape s, double x) {
    switch (s) {
        case Shape::HardClip: {
            const double a = std::fabs(x);
            return a <= 1.0 ? 0.5 * x * x : a - 0.5;
        }
        case Shape::SoftCubic: {
            const double a = std::fabs(x);
            if (a <= 1.0) return 1.5 * (0.5 * x * x - x * x * x * x / 12.0);
            return 1.5 * (2.0 / 3.0 * (a - 1.0) + 0.5 - 1.0 / 12.0);
        }
        case Shape::Arctan: {
            const double k = 1.5707963267948966, z = k * x;
            return 0.6366197723675814 * (x * std::atan(z) - std::log1p(z * z) / (2.0 * k));
        }
        case Shape::Fold: return -std::cos(x);
        case Shape::Asym: {
            const double z = x + 0.35, a = std::fabs(z);
            return (a + std::log1p(std::exp(-2.0 * a)) - 0.6931471805599453) - 0.33637554 * x;
        }
        default: {
            const double a = std::fabs(x);
            return a + std::log1p(std::exp(-2.0 * a)) - 0.6931471805599453;  // ln cosh x
        }
    }
}

class AdaaShaper {
public:
    void setShape(Shape s) { if (s != shape_) { shape_ = s; F1_ = shapeAD(s, x1_); } }
    void reset() { x1_ = 0.0; F1_ = shapeAD(shape_, 0.0); }
    float process(float xf) {
        const double x = xf;
        const double F0 = shapeAD(shape_, x);
        const double dx = x - x1_;
        float y;
        if (std::fabs(dx) > 1e-5) y = static_cast<float>((F0 - F1_) / dx);
        else y = shapeF(shape_, static_cast<float>(0.5 * (x + x1_)));
        x1_ = x;
        F1_ = F0;
        return y;
    }

private:
    Shape shape_ = Shape::Tanh;
    double x1_ = 0.0, F1_ = 0.0;
};

// Linear-phase half-band FIR 2x oversampler (Kaiser-windowed sinc). Latency (base rate) = (taps-1)/2.
class Oversampler2x {
public:
    // passthrough = true: the Zero-latency variant. No filters run, factor() == 1 and latency() == 0;
    // up() copies, down() copies. Callers process factor() * n samples at factor() * sampleRate.
    void prepare(int maxBlock, int taps = 47, float beta = 7.8f, bool passthrough = false);
    int latency() const { return passthrough_ ? 0 : (static_cast<int>(h_.size()) - 1) / 2; }
    int factor() const { return passthrough_ ? 1 : 2; }
    // up: n input samples -> factor()*n samples in `hi` (internal buffer returned)
    float* up(const float* in, int n);
    // down: factor()*n samples in `hi` -> n output samples
    void down(const float* hi, float* out, int n);
    void reset();

private:
    std::vector<float> h_, upHist_, downHist_, hi_;
    int upPos_ = 0, downPos_ = 0;
    bool passthrough_ = false;
};

}  // namespace ih1::dsp
