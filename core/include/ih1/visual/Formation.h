// IH1 formation (render-API independent), V2.1: a three-dimensional force field.
//
// What it is (V2.1 feedback 9; reference video = palette / texture authority; film force-field CGI = character):
//  * Every committed FX instance is an energy node: a soft magnetic dipole with a seeded axis, placed along an
//    understated path from bottom-left (input) to top-right (output). Nodes are offset sideways and in depth
//    from that path by their seed, so the structure finds its own route instead of drawing a line, tree or T.
//  * Filaments are field lines of the combined field (all dipoles + a weak input->output flow + slow,
//    divergence-free curl noise), integrated in 3-D with midpoint (RK2) steps from stable seeded start points:
//      - arcs: dipole field lines that leave a node, bend around its energy centre and return or link to a
//        neighbour (magnetic tension, bending around energy centres);
//      - transmission: bundles drawn from node i towards node i+1 (and sometimes i+2) through the field,
//        colour-graded from one family colour to the next (energy carried A -> B, neuron/web connections);
//      - wisps: short, faint curl-noise streamlines (the reference's fine smoke texture).
//    Strands keep their identity: the seeds never change, only the slowly evolving field moves them.
//  * Lighting: sharp additive cores (bright near strands are drawn 2 px), depth fog towards indigo, local
//    highlights near node cores; bloom is a secondary halo in the renderer. Glows are emitted separately: a
//    faint deep-violet field response around the nodes that the renderer adds under the strands. No nodes,
//    no glow: the void stays black. No stars, dust or decoration exist anywhere.
//  * Audio (committed-chain telemetry only; analysis snapshots, never audio buffers): energy raises emission
//    and field strength, transients send pulses along the transmission paths that cascade node to node, low
//    band adds a slow heave, high band lights the fine wisps, stereo balance leans the field slightly.
//  * SOLO: `monochrome` fades every colour to a cool grey; colours return when it falls back to 0.
//  * COMMIT: a packet of the FX's own field lines travels from the workflow column into its node, then the
//    node's filaments grow out of it. A transfer request waits (up to 2 s) until its instance appears in the
//    chain, so it can never be lost to frame / timer ordering.
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ih1/visual/FieldVolume.h"
#include "ih1/visual/Palette.h"

namespace ih1::visual {

struct InstanceInput {
    uint64_t id = 0;
    Rgb colour{0.6f, 0.5f, 1.0f};  // display colour (fxColour of its type and duplicate shade)
    uint32_t seed = 1;
    float level = 0.0f;  // audio RMS at this instance's output (linear)
    bool enabled = true; // V4: false = bypassed (its part of the field greys and dims, the medium stays continuous)
};

struct TransferRequest {
    uint64_t id = 0;        // committed instance that was just created by COMMIT
    float originY = 0.35f;  // where the packet enters, as a fraction of the field height (left edge)
};

// Smoothed analysis of the committed chain's output (computed on the UI side from telemetry atomics).
struct AudioFeatures {
    float energy = 0.0f;     // smoothed RMS, roughly 0..1
    float transient = 0.0f;  // 0..1 onset strength this frame (0 most of the time)
    float low = 0.0f, mid = 0.0f, high = 0.0f;  // smoothed band energies, roughly 0..1
    float balance = 0.0f;    // -1 (left) .. 1 (right)
};

struct FrameInput {
    std::vector<InstanceInput> chain;        // committed chain in processing order
    std::vector<TransferRequest> transfers;  // COMMIT events since the last frame
    AudioFeatures audio;
    float monochrome = 0.0f;                 // 0..1 (SOLO): fade the formation to grey
    float dt = 1.0f / 60.0f;
    float aspect = 1.6f;                     // field width / height
};

struct Segment {
    float x0, y0, x1, y1;  // field coordinates: x in [0, aspect], y in [0, 1] (y down)
    float r, g, b;         // additive intensity
    float w = 1.0f;        // width hint in pixels (1 = fine core, 2 = near / bright filament)
};

// A soft radial glow in field coordinates (the formation-coupled field response in empty space).
struct Glow {
    float x, y, radius;  // radius in field-height units
    float r, g, b;       // peak additive intensity
};

enum class Quality : uint8_t { Low = 0, Medium = 1, High = 2 };

class Formation {
public:
    explicit Formation(Quality q = Quality::Medium);
    void setQuality(Quality q) { quality_ = q; }
    Quality quality() const { return quality_; }

    void update(const FrameInput& in);
    const std::vector<Segment>& segments() const { return segs_; }
    const std::vector<Glow>& glows() const { return glows_; }
    // Persistence of the accumulation buffer for this frame's dt (light temporal smoothing only).
    float decay(float dt) const;

