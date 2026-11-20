#include "ih1/fx/AllFx.h"

namespace ih1::fx {

FxDescriptor describeTestLatency();

void registerAll(std::vector<FxDescriptor>& r) {
    // Time & Space
    r.push_back(describeDelay());
    r.push_back(describeTapeEcho());
    r.push_back(describeReverb());
    r.push_back(describeReverse());
    r.push_back(describeShimmer());
    r.push_back(describeConvolution());
    r.push_back(describeDoppler());
    // Pitch & Voice
    r.push_back(describeHarmony());
    r.push_back(describePitchShift());
    r.push_back(describeFormant());
    r.push_back(describeDoubler());
    r.push_back(describePitchCorrect());
    r.push_back(describeVocoder());
    // Modulation
    r.push_back(describeChorus());
    r.push_back(describeFlanger());
    r.push_back(describePhaser());
    r.push_back(describeVibrato());
    r.push_back(describeTremolo());
    r.push_back(describeFilter());
    r.push_back(describeRingMod());
    r.push_back(describeRotary());
    r.push_back(describeFreqShifter());
    // Texture & Spectral
    r.push_back(describeGrainEngine());
    r.push_back(describeGranular());
    r.push_back(describeSpectral());
    r.push_back(describeStutter());
    r.push_back(describeLooper());
    r.push_back(describeResonator());
    // Tone & Dynamics
    r.push_back(describeEq());
    r.push_back(describeSaturation());
    r.push_back(describeWidth());
    r.push_back(describeCompressor());
    r.push_back(describeGate());
    r.push_back(describeDeEsser());
    r.push_back(describeTransient());
    r.push_back(describeTapeColour());
    r.push_back(describeBitcrusher());
    r.push_back(describeExciter());
    r.push_back(describeAmp());
    // hidden engine-test effects
    r.push_back(describeTestGain());
    r.push_back(describeTestLatency());
}

}  // namespace ih1::fx
