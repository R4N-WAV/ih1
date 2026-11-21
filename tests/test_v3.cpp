// V3 feedback round: preset recall -> None, Init, Output Safety, de-click of fresh processors, landing level
// spikes (Saturation, Transient Shaper), grain seam guard.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/dsp/OutputSafety.h"
#include "ih1/engine/Engine.h"
#include "ih1/fx/FxProcessor.h"
#include "ih1/model/Session.h"
#include "ih1/presets/Presets.h"

using namespace ih1;

namespace {
constexpr double kSr = 48000.0;
constexpr int kBlock = 256;

float sig(int n, int c) {
    const double t = n / kSr;
    const float chord = 0.18f * static_cast<float>(std::sin(2 * dsp::kPi * 220.0 * t) + 0.6 * std::sin(2 * dsp::kPi * 331.0 * t + c));
    const int k = n % 12000;  // a 12.5 ms noise-like burst every 250 ms, with a 2 ms attack (no step of its own)
    const float burst = k < 600 ? 0.3f * static_cast<float>(std::sin(n * 0.71 + c * 1.3)) * std::min(1.0f, k / 96.0f) * (1.0f - static_cast<float>(k) / 600.0f) : 0.0f;
    return chord + burst;
}

struct Rig {
    Engine engine;
    Session session{engine};
    long n = 0;
    explicit Rig(int safety = 0) {
        engine.setSafety(safety, 0.0f);
        session.prepare(kSr, kBlock);
    }
    // run `blocks` blocks, appending the left channel (and right) to out
    void run(int blocks, std::vector<float>* outL = nullptr, std::vector<float>* outR = nullptr) {
        std::vector<float> L(kBlock), R(kBlock);
        ProcessContext ctx;
        ctx.sampleRate = kSr;
        for (int b = 0; b < blocks; ++b) {
            session.service();
            for (int i = 0; i < kBlock; ++i) { L[static_cast<size_t>(i)] = sig(static_cast<int>(n + i), 0); R[static_cast<size_t>(i)] = sig(static_cast<int>(n + i), 1); }
            engine.process(L.data(), R.data(), kBlock, ctx);
            n += kBlock;
            if (outL) outL->insert(outL->end(), L.begin(), L.end());
            if (outR) outR->insert(outR->end(), R.begin(), R.end());
        }
    }
};
}  // namespace

TEST_CASE("V3 P1: a preset recall is heard exactly as the preset chain alone, whatever was staged (null test, every FX)") {
    const auto& presets = factoryPresets();
    const int pi = factoryPresetIndex("gm.particle_drift");
    REQUIRE(pi >= 0);
    const auto& p = presets[static_cast<size_t>(pi)];
    const std::string text = presetToJson(p);
    int tested = 0;
    for (int t : fxCarouselOrder()) {
        // A: something committed, FX t staged with a modified draft, then the preset is selected during playback
        Rig a, b;
        a.session.select(fxTypeIndex("chorus"));
        a.session.commit();
        b.session.select(fxTypeIndex("chorus"));
        b.session.commit();
        a.session.select(t);
        if (!fxRegistry()[static_cast<size_t>(t)].params.empty()) a.session.setDraftParam(0, fxRegistry()[static_cast<size_t>(t)].params[0].maxValue);
        a.run(40);
        b.run(40);
        REQUIRE(a.session.fromJson(text, false));
        REQUIRE(b.session.fromJson(text, false));
        CHECK(a.session.selected() == Session::kNone);
        CHECK_FALSE(a.session.hasStaged());
        std::vector<float> ya, yb;
        a.run(200, &ya);
        b.run(200, &yb);
        // after the 10 ms program hand-over both engines run the same freshly built chain on the same input
        double err = 0.0, ref = 1e-12;
        for (size_t i = 2400; i < ya.size(); ++i) { err = std::max(err, static_cast<double>(std::fabs(ya[i] - yb[i]))); ref = std::max(ref, static_cast<double>(std::fabs(yb[i]))); }
        CHECK_MESSAGE(err < 1e-5 * std::max(1.0, ref), fxRegistry()[static_cast<size_t>(t)].typeId << " still audible after preset recall: " << err);
        // the draft stays cached for when the user returns to the FX
        if (!fxRegistry()[static_cast<size_t>(t)].params.empty())
            CHECK(a.session.draft(t)[0] == doctest::Approx(fxRegistry()[static_cast<size_t>(t)].params[0].maxValue));
        ++tested;
    }
    CHECK(tested >= 38);
}

