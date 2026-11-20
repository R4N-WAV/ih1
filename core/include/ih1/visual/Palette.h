// IH1 colour identities (V2.1). One palette shared by the carousel, menus, settings accents and the
// formation, so an effect has the same colour everywhere.
//
// Colour is assigned per FX *family* (FxCategory), not per type: nine family colours fitted inside the
// envelope measured from the reference video (61 frames, scripts in docs/V2_1_IMPLEMENTATION.md):
// lit pixels sit at hue 195-345 deg with 89 % between 225 and 315, saturation 0.16-0.36, cores near-white
// lavender (h 280, s 0.16). The families use saturation 0.14-0.46 and keep >= 15 delta-E from each other.
// Types inside a family vary slightly (hue +-6 deg, small value/saturation steps) so they read as related
// but still discernible; duplicate instances of one type share its colour with gentle deeper / lighter shades.
// No greens, yellows, oranges or reds; no neon saturation.
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace ih1::visual {

struct Rgb { float r = 0, g = 0, b = 0; };

Rgb hsvToRgb(float hDeg, float s, float v);
void rgbToHsv(const Rgb& c, float& hDeg, float& s, float& v);

// Base colour of a catalogue family (index = FxCategory value, 0..8). Out-of-range indices are clamped.
Rgb familyColour(int category);
const char* familyColourName(int category);

// Display colour of an FX type. `shade` = rank of a duplicate among committed instances of the same type
// (0 = first / the carousel colour). Unknown type ids get the Experimental family colour.
Rgb fxColour(const std::string& typeId, int shade = 0);

// Perceptual distance helper (CIE76 delta-E in Lab, sRGB input), used by tests and the palette preview.
float deltaE(const Rgb& a, const Rgb& b);

}  // namespace ih1::visual
