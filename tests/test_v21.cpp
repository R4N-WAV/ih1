// V2.1 regression tests: Grain Engine behaviour, latency modes (registry, reported == measured), tempo-sync
// control dependencies, Clear FX Chain semantics.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/engine/Engine.h"
#include "ih1/fx/FxProcessor.h"
#include "ih1/model/Json.h"
#include "ih1/model/Session.h"

using namespace ih1;

namespace {

constexpr double kSr = 48000.0;
constexpr int kBlock = 256;

int typeOf(const char* id) {
    const int t = fxTypeIndex(id);
    REQUIRE(t >= 0);
    return t;
}

const FxDescriptor& desc(const char* id) { return fxRegistry()[static_cast<size_t>(typeOf(id))]; }

ParamValues valuesFor(const char* id, std::initializer_list<std::pair<const char*, float>> set) {
    const auto& d = desc(id);
    ParamValues v = d.defaults();
    for (const auto& kv : set) {
        const int k = d.paramIndex(kv.first);
        REQUIRE_MESSAGE(k >= 0, kv.first);
        v[static_cast<size_t>(k)] = kv.second;
    }
    return d.clamp(v);
}

std::unique_ptr<FxProcessor> make(const char* id, const ParamValues& v, LatencyMode m = LatencyMode::Quality, double sr = kSr) {
    const auto& d = desc(id);
    auto p = d.create();
    p->setLatencyMode(d.resolveLatency(m));
    p->prepare(sr, kBlock);
    p->setParams(v, true);
    return p;
}

// Render `in` (mono, duplicated to both channels) through fx; returns left output.
std::vector<float> render(FxProcessor& fx, const std::vector<float>& in, std::vector<float>* right = nullptr, double bpm = 120.0) {
    std::vector<float> L(in), R(in);
    ProcessContext ctx;
    ctx.sampleRate = kSr;
    ctx.bpm = bpm;
    for (size_t off = 0; off < in.size(); off += kBlock) {
        const int n = static_cast<int>(std::min<size_t>(kBlock, in.size() - off));
        float* ch[2] = {L.data() + off, R.data() + off};
        fx.process(ch, n, ctx);
    }
    if (right) *right = R;
    return L;
}

std::vector<float> sine(double f, double seconds, float amp = 0.5f) {
    std::vector<float> v(static_cast<size_t>(seconds * kSr));
    for (size_t i = 0; i < v.size(); ++i) v[i] = amp * static_cast<float>(std::sin(2.0 * M_PI * f * static_cast<double>(i) / kSr));
    return v;
}

double rms(const std::vector<float>& x, size_t a, size_t b) {
    double s = 0;
    b = std::min(b, x.size());
    for (size_t i = a; i < b; ++i) s += static_cast<double>(x[i]) * x[i];
    return std::sqrt(s / std::max<size_t>(1, b - a));
}

// Goertzel magnitude (normalised) of x[a..b) at frequency f
double tone(const std::vector<float>& x, size_t a, size_t b, double f) {
    const double w = 2.0 * M_PI * f / kSr, c = 2.0 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (size_t i = a; i < b; ++i) {
        const double hann = 0.5 - 0.5 * std::cos(2.0 * M_PI * static_cast<double>(i - a) / static_cast<double>(b - a));
        const double s0 = x[i] * hann + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / static_cast<double>(b - a);
}

int peakIndex(const std::vector<float>& x) {
    int k = 0;
    for (size_t i = 0; i < x.size(); ++i) if (std::fabs(x[i]) > std::fabs(x[static_cast<size_t>(k)])) k = static_cast<int>(i);
    return k;
}

}  // namespace

// =============================================================================================== Grain Engine
TEST_CASE("grain engine: level stays steady across size and density (normalised overlap-add)") {
    const auto in = sine(330.0, 4.0, 0.3f);
    const double inRms = rms(in, 0, in.size());
    for (float size : {15.0f, 60.0f, 250.0f, 800.0f}) {
        for (float dens : {4.0f, 25.0f, 120.0f}) {
            auto fx = make("grain_engine", valuesFor("grain_engine", {{"size_ms", size}, {"density", dens}, {"mix", 100.0f}, {"spray_ms", 30.0f}, {"position_ms", 100.0f}}));
            const auto out = render(*fx, in);
            const double r = rms(out, in.size() / 2, in.size());
            const double ratioDb = 20.0 * std::log10(std::max(r, 1e-9) / inRms);
            INFO("size " << size << " ms, density " << dens << "/s: " << ratioDb << " dB");
            const double overlap = size * 0.001 * dens;
            if (overlap >= 1.0) CHECK(std::fabs(ratioDb) < 6.0);  // continuous texture: within +-6 dB of the input
            else CHECK(ratioDb < 3.0);                             // sparse grains are never boosted
        }
    }
}

TEST_CASE("grain engine: Stretch keeps pitch, Varispeed follows speed") {
    const auto in = sine(440.0, 5.0, 0.4f);
    auto stretch = make("grain_engine", valuesFor("grain_engine", {{"mode", 1.0f}, {"speed", 0.5f}, {"size_ms", 80.0f}, {"density", 40.0f}, {"spray_ms", 0.0f}, {"mix", 100.0f}, {"align", 1.0f}}));
    const auto a = render(*stretch, in);
    const size_t s0 = static_cast<size_t>(2.0 * kSr), s1 = static_cast<size_t>(4.5 * kSr);
    const double at440 = tone(a, s0, s1, 440.0), at220 = tone(a, s0, s1, 220.0), at400 = tone(a, s0, s1, 400.0);
    INFO("stretch: 440 " << at440 << " 220 " << at220 << " 400 " << at400);
    CHECK(at440 > 8.0 * at220);
    CHECK(at440 > 4.0 * at400);  // no pitch slip (KADAL CLM-EA2FBZCRGN)

    auto vari = make("grain_engine", valuesFor("grain_engine", {{"mode", 1.0f}, {"speed", 0.5f}, {"varispeed", 1.0f}, {"size_ms", 80.0f}, {"density", 40.0f}, {"spray_ms", 0.0f}, {"mix", 100.0f}}));
    const auto b = render(*vari, in);
    INFO("varispeed: 220 " << tone(b, s0, s1, 220.0) << " 440 " << tone(b, s0, s1, 440.0));
    CHECK(tone(b, s0, s1, 220.0) > 4.0 * tone(b, s0, s1, 440.0));
}

TEST_CASE("grain engine: Freeze holds the captured moment after the input stops") {
    auto in = sine(300.0, 4.0, 0.4f);
    std::fill(in.begin() + static_cast<long>(1.5 * kSr), in.end(), 0.0f);  // silence after 1.5 s
    auto fx = make("grain_engine", valuesFor("grain_engine", {{"mix", 100.0f}, {"position_ms", 300.0f}, {"spray_ms", 200.0f}}));
    // freeze is engaged by parameter after 1 s of material
    std::vector<float> L(in), R(in);
    ProcessContext ctx;
    for (size_t off = 0; off < in.size(); off += kBlock) {
        if (off == static_cast<size_t>(1.0 * kSr / kBlock) * kBlock)
            fx->setParams(valuesFor("grain_engine", {{"mix", 100.0f}, {"position_ms", 300.0f}, {"spray_ms", 200.0f}, {"freeze", 1.0f}}), false);
        float* ch[2] = {L.data() + off, R.data() + off};
        fx->process(ch, static_cast<int>(std::min<size_t>(kBlock, in.size() - off)), ctx);
    }
    CHECK(rms(L, static_cast<size_t>(3.0 * kSr), static_cast<size_t>(4.0 * kSr)) > 0.05);  // still sounding
    CHECK(tone(L, static_cast<size_t>(3.0 * kSr), static_cast<size_t>(4.0 * kSr), 300.0) > 0.01);
}

TEST_CASE("grain engine: scale quantisation keeps grain pitches on the chosen intervals") {
    const auto in = sine(440.0, 4.0, 0.4f);
    auto fx = make("grain_engine", valuesFor("grain_engine", {{"pitch_jit", 12.0f}, {"scale", 1.0f /*Octaves*/}, {"mix", 100.0f}, {"density", 40.0f}, {"size_ms", 120.0f}}));
    const auto y = render(*fx, in);
    const size_t a = static_cast<size_t>(1.5 * kSr), b = in.size();
    const double oct = tone(y, a, b, 440.0) + tone(y, a, b, 880.0) + tone(y, a, b, 220.0);
    const double off = tone(y, a, b, 440.0 * std::pow(2.0, 7.0 / 12.0)) + tone(y, a, b, 440.0 * std::pow(2.0, 4.0 / 12.0));
    INFO("octave energy " << oct << ", off-scale " << off);
    CHECK(oct > 10.0 * off);
}

TEST_CASE("grain engine: maximum settings stay bounded and finite (stress)") {
    std::vector<float> in(static_cast<size_t>(20.0 * kSr));
    dsp::Rng r(3);
    for (auto& x : in) x = 0.9f * r.bipolar();
    for (int mode = 0; mode < 3; ++mode) {
        auto fx = make("grain_engine", valuesFor("grain_engine", {{"mode", static_cast<float>(mode)}, {"voices", 64.0f}, {"density", 200.0f}, {"size_ms", 1000.0f},
                                                                  {"feedback", 100.0f}, {"delay_ms", 0.0f}, {"speed", -2.0f}, {"retrig", 1.0f}, {"sync", 0.0f}, {"retrig_ms", 20.0f},
                                                                  {"pitch_jit", 12.0f}, {"reverse", 50.0f}, {"stereo", 1.0f}, {"gain", 12.0f}, {"mix", 100.0f}}));
        const auto y = render(*fx, in);
        float peak = 0.0f;
        for (float v : y) { REQUIRE(std::isfinite(v)); peak = std::max(peak, std::fabs(v)); }
        INFO("mode " << mode << " peak " << peak);
        CHECK(peak < 16.0f);  // bounded (the engine's +12 dB safety clamp holds anything beyond)
        // and the feedback loop decays when the input stops
        std::vector<float> silent(static_cast<size_t>(12.0 * kSr), 0.0f);
        fx->setParams(valuesFor("grain_engine", {{"mode", 0.0f}, {"feedback", 100.0f}, {"mix", 100.0f}}), false);
        const auto t = render(*fx, silent);
        CHECK(rms(t, t.size() - static_cast<size_t>(kSr), t.size()) < 0.05);
    }
}

TEST_CASE("grain engine: rapid automation of every parameter is click-free enough and finite") {
    const auto in = sine(220.0, 6.0, 0.4f);
    const auto& d = desc("grain_engine");
    auto fx = make("grain_engine", d.defaults());
    dsp::Rng r(11);
    std::vector<float> L(in), R(in);
    ProcessContext ctx;
    for (size_t off = 0; off < in.size(); off += 64) {
        ParamValues v = d.defaults();
        for (size_t i = 0; i < d.params.size(); ++i) v[i] = d.params[i].fromNormalised(r.uniform());
        fx->setParams(d.clamp(v), false);
        float* ch[2] = {L.data() + off, R.data() + off};
        fx->process(ch, static_cast<int>(std::min<size_t>(64, in.size() - off)), ctx);
    }
    for (float v : L) REQUIRE(std::isfinite(v));
}

// =============================================================================================== latency
TEST_CASE("latency registry: every production FX resolves Zero / Low honestly") {
    for (size_t t = 0; t < fxRegistry().size(); ++t) {
        const auto& d = fxRegistry()[t];
        if (d.experimental) continue;
        INFO(d.typeId);
        CHECK(d.category != FxCategory::Count);
        for (auto m : {LatencyMode::Quality, LatencyMode::Low, LatencyMode::Zero}) {
            auto p = d.create();
            const auto resolved = d.resolveLatency(m);
            p->setLatencyMode(resolved);
            p->prepare(kSr, kBlock);
            p->setParams(d.defaults(), true);
            if (resolved == LatencyMode::Zero) CHECK(p->latencySamples() == 0);
            if (m == LatencyMode::Zero && !d.latency.zero) {
                CHECK(resolved != LatencyMode::Zero);
                CHECK(p->latencySamples() > 0);  // the UI names this FX as the reason zero latency is unavailable
            }
            if (m == LatencyMode::Low && d.latency.analysisWindow) CHECK(p->latencySamples() == 512);
            if (m == LatencyMode::Low && d.latency.oversampling) CHECK(p->latencySamples() == 5);
        }
    }
}

TEST_CASE("zero latency: impulse through oversampled nonlinear FX is sample-aligned") {
    for (const char* id : {"saturation", "tape_colour", "amp"}) {
        std::vector<float> imp(4096, 0.0f);
        imp[200] = 0.05f;  // small: stays in the near-linear region
        auto q = make(id, valuesFor(id, {{"mix", 100.0f}}), LatencyMode::Quality);
        auto z = make(id, valuesFor(id, {{"mix", 100.0f}}), LatencyMode::Zero);
        const auto yq = render(*q, imp), yz = render(*z, imp);
        const int pq = peakIndex(yq) - 200, pz = peakIndex(yz) - 200;
        // onset = first sample above 1 % of the peak: when the effect starts to respond at all
        int onset = -1;
        const float thr = 0.01f * std::fabs(yz[static_cast<size_t>(pz + 200)]);
        for (size_t i = 0; i < yz.size(); ++i) if (std::fabs(yz[i]) > thr) { onset = static_cast<int>(i) - 200; break; }
        INFO(std::string(id) << ": quality peak +" << pq << " (reported " << q->latencySamples() << "), zero peak +" << pz << ", zero onset +" << onset);
        CHECK(z->latencySamples() == 0);
        // the two variants differ by exactly the reported Quality latency (the filters' own shape, e.g. a
        // cabinet's low-pass, is the same in both and is part of the sound, not latency)
        CHECK(std::abs((pq - pz) - q->latencySamples()) <= 1);
        CHECK(onset >= 0);
        CHECK(onset <= 1);  // ADAA-1 is a half-sample average; nothing is buffered
    }
}

TEST_CASE("oversampled nonlinear FX: reported latency == measured broadband delay in all three variants; Low keeps Full anti-aliasing") {
    for (const char* id : {"saturation", "tape_colour"}) {
        for (auto m : {LatencyMode::Quality, LatencyMode::Low, LatencyMode::Zero}) {
            auto p = make(id, valuesFor(id, {{"drive", 0.0f}, {"mix", 100.0f}}), m);
            std::vector<float> x(48000);
            dsp::Rng r(21);
            for (auto& v : x) v = 0.05f * r.bipolar();
            const auto y = render(*p, x);
            int best = 0;
            double bv = -1e9;
            for (int lag = 0; lag < 40; ++lag) {
                double s2 = 0;
                for (size_t i = 100; i + static_cast<size_t>(lag) < x.size(); ++i) s2 += static_cast<double>(x[i]) * y[i + static_cast<size_t>(lag)];
                if (s2 > bv) { bv = s2; best = lag; }
            }
            INFO(std::string(id) << " mode " << static_cast<int>(m) << ": reported " << p->latencySamples() << ", measured " << best);
            // Zero: nothing is buffered; ADAA's two-point average is half a sample and the effect's own
            // minimum-phase tone shaping can move the broadband peak by one sample
            if (m == LatencyMode::Zero) CHECK(std::abs(best - p->latencySamples()) <= 1);
            else CHECK(best == p->latencySamples());
        }
    }
}

TEST_CASE("zero latency convolution == full-quality convolution, advanced by its partition") {
    std::vector<float> in(48000, 0.0f);
    dsp::Rng r(5);
    for (size_t i = 0; i < 20000; ++i) in[i] = 0.3f * r.bipolar();
    const auto v = valuesFor("convolution", {{"mix", 100.0f}, {"predelay", 0.0f}});
    auto q = make("convolution", v, LatencyMode::Quality);
    auto z = make("convolution", v, LatencyMode::Zero);
    const int P = q->latencySamples();
    REQUIRE(P > 0);
    CHECK(z->latencySamples() == 0);
    const auto yq = render(*q, in), yz = render(*z, in);
    double err = 0, ref = 0;
    for (size_t i = 0; i + static_cast<size_t>(P) < yq.size(); ++i) {
        const double d = yz[i] - yq[i + static_cast<size_t>(P)];
        err += d * d;
        ref += static_cast<double>(yq[i + static_cast<size_t>(P)]) * yq[i + static_cast<size_t>(P)];
    }
    INFO("error " << 10.0 * std::log10(err / ref) << " dB");
    CHECK(10.0 * std::log10(err / std::max(ref, 1e-12)) < -60.0);
}

TEST_CASE("session latency mode: reported chain latency, contributors and state") {
    Engine engine;
    Session s(engine);
    s.prepare(kSr, kBlock);
    for (const char* id : {"eq", "saturation", "convolution", "delay"}) { s.select(typeOf(id)); s.commit(); }
    CHECK(s.latencySamples() > 0);  // Full Quality: oversampler + convolution partition
    s.setLatencyMode(LatencyMode::Zero);
    CHECK(s.latencySamples() == 0);
    CHECK(s.latencyContributors().empty());
    s.select(typeOf("harmony"));
    s.commit();
    CHECK(s.latencySamples() == 512);  // Harmony has no zero variant: it runs Low and is named
    const auto c = s.latencyContributors();
    REQUIRE(c.size() == 1);
    CHECK(fxRegistry()[static_cast<size_t>(c[0].typeIndex)].typeId == "harmony");
    CHECK(c[0].resolved == LatencyMode::Low);
    // the engine runs exactly what the session reports
    for (int i = 0; i < 20; ++i) { std::vector<float> L(kBlock), R(kBlock); ProcessContext ctx; engine.process(L.data(), R.data(), kBlock, ctx); s.service(); }
    CHECK(engine.activeLatency() == 512);
    // DAW state keeps the mode; older states (no field) load as Full Quality
    const auto json = s.toJson(true);
    Engine e2;
    Session s2(e2);
    s2.prepare(kSr, kBlock);
    REQUIRE(s2.fromJson(json, true));
    CHECK(s2.latencyMode() == LatencyMode::Zero);
    CHECK(s2.latencySamples() == 512);
    Engine e3;
    Session s3(e3);
    s3.prepare(kSr, kBlock);
    REQUIRE(s3.fromJson(R"({"format":"ih1-state","version":1,"chain":[{"type":"saturation"}],"session":{}})", true));
    CHECK(s3.latencyMode() == LatencyMode::Quality);
    CHECK(s3.latencySamples() > 0);
}

TEST_CASE("zero latency chain: engine output of an impulse is aligned with the input") {
    Engine engine;
    Session s(engine);
    s.prepare(kSr, kBlock);
    s.setLatencyMode(LatencyMode::Zero);
    for (const char* id : {"eq", "saturation", "compressor", "tape_colour"}) { s.select(typeOf(id)); s.commit(); }
    REQUIRE(s.latencySamples() == 0);
    // let the program install
    for (int i = 0; i < 40; ++i) { std::vector<float> L(kBlock), R(kBlock); ProcessContext ctx; engine.process(L.data(), R.data(), kBlock, ctx); s.service(); }
    std::vector<float> L(kBlock * 8, 0.0f), R(kBlock * 8, 0.0f);
    L[300] = R[300] = 0.05f;
    ProcessContext ctx;
    for (int b = 0; b < 8; ++b) engine.process(L.data() + b * kBlock, R.data() + b * kBlock, kBlock, ctx);
    const int pk = peakIndex(L) - 300;
    INFO("measured offset " << pk << ", reported " << engine.activeLatency());
    CHECK(engine.activeLatency() == 0);
    CHECK(pk >= 0);
    CHECK(pk <= 2);
}

// =============================================================================================== tempo sync
TEST_CASE("tempo sync: every FX with Sync greys out its free time control, data-driven") {
    int linked = 0;
    for (const auto& d : fxRegistry()) {
        const int sync = d.paramIndex("sync");
        if (sync < 0) continue;
        INFO(d.typeId);
        int deps = 0;
        for (size_t i = 0; i < d.params.size(); ++i) {
            if (d.params[i].enabledBy != "sync") continue;
            ++deps;
            ParamValues v = d.defaults();
            v[static_cast<size_t>(sync)] = 1.0f;
            CHECK_FALSE(d.paramEnabled(static_cast<int>(i), v));
            v[static_cast<size_t>(sync)] = 0.0f;
            CHECK(d.paramEnabled(static_cast<int>(i), v));
            CHECK_FALSE(d.params[i].disabledReason.empty());
        }
        CHECK(deps == 1);
        ++linked;
        // the division selector itself always stays active
        const int div = d.paramIndex("division") >= 0 ? d.paramIndex("division") : d.paramIndex("retrig_div");
        if (div >= 0) {
            ParamValues v = d.defaults();
            v[static_cast<size_t>(sync)] = 1.0f;
            CHECK(d.paramEnabled(div, v));
        }
    }
    CHECK(linked >= 8);
}

TEST_CASE("tempo sync: the dormant free time has no effect while Sync is on, and is kept") {
    std::vector<float> in(static_cast<size_t>(1.5 * kSr), 0.0f);
    in[100] = 0.8f;
    auto a = make("delay", valuesFor("delay", {{"sync", 1.0f}, {"time_ms", 120.0f}, {"mix", 100.0f}}));
    auto b = make("delay", valuesFor("delay", {{"sync", 1.0f}, {"time_ms", 900.0f}, {"mix", 100.0f}}));
    const auto ya = render(*a, in), yb = render(*b, in);
    double diff = 0;
    for (size_t i = 0; i < ya.size(); ++i) diff = std::max(diff, static_cast<double>(std::fabs(ya[i] - yb[i])));
    CHECK(diff < 1e-6);
    // the value is preserved in state (not overwritten by toggling Sync)
    Engine e;
    Session s(e);
    s.prepare(kSr, kBlock);
    s.select(typeOf("delay"));
    const int ti = desc("delay").paramIndex("time_ms"), si = desc("delay").paramIndex("sync");
    s.setDraftParam(ti, 777.0f);
    s.setDraftParam(si, 1.0f);
    s.setDraftParam(si, 0.0f);
    CHECK(s.draft(typeOf("delay"))[static_cast<size_t>(ti)] == doctest::Approx(777.0f));
}

// =============================================================================================== Clear FX Chain
TEST_CASE("clear chain: removes every committed FX (incl. duplicates), keeps the staged FX, serialises empty") {
    Engine engine;
    Session s(engine);
    s.prepare(kSr, kBlock);
    for (const char* id : {"delay", "delay", "reverb", "chorus", "delay"}) { s.select(typeOf(id)); s.commit(); }
    s.move(0, 3);
    s.select(typeOf("granular"));
    s.setDraftParam(0, 200.0f);
    REQUIRE(s.chain().size() == 5);
    std::vector<float> L(kBlock), R(kBlock);
    for (int i = 0; i < 10; ++i) { for (auto& x : L) x = 0.2f; R = L; ProcessContext ctx; engine.process(L.data(), R.data(), kBlock, ctx); s.service(); }
    s.clearChain();
    CHECK(s.chain().empty());
    CHECK(s.selected() == typeOf("granular"));
    CHECK(s.draft(typeOf("granular"))[0] == doctest::Approx(200.0f));
    for (int i = 0; i < 20; ++i) { for (auto& x : L) x = 0.2f; R = L; ProcessContext ctx; engine.process(L.data(), R.data(), kBlock, ctx); s.service(); }
    CHECK(engine.telemetry().chainSize.load() == 0);
    json::Value root;
    std::string err;
    REQUIRE(json::parse(s.toJson(true), root, &err));
    CHECK(root.get("chain").items().empty());
    CHECK(root.get("session").get("selected").asString() == "granular");
    // a preset can load straight after a clear
    REQUIRE(s.fromJson(R"({"format":"ih1-state","version":1,"chain":[{"type":"reverb"},{"type":"delay"}]})", false));
    CHECK(s.chain().size() == 2);
    CHECK(s.selected() == Session::kNone);  // V3 Phase 1: a preset recall lands on None ...
    CHECK(s.draft(typeOf("granular"))[0] == doctest::Approx(200.0f));  // ... and keeps the draft cached
}

// =============================================================================================== DSP quality
TEST_CASE("delay diffusion: off changes nothing; on turns discrete repeats into a denser bloom; bounded at max feedback") {
    std::vector<float> in(static_cast<size_t>(3.0 * kSr), 0.0f);
    in[1000] = 0.8f;
    auto base = make("delay", valuesFor("delay", {{"sync", 0.0f}, {"time_ms", 250.0f}, {"feedback", 60.0f}, {"mix", 100.0f}, {"mod_depth", 0.0f}}));
    auto off = make("delay", valuesFor("delay", {{"sync", 0.0f}, {"time_ms", 250.0f}, {"feedback", 60.0f}, {"mix", 100.0f}, {"mod_depth", 0.0f}, {"diffusion", 0.0f}}));
    auto on = make("delay", valuesFor("delay", {{"sync", 0.0f}, {"time_ms", 250.0f}, {"feedback", 60.0f}, {"mix", 100.0f}, {"mod_depth", 0.0f}, {"diffusion", 100.0f}}));
    const auto yb = render(*base, in), y0 = render(*off, in), y1 = render(*on, in);
    double d = 0;
    for (size_t i = 0; i < yb.size(); ++i) d = std::max(d, static_cast<double>(std::fabs(yb[i] - y0[i])));
    CHECK(d == 0.0);  // diffusion 0 = the previous sound, bit for bit
    // echo density in the tail: fraction of samples above -60 dBFS between 0.6 s and 2 s
    auto density = [](const std::vector<float>& y) {
        int n = 0, k = 0;
        for (size_t i = static_cast<size_t>(0.6 * kSr); i < static_cast<size_t>(2.0 * kSr); ++i, ++k) n += std::fabs(y[i]) > 1e-3f;
        return static_cast<double>(n) / k;
    };
    INFO("density off " << density(y0) << ", on " << density(y1));
    CHECK(density(y1) > 4.0 * density(y0) + 0.05);
    auto maxFb = make("delay", valuesFor("delay", {{"sync", 0.0f}, {"time_ms", 40.0f}, {"feedback", 100.0f}, {"diffusion", 100.0f}, {"drive", 100.0f}, {"mix", 100.0f}}));
    std::vector<float> noise(static_cast<size_t>(10.0 * kSr));
    dsp::Rng r(4);
    for (auto& v : noise) v = 0.9f * r.bipolar();
    const auto yn = render(*maxFb, noise);
    float pk = 0.0f;
    for (float v : yn) { REQUIRE(std::isfinite(v)); pk = std::max(pk, std::fabs(v)); }
    CHECK(pk < 6.0f);
}
