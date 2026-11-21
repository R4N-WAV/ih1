// V3 Phase 3A: the field volume (CPU reference of the GL shader).
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "ih1/visual/FieldVolume.h"
#include "ih1/visual/Formation.h"
#include "ih1/visual/Palette.h"

using namespace ih1::visual;

namespace {
FrameInput chainOf(int n) {
    FrameInput in;
    static const char* types[] = {"delay", "reverb", "harmony", "chorus", "granular", "saturation", "tape_echo", "spectral"};
    for (int i = 0; i < n; ++i) in.chain.push_back({static_cast<uint64_t>(100 + i), fxColour(types[i % 8], 0), static_cast<uint32_t>(7919u * (i + 3)), 0.03f});
    in.aspect = 1.6f;
    in.audio.energy = 0.3f;
    return in;
}
std::vector<float> render(const Formation& f, int w = 160, int h = 100) {
    std::vector<float> rgb;
    renderVolume(f.scene(), w, h, rgb, 32);
    return rgb;
}
double total(const std::vector<float>& v) { double s = 0; for (float x : v) s += x; return s; }
}  // namespace

TEST_CASE("V3 field volume: no committed FX = exactly black; FX light up a bounded region; the corners stay black") {
    Formation f;
    FrameInput empty;
    empty.aspect = 1.6f;
    f.update(empty);
    CHECK(total(render(f)) == 0.0);
    Formation g;
    auto in = chainOf(6);
    for (int i = 0; i < 90; ++i) g.update(in);
    const auto img = render(g);
    CHECK(total(img) > 1.0);
    for (float v : img) REQUIRE(std::isfinite(v));
    // the void: the four corner regions are exactly black (rays outside the formation's bound are never marched)
    const int w = 160, h = 100;
    double corners = 0;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            for (int c = 0; c < 3; ++c) {
                corners += img[static_cast<size_t>((y * w + x) * 3 + c)];
                corners += img[static_cast<size_t>(((h - 1 - y) * w + (w - 1 - x)) * 3 + c)];
            }
    CHECK(corners == 0.0);
}

TEST_CASE("V3 field volume: deterministic, and the light stays in the accepted palette envelope") {
    Formation a, b;
    auto in = chainOf(5);
    for (int i = 0; i < 60; ++i) { a.update(in); b.update(in); }
    const auto x = render(a), y = render(b);
    CHECK(x == y);
    // hue of lit pixels: inside the reference's measured envelope (195..345 deg, docs/V2_1_IMPLEMENTATION.md 4.3)
    int lit = 0, inside = 0;
    for (size_t k = 0; k < x.size(); k += 3) {
        const Rgb c{x[k], x[k + 1], x[k + 2]};
        const float m = std::max({c.r, c.g, c.b});
        if (m < 0.02f) continue;
        float hd, s, v;
        rgbToHsv(c, hd, s, v);
        ++lit;
        inside += (hd >= 185.0f && hd <= 350.0f) || s < 0.12f;
    }
    REQUIRE(lit > 50);
    CHECK(inside >= lit * 95 / 100);
}

TEST_CASE("V3 field volume: an instance that is still transferring contributes no stable field light") {
    Formation f;
    auto three = chainOf(3);
    for (int i = 0; i < 120; ++i) f.update(three);
    const double before = total(render(f));
    auto four = chainOf(4);
    four.transfers.push_back({four.chain.back().id, 0.3f});
    f.update(four);
    four.transfers.clear();
    for (int i = 0; i < 20; ++i) f.update(four);  // mid-flight
    REQUIRE(f.lifeOf(four.chain.back().id) == static_cast<int>(Formation::Life::Transferring));
    // the scene's node for the new instance has zero strength; its packet exists separately
    const auto& sc = f.scene();
    REQUIRE(sc.nodes.size() == 4);
    CHECK(sc.nodes.back().strength == 0.0f);
    CHECK(sc.links.back().strength == 0.0f);
    CHECK(sc.packets.size() == 1);
    // after integration the new node has a field of its own
    for (int i = 0; i < 160; ++i) f.update(four);
    CHECK(f.scene().nodes.back().strength > 0.5f);
    CHECK(total(render(f)) > before);
}

TEST_CASE("V3 field volume: the GLSL source is generated from the same constants (no placeholders left)") {
    const auto& src = fieldFragmentShader();
    for (const char* k : {"SKIN", "CORE", "LINK", "ABSORB", "CAMDIST", "SCALE", "FOG", "FLUX", "VEIL"}) CHECK(src.find(k) == std::string::npos);
    CHECK(src.find("#version 150") == 0);
    CHECK(src.find("uniform vec4 uNodeP[32]") != std::string::npos);
}

TEST_CASE("V3 field volume: audio energy raises the light smoothly; SOLO turns it monochrome") {
    Formation q, l;
    auto quiet = chainOf(4), loud = chainOf(4);
    quiet.audio.energy = 0.05f;
    loud.audio.energy = 0.9f;
    for (int i = 0; i < 120; ++i) { q.update(quiet); l.update(loud); }
    const double a = total(render(q)), b = total(render(l));
    CHECK(b > 1.2 * a);
    CHECK(b < 4.0 * a);
    Formation m;
    auto solo = chainOf(4);
    solo.monochrome = 1.0f;
    for (int i = 0; i < 60; ++i) m.update(solo);
    const auto img = render(m);
    double sat = 0, n = 0;
    for (size_t k = 0; k < img.size(); k += 3) {
        const float mx = std::max({img[k], img[k + 1], img[k + 2]}), mn = std::min({img[k], img[k + 1], img[k + 2]});
        if (mx < 0.02f) continue;
        sat += (mx - mn) / mx;
        ++n;
    }
    REQUIRE(n > 0);
    CHECK(sat / n < 0.2);
}
