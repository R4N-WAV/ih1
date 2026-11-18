#include "ih1/visual/Palette.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ih1::visual {

Rgb hsvToRgb(float h, float s, float v) {
    h = std::fmod(std::fmod(h, 360.0f) + 360.0f, 360.0f) / 60.0f;
    const int i = static_cast<int>(h);
    const float f = h - static_cast<float>(i);
    const float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch (i) {
        case 0: return {v, t, p};
        case 1: return {q, v, p};
        case 2: return {p, v, t};
        case 3: return {p, q, v};
        case 4: return {t, p, v};
        default: return {v, p, q};
    }
}

void rgbToHsv(const Rgb& c, float& h, float& s, float& v) {
    const float mx = std::max({c.r, c.g, c.b}), mn = std::min({c.r, c.g, c.b});
    v = mx;
    const float d = mx - mn;
    s = mx > 0.0f ? d / mx : 0.0f;
    if (d <= 0.0f) { h = 0.0f; return; }
    if (mx == c.r) h = 60.0f * std::fmod((c.g - c.b) / d, 6.0f);
    else if (mx == c.g) h = 60.0f * ((c.b - c.r) / d + 2.0f);
    else h = 60.0f * ((c.r - c.g) / d + 4.0f);
    if (h < 0.0f) h += 360.0f;
}

