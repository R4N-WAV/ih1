#include "ih1/visual/Formation.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace ih1::visual {

// ------------------------------------------------------------------------------------- palette
float hueForType(const std::string& id) {
    // Families: time/space ~222-238, reverb ~246, pitch/voice ~258-272, modulation ~280-290,
    // texture ~298-306, tone/dynamics ~314-322 (docs/VISUAL_REFERENCE_ANALYSIS.md section 3).
    static const std::pair<const char*, float> table[] = {
        {"delay", 222}, {"tape_echo", 230}, {"reverse", 237}, {"reverb", 246}, {"shimmer", 250},
        {"harmony", 262}, {"pitch_shift", 268}, {"formant", 272}, {"doubler", 258},
        {"chorus", 280}, {"filter", 287}, {"flanger", 284}, {"phaser", 290},
        {"granular", 298}, {"spectral", 304}, {"stutter", 308},
        {"eq", 314}, {"saturation", 320}, {"width", 316}};
    for (const auto& kv : table) if (id == kv.first) return kv.second;
    // unknown types: hash into the arc
    uint32_t h = 2166136261u;
    for (char c : id) h = (h ^ static_cast<uint8_t>(c)) * 16777619u;
    return 218.0f + static_cast<float>(h % 104u);
}

Rgb hsvToRgb(float h, float s, float v) {
    h = std::fmod(std::fmod(h, 360.0f) + 360.0f, 360.0f) / 60.0f;
    const int i = static_cast<int>(h);
    const float f = h - static_cast<float>(i);
    const float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch (i) {
        case 0: return {v, t, p};
        case 1: return {q, v, p};
        case 2: return {p, v, t};
        case 3: return {p, q, v};
        case 4: return {t, p, v};
        default: return {v, p, q};
    }
}

Rgb toneMap(Rgb a, float exposure) {
    // per-channel soft saturation, then blend toward the luminance (white) as density rises
    auto sat = [&](float x) { return 1.0f - std::exp(-x * exposure); };
    Rgb c{sat(a.r), sat(a.g), sat(a.b)};
    const float l = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
    const float w = std::clamp((l - 0.45f) * 1.6f, 0.0f, 1.0f);  // highlights desaturate to pale lavender-white
    const Rgb white{0.93f, 0.86f, 0.98f};
    c.r += w * (white.r * std::max(l, 0.0f) * 1.15f - c.r) * 0.6f;
    c.g += w * (white.g * std::max(l, 0.0f) * 1.15f - c.g) * 0.6f;
    c.b += w * (white.b * std::max(l, 0.0f) * 1.15f - c.b) * 0.6f;
    // background floor ~ #0a080d
    c.r = std::min(1.0f, c.r + 0.039f);
    c.g = std::min(1.0f, c.g + 0.031f);
    c.b = std::min(1.0f, c.b + 0.051f);
    return c;
}

// ------------------------------------------------------------------------------------- noise
namespace {
const uint8_t kPerm[256] = {
    151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148,
    247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175,
    74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54,
    65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64,
    52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213,
    119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104,
    218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157,
    184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180};
inline int P(int i) { return kPerm[i & 255]; }
inline float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
inline float grad(int h, float x, float y, float z) {
    const int k = h & 15;
    const float u = k < 8 ? x : y, v = k < 4 ? y : (k == 12 || k == 14 ? x : z);
    return ((k & 1) ? -u : u) + ((k & 2) ? -v : v);
}
inline float lerp(float t, float a, float b) { return a + t * (b - a); }
}  // namespace

float noise3(float x, float y, float z) {
    const int X = static_cast<int>(std::floor(x)), Y = static_cast<int>(std::floor(y)), Z = static_cast<int>(std::floor(z));
    x -= std::floor(x); y -= std::floor(y); z -= std::floor(z);
    const float u = fade(x), v = fade(y), w = fade(z);
    const int A = P(X) + Y, AA = P(A) + Z, AB = P(A + 1) + Z, B = P(X + 1) + Y, BA = P(B) + Z, BB = P(B + 1) + Z;
    return lerp(w, lerp(v, lerp(u, grad(P(AA), x, y, z), grad(P(BA), x - 1, y, z)), lerp(u, grad(P(AB), x, y - 1, z), grad(P(BB), x - 1, y - 1, z))),
                lerp(v, lerp(u, grad(P(AA + 1), x, y, z - 1), grad(P(BA + 1), x - 1, y, z - 1)), lerp(u, grad(P(AB + 1), x, y - 1, z - 1), grad(P(BB + 1), x - 1, y - 1, z - 1))));
}

