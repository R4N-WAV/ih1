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
        // visible light only (parked ends and faded near-camera segments carry no intensity)
        std::vector<float> xs;
        for (const auto& s : x.segments()) if (s.r + s.g + s.b > 0.02f) xs.push_back(s.x0);
        std::sort(xs.begin(), xs.end());
        return xs.empty() ? 0.0f : xs[xs.size() * 98 / 100] - xs[xs.size() * 2 / 100];
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
    std::vector<float> steps;
    for (int i = 0; i < 240; ++i) {
        f.update(in);
        const auto& cur = f.segments();
        REQUIRE(cur.size() == prev.size());  // stable topology: no strand is created or dropped
        for (size_t k = 0; k < cur.size(); k += 7) {
            if (cur[k].r + cur[k].g + cur[k].b <= 0.0f) continue;  // parked, invisible
            if (cur[k].y0 < 0.0f || cur[k].y0 > 1.0f || cur[k].x0 < 0.0f || cur[k].x0 > in.aspect) continue;  // off screen
            const float d = std::max({std::fabs(cur[k].x0 - prev[k].x0), std::fabs(cur[k].y0 - prev[k].y0)});
            steps.push_back(d);
            worst = std::max(worst, d);
        }
        prev = cur;
    }
    std::sort(steps.begin(), steps.end());
    const float p99 = steps[steps.size() * 99 / 100];
    INFO("per-frame motion: p99 " << p99 << ", max " << worst << " (field-height units at 60 fps)");
    // field units are fractions of the field height: slow (p99 < 0.4 % per frame), and never a pop (a reseed
    // or teleport moves a strand 5 %+ in one frame). The few fastest segments are smoothed branch switches of
    // faint fringe strands (exponential glides over ~0.5 s); see docs/KNOWN_ISSUES.md.
    CHECK(p99 < 0.004f);
    CHECK(worst < 0.03f);
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

TEST_CASE("palette: every FX has a stable colour inside the reference's measured world") {
    for (const auto& d : ih1::fxRegistry()) {
        const Rgb c = fxColour(d.typeId);
        float h, s, v;
        rgbToHsv(c, h, s, v);
        // blue-cyan .. rose only (no greens, yellows, oranges or reds), restrained saturation (no neon)
        CHECK_MESSAGE((s < 0.16f || (h >= 195.0f && h <= 350.0f)), d.typeId << " hue " << h);
        CHECK_MESSAGE(s <= 0.5f, d.typeId << " saturation " << s);
        CHECK(v > 0.75f);  // readable on the dark UI
        const Rgb again = fxColour(d.typeId);
        CHECK(again.r == c.r);
    }
}

TEST_CASE("palette: families are clearly separated; types in a family are related; duplicates differ") {
    for (int a = 0; a < ih1::kNumCategories; ++a)
        for (int b = a + 1; b < ih1::kNumCategories; ++b)
            CHECK_MESSAGE(deltaE(familyColour(a), familyColour(b)) > 15.0f, familyColourName(a) << " vs " << familyColourName(b));
    // different families read as different colours
    for (auto [x, y] : {std::pair{"delay", "chorus"}, std::pair{"reverb", "harmony"}, std::pair{"granular", "reverb"},
                        std::pair{"saturation", "compressor"}, std::pair{"grain_engine", "spectral"}})
        CHECK_MESSAGE(deltaE(fxColour(x), fxColour(y)) > 12.0f, x << " vs " << y);
    // same family: related (closer than different families) but not identical
    for (auto [x, y] : {std::pair{"delay", "reverb"}, std::pair{"harmony", "doubler"}, std::pair{"chorus", "phaser"}}) {
        const float d = deltaE(fxColour(x), fxColour(y));
        CHECK_MESSAGE(d > 2.0f, x << " vs " << y << " " << d);
        CHECK_MESSAGE(d < 15.0f, x << " vs " << y << " " << d);
    }
    for (const char* t : {"delay", "reverb", "harmony", "shimmer"}) {
        float h0, s0, v0;
        rgbToHsv(fxColour(t, 0), h0, s0, v0);
        for (int k = 1; k <= 3; ++k) {
            float h, s, v;
            rgbToHsv(fxColour(t, k), h, s, v);
            if (s0 > 0.2f) CHECK(std::fabs(h - h0) < 12.0f);
            CHECK(deltaE(fxColour(t, k), fxColour(t, k - 1)) > 4.0f);
        }
    }
}

