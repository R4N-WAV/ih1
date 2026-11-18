// Registry-wide FX contract tests: every production FX must pass all of these.
#include <doctest/doctest.h>

#include <cmath>
#include <set>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/fx/FxProcessor.h"
#include "TestUtil.h"

using namespace ih1;

namespace {

constexpr double kSr = 48000.0;
constexpr int kBlock = 256;

std::vector<float> noise(size_t n, uint32_t seed, float amp = 0.3f) {
    dsp::Rng r(seed);
    std::vector<float> v(n);
    for (auto& x : v) x = amp * r.bipolar();
    return v;
}
std::vector<float> musical(size_t n, uint32_t seed) {
    // pitched tone + decaying bursts: exercises pitch trackers, envelopes and transients
    std::vector<float> v(n);
    dsp::Rng r(seed);
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kSr;
        const double env = std::exp(-3.0 * std::fmod(t, 0.5));
        v[i] = static_cast<float>(0.25 * std::sin(2 * M_PI * 220.0 * t) + 0.12 * std::sin(2 * M_PI * 440.0 * t) + 0.08 * env * r.bipolar());
    }
    return v;
}

std::unique_ptr<FxProcessor> make(int type, const ParamValues& v, double sr = kSr, int block = kBlock) {
    auto fx = fxRegistry()[static_cast<size_t>(type)].create();
    fx->prepare(sr, block);
    fx->setParams(fxRegistry()[static_cast<size_t>(type)].clamp(v), true);
    return fx;
}

std::vector<int> productionTypes() {
    std::vector<int> t;
    for (size_t i = 0; i < fxRegistry().size(); ++i) if (!fxRegistry()[i].experimental) t.push_back(static_cast<int>(i));
    return t;
}

}  // namespace

TEST_CASE("descriptors are complete and stable") {
    std::set<std::string> ids;
    REQUIRE(productionTypes().size() >= 16);
    for (const auto& d : fxRegistry()) {
        CAPTURE(d.typeId);
        CHECK(ids.insert(d.typeId).second);
        CHECK(!d.name.empty());
        CHECK(!d.tooltip.empty());
        CHECK(d.params.size() <= static_cast<size_t>(kMaxParams));
        CHECK(static_cast<bool>(d.create));
        std::set<std::string> pids;
        for (const auto& p : d.params) {
            CAPTURE(p.id);
            CHECK(pids.insert(p.id).second);
            CHECK(!p.tooltip.empty());
            CHECK(p.minValue <= p.defaultValue);
            CHECK(p.defaultValue <= p.maxValue);
            if (p.kind == ParamKind::Choice) CHECK(static_cast<int>(p.choices.size()) == static_cast<int>(p.maxValue) + 1);
            CHECK(p.fromNormalised(p.toNormalised(p.defaultValue)) == doctest::Approx(p.defaultValue).epsilon(1e-3));
        }
    }
}

TEST_CASE("every FX: silence in -> silence out, finite, bounded") {
    for (int t : productionTypes()) {
        const auto& d = fxRegistry()[static_cast<size_t>(t)];
        CAPTURE(d.typeId);
        auto fx = make(t, d.defaults());
        std::vector<float> L(48000, 0.0f), R(48000, 0.0f);
        test::runFx(*fx, L, R, kBlock, kSr);
        float peak = 0;
        for (size_t i = 0; i < L.size(); ++i) peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
        CHECK(peak < 1e-6f);
    }
}

