// IH1 formation simulation (render-API independent). Produces thin additive line segments each frame;
// the plug-in draws them with OpenGL into a decaying float accumulation buffer, the offline preview tool
// rasterises them on the CPU. Design targets come from docs/VISUAL_REFERENCE_ANALYSIS.md:
//   near-black field, narrow blue->violet->magenta palette at low saturation, asymmetric luminous mass with
//   long tendrils, brightness from overlapping translucent strands, slow continuous motion.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ih1::visual {

struct Rgb { float r = 0, g = 0, b = 0; };

// Palette: hue (degrees) per FX type id, restricted to the measured 215..325 degree arc.
float hueForType(const std::string& typeId);
Rgb hsvToRgb(float hDeg, float s, float v);

struct InstanceInput {
    uint64_t id = 0;
    float hue = 260.0f;     // degrees
    uint32_t seed = 1;
    float level = 0.0f;     // audio level at this instance's output, linear RMS
};

struct FrameInput {
    std::vector<InstanceInput> chain;  // committed chain in processing order
    bool hasStaged = false;
    float stagedLevel = 0.0f;
    float solo = 0.0f;                 // 0..1 (committed formation fades/desaturates)
    float outLevel = 0.0f;             // overall output RMS
    float low = 0.0f, mid = 0.0f, high = 0.0f;  // coarse band RMS
    float dt = 1.0f / 60.0f;
    float aspect = 1.6f;               // field width / height
};

struct Segment {
    float x0, y0, x1, y1;  // field coordinates: x in [0, aspect], y in [0, 1] (y down)
    float r, g, b;         // linear, premultiplied intensity (additive)
};

// Where each instance's core is drawn (for hit-testing labels / drag in the UI).
struct CoreInfo {
    uint64_t id;
    float x, y;
    Rgb colour;
    float brightness;
};

class Formation {
public:
    explicit Formation(int maxParticles = 26000);
    void update(const FrameInput& in);
    const std::vector<Segment>& segments() const { return segs_; }
    const std::vector<CoreInfo>& cores() const { return coreInfo_; }
    // Suggested per-frame decay of the accumulation buffer (trail persistence) for this frame's dt.
    float decay(float dt) const;
    int liveParticles() const { return live_; }

private:
    struct Particle {
        float x = 0, y = 0, px = 0, py = 0;
        float age = 0, life = 1;
        uint64_t owner = 0;  // instance id (0 = staged preview)
        uint8_t kind = 0;    // 0 filament, 1 tendril, 2 staged
        float gain = 1;
        bool alive = false;
    };
    struct Core {
        float x = 0, y = 0, tx = 0, ty = 0;  // current and target position
        Rgb colour;
        float hue = 260;
        float bright = 0;    // smoothed audio brightness
        float presence = 0;  // 0..1 fade in on commit / out on delete
        float spawnAcc = 0;
        uint32_t seed = 1;
        bool inChain = false;
        int index = 0;
    };

    void layout(const FrameInput& in);
    void spawn(Particle& p, const Core& c, uint64_t id, uint8_t kind, float spread);
    uint32_t rnd();
    float frand() { return static_cast<float>(rnd() >> 8) * (1.0f / 16777216.0f); }
    float gauss();

    std::vector<Particle> parts_;
    std::unordered_map<uint64_t, Core> cores_;
    Core staged_;
    std::vector<Segment> segs_;
    std::vector<CoreInfo> coreInfo_;
    float time_ = 0.0f;
    float energy_ = 0.0f, low_ = 0.0f, high_ = 0.0f, solo_ = 0.0f, stagedBright_ = 0.0f, stagedPresence_ = 0.0f;
    float aspect_ = 1.6f;
    int live_ = 0;
    uint32_t rng_ = 0x1234567u;
    int chainSize_ = 0;
};

// Shared tone map (mirrored in the plug-in's GLSL): soft-knee exposure that pushes dense overlaps toward a
// pale lavender-white while keeping the black floor black.
Rgb toneMap(Rgb accum, float exposure);

// 3-D gradient noise in [-1, 1] (Perlin), used for the curl flow field.
float noise3(float x, float y, float z);

}  // namespace ih1::visual
