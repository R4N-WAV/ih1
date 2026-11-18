// IH1 colour identities. One palette shared by the carousel, the settings panel and the formation, so an
// effect has the same colour everywhere.
//
// The palette stays inside the reference video's world (measured in docs/VISUAL_REFERENCE_ANALYSIS.md and
// the V1 feedback research): blue-indigo, periwinkle, violet, lavender, purple-magenta (orchid), restrained
// rose, icy blue and a pale lavender-white. No greens, yellows, oranges or reds.
//
// Each FX type has a fixed role (one of 8) plus a tier. The tier nudges hue and value slightly, so types
// that share a role are still not identical. Families are spread across roles, so the combinations people
// commonly chain (delay + reverb, harmony + doubler, ...) read as different colours. Duplicates of one type
// keep the type's hue and differ by shade (see fxColour's `shade`).
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace ih1::visual {

struct Rgb { float r = 0, g = 0, b = 0; };

Rgb hsvToRgb(float hDeg, float s, float v);
void rgbToHsv(const Rgb& c, float& hDeg, float& s, float& v);

enum class PaletteRole : uint8_t { Indigo, Periwinkle, Violet, Lavender, Orchid, Rose, Ice, Pearl, Count };
constexpr int kNumRoles = static_cast<int>(PaletteRole::Count);

const char* roleName(PaletteRole r);
Rgb roleColour(PaletteRole r);  // base display colour (sRGB, 0..1)

struct FxIdentity {
    PaletteRole role = PaletteRole::Violet;
    int tier = 0;  // 0..4: small hue/value variation between types sharing a role
};

// Stable identity for an FX type id. Unknown ids hash deterministically onto a role.
FxIdentity fxIdentity(const std::string& typeId);

// Display colour of an FX type. `shade` = rank of a duplicate among committed instances of the same type
// (0 = first / the carousel colour). Shades alternate lighter / deeper around the type's own hue.
Rgb fxColour(const std::string& typeId, int shade = 0);

// Perceptual distance helper (CIE76 delta-E in Lab, sRGB input), used by tests and the palette preview.
float deltaE(const Rgb& a, const Rgb& b);

}  // namespace ih1::visual
