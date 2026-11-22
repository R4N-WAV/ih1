// V4 feedback round: per-instance bypass, preset macros, None as a carousel ring entry, the staged discrete-switch dip,
// the parameter glide, and a behaviour test for each of the ten new processors.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "TestUtil.h"
#include "ih1/dsp/Basics.h"
#include "ih1/dsp/Fft.h"
#include "ih1/engine/Engine.h"
#include "ih1/fx/FxProcessor.h"
#include "ih1/model/Json.h"
#include "ih1/model/Session.h"
#include "ih1/presets/Presets.h"

using namespace ih1;

namespace {
constexpr double kSr = 48000.0;
constexpr int kBlock = 256;

float tone(long n, int c) {
    const double t = n / kSr;
    return 0.25f * static_cast<float>(std::sin(2 * dsp::kPi * 220.0 * t + 0.4 * c) + 0.5 * std::sin(2 * dsp::kPi * 660.0 * t));
}

struct Rig {
    Engine engine;
    Session session{engine};
    long n = 0;
    Rig() {
        engine.setSafety(0, 0.0f);
        session.prepare(kSr, kBlock);
    }
    void run(int blocks, std::vector<float>* outL = nullptr, std::vector<float>* outR = nullptr, float (*src)(long, int) = tone) {
        std::vector<float> L(kBlock), R(kBlock);
        ProcessContext ctx;
        ctx.sampleRate = kSr;
        for (int b = 0; b < blocks; ++b) {
            session.service();
            for (int i = 0; i < kBlock; ++i) { L[static_cast<size_t>(i)] = src(n + i, 0); R[static_cast<size_t>(i)] = src(n + i, 1); }
            engine.process(L.data(), R.data(), kBlock, ctx);
            n += kBlock;
            if (outL) outL->insert(outL->end(), L.begin(), L.end());
            if (outR) outR->insert(outR->end(), R.begin(), R.end());
        }
    }
    void addFx(const char* type, std::vector<std::pair<const char*, float>> params = {}) {
        const int t = fxTypeIndex(type);
        REQUIRE(t >= 0);
        session.select(t);
        const auto& d = fxRegistry()[static_cast<size_t>(t)];
        for (auto& [id, v] : params) session.setDraftParam(d.paramIndex(id), v);
        REQUIRE(session.commit());
    }
};

// largest sample-to-sample second difference relative to what a smooth signal of this content produces
float worstStep(const std::vector<float>& x, size_t from = 0) {
    float w = 0.0f;
    for (size_t i = std::max<size_t>(from, 2); i < x.size(); ++i) w = std::max(w, std::fabs(x[i] - 2 * x[i - 1] + x[i - 2]));
    return w;
}

std::vector<float> render(const char* type, std::vector<std::pair<const char*, float>> params, int seconds, float (*src)(long, int), std::vector<float>* right = nullptr, double bpm = 120.0) {
    const int t = fxTypeIndex(type);
    REQUIRE(t >= 0);
    const auto& d = fxRegistry()[static_cast<size_t>(t)];
    ParamValues v = d.defaults();
    for (auto& [id, val] : params) {
        const int k = d.paramIndex(id);
        REQUIRE_MESSAGE(k >= 0, type << "." << id);
        v[static_cast<size_t>(k)] = val;
    }
    auto fx = d.create();
    fx->prepare(kSr, kBlock);
    fx->setParams(d.clamp(v), true);
    std::vector<float> L(static_cast<size_t>(seconds * kSr)), R(L.size());
    for (size_t i = 0; i < L.size(); ++i) { L[i] = src(static_cast<long>(i), 0); R[i] = src(static_cast<long>(i), 1); }
    test::runFx(*fx, L, R, kBlock, kSr, bpm);
    if (right) *right = R;
    return L;
}

float drums(long n, int c) {  // a hit every 250 ms: 2 ms attack, decaying 180 Hz body + noise click
    const int k = static_cast<int>(n % 12000);
    const float env = std::min(1.0f, k / 96.0f) * std::exp(-k / 2400.0f);
    return env * (0.5f * static_cast<float>(std::sin(2 * dsp::kPi * 180.0 * k / kSr)) + 0.15f * static_cast<float>(std::sin(k * 1.913 + c)));
}
float ramp(long n, int) {  // a slow rising saw ramp: forwards vs backwards is easy to tell apart (slope sign)
    return 0.4f * static_cast<float>((n % 24000) / 24000.0) - 0.2f;
}

double spectralCentroid(const std::vector<float>& x, size_t from) {
    const int N = 8192;
    dsp::Fft fft;
    fft.prepare(N);
    std::vector<dsp::cfloat> d(N);
    double num = 0, den = 0;
    for (size_t off = from; off + N <= x.size(); off += N) {
        for (int i = 0; i < N; ++i) d[static_cast<size_t>(i)] = dsp::cfloat(x[off + i] * (0.5f - 0.5f * std::cos(2 * dsp::kPi * i / N)), 0.0f);
        fft.forward(d.data());
        for (int k = 1; k < N / 2; ++k) { const double m = std::abs(d[static_cast<size_t>(k)]); num += m * k * kSr / N; den += m; }
    }
    return num / std::max(den, 1e-12);
}
}  // namespace