    // V3 Phase 3A: the field-volume description of this frame (see FieldVolume.h). Always produced.
    const FieldScene& scene() const { return scene_; }
    // The V2.1 strand tracing (segments()) is only needed by tests and comparisons now; renderers switch it off.
    void setTraceStrands(bool on) { traceStrands_ = on; }
    int strandCount() const { return strands_; }
    int instanceCount() const { return static_cast<int>(insts_.size()); }
    bool transferActive() const;
    int pendingTransfers() const { return static_cast<int>(pending_.size()); }
    // V3 Phase 3: explicit per-instance visual lifecycle.
    //   Probation    : a new instance just appeared in the chain; for up to kProbation it is invisible while a
    //                  COMMIT transfer for it may still arrive (UI/GL ordering). Without one (preset load) it then
    //                  fades in as Integrated.
    //   Transferring : COMMIT accepted: only the travelling packet is visible. The instance contributes nothing
    //                  to the stable formation (no node field, no strands, no glow, no bundles towards it).
    //   Integrated   : the packet arrived (or no transfer): its stable contribution and light fade in.
    // Audio correctness never depends on any of this; an editor reopened mid-transfer starts Integrated.
    enum class Life : uint8_t { Integrated = 0, Probation = 1, Transferring = 2 };
    static constexpr float kProbation = 0.25f;
    int lifeOf(uint64_t id) const;            // Life as int, -1 if unknown
    float stablePresence(uint64_t id) const;  // 0..1 contribution to the stable formation (0 unless Integrated)
    int integratedByTransfer() const { return integratedCount_; }  // transfers that completed and integrated
    float time() const { return time_; }
    float monochrome() const { return mono_; }

    // world -> field projection (tests)
    struct V3 { float x = 0, y = 0, z = 0; };
    void projectPoint(const V3& p, float& x, float& y, float& depth) const { project(p, x, y, depth); }
    V3 nodePosition(uint64_t id) const;

private:
    struct Strand {
        V3 start;            // offset from the node centre (unit sphere direction scaled by radius)
        float gain = 1.0f;
        float phase = 0.0f;
        float lenScale = 1.0f;
        uint8_t kind = 0;    // 0 arc, 1 transmission, 2 wisp
        uint8_t target = 1;  // transmission: 1 = next node, 2 = the one after
        int8_t dir = 1;      // arcs: +1 along the field, -1 against it
        uint8_t steps = 24;
    };
    struct Pulse { float age = 0.0f, strength = 1.0f; bool cascade = true; };
    struct Inst {
        uint64_t id = 0;
        Rgb colour;
        uint32_t seed = 1;
        V3 anchor, target;          // node position (smoothed) and layout target
        V3 axis{0, 1, 0}, axisBase{0, 1, 0};
        float axisPhase = 0.0f, breathe = 0.0f;
        float presence = 0.0f;      // fades in on arrival, out on delete
        float grow = 0.0f;          // strand length fraction (grows after arrival)
        float hold = 0.0f;          // seconds until it may grow (packet in flight)
        float bright = 0.0f;        // smoothed audio level
        bool enabledIn = true;      // V4: committed instance enabled (input)
        float enabledAmt = 1.0f;    // V4: smoothed enabled amount (0.25 s)
        float levelSlow = 0.0f;     // slower level (per-node onset detection)
        bool inChain = false;
        int index = 0;
        std::vector<Strand> strands;
        std::vector<std::vector<V3>> trail;  // per strand: last displayed points (temporal smoothing)
        std::vector<Pulse> pulses;
        Life life = Life::Integrated;
        float probation = 0.0f;
        // transfer packet
        bool transfer = false;
        float tTransfer = 0.0f;
        float originY = 0.35f;
        V3 origin;                  // world-space start of the packet
    };
    struct Node { V3 p, axis; float strength, radius; int inst; };
    struct Pending { TransferRequest req; float age = 0.0f; };

    void layout(const FrameInput& in);
    void buildStrands(Inst& inst, int count);
    V3 field(const V3& p, float t) const;
    V3 curl(const V3& p, float t) const;
    void project(const V3& p, float& x, float& y, float& depth) const;
    V3 unproject(float fx, float fy, float worldZ) const;
    void emitStrand(Inst& inst, int nodeIndex, int strandIndex, float lengthFrac, float dt);
    void emitPolyline(const std::vector<V3>& pts, const Rgb& c0, const Rgb& c1, float gain, const Inst* pulsesOf, bool fineCore);
    void emitTransfer(const Inst& inst);
    Rgb shade(const Rgb& c) const;

    Quality quality_;
    std::unordered_map<uint64_t, Inst> insts_;
    std::vector<uint64_t> order_;  // chain order (ids)
    std::vector<Node> nodes_;      // per frame, chain order
    std::vector<Pending> pending_;
    int integratedCount_ = 0;
    FieldScene scene_;
    bool traceStrands_ = true;
    void buildScene();
    std::vector<Segment> segs_;
    std::vector<Glow> glows_;
    std::vector<V3> pts_;          // scratch
    float time_ = 0.0f;
    float energy_ = 0.0f, low_ = 0.0f, high_ = 0.0f, balance_ = 0.0f, transient_ = 0.0f;
    // V4 audio response: mid band, onset density (fine turbulence) and pulses travelling along the whole spine
    float mid_ = 0.0f, onsetRate_ = 0.0f;
    struct SpinePulse { float age = 0.0f, amp = 0.0f; };
    std::vector<SpinePulse> spinePulses_;
    float mono_ = 0.0f;
    float aspect_ = 1.6f;
    float extent_ = 0.5f, extentTarget_ = 0.5f;
    float yaw_ = 0.0f, pitch_ = 0.0f;
    int strands_ = 0;
    bool first_ = true;
};

// Hue-preserving tone map shared by the GL composite (mirrored in GLSL), the software fallback and the
// preview: black stays exactly black; colour is kept until density is very high, where it eases toward a
// pale lavender-white (the reference's bright intersections).
Rgb toneMap(Rgb accum, float exposure);

}  // namespace ih1::visual
