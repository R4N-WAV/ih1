// Formation mapping rules (docs/VISUAL_REFERENCE_ANALYSIS.md section 3).
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

#include "ih1/visual/Formation.h"

using namespace ih1::visual;

namespace {
FrameInput chainOf(int n) {
    FrameInput in;
    for (int i = 0; i < n; ++i) in.chain.push_back({static_cast<uint64_t>(100 + i), hueForType(i % 2 ? "delay" : "harmony"), static_cast<uint32_t>(31 * (i + 1)), 0.02f});
    in.outLevel = 0.03f;
    return in;
}
int settle(Formation& f, const FrameInput& in, int frames = 240) {
    for (int i = 0; i < frames; ++i) f.update(in);
    return f.liveParticles();
}
float meanSaturation(const Formation& f) {
    double s = 0;
    int n = 0;
    for (const auto& g : f.segments()) {
        const float mx = std::max({g.r, g.g, g.b}), mn = std::min({g.r, g.g, g.b});
        if (mx > 1e-6f) { s += (mx - mn) / mx; ++n; }
    }
    return n ? static_cast<float>(s / n) : 0.0f;
}
}  // namespace

TEST_CASE("no committed FX = empty field; more FX = larger formation; fewer = smaller") {
    Formation f;
    CHECK(settle(f, chainOf(0)) == 0);
    CHECK(f.segments().empty());
    const int one = settle(f, chainOf(1));
    const int five = settle(f, chainOf(5));
    CHECK(one > 0);
    CHECK(five > one * 2);
    const int two = settle(f, chainOf(2), 400);
    CHECK(two < five);
}

TEST_CASE("palette stays inside the measured hue arc") {
    for (const char* id : {"delay", "reverb", "harmony", "chorus", "granular", "saturation", "unknown_future_fx"}) {
        const float h = hueForType(id);
        CHECK(h >= 215.0f);
        CHECK(h <= 325.0f);
    }
}

TEST_CASE("SOLO desaturates the committed formation toward monochrome") {
    Formation f;
    auto in = chainOf(4);
    settle(f, in);
    const float sat = meanSaturation(f);
    in.solo = 1.0f;
    settle(f, in, 120);
    CHECK(meanSaturation(f) < sat * 0.2f);
}

TEST_CASE("reorder moves cores smoothly without resetting the field") {
    Formation f;
    auto in = chainOf(4);
    const int before = settle(f, in);
    std::swap(in.chain[0], in.chain[3]);
    f.update(in);
    CHECK(f.liveParticles() > before * 0.9);
    // cores move continuously (no jump in a single frame)
    float x0 = 0;
    for (const auto& c : f.cores()) if (c.id == 103) x0 = c.x;
    f.update(in);
    float x1 = 0;
    for (const auto& c : f.cores()) if (c.id == 103) x1 = c.x;
    CHECK(std::fabs(x1 - x0) < 0.05f);
}

TEST_CASE("formation is robust to hostile telemetry") {
    Formation f;
    auto in = chainOf(3);
    in.outLevel = NAN;
    in.low = -1.0f;
    in.high = INFINITY;
    in.chain[0].level = NAN;
    settle(f, in, 60);
    for (const auto& s : f.segments()) REQUIRE(std::isfinite(s.x0 + s.y0 + s.x1 + s.y1 + s.r + s.g + s.b));
}
