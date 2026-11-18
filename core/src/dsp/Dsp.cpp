#include <algorithm>
#include <cmath>

#include "ih1/dsp/Fft.h"
#include "ih1/dsp/Nonlinear.h"
#include "ih1/dsp/Svf.h"

namespace ih1::dsp {

// ---------------------------------------------------------------------------- SVF response
float Svf::magnitudeAt(float f, double sr) const {
    // Recover g, k from a1..a3: a2 = g*a1, a3 = g*a2  => g = a3/a2 ; a1 = 1/(1+g(g+k)) => k = (1/a1 - 1)/g - g
    const float g = a2_ != 0.0f ? a3_ / a2_ : 0.0f;
    if (g <= 0.0f) return 1.0f;
    const float k = (1.0f / a1_ - 1.0f) / g - g;
    // bilinear: s = j * tan(pi f / sr) / g  (normalised so that the filter corner is at s = j)
    const std::complex<double> s(0.0, std::tan(3.14159265358979 * std::min<double>(f, 0.4999 * sr) / sr) / g);
    const std::complex<double> D = s * s + static_cast<double>(k) * s + 1.0;
    const std::complex<double> H = static_cast<double>(m0_) + static_cast<double>(m1_) * s / D + static_cast<double>(m2_) / D;
    return static_cast<float>(std::abs(H));
}

// ---------------------------------------------------------------------------- Oversampler
static double besselI0(double x) {
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 50; ++k) {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1e-12 * sum) break;
    }
    return sum;
}

void Oversampler2x::prepare(int maxBlock, int taps, float beta) {
    if (taps % 2 == 0) ++taps;
    h_.assign(static_cast<size_t>(taps), 0.0f);
    const int M = taps - 1;
    double sum = 0.0;
    for (int i = 0; i < taps; ++i) {
        const double t = i - M / 2.0;
        const double sinc = (t == 0.0) ? 0.5 : std::sin(3.14159265358979 * 0.5 * t) / (3.14159265358979 * t);
        const double r = 2.0 * i / M - 1.0;
        const double w = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(beta);
        h_[static_cast<size_t>(i)] = static_cast<float>(sinc * w);
        sum += sinc * w;
    }
    for (auto& v : h_) v = static_cast<float>(v / sum);  // unity DC gain
    upHist_.assign(static_cast<size_t>(taps) * 2, 0.0f);
    downHist_.assign(static_cast<size_t>(taps) * 2, 0.0f);
    hi_.assign(static_cast<size_t>(maxBlock) * 2, 0.0f);
    upPos_ = downPos_ = 0;
}

void Oversampler2x::reset() {
    std::fill(upHist_.begin(), upHist_.end(), 0.0f);
    std::fill(downHist_.begin(), downHist_.end(), 0.0f);
    upPos_ = downPos_ = 0;
}

float* Oversampler2x::up(const float* in, int n) {
    const int T = static_cast<int>(h_.size());
    for (int i = 0; i < n; ++i) {
        for (int ph = 0; ph < 2; ++ph) {
            const float x = ph == 0 ? 2.0f * in[i] : 0.0f;  // zero-stuff, gain 2 to keep level
            upHist_[static_cast<size_t>(upPos_)] = x;
            upHist_[static_cast<size_t>(upPos_ + T)] = x;
            float acc = 0.0f;
            const float* hp = &upHist_[static_cast<size_t>(upPos_ + 1)];
            for (int k = 0; k < T; ++k) acc += h_[static_cast<size_t>(T - 1 - k)] * hp[k];
            upPos_ = (upPos_ + 1) % T;
            hi_[static_cast<size_t>(2 * i + ph)] = acc;
        }
    }
    return hi_.data();
}

void Oversampler2x::down(const float* hi, float* out, int n) {
    const int T = static_cast<int>(h_.size());
    for (int i = 0; i < n; ++i) {
        float acc = 0.0f;
        for (int ph = 0; ph < 2; ++ph) {
            const float x = hi[2 * i + ph];
            downHist_[static_cast<size_t>(downPos_)] = x;
            downHist_[static_cast<size_t>(downPos_ + T)] = x;
            if (ph == 0) {
                const float* hp = &downHist_[static_cast<size_t>(downPos_ + 1)];
                for (int k = 0; k < T; ++k) acc += h_[static_cast<size_t>(T - 1 - k)] * hp[k];
            }
            downPos_ = (downPos_ + 1) % T;
        }
        out[i] = acc;
    }
}

// ---------------------------------------------------------------------------- FFT
void Fft::prepare(int size) {
    n_ = size;
    tw_.resize(static_cast<size_t>(size / 2));
    for (int i = 0; i < size / 2; ++i) tw_[static_cast<size_t>(i)] = std::polar(1.0f, -2.0f * 3.14159265358979f * i / size);
    rev_.resize(static_cast<size_t>(size));
    int bits = 0;
    while ((1 << bits) < size) ++bits;
    for (int i = 0; i < size; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        rev_[static_cast<size_t>(i)] = r;
    }
}

void Fft::transform(cfloat* d, bool inv) const {
    const int n = n_;
    for (int i = 0; i < n; ++i) {
        const int r = rev_[static_cast<size_t>(i)];
        if (r > i) std::swap(d[i], d[r]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        const int half = len >> 1, step = n / len;
        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; ++j) {
                cfloat w = tw_[static_cast<size_t>(j * step)];
                if (inv) w = std::conj(w);
                const cfloat u = d[i + j], v = d[i + j + half] * w;
                d[i + j] = u + v;
                d[i + j + half] = u - v;
            }
        }
    }
}

// ---------------------------------------------------------------------------- STFT
void Stft::prepare(int fftSize, int hop) {
    n_ = fftSize;
    hop_ = hop;
    fft_.prepare(fftSize);
    win_.resize(static_cast<size_t>(n_));
    double wsum = 0.0;
    for (int i = 0; i < n_; ++i) {
        win_[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(2.0f * 3.14159265358979f * i / n_);  // periodic Hann
        wsum += static_cast<double>(win_[static_cast<size_t>(i)]) * win_[static_cast<size_t>(i)];
    }
    // overlap-add of analysis*synthesis windows sums to wsum/hop; also undo the unscaled inverse FFT
    norm_ = static_cast<float>(hop_ / wsum / n_);
    inFifo_.assign(static_cast<size_t>(n_), 0.0f);
    outFifo_.assign(static_cast<size_t>(n_), 0.0f);
    accum_.assign(static_cast<size_t>(n_), 0.0f);
    frame_.assign(static_cast<size_t>(n_), cfloat());
    rover_ = fifoOffset();
}

void Stft::reset() {
    std::fill(inFifo_.begin(), inFifo_.end(), 0.0f);
    std::fill(outFifo_.begin(), outFifo_.end(), 0.0f);
    std::fill(accum_.begin(), accum_.end(), 0.0f);
    rover_ = fifoOffset();
}

void Stft::mirror(cfloat* spec, int n) {
    spec[0] = cfloat(spec[0].real(), 0.0f);
    spec[n / 2] = cfloat(spec[n / 2].real(), 0.0f);
    for (int k = 1; k < n / 2; ++k) spec[n - k] = std::conj(spec[k]);
}

}  // namespace ih1::dsp
