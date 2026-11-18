#include "ih1/model/Param.h"

#include <cstdio>

namespace ih1 {

std::string ParamSpec::format(float v) const {
    v = clamp(v);
    if (kind == ParamKind::Toggle) return v >= 0.5f ? "On" : "Off";
    if (kind == ParamKind::Choice) {
        const int i = static_cast<int>(v);
        return (i >= 0 && i < static_cast<int>(choices.size())) ? choices[static_cast<size_t>(i)] : "?";
    }
    char buf[48];
    const float a = std::fabs(v);
    if (unit == "ms" && a >= 1000.0f) {
        std::snprintf(buf, sizeof buf, "%.2f s", v / 1000.0f);
    } else if (unit == "Hz" && a >= 1000.0f) {
        std::snprintf(buf, sizeof buf, "%.2f kHz", v / 1000.0f);
    } else {
        const char* fmt = a >= 100.0f ? "%.0f" : (a >= 10.0f ? "%.1f" : "%.2f");
        std::snprintf(buf, sizeof buf, fmt, v);
        std::string s(buf);
        if (!unit.empty()) s += (unit == "%" ? "" : " ") + unit;
        return s;
    }
    return buf;
}

namespace P {

ParamSpec flt(std::string id, std::string name, std::string unit, float lo, float hi, float def,
              std::string tooltip, ParamCurve curve, float exponent) {
    ParamSpec p;
    p.id = std::move(id);
    p.name = std::move(name);
    p.unit = std::move(unit);
    p.kind = ParamKind::Float;
    p.minValue = lo;
    p.maxValue = hi;
    p.defaultValue = def;
    p.curve = curve;
    p.curveExponent = exponent;
    p.tooltip = std::move(tooltip);
    return p;
}

ParamSpec logf(std::string id, std::string name, std::string unit, float lo, float hi, float def,
               std::string tooltip) {
    return flt(std::move(id), std::move(name), std::move(unit), lo, hi, def, std::move(tooltip),
               ParamCurve::Log);
}

ParamSpec pct(std::string id, std::string name, float def, std::string tooltip) {
    return flt(std::move(id), std::move(name), "%", 0.0f, 100.0f, def, std::move(tooltip));
}

ParamSpec choice(std::string id, std::string name, std::vector<std::string> choices, int def,
                 std::string tooltip) {
    ParamSpec p;
    p.id = std::move(id);
    p.name = std::move(name);
    p.kind = ParamKind::Choice;
    p.minValue = 0.0f;
    p.maxValue = static_cast<float>(choices.size() - 1);
    p.defaultValue = static_cast<float>(def);
    p.choices = std::move(choices);
    p.tooltip = std::move(tooltip);
    return p;
}

ParamSpec toggle(std::string id, std::string name, bool def, std::string tooltip) {
    ParamSpec p;
    p.id = std::move(id);
    p.name = std::move(name);
    p.kind = ParamKind::Toggle;
    p.minValue = 0.0f;
    p.maxValue = 1.0f;
    p.defaultValue = def ? 1.0f : 0.0f;
    p.tooltip = std::move(tooltip);
    return p;
}

ParamSpec& adv(ParamSpec& p) {
    p.advanced = true;
    return p;
}

}  // namespace P
}  // namespace ih1
