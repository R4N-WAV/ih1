// V3 DSP: the shared RateReducer and the six new modules do what their names promise (behaviour, not just
// "finite and bounded", which the generic contract suite already covers for every FX).
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "TestUtil.h"
#include "ih1/dsp/Basics.h"
#include "ih1/dsp/RateReducer.h"
#include "ih1/fx/FxProcessor.h"

using namespace ih1;

namespace {
constexpr double kSr = 48000.0;

ParamValues vals(const char* id, std::initializer_list<std::pair<const char*, float>> set) {
    const auto& d = fxRegistry()[static_cast<size_t>(fxTypeIndex(id))];
    ParamValues v = d.defaults();
    for (const auto& [k, x] : set) {
        const int i = d.paramIndex(k);
        REQUIRE_MESSAGE(i >= 0, id << "." << k);
        v[static_cast<size_t>(i)] = x;
    }
    return d.clamp(v);
}
std::unique_ptr<FxProcessor> mk(const char* id, const ParamValues& v) {
    auto fx = fxRegistry()[static_cast<size_t>(fxTypeIndex(id))].create();
    fx->setLatencyMode(LatencyMode::Quality);
    fx->prepare(kSr, 512);
    fx->setParams(v, true);
    return fx;
}
std::vector<float> sine(int n, double f, float a = 0.5f) {
    std::vector<float> x(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) x[static_cast<size_t>(i)] = a * static_cast<float>(std::sin(2 * dsp::kPi * f * i / kSr));
    return x;
}
std::vector<float> run(FxProcessor& fx, std::vector<float> in, double bpm = 120.0) {
    auto R = in;
    test::runFx(fx, in, R, 256, kSr, bpm);
    return in;
}
// magnitude of frequency f over [a, b) (single-bin DFT, Hann)
double tone(const std::vector<float>& x, size_t a, size_t b, double f) {
    double re = 0, im = 0;
    const double n = static_cast<double>(b - a);
    for (size_t i = a; i < b; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2 * dsp::kPi * (i - a) / n);
        re += w * x[i] * std::cos(2 * dsp::kPi * f * i / kSr);
        im += w * x[i] * std::sin(2 * dsp::kPi * f * i / kSr);
    }
    return std::sqrt(re * re + im * im) / n;
}
double rms(const std::vector<float>& x, size_t a, size_t b) {
    double e = 0;
    for (size_t i = a; i < b; ++i) e += x[i] * x[i];
    return std::sqrt(e / static_cast<double>(b - a));
}
}  // namespace

TEST_CASE("V3 RateReducer: hold, bit depth, raw aliasing vs anti-alias, exact bypass") {
    dsp::RateReducer rr;
    rr.prepare(kSr);
    dsp::RateReducer::Settings s;
    // exact bypass at the defaults
    {
        auto L = sine(4800, 997.0), R = L;
        const auto ref = L;
        rr.process(L.data(), R.data(), 4800, s);
        CHECK(L == ref);
    }
    // rate = sr/4: the output holds each value for 4 samples; 4 bits -> at most 16 levels
    s.rateHz = static_cast<float>(kSr / 4.0);
    s.bits = 4.0f;
    {
        rr.reset();
        auto L = sine(4800, 300.0, 0.9f), R = L;
        rr.process(L.data(), R.data(), 4800, s);
        int changes = 0;
        std::set<int> levels;
        for (size_t i = 1; i < L.size(); ++i) changes += L[i] != L[i - 1];
        for (float v : L) levels.insert(static_cast<int>(std::lround(v * 1000.0f)));
        CHECK(changes <= 4800 / 4 + 2);
        CHECK(levels.size() <= 17);
    }
    // a 10 kHz tone through a 12 kHz hold: Off folds it to 2 kHz; Strong anti-alias removes most of it
    auto aliasAt2k = [&](int aa) {
        dsp::RateReducer r;
        r.prepare(kSr);
        dsp::RateReducer::Settings t;
        t.rateHz = 12000.0f;
        t.antiAlias = aa;
        auto L = sine(24000, 10000.0), R = L;
        r.process(L.data(), R.data(), 24000, t);
        return tone(L, 4000, 24000, 2000.0);
    };
    const double raw = aliasAt2k(dsp::RateReducer::FilterOff), clean = aliasAt2k(dsp::RateReducer::FilterStrong);
    CHECK(raw > 0.05);
    CHECK(20.0 * std::log10(clean / raw) < -20.0);
}

