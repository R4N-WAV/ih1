// IH1 field volume (V3 Phase 3A): the formation rendered as a 3-D force-field volume instead of drawn strands.
//
// Feedback 3: the V2.1 formation read as 2-D lines plus foggy blur. Here nothing is a line primitive and nothing is
// a blur. The image is integrated along camera rays through an emission / absorption volume (front-to-back), so
// structure, light and depth come from the field itself:
//   * every committed instance is a magnetic dipole. Its light lives on the surfaces its field lines lie on: for a
//     dipole r = L sin^2(theta), so the "L-shell" coordinate L = r / sin^2(theta) is constant along a field line.
//     Thin luminous skins at log-spaced L values, broken up by an irregular azimuthal pattern, give nested,
//     arcing sheets of light that occupy space -- the magnetosphere picture, not a drawing of curves;
//   * the skins are concentrated near the poles (where field lines converge, i.e. where the field is strong) and
//     fade with distance, so each node has a defined luminous core and softer, intermittent periphery;
//   * transmission between consecutive nodes is a twisted flux sheet along the link, colour-graded between the
//     two family colours; audio onsets travel along it as brightness pulses;
//   * the whole field is slowly domain-warped by a smooth, deterministic flow (coherent motion, no swirl/twitch);
//   * emission doubles as density: bright structure absorbs light from behind it (local occlusion, depth layering),
//     far structure dims and cools towards indigo. Rays only march inside the formation's bounding sphere, so the
//     void stays exactly black. Bloom is a faint finishing accent in the compositor, not the structure.
//   * a COMMIT transfer is a separate, compact packet with its own small dipole skins; it is not part of the stable
//     field until the simulation integrates it (Formation::Life).
//
// The same mathematics exists twice: renderVolume() (CPU reference: software fallback, offline preview, tests) and
// fieldFragmentShader() (GLSL 1.50, the GPU path). Both read the scene through packScene(), so they cannot drift.
#pragma once

#include <array>
#include <string>
#include <vector>

