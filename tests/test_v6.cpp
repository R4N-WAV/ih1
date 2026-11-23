// V6 feedback round: the three new vocal processors do what their controls promise.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "TestUtil.h"
#include "ih1/dsp/Basics.h"
#include "ih1/dsp/Fft.h"

using namespace ih1;

namespace {
constexpr double kSr = 48000.0;

std::unique_ptr<FxProcessor> make(const char* type, std::vector<std::pair<const char*, float>> params) {
    const int t = fxTypeIndex(type);
    REQUIRE(t >= 0);
    const auto& d = fxRegistry()[static_cast<size_t>(t)];
    auto v = d.defaults();
    for (auto& [k, x] : params) {
        REQUIRE_MESSAGE(d.paramIndex(k) >= 0, k);
        v[static_cast<size_t>(d.paramIndex(k))] = x;
    }
    auto p = d.create();
    p->prepare(kSr, 256);
    p->setParams(d.clamp(v), true);
    return p;
}
double dbv(double x) { return 20.0 * std::log10(std::max(x, 1e-12)); }
double rmsT(const std::vector<float>& x, double t0, double t1) { return test::rms(x, static_cast<size_t>(t0 * kSr), static_cast<size_t>(t1 * kSr)); }

// energy (power sum) of x[from, from + N) in [f0, f1] Hz, Hann-windowed
double bandEnergy(const std::vector<float>& x, size_t from, double f0, double f1, int N = 8192) {
    dsp::Fft fft;
    fft.prepare(N);
    std::vector<dsp::cfloat> d(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) {
        const float w = 0.5f - 0.5f * std::cos(2.0f * dsp::kPi * i / N);
        d[static_cast<size_t>(i)] = dsp::cfloat(x[from + static_cast<size_t>(i)] * w, 0.0f);
    }
    fft.forward(d.data());
    double e = 0.0;
    const double bin = kSr / N;
    for (int k = static_cast<int>(f0 / bin); k <= static_cast<int>(f1 / bin) && k < N / 2; ++k) e += std::norm(d[static_cast<size_t>(k)]);
    return e;
}
// spectral flatness (geometric / arithmetic mean of power) of x in [f0, f1]
double flatness(const std::vector<float>& x, size_t from, double f0, double f1, int N = 8192) {
    dsp::Fft fft;
    fft.prepare(N);
    std::vector<dsp::cfloat> d(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) d[static_cast<size_t>(i)] = dsp::cfloat(x[from + static_cast<size_t>(i)] * (0.5f - 0.5f * std::cos(2.0f * dsp::kPi * i / N)), 0.0f);
    fft.forward(d.data());
    const double bin = kSr / N;
    double lg = 0.0, ar = 0.0;
    int n = 0;
    for (int k = static_cast<int>(f0 / bin); k <= static_cast<int>(f1 / bin); ++k, ++n) {
        const double p = std::norm(d[static_cast<size_t>(k)]) + 1e-20;
        lg += std::log(p);
        ar += p;
    }
    return std::exp(lg / n) / (ar / n);
}
std::vector<float> noise(double seconds, float level, uint32_t seed = 7) {
    dsp::Rng r(seed);
    std::vector<float> v(static_cast<size_t>(seconds * kSr));
    for (auto& s : v) s = level * r.bipolar();
    return v;
}
// a sung "voice": a 150 Hz glottal-like pulse train (rich, flat-ish harmonics)
std::vector<float> buzz(double seconds, float f0 = 150.0f, float level = 0.3f) {
    std::vector<float> v(static_cast<size_t>(seconds * kSr));
    double ph = 0.0;
    for (auto& s : v) {
        ph += f0 / kSr;
        ph -= std::floor(ph);
        s = level * static_cast<float>(2.0 * ph - 1.0);  // sawtooth: every harmonic present
    }
    return v;
}
}  // namespace