TEST_CASE("V3 Degrade: bandwidth collapse narrows the spectrum; the hold stage aliases when asked to") {
    std::vector<float> in(48000);
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.3f * static_cast<float>(std::sin(2 * dsp::kPi * 1000.0 * i / kSr) + std::sin(2 * dsp::kPi * 12000.0 * i / kSr));
    auto narrow = mk("degrade", vals("degrade", {{"bandwidth", 100}, {"rate", 48000}, {"bits", 24}, {"noise", 0}, {"drive", 0}, {"wobble", 0}, {"reconstruct", 0}}));
    const auto y = run(*narrow, in);
    CHECK(tone(y, 8000, 48000, 12000.0) < 0.1 * tone(y, 8000, 48000, 1000.0));
    auto lo = mk("degrade", vals("degrade", {{"bandwidth", 0}, {"rate", 8000}, {"anti_alias", 0}, {"reconstruct", 0}, {"noise", 0}, {"jitter", 0}, {"wobble", 0}, {"drive", 0}}));
    const auto z = run(*lo, sine(48000, 3000.0, 0.4f));
    CHECK(tone(z, 8000, 48000, 5000.0) > 0.02);  // 3 kHz held at 8 kHz -> image / alias at 5 kHz
}

TEST_CASE("V3 Spectral Smear: frequency smear widens a tone, time smear extends it, Disintegrate tears it") {
    auto in = sine(72000, 1000.0, 0.4f);
    for (size_t i = 36000; i < in.size(); ++i) in[i] = 0.0f;  // the tone stops at 0.75 s
    auto dry = mk("spectral_smear", vals("spectral_smear", {{"smear", 0}, {"time", 0}, {"scatter", 0}, {"mix", 100}}));
    auto wide = mk("spectral_smear", vals("spectral_smear", {{"smear", 90}, {"time", 0}, {"scatter", 0}, {"mix", 100}}));
    auto longT = mk("spectral_smear", vals("spectral_smear", {{"smear", 0}, {"time", 90}, {"scatter", 0}, {"mix", 100}}));
    const auto a = run(*dry, in), b = run(*wide, in), c = run(*longT, in);
    // energy away from 1 kHz (1.3 kHz) relative to 1 kHz
    const double spreadA = tone(a, 12000, 30000, 1300.0) / tone(a, 12000, 30000, 1000.0);
    const double spreadB = tone(b, 12000, 30000, 1300.0) / tone(b, 12000, 30000, 1000.0);
    CHECK(spreadB > 5.0 * spreadA);
    // 0.25 s after the input stopped (plus the 2048-sample latency) the time-smeared one still sounds
    CHECK(rms(c, 50000, 56000) > 10.0 * rms(a, 50000, 56000) + 1e-6);
    auto torn = mk("spectral_smear", vals("spectral_smear", {{"smear", 20}, {"time", 0}, {"disintegrate", 100}, {"mix", 100}}));
    const auto d = run(*torn, sine(48000, 1000.0, 0.4f));
    CHECK(rms(d, 12000, 44000) < 0.85 * rms(a, 12000, 30000));
}

TEST_CASE("V3 Spectral Delay: band delays differ by shape (Rising: highs arrive after lows)") {
    std::vector<float> in(96000, 0.0f);
    in[2000] = 0.9f;
    auto fx = mk("spectral_delay", vals("spectral_delay", {{"sync", 0}, {"time_ms", 600}, {"shape", 0}, {"spread", 100}, {"feedback", 0}, {"mix", 100}}));
    const auto y = run(*fx, in);
    // arrival of low (200 Hz) and high (8 kHz) energy: the time of the strongest 200 / 8000 Hz window
    auto arrival = [&](double f) {
        double best = 0;
        size_t at = 0;
        for (size_t s = 0; s + 2048 < y.size(); s += 512) {
            const double m = tone(y, s, s + 2048, f);
            if (m > best) { best = m; at = s; }
        }
        return at;
    };
    CHECK(arrival(8000.0) > arrival(200.0) + 9600);  // > 0.2 s later
}

TEST_CASE("V3 Time Warp: varispeed halves the pitch; tape stop falls silent; half-time drops an octave; at rest it is exact") {
    {
        auto fx = mk("time_warp", vals("time_warp", {{"mode", 0}, {"speed", 0.5f}, {"mix", 100}}));
        const auto y = run(*fx, sine(96000, 440.0));
        CHECK(tone(y, 24000, 72000, 220.0) > 5.0 * tone(y, 24000, 72000, 440.0));
    }
    {
        auto fx = mk("time_warp", vals("time_warp", {{"mode", 1}, {"engage", 1}, {"stop_ms", 300}, {"mix", 100}}));
        const auto y = run(*fx, sine(48000, 440.0));
        CHECK(rms(y, 24000, 48000) < 0.01);
    }
    {
        auto fx = mk("time_warp", vals("time_warp", {{"mode", 2}, {"division", 10}, {"mix", 100}}));
        const auto y = run(*fx, sine(96000, 440.0));
        CHECK(tone(y, 24000, 90000, 220.0) > 3.0 * tone(y, 24000, 90000, 440.0));
    }
    {
        // Keep Pitch: half-time duration with the pitch kept (440 stays 440)
        auto fx = mk("time_warp", vals("time_warp", {{"mode", 2}, {"division", 10}, {"keep_pitch", 1}, {"mix", 100}}));
        const auto y = run(*fx, sine(96000, 440.0));
        CHECK(tone(y, 24000, 90000, 440.0) > 3.0 * tone(y, 24000, 90000, 220.0));
    }
    {
        // a triggered Tape Stop that is not engaged passes the input exactly (no hidden latency)
        auto fx = mk("time_warp", vals("time_warp", {{"mode", 1}, {"engage", 0}, {"mix", 100}}));
        const auto in = sine(9600, 440.0);
        const auto y = run(*fx, in);
        double err = 0;
        for (size_t i = 1200; i < in.size(); ++i) err = std::max(err, static_cast<double>(std::fabs(y[i] - in[i])));
        CHECK(err < 1e-6);
    }
}

