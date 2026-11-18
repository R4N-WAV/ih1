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
constexpr float kPi = 3.14159265358979f;
constexpr float kCamDist = 4.0f;  // camera distance (world units)
constexpr float kScale = 0.34f;   // field-height units per world unit at the focal distance
constexpr int kMaxStrandsPerInst = 210;
constexpr float kTransferDur = 0.65f;
constexpr float kGain = 4.3f;  // overall strand intensity (tuned against the reference with exposure 1.6)

uint64_t mix64(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed) {}
    float next() { s += 0x9E3779B97F4A7C15ull; return static_cast<float>(mix64(s) >> 40) * (1.0f / 16777216.0f); }
    float gauss() {
        const float u = std::max(1e-6f, next()), v = next();
        return std::sqrt(-2.0f * std::log(u)) * std::cos(2.0f * kPi * v);
    }
};

// Divergence-free flow: v(p, t) = sum_k a_k cos(w_k . p + phi_k(t)) (w_k x e_k). Each term is the curl of
// a_k sin(w_k . p + phi_k) e_k, so the sum is exactly divergence-free. Wavelengths 0.7..2.6 world units, phase
// drift 0.05..0.12 rad/s: smooth, low-frequency and slow.
struct FlowTerm { float wx, wy, wz, cx, cy, cz, phase, omega; };
constexpr int kTerms = 9;
const std::array<FlowTerm, kTerms>& flowTerms() {
    static const std::array<FlowTerm, kTerms> terms = [] {
        std::array<FlowTerm, kTerms> t{};
        Rng r(0x1A1ull ^ 0xA5A5F00Dull);
        for (int k = 0; k < kTerms; ++k) {
            float dx = r.gauss(), dy = r.gauss(), dz = r.gauss();
            float l = std::sqrt(dx * dx + dy * dy + dz * dz) + 1e-6f;
            const float freq = 2.0f * kPi / (1.3f + 2.2f * r.next());
            const float wx = dx / l * freq, wy = dy / l * freq, wz = dz / l * freq;
            float ex = r.gauss(), ey = r.gauss(), ez = r.gauss();
            // c = w x e, normalised so each term has unit amplitude
            float cx = wy * ez - wz * ey, cy = wz * ex - wx * ez, cz = wx * ey - wy * ex;
            l = std::sqrt(cx * cx + cy * cy + cz * cz) + 1e-6f;
            t[static_cast<size_t>(k)] = {wx, wy, wz, cx / l, cy / l, cz / l, r.next() * 2.0f * kPi, (0.05f + 0.07f * r.next()) * (r.next() < 0.5f ? -1.0f : 1.0f)};
        }
        return t;
    }();
    return terms;
}

float smooth01(float x) { x = std::clamp(x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }
float easeInOut(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x < 0.5f ? 4.0f * x * x * x : 1.0f - std::pow(-2.0f * x + 2.0f, 3.0f) * 0.5f;
}
float safeLevel(float x) { return std::isfinite(x) ? std::clamp(std::sqrt(std::max(0.0f, x)) * 1.6f, 0.0f, 1.2f) : 0.0f; }
}  // namespace

// ------------------------------------------------------------------------------------- formation
Formation::Formation(Quality q) : quality_(q) {
    segs_.reserve(120000);
    pts_.reserve(128);
}

float Formation::decay(float dt) const { return std::pow(0.5f, std::clamp(dt, 0.0f, 0.2f) * 60.0f); }

bool Formation::transferActive() const {
    for (const auto& kv : insts_) if (kv.second.transfer) return true;
    return false;
}

Formation::V3 Formation::field(const V3& p, float t) const {
    V3 v;
    for (const auto& f : flowTerms()) {
        const float c = std::cos(f.wx * p.x + f.wy * p.y + f.wz * p.z + f.phase + f.omega * t);
        v.x += c * f.cx;
        v.y += c * f.cy;
        v.z += c * f.cz;
    }
    const float k = 1.0f / 2.1f;
    return {v.x * k, v.y * k, v.z * k};
}

