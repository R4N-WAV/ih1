// Shared measurement helpers for tests.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <functional>
#include <vector>

#include "ih1/dsp/Fft.h"
#include "ih1/fx/FxProcessor.h"

namespace ih1::test {

// Steady-state gain of a per-sample process at frequency f (RMS ratio after settling).
inline float sineGain(const std::function<float(float)>& proc, double f, double sr, int settle = 8000, int measure = 9600) {
    double in = 0, out = 0;
    for (int n = 0; n < settle + measure; ++n) {
        const float x = static_cast<float>(std::sin(2 * M_PI * f * n / sr));
        const float y = proc(x);
        if (n >= settle) { in += x * x; out += static_cast<double>(y) * y; }
    }
    return static_cast<float>(std::sqrt(out / in));
}

// Largest non-harmonic spectral component relative to the fundamental (dBc), Blackman-Harris window.
inline float largestAliasDbc(const std::vector<float>& y, double sr, double f0) {
    int N = 1;
    while (N * 2 <= static_cast<int>(y.size())) N *= 2;
    dsp::Fft fft;
    fft.prepare(N);
    std::vector<dsp::cfloat> d(static_cast<size_t>(N));
    const size_t off = y.size() - static_cast<size_t>(N);
    for (int i = 0; i < N; ++i) {
        const double t = 2 * M_PI * i / N;
        const double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t);
        d[static_cast<size_t>(i)] = dsp::cfloat(static_cast<float>(y[off + static_cast<size_t>(i)] * w), 0.0f);
    }
    fft.forward(d.data());
    std::vector<double> mag(static_cast<size_t>(N / 2));
    for (int k = 0; k < N / 2; ++k) mag[static_cast<size_t>(k)] = std::abs(d[static_cast<size_t>(k)]);
    const double binHz = sr / N;
    auto peakNear = [&](double f) {
        const int c = static_cast<int>(std::lround(f / binHz));
        double m = 0;
        for (int k = std::max(0, c - 4); k <= std::min(N / 2 - 1, c + 4); ++k) m = std::max(m, mag[static_cast<size_t>(k)]);
        return m;
    };
    const double fund = peakNear(f0);
    // mask harmonics (and their folded positions are what we want to find => only mask true harmonics < Nyquist)
    std::vector<bool> masked(static_cast<size_t>(N / 2), false);
    for (int h = 1; h * f0 < sr / 2; ++h) {
        const int c = static_cast<int>(std::lround(h * f0 / binHz));
        for (int k = std::max(0, c - 6); k <= std::min(N / 2 - 1, c + 6); ++k) masked[static_cast<size_t>(k)] = true;
    }
    double worst = 1e-12;
    const int kmin = static_cast<int>(20.0 / binHz), kmax = std::min(N / 2 - 1, static_cast<int>(20000.0 / binHz));
    for (int k = std::max(kmin, 6); k <= kmax; ++k) if (!masked[static_cast<size_t>(k)]) worst = std::max(worst, mag[static_cast<size_t>(k)]);
    return static_cast<float>(20.0 * std::log10(worst / fund));
}

// Run an FX processor on a stereo buffer (in place) in blocks.
inline void runFx(FxProcessor& fx, std::vector<float>& L, std::vector<float>& R, int block, double sr,
                  double bpm = 120.0, const std::vector<MidiEvent>* midi = nullptr) {
    ProcessContext ctx;
    ctx.sampleRate = sr;
    ctx.bpm = bpm;
    std::vector<MidiEvent> ev;
    for (size_t off = 0; off < L.size(); off += static_cast<size_t>(block)) {
        const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), L.size() - off));
        ev.clear();
        if (midi)
            for (auto e : *midi)
                if (e.sampleOffset >= static_cast<int>(off) && e.sampleOffset < static_cast<int>(off) + n) {
                    e.sampleOffset -= static_cast<int>(off);
                    ev.push_back(e);
                }
        ctx.midi = ev.data();
        ctx.numMidi = static_cast<int>(ev.size());
        float* ch[2] = {L.data() + off, R.data() + off};
        fx.process(ch, n, ctx);
    }
}

inline double rms(const std::vector<float>& x, size_t from = 0, size_t to = 0) {
    if (to == 0 || to > x.size()) to = x.size();
    double s = 0;
    for (size_t i = from; i < to; ++i) s += static_cast<double>(x[i]) * x[i];
    return std::sqrt(s / std::max<size_t>(1, to - from));
}

}  // namespace ih1::test
