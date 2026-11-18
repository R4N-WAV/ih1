// IH1 shared DSP basics: constants, smoothers, one-poles, RNG, envelope follower, LFO, tempo.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ih1::dsp {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float dbToGain(float db) { return std::pow(10.0f, db * 0.05f); }
inline float gainToDb(float g) { return 20.0f * std::log10(std::max(g, 1e-9f)); }
inline float semitonesToRatio(float st) { return std::exp2(st / 12.0f); }
inline float msToSamples(float ms, double sr) { return ms * 0.001f * static_cast<float>(sr); }
inline float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }
// equal-power crossfade gains (dry, wet) for mix in 0..1
inline void equalPower(float mix, float& dry, float& wet) {
    mix = clamp01(mix);
    dry = std::cos(mix * kPi * 0.5f);
    wet = std::sin(mix * kPi * 0.5f);
}
// constant-power pan: p in -1..1 -> (l, r)
inline void panGains(float p, float& l, float& r) {
    const float a = (std::clamp(p, -1.0f, 1.0f) + 1.0f) * kPi * 0.25f;
    l = std::cos(a);
    r = std::sin(a);
}

// One-pole exponential smoother (time constant in ms). Good for gains and continuous controls.
class Smoother {
public:
    void prepare(double sr, float timeMs) {
        sr_ = sr;
        setTime(timeMs);
    }
    void setTime(float timeMs) {
        coef_ = timeMs <= 0.0f ? 1.0f : 1.0f - std::exp(-1.0f / (timeMs * 0.001f * static_cast<float>(sr_)));
    }
    void setTarget(float t) { target_ = t; }
    void snap(float v) { target_ = cur_ = v; }
    float next() {
        cur_ += coef_ * (target_ - cur_);
        if (std::fabs(target_ - cur_) < 1e-7f) cur_ = target_;
        return cur_;
    }
    float current() const { return cur_; }
    float target() const { return target_; }
    bool settled() const { return cur_ == target_; }

private:
    double sr_ = 48000.0;
    float coef_ = 1.0f, cur_ = 0.0f, target_ = 0.0f;
};

// Simple one-pole low-pass / high-pass (matched-z), per channel state.
class OnePole {
public:
    void setLowpass(float fc, double sr) { a_ = 1.0f - std::exp(-kTwoPi * std::clamp(fc, 1.0f, 0.49f * static_cast<float>(sr)) / static_cast<float>(sr)); }
    float lp(float x) { z_ += a_ * (x - z_); return z_; }
    float hp(float x) { return x - lp(x); }
    void reset(float v = 0.0f) { z_ = v; }
    void flush() { if (std::fabs(z_) < 1e-20f) z_ = 0.0f; }
    float coef() const { return a_; }

private:
    float a_ = 1.0f, z_ = 0.0f;
};

// DC blocker (first-order high-pass at ~10 Hz).
class DcBlocker {
public:
    void prepare(double sr, float fc = 10.0f) { r_ = 1.0f - kTwoPi * fc / static_cast<float>(sr); }
    float process(float x) {
        const float y = x - x1_ + r_ * y1_;
        x1_ = x;
        y1_ = (std::fabs(y) < 1e-20f) ? 0.0f : y;
        return y;
    }
    void reset() { x1_ = y1_ = 0.0f; }

private:
    float r_ = 0.999f, x1_ = 0.0f, y1_ = 0.0f;
};

// Deterministic, seedable random generator (xoshiro128+); RT-safe.
class Rng {
public:
    explicit Rng(uint32_t seed = 1) { seed_(seed); }
    void seed_(uint32_t seed) {
        uint64_t z = seed + 0x9E3779B97F4A7C15ull;
        for (auto& s : s_) {
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            s = static_cast<uint32_t>(z >> 32) | 1u;
        }
    }
    uint32_t nextU32() {
        const uint32_t r = s_[0] + s_[3];
        const uint32_t t = s_[1] << 9;
        s_[2] ^= s_[0]; s_[3] ^= s_[1]; s_[1] ^= s_[2]; s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = (s_[3] << 11) | (s_[3] >> 21);
        return r;
    }
    float uniform() { return static_cast<float>(nextU32() >> 8) * (1.0f / 16777216.0f); }  // [0,1)
    float bipolar() { return uniform() * 2.0f - 1.0f; }                                       // [-1,1)

private:
    uint32_t s_[4]{};
};