void Formation::project(const V3& p, float& x, float& y, float& depth) const {
    const float cy = std::cos(yaw_), sy = std::sin(yaw_);
    const float x1 = cy * p.x + sy * p.z, z1 = -sy * p.x + cy * p.z;
    const float cp = std::cos(pitch_), sp = std::sin(pitch_);
    const float y2 = cp * p.y - sp * z1, z2 = sp * p.y + cp * z1;
    depth = std::max(0.6f, kCamDist - z2);
    const float k = kCamDist / depth;
    // composition: the core sits right of centre with room on the left for the arm (landscape field)
    const float cx = std::clamp(aspect_ * 0.64f, aspect_ * 0.5f, std::max(aspect_ * 0.5f, aspect_ - 0.36f));
    x = cx + x1 * k * kScale;
    y = 0.5f - y2 * k * kScale;
}

void Formation::buildStrands(Inst& inst, int count) {
    inst.strands.clear();
    inst.strands.reserve(static_cast<size_t>(count));
    for (int k = 0; k < count; ++k) {
        Rng r(mix64((static_cast<uint64_t>(inst.seed) << 20) ^ static_cast<uint64_t>(k) ^ 0x5EEDull));
        Strand s;
        const float kindR = r.next();
        s.kind = kindR < 0.50f ? 0 : (kindR < 0.70f ? 1 : (kindR < 0.80f ? 2 : 3));  // hair, wisp, spark, veil
        s.offset = {r.gauss(), r.gauss(), r.gauss()};
        s.drift = {r.next() * 2.0f * kPi, r.next() * 2.0f * kPi, r.next() * 2.0f * kPi};
        const float g = r.next();
        s.gain = 0.22f + 2.2f * g * g * g;  // mostly faint, a few bright
        s.turb = s.kind == 0 ? 0.45f + 0.3f * r.next() : (s.kind == 1 ? 1.1f + 0.5f * r.next() : (s.kind == 2 ? 0.7f : 0.75f + 0.3f * r.next()));
        s.phase = r.next() * 2.0f * kPi;
        s.steps = static_cast<uint8_t>(s.kind == 0 ? 10 + static_cast<int>(22 * r.next() * r.next()) + static_cast<int>(8 * r.next())
                                       : (s.kind == 1 ? 8 + static_cast<int>(8 * r.next()) : (s.kind == 2 ? 4 + static_cast<int>(4 * r.next()) : 10 + static_cast<int>(10 * r.next()))));
        s.dirBias = (r.next() - 0.5f) * 0.75f;  // fan angle within the sheet (radians): a plume, not a burst
        inst.strands.push_back(s);
    }
}

void Formation::layout(const FrameInput& in) {
    const int n = static_cast<int>(in.chain.size());
    extentTarget_ = 0.42f + 0.58f * std::min(1.0f, static_cast<float>(std::max(0, n - 1)) / 6.0f);
    for (auto& kv : insts_) kv.second.inChain = false;
    for (int i = 0; i < n; ++i) {
        const auto& ii = in.chain[static_cast<size_t>(i)];
        auto it = insts_.find(ii.id);
        const bool fresh = it == insts_.end();
        Inst& c = insts_[ii.id];
        c.inChain = true;
        c.index = i;
        auto ch = [](float v) { return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f; };
        c.colour = {ch(ii.colour.r), ch(ii.colour.g), ch(ii.colour.b)};
        if (fresh || c.seed != ii.seed || c.strands.empty()) {
            c.id = ii.id;
            c.seed = ii.seed ? ii.seed : 1u;
            buildStrands(c, kMaxStrandsPerInst);
        }
        // slot on the skeleton: first FX at the arm's tip, through the core, last FX up/down the plume
        V3 pos, tan;
        float plume;
        Rng jr(mix64(c.seed ^ 0xC0FFEEull));
        if (n == 1) {
            pos = {0, 0, 0};
            tan = {0.1f, 1.0f, -0.12f};  // a single FX is a small vertical plume, not a radial burst
            plume = 0.85f;
        } else {
            const float t = static_cast<float>(i) / static_cast<float>(n - 1);
            if (t <= 0.5f) {
                const float s = (0.5f - t) / 0.5f * 0.95f;
                pos = {-1.40f * s, -0.05f * s - 0.06f * s * s, 0.50f * s};
                tan = {-1.40f, -0.05f - 0.12f * s, 0.50f};
                plume = std::clamp(0.5f - s * 3.0f, 0.0f, 0.5f);
            } else {
                const float m = (t - 0.5f) / 0.5f * 0.85f;
                const float sgn = (i % 2) ? 1.0f : -1.0f;
                const float s = sgn * m;
                pos = {0.06f * s + 0.05f * s * s, 0.95f * s, -0.15f * s};
                tan = {0.06f, 0.95f, -0.15f};
                plume = std::clamp(0.5f + m * 2.0f, 0.5f, 1.0f);
            }
        }
        const float e = extentTarget_;
        c.target = {pos.x * e + (jr.next() - 0.5f) * 0.12f, pos.y * e + (jr.next() - 0.5f) * 0.10f, pos.z * e + (jr.next() - 0.5f) * 0.12f};
        const float tl = std::sqrt(tan.x * tan.x + tan.y * tan.y + tan.z * tan.z) + 1e-6f;
        c.tangentTarget = {tan.x / tl, tan.y / tl, tan.z / tl};
        c.plumeTarget = plume;
        if (fresh) {
            c.anchor = c.target;
            c.tangent = c.tangentTarget;
            c.plume = plume;
            c.presence = first_ ? 1.0f : 0.0f;
            c.grow = first_ ? 1.0f : 0.0f;
        }
    }
    for (const auto& tr : in.transfers) {
        auto it = insts_.find(tr.id);
        if (it == insts_.end() || !it->second.inChain) continue;
        Inst& c = it->second;
        c.transfer = true;
        c.tTransfer = 0.0f;
        c.originY = std::isfinite(tr.originY) ? std::clamp(tr.originY, 0.02f, 0.98f) : 0.35f;
        c.hold = kTransferDur * 0.9f;
        c.grow = 0.0f;
        c.presence = 1.0f;
    }
}

