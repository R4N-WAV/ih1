// Registration entry points of every FX module. Order here = registry order within a family.
#pragma once

#include <vector>

#include "ih1/fx/FxProcessor.h"

namespace ih1::fx {

void registerAll(std::vector<FxDescriptor>& r);

// individual modules
FxDescriptor describeTestGain();  // hidden test effect used to prove the engine semantics

}  // namespace ih1::fx