namespace {
// curl of a 2-D stream function built from two octaves of noise (divergence-free flow)
inline void curl(float x, float y, float t, float& vx, float& vy) {
    const float e = 0.01f;
    auto psi = [t](float a, float b) { return noise3(a * 1.7f, b * 1.7f, t) + 0.45f * noise3(a * 4.1f + 7.3f, b * 4.1f - 3.1f, t * 1.6f); };
    vx = (psi(x, y + e) - psi(x, y - e)) / (2 * e);
    vy = -(psi(x + e, y) - psi(x - e, y)) / (2 * e);
}
}  // namespace

// ------------------------------------------------------------------------------------- formation
Formation::Formation(int maxParticles) : parts_(static_cast<size_t>(std::max(1000, maxParticles))) {
    segs_.reserve(parts_.size());
}

uint32_t Formation::rnd() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}
float Formation::gauss() {
    const float u = std::max(1e-6f, frand()), v = frand();
    return std::sqrt(-2.0f * std::log(u)) * std::cos(6.2831853f * v);
}

float Formation::decay(float dt) const { return std::pow(0.935f, dt * 60.0f); }

void Formation::layout(const FrameInput& in) {
    const int n = static_cast<int>(in.chain.size());
    // the luminous mass sits right of centre (reference energy centroid ~0.82 W in portrait; landscape ~0.6)
    const float cx = aspect_ * 0.60f, cy = 0.50f;
    const float R = std::min(0.42f, 0.05f + 0.085f * std::sqrt(static_cast<float>(n)));
    for (auto& kv : cores_) kv.second.inChain = false;
    for (int i = 0; i < n; ++i) {
        const auto& ii = in.chain[static_cast<size_t>(i)];
        auto it = cores_.find(ii.id);
        const bool fresh = it == cores_.end();
        Core& c = cores_[ii.id];
        c.inChain = true;
        c.index = i;
        c.hue = ii.hue;
        c.seed = ii.seed;
        c.colour = hsvToRgb(ii.hue, 0.48f, 1.0f);
        // positions along a slanted, slowly winding spine; first in chain toward the outer left/lower end,
        // last toward the dense centre (signal flows into the mass)
        const float u = n <= 1 ? 1.0f : static_cast<float>(i) / static_cast<float>(n - 1);
        const float ang = 3.6f - 2.7f * u + 0.35f * std::sin(static_cast<float>(ii.seed % 1000) * 0.0123f);
        const float rad = R * (0.25f + 0.75f * (1.0f - u * 0.8f));
        const float jx = (static_cast<float>((ii.seed >> 4) % 1000) / 1000.0f - 0.5f) * 0.05f;
        const float jy = (static_cast<float>((ii.seed >> 14) % 1000) / 1000.0f - 0.5f) * 0.05f;
        c.tx = cx + std::cos(ang) * rad * 1.35f + jx;
        c.ty = cy + std::sin(ang) * rad * 0.8f + jy;
        if (fresh) { c.x = cx + (c.tx - cx) * 0.3f; c.y = cy + (c.ty - cy) * 0.3f; c.presence = 0.0f; }
    }
}

void Formation::spawn(Particle& p, const Core& c, uint64_t id, uint8_t kind, float spread) {
    p.alive = true;
    p.owner = id;
    p.kind = kind;
    p.age = 0.0f;
    if (kind == 1) {  // tendril: spawned on the core's outskirts, long-lived
        p.x = c.x + gauss() * spread * 1.6f;
        p.y = c.y + gauss() * spread * 0.7f;
        p.life = 5.0f + 6.0f * frand();
        p.gain = 0.55f + 0.45f * frand();
    } else if (kind == 2) {
        const float a = frand() * 6.2831853f;
        p.x = c.x + std::cos(a) * spread;
        p.y = c.y + std::sin(a) * spread;
        p.life = 1.5f + frand();
        p.gain = 0.5f + frand();
    } else {
        p.x = c.x + gauss() * spread;
        p.y = c.y + gauss() * spread * 0.85f;
        p.life = 2.0f + 3.5f * frand();
        p.gain = 0.6f + 0.8f * frand() * frand();
    }
    p.px = p.x;
    p.py = p.y;
}

