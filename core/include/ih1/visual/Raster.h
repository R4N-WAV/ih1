// CPU rasteriser for formation segments: the same look as the GL path (additive float accumulation with
// light persistence, quarter-resolution bloom for the smoky wisps, hue-preserving tone map, pure black
// floor). Used by the software fallback in the plug-in, the offline preview tool and tests.
#pragma once

#include <cstdint>
#include <vector>

#include "ih1/visual/Formation.h"

namespace ih1::visual {

class Canvas {
public:
    Canvas(int w = 1, int h = 1) { resize(w, h); }
    void resize(int w, int h);  // clears
    int width() const { return w_; }
    int height() const { return h_; }
    void clear();
    void decay(float d);
    // field coordinates (x in [0, aspect], y in [0, 1]) scaled by `scale` pixels per field-height unit
    void draw(const std::vector<Segment>& segs, float scale, float gain = 1.0f);
    // Composite to 8-bit RGB (row-major, 3 bytes per pixel): accumulation + bloom + field glows -> toneMap.
    // Glows (field coordinates, `scale` px per field-height unit) are evaluated at bloom resolution.
    void composite(std::vector<uint8_t>& rgb, float exposure, float bloomGain = 0.75f, const std::vector<Glow>* glows = nullptr, float scale = 1.0f) const;
    // Linear value of the accumulation buffer (tests).
    float at(int x, int y, int ch) const { return acc_[static_cast<size_t>((y * w_ + x) * 3 + ch)]; }

private:
    void line(const Segment& s, float scale, float gain);
    int w_ = 1, h_ = 1;
    std::vector<float> acc_;
    mutable std::vector<float> bloom_, tmp_;
};

}  // namespace ih1::visual
