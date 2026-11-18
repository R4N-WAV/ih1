#include "ih1/fx/AllFx.h"

namespace ih1::fx {

FxDescriptor describeTestLatency();

void registerAll(std::vector<FxDescriptor>& r) {
    r.push_back(describeTestGain());
    r.push_back(describeTestLatency());
}

}  // namespace ih1::fx