TEST_CASE("every FX: default, min, max and random settings stay finite and bounded") {
    for (int t : productionTypes()) {
        const auto& d = fxRegistry()[static_cast<size_t>(t)];
        CAPTURE(d.typeId);
        std::vector<ParamValues> sets;
        sets.push_back(d.defaults());
        ParamValues lo{}, hi{};
        for (size_t i = 0; i < d.params.size(); ++i) { lo[i] = d.params[i].minValue; hi[i] = d.params[i].maxValue; }
        sets.push_back(lo);
        sets.push_back(hi);
        dsp::Rng r(static_cast<uint32_t>(t) + 9);
        for (int k = 0; k < 6; ++k) {
            ParamValues v{};
            for (size_t i = 0; i < d.params.size(); ++i) v[i] = d.params[i].fromNormalised(r.uniform());
            sets.push_back(v);
        }
        int idx = 0;
        for (const auto& v : sets) {
            CAPTURE(idx++);
            auto fx = make(t, v);
            auto L = musical(48000 * 2, 1), R = musical(48000 * 2, 2);
            for (auto& x : L) x *= 2.0f;  // hot input (~0 dBFS peaks)
            test::runFx(*fx, L, R, kBlock, kSr);
            float peak = 0;
            bool finite = true;
            for (size_t i = 0; i < L.size(); ++i) {
                finite = finite && std::isfinite(L[i]) && std::isfinite(R[i]);
                peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
            }
            CHECK(finite);
            CHECK(peak < 64.0f);  // no runaway: <= +36 dBFS even with every gain/feedback control at maximum
        }
    }
}

TEST_CASE("every FX: reported latency is exact (mix=0 returns the input delayed by the latency)") {
    for (int t : productionTypes()) {
        const auto& d = fxRegistry()[static_cast<size_t>(t)];
        const int mixIdx = d.paramIndex("mix");
        if (mixIdx < 0) continue;  // FX without a mix control (EQ, Width, Harmony) are tested specifically
        CAPTURE(d.typeId);
        ParamValues v = d.defaults();
        v[static_cast<size_t>(mixIdx)] = 0.0f;
        auto fx = make(t, v);
        const int lat = fx->latencySamples();
        const auto in = noise(24000, 3);
        auto L = in, R = in;
        test::runFx(*fx, L, R, kBlock, kSr);
        double err = 0;
        for (size_t i = static_cast<size_t>(lat) + 10; i < in.size(); ++i) err = std::max(err, static_cast<double>(std::fabs(L[i] - in[i - static_cast<size_t>(lat)])));
        CHECK(err < 1e-4);
    }
}

TEST_CASE("every FX: block-size independence and determinism") {
    for (int t : productionTypes()) {
        const auto& d = fxRegistry()[static_cast<size_t>(t)];
        CAPTURE(d.typeId);
        const auto in = musical(48000, 4);
        auto run = [&](int block) {
            auto fx = make(t, d.defaults(), kSr, 512);
            auto L = in, R = in;
            test::runFx(*fx, L, R, block, kSr);
            return L;
        };
        const auto a = run(512), b = run(512), c = run(37);
        double diffDet = 0, diffBlock = 0, energy = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            diffDet = std::max(diffDet, static_cast<double>(std::fabs(a[i] - b[i])));
            diffBlock += (a[i] - c[i]) * static_cast<double>(a[i] - c[i]);
            energy += static_cast<double>(a[i]) * a[i];
        }
        CHECK(diffDet == 0.0);
        // control-rate updates may land on different samples; require the difference to be tiny
        CHECK(10.0 * std::log10((diffBlock + 1e-20) / (energy + 1e-20)) < -40.0);
    }
}

TEST_CASE("every FX: reset clears state; re-prepare at another sample rate works") {
    for (int t : productionTypes()) {
        const auto& d = fxRegistry()[static_cast<size_t>(t)];
        CAPTURE(d.typeId);
        auto fx = make(t, d.defaults());
        auto L = musical(24000, 5), R = musical(24000, 6);
        test::runFx(*fx, L, R, kBlock, kSr);
        fx->reset();
        std::vector<float> z1(24000, 0.0f), z2(24000, 0.0f);
        test::runFx(*fx, z1, z2, kBlock, kSr);
        float peak = 0;
        for (float x : z1) peak = std::max(peak, std::fabs(x));
        CHECK(peak < 1e-6f);
        for (double sr : {44100.0, 96000.0, 192000.0}) {
            fx->prepare(sr, 1024);
            fx->setParams(d.defaults(), true);
            auto l2 = musical(static_cast<size_t>(sr / 4), 7), r2 = l2;
            test::runFx(*fx, l2, r2, 1024, sr);
            for (float x : l2) REQUIRE(std::isfinite(x));
        }
    }
}
