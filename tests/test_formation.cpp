// Formation and palette rules (V1 feedback 4.2, 4.4, 4.5, 4.7, 4.10, 4.17).
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <map>

#include "ih1/fx/FxProcessor.h"
#include "ih1/visual/Formation.h"
#include "ih1/visual/Palette.h"
#include "ih1/visual/Raster.h"

using namespace ih1::visual;

namespace {
const char* kTypes[] = {"delay", "reverb", "harmony", "chorus", "granular", "saturation", "tape_echo", "filter"};

FrameInput chainOf(int n, float level = 0.02f) {
    FrameInput in;
    std::map<std::string, int> rank;
    for (int i = 0; i < n; ++i) {
        const char* t = kTypes[i % 8];
        in.chain.push_back({static_cast<uint64_t>(100 + i), fxColour(t, rank[t]++), static_cast<uint32_t>(31 * (i + 1)), level});
    }
    return in;
}
void settle(Formation& f, const FrameInput& in, int frames = 200) {
    for (int i = 0; i < frames; ++i) f.update(in);
}
float maxStep(const std::vector<Segment>& a, const std::vector<Segment>& b) {
    REQUIRE(a.size() == b.size());
    float m = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
        m = std::max({m, std::fabs(a[i].x0 - b[i].x0), std::fabs(a[i].y0 - b[i].y0), std::fabs(a[i].x1 - b[i].x1), std::fabs(a[i].y1 - b[i].y1)});
    return m;
}
}  // namespace

TEST_CASE("no committed FX = empty field; more FX = larger formation") {
    Formation f;
    settle(f, chainOf(0));
    CHECK(f.segments().empty());
    CHECK(f.strandCount() == 0);
    Formation a, b;
    settle(a, chainOf(1));
    settle(b, chainOf(6));
    CHECK(a.strandCount() > 0);
    CHECK(b.strandCount() > a.strandCount() * 3);
    auto spanX = [](const Formation& x) {
        float lo = 1e9f, hi = -1e9f;
        for (const auto& s : x.segments()) { lo = std::min(lo, s.x0); hi = std::max(hi, s.x0); }
        return hi - lo;
    };
    CHECK(spanX(b) > spanX(a) * 1.4f);
}

TEST_CASE("formation is deterministic: same committed state, same picture") {
    Formation a, b;
    auto in = chainOf(4);
    settle(a, in, 90);
    settle(b, in, 90);
    REQUIRE(a.segments().size() == b.segments().size());
    CHECK(maxStep(a.segments(), b.segments()) == 0.0f);
}

TEST_CASE("motion is slow and continuous (no jumps, no fast swirl)") {
    Formation f;
    auto in = chainOf(5);
    in.dt = 1.0f / 60.0f;
    settle(f, in, 400);  // grown and settled
    auto prev = f.segments();
    float worst = 0.0f;
    for (int i = 0; i < 240; ++i) {
        f.update(in);
        worst = std::max(worst, maxStep(prev, f.segments()));
        prev = f.segments();
    }
    // field units are fractions of the field height: < 0.4 % of the height per frame at 60 fps
    CHECK(worst < 0.004f);
}

TEST_CASE("reorder glides without resetting the field") {
    Formation f;
    auto in = chainOf(4);
    settle(f, in, 400);
    const int before = f.strandCount();
    auto prev = f.segments();
    std::swap(in.chain[0], in.chain[3]);
    f.update(in);
    CHECK(std::abs(f.strandCount() - before) <= before / 50);  // nothing is reseeded or cleared
    // same strands, same order of emission per instance: nothing teleports in the reorder frame
    float worst = 0.0f;
    if (prev.size() == f.segments().size()) worst = maxStep(prev, f.segments());
    CHECK(worst < 0.05f);  // a glide, not a jump (a reseed or teleport moves strands ~0.3+)
}

TEST_CASE("commit transfer: a packet travels, then the region grows; chain state is never changed by it") {
    Formation f;
    auto in = chainOf(3);
    settle(f, in, 300);
    auto full = chainOf(4);
    full.transfers.push_back({full.chain.back().id, 0.3f});
    f.update(full);
    full.transfers.clear();
    CHECK(f.transferActive());
    CHECK(f.instanceCount() == 4);
    for (int i = 0; i < 90; ++i) f.update(full);  // 1.5 s
    CHECK_FALSE(f.transferActive());
    CHECK(f.instanceCount() == 4);
    // rapid consecutive commits stay correct
    auto five = chainOf(6);
    five.transfers.push_back({104, 0.3f});
    five.transfers.push_back({105, 0.3f});
    f.update(five);
    five.transfers.clear();
    for (int i = 0; i < 120; ++i) f.update(five);
    CHECK(f.instanceCount() == 6);
    CHECK_FALSE(f.transferActive());
}

