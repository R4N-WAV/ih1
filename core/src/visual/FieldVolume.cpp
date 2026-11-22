#include "ih1/visual/FieldVolume.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>

// V4 Phase 5: the formation is ONE continuous force-field medium.
//
// V3 lit a magnetic dipole per committed FX (skins, flux body and a defined core around each node) and joined the
// nodes with twisted link sheets. The owner's V4 feedback: no centre / orb / halo per effect, a whole interconnected
// web rather than a few strands, material weight instead of fog, vibrancy, no squares, a plum void instead of black.
// The model is therefore rebuilt around a spine (the committed chain, input lower-left -> output upper-right):
//   * envelope: a smooth tube around the spine, radius growing with the chain and broken up by slow noise. It is the
//     MAX (not the sum) over spine segments, so joints between effects never brighten: nothing is radial around a node;
//   * material, anisotropic along the local flow direction (structures elongate along the spine like a woven web):
//       - fine filaments: where two thin ridge sheets of a warped noise field intersect;
//       - ribbons: single thin ridge sheets at a coarser scale (translucent, folded);
//       - veils: broad, soft density layers that also absorb (depth, shadow, weight);
//   * colour is blended along the spine coordinate from the FX palette (not radially), its chroma lifted where the
//     field is energetic; white-lilac only in the hottest filaments; a faint cyan / pink iridescence at filament
//     edges. A bypassed FX greys and dims its stretch of the field; the medium stays continuous;
//   * audio: energy = slow overall emission (with a floor: a quiet field stays alive), low band = large-scale veil
//     motion, mid = ribbon brightness, high = filament brightness, onset density = fine turbulence, transients =
//     pulses travelling along the spine, stereo balance = lateral lean;
//   * light: emission + absorption along the ray, depth cools towards indigo, a faint scatter halo around the field.
// One model, two implementations: CPU reference (software fallback, preview, tests) and generated GLSL.
namespace ih1::visual {

namespace {
// Tuning shared by both implementations (the GLSL source below is generated with these values).
constexpr float kFilGain = 22.0f;      // fine filaments
constexpr float kRibGain = 4.5f;      // ribbons / sheets
constexpr float kVeilGain = 0.55f;    // broad veils (emission)
constexpr float kVeilAbsorb = 1.6f;   // broad veils (absorption: depth and shadow)
constexpr float kAbsorb = 0.40f;      // density per unit of emitted luminance
constexpr float kFog = 0.38f;         // depth attenuation beyond the focal plane
constexpr float kHalo = 0.045f;
constexpr float kTendril = 3.2f;      // long filaments reaching out of the medium into space       // faint scatter around the field (atmosphere near the formation)
constexpr float kChroma = 1.45f;      // chroma lift of the FX palette inside the medium (vibrancy)
constexpr float kPacketGain = 1.0f;   // COMMIT transfer packets

struct V { float x, y, z; };
inline V add(V a, V b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V sub(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V mul(V a, float k) { return {a.x * k, a.y * k, a.z * k}; }
inline V vmul(V a, V b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline float dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V ld(const float* p) { return {p[0], p[1], p[2]}; }
inline V vmix(V a, V b, float t) { return add(a, mul(sub(b, a), t)); }
inline float smooth(float e0, float e1, float x) {
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
inline float lum(V c) { return 0.30f * c.x + 0.55f * c.y + 0.15f * c.z; }
inline V normv(V a) { const float l = std::sqrt(dot(a, a)); return l > 1e-6f ? mul(a, 1.0f / l) : V{1, 0, 0}; }

// smooth 3-D value noise in [0, 1] (the GLSL source uses the same hash and interpolation)
inline float hash3(float x, float y, float z) {
    const float h = std::sin(x * 127.1f + y * 311.7f + z * 74.7f) * 43758.5453f;
    return h - std::floor(h);
}
float noise3(V p) {
    const float ix = std::floor(p.x), iy = std::floor(p.y), iz = std::floor(p.z);
    float fx = p.x - ix, fy = p.y - iy, fz = p.z - iz;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    fz = fz * fz * (3.0f - 2.0f * fz);
    auto h = [&](float a, float b, float c) { return hash3(ix + a, iy + b, iz + c); };
    const float x00 = h(0, 0, 0) + fx * (h(1, 0, 0) - h(0, 0, 0)), x10 = h(0, 1, 0) + fx * (h(1, 1, 0) - h(0, 1, 0));
    const float x01 = h(0, 0, 1) + fx * (h(1, 0, 1) - h(0, 0, 1)), x11 = h(0, 1, 1) + fx * (h(1, 1, 1) - h(0, 1, 1));
    const float y0 = x00 + fy * (x10 - x00), y1 = x01 + fy * (x11 - x01);
    return y0 + fz * (y1 - y0);
}

V warp(V q, float t, float low, float turb) {
    V w{std::sin(1.3f * q.y + 0.07f * t + 1.1f), std::sin(1.1f * q.z + 0.05f * t + 2.3f), std::sin(1.2f * q.x + 0.06f * t + 0.4f)};
    const V qs = mul(q, 2.2f);
    V w3{noise3(add(qs, V{0.0f, 0.0f, 0.013f * t})) - 0.5f, noise3(add(qs, V{5.2f, 1.3f, 0.011f * t})) - 0.5f, noise3(add(qs, V{9.1f, 4.7f, 0.012f * t})) - 0.5f};
    return add(q, add(mul(w, 0.10f + 0.10f * low), mul(w3, 0.26f + 0.18f * turb)));
}

// One dipole's light at q (COMMIT transfer packets only; the stable formation has no per-effect dipoles any more).
V packet(V q, V p, V axis, V u, V v, V col, float strength, float radius, float infl, float seed, float t) {
    const V r = sub(q, p);
    const float d2 = dot(r, r);
    if (d2 > infl * infl || strength <= 0.0f) return {0, 0, 0};
    const float d = std::sqrt(d2) + 1e-4f;
    const float ct = dot(r, axis) / d;
    const float s2 = std::max(1.0f - ct * ct, 0.02f);
    const float lu = std::log(std::max(d / s2, 1e-3f) / radius);
    const float c0 = 0.5f + 0.5f * std::cos(6.2832f * (lu * 2.2f + seed * 6.2832f - t * 0.14f));
    const float c2 = c0 * c0, shell = c2 * c2;
    const float phi = std::atan2(dot(r, v), dot(r, u));
    const float br = 0.5f + 0.5f * std::cos(3.0f * phi + 2.0f * lu + t * 0.2f);
    const float env = std::exp(-d2 / (0.25f * infl * infl));
    return mul(col, strength * env * shell * (0.2f + 0.8f * br * br) * 5.0f * kPacketGain);
}

struct Medium {
    V emit{0, 0, 0};
    float absorb = 0.0f;
};

Medium medium(const PackedScene& P, V q, float t) {
    Medium m;
    const float en = P.audio[0], hi = P.audio[1], lo = P.audio[2], mi = P.audio[3];
    const float bal = P.audio2[0], turb = P.audio2[1], R0 = P.audio2[2];
    q.x -= bal * 0.12f;  // stereo lean
    // the outline follows a cheap, smooth warp (sine terms only); the full warp (with noise) is evaluated only where
    // the material can be visible (V4 performance: most of the bound is empty halo)
    const V pc = add(q, mul(V{std::sin(1.3f * q.y + 0.07f * t + 1.1f), std::sin(1.1f * q.z + 0.05f * t + 2.3f), std::sin(1.2f * q.x + 0.06f * t + 0.4f)}, 0.16f + 0.12f * lo));
    // ---- spine query on the warped position: envelope (max over segments: joints never brighten), coordinate, tangent
    float best = 0.0f, broad = 0.0f, sS = 0.0f, sW = 0.0f;
    V tan{0, 0, 0};
    const float R = R0 * (0.55f + 1.0f * noise3(add(mul(q, 0.42f), V{0.0f, 0.0f, 0.01f * t})));
    const float inv = 1.0f / (R * R);
    for (int j = 0; j + 1 < P.spineN; ++j) {
        const V a = ld(&P.spine[static_cast<size_t>(4 * j)]), b = ld(&P.spine[static_cast<size_t>(4 * j + 4)]);
        const V ab = sub(b, a);
        const float L2 = std::max(dot(ab, ab), 1e-6f);
        const float u = std::clamp(dot(sub(pc, a), ab) / L2, 0.0f, 1.0f);
        const V w = sub(pc, add(a, mul(ab, u)));
        const float d2 = dot(w, w);
        const float sj0 = P.spine[static_cast<size_t>(4 * j + 3)], sj1b = P.spine[static_cast<size_t>(4 * j + 7)];
        const float sm = sj0 + u * (sj1b - sj0) - 0.78f;
        const float swell = 1.0f + 0.65f * std::exp(-sm * sm / 0.08f);  // the body is fuller around the hot mass
        const float e = std::exp(-d2 * inv / (swell * swell));
        best = std::max(best, e);
        const float eb = std::exp(-d2 * inv * 0.30f / (swell * swell));
        broad = std::max(broad, eb);
        const float sj = P.spine[static_cast<size_t>(4 * j + 3)], sj1 = P.spine[static_cast<size_t>(4 * j + 7)];
        sS += eb * (sj + u * (sj1 - sj));
        sW += eb;
        tan = add(tan, mul(ab, eb / std::sqrt(L2)));
    }
    // everything fades out well before the ray-march bound (no visible edge of the bounding sphere)
    {
        const V bc{P.bound[0], P.bound[1], P.bound[2]};
        const float rr = std::sqrt(dot(sub(q, bc), sub(q, bc))) / std::max(P.bound[3], 1e-3f);
        const float fade = 1.0f - smooth(0.6f, 0.95f, rr);
        best *= fade;
        broad *= fade;
    }
    if (broad < 2e-3f) return m;
    const V plum{0.42f, 0.30f, 0.62f};
    m.emit = mul(plum, broad * kHalo * (0.45f + 0.8f * en));  // (the halo is part of the void's atmosphere)
    if (best < 0.01f && broad < 0.25f) return m;  // nothing but faint halo here: skip the material
    const V p = warp(q, t, lo, turb);
    const float s = sW > 1e-6f ? sS / sW : 0.5f;
    const V T = normv(tan);
    // ---- material: flow-aligned multi-scale structure
    const float along = dot(p, T);
    const V pa = add(sub(p, mul(T, along)), mul(T, along * 0.32f));  // compressed along the flow: elongated structure
    const float n1 = noise3(add(mul(pa, 4.3f), V{0.0f, 0.021f * t, 1.7f}));
    const float n2 = noise3(add(mul(pa, 6.1f), V{3.1f, 0.6f, 0.017f * t}));
    const float n3 = noise3(add(mul(pa, 1.9f), V{7.7f, 0.013f * t, 2.9f}));
    const float n4 = noise3(add(mul(p, 1.05f), V{1.3f, 4.1f, 0.008f * t}));
    // a second, isotropic filament family crossing the flow-aligned one: tangles instead of parallel streaks
    const float n5 = noise3(add(mul(p, 5.6f), V{2.2f, 0.019f * t, 5.3f}));
    const float n6 = noise3(add(mul(p, 8.3f), V{6.4f, 3.3f, 0.023f * t}));
    const float r1 = 1.0f - std::fabs(2.0f * n1 - 1.0f);
    const float r2 = 1.0f - std::fabs(2.0f * n2 - 1.0f);
    const float r3 = 1.0f - std::fabs(2.0f * n3 - 1.0f);
    const float r1s = r1 * r1, r1q = r1s * r1s, r1e = r1q * r1q;          // r1^8
    const float r2s = r2 * r2, r2q = r2s * r2s;                           // r2^4
    const float r5 = 1.0f - std::fabs(2.0f * n5 - 1.0f), r6 = 1.0f - std::fabs(2.0f * n6 - 1.0f);
    const float r5s = r5 * r5, r5q = r5s * r5s, r6s = r6 * r6, r6q = r6s * r6s;
    float fil = r1e * r1s * r2q * r2s + 0.8f * r5q * r5q * r5s * r6q * r6s;  // two families of thin crossing lines
    // low detail (few ray steps): thin lines would be sampled as sparkle; blend towards a softer, wider version
    const float detail = P.audio2[3];
    if (detail < 1.0f) fil = fil * detail + (1.0f - detail) * 0.35f * (r1q * r2s + 0.8f * r5q * r6s);
    const float r3s = r3 * r3, r3q = r3s * r3s;
    const float rib = r3q * r3s;                                          // r3^6: folded translucent sheets
    const float tendril = r3q * r3q * r3q * r3q * r1q;                    // r3^16 * r1^4: long thin threads into space
    const float veil = smooth(0.50f, 0.90f, n4);                          // broad soft layers
    // the hot region of the whole formation (towards the output, as in the reference), plus travelling pulses
    float pulse = 0.0f;
    for (int k = 0; k < 2; ++k) {
        const float ps = P.pulse[2 * k], pa2 = P.pulse[2 * k + 1];
        if (ps < 0.0f) continue;
        const float z = (s - ps) / 0.09f;
        pulse += pa2 * std::exp(-z * z);
    }
    const float massZ = (s - 0.78f) / 0.28f;
    const float mass = std::exp(-massZ * massZ);
    const float hot = 0.50f + 0.55f * smooth(0.1f, 1.0f, s) + 0.9f * mass + 1.4f * pulse;
    const float alive = 0.32f + 0.85f * en;  // a quiet field stays faintly alive
    // ---- colour and presence along the spine (weighted averages: no gaps, no radial weighting)
    V col{0, 0, 0};
    float cw = 0.0f, enabled = 0.0f, strength = 0.0f;
    const float sig = 0.9f / std::max(1.0f, static_cast<float>(P.nodes));
    for (int i = 0; i < P.nodes; ++i) {
        const float z = (s - P.nodeP[static_cast<size_t>(4 * i + 3)]) / sig;
        const float g = std::exp(-z * z) + 1e-6f;
        const float st = std::max(P.nodeA[static_cast<size_t>(4 * i + 3)], 0.0f);
        col = add(col, mul(ld(&P.nodeC[static_cast<size_t>(4 * i)]), g * (0.05f + st)));
        enabled += g * P.nodeC[static_cast<size_t>(4 * i + 3)];
        strength += g * st;
        cw += g;
    }
    if (cw <= 1e-5f) return m;
    const float colW = cw * 0.05f + strength;
    col = mul(col, 1.0f / std::max(colW, 1e-6f));
    enabled /= cw;
    strength = std::min(1.0f, strength / cw);
    const V grey{0.52f, 0.52f, 0.58f};
    col = vmix(grey, col, enabled);
    const float dim = 0.30f + 0.70f * enabled;
    const float l0 = lum(col);
    const float chroma = kChroma * (0.85f + 0.3f * en) * (0.4f + 0.6f * enabled);
    V vivid = add(mul(V{l0, l0, l0}, 1.0f - chroma), mul(col, chroma));
    vivid = V{std::max(vivid.x, 0.0f), std::max(vivid.y, 0.0f), std::max(vivid.z, 0.0f)};
    const float edge = smooth(0.55f, 0.75f, r1) * (1.0f - smooth(0.85f, 0.97f, r1));
    const float ip = std::sin(6.0f * n2 + 3.0f * along + 0.3f * t);
    const V iris = vmix(V{0.55f, 0.88f, 1.0f}, V{1.0f, 0.62f, 0.90f}, 0.5f + 0.5f * ip);
    // emission per layer: the body (filaments, ribbons, veils) inside the envelope, threads in the broad halo
    const float base = strength * dim * hot * alive / (1.0f + 0.05f * static_cast<float>(std::max(0, P.nodes - 1)));
    const float E = best * base;
    const float eFil = fil * kFilGain * (0.55f + 0.9f * hi) * (1.0f + 0.8f * mass);
    const float eRib = rib * kRibGain * (0.45f + 0.8f * mi);
    const float eVeil = veil * kVeilGain * (0.5f + 0.7f * lo) * (0.6f + 0.8f * mass);
    const float eTen = tendril * kTendril * broad * (1.0f - 0.6f * best) * base * (0.6f + 0.6f * hi);
    V c = mul(vivid, E * (eFil + eRib + eVeil) + eTen);
    // iridescence scales with the colour's own chroma (a greyed SOLO / bypassed field has no stray tints)
    const float cmax = std::max({col.x, col.y, col.z}), cmin = std::min({col.x, col.y, col.z});
    const float csat = std::clamp(2.5f * (cmax - cmin) / std::max(cmax, 1e-4f), 0.0f, 1.0f);
    c = add(c, mul(iris, (E + 0.4f * broad * base) * edge * fil * 2.2f * enabled * csat));
    m.emit = mul(vmix(grey, plum, csat), broad * kHalo * (0.45f + 0.8f * en));  // the halo greys with the field (SOLO)
    const float hl = smooth(0.12f, 0.5f, fil * hot * best * (0.6f + en)) * enabled;
    c = vmix(c, mul(V{0.96f, 0.91f, 1.0f}, lum(c) * 1.25f), hl * 0.55f);
    m.emit = add(m.emit, c);
    m.absorb = kAbsorb * lum(c) + kVeilAbsorb * veil * best * strength * 0.5f;
    return m;
}

V emissionPackets(const PackedScene& P, V q, float t) {
    V e{0, 0, 0};
    for (int i = 0; i < P.packets; ++i) {
        const float* pp = &P.packP[static_cast<size_t>(4 * i)];
        const float* pa = &P.packA[static_cast<size_t>(4 * i)];
        const float* pu = &P.packU[static_cast<size_t>(4 * i)];
        e = add(e, packet(q, ld(pp), ld(pa), ld(pu), ld(&P.packV[static_cast<size_t>(4 * i)]), ld(&P.packC[static_cast<size_t>(4 * i)]), pa[3] * 1.4f, pp[3], pp[3] * 4.5f, pu[3], t * 4.0f));
    }
    return e;
}
}  // namespace

namespace { Mood gMood; }
const Mood& mood() { return gMood; }
void setMood(const Mood& m) { gMood = m; }

void backgroundColour(float u, float v, float energy, float& r, float& g, float& b) {
    // near-black plum / navy, a touch lighter towards the middle-right of the field (where the formation lives)
    const float dx = u - 0.6f, dy = v - 0.48f;
    const float k = std::exp(-(dx * dx * 1.6f + dy * dy * 2.2f));
    const float lift = 0.65f + 0.35f * k * (1.0f + 0.25f * std::clamp(energy, 0.0f, 1.2f));
    r = 0.016f * lift * gMood.bg;
    g = 0.011f * lift * gMood.bg;
    b = 0.026f * lift * gMood.bg;
}

void FieldScene::computeBound() {
    if (nodes.empty() && packets.empty()) { bound[0] = bound[1] = bound[2] = bound[3] = 0.0f; return; }
    float c[3] = {0, 0, 0};
    int n = 0;
    for (const auto& nd : nodes) { for (int k = 0; k < 3; ++k) c[k] += nd.p[k]; ++n; }
    for (const auto& pk : packets) { for (int k = 0; k < 3; ++k) c[k] += pk.p[k]; ++n; }
    for (float& x : c) x /= static_cast<float>(std::max(1, n));
    float r = 0.0f;
    auto grow = [&](const float* p, float reach) {
        const float dx = p[0] - c[0], dy = p[1] - c[1], dz = p[2] - c[2];
        r = std::max(r, std::sqrt(dx * dx + dy * dy + dz * dz) + reach);
    };
    // the medium (and its faint halo) reaches ~3 radii from the spine, + the domain warp
    for (const auto& nd : nodes) grow(nd.p, 4.4f * radius + 0.45f);
    for (const auto& pk : packets) grow(pk.p, pk.radius * 4.5f + 0.25f);
    bound[0] = c[0];
    bound[1] = c[1];
    bound[2] = c[2];
    bound[3] = r;
}

PackedScene packScene(const FieldScene& s) {
    PackedScene P;
    P.nodes = std::min(static_cast<int>(s.nodes.size()), kMaxFieldNodes);
    const int N = P.nodes;
    for (int i = 0; i < N; ++i) {
        const auto& n = s.nodes[static_cast<size_t>(i)];
        const size_t k = static_cast<size_t>(4 * i);
        for (int j = 0; j < 3; ++j) {
            P.nodeP[k + static_cast<size_t>(j)] = n.p[j];
            P.nodeA[k + static_cast<size_t>(j)] = n.axis[j];
            P.nodeU[k + static_cast<size_t>(j)] = n.u[j];
            P.nodeV[k + static_cast<size_t>(j)] = n.v[j];
            P.nodeC[k + static_cast<size_t>(j)] = n.col[j];
        }
        P.nodeP[k + 3] = N > 1 ? static_cast<float>(i) / static_cast<float>(N - 1) : 0.5f;  // spine coordinate
        P.nodeA[k + 3] = n.strength;
        P.nodeU[k + 3] = n.influence;
        P.nodeV[k + 3] = n.seed;
        P.nodeC[k + 3] = std::clamp(n.enabled, 0.0f, 1.0f);
    }
    // spine: the chain in order, resampled to <= kMaxSpine points, extended at both ends; a single effect gets a
    // short spine along the input -> output direction (a small formation, still no centre)
    if (N >= 1) {
        std::vector<V> pts;
        for (int i = 0; i < N; ++i) pts.push_back(ld(s.nodes[static_cast<size_t>(i)].p));
        V dir{0.62f, 0.55f, -0.1f};  // world direction input -> output (lower-left to upper-right on screen)
        if (N >= 2) dir = normv(sub(pts.back(), pts.front()));
        const float ext = 0.25f + 0.6f * s.radius;
        std::vector<V> sp;
        sp.push_back(sub(pts.front(), mul(dir, ext)));
        if (N <= kMaxSpine - 2) {
            for (const auto& p : pts) sp.push_back(p);
        } else {
            for (int k = 0; k < kMaxSpine - 2; ++k) {
                const float x = static_cast<float>(k) * static_cast<float>(N - 1) / static_cast<float>(kMaxSpine - 3);
                const int i0 = std::min(N - 1, static_cast<int>(x));
                const int i1 = std::min(N - 1, i0 + 1);
                sp.push_back(vmix(pts[static_cast<size_t>(i0)], pts[static_cast<size_t>(i1)], x - static_cast<float>(i0)));
            }
        }
        sp.push_back(add(pts.back(), mul(dir, ext)));
        // a gentle S-bend across the whole spine (slowly drifting): the input -> output direction stays a bias,
        // never a straight diagonal band
        {
            const V up{0.0f, 1.0f, 0.0f};
            V side = normv(V{dir.y * up.z - dir.z * up.y, dir.z * up.x - dir.x * up.z, dir.x * up.y - dir.y * up.x});
            const V depth = normv(V{dir.y * side.z - dir.z * side.y, dir.z * side.x - dir.x * side.z, dir.x * side.y - dir.y * side.x});
            const int M = static_cast<int>(sp.size());
            for (int k = 1; k + 1 < M; ++k) {
                const float u = static_cast<float>(k) / static_cast<float>(M - 1);
                const float a = std::sin(3.14159f * u) * (0.18f + 0.35f * s.radius);
                const float bend = std::sin(2.2f * u * 3.14159f + 0.05f * s.time + 0.7f);
                const float dz = std::cos(1.7f * u * 3.14159f + 0.04f * s.time);
                sp[static_cast<size_t>(k)] = add(sp[static_cast<size_t>(k)], add(mul(side, a * bend), mul(depth, 0.6f * a * dz)));
            }
        }
        P.spineN = static_cast<int>(sp.size());
        // spine coordinate: 0 at the first effect, 1 at the last (ends extend a little outside)
        const float e0 = N >= 2 ? -ext / std::max(1e-3f, std::sqrt(dot(sub(pts.back(), pts.front()), sub(pts.back(), pts.front())))) : -0.5f;
        for (int k = 0; k < P.spineN; ++k) {
            const size_t o = static_cast<size_t>(4 * k);
            P.spine[o] = sp[static_cast<size_t>(k)].x;
            P.spine[o + 1] = sp[static_cast<size_t>(k)].y;
            P.spine[o + 2] = sp[static_cast<size_t>(k)].z;
            float sv;
            if (k == 0) sv = N >= 2 ? e0 : 0.0f;
            else if (k == P.spineN - 1) sv = N >= 2 ? 1.0f - e0 : 1.0f;
            else sv = N >= 2 ? static_cast<float>(k - 1) / static_cast<float>(P.spineN - 3) : 0.5f;
            P.spine[o + 3] = sv;
        }
    }
    P.links = 0;  // V4: no link sheets (the medium itself connects everything)
    P.packets = std::min(static_cast<int>(s.packets.size()), kMaxFieldPackets);
    for (int i = 0; i < P.packets; ++i) {
        const auto& pk = s.packets[static_cast<size_t>(i)];
        const size_t k = static_cast<size_t>(4 * i);
        for (int j = 0; j < 3; ++j) {
            P.packP[k + static_cast<size_t>(j)] = pk.p[j];
            P.packA[k + static_cast<size_t>(j)] = pk.axis[j];
            P.packU[k + static_cast<size_t>(j)] = pk.u[j];
            P.packV[k + static_cast<size_t>(j)] = pk.v[j];
            P.packC[k + static_cast<size_t>(j)] = pk.col[j];
        }
        P.packP[k + 3] = pk.radius;
        P.packA[k + 3] = pk.env;
        P.packU[k + 3] = pk.seed;
    }
    P.cam[0] = s.yaw;
    P.cam[1] = s.pitch;
    P.cam[2] = s.aspect;
    P.cam[3] = s.time;
    P.view[0] = std::clamp(std::isfinite(s.zoom) ? s.zoom : 1.0f, 0.3f, 1.0f);
    P.audio[0] = s.energy;
    P.audio[1] = s.high;
    P.audio[2] = s.low;
    P.audio[3] = s.mid;
    P.audio2[0] = s.balance;
    P.audio2[1] = s.turbulence;
    P.audio2[2] = s.radius;
    P.audio2[3] = std::clamp(s.detail, 0.0f, 1.0f);
    for (int k = 0; k < 2; ++k) { P.pulse[2 * k] = s.pulseS[k]; P.pulse[2 * k + 1] = s.pulseA[k]; }
    for (int j = 0; j < 4; ++j) P.bound[j] = s.bound[j];
    return P;
}

namespace {
void renderRows(const PackedScene& P, int w, int h, int y0, int y1, std::vector<float>& rgb, int steps, int frame);
}

void renderVolume(const FieldScene& s, int w, int h, std::vector<float>& rgb, int steps, int threads, int jitterFrame) {
    rgb.assign(static_cast<size_t>(std::max(0, w)) * static_cast<size_t>(std::max(0, h)) * 3, 0.0f);
    if (w <= 0 || h <= 0) return;
    const PackedScene P = packScene(s);
    const int nt = threads > 0 ? threads : std::clamp(static_cast<int>(std::thread::hardware_concurrency()), 1, 4);
    if (nt == 1 || h < 32) { renderRows(P, w, h, 0, h, rgb, steps, jitterFrame); return; }
    std::vector<std::thread> th;
    for (int k = 0; k < nt; ++k) th.emplace_back([&, k] { renderRows(P, w, h, h * k / nt, h * (k + 1) / nt, rgb, steps, jitterFrame); });
    for (auto& t : th) t.join();
}

namespace {
void renderRows(const PackedScene& P, int w, int h, int y0, int y1, std::vector<float>& rgb, int steps, int frame) {
    const float cy = std::cos(P.cam[0]), sy = std::sin(P.cam[0]), cp = std::cos(P.cam[1]), sp = std::sin(P.cam[1]);
    // camera space -> world (inverse of Formation::project's yaw then pitch)
    auto toWorld = [&](V c) {
        const float y = cp * c.y + sp * c.z, z1 = -sp * c.y + cp * c.z;
        return V{cy * c.x - sy * z1, y, sy * c.x + cy * z1};
    };
    const V camW = toWorld({0.0f, 0.0f, kFieldCamDist});
    const V bc{P.bound[0], P.bound[1], P.bound[2]};
    const float br = P.bound[3];
    const float aspect = P.cam[2];
    const float t = P.cam[3];
    // depth tint: V4 indigo (0.10, 0.08, 0.30) desaturated toward its own luminance by the mood
    const float ind = gMood.indigo, indL = 0.30f * 0.10f + 0.59f * 0.08f + 0.11f * 0.30f;
    const V indigo{indL + ind * (0.10f - indL), indL + ind * (0.08f - indL), indL + ind * (0.30f - indL)};
    // per-frame sub-pixel and step jitter (golden-ratio sequence): temporal accumulation reconstructs the image
    const float jx = std::fmod(0.5f + 0.7548777f * static_cast<float>(frame), 1.0f) - 0.5f;
    const float jy = std::fmod(0.5f + 0.5698403f * static_cast<float>(frame), 1.0f) - 0.5f;
    for (int py = y0; py < y1; ++py)
        for (int px = 0; px < w; ++px) {
            float* o = &rgb[(static_cast<size_t>(py) * static_cast<size_t>(w) + static_cast<size_t>(px)) * 3];
            float bgR, bgG, bgB;
            backgroundColour((static_cast<float>(px) + 0.5f) / static_cast<float>(w), (static_cast<float>(py) + 0.5f) / static_cast<float>(h), P.audio[0], bgR, bgG, bgB);
            o[0] = bgR; o[1] = bgG; o[2] = bgB;
            if (br <= 0.0f) continue;
            const float fx = (static_cast<float>(px) + 0.5f + jx) / static_cast<float>(h);
            const float fy = (static_cast<float>(py) + 0.5f + jy) / static_cast<float>(h);
            const float zs = kFieldScale * P.view[0];
            const V dirC{(fx - 0.54f * aspect) / (kFieldCamDist * zs), -(fy - 0.5f) / (kFieldCamDist * zs), -1.0f};
            const V dir = toWorld(dirC);  // per unit of depth
            const V oc = sub(camW, bc);
            const float A = dot(dir, dir), B = 2.0f * dot(oc, dir), C = dot(oc, oc) - br * br;
            const float disc = B * B - 4.0f * A * C;
            if (disc <= 0.0f) continue;
            const float sq = std::sqrt(disc);
            float d0 = (-B - sq) / (2.0f * A), d1 = (-B + sq) / (2.0f * A);
            d0 = std::max(d0, 0.3f);
            if (d1 <= d0) continue;
            const float dd = (d1 - d0) / static_cast<float>(steps);
            const float ds = dd * std::sqrt(A);
            // interleaved-gradient step jitter (no visible pattern after accumulation)
            // white-noise step jitter, re-drawn every frame (structured sequences left a visible dot pattern)
            const float ign = hash3(static_cast<float>(px) * 0.731f + static_cast<float>(frame) * 3.17f, static_cast<float>(py) * 1.177f, 3.1f + static_cast<float>(frame % 64) * 0.37f);
            V col{0, 0, 0};
            float T = 1.0f;
            for (int k = 0; k < steps && T > 0.02f; ++k) {
                const float d = d0 + (static_cast<float>(k) + ign) * dd;
                const V q = add(camW, mul(dir, d));
                Medium m = medium(P, q, t);
                m.emit = add(m.emit, emissionPackets(P, q, t));
                const float l = lum(m.emit);
                if (l <= 1e-7f && m.absorb <= 1e-7f) continue;
                const float fog = std::exp(-std::max(0.0f, d - kFieldCamDist) * kFog);
                const V e = mul(add(mul(m.emit, 0.5f + 0.5f * fog), mul(indigo, l * (0.5f - 0.5f * fog))), fog);
                col = add(col, mul(e, T * ds));
                T *= std::exp(-m.absorb * ds);
            }
            o[0] = bgR * T + col.x;
            o[1] = bgG * T + col.y;
            o[2] = bgB * T + col.z;
        }
}
}  // namespace

const std::string& fieldFragmentShader() {
    static const std::string src = [] {
        auto f = [](float x) { char b[32]; std::snprintf(b, sizeof b, "%.6f", static_cast<double>(x)); return std::string(b); };
        std::string s = R"(#version 150
in vec2 vUv;
out vec4 o;
uniform vec4 uNodeP[32]; uniform vec4 uNodeA[32]; uniform vec4 uNodeU[32]; uniform vec4 uNodeV[32]; uniform vec4 uNodeC[32];
uniform vec4 uSpine[16];
uniform vec4 uPackP[4]; uniform vec4 uPackA[4]; uniform vec4 uPackU[4]; uniform vec4 uPackV[4]; uniform vec4 uPackC[4];
uniform int uNodes; uniform int uSpineN; uniform int uPackets; uniform int uSteps;
uniform vec4 uCam; uniform vec4 uAudio; uniform vec4 uAudio2; uniform vec4 uPulse; uniform vec4 uBound; uniform vec4 uView; uniform vec2 uRes; uniform float uFrame;
float smoothF(float e0, float e1, float x) { float t = clamp((x - e0) / (e1 - e0), 0.0, 1.0); return t * t * (3.0 - 2.0 * t); }
float lumF(vec3 c) { return 0.30 * c.x + 0.55 * c.y + 0.15 * c.z; }
float hash3(float x, float y, float z) { float h = sin(x * 127.1 + y * 311.7 + z * 74.7) * 43758.5453; return h - floor(h); }
float noise3(vec3 p) {
  vec3 i = floor(p); vec3 f = p - i; f = f * f * (3.0 - 2.0 * f);
  float x00 = mix(hash3(i.x, i.y, i.z), hash3(i.x + 1.0, i.y, i.z), f.x);
  float x10 = mix(hash3(i.x, i.y + 1.0, i.z), hash3(i.x + 1.0, i.y + 1.0, i.z), f.x);
  float x01 = mix(hash3(i.x, i.y, i.z + 1.0), hash3(i.x + 1.0, i.y, i.z + 1.0), f.x);
  float x11 = mix(hash3(i.x, i.y + 1.0, i.z + 1.0), hash3(i.x + 1.0, i.y + 1.0, i.z + 1.0), f.x);
  return mix(mix(x00, x10, f.y), mix(x01, x11, f.y), f.z);
}
vec3 warpF(vec3 q, float t, float low, float turb) {
  vec3 w = vec3(sin(1.3 * q.y + 0.07 * t + 1.1), sin(1.1 * q.z + 0.05 * t + 2.3), sin(1.2 * q.x + 0.06 * t + 0.4));
  vec3 qs = q * 2.2;
  vec3 w3 = vec3(noise3(qs + vec3(0.0, 0.0, 0.013 * t)) - 0.5, noise3(qs + vec3(5.2, 1.3, 0.011 * t)) - 0.5, noise3(qs + vec3(9.1, 4.7, 0.012 * t)) - 0.5);
  return q + w * (0.10 + 0.10 * low) + w3 * (0.26 + 0.18 * turb);
}
vec3 packetE(vec3 q, vec3 p, vec3 axis, vec3 u, vec3 v, vec3 col, float strength, float radius, float infl, float seed, float t) {
  vec3 r = q - p; float d2 = dot(r, r);
  if (d2 > infl * infl || strength <= 0.0) return vec3(0.0);
  float d = sqrt(d2) + 1e-4; float ct = dot(r, axis) / d; float s2 = max(1.0 - ct * ct, 0.02);
  float lu = log(max(d / s2, 1e-3) / radius);
  float c0 = 0.5 + 0.5 * cos(6.2832 * (lu * 2.2 + seed * 6.2832 - t * 0.14)); float c2 = c0 * c0; float shell = c2 * c2;
  float phi = atan(dot(r, v), dot(r, u));
  float br = 0.5 + 0.5 * cos(3.0 * phi + 2.0 * lu + t * 0.2);
  float env = exp(-d2 / (0.25 * infl * infl));
  return col * (strength * env * shell * (0.2 + 0.8 * br * br) * 5.0 * PACKET);
}
// returns emission (rgb) and absorption (a)
vec4 medium(vec3 q, float t) {
  float en = uAudio.x, hi = uAudio.y, lo = uAudio.z, mi = uAudio.w;
  float bal = uAudio2.x, turb = uAudio2.y, R0 = uAudio2.z;
  q.x -= bal * 0.12;
  vec3 pc = q + vec3(sin(1.3 * q.y + 0.07 * t + 1.1), sin(1.1 * q.z + 0.05 * t + 2.3), sin(1.2 * q.x + 0.06 * t + 0.4)) * (0.16 + 0.12 * lo);
  float best = 0.0, broad = 0.0, sS = 0.0, sW = 0.0; vec3 tan = vec3(0.0);
  float R = R0 * (0.55 + 1.0 * noise3(q * 0.42 + vec3(0.0, 0.0, 0.01 * t)));
  float inv = 1.0 / (R * R);
  for (int j = 0; j < 15; ++j) {
    if (j + 1 >= uSpineN) break;
    vec3 a = uSpine[j].xyz, b = uSpine[j + 1].xyz; vec3 ab = b - a;
    float L2 = max(dot(ab, ab), 1e-6);
    float u = clamp(dot(pc - a, ab) / L2, 0.0, 1.0);
    vec3 w = pc - (a + ab * u); float d2 = dot(w, w);
    float sm = mix(uSpine[j].w, uSpine[j + 1].w, u) - 0.78;
    float swell = 1.0 + 0.65 * exp(-sm * sm / 0.08);
    float e = exp(-d2 * inv / (swell * swell));
    best = max(best, e);
    float eb = exp(-d2 * inv * 0.30 / (swell * swell));
    broad = max(broad, eb);
    sS += eb * mix(uSpine[j].w, uSpine[j + 1].w, u);
    sW += eb;
    tan += ab * (eb / sqrt(L2));
  }
  float rr = length(q - uBound.xyz) / max(uBound.w, 1e-3);
  float fadeB = 1.0 - smoothF(0.6, 0.95, rr);
  best *= fadeB; broad *= fadeB;
  if (broad < 2e-3) return vec4(0.0);
  vec3 emit = vec3(0.42, 0.30, 0.62) * (broad * HALO * (0.45 + 0.8 * en));
  if (best < 0.01 && broad < 0.25) return vec4(emit, 0.0);
  vec3 p = warpF(q, t, lo, turb);
  float s = sW > 1e-6 ? sS / sW : 0.5;
  vec3 T = length(tan) > 1e-6 ? normalize(tan) : vec3(1.0, 0.0, 0.0);
  float along = dot(p, T);
  vec3 pa = (p - T * along) + T * (along * 0.32);
  float n1 = noise3(pa * 4.3 + vec3(0.0, 0.021 * t, 1.7));
  float n2 = noise3(pa * 6.1 + vec3(3.1, 0.6, 0.017 * t));
  float n3 = noise3(pa * 1.9 + vec3(7.7, 0.013 * t, 2.9));
  float n4 = noise3(p * 1.05 + vec3(1.3, 4.1, 0.008 * t));
  float n5 = noise3(p * 5.6 + vec3(2.2, 0.019 * t, 5.3));
  float n6 = noise3(p * 8.3 + vec3(6.4, 3.3, 0.023 * t));
  float r1 = 1.0 - abs(2.0 * n1 - 1.0), r2 = 1.0 - abs(2.0 * n2 - 1.0), r3 = 1.0 - abs(2.0 * n3 - 1.0);
  float r5 = 1.0 - abs(2.0 * n5 - 1.0), r6 = 1.0 - abs(2.0 * n6 - 1.0);
  float r5s = r5 * r5, r5q = r5s * r5s, r6s = r6 * r6, r6q = r6s * r6s;
  float r1s = r1 * r1, r1q = r1s * r1s, r1e = r1q * r1q, r2s = r2 * r2, r2q = r2s * r2s, r3s = r3 * r3, r3q = r3s * r3s;
  float fil = r1e * r1s * r2q * r2s + 0.8 * r5q * r5q * r5s * r6q * r6s;
  float detail = uAudio2.w;
  if (detail < 1.0) fil = fil * detail + (1.0 - detail) * 0.35 * (r1q * r2s + 0.8 * r5q * r6s);
  float rib = r3q * r3s;
  float tendril = r3q * r3q * r3q * r3q * r1q;
  float veil = smoothF(0.50, 0.90, n4);
  float pulse = 0.0;
  if (uPulse.x >= 0.0) { float z = (s - uPulse.x) / 0.09; pulse += uPulse.y * exp(-z * z); }
  if (uPulse.z >= 0.0) { float z = (s - uPulse.z) / 0.09; pulse += uPulse.w * exp(-z * z); }
  float massZ = (s - 0.78) / 0.28; float mass = exp(-massZ * massZ);
  float hot = 0.50 + 0.55 * smoothF(0.1, 1.0, s) + 0.9 * mass + 1.4 * pulse;
  float alive = 0.32 + 0.85 * en;
  vec3 col = vec3(0.0); float cw = 0.0, enabled = 0.0, strength = 0.0;
  float sig = 0.9 / max(1.0, float(uNodes));
  for (int i = 0; i < 32; ++i) {
    if (i >= uNodes) break;
    float z = (s - uNodeP[i].w) / sig;
    float g = exp(-z * z) + 1e-6;
    float st = max(uNodeA[i].w, 0.0);
    col += uNodeC[i].xyz * (g * (0.05 + st)); enabled += g * uNodeC[i].w; strength += g * st; cw += g;
  }
  if (cw <= 1e-5) return vec4(emit, 0.0);
  float colW = cw * 0.05 + strength;
  col /= max(colW, 1e-6); enabled /= cw; strength = min(1.0, strength / cw);
  col = mix(vec3(0.52, 0.52, 0.58), col, enabled);
  float dim = 0.30 + 0.70 * enabled;
  float l0 = lumF(col);
  float chroma = CHROMA * (0.85 + 0.3 * en) * (0.4 + 0.6 * enabled);
  vec3 vivid = max(vec3(l0) * (1.0 - chroma) + col * chroma, vec3(0.0));
  float edge = smoothF(0.55, 0.75, r1) * (1.0 - smoothF(0.85, 0.97, r1));
  float ip = sin(6.0 * n2 + 3.0 * along + 0.3 * t);
  vec3 iris = mix(vec3(0.55, 0.88, 1.0), vec3(1.0, 0.62, 0.90), 0.5 + 0.5 * ip);
  float base = strength * dim * hot * alive / (1.0 + 0.05 * float(max(0, uNodes - 1)));
  float E = best * base;
  float eFil = fil * FIL * (0.55 + 0.9 * hi) * (1.0 + 0.8 * mass);
  float eRib = rib * RIB * (0.45 + 0.8 * mi);
  float eVeil = veil * VEILE * (0.5 + 0.7 * lo) * (0.6 + 0.8 * mass);
  float eTen = tendril * TENDRIL * broad * (1.0 - 0.6 * best) * base * (0.6 + 0.6 * hi);
  vec3 c = vivid * (E * (eFil + eRib + eVeil) + eTen);
  float cmax = max(col.x, max(col.y, col.z)), cmin = min(col.x, min(col.y, col.z));
  float csat = clamp(2.5 * (cmax - cmin) / max(cmax, 1e-4), 0.0, 1.0);
  c += iris * ((E + 0.4 * broad * base) * edge * fil * 2.2 * enabled * csat);
  emit = mix(vec3(0.52, 0.52, 0.58), vec3(0.42, 0.30, 0.62), csat) * (broad * HALO * (0.45 + 0.8 * en));
  float hl = smoothF(0.12, 0.5, fil * hot * best * (0.6 + en)) * enabled;
  c = mix(c, vec3(0.96, 0.91, 1.0) * (lumF(c) * 1.25), hl * 0.55);
  emit += c;
  return vec4(emit, ABSORB * lumF(c) + VEILA * veil * best * strength * 0.5);
}
vec3 toWorld(vec3 c, float cy, float sy, float cp, float sp) {
  float y = cp * c.y + sp * c.z; float z1 = -sp * c.y + cp * c.z;
  return vec3(cy * c.x - sy * z1, y, sy * c.x + cy * z1);
}
vec3 background(vec2 uv, float energy) {
  float dx = uv.x - 0.6, dy = uv.y - 0.48;
  float k = exp(-(dx * dx * 1.6 + dy * dy * 2.2));
  float lift = 0.65 + 0.35 * k * (1.0 + 0.25 * clamp(energy, 0.0, 1.2));
  return vec3(0.016, 0.011, 0.026) * lift * MOODBG;
}
void main() {
  vec2 pix = vec2(vUv.x * uRes.x, (1.0 - vUv.y) * uRes.y);
  vec3 bg = background(vec2(vUv.x, 1.0 - vUv.y), uAudio.x);
  if (uBound.w <= 0.0) { o = vec4(bg, 1.0); return; }
  float cy = cos(uCam.x), sy = sin(uCam.x), cp = cos(uCam.y), sp = sin(uCam.y);
  float aspect = uCam.z, t = uCam.w;
  // per-frame sub-pixel jitter (golden-ratio sequence): the temporal accumulation reconstructs a stable image
  vec2 jit = vec2(fract(0.5 + 0.7548777 * uFrame), fract(0.5 + 0.5698403 * uFrame)) - 0.5;
  float fx = (vUv.x + jit.x / uRes.x) * aspect, fy = 1.0 - vUv.y + jit.y / uRes.y;
  vec3 camW = toWorld(vec3(0.0, 0.0, CAMDIST), cy, sy, cp, sp);
  float zs = SCALE * clamp(uView.x, 0.3, 1.0);
  vec3 dir = toWorld(vec3((fx - 0.54 * aspect) / (CAMDIST * zs), -(fy - 0.5) / (CAMDIST * zs), -1.0), cy, sy, cp, sp);
  vec3 oc = camW - uBound.xyz;
  float A = dot(dir, dir), B = 2.0 * dot(oc, dir), C = dot(oc, oc) - uBound.w * uBound.w;
  float disc = B * B - 4.0 * A * C;
  if (disc <= 0.0) { o = vec4(bg, 1.0); return; }
  float sq = sqrt(disc);
  float d0 = max((-B - sq) / (2.0 * A), 0.3), d1 = (-B + sq) / (2.0 * A);
  if (d1 <= d0) { o = vec4(bg, 1.0); return; }
  float dd = (d1 - d0) / float(uSteps); float ds = dd * sqrt(A);
  // white-noise step jitter, re-drawn every frame (averages out in the temporal accumulation)
  float ign = hash3(floor(pix.x) * 0.731 + uFrame * 3.17, floor(pix.y) * 1.177, 3.1 + mod(uFrame, 64.0) * 0.37);
  vec3 col = vec3(0.0); float T = 1.0; float indL = 0.30 * 0.10 + 0.59 * 0.08 + 0.11 * 0.30;
  vec3 indigo = vec3(indL) + MOODIND * (vec3(0.10, 0.08, 0.30) - vec3(indL));
  for (int k = 0; k < 96; ++k) {
    if (k >= uSteps || T <= 0.02) break;
    float d = d0 + (float(k) + ign) * dd;
    vec3 q = camW + dir * d;
    vec4 m = medium(q, t);
    for (int i = 0; i < 4; ++i) {
      if (i >= uPackets) break;
      m.rgb += packetE(q, uPackP[i].xyz, uPackA[i].xyz, uPackU[i].xyz, uPackV[i].xyz, uPackC[i].xyz, uPackA[i].w * 1.4, uPackP[i].w, uPackP[i].w * 4.5, uPackU[i].w, t * 4.0);
    }
    float l = lumF(m.rgb);
    if (l <= 1e-7 && m.a <= 1e-7) continue;
    float fog = exp(-max(0.0, d - CAMDIST) * FOG);
    vec3 e = (m.rgb * (0.5 + 0.5 * fog) + indigo * (l * (0.5 - 0.5 * fog))) * fog;
    col += e * (T * ds);
    T *= exp(-m.a * ds);
  }
  o = vec4(bg * T + col, 1.0);
}
)";
        auto rep = [&](const std::string& k, const std::string& v) { for (size_t p; (p = s.find(k)) != std::string::npos;) s.replace(p, k.size(), v); };
        rep("PACKET", f(kPacketGain));
        rep("FIL", f(kFilGain));
        rep("RIB", f(kRibGain));
        rep("VEILE", f(kVeilGain));
        rep("VEILA", f(kVeilAbsorb));
        rep("ABSORB", f(kAbsorb));
        rep("HALO", f(kHalo));
        rep("TENDRIL", f(kTendril));
        rep("CHROMA", f(kChroma));
        rep("CAMDIST", f(kFieldCamDist));
        rep("SCALE", f(kFieldScale));
        rep("FOG", f(kFog));
        rep("MOODBG", f(gMood.bg));
        rep("MOODIND", f(gMood.indigo));
        return s;
    }();
    return src;
}

}  // namespace ih1::visual