TEST_CASE("V3 P1: rapid preset stepping while something is staged never leaves the stage audible and stays finite") {
    Rig r;
    r.session.select(fxTypeIndex("saturation"));
    r.session.setDraftParam(1, 30.0f);
    std::vector<float> y;
    const auto& presets = factoryPresets();
    for (int k = 0; k < 40; ++k) {
        if (k % 3 == 0) r.session.select(fxCarouselOrder()[static_cast<size_t>(k % fxCarouselOrder().size())]);
        REQUIRE(r.session.fromJson(presetToJson(presets[static_cast<size_t>((k * 7) % presets.size())]), false));
        CHECK(r.session.selected() == Session::kNone);
        r.run(3, &y);
    }
    r.run(100, &y);
    for (float v : y) REQUIRE(std::isfinite(v));
    CHECK(r.engine.telemetry().faults.load() == 0u);
}

TEST_CASE("V3 P7: Init is the explicit clean state") {
    Rig r;
    r.session.select(fxTypeIndex("delay"));
    r.session.setDraftParam(1, 900.0f);
    r.session.commit();
    r.session.select(fxTypeIndex("grain_engine"));
    r.session.setSolo(true);
    r.run(20);
    r.session.initialise();
    CHECK(r.session.chain().empty());
    CHECK(r.session.selected() == Session::kNone);
    CHECK_FALSE(r.session.solo());
    for (size_t t = 0; t < fxRegistry().size(); ++t) CHECK(r.session.draft(static_cast<int>(t)) == fxRegistry()[t].defaults());
    std::vector<float> y;
    r.run(100, &y);
    CHECK(r.engine.telemetry().chainSize.load() == 0);
    // empty chain: output == input (after the 10 ms hand-over)
    double err = 0.0;
    for (size_t i = 4000; i < y.size(); ++i) err = std::max(err, static_cast<double>(std::fabs(y[i] - sig(static_cast<int>(r.n - static_cast<long>(y.size()) + static_cast<long>(i)), 0))));
    CHECK(err < 1e-5);
    // Init is pinned first in the factory list, and validates with its empty chain
    CHECK(factoryPresets().front().id == kInitPresetId);
    CHECK(validatePreset(factoryPresets().front()));
}

TEST_CASE("V3 P8: landing on a fresh delay-type FX starts without a step (fresh-input fade)") {
    for (const char* id : {"delay", "doubler", "pitch_correct", "chorus", "convolution", "spectral", "harmony"}) {
        Rig r;
        std::vector<float> y;
        r.run(60, &y);  // 1.5 bursts of programme before landing
        r.session.select(fxTypeIndex(id));
        r.run(120, &y);
        auto maxStep = [&](size_t a, size_t b) {
            float m = 0.0f;
            for (size_t i = a + 1; i < b; ++i) m = std::max(m, std::fabs(y[i] - y[i - 1]));
            return m;
        };
        // the programme's own largest step (bursts) before landing vs around the landing
        const float before = maxStep(1000, 60 * kBlock);
        const float around = maxStep(60 * kBlock, 60 * kBlock + 9600);
        CHECK_MESSAGE(around <= 1.6f * before + 1e-3f, id << ": step " << around << " vs programme " << before);
    }
}

TEST_CASE("V3 P8: Saturation and the Transient Shaper no longer jump in level when staged") {
    auto overshoot = [](const char* id, std::vector<std::pair<int, float>> draft) {
        Rig r;
        const int t = fxTypeIndex(id);
        r.session.select(t);
        for (auto [k, v] : draft) r.session.setDraftParam(k, v);
        r.session.select(Session::kNone);
        std::vector<float> y;
        r.run(190, &y);
        const size_t at = y.size();
        r.session.select(t);
        r.run(560, &y);
        auto loudest = [&](size_t a, size_t b) {
            float m = 0.0f;
            for (size_t s = a; s + 240 <= b; s += 120) {
                double e = 0;
                for (size_t i = s; i < s + 240; ++i) e += y[i] * y[i];
                m = std::max(m, static_cast<float>(std::sqrt(e / 240)));
            }
            return m;
        };
        const float before = loudest(at - 36000, at), first = loudest(at, at + 24000), steady = loudest(at + 72000, at + 140000);
        return 20.0f * std::log10(first / std::max(before, steady));
    };
    CHECK(overshoot("saturation", {}) < 2.0f);
    CHECK(overshoot("saturation", {{1, 30.0f}}) < 2.0f);  // a hot cached draft (was +23 dB in V2.1)
    CHECK(overshoot("transient", {}) < 2.0f);             // was +14 dB in V2.1
}

