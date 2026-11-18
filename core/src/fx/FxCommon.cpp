#include "ih1/fx/FxBase.h"

namespace ih1::fx {

std::vector<std::string> divisionLabels() {
    std::vector<std::string> v;
    for (const auto& d : dsp::kDivisions) v.emplace_back(d.label);
    return v;
}

}  // namespace ih1::fx
