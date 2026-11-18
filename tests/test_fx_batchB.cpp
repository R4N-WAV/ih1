// Behavioural tests for the Batch B/C effects.
#include <doctest/doctest.h>

#include <cmath>
#include <set>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/dsp/Fft.h"
#include "ih1/fx/FxProcessor.h"
#include "TestUtil.h"

using namespace ih1;

namespace {
constexpr double kSr = 48000.0;

struct Fx {
    std::unique_ptr<FxProcessor> p;
    const FxDescriptor* d;
    ParamValues v;
    explicit Fx(const char* id) {
        const int t = fxTypeIndex(id);
        REQUIRE(t >= 0);
        d = &fxRegistry()[static_cast<size_t>(t)];
        v = d->defaults();
        p = d->create();
        p->prepare(kSr, 512);
        p->setParams(v, true);
    }
    Fx& set(const char* pid, float val) {
        const int i = d->paramIndex(pid);
        REQUIRE_MESSAGE(i >= 0, pid);
        v[static_cast<size_t>(i)] = d->params[static_cast<size_t>(i)].clamp(val);
        p->setParams(v, true);
        return *this;
    }
    void run(std::vector<float>& L, std::vector<float>& R, const std::vector<MidiEvent>* midi = nullptr) { test::runFx(*p, L, R, 256, kSr, 120.0, midi); }
};

std::vector<float> sine(double f, size_t n, float a = 0.5f) {
    std::vector<float> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = a * static_cast<float>(std::sin(2 * M_PI * f * static_cast<double>(i) / kSr));
    return v;
}
std::vector<float> noise(size_t n, float a = 0.3f, uint32_t seed = 1) {
    dsp::Rng r(seed);
    std::vector<float> v(n);
    for (auto& x : v) x = a * r.bipolar();
    return v;
}
double db(const std::vector<float>& x, size_t a, size_t b) { return 20.0 * std::log10(test::rms(x, a, b) + 1e-12); }

// magnitude spectrum (Hann) of x[from, from+N)
std::vector<double> spectrum(const std::vector<float>& x, size_t from, int N) {
    dsp::Fft f;
    f.prepare(N);
    std::vector<dsp::cfloat> d(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) d[static_cast<size_t>(i)] = dsp::cfloat(x[from + static_cast<size_t>(i)] * (0.5f - 0.5f * std::cos(6.2831853f * i / N)), 0.0f);
    f.forward(d.data());
    std::vector<double> m(static_cast<size_t>(N / 2));
    for (int k = 0; k < N / 2; ++k) m[static_cast<size_t>(k)] = std::abs(d[static_cast<size_t>(k)]);
    return m;
}
double bandDb(const std::vector<double>& m, double f, int N, double halfWidthHz = 15) {
    double s = 0;
    for (size_t k = 0; k < m.size(); ++k) { const double hz = k * kSr / N; if (std::fabs(hz - f) <= halfWidthHz) s += m[k] * m[k]; }
    return 10 * std::log10(s + 1e-20);
}
}  // namespace

TEST_CASE("Flanger and Phaser carve deep moving notches into noise") {
    for (const char* id : {"flanger", "phaser"}) {
        CAPTURE(id);
        Fx fx(id);
        fx.set("rate", 0.02f);
        auto L = noise(96000), R = L;
        fx.run(L, R);
        const int N = 8192;
        auto m = spectrum(L, 48000, N);
        // smooth over 6 bins, then compare deepest notch with typical level in 200 Hz..10 kHz
        std::vector<double> sm;
        for (size_t k = 40; k < 1700; ++k) { double s = 0; for (int j = -3; j <= 3; ++j) s += m[k + static_cast<size_t>(j)] * m[k + static_cast<size_t>(j)]; sm.push_back(s); }
        auto sorted = sm;
        std::sort(sorted.begin(), sorted.end());
        const double depth = 10 * std::log10(sorted[sorted.size() / 2] / (sorted[2] + 1e-20));
        MESSAGE(id << " notch depth " << depth << " dB");
        CHECK(depth > 12.0);
    }
}

TEST_CASE("Vibrato swings pitch by about the set depth in cents") {
    Fx fx("vibrato");
    fx.set("depth", 50).set("rate", 2).set("mix", 100);
    auto L = sine(1000, 96000), R = L;
    fx.run(L, R);
    double mn = 1e9, mx = 0, last = -1;
    for (size_t i = 24001; i < L.size(); ++i)
        if (L[i - 1] < 0 && L[i] >= 0) {
            const double t = static_cast<double>(i) - L[i] / (L[i] - L[i - 1]);
            if (last > 0) { const double f = kSr / (t - last); mn = std::min(mn, f); mx = std::max(mx, f); }
            last = t;
        }
    const double swingCents = 1200 * std::log2(mx / 1000.0);
    MESSAGE("vibrato peak deviation " << swingCents << " cents");
    CHECK(swingCents == doctest::Approx(50).epsilon(0.3));
}