TEST_CASE("V3 P8: Output Safety Guard holds the sample-peak ceiling at zero latency and is transparent below it") {
    dsp::OutputSafety s;
    s.setMode(dsp::OutputSafety::Guard);
    s.setCeilingDb(-1.0f);
    s.prepare(kSr, kBlock);
    CHECK(s.latency() == 0);
    const float ceil = std::pow(10.0f, -1.0f / 20.0f);
    std::vector<float> L(kBlock), R(kBlock);
    float worst = 0.0f;
    double quietErr = 0.0;
    for (int b = 0; b < 800; ++b) {
        const bool loud = (b / 200) % 2 == 1;
        for (int i = 0; i < kBlock; ++i) {
            const int n = b * kBlock + i;
            L[static_cast<size_t>(i)] = (loud ? 4.0f : 0.3f) * static_cast<float>(std::sin(2 * dsp::kPi * 997.0 * n / kSr));
            R[static_cast<size_t>(i)] = -L[static_cast<size_t>(i)];
        }
        const auto inL = L;
        s.process(L.data(), R.data(), kBlock);
        for (int i = 0; i < kBlock; ++i) worst = std::max({worst, std::fabs(L[static_cast<size_t>(i)]), std::fabs(R[static_cast<size_t>(i)])});
        if (!loud && (b % 200) > 150)  // after a loud section has fully released (> 0.8 s at 120 ms)
            for (int i = 0; i < kBlock; ++i) quietErr = std::max(quietErr, static_cast<double>(std::fabs(L[static_cast<size_t>(i)] - inL[static_cast<size_t>(i)])));
    }
    CHECK(worst <= ceil + 1e-6f);
    CHECK(quietErr < 1e-3);  // fully released: below -60 dB of difference
}

TEST_CASE("V3 P8: True Peak safety: reported latency == measured, inter-sample peaks held, transparent below") {
    dsp::OutputSafety s;
    s.setMode(dsp::OutputSafety::TruePeak);
    s.setCeilingDb(-1.0f);
    s.prepare(kSr, kBlock);
    const int lat = s.latency();
    CHECK(lat > 0);
    CHECK(lat < 200);
    // impulse below the ceiling: delayed by exactly the reported latency, unchanged
    {
        std::vector<float> L(4096, 0.0f), R(4096, 0.0f);
        L[100] = 0.5f;
        R[100] = 0.5f;
        for (int off = 0; off < 4096; off += kBlock) s.process(L.data() + off, R.data() + off, kBlock);
        int at = 0;
        for (int i = 0; i < 4096; ++i) if (std::fabs(L[static_cast<size_t>(i)]) > std::fabs(L[static_cast<size_t>(at)])) at = i;
        CHECK(at == 100 + lat);
        CHECK(L[static_cast<size_t>(at)] == doctest::Approx(0.5f).epsilon(1e-5));
    }
    // fs/4 sine sampled at 45 deg: every sample is 0.707 of the true peak. Driven 6 dB over the ceiling.
    s.reset();
    const float ceil = std::pow(10.0f, -1.0f / 20.0f);
    std::vector<float> L(48000), R(48000);
    for (int n = 0; n < 48000; ++n) L[static_cast<size_t>(n)] = R[static_cast<size_t>(n)] = 2.0f * ceil * static_cast<float>(std::sin(dsp::kPi * 0.5 * n + dsp::kPi * 0.25));
    for (int off = 0; off < 48000; off += kBlock) s.process(L.data() + off, R.data() + off, std::min(kBlock, 48000 - off));
    const float tp = dsp::OutputSafety::estimateTruePeak(L.data() + 4000, 40000);
    CHECK(20.0f * std::log10(tp / ceil) < 0.25f);
    float spk = 0.0f;
    for (size_t i = 4000; i < L.size(); ++i) spk = std::max(spk, std::fabs(L[i]));
    CHECK(spk <= ceil + 1e-6f);
}