// Peak envelope follower with attack/release times (linear domain).
class EnvelopeFollower {
public:
    void prepare(double sr, float attackMs, float releaseMs) {
        sr_ = sr;
        set(attackMs, releaseMs);
    }
    void set(float attackMs, float releaseMs) {
        att_ = 1.0f - std::exp(-1.0f / (std::max(attackMs, 0.01f) * 0.001f * static_cast<float>(sr_)));
        rel_ = 1.0f - std::exp(-1.0f / (std::max(releaseMs, 0.01f) * 0.001f * static_cast<float>(sr_)));
    }
    float process(float x) {
        const float a = std::fabs(x);
        env_ += (a > env_ ? att_ : rel_) * (a - env_);
        if (env_ < 1e-20f) env_ = 0.0f;
        return env_;
    }
    float value() const { return env_; }
    void reset() { env_ = 0.0f; }

private:
    double sr_ = 48000.0;
    float att_ = 1.0f, rel_ = 0.001f, env_ = 0.0f;
};

// Musical note divisions for tempo sync.
struct NoteDivision {
    const char* label;
    double beats;  // in quarter notes
};
constexpr NoteDivision kDivisions[] = {
    {"1/64", 0.0625}, {"1/32T", 0.0833333333}, {"1/32", 0.125}, {"1/16T", 0.1666666667}, {"1/16", 0.25},
    {"1/16D", 0.375}, {"1/8T", 0.3333333333}, {"1/8", 0.5}, {"1/8D", 0.75}, {"1/4T", 0.6666666667},
    {"1/4", 1.0}, {"1/4D", 1.5}, {"1/2T", 1.3333333333}, {"1/2", 2.0}, {"1/2D", 3.0}, {"1/1", 4.0},
    {"2/1", 8.0}, {"4/1", 16.0}};
constexpr int kNumDivisions = static_cast<int>(sizeof(kDivisions) / sizeof(kDivisions[0]));
inline double divisionSeconds(int idx, double bpm) {
    idx = std::clamp(idx, 0, kNumDivisions - 1);
    return kDivisions[idx].beats * 60.0 / std::clamp(bpm, 20.0, 999.0);
}

// Low-frequency oscillator with continuous phase. Shapes: 0 sine, 1 triangle, 2 saw up, 3 square,
// 4 smoothed random (sample & hold through a one-pole), 5 drift (filtered noise).
class Lfo {
public:
    enum Shape { Sine = 0, Triangle, Saw, Square, Random, Drift };
    void prepare(double sr, uint32_t seed = 1) { sr_ = sr; rng_.seed_(seed); smooth_.setLowpass(20.0f, sr); }
    void setRate(float hz) { inc_ = std::clamp(hz, 0.0f, 200.0f) / static_cast<float>(sr_); }
    void setShape(int s) { shape_ = std::clamp(s, 0, 5); }
    void setPhase(float p) { phase_ = p - std::floor(p); }
    float phase() const { return phase_; }
    // returns -1..1
    float next() {
        float y;
        switch (shape_) {
            case Triangle: y = 1.0f - 4.0f * std::fabs(phase_ - 0.5f); break;
            case Saw: y = 2.0f * phase_ - 1.0f; break;
            case Square: y = phase_ < 0.5f ? 1.0f : -1.0f; y = smooth_.lp(y); break;
            case Random: y = smooth_.lp(held_); break;
            case Drift: driftState_ += 0.002f * (rng_.bipolar() - driftState_ * 0.02f); y = std::clamp(driftState_ * 0.25f, -1.0f, 1.0f); break;
            default: y = std::sin(kTwoPi * phase_);
        }
        phase_ += inc_;
        if (phase_ >= 1.0f) {
            phase_ -= std::floor(phase_);
            held_ = rng_.bipolar();
        }
        return y;
    }

private:
    double sr_ = 48000.0;
    float phase_ = 0.0f, inc_ = 0.0f, held_ = 0.0f, driftState_ = 0.0f;
    int shape_ = Sine;
    Rng rng_;
    OnePole smooth_;
};

}  // namespace ih1::dsp