// =========================================================================================== workflow / engine
TEST_CASE("V4 P6: bypassing a committed instance passes its input, keeps its values, and is click-free") {
    Rig r;
    r.addFx("ring_mod", {{"freq", 900.0f}, {"mix", 100.0f}});
    const uint64_t id = r.session.chain()[0].id();
    const ParamValues before = r.session.chain()[0].values();
    r.run(40);
    std::vector<float> L;
    REQUIRE(r.session.setEnabled(id, false));
    r.run(60, &L);
    CHECK_FALSE(r.session.chain()[0].enabled());
    CHECK(r.session.chain()[0].values() == before);  // bypass never edits the frozen values
    // after the 15 ms ramp the output equals the input (ring mod at 100 % would otherwise remove the 220 Hz tone)
    double err = 0, ref = 0;
    for (size_t i = 4000; i < L.size(); ++i) {
        const long n = static_cast<long>(r.n - static_cast<long>(L.size()) + static_cast<long>(i));
        err += std::pow(L[i] - tone(n, 0), 2.0);
        ref += std::pow(tone(n, 0), 2.0);
    }
    CHECK(10.0 * std::log10(err / ref) < -60.0);
    // the transition itself: no step larger than the signal's own curvature allows (smooth 15 ms crossfade)
    CHECK(worstStep(L) < 0.05f);
    // re-enable: exactly the same processing as before
    REQUIRE(r.session.setEnabled(id, true));
    CHECK(r.session.chain()[0].enabled());
    CHECK(r.session.chain()[0].values() == before);
}

TEST_CASE("V4 P6: bypass state survives save / reload and reorder keeps the identity") {
    Rig r;
    r.addFx("chorus");
    r.addFx("delay");
    r.addFx("reverb");
    const uint64_t mid = r.session.chain()[1].id();
    r.session.setEnabled(mid, false);
    REQUIRE(r.session.move(1, 2));
    CHECK(r.session.chain()[2].id() == mid);
    CHECK_FALSE(r.session.chain()[2].enabled());
    const std::string text = r.session.toJson(false);
    Rig r2;
    REQUIRE(r2.session.fromJson(text, false));
    REQUIRE(r2.session.chain().size() == 3);
    CHECK_FALSE(r2.session.chain()[2].enabled());
    CHECK(r2.session.chain()[0].enabled());
    // a V3 document (no "enabled" fields) loads with everything enabled
    json::Value root;
    REQUIRE(json::parse(text, root));
    std::string v3 = text;
    for (size_t p; (p = v3.find("\"enabled\": false")) != std::string::npos;) v3.replace(p, 16, "\"x\": 0");
    Rig r3;
    REQUIRE(r3.session.fromJson(v3, false));
    for (const auto& ci : r3.session.chain()) CHECK(ci.enabled());
}

