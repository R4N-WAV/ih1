// Shared DSP primitive tests (numerical; criteria mirror KADAL validation notes).
#include <doctest/doctest.h>

#include <cmath>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Fft.h"
#include "ih1/dsp/Nonlinear.h"
#include "ih1/dsp/Svf.h"
#include "TestUtil.h"

using namespace ih1;
using namespace ih1::dsp;

TEST_CASE("SVF: measured response matches the analytic response") {
    const double sr = 48000.0;
    struct C { Svf::Mode m; float fc, q, g; };
    for (C c : {C{Svf::Lowpass, 1000, 0.707f, 0}, C{Svf::Highpass, 1000, 0.707f, 0}, C{Svf::Bell, 2000, 1.0f, 6},
                C{Svf::Bell, 2000, 1.0f, -9}, C{Svf::LowShelf, 300, 0.707f, 6}, C{Svf::HighShelf, 6000, 0.707f, -6},
                C{Svf::Bandpass, 800, 4.0f, 0}}) {
        for (float f : {100.0f, 1000.0f, 2000.0f, 8000.0f}) {
            Svf s;
            s.setup(c.m, c.fc, c.q, c.g, sr);
            const float measured = test::sineGain([&](float x) { return s.process(x); }, f, sr);
            CHECK(gainToDb(measured) == doctest::Approx(gainToDb(s.magnitudeAt(f, sr))).epsilon(0.02).scale(1.0));
        }
    }
    Svf lp;
    lp.setup(Svf::Lowpass, 1000, 0.70710678f, 0, sr);
    CHECK(gainToDb(lp.magnitudeAt(1000, sr)) == doctest::Approx(-3.01).epsilon(0.01));
    Svf bell;
    bell.setup(Svf::Bell, 2000, 1.0f, 6.0f, sr);
    CHECK(gainToDb(bell.magnitudeAt(2000, sr)) == doctest::Approx(6.0).epsilon(0.01));
}

TEST_CASE("LR4 crossover sums flat") {
    const double sr = 48000.0;
    for (float f : {50.0f, 300.0f, 1000.0f, 3000.0f, 12000.0f}) {
        Crossover x;
        x.setup(1000.0f, sr);
        const float g = test::sineGain([&](float in) { float lo, hi; x.process(in, lo, hi); return lo + hi; }, f, sr);
        CHECK(std::fabs(gainToDb(g)) < 0.05f);
    }
}

TEST_CASE("fractional delay: interpolation error is small for audio-band signals") {
    DelayLine d;
    d.prepare(1000);
    const double sr = 48000.0, f = 2000.0;
    double errCubic = 0, errLin = 0;
    for (int n = 0; n < 4000; ++n) {
        d.push(static_cast<float>(std::sin(2 * M_PI * f * n / sr)));
        if (n > 200) {
            const float D = 37.37f;
            const double ref = std::sin(2 * M_PI * f * (n - D) / sr);
            errCubic = std::max(errCubic, std::fabs(d.readCubic(D) - ref));
            errLin = std::max(errLin, std::fabs(d.readLinear(D) - ref));
            CHECK(std::fabs(d.readLagrange(D) - ref) < 2e-3);
        }
    }
    CHECK(errCubic < 2e-3);
    CHECK(errCubic < errLin);
}

TEST_CASE("ADAA + 2x oversampling suppresses aliasing (KADAL KNW-MCXD9ZQP83 conditions)") {
    const double sr = 48000.0;
    const double f0 = 4999.0;
    const int N = 1 << 15;
    auto aliasDb = [&](bool adaa, bool os) {
        AdaaShaper sh;
        sh.setShape(Shape::Tanh);
        Oversampler2x ov;
        ov.prepare(N);
        std::vector<float> x(N), y(N);
        for (int n = 0; n < N; ++n) x[n] = 4.0f * static_cast<float>(std::sin(2 * M_PI * f0 * n / sr));
        if (os) {
            float* hi = ov.up(x.data(), N);
            for (int i = 0; i < 2 * N; ++i) hi[i] = adaa ? sh.process(hi[i]) : std::tanh(hi[i]);
            ov.down(hi, y.data(), N);
        } else {
            for (int n = 0; n < N; ++n) y[n] = adaa ? sh.process(x[n]) : std::tanh(x[n]);
        }
        return test::largestAliasDbc(y, sr, f0);
    };
    const float naive = aliasDb(false, false);
    const float best = aliasDb(true, true);
    MESSAGE("naive alias " << naive << " dBc, ADAA+2x " << best << " dBc");
    CHECK(naive > -40.0f);
    CHECK(best < -60.0f);
}

TEST_CASE("oversampler: unity passband, integer latency") {
    Oversampler2x ov;
    const int N = 4096;
    ov.prepare(N);
    const int lat = ov.latency();
    CHECK(lat == 23);
    for (double f : {100.0, 1000.0, 10000.0, 17000.0}) {
        ov.reset();
        std::vector<float> x(N), y(N);
        for (int n = 0; n < N; ++n) x[n] = static_cast<float>(std::sin(2 * M_PI * f * n / 48000.0));
        float* hi = ov.up(x.data(), N);
        ov.down(hi, y.data(), N);
        double err = 0;
        for (int n = 200; n < N; ++n) err = std::max(err, static_cast<double>(std::fabs(y[n] - x[n - lat])));
        if (f <= 10000.0) CHECK(err < 2e-3);
        else CHECK(err < 0.12);  // <1 dB droop near 17 kHz
    }
}

TEST_CASE("FFT round trip and STFT identity with exact latency") {
    Fft fft;
    fft.prepare(256);
    std::vector<cfloat> d(256);
    Rng r(3);
    for (auto& v : d) v = cfloat(r.bipolar(), r.bipolar());
    auto orig = d;
    fft.forward(d.data());
    fft.inverse(d.data());
    for (int i = 0; i < 256; ++i) CHECK(std::abs(d[i] / 256.0f - orig[i]) < 1e-5f);

    Stft st;
    st.prepare(1024, 256);
    CHECK(st.latency() == 1024);
    std::vector<float> x(16384), y;
    for (auto& v : x) v = r.bipolar() * 0.5f;
    y = x;
    st.process(y.data(), static_cast<int>(y.size()), [](cfloat*, int) {});
    double err = 0;
    for (size_t n = 2048; n < x.size(); ++n) err = std::max(err, static_cast<double>(std::fabs(y[n] - x[n - 1024])));
    CHECK(err < 1e-4);
}

TEST_CASE("RNG and LFO are deterministic and bounded") {
    Rng a(42), b(42);
    for (int i = 0; i < 1000; ++i) CHECK(a.nextU32() == b.nextU32());
    for (int shape = 0; shape <= 5; ++shape) {
        Lfo l;
        l.prepare(48000.0, 9);
        l.setShape(shape);
        l.setRate(3.0f);
        for (int i = 0; i < 48000; ++i) {
            const float v = l.next();
            CHECK(v >= -1.0001f);
            CHECK(v <= 1.0001f);
        }
    }
}