void Formation::emitStrand(const Inst& inst, const Strand& s, float lengthFrac, float bright) {
    if (lengthFrac <= 0.001f) return;
    // local frame: T along the skeleton, N the sheet normal, B across within the sheet
    const V3 T = inst.tangent;
    const float pl = inst.plume;
    V3 N{0.0f * (1 - pl) + 0.18f * pl, 1.0f * (1 - pl) + 0.0f * pl, 0.0f * (1 - pl) + 1.0f * pl};
    {
        // orthogonalise against T
        const float d = N.x * T.x + N.y * T.y + N.z * T.z;
        N = {N.x - d * T.x, N.y - d * T.y, N.z - d * T.z};
        const float l = std::sqrt(N.x * N.x + N.y * N.y + N.z * N.z) + 1e-6f;
        N = {N.x / l, N.y / l, N.z / l};
    }
    const V3 B{N.y * T.z - N.z * T.y, N.z * T.x - N.x * T.z, N.x * T.y - N.y * T.x};
    const float e = std::sqrt(extent_);
    const float t = time_;
    // seed point: gaussian in the local frame, thin across the sheet, breathing slowly
    const float br = 0.025f;
    const V3 o{s.offset.x * (0.24f + 0.06f * pl) * e, s.offset.y * (0.13f + 0.17f * pl) * e, s.offset.z * (0.06f + 0.05f * pl)};
    V3 p0{inst.anchor.x + T.x * o.x + B.x * o.y + N.x * o.z + br * std::sin(0.11f * t + s.drift.x),
          inst.anchor.y + T.y * o.x + B.y * o.y + N.y * o.z + br * std::sin(0.083f * t + s.drift.y),
          inst.anchor.z + T.z * o.x + B.z * o.y + N.z * o.z + br * std::sin(0.067f * t + s.drift.z)};
    if (s.kind == 2) {
        // sparks from every instance gather where the sheets cross: overlapping colours make the pale,
        // luminous core of the reference (the brightest energy stays focused there)
        const float k = 0.62f;
        p0 = {(inst.anchor.x + (p0.x - inst.anchor.x) * 0.35f) * (1.0f - k), (inst.anchor.y + (p0.y - inst.anchor.y) * 0.35f) * (1.0f - k),
              (inst.anchor.z + (p0.z - inst.anchor.z) * 0.35f) * (1.0f - k)};
    }
    const float ca = std::cos(s.dirBias), sa = std::sin(s.dirBias);
    const V3 guide{T.x * ca + B.x * sa, T.y * ca + B.y * sa, T.z * ca + B.z * sa};
    const float tension = 1.0f + 0.12f * energy_;  // audio tightens strands slightly, never spins them
    const float turb = s.turb * (0.8f + 0.55f * pl) / tension;  // the plume is smokier than the arm
    const float h = (s.kind == 0 ? 0.034f : (s.kind == 1 ? 0.028f : 0.02f)) * (0.55f + 0.45f * e) * lengthFrac;
    int steps = s.steps;
    if (quality_ == Quality::Low) steps = std::max(3, steps * 3 / 5);
    else if (quality_ == Quality::High) steps = steps * 5 / 4;
    const float fieldT = t * 1.0f;

    // trace both ways from the seed
    pts_.clear();
    pts_.resize(static_cast<size_t>(2 * steps + 1));
    pts_[static_cast<size_t>(steps)] = p0;
    for (int dir = -1; dir <= 1; dir += 2) {
        V3 p = p0;
        for (int k = 1; k <= steps; ++k) {
            // midpoint (RK2) step on the guided field: smooth curves, no kinks
            V3 f = field(p, fieldT);
            {
                const V3 gd0{guide.x * dir, guide.y * dir, guide.z * dir};
                const float ft = turb * static_cast<float>(dir);  // a streamline: backwards = -(guide + field)
                V3 v0{gd0.x + f.x * ft, gd0.y + f.y * ft, gd0.z + f.z * ft};
                const float l0 = std::sqrt(v0.x * v0.x + v0.y * v0.y + v0.z * v0.z) + 1e-6f;
                f = field({p.x + v0.x / l0 * h * 0.5f, p.y + v0.y / l0 * h * 0.5f, p.z + v0.z / l0 * h * 0.5f}, fieldT);
            }
            // keep the sheet thin: pull back toward the anchor along the sheet normal
            const float dn = (p.x - inst.anchor.x) * N.x + (p.y - inst.anchor.y) * N.y + (p.z - inst.anchor.z) * N.z;
            const V3 gd{guide.x * dir, guide.y * dir, guide.z * dir};
            const float ft = turb * static_cast<float>(dir);
            V3 v{gd.x + f.x * ft - N.x * dn * 2.2f, gd.y + f.y * ft - N.y * dn * 2.2f, gd.z + f.z * ft - N.z * dn * 2.2f};
            // always keep moving forward along the guide: where the field opposes it the step would cancel
            // and the strand's tail could flip from one frame to the next. This clamp is continuous.
            const float fwd = v.x * gd.x + v.y * gd.y + v.z * gd.z;
            if (fwd < 0.4f) { v.x += (0.4f - fwd) * gd.x; v.y += (0.4f - fwd) * gd.y; v.z += (0.4f - fwd) * gd.z; }
            const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z) + 1e-6f;
            p = {p.x + v.x / l * h, p.y + v.y / l * h, p.z + v.z / l * h};
            pts_[static_cast<size_t>(steps + dir * k)] = p;
        }
    }

    // colour: instance identity; sparks lean pale; depth fog dims and cools the far side
    Rgb col = inst.colour;
    if (s.kind == 2) col = {col.r * 0.45f + 0.52f, col.g * 0.45f + 0.49f, col.b * 0.45f + 0.55f};
    const float kindGain = (s.kind == 0 ? 0.034f : (s.kind == 1 ? 0.022f : (s.kind == 2 ? 0.11f : 0.009f))) * kGain;
    const float base = kindGain * s.gain * inst.presence * (0.72f + 0.65f * bright);
    const int np = 2 * steps + 1;
    // veils: the same traced curve emitted as a few near-parallel copies across the sheet (a translucent
    // smoky band); every other kind is a single filament
    const int copies = s.kind == 3 ? 6 : 1;
    for (int cpy = 0; cpy < copies; ++cpy) {
        const float sh = copies > 1 ? (static_cast<float>(cpy) - 2.5f) * 0.016f * e : 0.0f;
        const V3 d{B.x * sh, B.y * sh, B.z * sh};
        float px0 = 0, py0 = 0, d0 = 0;
        project({pts_[0].x + d.x, pts_[0].y + d.y, pts_[0].z + d.z}, px0, py0, d0);
        for (int j = 1; j < np; ++j) {
            const V3& q = pts_[static_cast<size_t>(j)];
            float px1, py1, d1;
            project({q.x + d.x, q.y + d.y, q.z + d.z}, px1, py1, d1);
            const float sm = (static_cast<float>(j) - 0.5f) / static_cast<float>(np - 1) * 2.0f - 1.0f;  // -1..1 along the strand
            const float taper = std::pow(std::max(0.0f, 1.0f - sm * sm), 1.3f);
            const float bead = 0.78f + 0.22f * std::sin(6.5f * sm + s.phase + 0.21f * t);
            const float depth = 0.5f * (d0 + d1);
            const float fog = std::clamp(1.3f - 0.42f * (depth - kCamDist), 0.4f, 1.7f);
            const float a = base * taper * bead * fog;
            // always emitted (stable segment order frame to frame); the renderer clips to the field
            const float cool = std::clamp((depth - kCamDist) * 0.25f, 0.0f, 0.35f);  // far: toward indigo
            segs_.push_back({px0, py0, px1, py1, a * (col.r * (1 - cool) + 0.30f * cool), a * (col.g * (1 - cool) + 0.32f * cool), a * (col.b * (1 - cool) + 0.85f * cool)});
            px0 = px1; py0 = py1; d0 = d1;
        }
    }
}