TEST_CASE("V4 P6: a Time & Space effect lets its tail ring out when bypassed; others switch off") {
    Rig r;
    r.addFx("delay", {{"sync", 0.0f}, {"time_ms", 120.0f}, {"feedback", 60.0f}, {"mix", 100.0f}});
    r.run(60);
    r.session.setEnabled(r.session.chain()[0].id(), false);
    // with the input now passing dry, the echoes of what was played before still decay (tail kept)
    std::vector<float> L;
    r.run(20, &L, nullptr, [](long, int) { return 0.0f; });
    CHECK(test::rms(L, 2000, 5000) > 1e-3);
}

TEST_CASE("V4 P1.5: preset macros modulate committed parameters at runtime only; 0 = the stored chain") {
    FactoryPreset p;
    p.id = "test.macro";
    p.name = "Macro";
    p.category = "Init / Utility";
    p.chain = {{"filter", {{"type", 0}, {"cutoff", 400.0f}, {"resonance", 10.0f}}}};
    p.macros = {{"Open", {{0, "cutoff", 8000.0f}}}};
    std::string err;
    REQUIRE_MESSAGE(validatePreset(p, &err), err);
    Rig r;
    REQUIRE(r.session.fromJson(presetToJson(p), false));
    REQUIRE(r.session.macros().size() == 1);
    CHECK(r.session.activeMacroCount() == 1);
    const ParamValues stored = r.session.chain()[0].values();
    std::vector<float> a, b;
    r.engine.setMacro(0, 0.0f);
    r.run(80);
    r.run(40, &a);
    r.engine.setMacro(0, 1.0f);
    r.run(80);
    r.run(40, &b);
    CHECK(r.session.chain()[0].values() == stored);       // the committed values never change
    CHECK(spectralCentroid(b, 0) > 1.15 * spectralCentroid(a, 0));  // the macro opened the filter
    // macros are saved with the chain and follow the instance id
    const std::string text = r.session.toJson(false);
    Rig r2;
    REQUIRE(r2.session.fromJson(text, false));
    CHECK(r2.session.macros().size() == 1);
    CHECK(r2.session.activeMacroCount() == 1);
}

TEST_CASE("V4 P3: None is position 0 of the carousel ring; stepping continues through it in both directions") {
    Rig r;
    const auto& order = fxCarouselOrder();
    REQUIRE(!order.empty());
    CHECK(r.session.selected() == Session::kNone);
    r.session.step(1);
    CHECK(r.session.selected() == order.front());
    r.session.step(-1);
    CHECK(r.session.selected() == Session::kNone);
    r.session.step(-1);
    CHECK(r.session.selected() == order.back());
    r.session.step(1);
    CHECK(r.session.selected() == Session::kNone);
    CHECK_FALSE(r.session.hasStaged());
    // a full lap returns to None
    for (size_t i = 0; i < order.size() + 1; ++i) r.session.step(1);
    CHECK(r.session.selected() == Session::kNone);
}

TEST_CASE("V4 P4: a discrete switch of the staged effect is crossfaded (no step in the output)") {
    Rig r;
    const int t = fxTypeIndex("delay");
    r.session.select(t);
    const auto& d = fxRegistry()[static_cast<size_t>(t)];
    r.session.setDraftParam(d.paramIndex("sync"), 0.0f);
    r.session.setDraftParam(d.paramIndex("time_ms"), 90.0f);
    r.session.setDraftParam(d.paramIndex("mix"), 100.0f);
    r.run(80);
    std::vector<float> L;
    for (int k = 0; k < 6; ++k) {
        r.session.setDraftParam(d.paramIndex("stereo"), static_cast<float>(k % 3));  // mono / stereo / ping-pong
        r.session.setDraftParam(d.paramIndex("pattern"), static_cast<float>(k % 4));
        r.run(12, &L);
    }
    for (float x : L) REQUIRE(std::isfinite(x));
    CHECK(worstStep(L) < 0.08f);
}