TEST_CASE("the field is an empty black void outside the strands") {
    Canvas c(320, 200);
    Formation f;
    for (int i = 0; i < 60; ++i) {
        f.update(chainOf(0));
        c.decay(f.decay(1.0f / 60.0f));
        c.draw(f.segments(), 200.0f);
    }
    std::vector<uint8_t> px;
    c.composite(px, 1.6f);
    CHECK(*std::max_element(px.begin(), px.end()) == 0);  // pure #000000, no stars, no floor

    Formation g;
    Canvas d(320, 200);
    for (int i = 0; i < 300; ++i) {
        g.update(chainOf(5));
        d.decay(g.decay(1.0f / 60.0f));
        d.draw(g.segments(), 200.0f);
    }
    d.composite(px, 1.6f);
    int lit = 0, cornerMax = 0;
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320; ++x) {
            const int v = std::max({px[static_cast<size_t>((y * 320 + x) * 3)], px[static_cast<size_t>((y * 320 + x) * 3 + 1)], px[static_cast<size_t>((y * 320 + x) * 3 + 2)]});
            lit += v > 8;
            if (x < 24 && y < 16) cornerMax = std::max(cornerMax, v);  // top-left corner: negative space
        }
    CHECK(lit > 320 * 200 / 50);   // there is a formation
    CHECK(lit < 320 * 200 * 3 / 4);  // and generous black space around it
    CHECK(cornerMax == 0);
}

TEST_CASE("tone map keeps black black and preserves hue below the highlights") {
    const Rgb z = toneMap({0, 0, 0}, 1.6f);
    CHECK(z.r == 0.0f);
    CHECK(z.g == 0.0f);
    CHECK(z.b == 0.0f);
    const Rgb in = fxColour("delay");
    float h0, s0, v0, h1, s1, v1;
    rgbToHsv(in, h0, s0, v0);
    rgbToHsv(toneMap({in.r * 0.3f, in.g * 0.3f, in.b * 0.3f}, 1.6f), h1, s1, v1);
    CHECK(std::fabs(h1 - h0) < 1.0f);
    CHECK(std::fabs(s1 - s0) < 0.02f);
}

TEST_CASE("palette: every FX has a stable colour inside the reference's world") {
    for (const auto& d : ih1::fxRegistry()) {
        const Rgb c = fxColour(d.typeId);
        float h, s, v;
        rgbToHsv(c, h, s, v);
        // blue-cyan .. rose only (no greens, yellows, oranges or reds), or a near-neutral pearl
        CHECK_MESSAGE((s < 0.16f || (h >= 185.0f && h <= 340.0f)), d.typeId << " hue " << h);
        CHECK(v > 0.8f);  // readable on the dark UI
        const Rgb again = fxColour(d.typeId);
        CHECK(again.r == c.r);
    }
}

TEST_CASE("palette: roles are clearly separated; duplicates keep their hue but differ") {
    for (int a = 0; a < kNumRoles; ++a)
        for (int b = a + 1; b < kNumRoles; ++b)
            CHECK_MESSAGE(deltaE(roleColour(static_cast<PaletteRole>(a)), roleColour(static_cast<PaletteRole>(b))) > 12.0f,
                          roleName(static_cast<PaletteRole>(a)) << " vs " << roleName(static_cast<PaletteRole>(b)));
    // commonly chained pairs read as different colours
    for (auto [x, y] : {std::pair{"delay", "reverb"}, std::pair{"reverb", "shimmer"}, std::pair{"harmony", "doubler"},
                        std::pair{"chorus", "reverb"}, std::pair{"saturation", "compressor"}, std::pair{"granular", "reverb"}})
        CHECK_MESSAGE(deltaE(fxColour(x), fxColour(y)) > 12.0f, x << " vs " << y);
    for (const char* t : {"delay", "reverb", "harmony", "shimmer"}) {
        float h0, s0, v0;
        rgbToHsv(fxColour(t, 0), h0, s0, v0);
        for (int k = 1; k <= 3; ++k) {
            float h, s, v;
            rgbToHsv(fxColour(t, k), h, s, v);
            if (s0 > 0.2f) CHECK(std::fabs(h - h0) < 12.0f);
            CHECK(deltaE(fxColour(t, k), fxColour(t, k - 1)) > 5.0f);
        }
    }
}

TEST_CASE("formation is robust to hostile input") {
    Formation f;
    auto in = chainOf(3);
    in.chain[0].level = NAN;
    in.chain[1].level = -1.0f;
    in.chain[2].level = INFINITY;
    in.chain[2].colour = {NAN, 2.0f, -1.0f};
    in.aspect = 0.0f;
    in.dt = NAN;
    settle(f, in, 60);
    for (const auto& s : f.segments()) REQUIRE(std::isfinite(s.x0 + s.y0 + s.x1 + s.y1 + s.r + s.g + s.b));
    in.transfers.push_back({999999, NAN});  // unknown id: ignored
    f.update(in);
    CHECK(f.instanceCount() == 3);
}
