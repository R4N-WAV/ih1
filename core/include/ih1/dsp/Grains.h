// Grain engine (KADAL KNW-01J68SK1RC): rolling stereo buffer, fixed grain pool (no allocation per
// grain), Hann/Tukey/percussive envelope table, seeded scheduler, gain normalised by expected overlap.
#pragma once

#include <array>
#include <vector>

#include "ih1/dsp/Basics.h"

namespace ih1::dsp {

class GrainCloud {
public:
    struct Params {
        float sizeSamples = 4800;
        float density = 10;          // grains per second
        float positionSamples = 0;   // how far behind the write head grains start
        float spraySamples = 0;      // random extra distance
        float pitchRatio = 1;
        float pitchScatterSt = 0;    // random +- semitones
        float reverseProb = 0;       // 0..1
        float spread = 0.5f;         // stereo randomisation 0..1
        int envelope = 0;            // 0 Hann, 1 Tukey (flat top), 2 Percussive
        float timingJitter = 0.3f;   // 0 = metronomic, 1 = fully random intervals
    };

    void prepare(double sr, float bufferSeconds, uint32_t seed) {
        sr_ = sr;
        int size = 1;
        while (size < static_cast<int>(bufferSeconds * sr)) size <<= 1;
        for (auto& b : buf_) b.assign(static_cast<size_t>(size), 0.0f);
        mask_ = size - 1;
        seed_ = seed;
        for (int e = 0; e < 3; ++e)
            for (int i = 0; i <= kEnvSize; ++i) {
                const float t = static_cast<float>(i) / kEnvSize;
                float v;
                if (e == 0) v = 0.5f - 0.5f * std::cos(kTwoPi * t);
                else if (e == 1) v = t < 0.2f ? 0.5f - 0.5f * std::cos(kPi * t / 0.2f) : (t > 0.8f ? 0.5f - 0.5f * std::cos(kPi * (1.0f - t) / 0.2f) : 1.0f);
                else v = std::min(1.0f, t * 40.0f) * std::exp(-5.0f * t) * (1.0f - std::pow(t, 8.0f));
                env_[static_cast<size_t>(e)][static_cast<size_t>(i)] = v;
            }
        reset();
    }
    void reset() {
        for (auto& b : buf_) std::fill(b.begin(), b.end(), 0.0f);
        for (auto& g : grains_) g.active = false;
        w_ = 0;
        countdown_ = 0.0f;
        rng_.seed_(seed_);
        filled_ = 0;
    }
    void setFrozen(bool f) { frozen_ = f; }
    bool frozen() const { return frozen_; }
    int capacity() const { return mask_ + 1; }

    void write(float l, float r) {
        if (frozen_) return;
        w_ = (w_ + 1) & mask_;
        buf_[0][static_cast<size_t>(w_)] = l;
        buf_[1][static_cast<size_t>(w_)] = r;
        if (filled_ < mask_) ++filled_;
    }

    void tick(const Params& p, float& outL, float& outR) {
        countdown_ -= 1.0f;
        if (countdown_ <= 0.0f) {
            spawn(p);
            const float interval = static_cast<float>(sr_) / std::max(0.1f, p.density);
            countdown_ += interval * (1.0f + clamp01(p.timingJitter) * rng_.bipolar() * 0.9f);
            if (countdown_ < 1.0f) countdown_ = 1.0f;
        }
        float l = 0.0f, r = 0.0f;
        for (auto& g : grains_) {
            if (!g.active) continue;
            const float ph = g.t / g.len;
            const float fi = ph * kEnvSize;
            const int ii = static_cast<int>(fi);
            const auto& e = env_[static_cast<size_t>(g.env)];
            const float a = e[static_cast<size_t>(ii)] + (fi - ii) * (e[static_cast<size_t>(ii + 1)] - e[static_cast<size_t>(ii)]);
            const double pos = g.pos;
            const int i0 = static_cast<int>(std::floor(pos));
            const float f = static_cast<float>(pos - i0);
            const float s0 = read(0, i0, f), s1 = read(1, i0, f);
            l += a * g.gl * s0;
            r += a * g.gr * s1;
            g.pos += g.inc;
            g.t += 1.0f;
            if (g.t >= g.len) g.active = false;
        }
        const float overlap = std::max(1.0f, p.density * p.sizeSamples / static_cast<float>(sr_));
        const float norm = 1.0f / std::sqrt(overlap) * 1.3f;
        outL = l * norm;
        outR = r * norm;
    }

private:
    static constexpr int kEnvSize = 512;
    static constexpr int kMaxGrains = 96;
    struct Grain { bool active = false; double pos = 0; double inc = 1; float t = 0, len = 1, gl = 1, gr = 1; int env = 0; };

    float read(int c, int i0, float f) const {
        const auto& b = buf_[static_cast<size_t>(c)];
        const float a = b[static_cast<size_t>(i0 & mask_)], bb = b[static_cast<size_t>((i0 + 1) & mask_)];
        return a + f * (bb - a);
    }

    void spawn(const Params& p) {
        Grain* g = nullptr;
        for (auto& x : grains_) if (!x.active) { g = &x; break; }
        if (!g) return;  // bounded: skip when the pool is full
        const float len = std::max(16.0f, p.sizeSamples);
        float ratio = p.pitchRatio * std::exp2(p.pitchScatterSt * rng_.bipolar() / 12.0f);
        ratio = std::clamp(ratio, 0.125f, 4.0f);
        const bool rev = rng_.uniform() < p.reverseProb;
        // forward grains must stay behind the write head; reverse grains walk into the past
        const float minBack = rev ? 4.0f : len * std::max(1.0f, ratio) + 4.0f;
        float back = p.positionSamples + p.spraySamples * rng_.uniform();
        back = std::max(back, minBack);
        if (rev) back = std::max(back, 4.0f);
        const float maxBack = static_cast<float>(std::min(filled_, mask_)) - len * ratio - 8.0f;
        if (maxBack < minBack) return;  // not enough recorded audio yet
        back = std::min(back, maxBack);
        g->active = true;
        g->pos = static_cast<double>(w_) - back + (rev ? 0.0 : 0.0);
        g->inc = rev ? -ratio : ratio;
        g->t = 0.0f;
        g->len = len;
        g->env = std::clamp(p.envelope, 0, 2);
        const float pan = clamp01(p.spread) * rng_.bipolar();
        panGains(pan, g->gl, g->gr);
        g->gl *= 1.41421356f;
        g->gr *= 1.41421356f;
    }

    double sr_ = 48000.0;
    std::array<std::vector<float>, 2> buf_;
    int mask_ = 0, w_ = 0, filled_ = 0;
    std::array<Grain, kMaxGrains> grains_{};
    std::array<std::array<float, kEnvSize + 1>, 3> env_{};
    float countdown_ = 0.0f;
    bool frozen_ = false;
    uint32_t seed_ = 1;
    Rng rng_;
};

}  // namespace ih1::dsp
