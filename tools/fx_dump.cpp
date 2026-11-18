// fx_dump: prints the FX registry (type ids, families, parameter schema) as plain text.
// Used to author factory presets and to check docs against the real schema.
#include <cstdio>

#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxProcessor.h"

int main() {
    using namespace ih1;
    for (int idx : fxCarouselOrder()) {
        const auto& d = fxRegistry()[static_cast<size_t>(idx)];
        std::printf("%s | %s | %s | midi=%d\n", d.typeId.c_str(), d.name.c_str(), familyName(d.family), d.wantsMidi ? 1 : 0);
        for (const auto& p : d.params) {
            std::printf("   %-14s %-18s ", p.id.c_str(), p.name.c_str());
            if (p.kind == ParamKind::Choice) {
                std::printf("choice def=%g [", p.defaultValue);
                for (size_t k = 0; k < p.choices.size(); ++k) std::printf("%s%zu:%s", k ? ", " : "", k, p.choices[k].c_str());
                std::printf("]");
            } else if (p.kind == ParamKind::Toggle) {
                std::printf("toggle def=%g", p.defaultValue);
            } else {
                std::printf("%g..%g %s def=%g", p.minValue, p.maxValue, p.unit.c_str(), p.defaultValue);
            }
            std::printf("%s\n", p.advanced ? "  (adv)" : "");
        }
    }
    return 0;
}