TEST_CASE("V4 P4: continuous parameter jumps glide (no single-sample jump in a read position)") {
    const int t = fxTypeIndex("delay");
    const auto& d = fxRegistry()[static_cast<size_t>(t)];
    auto fx = d.create();
    fx->prepare(kSr, kBlock);
    ParamValues v = d.defaults();
    v[static_cast<size_t>(d.paramIndex("mix"))] = 100.0f;
    v[static_cast<size_t>(d.paramIndex("mod_depth"))] = 0.0f;
    fx->setParams(d.clamp(v), true);
    std::vector<float> L(48128), R(48128);  // a whole number of blocks
    for (size_t i = 0; i < L.size(); ++i) L[i] = R[i] = tone(static_cast<long>(i), 0);
    ProcessContext ctx;
    for (size_t off = 0; off < L.size(); off += kBlock) {
        if (off == 24064) { v[static_cast<size_t>(d.paramIndex("mod_depth"))] = 100.0f; v[static_cast<size_t>(d.paramIndex("duck"))] = 100.0f; fx->setParams(d.clamp(v), false); }
        float* ch[2] = {L.data() + off, R.data() + off};
        fx->process(ch, kBlock, ctx);
    }
    CHECK(worstStep(L, 20000) < 0.03f);
}

// ======================================================================================= the ten new processors
TEST_CASE("V4 FX: Slice Shuffler replays the previous bar rearranged (not the live input), deterministically") {
    // 1 beat at 120 bpm = 24000 samples, 4 slices; the source is a ramp that rises across each 24000-sample cycle,
    // so "Reverse Order" makes the slices of the replayed beat descend
    std::vector<float> a = render("slice_shuffle", {{"length", 0}, {"slices", 0}, {"pattern", 0}, {"reverse", 0}, {"gate", 100}, {"mix", 100}}, 3, ramp);
    std::vector<float> b = render("slice_shuffle", {{"length", 0}, {"slices", 0}, {"pattern", 0}, {"reverse", 0}, {"gate", 100}, {"mix", 100}}, 3, ramp);
    CHECK(a == b);
    // slice means in the third beat: descending (the ramp of the second beat in reverse slice order)
    auto mean = [&](size_t from, size_t len) { double s = 0; for (size_t i = from; i < from + len; ++i) s += a[i]; return s / len; };
    const size_t base = 48000;
    CHECK(mean(base + 600, 5000) > mean(base + 6600, 5000));
    CHECK(mean(base + 6600, 5000) > mean(base + 12600, 5000));
    CHECK(mean(base + 12600, 5000) > mean(base + 18600, 5000));
}

TEST_CASE("V4 FX: Attack / Body sums back to the input at defaults and can remove or delay the attacks") {
    std::vector<float> x(static_cast<size_t>(2 * kSr));
    for (size_t i = 0; i < x.size(); ++i) x[i] = drums(static_cast<long>(i), 0);
    const std::vector<float> neutral = render("transient_split", {}, 2, drums);
    double err = 0, ref = 0;
    for (size_t i = 0; i < x.size(); ++i) { err += std::pow(neutral[i] - x[i], 2.0); ref += x[i] * x[i]; }
    CHECK(10.0 * std::log10(err / ref) < -60.0);  // the split is exact
    const std::vector<float> noAttack = render("transient_split", {{"att_gain", -60.0f}, {"length", 30.0f}}, 2, drums);
    // the first 3 ms of each hit (the attack) lose most of their energy, the decaying body stays
    double aIn = 0, aOut = 0, bIn = 0, bOut = 0;
    for (int h = 1; h < 7; ++h) {
        const size_t s = static_cast<size_t>(h) * 12000;
        for (size_t i = s; i < s + 144; ++i) { aIn += x[i] * x[i]; aOut += noAttack[i] * noAttack[i]; }
        for (size_t i = s + 4000; i < s + 9000; ++i) { bIn += x[i] * x[i]; bOut += noAttack[i] * noAttack[i]; }
    }
    CHECK(aOut < 0.3 * aIn);
    CHECK(bOut > 0.6 * bIn);
}

