#include "ih1/dsp/Spectral.h"

#include <algorithm>
#include <cmath>

namespace ih1::dsp {

namespace {
constexpr float kTwoPiF = 6.28318530717958647692f;
inline float princarg(float p) { return p - kTwoPiF * std::floor((p + 3.14159265358979f) / kTwoPiF); }
}  // namespace

int stftSizeFor(double sr, int base48k) {
    int n = base48k;
    double s = sr;
    while (s > 60000.0) { n *= 2; s *= 0.5; }
    return n;
}

// -------------------------------------------------------------------------------- analysis
void SpectralAnalysis::prepare(int fftSize, int hop) {
    n_ = fftSize;
    hop_ = hop;
    const size_t b = static_cast<size_t>(bins());
    mag.assign(b, 0.0f);
    phase.assign(b, 0.0f);
    instFreq.assign(b, 0.0f);
    env.assign(b, 1.0f);
    prevPhase_.assign(b, 0.0f);
    peaks.reserve(b);
    regionLo.reserve(b);
    regionHi.reserve(b);
    fft_.prepare(fftSize);
    cep_.assign(static_cast<size_t>(fftSize), cfloat());
    logA_.assign(b, 0.0f);
}

void SpectralAnalysis::reset() {
    std::fill(prevPhase_.begin(), prevPhase_.end(), 0.0f);
    std::fill(env.begin(), env.end(), 1.0f);
}

void SpectralAnalysis::analyse(const cfloat* X) {
    const int B = bins();
    float maxMag = 0.0f;
    for (int k = 0; k < B; ++k) {
        const float m = std::abs(X[k]);
        mag[static_cast<size_t>(k)] = m;
        maxMag = std::max(maxMag, m);
        const float ph = std::arg(X[k]);
        phase[static_cast<size_t>(k)] = ph;
        const float omega = kTwoPiF * static_cast<float>(k) / static_cast<float>(n_);
        const float dp = princarg(ph - prevPhase_[static_cast<size_t>(k)] - static_cast<float>(hop_) * omega);
        instFreq[static_cast<size_t>(k)] = omega + dp / static_cast<float>(hop_);
        prevPhase_[static_cast<size_t>(k)] = ph;
    }
    peaks.clear();
    regionLo.clear();
    regionHi.clear();
    const float floor = maxMag * 1e-4f + 1e-9f;  // -80 dB relative
    for (int k = 2; k < B - 2; ++k) {
        const float m = mag[static_cast<size_t>(k)];
        if (m > floor && m > mag[static_cast<size_t>(k - 1)] && m >= mag[static_cast<size_t>(k + 1)] && m > mag[static_cast<size_t>(k - 2)] && m >= mag[static_cast<size_t>(k + 2)])
            peaks.push_back(k);
    }
    // regions of influence: split at the lowest bin between neighbouring peaks
    for (size_t p = 0; p < peaks.size(); ++p) {
        int lo = 0, hi = B - 1;
        if (p > 0) {
            int best = peaks[p - 1] + 1;
            for (int k = peaks[p - 1] + 1; k < peaks[p]; ++k) if (mag[static_cast<size_t>(k)] < mag[static_cast<size_t>(best)]) best = k;
            lo = best;
        }
        if (p + 1 < peaks.size()) {
            int best = peaks[p] + 1;
            for (int k = peaks[p] + 1; k < peaks[p + 1]; ++k) if (mag[static_cast<size_t>(k)] < mag[static_cast<size_t>(best)]) best = k;
            hi = best - 1;
        }
        regionLo.push_back(lo);
        regionHi.push_back(hi);
    }
}

void SpectralAnalysis::estimateEnvelope(int order, int iterations, int maxHalfWidth) {
    const int B = bins();
    order = std::clamp(order, 4, n_ / 4);
    const int w = std::max(1, maxHalfWidth);
    for (int k = 0; k < B; ++k) {
        float m = 0.0f;
        for (int j = std::max(0, k - w); j <= std::min(B - 1, k + w); ++j) m = std::max(m, mag[static_cast<size_t>(j)]);
        logA_[static_cast<size_t>(k)] = std::log(m + 1e-7f);
    }
    std::vector<float>& V = env;  // reuse storage for the smoothed log envelope
    for (int it = 0; it < iterations; ++it) {
        for (int k = 0; k < B; ++k) cep_[static_cast<size_t>(k)] = cfloat(logA_[static_cast<size_t>(k)], 0.0f);
        for (int k = 1; k < n_ / 2; ++k) cep_[static_cast<size_t>(n_ - k)] = cep_[static_cast<size_t>(k)];
        fft_.inverse(cep_.data());  // real cepstrum (unscaled)
        const float inv = 1.0f / static_cast<float>(n_);
        for (int q = 0; q < n_; ++q) {
            const int qq = std::min(q, n_ - q);
            float w = 0.0f;
            if (qq < order) w = 1.0f;
            else if (qq == order) w = 0.5f;
            cep_[static_cast<size_t>(q)] = cfloat(cep_[static_cast<size_t>(q)].real() * inv * w, 0.0f);
        }
        fft_.forward(cep_.data());
        for (int k = 0; k < B; ++k) {
            const float v = cep_[static_cast<size_t>(k)].real();
            V[static_cast<size_t>(k)] = v;
            logA_[static_cast<size_t>(k)] = std::max(logA_[static_cast<size_t>(k)], v);  // true-envelope update
        }
    }
    for (int k = 0; k < B; ++k) V[static_cast<size_t>(k)] = std::exp(V[static_cast<size_t>(k)]);
}

// -------------------------------------------------------------------------------- shifter
void PeakShifter::prepare(int fftSize, int hop) {
    n_ = fftSize;
    hop_ = hop;
    for (auto& p : outPhase_) p.assign(static_cast<size_t>(fftSize / 2 + 1), 0.0f);
}

void PeakShifter::reset() {
    for (auto& p : outPhase_) std::fill(p.begin(), p.end(), 0.0f);
}

void PeakShifter::addVoice(const SpectralAnalysis& a, float ratio, float formantRatio, bool preserveFormants,
                           float gain, cfloat* outL, cfloat* outR, float gl, float gr, int voice) {
    if (gain <= 1e-6f) return;
    const int B = a.bins();
    auto& outPh = outPhase_[std::clamp(voice, 0, kMaxVoices - 1)];
    const bool envWarp = preserveFormants || std::fabs(formantRatio - 1.0f) > 1e-3f;
    auto envAt = [&](float bin) {
        bin = std::clamp(bin, 0.0f, static_cast<float>(B - 1));
        const int i = static_cast<int>(bin);
        const float f = bin - static_cast<float>(i);
        const int j = std::min(i + 1, B - 1);
        return a.env[static_cast<size_t>(i)] + f * (a.env[static_cast<size_t>(j)] - a.env[static_cast<size_t>(i)]);
    };
    for (size_t p = 0; p < a.peaks.size(); ++p) {
        const int pk = a.peaks[p];
        const float target = static_cast<float>(pk) * ratio;
        const int shift = static_cast<int>(std::lround(target)) - pk;
        const int tp = pk + shift;
        if (tp <= 0 || tp >= B - 1) continue;
        // phase-locked synthesis: advance the peak's output phase by its shifted instantaneous frequency
        const float wNew = a.instFreq[static_cast<size_t>(pk)] * ratio;
        const float phOut = princarg(outPh[static_cast<size_t>(tp)] + static_cast<float>(hop_) * wNew);
        const float rot = phOut - a.phase[static_cast<size_t>(pk)];
        const int lo = a.regionLo[p], hi = a.regionHi[p];
        for (int k = lo; k <= hi; ++k) {
            const int t = k + shift;
            if (t <= 0 || t >= B - 1) continue;
            float m = a.mag[static_cast<size_t>(k)] * gain;
            if (envWarp) {
                // preserve: the partial now at t takes the envelope value at t (or t/fr when shifting formants).
                // follow : formants move with the pitch; an extra formant shift fr samples env at k/fr.
                const float srcEnv = std::max(1e-9f, a.env[static_cast<size_t>(k)]);
                const float dstBin = preserveFormants ? static_cast<float>(t) / formantRatio : static_cast<float>(k) / formantRatio;
                m *= std::min(envAt(dstBin) / srcEnv, 16.0f);
            }
            const float ph = a.phase[static_cast<size_t>(k)] + rot;
            const cfloat v = std::polar(m, ph);
            outL[t] += v * gl;
            if (outR) outR[t] += v * gr;
            outPh[static_cast<size_t>(t)] = ph;
        }
    }
}

// -------------------------------------------------------------------------------- YIN
void Yin::prepare(double sr, int decimation, int window) {
    sr_ = sr;
    dec_ = std::max(1, decimation);
    w_ = window;
    const double dsr = sr / dec_;
    minTau_ = std::max(2, static_cast<int>(dsr / 1000.0));  // 1 kHz max
    maxTau_ = static_cast<int>(dsr / 65.0);                  // 65 Hz min
    buf_.assign(static_cast<size_t>(w_ + maxTau_ + 2), 0.0f);
    d_.assign(static_cast<size_t>(maxTau_ + 2), 0.0f);
    pos_ = 0;
    f0_ = 0.0f;
}

void Yin::push(float x) {
    // 2-pole low-pass at ~ dsr/4 before decimation
    lp_ += 0.45f * (x - lp_);
    lp2_ += 0.45f * (lp_ - lp2_);
    if (++decCount_ < dec_) return;
    decCount_ = 0;
    // shift-register (small: w + maxTau ~ 900 floats)
    std::move(buf_.begin() + 1, buf_.end(), buf_.begin());
    buf_.back() = lp2_;
}

float Yin::estimate() {
    const int W = w_;
    const float* x = buf_.data();
    double energy = 0.0;
    for (int j = 0; j < W; ++j) energy += static_cast<double>(x[j]) * x[j];
    if (energy / W < 1e-7) { f0_ = 0.0f; conf_ = 0.0f; return 0.0f; }  // ~ -70 dBFS
    float running = 0.0f;
    int best = -1;
    d_[0] = 1.0f;
    for (int tau = 1; tau <= maxTau_; ++tau) {
        float s = 0.0f;
        for (int j = 0; j < W; ++j) {
            const float diff = x[j] - x[j + tau];
            s += diff * diff;
        }
        running += s;
        d_[static_cast<size_t>(tau)] = running > 0.0f ? s * static_cast<float>(tau) / running : 1.0f;
    }
    for (int tau = minTau_; tau < maxTau_; ++tau) {
        if (d_[static_cast<size_t>(tau)] < 0.15f) {
            while (tau + 1 < maxTau_ && d_[static_cast<size_t>(tau + 1)] < d_[static_cast<size_t>(tau)]) ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0) {
        best = minTau_;
        for (int tau = minTau_; tau < maxTau_; ++tau) if (d_[static_cast<size_t>(tau)] < d_[static_cast<size_t>(best)]) best = tau;
        if (d_[static_cast<size_t>(best)] > 0.35f) { f0_ = 0.0f; conf_ = 0.0f; return 0.0f; }
    }
    float t = static_cast<float>(best);
    if (best > 1 && best < maxTau_) {
        const float a = d_[static_cast<size_t>(best - 1)], b = d_[static_cast<size_t>(best)], c = d_[static_cast<size_t>(best + 1)];
        const float den = a - 2.0f * b + c;
        if (std::fabs(den) > 1e-9f) t += 0.5f * (a - c) / den;
    }
    conf_ = 1.0f - std::min(1.0f, d_[static_cast<size_t>(best)]);
    f0_ = static_cast<float>(sr_ / dec_ / t);
    return f0_;
}

// -------------------------------------------------------------------------------- mono -> stereo STFT
void StftMonoToStereo::prepare(int fftSize, int hop) {
    n_ = fftSize;
    hop_ = hop;
    fft_.prepare(fftSize);
    win_.resize(static_cast<size_t>(n_));
    double wsum = 0.0;
    for (int i = 0; i < n_; ++i) {
        win_[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(kTwoPiF * static_cast<float>(i) / static_cast<float>(n_));
        wsum += static_cast<double>(win_[static_cast<size_t>(i)]) * win_[static_cast<size_t>(i)];
    }
    norm_ = static_cast<float>(hop_ / wsum / n_);
    inFifo_.assign(static_cast<size_t>(n_), 0.0f);
    outFifoL_.assign(static_cast<size_t>(n_), 0.0f);
    outFifoR_.assign(static_cast<size_t>(n_), 0.0f);
    accL_.assign(static_cast<size_t>(n_), 0.0f);
    accR_.assign(static_cast<size_t>(n_), 0.0f);
    frame_.assign(static_cast<size_t>(n_), cfloat());
    yl_.assign(static_cast<size_t>(n_), cfloat());
    yr_.assign(static_cast<size_t>(n_), cfloat());
    rover_ = n_ - hop_;
}

void StftMonoToStereo::reset() {
    std::fill(inFifo_.begin(), inFifo_.end(), 0.0f);
    std::fill(outFifoL_.begin(), outFifoL_.end(), 0.0f);
    std::fill(outFifoR_.begin(), outFifoR_.end(), 0.0f);
    std::fill(accL_.begin(), accL_.end(), 0.0f);
    std::fill(accR_.begin(), accR_.end(), 0.0f);
    rover_ = n_ - hop_;
}

}  // namespace ih1::dsp