TEST_CASE("Tremolo at full depth pulses the level; Auto-Pan keeps constant power") {
    Fx fx("tremolo");
    fx.set("sync", 0).set("rate", 4).set("depth", 100).set("shape", 2);
    auto L = sine(500, 48000), R = L;
    fx.run(L, R);
    double lo = 1e9, hi = -1e9;
    for (size_t s = 4800; s + 600 < L.size(); s += 600) { const double v = db(L, s, s + 600); lo = std::min(lo, v); hi = std::max(hi, v); }
    CHECK(hi - lo > 25.0);
    Fx pan("tremolo");
    pan.set("mode", 1).set("sync", 0).set("rate", 2).set("depth", 100);
    auto a = sine(500, 48000), b = a;
    pan.run(a, b);
    double pmin = 1e9, pmax = 0;
    for (size_t s = 4800; s + 600 < a.size(); s += 600) {
        const double p = test::rms(a, s, s + 600) * test::rms(a, s, s + 600) + test::rms(b, s, s + 600) * test::rms(b, s, s + 600);
        pmin = std::min(pmin, p); pmax = std::max(pmax, p);
    }
    CHECK(10 * std::log10(pmax / pmin) < 1.0);
}

TEST_CASE("Ring modulator: sum and difference sidebands, no carrier or input leak") {
    Fx fx("ring_mod");
    fx.set("freq", 200).set("mix", 100);
    auto L = sine(1000, 48000), R = L;
    fx.run(L, R);
    const int N = 16384;
    auto m = spectrum(L, 24000, N);
    CHECK(bandDb(m, 800, N) - bandDb(m, 1000, N) > 40.0);
    CHECK(bandDb(m, 1200, N) - bandDb(m, 200, N) > 40.0);
}

TEST_CASE("Rotary at Fast throbs at the rotor speed") {
    Fx fx("rotary");
    fx.set("speed", 1).set("accel", 0.2f);
    auto L = sine(3000, 96000), R = L;
    fx.run(L, R);
    // envelope fluctuation of the horn band (AM) over 1 s
    double lo = 1e9, hi = -1e9;
    for (size_t s = 48000; s + 480 < L.size(); s += 480) { const double v = db(L, s, s + 480); lo = std::min(lo, v); hi = std::max(hi, v); }
    CHECK(hi - lo > 3.0);
}

TEST_CASE("Compressor follows its static curve; Gate attenuates below threshold") {
    Fx fx("compressor");
    fx.set("threshold", -20).set("ratio", 4).set("knee", 0).set("auto_makeup", 0).set("attack", 1).set("release", 50);
    auto L = sine(1000, 48000, 0.316f * 1.414f), R = L;  // -10 dBFS RMS-ish peak level = -7 dBFS peak
    fx.run(L, R);
    const double in = 20 * std::log10(0.316 * 1.414), out = 20 * std::log10(*std::max_element(L.begin() + 24000, L.end()));
    // peak detector: input peak -7 dB, 13 dB over threshold -> 9.75 dB reduction
    CHECK(out == doctest::Approx(in - 9.75).epsilon(0.06));

    Fx gate("gate");
    gate.set("threshold", -30).set("range", 40);
    auto q = sine(1000, 48000, 0.003f), q2 = q;  // ~ -50 dBFS
    gate.run(q, q2);
    CHECK(db(q, 24000, 48000) < db(sine(1000, 48000, 0.003f), 24000, 48000) - 35.0);
    Fx gate2("gate");
    gate2.set("threshold", -30).set("range", 40);
    auto lo = sine(1000, 48000, 0.3f), lo2 = lo;
    gate2.run(lo, lo2);
    CHECK(std::fabs(db(lo, 24000, 48000) - db(sine(1000, 48000, 0.3f), 24000, 48000)) < 0.5);
}