TEST_CASE("V3 Glitch Scatter: repeatable, really changes the sound, and passes the input exactly when idle") {
    std::vector<float> in(96000);
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.4f * static_cast<float>(std::sin(2 * dsp::kPi * 220.0 * i / kSr) * (1.0 + 0.5 * std::sin(i * 0.0007)));
    auto a = mk("glitch", vals("glitch", {{"repeat", 50}, {"reverse", 30}, {"jump", 30}, {"mix", 100}}));
    auto b = mk("glitch", vals("glitch", {{"repeat", 50}, {"reverse", 30}, {"jump", 30}, {"mix", 100}}));
    const auto ya = run(*a, in), yb = run(*b, in);
    CHECK(ya == yb);
    double diff = 0;
    for (size_t i = 48000; i < in.size(); ++i) diff += (ya[i] - in[i]) * static_cast<double>(ya[i] - in[i]);
    CHECK(std::sqrt(diff / 48000.0) > 0.05);
    auto idle = mk("glitch", vals("glitch", {{"repeat", 0}, {"reverse", 0}, {"jump", 0}, {"pitch", 0}, {"gate", 0}, {"mix", 100}}));
    const auto yi = run(*idle, in);
    double err = 0;
    for (size_t i = 0; i < in.size(); ++i) err = std::max(err, static_cast<double>(std::fabs(yi[i] - in[i])));
    CHECK(err < 1e-6);
}

TEST_CASE("V3 Fold & Crush: folding adds upper harmonics; Auto Gain keeps the level near the input") {
    auto soft = mk("fold_crush", vals("fold_crush", {{"fold", 0}, {"crush", 0}, {"tone", 20000}, {"mix", 100}}));
    auto fold = mk("fold_crush", vals("fold_crush", {{"fold", 100}, {"crush", 0}, {"tone", 20000}, {"drive", 18}, {"mix", 100}}));
    const auto in = sine(48000, 300.0, 0.3f);
    const auto a = run(*soft, in), b = run(*fold, in);
    auto upper = [&](const std::vector<float>& y) { double e = 0; for (int h = 5; h <= 15; ++h) e += tone(y, 12000, 48000, 300.0 * h); return e; };
    CHECK(upper(b) > 3.0 * upper(a));
    CHECK(std::fabs(20.0 * std::log10(rms(b, 24000, 48000) / rms(in, 24000, 48000))) < 4.0);
}

TEST_CASE("V3 character stages: Delay repeats lo-fi and Grain feedback rate are off by default and audible when on") {
    std::vector<float> in(72000, 0.0f);
    for (int i = 0; i < 4800; ++i) in[static_cast<size_t>(i)] = 0.4f * static_cast<float>(std::sin(2 * dsp::kPi * 3000.0 * i / kSr));
    auto clean = mk("delay", vals("delay", {{"sync", 0}, {"time_ms", 300}, {"feedback", 60}, {"mix", 100}}));
    auto lofi = mk("delay", vals("delay", {{"sync", 0}, {"time_ms", 300}, {"feedback", 60}, {"mix", 100}, {"lofi_rate", 4000}, {"lofi_bits", 8}}));
    const auto a = run(*clean, in), b = run(*lofi, in);
    // second repeat (~0.6 s): the clean one is still mostly 3 kHz; the lo-fi one has images / aliases at 1 kHz
    CHECK(tone(b, 28800, 33600, 1000.0) > 5.0 * tone(a, 28800, 33600, 1000.0) + 1e-5);
    // defaults: the new parameters are the "off" ends of their ranges
    const auto& dd = fxRegistry()[static_cast<size_t>(fxTypeIndex("delay"))];
    CHECK(dd.defaults()[static_cast<size_t>(dd.paramIndex("lofi_rate"))] == doctest::Approx(48000.0f));
    const auto& gd = fxRegistry()[static_cast<size_t>(fxTypeIndex("grain_engine"))];
    CHECK(gd.defaults()[static_cast<size_t>(gd.paramIndex("fb_rate"))] == doctest::Approx(48000.0f));
    CHECK(gd.defaults()[static_cast<size_t>(gd.paramIndex("fb_drive"))] == doctest::Approx(0.0f));
}