TEST_CASE("V4 FX: Echo Matrix taps land on the step grid and can be pitched") {
    // an impulse; Straight pattern at 1/16 (125 ms at 120 bpm): echoes at 6000, 12000, ... samples
    auto imp = [](long n, int) { return n == 100 ? 1.0f : 0.0f; };
    std::vector<float> y = render("echo_matrix", {{"pattern", 0}, {"division", 4}, {"decay", 90}, {"feedback", 0}, {"mix", 100}, {"spread", 0}, {"tone", 18000}, {"darken", 0}}, 2, imp);
    for (int t = 1; t <= 4; ++t) {
        float pk = 0.0f;
        for (size_t i = 100 + t * 6000 - 40; i < 100 + t * 6000 + 40; ++i) pk = std::max(pk, std::fabs(y[i]));
        CHECK_MESSAGE(pk > 0.05f, "tap " << t);
        float between = 0.0f;
        for (size_t i = 100 + t * 6000 + 900; i < 100 + t * 6000 + 2000; ++i) between = std::max(between, std::fabs(y[i]));
        CHECK(between < 0.1f * pk);
    }
    // climbing pitch raises the spectral centroid of the echoes
    auto sine = [](long n, int) { return n < 4800 ? 0.4f * static_cast<float>(std::sin(2 * dsp::kPi * 300.0 * n / kSr)) * std::min(1.0f, n / 480.0f) * std::min(1.0f, (4800 - n) / 480.0f) : 0.0f; };
    std::vector<float> flat = render("echo_matrix", {{"pattern", 0}, {"pitch_mode", 0}, {"mix", 100}, {"feedback", 0}}, 2, sine);
    std::vector<float> climb = render("echo_matrix", {{"pattern", 0}, {"pitch_mode", 1}, {"interval", 7}, {"mix", 100}, {"feedback", 0}}, 2, sine);
    CHECK(spectralCentroid(climb, 6000) > 1.3 * spectralCentroid(flat, 6000));
}

TEST_CASE("V4 FX: Reverse Pitch Echo returns material backwards and pitched up by the interval") {
    // a rising ramp comes back falling (reverse), and a 300 Hz tone comes back near 600 Hz (+12 st)
    std::vector<float> y = render("reverse_echo", {{"interval", 0}, {"feedback", 0}, {"mix", 100}, {"sync", 0}, {"length_ms", 200}, {"delay_ms", 10}}, 2, ramp);
    // slope of the output over the middle of a grain: negative (backwards) while the input ramp rises
    double slope = 0;
    for (size_t i = 30000; i < 60000; ++i) slope += y[i] - y[i - 1];
    CHECK(slope < 0.0);
    auto s300 = [](long n, int) { return 0.4f * static_cast<float>(std::sin(2 * dsp::kPi * 300.0 * n / kSr)); };
    std::vector<float> up = render("reverse_echo", {{"interval", 12}, {"feedback", 0}, {"mix", 100}}, 2, s300);
    const double c = spectralCentroid(up, 24000);
    CHECK(c > 500.0);
    CHECK(c < 720.0);
}

