#include "ih1/visual/Palette.h"

#include "ih1/fx/FxProcessor.h"

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
struct FamilyDef { const char* name; float h, s, v; };
// index = FxCategory: TimeSpace, PitchVocal, Modulation, GranularTime, Spectral, RhythmGlitch, Dynamics,
// ToneCharacter, Experimental. Fitted to the measured reference envelope (see the header).
constexpr FamilyDef kFamilies[kNumCategories] = {
    {"Indigo", 236.0f, 0.44f, 0.96f},
    {"Pale lavender", 281.0f, 0.17f, 1.00f},
    {"Violet", 262.0f, 0.46f, 1.00f},
    {"Ice blue", 208.0f, 0.36f, 1.00f},
    {"Mauve", 296.0f, 0.34f, 0.98f},
    {"Rose orchid", 322.0f, 0.40f, 0.96f},
    {"Slate", 248.0f, 0.20f, 0.82f},
    {"Dusty rose", 340.0f, 0.26f, 0.90f},
    {"Pearl", 222.0f, 0.14f, 1.00f},
};
// tier (position of the type inside its family) -> small hue / saturation / value steps
constexpr float kTierHue[6] = {0.0f, 6.0f, -6.0f, 3.0f, -3.0f, 8.0f};
constexpr float kTierSat[6] = {1.0f, 0.86f, 1.10f, 0.94f, 1.05f, 0.80f};
constexpr float kTierVal[6] = {1.0f, 0.94f, 0.97f, 0.90f, 1.0f, 0.92f};

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

Rgb familyColour(int c) {
    const auto& f = kFamilies[std::clamp(c, 0, kNumCategories - 1)];
    return hsvToRgb(f.h, f.s, f.v);
}

const char* familyColourName(int c) { return kFamilies[std::clamp(c, 0, kNumCategories - 1)].name; }

Rgb fxColour(const std::string& typeId, int shade) {
    const auto& reg = fxRegistry();
    int cat = static_cast<int>(FxCategory::Experimental), tier = 0;
    const int t = fxTypeIndex(typeId);
    if (t >= 0) {
        cat = static_cast<int>(reg[static_cast<size_t>(t)].category);
        for (int i = 0; i < t; ++i)
            if (static_cast<int>(reg[static_cast<size_t>(i)].category) == cat && !reg[static_cast<size_t>(i)].experimental) ++tier;
    }
    cat = std::clamp(cat, 0, kNumCategories - 1);
    tier %= 6;
    const auto& f = kFamilies[cat];
    float h = f.h + kTierHue[tier];
    float s = std::clamp(f.s * kTierSat[tier], 0.0f, 0.5f);
    float v = std::clamp(f.v * kTierVal[tier], 0.0f, 1.0f);
    // duplicates: same hue, alternating deeper / lighter in growing steps. Pale families gain saturation
    // instead of losing it so their shades stay distinguishable.
    if (shade > 0) {
        const int step = (shade + 1) / 2;  // 1,1,2,2,3,3...
        const float k = std::min(1.0f, 0.45f * static_cast<float>(step));
        if (shade % 2 == 1) {  // deeper
            s = std::min(0.55f, s * (1.0f + 0.35f * k) + 0.05f * k);
            v *= 1.0f - 0.32f * k;
        } else {               // lighter
            s = s < 0.3f ? std::min(0.5f, s + 0.12f * k) : s * (1.0f - 0.4f * k);
            v = std::min(1.0f, v + 0.05f * k);
            h += 4.0f * static_cast<float>(step);
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