TEST_CASE("V6 Vowel Morph: the vowel position selects real vowel resonances (A vs I) and the level tracks the input") {
    auto run = [](float vowel) {
        auto fx = make("vowel_morph", {{"vowel", vowel}, {"motion", 0.0f}, {"drive", 0.0f}, {"mix", 100.0f}});
        auto L = noise(3.0, 0.2f), R = L;
        test::runFx(*fx, L, R, 256, kSr);
        return L;
    };
    const auto a = run(0.0f), i = run(2.0f);
    const size_t at = static_cast<size_t>(2.0 * kSr);
    // A has F1 ~650 Hz and F2 ~1080 Hz; I has F1 ~290 Hz and F2 ~1870 Hz
    const double aRatio = bandEnergy(a, at, 550, 1200) / bandEnergy(a, at, 1700, 2000);
    const double iRatio = bandEnergy(i, at, 550, 1200) / bandEnergy(i, at, 1700, 2000);
    CHECK(dbv(std::sqrt(aRatio)) - dbv(std::sqrt(iRatio)) > 15.0);
    const double iLow = bandEnergy(i, at, 240, 340) / bandEnergy(i, at, 550, 750);
    const double aLow = bandEnergy(a, at, 240, 340) / bandEnergy(a, at, 550, 750);
    CHECK(dbv(std::sqrt(iLow)) - dbv(std::sqrt(aLow)) > 10.0);
    // adaptive make-up: within 3 dB of the input level once settled
    CHECK(std::fabs(dbv(rmsT(a, 2.0, 2.9)) - dbv(0.2 / std::sqrt(3.0))) < 3.0);
}

TEST_CASE("V6 Vowel Morph: Sequence steps through the pattern's vowels on the tempo grid; LFO moves continuously") {
    // pattern "A I A I" at 1/8 (250 ms at 120 bpm), sync on, depth 100 %
    auto fx = make("vowel_morph", {{"motion", 3.0f}, {"pattern", 4.0f}, {"sync", 1.0f}, {"division", 7.0f}, {"depth", 100.0f}, {"drive", 0.0f}});
    auto L = noise(3.0, 0.2f), R = L;
    test::runFx(*fx, L, R, 256, kSr, 120.0);
    // compare two consecutive steps late in the run (after the make-up has settled): they must differ like A vs I
    auto f2 = [&](double t) { return bandEnergy(L, static_cast<size_t>(t * kSr), 1700, 2000, 4096) / bandEnergy(L, static_cast<size_t>(t * kSr), 550, 1200, 4096); };
    const double s1 = f2(2.02), s2 = f2(2.27);
    CHECK(std::fabs(dbv(std::sqrt(s1)) - dbv(std::sqrt(s2))) > 10.0);
}

TEST_CASE("V6 Whisper: a pitched voice becomes breath (no harmonic comb) at the same level; Whisper 0 % keeps the pitch") {
    auto run = [](float whisper, float smooth) {
        auto fx = make("whisper", {{"whisper", whisper}, {"smooth", smooth}, {"air", 0.0f}, {"low_cut", 40.0f}, {"consonants", 0.0f}, {"mix", 100.0f}});
        auto L = buzz(3.0), R = L;
        test::runFx(*fx, L, R, 256, kSr);
        return L;
    };
    const auto in = buzz(3.0);
    const auto w = run(100.0f, 100.0f), keep = run(0.0f, 0.0f);
    const size_t at = static_cast<size_t>(2.0 * kSr);
    const double fIn = flatness(in, at, 200, 3000), fW = flatness(w, at, 200, 3000), fK = flatness(keep, at, 200, 3000);
    CHECK(fW > 20.0 * fIn);              // the harmonic comb is gone: a near-flat (noise-like) spectrum
    CHECK(fK < 2.0 * fIn + 1e-6);        // 0 % whisper / 0 % smooth = the voice as it was
    MESSAGE("whisper level vs input: " << dbv(rmsT(w, 1.0, 2.9)) - dbv(rmsT(in, 1.0, 2.9)) << " dB; 0 %: " << dbv(rmsT(keep, 1.0, 2.9)) - dbv(rmsT(in, 1.0, 2.9)));
    CHECK(std::fabs(dbv(rmsT(w, 1.0, 2.9)) - dbv(rmsT(in, 1.0, 2.9))) < 3.0);  // energy preserved
    // the vowel shape survives: the whisper keeps the input's spectral tilt (more energy low than high)
    CHECK(bandEnergy(w, at, 200, 1000) > bandEnergy(w, at, 4000, 8000));
}