TEST_CASE("V4 FX: Dispersion is all-pass (level kept) but smears an impulse in time") {
    auto noise = [](long n, int c) { return 0.3f * static_cast<float>(std::sin(n * 12.9898 + c * 78.233) * 43758.5453 - std::floor(std::sin(n * 12.9898 + c * 78.233) * 43758.5453)) - 0.15f; };
    std::vector<float> x(static_cast<size_t>(2 * kSr));
    for (size_t i = 0; i < x.size(); ++i) x[i] = noise(static_cast<long>(i), 0);
    std::vector<float> y = render("dispersion", {{"stages", 5}, {"mix", 100}, {"feedback", 0}}, 2, noise);
    const double ri = test::rms(x, 20000), ro = test::rms(y, 20000);
    CHECK(std::fabs(20.0 * std::log10(ro / ri)) < 1.0);
    // a 2 ms Hann pulse (energy mostly below ~1 kHz, where all-pass sections can delay a lot): after dispersion its
    // energy is no longer concentrated around the pulse
    auto pulse = [](long n, int) { return (n >= 1000 && n < 1096) ? 0.8f * (0.5f - 0.5f * static_cast<float>(std::cos(2 * dsp::kPi * (n - 1000) / 96.0))) : 0.0f; };
    std::vector<float> h = render("dispersion", {{"stages", 5}, {"center", 300}, {"spread", 4}, {"q", 3}, {"mix", 100}, {"feedback", 0}}, 1, pulse);
    double near = 0, all = 0;
    for (size_t i = 0; i < h.size(); ++i) { all += h[i] * h[i]; if (i >= 1000 && i < 1192) near += h[i] * h[i]; }
    CHECK(near < 0.5 * all);
}

TEST_CASE("V4 FX: Modal Resonator rings at the material's mode ratios after a strike") {
    auto imp = [](long n, int) { return n == 100 ? 0.8f : 0.0f; };
    // A3 = 220 Hz. Harmonic: second mode at 440 Hz. Glass: second mode at 2.32 x = 510 Hz.
    std::vector<float> h = render("modal", {{"material", 0}, {"note", 9}, {"octave", 2}, {"modes", 4}, {"mix", 100}, {"decay", 1.5f}}, 1, imp);
    std::vector<float> g = render("modal", {{"material", 3}, {"note", 9}, {"octave", 2}, {"modes", 4}, {"mix", 100}, {"decay", 1.5f}}, 1, imp);
    CHECK(test::rms(h, 4800, 24000) > 2e-4);  // it rings after the impulse is gone
    auto band = [](const std::vector<float>& x, double f) {
        double re = 0, im = 0;
        for (size_t n = 2400; n < 26400; ++n) { re += x[n] * std::cos(2 * dsp::kPi * f * n / kSr); im += x[n] * std::sin(2 * dsp::kPi * f * n / kSr); }
        return std::sqrt(re * re + im * im);
    };
    CHECK(band(g, 220.0 * 2.32) > 5.0 * band(h, 220.0 * 2.32));
    CHECK(band(h, 440.0) > 5.0 * band(g, 440.0));
    for (float v : h) REQUIRE(std::isfinite(v));
}

TEST_CASE("V4 FX: Texture Imprint follows the source and is repeatable per seed") {
    std::vector<float> a = render("texture", {{"type", 0}, {"amount", 0.0f}, {"follow", 100}, {"seed", 3}}, 2, drums);
    std::vector<float> b = render("texture", {{"type", 0}, {"amount", 0.0f}, {"follow", 100}, {"seed", 3}}, 2, drums);
    std::vector<float> c = render("texture", {{"type", 0}, {"amount", 0.0f}, {"follow", 100}, {"seed", 4}}, 2, drums);
    CHECK(a == b);
    CHECK(a != c);
    std::vector<float> x(a.size());
    for (size_t i = 0; i < x.size(); ++i) x[i] = drums(static_cast<long>(i), 0);
    double d = 0;
    for (size_t i = 0; i < x.size(); ++i) d += std::pow(a[i] - x[i], 2.0);
    CHECK(d > 1e-3);  // texture was added
}