namespace {
struct RoleDef { const char* name; float h, s, v; };
// Tuned against frames of the reference: the dominant body is indigo-violet (250-275 deg), with blue-indigo
// secondaries, a violet/orchid luminous family, sparse rose and icy-blue strands, and pale lavender-white
// highlights. Saturation is held moderate so additive overlap does not neon out.
constexpr RoleDef kRoles[kNumRoles] = {
    {"Indigo", 234.0f, 0.66f, 0.95f},
    {"Periwinkle", 222.0f, 0.42f, 1.00f},
    {"Violet", 264.0f, 0.66f, 1.00f},
    {"Lavender", 274.0f, 0.32f, 1.00f},
    {"Orchid", 292.0f, 0.56f, 1.00f},
    {"Rose", 328.0f, 0.50f, 0.98f},
    {"Ice", 196.0f, 0.44f, 1.00f},
    {"Pearl", 262.0f, 0.11f, 1.00f},
};

// type id -> (role, tier). Families are deliberately spread across roles.
const std::pair<const char*, FxIdentity> kTable[] = {
    // Time & Space
    {"delay", {PaletteRole::Ice, 0}},        {"tape_echo", {PaletteRole::Periwinkle, 0}}, {"reverb", {PaletteRole::Indigo, 0}},
    {"reverse", {PaletteRole::Rose, 0}},     {"shimmer", {PaletteRole::Pearl, 0}},        {"convolution", {PaletteRole::Violet, 0}},
    {"doppler", {PaletteRole::Orchid, 0}},
    // Pitch & Voice
    {"harmony", {PaletteRole::Lavender, 0}}, {"pitch_shift", {PaletteRole::Orchid, 1}},   {"formant", {PaletteRole::Rose, 1}},
    {"doubler", {PaletteRole::Periwinkle, 1}}, {"pitch_correct", {PaletteRole::Ice, 1}}, {"vocoder", {PaletteRole::Violet, 1}},
    // Modulation
    {"chorus", {PaletteRole::Periwinkle, 2}}, {"flanger", {PaletteRole::Orchid, 2}},     {"phaser", {PaletteRole::Rose, 2}},
    {"vibrato", {PaletteRole::Lavender, 1}}, {"tremolo", {PaletteRole::Indigo, 1}},      {"filter", {PaletteRole::Ice, 2}},
    {"ring_mod", {PaletteRole::Pearl, 1}},   {"rotary", {PaletteRole::Violet, 2}},       {"freq_shift", {PaletteRole::Indigo, 2}},
    // Texture & Spectral
    {"granular", {PaletteRole::Lavender, 2}}, {"spectral", {PaletteRole::Pearl, 2}},     {"stutter", {PaletteRole::Rose, 3}},
    {"looper", {PaletteRole::Ice, 3}},       {"resonator", {PaletteRole::Orchid, 3}},
    // Tone & Dynamics
    {"eq", {PaletteRole::Periwinkle, 3}},    {"saturation", {PaletteRole::Rose, 4}},     {"width", {PaletteRole::Ice, 4}},
    {"compressor", {PaletteRole::Indigo, 3}}, {"gate", {PaletteRole::Lavender, 3}},      {"deesser", {PaletteRole::Periwinkle, 4}},
    {"transient", {PaletteRole::Violet, 3}}, {"tape_colour", {PaletteRole::Orchid, 4}},  {"bitcrush", {PaletteRole::Pearl, 3}},
    {"exciter", {PaletteRole::Lavender, 4}}, {"amp", {PaletteRole::Indigo, 4}},
};

// tier -> (hue offset, saturation scale, value scale)
constexpr float kTierHue[5] = {0.0f, 6.0f, -6.0f, 10.0f, -10.0f};
constexpr float kTierSat[5] = {1.0f, 0.88f, 1.1f, 0.8f, 1.15f};
constexpr float kTierVal[5] = {1.0f, 0.97f, 0.93f, 1.0f, 0.9f};

float srgbToLinear(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
void toLab(const Rgb& c, float& L, float& a, float& b) {
    const float r = srgbToLinear(c.r), g = srgbToLinear(c.g), bl = srgbToLinear(c.b);
    float X = 0.4124f * r + 0.3576f * g + 0.1805f * bl, Y = 0.2126f * r + 0.7152f * g + 0.0722f * bl,
          Z = 0.0193f * r + 0.1192f * g + 0.9505f * bl;
    X /= 0.95047f; Z /= 1.08883f;
    auto f = [](float t) { return t > 0.008856f ? std::cbrt(t) : 7.787f * t + 16.0f / 116.0f; };
    L = 116.0f * f(Y) - 16.0f;
    a = 500.0f * (f(X) - f(Y));
    b = 200.0f * (f(Y) - f(Z));
}
}  // namespace

const char* roleName(PaletteRole r) {
    const int i = static_cast<int>(r);
    return i >= 0 && i < kNumRoles ? kRoles[i].name : "?";
}

Rgb roleColour(PaletteRole r) {
    const int i = std::clamp(static_cast<int>(r), 0, kNumRoles - 1);
    return hsvToRgb(kRoles[i].h, kRoles[i].s, kRoles[i].v);
}

FxIdentity fxIdentity(const std::string& id) {
    for (const auto& kv : kTable) if (id == kv.first) return kv.second;
    uint32_t h = 2166136261u;
    for (char c : id) h = (h ^ static_cast<uint8_t>(c)) * 16777619u;
    return {static_cast<PaletteRole>(h % static_cast<uint32_t>(kNumRoles)), static_cast<int>((h >> 8) % 5u)};
}

Rgb fxColour(const std::string& typeId, int shade) {
    const auto id = fxIdentity(typeId);
    const auto& r = kRoles[static_cast<int>(id.role)];
    const int t = std::clamp(id.tier, 0, 4);
    float h = r.h + kTierHue[t];
    float s = std::clamp(r.s * kTierSat[t], 0.0f, 0.95f);
    float v = std::clamp(r.v * kTierVal[t], 0.0f, 1.0f);
    // duplicates: same hue family, alternating deeper / lighter with growing steps. Near-neutral colours
    // (pearl, lavender) gain saturation instead of losing it, so their shades stay distinguishable.
    if (shade > 0) {
        const int step = (shade + 1) / 2;  // 1,1,2,2,3,3...
        const float k = std::min(1.0f, 0.5f * static_cast<float>(step));
        if (shade % 2 == 1) {  // deeper
            s = std::min(0.95f, s * (1.0f + 0.5f * k) + 0.06f * k);
            v *= 1.0f - 0.36f * k;
            h -= 4.0f * static_cast<float>(step);
        } else {               // lighter
            s = s < 0.36f ? std::min(0.95f, s + 0.16f * k) : s * (1.0f - 0.5f * k);
            v = std::min(1.0f, v + 0.05f * k);
            h += 7.0f * static_cast<float>(step);
        }
    }
    return hsvToRgb(h, s, v);
}

float deltaE(const Rgb& x, const Rgb& y) {
    float L1, a1, b1, L2, a2, b2;
    toLab(x, L1, a1, b1);
    toLab(y, L2, a2, b2);
    return std::sqrt((L1 - L2) * (L1 - L2) + (a1 - a2) * (a1 - a2) + (b1 - b2) * (b1 - b2));
}

}  // namespace ih1::visual