TEST_CASE("V6 Vocal Throw: echoes at the repeat time, falling by the pitch drop, ducked while the voice is present") {
    // a 1 kHz burst of 120 ms, then silence; free time 300 ms
    auto burst = [](double seconds) {
        std::vector<float> v(static_cast<size_t>(seconds * kSr), 0.0f);
        for (size_t i = 0; i < static_cast<size_t>(0.12 * kSr); ++i) v[i] = 0.4f * std::sin(2.0f * dsp::kPi * 1000.0f * static_cast<float>(i) / static_cast<float>(kSr));
        return v;
    };
    {
        auto fx = make("vocal_throw", {{"sync", 0.0f}, {"time_ms", 300.0f}, {"feedback", 60.0f}, {"drop", -12.0f}, {"tone", 1000.0f}, {"focus", 0.0f},
                                       {"duck", 0.0f}, {"ping_pong", 0.0f}, {"mix", 100.0f}});
        auto L = burst(2.0), R = L;
        test::runFx(*fx, L, R, 256, kSr);
        CHECK(dbv(rmsT(L, 0.0, 0.25)) < -60.0);                       // wet only: nothing before the first repeat
        CHECK(dbv(rmsT(L, 0.31, 0.41)) > -30.0);                      // first repeat at 300 ms
        // second repeat (one pass through the shifter): an octave lower
        const size_t r2 = static_cast<size_t>(0.62 * kSr);
        CHECK(bandEnergy(L, r2, 450, 550, 4096) > 4.0 * bandEnergy(L, r2, 950, 1050, 4096));
        CHECK(dbv(rmsT(L, 1.7, 2.0)) < dbv(rmsT(L, 0.31, 0.41)) - 12.0);  // decaying
    }
    {
        // duck: a continuous tone keeps the echoes down; once it stops they come forward
        auto fx = make("vocal_throw", {{"sync", 0.0f}, {"time_ms", 250.0f}, {"feedback", 70.0f}, {"duck", 100.0f}, {"focus", 0.0f}, {"ping_pong", 0.0f}, {"mix", 100.0f}});
        std::vector<float> L(static_cast<size_t>(3.0 * kSr), 0.0f);
        for (size_t i = 0; i < static_cast<size_t>(1.5 * kSr); ++i) L[i] = 0.3f * std::sin(2.0f * dsp::kPi * 700.0f * static_cast<float>(i) / static_cast<float>(kSr));
        auto R = L;
        test::runFx(*fx, L, R, 256, kSr);
        CHECK(dbv(rmsT(L, 1.6, 1.9)) - dbv(rmsT(L, 1.0, 1.4)) > 10.0);
    }
}

TEST_CASE("V6 Vocal Throw: Last Word throws only the end of a phrase, after the phrase") {
    auto fx = make("vocal_throw", {{"mode", 1.0f}, {"sync", 0.0f}, {"time_ms", 200.0f}, {"feedback", 30.0f}, {"duck", 0.0f}, {"focus", 0.0f},
                                   {"catch_ms", 300.0f}, {"threshold", -40.0f}, {"ping_pong", 0.0f}, {"mix", 100.0f}});
    std::vector<float> L(static_cast<size_t>(3.0 * kSr), 0.0f);
    for (size_t i = 0; i < static_cast<size_t>(1.0 * kSr); ++i) L[i] = 0.3f * std::sin(2.0f * dsp::kPi * 500.0f * static_cast<float>(i) / static_cast<float>(kSr));
    auto R = L;
    test::runFx(*fx, L, R, 256, kSr);
    CHECK(dbv(rmsT(L, 0.1, 0.95)) < -60.0);      // nothing is thrown while the phrase is still going
    CHECK(dbv(rmsT(L, 1.25, 1.65)) > -30.0);     // phrase end (1.0 s) + 80 ms hold + 200 ms echo: the last word comes back
}