TEST_CASE("V2.1: the formation runs from bottom-left (input) to top-right (output), never a sideways T") {
    Formation f;
    auto in = chainOf(7);
    settle(f, in, 300);
    // node positions in field coordinates (y down): x rises and y falls with chain position
    std::vector<float> xs, ys;
    for (const auto& ii : in.chain) {
        float x, y, d;
        f.projectPoint(f.nodePosition(ii.id), x, y, d);
        xs.push_back(x);
        ys.push_back(y);
    }
    auto corr = [](const std::vector<float>& a) {
        const float n = static_cast<float>(a.size());
        float mi = (n - 1) / 2, ma = 0, sab = 0, saa = 0, sbb = 0;
        for (float v : a) ma += v / n;
        for (size_t i = 0; i < a.size(); ++i) { const float di = static_cast<float>(i) - mi, da = a[i] - ma; sab += di * da; saa += di * di; sbb += da * da; }
        return sab / std::sqrt(saa * sbb + 1e-12f);
    };
    CHECK(corr(xs) > 0.85f);   // left -> right
    CHECK(corr(ys) < -0.75f);  // bottom -> top
    // and the light is spread along that diagonal: no perpendicular arm (the V1.1 'T'). Covariance of the
    // segment midpoints: the main axis must be diagonal (between 20 and 70 degrees above horizontal).
    double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0, w = 0;
    for (const auto& sg : f.segments()) {
        const double i = sg.r + sg.g + sg.b;
        if (i <= 0) continue;
        const double mx = 0.5 * (sg.x0 + sg.x1), my = -0.5 * (sg.y0 + sg.y1);
        sx += i * mx; sy += i * my; sxx += i * mx * mx; syy += i * my * my; sxy += i * mx * my; w += i;
    }
    const double cxx = sxx / w - (sx / w) * (sx / w), cyy = syy / w - (sy / w) * (sy / w), cxy = sxy / w - (sx / w) * (sy / w);
    const double angle = 0.5 * std::atan2(2.0 * cxy, cxx - cyy) * 180.0 / M_PI;
    INFO("principal axis " << angle << " deg");
    CHECK(angle > 20.0);
    CHECK(angle < 70.0);
}

TEST_CASE("V2.1: a transfer requested before its instance reaches the formation is not lost") {
    Formation f;
    auto three = chainOf(3);
    settle(f, three, 120);
    // the COMMIT event arrives a frame before the chain snapshot that contains the new instance
    FrameInput early = three;
    early.transfers.push_back({103, 0.3f});
    f.update(early);
    CHECK(f.pendingTransfers() == 1);
    auto four = chainOf(4);
    f.update(four);
    CHECK(f.pendingTransfers() == 0);
    CHECK(f.transferActive());
    for (int i = 0; i < 100; ++i) f.update(four);
    CHECK_FALSE(f.transferActive());
    // a transfer for an id that never arrives expires (no leak, no ghost)
    FrameInput bogus = four;
    bogus.transfers.push_back({424242, 0.3f});
    f.update(bogus);
    for (int i = 0; i < 150; ++i) f.update(four);
    CHECK(f.pendingTransfers() == 0);
}

TEST_CASE("V2.1: SOLO turns the formation monochrome and colour returns afterwards") {
    Formation f;
    auto in = chainOf(4);
    settle(f, in, 200);
    auto satOf = [&] {
        double s = 0, n = 0;
        for (const auto& sg : f.segments()) {
            const float mx = std::max({sg.r, sg.g, sg.b}), mn = std::min({sg.r, sg.g, sg.b});
            if (mx > 1e-3f) { s += (mx - mn) / mx; n += 1; }
        }
        return s / std::max(1.0, n);
    };
    const double colour = satOf();
    in.monochrome = 1.0f;
    settle(f, in, 60);
    const double mono = satOf();
    in.monochrome = 0.0f;
    settle(f, in, 60);
    const double back = satOf();
    INFO("saturation colour " << colour << " mono " << mono << " back " << back);
    CHECK(mono < 0.12);
    CHECK(colour > 0.2);
    CHECK(std::fabs(back - colour) < 0.03);
}

TEST_CASE("V2.1: audio energy raises the light smoothly and boundedly; transients travel as pulses") {
    auto total = [](const Formation& f) { double t = 0; for (const auto& s : f.segments()) t += s.r + s.g + s.b; return t; };
    Formation quiet, loud;
    auto in = chainOf(4, 0.0f);
    settle(quiet, in, 200);
    auto inLoud = chainOf(4, 0.25f);
    inLoud.audio.energy = 0.8f;
    inLoud.audio.high = 0.6f;
    settle(loud, inLoud, 200);
    const double ratio = total(loud) / total(quiet);
    INFO("loud / quiet light " << ratio);
    CHECK(ratio > 1.3);
    CHECK(ratio < 4.0);
    // a transient: light rises briefly, then settles (no permanent pump)
    const double base = total(loud);
    inLoud.audio.transient = 1.0f;
    loud.update(inLoud);
    inLoud.audio.transient = 0.0f;
    double peak = 0;
    for (int i = 0; i < 30; ++i) { loud.update(inLoud); peak = std::max(peak, total(loud)); }
    for (int i = 0; i < 240; ++i) loud.update(inLoud);
    INFO("transient peak " << peak / base << ", settled " << total(loud) / base);
    CHECK(peak > base * 1.03);
    CHECK(std::fabs(total(loud) / base - 1.0) < 0.1);
    // silence: steady, not dark
    CHECK(total(quiet) > 0.0);
}

TEST_CASE("V2.1: the field glow exists only around committed FX and stays faint") {
    Formation f;
    settle(f, chainOf(0), 30);
    CHECK(f.glows().empty());
    settle(f, chainOf(3), 200);
    REQUIRE_FALSE(f.glows().empty());
    for (const auto& g : f.glows()) {
        CHECK(std::max({g.r, g.g, g.b}) < 0.4f);
        CHECK(g.b >= g.r);  // deep violet / indigo, never warm
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