TEST_CASE("V3 P8: Output Safety mode switches fade instead of jumping, and the engine reports the safety state") {
    Rig r(dsp::OutputSafety::Guard);
    std::vector<float> y;
    r.run(40, &y);
    r.engine.setSafety(dsp::OutputSafety::TruePeak, -0.5f);
    r.run(40, &y);
    r.engine.setSafety(dsp::OutputSafety::Off, 0.0f);
    r.run(40, &y);
    float m = 0.0f;
    for (size_t i = 1; i < y.size(); ++i) m = std::max(m, std::fabs(y[i] - y[i - 1]));
    float prog = 0.0f;
    for (int i = 1; i < 30000; ++i) prog = std::max(prog, std::fabs(sig(i, 0) - sig(i - 1, 0)));
    CHECK(m <= 1.3f * prog);
    CHECK(r.engine.telemetry().safetyMode.load() == dsp::OutputSafety::Off);
}

TEST_CASE("V3 P8: full-scale abuse through the engine with Guard never leaves the ceiling and never goes non-finite") {
    Rig r(dsp::OutputSafety::Guard);
    r.engine.setSafety(dsp::OutputSafety::Guard, -0.5f);
    for (const char* id : {"saturation", "delay", "grain_engine", "resonator", "filter"}) {
        const int t = fxTypeIndex(id);
        r.session.select(t);
        const auto& d = fxRegistry()[static_cast<size_t>(t)];
        for (size_t i = 0; i < d.params.size(); ++i) r.session.setDraftParam(static_cast<int>(i), d.params[i].maxValue);
        r.session.commit();
    }
    r.engine.setInputGainDb(24.0f);
    std::vector<float> y;
    r.run(600, &y);
    const float ceil = std::pow(10.0f, -0.5f / 20.0f);
    float pk = 0.0f;
    for (float v : y) { REQUIRE(std::isfinite(v)); pk = std::max(pk, std::fabs(v)); }
    CHECK(pk <= ceil + 1e-6f);
}

TEST_CASE("V3 P8: Grain Engine Loop / Freeze grains never read across the held write head (seam guard)") {
    const int t = fxTypeIndex("grain_engine");
    const auto& d = fxRegistry()[static_cast<size_t>(t)];
    auto set = [&](ParamValues& v, const char* id, float x) { v[static_cast<size_t>(d.paramIndex(id))] = x; };
    ParamValues v = d.defaults();
    set(v, "mode", 2);  // Loop
    set(v, "size_ms", 700);
    set(v, "density", 6);
    set(v, "pitch", 12);
    set(v, "pitch_jit", 4);
    set(v, "loop_ms", 600);
    set(v, "speed", 1.0f);
    set(v, "align", 0);
    set(v, "mix", 100);
    for (int fr = 0; fr < 2; ++fr) {
        set(v, "freeze", static_cast<float>(fr));
        auto fx = d.create();
        fx->prepare(kSr, kBlock);
        fx->setParams(d.clamp(v), true);
        std::vector<float> L(kBlock), R(kBlock);
        ProcessContext ctx;
        float y1 = 0, y2 = 0, worst = 0;
        for (int off = 0; off < static_cast<int>(6 * kSr); off += kBlock) {
            for (int i = 0; i < kBlock; ++i) L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = 0.5f * static_cast<float>(std::sin(2 * dsp::kPi * 110.0 * (off + i) / kSr));
            float* ch[2] = {L.data(), R.data()};
            fx->process(ch, kBlock, ctx);
            for (int i = 0; i < kBlock; ++i) {
                const float yy = L[static_cast<size_t>(i)];
                if (off + i > 4) worst = std::max(worst, std::fabs(yy - 2 * y1 + y2));
                y2 = y1;
                y1 = yy;
            }
        }
        // smooth bound: sine at up to 2x pitch with ~1.4 overlap headroom, plus the windows' own curvature
        CHECK_MESSAGE(worst < 0.05f, "freeze " << fr << " curvature " << worst);
    }
}
