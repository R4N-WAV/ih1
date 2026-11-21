#include "ih1/visual/FieldVolume.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>

namespace ih1::visual {

namespace {
// Tuning shared by both implementations (the GLSL source below is generated with these values).
constexpr float kSkinGain = 6.5f;     // emission of the field-line skins
constexpr float kCoreGain = 3.6f;    // emission of the node cores
constexpr float kFluxGain = 0.55f;     // flux-density body around each node
constexpr float kVeilGain = 8.0f;     // creased membranes near each node
constexpr float kLinkGain = 2.4f;     // emission of the transmission sheets
constexpr float kAbsorb = 0.55f;      // density per unit of emitted luminance (local occlusion)
constexpr float kFog = 0.42f;         // depth attenuation beyond the focal plane

struct V { float x, y, z; };
inline V add(V a, V b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V sub(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V mul(V a, float k) { return {a.x * k, a.y * k, a.z * k}; }
inline float dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V ld(const float* p) { return {p[0], p[1], p[2]}; }
inline float smooth(float e0, float e1, float x) {
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
inline float lum(V c) { return 0.30f * c.x + 0.55f * c.y + 0.15f * c.z; }

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

V warp(V q, float t) {
    V w{std::sin(1.3f * q.y + 0.07f * t + 1.1f), std::sin(1.1f * q.z + 0.05f * t + 2.3f), std::sin(1.2f * q.x + 0.06f * t + 0.4f)};
    V w2{std::sin(3.1f * q.z + 0.11f * t + 0.7f), std::sin(2.9f * q.x + 0.09f * t + 1.9f), std::sin(3.3f * q.y + 0.10f * t + 2.8f)};
    const V qs = mul(q, 2.2f);
    V w3{noise3(add(qs, V{0.0f, 0.0f, 0.013f * t})) - 0.5f, noise3(add(qs, V{5.2f, 1.3f, 0.011f * t})) - 0.5f, noise3(add(qs, V{9.1f, 4.7f, 0.012f * t})) - 0.5f};
    return add(q, add(add(mul(w, 0.11f), mul(w2, 0.04f)), mul(w3, 0.22f)));
}

// one dipole's light at q (node or packet)
V dipole(V q, V p, V axis, V u, V v, V col, float strength, float radius, float infl, float seed, float energy, float t) {
    const V r = sub(q, p);
    const float d2 = dot(r, r);
    if (d2 > infl * infl || strength <= 0.0f) return {0, 0, 0};
    const float d = std::sqrt(d2) + 1e-4f;
    const float ct = dot(r, axis) / d;
    const float s2 = std::max(1.0f - ct * ct, 0.02f);
    const float L = d / s2;  // dipole L-shell: constant along a field line
    const float lu = std::log(std::max(L, 1e-3f) / radius);
    // crumpled sheets: the L-shell phase is displaced by noise, so sheets follow the field but fold and crease
    const float nq = noise3(add(mul(q, 1.7f), V{seed * 13.0f, seed * 7.0f, 0.015f * t}));
    const float ush = lu * (1.8f + 1.2f * seed) + seed * 6.2832f - t * 0.035f + 0.9f * nq;
    const float c0 = 0.5f + 0.5f * std::cos(6.2832f * ush);
    const float c2 = c0 * c0, shell = c2 * c2 * c2;
    const float phi = std::atan2(dot(r, v), dot(r, u));
    const float br = 0.5f + 0.5f * std::cos(3.0f * phi + 1.7f * std::sin(2.3f * lu + seed * 9.0f) + 0.6f * std::sin(5.0f * phi - 1.3f * lu) + 0.05f * t);
    const float bund = 0.15f + 0.85f * br * br * br;
    // intermittent skins: slow 3-D patches switch whole regions of a sheet on and off
    const float pn = std::sin(2.1f * q.x + seed * 5.0f + 0.03f * t) * std::sin(2.4f * q.y - seed * 3.0f) * std::sin(1.9f * q.z + 0.02f * t + seed * 7.0f);
    const float patch = smooth(-0.15f, 0.45f, pn);
    const float env = std::exp(-d2 / (0.30f * infl * infl)) * smooth(0.6f * radius, 1.6f * radius, d);
    const float pole = 0.55f + 0.9f * ct * ct;
    const float skin = strength * env * shell * bund * (0.3f + 0.7f * patch) * pole * (0.75f + 0.6f * energy) * kSkinGain;
    // creased membranes: thin ridges where a warped noise field crosses its midpoint, only inside the strong field
    const float nm = noise3(add(mul(q, 2.6f), V{seed * 31.0f, 0.02f * t, seed * 17.0f}));
    const float ridge = 1.0f - std::fabs(2.0f * nm - 1.0f);
    const float r2p = ridge * ridge, r4 = r2p * r2p, r8 = r4 * r4;
    const float r3 = radius * radius * radius * 2.0f;
    const float fd = r3 / (d2 * d + r3) * (0.4f + 0.6f * ct * ct);  // |B| ~ 1/r^3: compact, strongest at the poles
    const float veil = std::exp(-d2 / (0.12f * infl * infl)) * r8 * r8 * (0.4f + 0.6f * ct * ct);
    const float flux = strength * (fd * (0.55f + 0.45f * pn) * kFluxGain + veil * kVeilGain) * (0.7f + 0.6f * energy);
    const float cr = radius * 0.6f;
    const float core = strength * std::exp(-d2 / (cr * cr)) * (2.0f + 1.5f * energy) * kCoreGain;
    const V coreCol = add(mul(col, 0.35f), mul(V{0.93f, 0.90f, 1.0f}, 0.65f));
    return add(mul(col, skin + flux), mul(coreCol, core));
}

V link(V q, V a, V b, V u, V v, V ca, V cb, float strength, float pulse, float pulseAmp, float seed, float energy, float t) {
    if (strength <= 0.0f) return {0, 0, 0};
    const V ab = sub(b, a);
    const float L2 = std::max(dot(ab, ab), 1e-6f);
    const float s = std::clamp(dot(sub(q, a), ab) / L2, 0.0f, 1.0f);
    const V w = sub(q, add(a, mul(ab, s)));
    const float dist2 = dot(w, w);
    const float width = (0.07f + 0.05f * std::sin(3.14159f * s)) * (0.8f + 0.4f * std::sin(s * 9.0f + seed * 6.28f + t * 0.04f));
    if (dist2 > 9.0f * width * width) return {0, 0, 0};
    const float ang = std::atan2(dot(w, v), dot(w, u));
    const float rc = 0.5f + 0.5f * std::cos(2.0f * ang + s * 6.0f + seed * 6.28f + t * 0.06f);
    const float rib = rc * rc * rc * rc;
    const float prof = std::exp(-dist2 / (width * width));
    const float ends = std::sqrt(std::max(0.0f, std::sin(3.14159f * s)));
    const float pz = (s - pulse) / 0.08f;
    const float pu = pulse >= 0.0f ? pulseAmp * std::exp(-pz * pz) * 3.0f : 0.0f;
    const float e = strength * prof * (0.25f + 0.75f * rib) * ends * (0.6f + 0.5f * energy + pu) * kLinkGain;
    return mul(add(mul(ca, 1.0f - s), mul(cb, s)), e);
}

V emission(const PackedScene& P, V q, float t) {
    const V w = warp(q, t);
    V e{0, 0, 0};
    const float en = P.audio[0];
    for (int i = 0; i < P.nodes; ++i) {
        const float* np = &P.nodeP[static_cast<size_t>(4 * i)];
        const float* na = &P.nodeA[static_cast<size_t>(4 * i)];
        const float* nu = &P.nodeU[static_cast<size_t>(4 * i)];
        const float* nv = &P.nodeV[static_cast<size_t>(4 * i)];
        const float* nc = &P.nodeC[static_cast<size_t>(4 * i)];
        e = add(e, dipole(w, ld(np), ld(na), ld(nu), ld(nv), ld(nc), na[3], np[3], nu[3], nv[3], en, t));
    }
    for (int i = 0; i < P.links; ++i) {
        const float* la = &P.linkA[static_cast<size_t>(4 * i)];
        const float* lb = &P.linkB[static_cast<size_t>(4 * i)];
        const int a = static_cast<int>(la[0]), b = static_cast<int>(la[1]);
        if (a < 0 || b < 0 || a >= P.nodes || b >= P.nodes) continue;
        e = add(e, link(w, ld(&P.nodeP[static_cast<size_t>(4 * a)]), ld(&P.nodeP[static_cast<size_t>(4 * b)]), ld(&P.linkU[static_cast<size_t>(4 * i)]),
                        ld(&P.linkV[static_cast<size_t>(4 * i)]), ld(&P.nodeC[static_cast<size_t>(4 * a)]), ld(&P.nodeC[static_cast<size_t>(4 * b)]), la[2], la[3],
                        lb[0], lb[1], en, t));
    }
    for (int i = 0; i < P.packets; ++i) {
        const float* pp = &P.packP[static_cast<size_t>(4 * i)];
        const float* pa = &P.packA[static_cast<size_t>(4 * i)];
        const float* pu = &P.packU[static_cast<size_t>(4 * i)];
        e = add(e, dipole(q, ld(pp), ld(pa), ld(pu), ld(&P.packV[static_cast<size_t>(4 * i)]), ld(&P.packC[static_cast<size_t>(4 * i)]), pa[3] * 1.4f, pp[3],
                          pp[3] * 4.5f, pu[3], en, t * 4.0f));
    }
    return e;
}
}  // namespace

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
    for (const auto& nd : nodes) grow(nd.p, nd.influence + 0.25f);  // + the domain warp's reach
    for (const auto& pk : packets) grow(pk.p, pk.radius * 4.5f + 0.25f);
    bound[0] = c[0];
    bound[1] = c[1];
    bound[2] = c[2];
    bound[3] = r;
}

PackedScene packScene(const FieldScene& s) {
    PackedScene P;
    P.nodes = std::min(static_cast<int>(s.nodes.size()), kMaxFieldNodes);
    for (int i = 0; i < P.nodes; ++i) {
        const auto& n = s.nodes[static_cast<size_t>(i)];
        const size_t k = static_cast<size_t>(4 * i);
        for (int j = 0; j < 3; ++j) {
            P.nodeP[k + static_cast<size_t>(j)] = n.p[j];
            P.nodeA[k + static_cast<size_t>(j)] = n.axis[j];
            P.nodeU[k + static_cast<size_t>(j)] = n.u[j];
            P.nodeV[k + static_cast<size_t>(j)] = n.v[j];
            P.nodeC[k + static_cast<size_t>(j)] = n.col[j];
        }
        P.nodeP[k + 3] = n.radius;
        P.nodeA[k + 3] = n.strength;
        P.nodeU[k + 3] = n.influence;
        P.nodeV[k + 3] = n.seed;
    }
    P.links = std::min(static_cast<int>(s.links.size()), kMaxFieldLinks);
    for (int i = 0; i < P.links; ++i) {
        const auto& l = s.links[static_cast<size_t>(i)];
        const size_t k = static_cast<size_t>(4 * i);
        P.linkA[k] = static_cast<float>(l.a);
        P.linkA[k + 1] = static_cast<float>(l.b);
        P.linkA[k + 2] = l.strength;
        P.linkA[k + 3] = l.pulse;
        P.linkB[k] = l.pulseAmp;
        P.linkB[k + 1] = l.seed;
        for (int j = 0; j < 3; ++j) { P.linkU[k + static_cast<size_t>(j)] = l.u[j]; P.linkV[k + static_cast<size_t>(j)] = l.v[j]; }
    }
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
    P.audio[0] = s.energy;
    P.audio[1] = s.high;
    for (int j = 0; j < 4; ++j) P.bound[j] = s.bound[j];
    return P;
}

namespace {
void renderRows(const PackedScene& P, int w, int h, int y0, int y1, std::vector<float>& rgb, int steps);
}

void renderVolume(const FieldScene& s, int w, int h, std::vector<float>& rgb, int steps, int threads) {
    rgb.assign(static_cast<size_t>(std::max(0, w)) * static_cast<size_t>(std::max(0, h)) * 3, 0.0f);
    if (w <= 0 || h <= 0 || s.bound[3] <= 0.0f) return;
    const PackedScene P = packScene(s);
    const int nt = threads > 0 ? threads : std::clamp(static_cast<int>(std::thread::hardware_concurrency()), 1, 4);
    if (nt == 1 || h < 32) { renderRows(P, w, h, 0, h, rgb, steps); return; }
    std::vector<std::thread> th;
    for (int k = 0; k < nt; ++k) th.emplace_back([&, k] { renderRows(P, w, h, h * k / nt, h * (k + 1) / nt, rgb, steps); });
    for (auto& t : th) t.join();
}

namespace {
void renderRows(const PackedScene& P, int w, int h, int y0, int y1, std::vector<float>& rgb, int steps) {
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
    const V indigo{0.10f, 0.08f, 0.30f};
    for (int py = y0; py < y1; ++py)
        for (int px = 0; px < w; ++px) {
            const float fx = (static_cast<float>(px) + 0.5f) / static_cast<float>(h);
            const float fy = (static_cast<float>(py) + 0.5f) / static_cast<float>(h);
            const V dirC{(fx - 0.54f * aspect) / (kFieldCamDist * kFieldScale), -(fy - 0.5f) / (kFieldCamDist * kFieldScale), -1.0f};
            const V dir = toWorld(dirC);  // per unit of depth
            // ray-sphere: |camW + dir*d - bc| = br
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
            const float jit = hash3(static_cast<float>(px) * 0.731f, static_cast<float>(py) * 1.177f, 3.1f);  // white-noise jitter (no pattern)
            V col{0, 0, 0};
            float T = 1.0f;
            for (int k = 0; k < steps && T > 0.02f; ++k) {
                const float d = d0 + (static_cast<float>(k) + jit) * dd;
                const V q = add(camW, mul(dir, d));
                V e = emission(P, q, t);
                const float l = lum(e);
                if (l <= 1e-6f) continue;
                const float fog = std::exp(-std::max(0.0f, d - kFieldCamDist) * kFog);
                e = mul(add(mul(e, 0.5f + 0.5f * fog), mul(indigo, l * (0.5f - 0.5f * fog))), fog);
                col = add(col, mul(e, T * ds));
                T *= std::exp(-kAbsorb * l * ds);
            }
            float* o = &rgb[(static_cast<size_t>(py) * static_cast<size_t>(w) + static_cast<size_t>(px)) * 3];
            o[0] = col.x;
            o[1] = col.y;
            o[2] = col.z;
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
uniform vec4 uLinkA[32]; uniform vec4 uLinkB[32]; uniform vec4 uLinkU[32]; uniform vec4 uLinkV[32];
uniform vec4 uPackP[4]; uniform vec4 uPackA[4]; uniform vec4 uPackU[4]; uniform vec4 uPackV[4]; uniform vec4 uPackC[4];
uniform int uNodes; uniform int uLinks; uniform int uPackets; uniform int uSteps;
uniform vec4 uCam; uniform vec4 uAudio; uniform vec4 uBound; uniform vec2 uRes; uniform float uFrame;
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
vec3 warp(vec3 q, float t) {
  vec3 w = vec3(sin(1.3 * q.y + 0.07 * t + 1.1), sin(1.1 * q.z + 0.05 * t + 2.3), sin(1.2 * q.x + 0.06 * t + 0.4));
  vec3 w2 = vec3(sin(3.1 * q.z + 0.11 * t + 0.7), sin(2.9 * q.x + 0.09 * t + 1.9), sin(3.3 * q.y + 0.10 * t + 2.8));
  vec3 qs = q * 2.2;
  vec3 w3 = vec3(noise3(qs + vec3(0.0, 0.0, 0.013 * t)) - 0.5, noise3(qs + vec3(5.2, 1.3, 0.011 * t)) - 0.5, noise3(qs + vec3(9.1, 4.7, 0.012 * t)) - 0.5);
  return q + w * 0.11 + w2 * 0.04 + w3 * 0.22;
}
vec3 dipole(vec3 q, vec3 p, vec3 axis, vec3 u, vec3 v, vec3 col, float strength, float radius, float infl, float seed, float energy, float t) {
  vec3 r = q - p; float d2 = dot(r, r);
  if (d2 > infl * infl || strength <= 0.0) return vec3(0.0);
  float d = sqrt(d2) + 1e-4; float ct = dot(r, axis) / d; float s2 = max(1.0 - ct * ct, 0.02);
  float L = d / s2; float lu = log(max(L, 1e-3) / radius);
  float nq = noise3(q * 1.7 + vec3(seed * 13.0, seed * 7.0, 0.015 * t));
  float ush = lu * (1.8 + 1.2 * seed) + seed * 6.2832 - t * 0.035 + 0.9 * nq;
  float c0 = 0.5 + 0.5 * cos(6.2832 * ush); float c2 = c0 * c0; float shell = c2 * c2 * c2;
  float phi = atan(dot(r, v), dot(r, u));
  float br = 0.5 + 0.5 * cos(3.0 * phi + 1.7 * sin(2.3 * lu + seed * 9.0) + 0.6 * sin(5.0 * phi - 1.3 * lu) + 0.05 * t);
  float bund = 0.15 + 0.85 * br * br * br;
  float pn = sin(2.1 * q.x + seed * 5.0 + 0.03 * t) * sin(2.4 * q.y - seed * 3.0) * sin(1.9 * q.z + 0.02 * t + seed * 7.0);
  float patchv = smoothF(-0.15, 0.45, pn);
  float env = exp(-d2 / (0.30 * infl * infl)) * smoothF(0.6 * radius, 1.6 * radius, d);
  float pole = 0.55 + 0.9 * ct * ct;
  float skin = strength * env * shell * bund * (0.3 + 0.7 * patchv) * pole * (0.75 + 0.6 * energy) * SKIN;
  float nm = noise3(q * 2.6 + vec3(seed * 31.0, 0.02 * t, seed * 17.0));
  float ridge = 1.0 - abs(2.0 * nm - 1.0); float r2p = ridge * ridge; float r4 = r2p * r2p; float r8 = r4 * r4;
  float r3 = radius * radius * radius * 2.0;
  float fd = r3 / (d2 * d + r3) * (0.4 + 0.6 * ct * ct);
  float veil = exp(-d2 / (0.12 * infl * infl)) * r8 * r8 * (0.4 + 0.6 * ct * ct);
  float flux = strength * (fd * (0.55 + 0.45 * pn) * FLUX + veil * VEIL) * (0.7 + 0.6 * energy);
  float cr = radius * 0.6;
  float core = strength * exp(-d2 / (cr * cr)) * (2.0 + 1.5 * energy) * CORE;
  vec3 coreCol = col * 0.35 + vec3(0.93, 0.90, 1.0) * 0.65;
  return col * (skin + flux) + coreCol * core;
}
vec3 linkE(vec3 q, vec3 a, vec3 b, vec3 u, vec3 v, vec3 ca, vec3 cb, float strength, float pulse, float pulseAmp, float seed, float energy, float t) {
  if (strength <= 0.0) return vec3(0.0);
  vec3 ab = b - a; float L2 = max(dot(ab, ab), 1e-6);
  float s = clamp(dot(q - a, ab) / L2, 0.0, 1.0);
  vec3 w = q - (a + ab * s); float dist2 = dot(w, w);
  float width = (0.07 + 0.05 * sin(3.14159 * s)) * (0.8 + 0.4 * sin(s * 9.0 + seed * 6.28 + t * 0.04));
  if (dist2 > 9.0 * width * width) return vec3(0.0);
  float ang = atan(dot(w, v), dot(w, u));
  float rc = 0.5 + 0.5 * cos(2.0 * ang + s * 6.0 + seed * 6.28 + t * 0.06); float rib = rc * rc * rc * rc;
  float prof = exp(-dist2 / (width * width));
  float ends = sqrt(max(0.0, sin(3.14159 * s)));
  float pz = (s - pulse) / 0.08;
  float pu = pulse >= 0.0 ? pulseAmp * exp(-pz * pz) * 3.0 : 0.0;
  float e = strength * prof * (0.25 + 0.75 * rib) * ends * (0.6 + 0.5 * energy + pu) * LINK;
  return mix(ca, cb, s) * e;
}
vec3 emission(vec3 q, float t) {
  vec3 w = warp(q, t); vec3 e = vec3(0.0); float en = uAudio.x;
  for (int i = 0; i < uNodes; ++i)
    e += dipole(w, uNodeP[i].xyz, uNodeA[i].xyz, uNodeU[i].xyz, uNodeV[i].xyz, uNodeC[i].xyz, uNodeA[i].w, uNodeP[i].w, uNodeU[i].w, uNodeV[i].w, en, t);
  for (int i = 0; i < uLinks; ++i) {
    int a = int(uLinkA[i].x + 0.5), b = int(uLinkA[i].y + 0.5);
    if (a < 0 || b < 0 || a >= uNodes || b >= uNodes) continue;
    e += linkE(w, uNodeP[a].xyz, uNodeP[b].xyz, uLinkU[i].xyz, uLinkV[i].xyz, uNodeC[a].xyz, uNodeC[b].xyz, uLinkA[i].z, uLinkA[i].w, uLinkB[i].x, uLinkB[i].y, en, t);
  }
  for (int i = 0; i < uPackets; ++i)
    e += dipole(q, uPackP[i].xyz, uPackA[i].xyz, uPackU[i].xyz, uPackV[i].xyz, uPackC[i].xyz, uPackA[i].w * 1.4, uPackP[i].w, uPackP[i].w * 4.5, uPackU[i].w, en, t * 4.0);
  return e;
}
vec3 toWorld(vec3 c, float cy, float sy, float cp, float sp) {
  float y = cp * c.y + sp * c.z; float z1 = -sp * c.y + cp * c.z;
  return vec3(cy * c.x - sy * z1, y, sy * c.x + cy * z1);
}
void main() {
  if (uBound.w <= 0.0) { o = vec4(0.0, 0.0, 0.0, 1.0); return; }
  float cy = cos(uCam.x), sy = sin(uCam.x), cp = cos(uCam.y), sp = sin(uCam.y);
  float aspect = uCam.z, t = uCam.w;
  vec2 pix = vec2(vUv.x * uRes.x, (1.0 - vUv.y) * uRes.y);
  float fx = vUv.x * aspect, fy = 1.0 - vUv.y;
  vec3 camW = toWorld(vec3(0.0, 0.0, CAMDIST), cy, sy, cp, sp);
  vec3 dir = toWorld(vec3((fx - 0.54 * aspect) / (CAMDIST * SCALE), -(fy - 0.5) / (CAMDIST * SCALE), -1.0), cy, sy, cp, sp);
  vec3 oc = camW - uBound.xyz;
  float A = dot(dir, dir), B = 2.0 * dot(oc, dir), C = dot(oc, oc) - uBound.w * uBound.w;
  float disc = B * B - 4.0 * A * C;
  if (disc <= 0.0) { o = vec4(0.0, 0.0, 0.0, 1.0); return; }
  float sq = sqrt(disc);
  float d0 = max((-B - sq) / (2.0 * A), 0.3), d1 = (-B + sq) / (2.0 * A);
  if (d1 <= d0) { o = vec4(0.0, 0.0, 0.0, 1.0); return; }
  float dd = (d1 - d0) / float(uSteps); float ds = dd * sqrt(A);
  float jit = fract(hash3(floor(pix.x) * 0.731, floor(pix.y) * 1.177, 3.1) + uFrame * 0.618034);
  vec3 col = vec3(0.0); float T = 1.0; vec3 indigo = vec3(0.10, 0.08, 0.30);
  for (int k = 0; k < 96; ++k) {
    if (k >= uSteps || T <= 0.02) break;
    float d = d0 + (float(k) + jit) * dd;
    vec3 e = emission(camW + dir * d, t);
    float l = lumF(e);
    if (l <= 1e-6) continue;
    float fog = exp(-max(0.0, d - CAMDIST) * FOG);
    e = (e * (0.5 + 0.5 * fog) + indigo * (l * (0.5 - 0.5 * fog))) * fog;
    col += e * (T * ds);
    T *= exp(-ABSORB * l * ds);
  }
  o = vec4(col, 1.0);
}
)";
        auto rep = [&](const std::string& k, const std::string& v) { for (size_t p; (p = s.find(k)) != std::string::npos;) s.replace(p, k.size(), v); };
        rep("SKIN", f(kSkinGain));
        rep("CORE", f(kCoreGain));
        rep("LINK", f(kLinkGain));
        rep("ABSORB", f(kAbsorb));
        rep("CAMDIST", f(kFieldCamDist));
        rep("SCALE", f(kFieldScale));
        rep("FOG", f(kFog));
        rep("FLUX", f(kFluxGain));
        rep("VEIL", f(kVeilGain));
        return s;
    }();
    return src;
}

}  // namespace ih1::visual
