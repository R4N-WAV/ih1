#include "ih1/visual/Formation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace ih1::visual {

// ------------------------------------------------------------------------------------- tone map
Rgb toneMap(Rgb a, float exposure) {
    auto nz = [](float x) { return std::isfinite(x) ? std::max(0.0f, x) : 0.0f; };
    a = {nz(a.r), nz(a.g), nz(a.b)};
    const float m = std::max({a.r, a.g, a.b});
    if (m <= 0.0f) return {0.0f, 0.0f, 0.0f};
    // hue-preserving: compress the brightest channel, scale the others by the same factor
    const float t = 1.0f - std::exp(-m * exposure);
    const float k = t / m;
    Rgb c{a.r * k, a.g * k, a.b * k};
    // only very dense overlaps ease toward a pale lavender-white
    const float u = std::clamp((t - 0.80f) / 0.20f, 0.0f, 1.0f);
    const float w = u * u * (3.0f - 2.0f * u) * 0.55f;
    const Rgb pearl{0.95f * t, 0.91f * t, 1.0f * t};
    c.r += w * (pearl.r - c.r);
    c.g += w * (pearl.g - c.g);
    c.b += w * (pearl.b - c.b);
    return c;
}

// ------------------------------------------------------------------------------------- helpers
namespace {
using V3 = Formation::V3;
constexpr float kPi = 3.14159265358979f;
constexpr float kCamDist = 4.2f;   // camera distance (world units)
constexpr float kScale = 0.23f;    // field-height units per world unit at the focal distance
constexpr int kMaxStrandsPerInst = 170;
constexpr float kTransferDur = 0.95f;
constexpr float kGain = 1.7f;      // overall strand intensity (tuned with exposure 2.2)
constexpr float kPulseTravel = 0.6f;  // seconds for a pulse to cross one link

inline V3 add(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 mul(V3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float len(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 norm(V3 a) { const float l = len(a); return l > 1e-9f ? mul(a, 1.0f / l) : V3{0, 1, 0}; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline V3 lerp3(V3 a, V3 b, float t) { return add(a, mul(sub(b, a), t)); }
inline Rgb lerpC(const Rgb& a, const Rgb& b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
inline bool finite3(V3 a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }

uint64_t mix64(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed) {}
    float next() { s += 0x9E3779B97F4A7C15ull; return static_cast<float>(mix64(s) >> 40) * (1.0f / 16777216.0f); }
    float bip() { return next() * 2.0f - 1.0f; }
    float gauss() {
        const float u = std::max(1e-6f, next()), v = next();
        return std::sqrt(-2.0f * std::log(u)) * std::cos(2.0f * kPi * v);
    }
    V3 sphere() { return norm({gauss(), gauss(), gauss()}); }
};

// Divergence-free curl noise: v(p, t) = sum_k cos(w_k . p + phi_k(t)) (w_k x e_k) / |.|. Each term is the curl
// of a_k sin(w_k . p + phi_k) e_k, so the sum is exactly divergence-free. Wavelengths 1.1..3.1 world units,
// phase drift 0.04..0.10 rad/s: smooth, low-frequency and slow (no swirling).
struct FlowTerm { float wx, wy, wz, cx, cy, cz, phase, omega; };
constexpr int kTerms = 6;
const std::array<FlowTerm, kTerms>& flowTerms() {
    static const std::array<FlowTerm, kTerms> terms = [] {
        std::array<FlowTerm, kTerms> t{};
        Rng r(0x1A1ull ^ 0xA5A5F00Dull);
        for (int k = 0; k < kTerms; ++k) {
            const V3 d = r.sphere();
            const float freq = 2.0f * kPi / (1.1f + 2.0f * r.next());
            const V3 w = mul(d, freq);
            const V3 c = norm(cross(w, r.sphere()));
            t[static_cast<size_t>(k)] = {w.x, w.y, w.z, c.x, c.y, c.z, r.next() * 2.0f * kPi, (0.04f + 0.06f * r.next()) * (r.next() < 0.5f ? -1.0f : 1.0f)};
        }
        return t;
    }();
    return terms;
}

// The understated signal path: bottom-left (input) to top-right (output), with a gentle S so it never reads
// as a straight line, axis or arrow.
const V3 kIn{-1.65f, -1.0f, 0.25f}, kOut{1.65f, 1.0f, -0.25f};
V3 pathTangent() { return norm(sub(kOut, kIn)); }
V3 pathPerp() { return norm(cross(pathTangent(), {0, 0, 1})); }
V3 pathPoint(float t, float extent) {
    const V3 base = lerp3(kIn, kOut, t);
    const V3 s = mul(pathPerp(), 0.30f * std::sin(2.0f * kPi * t + 0.6f));
    return mul(add(base, s), extent);
}

float smooth01(float x) { x = std::clamp(x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }
float easeInOut(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x < 0.5f ? 4.0f * x * x * x : 1.0f - std::pow(-2.0f * x + 2.0f, 3.0f) * 0.5f;
}
float safe(float x, float lo, float hi, float def = 0.0f) { return std::isfinite(x) ? std::clamp(x, lo, hi) : def; }
float levelMap(float x) { return std::isfinite(x) ? std::clamp(std::sqrt(std::max(0.0f, x)) * 1.6f, 0.0f, 1.2f) : 0.0f; }
// one-pole follower with separate attack / release (seconds)
void follow(float& y, float x, float dt, float att, float rel) {
    const float tc = x > y ? att : rel;
    y += (x - y) * (1.0f - std::exp(-dt / std::max(1e-3f, tc)));
    if (!std::isfinite(y)) y = 0.0f;
}
}  // namespace

// ------------------------------------------------------------------------------------- formation
Formation::Formation(Quality q) : quality_(q) {
    segs_.reserve(140000);
    glows_.reserve(80);
    pts_.reserve(128);
    nodes_.reserve(64);
}

// Light temporal smoothing only: most of the previous frame is gone after two frames, so filaments stay crisp.
float Formation::decay(float dt) const { return std::pow(0.3f, std::clamp(dt, 0.0f, 0.2f) * 60.0f); }

int Formation::lifeOf(uint64_t id) const {
    auto f = insts_.find(id);
    return f == insts_.end() ? -1 : static_cast<int>(f->second.life);
}

float Formation::stablePresence(uint64_t id) const {
    auto f = insts_.find(id);
    if (f == insts_.end() || f->second.life != Life::Integrated) return 0.0f;
    return f->second.presence;
}

bool Formation::transferActive() const {
    for (const auto& kv : insts_) if (kv.second.transfer) return true;
    return !pending_.empty();
}

V3 Formation::nodePosition(uint64_t id) const {
    auto it = insts_.find(id);
    return it == insts_.end() ? V3{} : it->second.anchor;
}

V3 Formation::curl(const V3& p, float t) const {
    V3 v;
    const float lean = balance_ * 0.35f;  // stereo balance leans the noise slightly
    for (const auto& f : flowTerms()) {
        const float c = std::cos(f.wx * (p.x - lean) + f.wy * p.y + f.wz * p.z + f.phase + f.omega * t);
        v.x += c * f.cx;
        v.y += c * f.cy;
        v.z += c * f.cz;
    }
    return mul(v, 1.0f / 1.6f);
}

V3 Formation::field(const V3& p, float t) const {
    // soft dipoles: B = s (3 (m.r) r / r^2 - m) / r^3, softened by the node radius
    V3 b;
    for (const auto& n : nodes_) {
        const V3 r = sub(p, n.p);
        const float d2 = dot(r, r);
        if (d2 > 9.0f) continue;  // beyond 3 units a dipole contributes < 1 %: skip (bounded cost)
        const float e2 = n.radius * n.radius;
        const float r2 = d2 + e2;
        const float inv = n.strength / (r2 * std::sqrt(r2));
        const float md = 3.0f * dot(n.axis, r) / r2;
        b = add(b, mul(sub(mul(r, md), n.axis), inv));
    }
    // weak input -> output transmission flow, plus slow curl noise for organic bending
    const V3 flow = mul(pathTangent(), 1.1f);
    const V3 c = mul(curl(p, t), 1.6f * (1.0f + 0.5f * low_));
    return add(b, add(flow, c));
}

void Formation::project(const V3& p, float& x, float& y, float& depth) const {
    const float cy = std::cos(yaw_), sy = std::sin(yaw_);
    const float x1 = cy * p.x + sy * p.z, z1 = -sy * p.x + cy * p.z;
    const float cp = std::cos(pitch_), sp = std::sin(pitch_);
    const float y2 = cp * p.y - sp * z1, z2 = sp * p.y + cp * z1;
    depth = std::max(0.6f, kCamDist - z2);
    const float k = kCamDist / depth;
    const float cx = aspect_ * 0.54f;  // slightly right of centre: generous black space on both sides
    x = cx + x1 * k * kScale;
    y = 0.5f - y2 * k * kScale;
}

V3 Formation::unproject(float fx, float fy, float worldZ) const {
    // inverse of project() for a point at world depth z (ignores the tiny camera sway)
    const float depth = std::max(0.6f, kCamDist - worldZ);
    const float k = kCamDist / depth;
    const float cx = aspect_ * 0.54f;
    return {(fx - cx) / (k * kScale), (0.5f - fy) / (k * kScale), worldZ};
}

Rgb Formation::shade(const Rgb& c) const {
    if (mono_ <= 0.0f) return c;
    const float l = 0.30f * c.r + 0.55f * c.g + 0.15f * c.b;
    const Rgb grey{l * 0.93f, l * 0.95f, l * 1.03f};
    return lerpC(c, grey, std::clamp(mono_, 0.0f, 1.0f));
}

void Formation::buildStrands(Inst& inst, int count) {
    inst.strands.clear();
    inst.strands.reserve(static_cast<size_t>(count));
    for (int k = 0; k < count; ++k) {
        Rng r(mix64((static_cast<uint64_t>(inst.seed) << 20) ^ static_cast<uint64_t>(k) ^ 0xF1E1Dull));
        Strand s;
        const float kr = r.next();
        s.kind = kr < 0.38f ? 0 : (kr < 0.72f ? 1 : 2);
        s.start = r.sphere();
        const float g = r.next();
        s.gain = 0.16f + 1.7f * g * g * g * g;  // mostly fine and faint, a few bright cores
        s.phase = r.next() * 2.0f * kPi;
        s.lenScale = 0.7f + 0.6f * r.next();
        s.dir = r.next() < 0.5f ? 1 : -1;
        if (s.kind == 1) {
            const float tr = r.next();
            s.target = tr < 0.12f ? 3 : (tr < 0.30f ? 2 : 1);  // 3: an input strand (only drawn on the first node)
            s.steps = static_cast<uint8_t>(36 + static_cast<int>(20 * r.next()));
        } else if (s.kind == 0) {
            s.steps = static_cast<uint8_t>(22 + static_cast<int>(26 * r.next() * r.next()));
        } else {
            s.steps = static_cast<uint8_t>(9 + static_cast<int>(10 * r.next()));
            s.gain *= 0.55f;
        }
        inst.strands.push_back(s);
    }
}

void Formation::layout(const FrameInput& in) {
    const int n = static_cast<int>(in.chain.size());
    extentTarget_ = 0.40f + 0.60f * std::min(1.0f, static_cast<float>(std::max(0, n - 1)) / 6.0f);
    for (auto& kv : insts_) kv.second.inChain = false;
    order_.clear();
    for (int i = 0; i < n; ++i) {
        const auto& ii = in.chain[static_cast<size_t>(i)];
        if (ii.id == 0) continue;
        order_.push_back(ii.id);
        const bool fresh = insts_.find(ii.id) == insts_.end();
        Inst& c = insts_[ii.id];
        c.inChain = true;
        c.index = static_cast<int>(order_.size()) - 1;
        auto ch = [](float v) { return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f; };
        c.colour = {ch(ii.colour.r), ch(ii.colour.g), ch(ii.colour.b)};
        if (fresh || c.seed != ii.seed || c.strands.empty()) {
            c.id = ii.id;
            c.seed = ii.seed ? ii.seed : 1u;
            buildStrands(c, kMaxStrandsPerInst);
            Rng ar(mix64(c.seed ^ 0xA715ull));
            // dipole axis: deliberately oblique to the path, so arcs cross it instead of lying along it
            const float a = (35.0f + 50.0f * ar.next()) * kPi / 180.0f, b = ar.next() * 2.0f * kPi;
            const V3 T = pathTangent(), P = pathPerp(), Z{0, 0, 1};
            c.axisBase = norm(add(mul(T, std::cos(a)), add(mul(P, std::sin(a) * std::cos(b)), mul(Z, std::sin(a) * std::sin(b)))));
            c.axis = c.axisBase;
            c.axisPhase = ar.next() * 2.0f * kPi;
            c.breathe = ar.next() * 2.0f * kPi;
        }
        // slot along the path; seeded offsets sideways and in depth give the field its own route
        const int m = static_cast<int>(in.chain.size());
        const float t = m <= 1 ? 0.5f : 0.10f + 0.80f * static_cast<float>(c.index) / static_cast<float>(m - 1);
        Rng jr(mix64(c.seed ^ 0xC0FFEEull));
        const float e = extentTarget_;
        const V3 base = pathPoint(t, e);
        const V3 lateral = mul(pathPerp(), (0.42f * jr.bip()) * (0.6f + 0.4f * e));
        const V3 depth{0, 0, 0.70f * jr.bip()};
        const V3 along = mul(pathTangent(), 0.10f * jr.bip());
        c.target = add(base, add(lateral, add(depth, along)));
        if (fresh) {
            c.anchor = c.target;
            c.presence = first_ ? 1.0f : 0.0f;
            c.grow = first_ ? 1.0f : 0.0f;
            // the first frame (editor opened / reopened) shows the committed chain as it is; afterwards a new
            // instance waits invisibly for a possible COMMIT transfer
            c.life = first_ ? Life::Integrated : Life::Probation;
            c.probation = first_ ? 0.0f : kProbation;
        }
    }
    // transfer requests: wait for their instance (up to 2 s) instead of being dropped
    // (one request per id: a double click or a repeated request can never create a second packet)
    for (const auto& tr : in.transfers)
        if (tr.id != 0 && pending_.size() < 64 && std::none_of(pending_.begin(), pending_.end(), [&](const Pending& p) { return p.req.id == tr.id; }))
            pending_.push_back({tr, 0.0f});
    for (auto it = pending_.begin(); it != pending_.end();) {
        auto f = insts_.find(it->req.id);
        if (f != insts_.end() && f->second.inChain && f->second.life == Life::Integrated) {
            it = pending_.erase(it);  // already part of the stable formation (late or repeated request): no phantom
        } else if (f != insts_.end() && f->second.inChain) {
            Inst& c = f->second;
            c.life = Life::Transferring;
            c.transfer = true;
            c.tTransfer = 0.0f;
            c.originY = std::isfinite(it->req.originY) ? std::clamp(it->req.originY, 0.02f, 0.98f) : 0.35f;
            c.origin = unproject(0.0f, c.originY, 0.6f);
            c.hold = 0.0f;
            c.grow = 0.0f;
            c.presence = 0.0f;  // V3: nothing of the stable formation until the packet has arrived
            it = pending_.erase(it);
        } else if (it->age > 2.0f) {
            it = pending_.erase(it);
        } else {
            ++it;
        }
    }
}

void Formation::emitPolyline(const std::vector<V3>& pts, const Rgb& c0, const Rgb& c1, float gain, const Inst* pulsesOf, bool fineCore) {
    const size_t np = pts.size();
    if (np < 2) return;
    gain = std::max(0.0f, gain);
    float px = 0, py = 0, pd = 0;
    project(pts[0], px, py, pd);
    for (size_t k = 1; k < np; ++k) {
        float x, y, d;
        project(pts[k], x, y, d);
        const float u = static_cast<float>(k) / static_cast<float>(np - 1);
        // fade in / out along the strand: soft tips, luminous body
        const float tip = smooth01(std::min(u, 1.0f - u) * 5.0f + 0.12f);
        // depth: near strands are brighter and thicker, far ones dim and cool towards indigo
        const float dd = 0.5f * (d + pd);
        const float fog = std::exp(-std::max(0.0f, dd - kCamDist) * 0.42f);
        const float nearB = 1.0f + 0.45f * std::clamp((kCamDist - dd) / 0.8f, 0.0f, 1.0f);
        // filaments that come too close to the camera fade out instead of sweeping across the view
        const float nearFade = smooth01((dd - 2.4f) / 0.7f);
        float g = gain * tip * fog * nearB * nearFade;
        if (pulsesOf) {
            for (const auto& p : pulsesOf->pulses) {
                const float pu = p.age / kPulseTravel;
                const float x0 = (u - pu) / 0.07f;
                g *= 1.0f + 2.4f * p.strength * std::exp(-x0 * x0);
            }
        }
        Rgb c = lerpC(c0, c1, u);
        const float cool = (1.0f - fog) * 0.45f;
        c = lerpC(c, {0.36f, 0.36f, 0.78f}, cool);
        c = shade(c);
        const float w = (!fineCore && dd < kCamDist - 0.25f && gain > 0.9f) ? 2.0f : 1.0f;
        if (x == px && y == py) g = 0.0f;  // parked end of a finished strand: keeps the count, draws nothing
        if (std::isfinite(x + y + px + py)) segs_.push_back({px, py, x, y, c.r * g, c.g * g, c.b * g, w});
        px = x;
        py = y;
        pd = d;
    }
}

void Formation::emitStrand(Inst& inst, int ni, int si, float lengthFrac, float dt) {
    if (lengthFrac <= 0.001f) return;
    const Strand& s = inst.strands[static_cast<size_t>(si)];
    const Node& node = nodes_[static_cast<size_t>(ni)];
    const int nn = static_cast<int>(nodes_.size());
    const int steps = std::max(2, static_cast<int>(static_cast<float>(s.steps) * lengthFrac));
    const float sway = 0.03f * std::sin(time_ * 0.21f + s.phase);
    pts_.clear();
    float gain = kGain * s.gain * inst.presence * (0.55f + 0.55f * energy_ + 0.75f * inst.bright);
    Rgb c0 = inst.colour, c1 = inst.colour;
    const Inst* pulses = nullptr;
    if (s.kind == 0) {
        // arc: a field line leaving the node near a pole, bending around the energy centre
        // leave near the pole the strand belongs to (continuous in the axis: no switch as the axis precesses)
        const V3 dir = norm(add(s.start, mul(node.axis, 1.1f * static_cast<float>(s.dir))));
        V3 p = add(node.p, mul(dir, node.radius * (1.4f + 1.4f * std::fabs(s.start.y)) + sway));
        const float h = 0.034f * s.lenScale;
        bool alive = true;  // fixed point count per strand: a finished strand parks at its end
        for (int k = 0; k <= steps; ++k) {
            pts_.push_back(p);
            if (!alive) continue;
            // soft normalisation: near the field's null points the step shrinks smoothly instead of flipping
            // direction, which keeps field lines from jumping between branches from frame to frame
            auto soft = [](V3 f) { return mul(f, 1.0f / (len(f) + 0.9f)); };
            const V3 v1 = mul(soft(field(p, time_)), static_cast<float>(s.dir));
            const V3 mid = add(p, mul(v1, 0.5f * h));
            const V3 v2 = mul(soft(field(mid, time_)), static_cast<float>(s.dir));
            const V3 q = add(p, mul(v2, h));
            if (!finite3(q) || len(sub(q, node.p)) > 2.2f) alive = false;
            else p = q;
        }
        pulses = &inst;
    } else if (s.kind == 1) {
        // transmission towards the next node (or the one after); input strands only on the first node
        int target = s.target == 2 ? ni + 2 : ni + 1;
        if (s.target == 3 && ni != 0) return;
        if (s.target == 2 && target >= nn) target = ni + 1;
        V3 goal;
        bool toOutput = false, fromInput = s.target == 3;
        // every strand aims at its own point on the target's shell: bundles spread and arrive from many
        // directions instead of converging into one hard junction / straight spine
        const V3 aim = mul(norm({s.start.z, s.start.x, s.start.y}), 1.0f);
        if (fromInput) {
            goal = add(node.p, mul(aim, node.radius * 2.2f));
        } else if (target < nn && nodes_[static_cast<size_t>(target)].strength <= 0.0f) {
            return;  // V3: no bundle reaches an instance that has not integrated yet
        } else if (target < nn) {
            goal = add(nodes_[static_cast<size_t>(target)].p, mul(aim, nodes_[static_cast<size_t>(target)].radius * (1.8f + 1.6f * std::fabs(s.start.x))));
            const auto& other = insts_.find(order_[static_cast<size_t>(target)]);
            if (other != insts_.end()) c1 = other->second.colour;
        } else {
            toOutput = true;  // last node: an understated output path towards the top-right
            goal = add(node.p, mul(pathTangent(), 0.9f));
        }
        V3 p = fromInput ? add(mul(kIn, std::max(0.6f, extent_)), mul(s.start, 0.35f)) : add(node.p, mul(s.start, node.radius * 1.4f));
        const float h = 0.046f;
        bool alive = true;
        for (int k = 0; k <= steps * 2; ++k) {
            pts_.push_back(p);
            if (!alive) continue;
            const V3 toGoal = sub(goal, p);
            const float dist = len(toGoal);
            if (dist < 0.09f) { alive = false; continue; }
            // attraction + the field's bending + a little curl: the strand finds a coherent path, not a line
            const V3 f = norm(field(p, time_));
            const V3 cn = curl(add(p, mul(s.start, 0.6f)), time_ + s.phase);
            const float pull = std::clamp(0.35f + 0.35f / std::max(0.2f, dist), 0.35f, 1.8f);
            const V3 v = norm(add(mul(norm(toGoal), pull), add(mul(f, 0.85f), mul(cn, 0.6f))));
            const V3 q = add(p, mul(v, h));
            if (!finite3(q)) alive = false;
            else p = q;
        }
        if (toOutput) gain *= 0.45f;
        if (fromInput) gain *= 0.4f;
        pulses = fromInput ? nullptr : &inst;
    } else {
        // wisp: a short, faint curl streamline in the node's neighbourhood; the high band lights it
        V3 p = add(node.p, mul(s.start, node.radius * (2.0f + 3.0f * std::fabs(s.start.z))));
        const float h = 0.03f;
        for (int k = 0; k <= steps; ++k) {
            pts_.push_back(p);
            const V3 q = add(p, mul(norm(add(curl(p, time_ + s.phase), mul(norm(field(p, time_)), 0.3f))), h));
            if (finite3(q)) p = q;
        }
        gain *= 0.55f + 1.4f * high_;
    }
    // Temporal smoothing in world space. Field lines are sensitive near the field's null points: a strand
    // can switch to a neighbouring branch from one frame to the next. Easing every point towards its newly
    // traced position turns such a switch into a ~0.5 s glide instead of a pop.
    if (inst.trail.size() != inst.strands.size()) inst.trail.resize(inst.strands.size());
    auto& tr = inst.trail[static_cast<size_t>(si)];
    if (tr.size() != pts_.size()) {
        tr = pts_;
    } else {
        const float a = 1.0f - std::exp(-std::max(0.0f, dt) / 0.5f);
        for (size_t k = 0; k < pts_.size(); ++k) {
            tr[k] = add(tr[k], mul(sub(pts_[k], tr[k]), a));
            pts_[k] = tr[k];
        }
    }
    // hotspot: brighter near the node's energy centre (intersections and cores read as highlights)
    if (!pts_.empty()) {
        const float d0 = len(sub(pts_[0], node.p));
        gain *= 1.0f + 0.25f * std::exp(-d0 * d0 / 0.03f);
    }
    emitPolyline(pts_, c0, c1, gain, pulses, s.kind == 2);
}

void Formation::emitTransfer(const Inst& inst) {
    // a packet of the FX's own field lines travels from the workflow column into its node
    const float tt = inst.tTransfer;
    if (tt > kTransferDur + 0.25f) return;
    const float u = easeInOut(tt / kTransferDur);
    const V3 a = inst.origin, b = inst.anchor;
    const V3 ctrl = add(mul(add(a, b), 0.5f), {0.0f, 0.45f, 0.35f});
    auto bez = [&](float t) { return add(add(mul(a, (1 - t) * (1 - t)), mul(ctrl, 2 * (1 - t) * t)), mul(b, t * t)); };
    const V3 q = bez(std::min(u, 1.0f));
    const V3 q2 = bez(std::min(1.0f, u + 0.02f));
    const V3 axis = norm(sub(q2, q));
    const float env = smooth01(tt / 0.12f) * (1.0f - smooth01((tt - kTransferDur) / 0.25f));
    const float radius = 0.26f - 0.16f * u;
    const Rgb col = inst.colour;
    Rng r(mix64(inst.seed ^ 0x7A4Eull));
    // the packet: dipole loops around the travelling centre (the same field language as the nodes)
    for (int k = 0; k < 22; ++k) {
        V3 dir = r.sphere();
        const float sgn = dot(dir, axis) >= 0.0f ? 1.0f : -1.0f;
        dir = norm(add(dir, mul(axis, 0.8f * sgn)));
        V3 p = add(q, mul(dir, radius * 0.35f));
        pts_.clear();
        for (int s = 0; s < 26; ++s) {
            pts_.push_back(p);
            const V3 rr = sub(p, q);
            const float r2 = dot(rr, rr) + radius * radius * 0.08f;
            const V3 bf = sub(mul(rr, 3.0f * dot(axis, rr) / r2), axis);
            const V3 v = norm(add(mul(norm(bf), sgn), mul(curl(p, time_ + static_cast<float>(k)), 0.25f)));
            p = add(p, mul(v, radius * 0.11f));
            if (len(sub(p, q)) > radius * 1.6f) break;
        }
        emitPolyline(pts_, col, col, kGain * 1.25f * env * (0.4f + r.next()), nullptr, false);
    }
    // a faint comet trail along the path behind it
    pts_.clear();
    for (int s = 0; s <= 24; ++s) pts_.push_back(bez(std::max(0.0f, u - 0.28f * (1.0f - static_cast<float>(s) / 24.0f))));
    emitPolyline(pts_, col, col, kGain * 0.6f * env, nullptr, true);
}

void Formation::update(const FrameInput& in) {
    const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.0f, 0.1f) : 1.0f / 60.0f;
    time_ += dt;
    aspect_ = std::isfinite(in.aspect) ? std::clamp(in.aspect, 0.25f, 8.0f) : 1.6f;
    for (auto& p : pending_) p.age += dt;
    layout(in);
    extent_ += (extentTarget_ - extent_) * (1.0f - std::exp(-dt / 1.2f));
    yaw_ = 0.05f * std::sin(time_ * 2.0f * kPi / 90.0f);
    pitch_ = 0.035f * std::sin(time_ * 2.0f * kPi / 76.0f + 1.0f);
    mono_ += (safe(in.monochrome, 0.0f, 1.0f) - mono_) * (1.0f - std::exp(-dt / 0.15f));

    // ---- audio analysis snapshot: attack / release smoothing, bounded mappings (no flicker) ----
    follow(energy_, safe(in.audio.energy, 0.0f, 1.5f), dt, 0.08f, 0.6f);
    follow(low_, safe(in.audio.low, 0.0f, 1.5f), dt, 0.25f, 1.2f);
    follow(high_, safe(in.audio.high, 0.0f, 1.5f), dt, 0.05f, 0.4f);
    follow(balance_, safe(in.audio.balance, -1.0f, 1.0f), dt, 0.4f, 0.8f);
    const float tr = safe(in.audio.transient, 0.0f, 1.0f);
    const bool onset = tr > 0.3f && transient_ < 0.15f;
    transient_ = std::max(tr, transient_ - dt * 4.0f);

    // ---- instances ----
    for (auto it = insts_.begin(); it != insts_.end();) {
        Inst& c = it->second;
        if (!c.inChain) {
            c.presence -= dt / 0.6f;
            c.pulses.clear();
            if (c.presence <= 0.0f) { it = insts_.erase(it); continue; }
        } else if (c.life == Life::Probation) {
            c.presence = 0.0f;
            c.grow = 0.0f;
            c.probation -= dt;
            if (c.probation <= 0.0f) c.life = Life::Integrated;  // no transfer came: a plain fade-in (preset load)
        } else if (c.life == Life::Transferring) {
            c.presence = 0.0f;
            c.grow = 0.0f;
        } else {
            c.presence = std::min(1.0f, c.presence + dt / (c.transfer ? 0.35f : 0.9f));
            if (c.hold > 0.0f) c.hold -= dt;
            else c.grow = std::min(1.0f, c.grow + dt / 1.2f);
        }
        const float k = 1.0f - std::exp(-dt / 1.1f);  // reorders glide
        c.anchor = add(c.anchor, mul(sub(c.target, c.anchor), k));
        const float br = 0.035f * (1.0f + 1.5f * low_);
        c.anchor = add(c.anchor, {br * std::sin(time_ * 0.19f + c.breathe) * dt, br * std::cos(time_ * 0.16f + c.breathe) * dt, 0.0f});
        const V3 prec{0.16f * std::sin(time_ * 0.07f + c.axisPhase), 0.16f * std::cos(time_ * 0.05f + c.axisPhase), 0.16f * std::sin(time_ * 0.06f + 2.0f * c.axisPhase)};
        c.axis = norm(add(c.axisBase, prec));
        if (c.transfer) {
            c.tTransfer += dt;
            if (c.life == Life::Transferring && c.tTransfer >= kTransferDur) {
                // arrival: the completion event. Only now does the instance join the stable formation.
                c.life = Life::Integrated;
                ++integratedCount_;
                if (c.pulses.size() < 4) c.pulses.push_back({0.0f, 1.0f, true});
            }
            if (c.tTransfer > kTransferDur + 0.3f) c.transfer = false;  // the packet's tail has faded
        }
        it = std::next(it);
    }
    // per-node levels and onsets
    for (const auto& ii : in.chain) {
        auto f = insts_.find(ii.id);
        if (f == insts_.end()) continue;
        Inst& c = f->second;
        const float lv = levelMap(ii.level);
        follow(c.bright, lv, dt, 0.05f, 0.35f);
        follow(c.levelSlow, lv, dt, 0.4f, 0.4f);
        if (c.bright - c.levelSlow > 0.22f && c.pulses.size() < 3 && (c.pulses.empty() || c.pulses.back().age > 0.25f))
            c.pulses.push_back({0.0f, std::clamp((c.bright - c.levelSlow) * 2.5f, 0.3f, 1.0f), true});
    }
    if (onset && !order_.empty()) {
        Inst& c = insts_[order_.front()];
        if (c.pulses.size() < 4) c.pulses.push_back({0.0f, std::clamp(tr, 0.4f, 1.0f), true});
    }
    // advance pulses; a pulse that reaches the next node continues there, weaker (A -> B transmission)
    for (size_t i = 0; i < order_.size(); ++i) {
        Inst& c = insts_[order_[i]];
        for (auto& p : c.pulses) p.age += dt;
        for (auto& p : c.pulses) {
            if (p.cascade && p.age >= kPulseTravel && i + 1 < order_.size() && p.strength > 0.25f) {
                Inst& nxt = insts_[order_[i + 1]];
                if (nxt.pulses.size() < 4) nxt.pulses.push_back({0.0f, p.strength * 0.72f, true});
                p.cascade = false;
            }
        }
        c.pulses.erase(std::remove_if(c.pulses.begin(), c.pulses.end(), [](const Pulse& p) { return p.age > kPulseTravel * 1.3f; }), c.pulses.end());
    }

    // ---- nodes for this frame (chain order) ----
    nodes_.clear();
    for (size_t i = 0; i < order_.size(); ++i) {
        const Inst& c = insts_[order_[i]];
        // a node that is not integrated has no field at all (it must not bend other nodes' field lines either)
        const float strength = c.life != Life::Integrated ? 0.0f : (0.35f + 0.65f * c.presence) * (1.0f + 0.25f * energy_ + 0.3f * c.bright);
        nodes_.push_back({c.anchor, c.axis, strength, 0.16f + 0.05f * c.bright, static_cast<int>(i)});
    }

    // ---- emit ----
    segs_.clear();
    glows_.clear();
    strands_ = 0;
    const int budget = quality_ == Quality::Low ? 800 : (quality_ == Quality::High ? 2600 : 1500);
    const int nn = std::max(1, static_cast<int>(nodes_.size()));
    const int perNode = std::clamp(budget / nn, 40, kMaxStrandsPerInst);
    for (size_t i = 0; i < order_.size(); ++i) {
        Inst& c = insts_[order_[i]];
        const float frac = smooth01(c.grow);
        if (frac > 0.0f) {
            const int count = std::min(perNode, static_cast<int>(c.strands.size()));
            for (int k = 0; k < count; ++k) {
                emitStrand(c, static_cast<int>(i), k, frac, dt);
                ++strands_;
            }
        }
        if (c.transfer) emitTransfer(c);
        // the field's influence on empty space: a faint deep-violet response around each node
        float gx, gy, gd;
        project(c.anchor, gx, gy, gd);
        const float k = kCamDist / gd;
        // normalised by chain length so a long chain never becomes a broad purple wall
        const float inten = c.presence * (0.55f + 0.5f * energy_ + 0.45f * c.bright) / (1.0f + 0.18f * static_cast<float>(order_.size() - 1));
        const Rgb violet{0.030f, 0.014f, 0.068f};
        const Rgb tint = lerpC(violet, c.colour, 0.18f);
        const Rgb g = shade(tint);
        glows_.push_back({gx, gy, (0.22f + 0.05f * c.bright) * k, g.r * inten, g.g * inten, g.b * inten});
        if (i + 1 < order_.size()) {
            const Inst& d = insts_[order_[i + 1]];
            float hx, hy, hd;
            project(mul(add(c.anchor, d.anchor), 0.5f), hx, hy, hd);
            const float in2 = 0.5f * std::min(c.presence, d.presence) * (0.5f + 0.5f * energy_);
            glows_.push_back({hx, hy, 0.13f * kCamDist / hd, g.r * in2, g.g * in2, g.b * in2});
        }
    }
    // dying instances (deleted / cleared) fade out without ghosts: their strands are simply not drawn once
    // they leave the chain, and the record is erased when the fade completes.
    first_ = false;
}


}  // namespace ih1::visual
