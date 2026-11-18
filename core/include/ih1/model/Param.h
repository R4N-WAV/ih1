// IH1 — parameter specification and plain-value helpers.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ih1 {

constexpr int kMaxParams   = 32;  // per FX type
constexpr int kMaxChain    = 32;  // committed instances
constexpr int kNumChannels = 2;   // engine is always stereo internally

using ParamValues = std::array<float, kMaxParams>;

enum class ParamKind : uint8_t { Float, Choice, Toggle };
enum class ParamCurve : uint8_t { Linear, Log, Pow };

struct ParamSpec {
    std::string id;       // stable, never renamed (state is keyed by it)
    std::string name;     // display name
    std::string unit;     // "ms", "Hz", "dB", "%", "st", "ct", ""
    ParamKind kind = ParamKind::Float;
    float minValue = 0.0f, maxValue = 1.0f, defaultValue = 0.0f;
    ParamCurve curve = ParamCurve::Linear;
    float curveExponent = 1.0f;          // ParamCurve::Pow only
    std::vector<std::string> choices;    // ParamKind::Choice only
    std::string tooltip;                 // plain-language: what it does to the sound
    bool advanced = false;               // shown under "More" in the settings panel

    float clamp(float v) const {
        if (!std::isfinite(v)) v = defaultValue;
        v = std::clamp(v, minValue, maxValue);
        if (kind != ParamKind::Float) v = std::round(v);
        return v;
    }

    float toNormalised(float v) const {
        v = clamp(v);
        if (maxValue <= minValue) return 0.0f;
        switch (curve) {
            case ParamCurve::Log: {
                const float lo = std::log(std::max(minValue, 1e-6f));
                const float hi = std::log(std::max(maxValue, 1e-6f));
                return (std::log(std::max(v, 1e-6f)) - lo) / (hi - lo);
            }
            case ParamCurve::Pow:
                return std::pow((v - minValue) / (maxValue - minValue), 1.0f / curveExponent);
            default:
                return (v - minValue) / (maxValue - minValue);
        }
    }

    float fromNormalised(float n) const {
        n = std::clamp(std::isfinite(n) ? n : 0.0f, 0.0f, 1.0f);
        float v;
        switch (curve) {
            case ParamCurve::Log: {
                const float lo = std::log(std::max(minValue, 1e-6f));
                const float hi = std::log(std::max(maxValue, 1e-6f));
                v = std::exp(lo + n * (hi - lo));
                break;
            }
            case ParamCurve::Pow:
                v = minValue + std::pow(n, curveExponent) * (maxValue - minValue);
                break;
            default:
                v = minValue + n * (maxValue - minValue);
        }
        return clamp(v);
    }

    int numSteps() const {
        if (kind == ParamKind::Toggle) return 2;
        if (kind == ParamKind::Choice) return static_cast<int>(choices.size());
        return 0;
    }

    std::string format(float v) const;  // human-readable value with unit
};

// Builders keep FX descriptor tables compact and readable.
namespace P {
ParamSpec flt(std::string id, std::string name, std::string unit, float lo, float hi, float def,
              std::string tooltip, ParamCurve curve = ParamCurve::Linear, float exponent = 1.0f);
ParamSpec logf(std::string id, std::string name, std::string unit, float lo, float hi, float def,
               std::string tooltip);
ParamSpec pct(std::string id, std::string name, float def, std::string tooltip);  // 0..100 %
ParamSpec choice(std::string id, std::string name, std::vector<std::string> choices, int def,
                 std::string tooltip);
ParamSpec toggle(std::string id, std::string name, bool def, std::string tooltip);
ParamSpec& adv(ParamSpec& p);
inline ParamSpec adv(ParamSpec&& p) { p.advanced = true; return std::move(p); }
}  // namespace P

}  // namespace ih1
