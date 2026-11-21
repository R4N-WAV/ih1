// V3 Phase 3A / V4 Phase 5: the field volume (CPU reference of the GL shader).
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

namespace {
// V4: the void is the plum / navy background (never pure black, never decorated)
void bgAt(const std::vector<float>& img, int w, int h, int x, int y, float energy, float* exp3) {
    (void)img;
    backgroundColour((x + 0.5f) / w, (y + 0.5f) / h, energy, exp3[0], exp3[1], exp3[2]);
}
}  // namespace

TEST_CASE("V4 field volume: no committed FX = only the plum void (not black, no stars); FX light a bounded region") {
    Formation f;
    FrameInput empty;
    empty.aspect = 1.6f;
    f.update(empty);
    {
        const auto e = render(f);
        double mx = 0, mn = 1;
        for (size_t k = 0; k < e.size(); k += 3) { mx = std::max<double>(mx, e[k + 2]); mn = std::min<double>(mn, e[k + 2]); }
        CHECK(mn > 0.0);            // never pure black
        CHECK(mx < 0.04);           // a dark void
        CHECK(mx < 1.6 * mn);       // smooth, no points of light (no stars, dots or decoration)
    }
    Formation g;
    auto in = chainOf(6);
    for (int i = 0; i < 90; ++i) g.update(in);
    const auto img = render(g);
    CHECK(total(img) > 1.0);
    for (float v : img) REQUIRE(std::isfinite(v));
    // the corners are the plain background (the field fades out well inside its bound: no visible bound edge)
    const int w = 160, h = 100;
    double dev = 0;
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 6; ++x) {
            float b[3];
            bgAt(img, w, h, x, y, g.scene().energy, b);
            for (int c = 0; c < 3; ++c) dev += std::fabs(img[static_cast<size_t>((y * w + x) * 3 + c)] - b[c]);
        }
    CHECK(dev < 1e-3);
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
    CHECK(sc.packets.size() == 1);
    // after integration the new node has a field of its own
    for (int i = 0; i < 160; ++i) f.update(four);
    CHECK(f.scene().nodes.back().strength > 0.5f);
    CHECK(total(render(f)) > before);
}

TEST_CASE("V3 field volume: the GLSL source is generated from the same constants (no placeholders left)") {
    const auto& src = fieldFragmentShader();
    for (const char* k : {"FIL ", "RIB ", "VEILE", "VEILA", "ABSORB", "CAMDIST", "SCALE", "FOG", "HALO", "TENDRIL", "CHROMA", "PACKET"}) CHECK(src.find(k) == std::string::npos);
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
        float b[3];
        const int px = static_cast<int>((k / 3) % 160), py = static_cast<int>((k / 3) / 160);
        bgAt(img, 160, 100, px, py, m.scene().energy, b);
        const float r = img[k] - b[0], g2 = img[k + 1] - b[1], bl = img[k + 2] - b[2];  // the field's own light
        const float mx = std::max({r, g2, bl}), mn = std::min({r, g2, bl});
        if (mx < 0.02f) continue;
        sat += (mx - mn) / mx;
        ++n;
    }
    REQUIRE(n > 0);
    CHECK(sat / n < 0.2);
}
