// IH1 formation (render-API independent): a genuine 3-D field of fine luminous filaments, one region per
// committed FX instance, projected through a perspective camera into 2-D additive line segments. The
// plug-in draws the segments with OpenGL into a float accumulation buffer, adds bloom (the smoky wisps)
// and tone-maps. The software fallback and the offline preview rasterise the same segments on the CPU
// (ih1/visual/Raster.h).
//
// Design (V1 feedback 4.2, 4.3, 4.7, 4.10, 4.17; reference video):
//  * Only committed instances exist here. Nothing about the staged FX is an input, so a staged FX cannot
//    show in the formation.
//  * Every strand is a streamline of a slowly evolving, divergence-free 3-D flow field (a sum of
//    plane-wave curl terms with drifting phases) traced from a stable, seeded anchor point. Strands keep
//    their identity from frame to frame: no reseeding, teleporting or random per-frame motion.
//  * The skeleton follows the reference: a vertical plume crossing a horizontal arm that recedes in depth.
//    The luminous core is at their intersection, right of centre, with black negative space around it.
//    Signal order runs from the tip of the arm (first FX) through the core into the plume (last FX).
//  * Motion is slow: field phases drift at ~0.05-0.12 rad/s, anchors breathe a few hundredths of a unit,
//    and the camera parallax is under 4 degrees over more than a minute. Audio raises light and slightly
//    raises tension; it never spins the field.
//  * A COMMIT sends a short packet of the FX's colour from the left edge of the field into its new
//    region, which then grows its strands outward (the transfer animation).
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ih1/visual/Palette.h"

namespace ih1::visual {

struct InstanceInput {
    uint64_t id = 0;
    Rgb colour{0.6f, 0.5f, 1.0f};  // display colour (fxColour of its type and duplicate shade)
    uint32_t seed = 1;
    float level = 0.0f;  // audio RMS at this instance's output (linear)
};

struct TransferRequest {
    uint64_t id = 0;        // committed instance that was just created by COMMIT
    float originY = 0.35f;  // where the packet enters, as a fraction of the field height (left edge)
};

struct FrameInput {
    std::vector<InstanceInput> chain;      // committed chain in processing order
    std::vector<TransferRequest> transfers;  // COMMIT events since the last frame
    float dt = 1.0f / 60.0f;
    float aspect = 1.6f;                   // field width / height
};

struct Segment {
    float x0, y0, x1, y1;  // field coordinates: x in [0, aspect], y in [0, 1] (y down)
    float r, g, b;         // additive intensity
};

enum class Quality : uint8_t { Low = 0, Medium = 1, High = 2 };

class Formation {
public:
    explicit Formation(Quality q = Quality::Medium);
    void setQuality(Quality q) { quality_ = q; }
    Quality quality() const { return quality_; }

    void update(const FrameInput& in);
    const std::vector<Segment>& segments() const { return segs_; }
    // Persistence of the accumulation buffer for this frame's dt (light temporal smoothing only).
    float decay(float dt) const;

    int strandCount() const { return strands_; }
    int instanceCount() const { return static_cast<int>(insts_.size()); }
    bool transferActive() const;
    float time() const { return time_; }

private:
    struct V3 { float x = 0, y = 0, z = 0; };
    struct Strand {
        V3 offset;        // from the anchor, in world units
        V3 drift;         // slow breathing phases
        float gain = 1.0f;
        float turb = 1.0f;    // how much the curl field folds it
        float phase = 0.0f;
        uint8_t kind = 0;     // 0 hair, 1 wisp, 2 spark, 3 veil
        uint8_t steps = 16;   // integration steps each way
        float dirBias = 0.0f; // which way along the skeleton it leans
    };
    struct Inst {
        uint64_t id = 0;
        Rgb colour;
        uint32_t seed = 1;
        V3 anchor, target;
        V3 tangent{1, 0, 0}, tangentTarget{1, 0, 0};
        float plume = 0.0f, plumeTarget = 0.0f;  // 0 = arm (horizontal sheet), 1 = plume (vertical sheet)
        float presence = 0.0f;  // fades in on arrival, out on delete
        float grow = 0.0f;      // strand length fraction (grows after arrival)
        float hold = 0.0f;      // seconds until it may grow (packet in flight)
        float bright = 0.0f;    // smoothed audio level
        bool inChain = false;
        int index = 0;
        std::vector<Strand> strands;
        // transfer packet
        bool transfer = false;
        float tTransfer = 0.0f;
        float originY = 0.35f;
    };

    void layout(const FrameInput& in);
    void buildStrands(Inst& inst, int count);
    V3 field(const V3& p, float t) const;
    void project(const V3& p, float& x, float& y, float& depth) const;
    void emitStrand(const Inst& inst, const Strand& s, float lengthFrac, float bright);
    void emitTransfer(const Inst& inst);

    Quality quality_;
    std::unordered_map<uint64_t, Inst> insts_;
    std::vector<Segment> segs_;
    std::vector<V3> pts_;  // scratch
    float time_ = 0.0f;
    float energy_ = 0.0f;
    float aspect_ = 1.6f;
    float extent_ = 0.5f, extentTarget_ = 0.5f;
    float yaw_ = 0.0f, pitch_ = 0.0f;
    int strands_ = 0;
    int perInstTarget_ = 0;
    bool first_ = true;
};

// Hue-preserving tone map shared by the GL composite (mirrored in GLSL), the software fallback and the
// preview: black stays exactly black; colour is kept until density is very high, where it eases toward a
// pale lavender-white (the reference's bright intersections).
Rgb toneMap(Rgb accum, float exposure);

}  // namespace ih1::visual