TEST_CASE("De-esser reduces the sibilant band only; Transient shaper emphasises attacks") {
    auto gainAt = [](double f) {
        Fx fx("deesser");
        fx.set("freq", 7000).set("threshold", -30).set("range", 12).set("relative", 0);
        auto a = sine(f, 48000, 0.4f), b = a;
        fx.run(a, b);
        return db(a, 24000, 48000) - db(sine(f, 48000, 0.4f), 24000, 48000);
    };
    CHECK(gainAt(7000) < -6.0);
    CHECK(std::fabs(gainAt(400)) < 0.5);

    Fx ts("transient");
    ts.set("attack", 100).set("sustain", 0);
    std::vector<float> L(48000, 0.0f);
    for (int k = 0; k < 4; ++k) for (int i = 0; i < 6000; ++i) L[static_cast<size_t>(k * 12000 + i)] = 0.3f * std::exp(-i / 2000.0f) * static_cast<float>(std::sin(2 * M_PI * 300 * i / kSr));
    auto in = L, R = L;
    ts.run(L, R);
    const double attackGain = db(L, 36000, 36300) - db(in, 36000, 36300);
    const double tailGain = db(L, 40000, 42000) - db(in, 40000, 42000);
    MESSAGE("transient attack " << attackGain << " dB, tail " << tailGain << " dB");
    CHECK(attackGain > tailGain + 3.0);
}

TEST_CASE("Bitcrusher quantises to the set bit depth; Tape colour holds level; Exciter adds upper harmonics") {
    Fx bc("bitcrush");
    bc.set("bits", 3).set("rate", 48000).set("dither", 0).set("prefilter", 0);
    auto L = sine(220, 4800, 0.9f), R = L;
    bc.run(L, R);
    std::set<int> levels;
    for (float x : L) levels.insert(static_cast<int>(std::lround(x * 4.0f)));
    CHECK(levels.size() <= 9);

    Fx tc("tape_colour");
    tc.set("drive", 18);
    auto a = sine(200, 96000, 0.5f), b = a;
    tc.run(a, b);
    CHECK(std::fabs(db(a, 72000, 96000) - db(sine(200, 96000, 0.5f), 72000, 96000)) < 1.5);

    Fx ex("exciter");
    ex.set("freq", 1500).set("amount", 80);
    auto c = sine(2000, 48000, 0.3f), d = c;
    ex.run(c, d);
    const int N = 16384;
    auto m = spectrum(c, 24000, N), m0 = spectrum(sine(2000, 48000, 0.3f), 24000, N);
    CHECK(bandDb(m, 4000, N) > bandDb(m0, 4000, N) + 30.0);
}

TEST_CASE("Shimmer's tail climbs an octave above the input") {
    Fx fx("shimmer");
    fx.set("mix", 100).set("regen", 90).set("shift1", 12).set("balance", 0).set("decay", 8);
    auto L = sine(500, 48000, 0.4f), R = L;
    L.resize(48000 * 4, 0.0f);
    R.resize(48000 * 4, 0.0f);
    fx.run(L, R);
    const int N = 16384;
    auto m = spectrum(L, 48000 * 3, N);
    MESSAGE("tail 1 kHz " << bandDb(m, 1000, N) << " dB vs 500 Hz " << bandDb(m, 500, N));
    CHECK(bandDb(m, 1000, N) > bandDb(m, 3000, N) + 6.0);
    CHECK(bandDb(m, 1000, N) > bandDb(m, 500, N) - 6.0);
}

TEST_CASE("Pitch correction pulls a slightly sharp A4 onto 440 Hz") {
    Fx fx("pitch_correct");
    fx.set("speed", 0).set("scale", 0).set("key", 0);
    std::vector<float> L(96000, 0.0f);
    for (size_t i = 0; i < L.size(); ++i) for (int h = 1; h <= 6; ++h) L[i] += 0.2f / h * static_cast<float>(std::sin(2 * M_PI * 448.0 * h * static_cast<double>(i) / kSr));
    auto R = L;
    fx.run(L, R);
    const int N = 32768;
    auto m = spectrum(L, 48000, N);
    size_t best = 0;
    for (size_t k = 200; k < 700; ++k) if (m[k] > m[best]) best = k;
    const double hz = best * kSr / N;
    MESSAGE("corrected fundamental " << hz << " Hz");
    CHECK(hz == doctest::Approx(440.0).epsilon(0.006));
}

TEST_CASE("Vocoder speaks at the carrier pitch only while the modulator is active") {
    Fx fx("vocoder");
    fx.set("carrier", 0).set("note", 48).set("use_midi", 0).set("mix", 100);
    auto L = noise(48000, 0.3f), R = L;
    for (size_t i = 24000; i < L.size(); ++i) L[i] = R[i] = 0.0f;
    fx.run(L, R);
    const int N = 16384;
    auto m = spectrum(L, 2000, N);
    const double f0 = 130.81;
    CHECK(bandDb(m, f0 * 4, N, 6) > bandDb(m, f0 * 4.5, N, 6) + 10.0);  // harmonic structure of the carrier
    CHECK(db(L, 40000, 48000) < db(L, 4000, 20000) - 40.0);             // silent when the modulator stops
}