void Formation::update(const FrameInput& in) {
    const float dt = std::clamp(in.dt, 0.001f, 0.1f);
    time_ += dt;
    aspect_ = std::max(0.5f, in.aspect);
    // smoothed audio features (attack/release, never flicker)
    auto follow = [dt](float& s, float x, float att, float rel) { s += (1.0f - std::exp(-dt / (x > s ? att : rel))) * (x - s); };
    const float lvl = std::isfinite(in.outLevel) ? std::clamp(std::sqrt(std::max(0.0f, in.outLevel)) * 1.6f, 0.0f, 1.2f) : 0.0f;
    follow(energy_, lvl, 0.08f, 0.6f);
    auto safe = [](float x) { return std::isfinite(x) ? std::max(0.0f, x) : 0.0f; };
    follow(low_, std::clamp(std::sqrt(safe(in.low)) * 1.8f, 0.0f, 1.2f), 0.1f, 0.8f);
    follow(high_, std::clamp(std::sqrt(safe(in.high)) * 3.0f, 0.0f, 1.2f), 0.05f, 0.5f);
    follow(solo_, std::isfinite(in.solo) ? std::clamp(in.solo, 0.0f, 1.0f) : 0.0f, 0.15f, 0.15f);
    layout(in);
    chainSize_ = static_cast<int>(in.chain.size());

    // cores glide (reorder never resets the field), fade in/out, follow their own audio level
    for (auto& kv : cores_) {
        Core& c = kv.second;
        const float k = 1.0f - std::exp(-dt / 0.9f);
        c.x += k * (c.tx - c.x);
        c.y += k * (c.ty - c.y);
        follow(c.presence, c.inChain ? 1.0f : 0.0f, 1.2f, 1.0f);
        float lv = 0.0f;
        for (const auto& ii : in.chain) if (ii.id == kv.first && std::isfinite(ii.level)) lv = std::clamp(std::sqrt(std::max(0.0f, ii.level)) * 1.6f, 0.0f, 1.2f);
        follow(c.bright, lv, 0.08f, 0.7f);
    }
    for (auto it = cores_.begin(); it != cores_.end();) {
        if (!it->second.inChain && it->second.presence < 0.01f) {
            const uint64_t id = it->first;
            for (auto& p : parts_) if (p.alive && p.owner == id) p.alive = false;
            it = cores_.erase(it);
        } else ++it;
    }
    // staged preview: neutral, restrained, away from the committed mass (lower-left of the field)
    follow(stagedPresence_, in.hasStaged ? 1.0f : 0.0f, 0.4f, 0.4f);
    follow(stagedBright_, std::clamp(std::sqrt(std::max(0.0f, in.stagedLevel)) * 1.6f, 0.0f, 1.0f), 0.08f, 0.5f);
    staged_.x = aspect_ * 0.16f;
    staged_.y = 0.80f;
    staged_.colour = {0.80f, 0.80f, 0.84f};

    // spawn budget: formation size and complexity grow with the chain
    const int n = chainSize_;
    const int target = n == 0 ? 0 : std::min(static_cast<int>(parts_.size()) - 600, 2200 + 1900 * n);
    const float spread = 0.035f + 0.02f * std::pow(static_cast<float>(std::max(1, n)), 0.45f);
    int alive = 0;
    for (const auto& p : parts_) alive += p.alive ? 1 : 0;
    int freeSlot = 0;
    auto nextFree = [&]() -> Particle* {
        while (freeSlot < static_cast<int>(parts_.size()) && parts_[static_cast<size_t>(freeSlot)].alive) ++freeSlot;
        return freeSlot < static_cast<int>(parts_.size()) ? &parts_[static_cast<size_t>(freeSlot)] : nullptr;
    };
    if (n > 0 && alive < target) {
        const float perCore = static_cast<float>(target - alive) * std::min(1.0f, dt * 2.5f) / static_cast<float>(n);
        for (auto& kv : cores_) {
            Core& c = kv.second;
            if (!c.inChain) continue;
            c.spawnAcc += perCore * (0.6f + 0.4f * c.presence);
            while (c.spawnAcc >= 1.0f) {
                c.spawnAcc -= 1.0f;
                Particle* p = nextFree();
                if (!p) break;
                const float r = frand();
                if (r < 0.18f) {  // luminous centre: strands born near the formation centroid (dense overlap)
                    Core centre = c;
                    centre.x = aspect_ * 0.60f + (c.x - aspect_ * 0.60f) * 0.25f;
                    centre.y = 0.50f + (c.y - 0.50f) * 0.25f;
                    spawn(*p, centre, kv.first, 0, spread * 0.7f);
                    p->gain *= 1.6f;
                } else {
                    spawn(*p, c, kv.first, r < 0.40f ? 1 : 0, spread);
                }
            }
        }
    }
    if (stagedPresence_ > 0.02f) {
        const int want = static_cast<int>(160 * stagedPresence_);
        int have = 0;
        for (const auto& p : parts_) have += (p.alive && p.kind == 2) ? 1 : 0;
        for (int i = have; i < want; ++i) { Particle* p = nextFree(); if (!p) break; spawn(*p, staged_, 0, 2, 0.045f); }
    }

    // advect
    segs_.clear();
    live_ = 0;
    const float flowT = time_ * (0.035f + 0.03f * high_);
    const float speed = 0.055f * (0.7f + 0.5f * energy_ + 0.3f * low_);
    const float desat = solo_;  // SOLO: committed formation toward monochrome and darker
    const float soloDim = 1.0f - 0.88f * solo_;
    for (auto& p : parts_) {
        if (!p.alive) continue;
        p.age += dt;
        if (p.age >= p.life) { p.alive = false; continue; }
        const Core* c = nullptr;
        if (p.kind == 2) c = &staged_;
        else {
            auto it = cores_.find(p.owner);
            if (it == cores_.end()) { p.alive = false; continue; }
            c = &it->second;
        }
        p.px = p.x;
        p.py = p.y;
        // flow = slowly turning direction field (long fibrous strands) + small curl perturbation
        const float ang = 0.35f + 2.6f * noise3(p.x * 0.55f, p.y * 0.55f, flowT * 0.5f + 11.0f);
        float dirx = std::cos(ang), diry = std::sin(ang) * 0.75f;
        float vx, vy;
        curl(p.x * 1.1f, p.y * 1.1f, flowT + (p.kind == 1 ? 3.7f : 0.0f), vx, vy);
        const float dx = c->x - p.x, dy = c->y - p.y;
        const float d = std::sqrt(dx * dx + dy * dy) + 1e-4f;
        float mx, my;
        if (p.kind == 2) {  // staged preview: a quiet orbiting halo (clearly not part of the committed mass)
            const float r0 = 0.022f + 0.04f * (p.gain - 0.5f);  // each strand orbits at its own radius: a soft haze
            const float radial = (d - r0) * 3.0f;
            mx = -dy / d * 0.05f + dx / d * radial + 0.1f * vx * speed;
            my = dx / d * 0.05f + dy / d * radial + 0.1f * vy * speed;
        } else if (p.kind == 1) {  // fibres: long, faint, nearly straight streaks across the field
            mx = dirx * speed * 2.2f + vx * speed * 0.25f - 0.012f;
            my = diry * speed * 1.2f + vy * speed * 0.25f;
        } else {  // body strands: emanate from the core along the field, folded by the curl
            const float reach = 0.10f + 0.035f * std::sqrt(static_cast<float>(std::max(1, chainSize_)));
            const float back = d > reach ? (d - reach) * 1.4f : 0.0f;  // contain the body softly
            const float sgn = (p.gain > 1.0f) ? -1.0f : 1.0f;            // strands leave in both directions
            mx = sgn * dirx * speed * 1.1f + vx * speed * 0.55f + dx * back;
            my = sgn * diry * speed * 1.1f + vy * speed * 0.55f + dy * back;
            // weak pull of every strand toward the formation centre merges the cores into one mass
            mx += (aspect_ * 0.6f - p.x) * 0.03f;
            my += (0.5f - p.y) * 0.03f;
        }
        p.x += mx * dt * 2.0f;
        p.y += my * dt * 2.0f;
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.x < -0.2f || p.x > aspect_ + 0.2f || p.y < -0.2f || p.y > 1.2f) { p.alive = false; continue; }
        ++live_;
        // brightness: fade in/out over life, audio-reactive per instance, kind gain
        const float t = p.age / p.life;
        const float env = std::min(1.0f, t * 6.0f) * std::min(1.0f, (1.0f - t) * 3.0f);
        Rgb col;
        float b;
        if (p.kind == 2) {
            col = staged_.colour;
            b = env * stagedPresence_ * (0.0035f + 0.012f * stagedBright_);
        } else {
            col = c->colour;
            // chromatic accent: a few strands lean icy-cyan or warm-magenta (edge iridescence)
            const float l = 0.2126f * col.r + 0.7152f * col.g + 0.0722f * col.b;
            col.r = col.r + desat * (l - col.r);
            col.g = col.g + desat * (l - col.g);
            col.b = col.b + desat * (l - col.b);
            b = env * c->presence * p.gain * (p.kind == 1 ? 0.012f : 0.020f) * (0.65f + 0.9f * c->bright + 0.25f * energy_) * soloDim;
        }
        segs_.push_back({p.px, p.py, p.x, p.y, col.r * b, col.g * b, col.b * b});
    }
    // core info for the UI
    coreInfo_.clear();
    for (const auto& kv : cores_) {
        if (!kv.second.inChain) continue;
        coreInfo_.push_back({kv.first, kv.second.x, kv.second.y, kv.second.colour, kv.second.bright});
    }
}

}  // namespace ih1::visual
