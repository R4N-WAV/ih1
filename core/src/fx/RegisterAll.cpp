#include "ih1/fx/AllFx.h"

namespace ih1::fx {

FxDescriptor describeTestLatency();

void registerAll(std::vector<FxDescriptor>& r) {
    // Time & Space
    r.push_back(describeDelay());
    r.push_back(describeTapeEcho());
    r.push_back(describeReverb());
    r.push_back(describeReverse());
    // Pitch & Voice
    r.push_back(describeHarmony());
    r.push_back(describePitchShift());
    r.push_back(describeFormant());
    r.push_back(describeDoubler());
    // Modulation
    r.push_back(describeChorus());
    r.push_back(describeFilter());
    // Texture & Spectral
    r.push_back(describeGranular());
    r.push_back(describeSpectral());
    r.push_back(describeStutter());
    // Tone & Dynamics
    r.push_back(describeEq());
    r.push_back(describeSaturation());
    r.push_back(describeWidth());
    // hidden engine-test effects
    r.push_back(describeTestGain());
    r.push_back(describeTestLatency());
}

}  // namespace ih1::fx