void Formation::emitTransfer(const Inst& inst) {
    // Packet: the FX's colour gathers at the left edge of the field, travels along a gentle arc into its new
    // region, then disperses as a short burst while the region's strands grow out of it.
    float ex, ey, ed;
    project(inst.anchor, ex, ey, ed);
    const float sx = 0.012f, sy = inst.originY;
    const float cx = 0.5f * (sx + ex), cy = std::min(sy, ey) - 0.10f;
    auto bez = [&](float u, float& x, float& y) {
        const float a = (1 - u) * (1 - u), b = 2 * (1 - u) * u, c = u * u;
        x = a * sx + b * cx + c * ex;
        y = a * sy + b * cy + c * ey;
    };
    const float tt = inst.tTransfer;
    const Rgb col = inst.colour;
    const Rgb hot{col.r * 0.55f + 0.45f, col.g * 0.55f + 0.42f, col.b * 0.55f + 0.48f};
    if (tt < kTransferDur + 0.05f) {
        const float env = smooth01(tt / 0.08f) * (1.0f - smooth01((tt - kTransferDur + 0.12f) / 0.17f));
        const int tail = 18, lanes = 7;
        for (int lane = 0; lane < lanes; ++lane) {
            const float ang = static_cast<float>(lane) / lanes * 2.0f * kPi + tt * 5.0f;
            float lx = 0, ly = 0;
            for (int j = 0; j <= tail; ++j) {
                const float u = easeInOut((tt - static_cast<float>(j) * 0.016f) / kTransferDur);
                float x, y;
                bez(u, x, y);
                // compressed bundle: a few parallel strands twisting around the path, tightening in flight
                const float rad = 0.004f + 0.012f * (1.0f - u) * (1.0f - u);
                x += std::cos(ang + static_cast<float>(j) * 0.35f) * rad;
                y += std::sin(ang + static_cast<float>(j) * 0.35f) * rad;
                if (j > 0) {
                    const float w = std::pow(1.0f - static_cast<float>(j) / tail, 2.0f) * env * (lane == 0 ? 0.34f : 0.13f);
                    segs_.push_back({lx, ly, x, y, hot.r * w, hot.g * w, hot.b * w});
                }
                lx = x; ly = y;
            }
        }
    }
    // dispersal burst at arrival
    const float u = (tt - kTransferDur * 0.92f) / 0.45f;
    if (u > 0.0f && u < 1.0f) {
        const int rays = 22;
        for (int k = 0; k < rays; ++k) {
            Rng r(mix64(inst.seed ^ static_cast<uint64_t>(k * 7919)));
            const float a = r.next() * 2.0f * kPi;
            const float len = 0.03f + 0.06f * r.next();
            const float r0 = len * smooth01(u) * 0.6f, r1 = len * smooth01(u);
            const float w = (1.0f - u) * (1.0f - u) * 0.10f * (0.5f + r.next());
            segs_.push_back({ex + std::cos(a) * r0, ey + std::sin(a) * r0 * 0.8f, ex + std::cos(a) * r1, ey + std::sin(a) * r1 * 0.8f, col.r * w, col.g * w, col.b * w});
        }
    }
}