TEST_CASE("V4 FX: Spectral Shatter moves energy between bands on the step grid") {
    auto s = [](long n, int) { return 0.3f * static_cast<float>(std::sin(2 * dsp::kPi * 440.0 * n / kSr)); };
    std::vector<float> y = render("spectral_shatter", {{"reorder", 100}, {"freeze", 0}, {"gate", 0}, {"shards", 1}, {"mix", 100}}, 3, s);
    // the 440 Hz tone is relocated in at least some steps: the centroid differs from 440 Hz
    const double c = spectralCentroid(y, 4096);
    CHECK(std::fabs(c - 440.0) > 60.0);
    for (float v : y) REQUIRE(std::isfinite(v));
}

TEST_CASE("V4 FX: Partial Recomposer stretches harmonic partials (inharmonic result)") {
    auto saw = [](long n, int) {
        float v = 0.0f;
        for (int h = 1; h <= 8; ++h) v += static_cast<float>(std::sin(2 * dsp::kPi * 200.0 * h * n / kSr)) / h;
        return 0.25f * v;
    };
    std::vector<float> base = render("partial_recompose", {{"stretch", 0}, {"residual", -60}, {"mix", 100}}, 2, saw);
    std::vector<float> st = render("partial_recompose", {{"stretch", 60}, {"residual", -60}, {"mix", 100}}, 2, saw);
    CHECK(spectralCentroid(st, 8192) > 1.25 * spectralCentroid(base, 8192));
}

TEST_CASE("V4 FX: Spectral Graft imposes a held spectral shape (bright input becomes darker after a dark snapshot)") {
    // first second: a dark tone (captured as the held moment); then a bright tone
    auto src = [](long n, int) {
        const double t = n / kSr;
        return n < 48000 ? 0.3f * static_cast<float>(std::sin(2 * dsp::kPi * 200.0 * t))
                         : 0.3f * static_cast<float>(std::sin(2 * dsp::kPi * 200.0 * t) + std::sin(2 * dsp::kPi * 3000.0 * t));
    };
    std::vector<float> dry(static_cast<size_t>(3 * kSr));
    for (size_t i = 0; i < dry.size(); ++i) dry[i] = src(static_cast<long>(i), 0);
    std::vector<float> y = render("spectral_graft", {{"source", 0}, {"amount", 100}, {"keep_level", 0}, {"mix", 100}}, 3, src);
    CHECK(spectralCentroid(y, 72000) < 0.7 * spectralCentroid(dry, 72000));
}

TEST_CASE("V4 P1.5 / V5 P8: every factory preset's macros (authored or combination) stay safe at their extremes") {
    // V5: a preset has macros only when its author wrote them or its effect combination is eligible (comboMacros)
    int withMacros = 0;
    for (const auto& p : factoryPresets()) {
        if (p.id == kInitPresetId) { CHECK(p.macros.empty()); continue; }
        CHECK_MESSAGE(p.macros.size() <= 4, p.id);
        Rig r;
        REQUIRE(r.session.fromJson(presetToJson(p), false));
        if (r.session.activeMacroCount() == 0) continue;
        ++withMacros;
        for (int m = 0; m < kNumPresetMacros; ++m) r.engine.setMacro(m, 1.0f);
        std::vector<float> L, R;
        r.run(90, &L, &R, drums);  // ~0.5 s
        float pk = 0.0f;
        for (size_t i = 0; i < L.size(); ++i) {
            REQUIRE(std::isfinite(L[i]));
            REQUIRE(std::isfinite(R[i]));
            pk = std::max(pk, std::max(std::fabs(L[i]), std::fabs(R[i])));
        }
        CHECK_MESSAGE(pk < 4.0f, p.id << " peak " << pk);
        CHECK_MESSAGE(test::rms(L, L.size() / 2) < 1.0, p.id);
    }
    MESSAGE(withMacros << " presets with macros");
    CHECK(withMacros >= 40);
}