namespace ih1::visual {

constexpr int kMaxFieldNodes = 32;
constexpr int kMaxSpine = 16;  // V4: control points of the continuous field's spine (the chain, resampled)
constexpr int kMaxFieldLinks = 32;
constexpr int kMaxFieldPackets = 4;

struct FieldNode {
    float p[3] = {0, 0, 0};
    float axis[3] = {0, 1, 0};
    float u[3] = {1, 0, 0}, v[3] = {0, 0, 1};  // perpendicular frame around the axis (azimuth)
    float col[3] = {0.6f, 0.5f, 1.0f};         // display colour (already monochrome-faded for SOLO)
    float strength = 1.0f;                     // 0 = no contribution (not integrated)
    float radius = 0.18f;                      // core radius (world units)
    float influence = 1.0f;                    // reach of the skins (world units)
    float seed = 0.0f;                         // 0..1, stable per instance
    float enabled = 1.0f;                      // V4: 1 = enabled, 0 = bypassed (its stretch of the field greys and dims)
};
struct FieldLink {
    int a = 0, b = 1;
    float strength = 0.0f;
    float pulse = -1.0f, pulseAmp = 0.0f;  // travelling onset pulse position 0..1 along the link (< 0 = none)
    float u[3] = {1, 0, 0}, v[3] = {0, 0, 1};
    float seed = 0.0f;
};
struct FieldPacket {
    float p[3] = {0, 0, 0}, axis[3] = {0, 1, 0}, u[3] = {1, 0, 0}, v[3] = {0, 0, 1};
    float col[3] = {0.6f, 0.5f, 1.0f};
    float env = 0.0f, radius = 0.12f, seed = 0.0f;
};
struct FieldScene {
    std::vector<FieldNode> nodes;
    std::vector<FieldLink> links;
    std::vector<FieldPacket> packets;
    float yaw = 0.0f, pitch = 0.0f, aspect = 1.6f, time = 0.0f;
    float zoom = 1.0f;  // V5: camera framing (<= 1 pulls back so the dense medium never meets the panel edges)
    float energy = 0.0f, high = 0.0f;
    // V4 multi-dimensional audio response: bands, stereo lean, onset density (fine turbulence), travelling pulses
    float low = 0.0f, mid = 0.0f, balance = 0.0f, turbulence = 0.0f;
    float pulseS[2] = {-1.0f, -1.0f}, pulseA[2] = {0.0f, 0.0f};
    float detail = 1.0f;  // V4: 1 = full filament sharpness; lower for few ray steps (softer lines instead of sparkle)
    float radius = 0.3f;  // V4: base radius of the continuous medium around the spine (grows with the chain)
    float bound[4] = {0, 0, 0, 0};
    // bounding sphere centre xyz + radius (radius 0 = nothing to draw)
    void computeBound();
};

// Camera shared with Formation::project(): pinhole at (0, 0, kCamDist) looking down -z after yaw / pitch; field
// coordinates x in [0, aspect], y in [0, 1] (y down); the image centre is at x = 0.54 * aspect.
constexpr float kFieldCamDist = 4.2f;
constexpr float kFieldScale = 0.23f;
// V5 Phase 1.3 (mood): the V4 palette is kept; only the emotional weighting moves. kMoodBg lifts the near-black void
// toward the existing plum (more atmosphere, less threat), kMoodIndigo desaturates the deep indigo that distant
// structure cools toward (the darkest violet was the ominous part). Compared as variants A (V4) / B / C in
// docs/V5_IMPLEMENTATION.md; B was chosen. setMood() exists for the comparison renders (formation_preview) only.
struct Mood { float bg = 1.45f; float indigo = 0.65f; };
const Mood& mood();
void setMood(const Mood& m);

// Flat uniform layout shared by both implementations (vec4 slots).
struct PackedScene {
    std::array<float, 4 * kMaxFieldNodes> nodeP{}, nodeA{}, nodeU{}, nodeV{}, nodeC{};
    std::array<float, 4 * kMaxFieldLinks> linkA{}, linkB{}, linkU{}, linkV{};
    std::array<float, 4 * kMaxFieldPackets> packP{}, packA{}, packU{}, packV{}, packC{};
    std::array<float, 4 * kMaxSpine> spine{};  // xyz + spine coordinate s (0 = input, 1 = output)
    int nodes = 0, links = 0, packets = 0, spineN = 0;
    float cam[4] = {0, 0, 1.6f, 0};    // yaw, pitch, aspect, time
    float audio[4] = {0, 0, 0, 0};     // energy, high, low, mid
    float audio2[4] = {0, 0, 0.3f, 1}; // balance, turbulence, radius, detail
    float pulse[4] = {-1, 0, -1, 0};   // s0, a0, s1, a1
    float bound[4] = {0, 0, 0, 0};
    float view[4] = {1, 0, 0, 0};      // V5: zoom (auto-framing), reserved
};
PackedScene packScene(const FieldScene& s);

// V4 background: the near-black plum / navy void (linear RGB, before tone mapping), slightly lighter towards the
// middle of the field area. Never black, never decorated (no stars, dots or nebula).
void backgroundColour(float u, float v, float energy, float& r, float& g, float& b);
// CPU reference renderer: linear RGB (3 floats per pixel, row-major, y down). `steps` ray-march samples.
// threads: 0 = up to 4 worker threads (offline tools, tests); 1 = the calling thread only (the plug-in's software
// fallback, which must never compete with the UI or audio threads for CPU).
// jitterFrame: per-frame sub-pixel / step jitter index (temporal accumulation reconstructs the full resolution).
void renderVolume(const FieldScene& s, int w, int h, std::vector<float>& rgb, int steps = 40, int threads = 0, int jitterFrame = 0);

// GLSL 1.50 fragment shader implementing exactly renderVolume()'s per-pixel function. Inputs: vUv (0..1, y up);
// uniforms as named in packScene's layout (see the source); output: linear RGB.
const std::string& fieldFragmentShader();

}  // namespace ih1::visual