void Formation::update(const FrameInput& in) {
    const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.001f, 0.1f) : 1.0f / 60.0f;
    time_ += dt;
    aspect_ = std::isfinite(in.aspect) ? std::clamp(in.aspect, 0.3f, 6.0f) : 1.6f;
    layout(in);
    first_ = false;

    auto follow = [dt](float& s, float x, float att, float rel) { s += (1.0f - std::exp(-dt / (x > s ? att : rel))) * (x - s); };
    float maxLevel = 0.0f;
    for (const auto& ii : in.chain) maxLevel = std::max(maxLevel, safeLevel(ii.level));
    follow(energy_, maxLevel, 0.25f, 1.2f);
    extent_ += (1.0f - std::exp(-dt / 1.4f)) * (extentTarget_ - extent_);

    // camera: very slow parallax (< 4 degrees), stable framing
    yaw_ = 0.38f + 0.06f * std::sin(time_ * 2.0f * kPi / 75.0f);
    pitch_ = 0.15f + 0.025f * std::sin(time_ * 2.0f * kPi / 92.0f + 1.3f);

    const int n = static_cast<int>(in.chain.size());
    const int budget = quality_ == Quality::Low ? 800 : (quality_ == Quality::High ? 3000 : 2000);
    perInstTarget_ = n > 0 ? std::clamp(budget / n, 28, kMaxStrandsPerInst) : 0;

    const float kGlide = 1.0f - std::exp(-dt / 1.1f);  // reorder: anchors glide, nothing resets
    for (auto& kv : insts_) {
        Inst& c = kv.second;
        c.anchor.x += kGlide * (c.target.x - c.anchor.x);
        c.anchor.y += kGlide * (c.target.y - c.anchor.y);
        c.anchor.z += kGlide * (c.target.z - c.anchor.z);
        c.tangent.x += kGlide * (c.tangentTarget.x - c.tangent.x);
        c.tangent.y += kGlide * (c.tangentTarget.y - c.tangent.y);
        c.tangent.z += kGlide * (c.tangentTarget.z - c.tangent.z);
        const float tl = std::sqrt(c.tangent.x * c.tangent.x + c.tangent.y * c.tangent.y + c.tangent.z * c.tangent.z) + 1e-6f;
        c.tangent = {c.tangent.x / tl, c.tangent.y / tl, c.tangent.z / tl};
        c.plume += kGlide * (c.plumeTarget - c.plume);
        follow(c.presence, c.inChain ? 1.0f : 0.0f, 0.8f, 0.5f);
        if (c.hold > 0.0f) c.hold -= dt;
        else if (c.inChain) c.grow = std::min(1.0f, c.grow + dt / 1.4f);
        if (c.transfer) {
            c.tTransfer += dt;
            if (c.tTransfer > kTransferDur + 0.5f) c.transfer = false;
        }
        float lv = 0.0f;
        for (const auto& ii : in.chain) if (ii.id == kv.first) lv = safeLevel(ii.level);
        follow(c.bright, lv, 0.12f, 0.9f);
    }
    for (auto it = insts_.begin(); it != insts_.end();) {
        if (!it->second.inChain && it->second.presence < 0.01f) it = insts_.erase(it);
        else ++it;
    }

    // emit
    segs_.clear();
    strands_ = 0;
    for (auto& kv : insts_) {
        Inst& c = kv.second;
        const float lengthFrac = smooth01(c.grow);
        // the vertical plume carries the mass; the receding arm stays finer
        const int count = c.inChain ? static_cast<int>(static_cast<float>(perInstTarget_) * (0.6f + 0.5f * c.plume)) : static_cast<int>(c.strands.size());
        const int lim = std::min(count, static_cast<int>(c.strands.size()));
        for (int k = 0; k < lim; ++k) {
            const Strand& st = c.strands[static_cast<size_t>(k)];
            emitStrand(c, st, lengthFrac, c.bright);
            ++strands_;
        }
        if (c.transfer) emitTransfer(c);
    }
}

}  // namespace ih1::visual
