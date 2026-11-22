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
    // V3 (appended after every existing type: registry order inside a family sets the colour tier, so appending keeps
    // every accepted colour unchanged)
    r.push_back(describeSpectralSmear());
    r.push_back(describeSpectralDelay());
    r.push_back(describeTimeWarp());
    r.push_back(describeGlitchScatter());
    r.push_back(describeDegrade());
    r.push_back(describeFoldCrush());
    // V4 (appended after V3 for the same reason: every accepted colour tier stays where it is)
    r.push_back(describeSliceShuffler());
    r.push_back(describeAttackBody());
    r.push_back(describeEchoMatrix());
    r.push_back(describeReversePitchEcho());
    r.push_back(describeDispersion());
    r.push_back(describeModalResonator());
    r.push_back(describeTextureImprint());
    r.push_back(describeSpectralShatter());
    r.push_back(describePartialRecomposer());
    r.push_back(describeSpectralGraft());
    // V5 (appended after V4: every accepted colour tier stays where it is)
    r.push_back(describeCloudChamber());
    r.push_back(describeReflectionSculptor());
    r.push_back(describeReverseBloom());
    r.push_back(describeSwell());
    r.push_back(describePulseShaper());
    r.push_back(describeDrumCrush());
    r.push_back(describeDrumSpace());
    r.push_back(describeBandTransients());
    r.push_back(describeHitMelody());
    r.push_back(describeTestGain());
    r.push_back(describeTestLatency());
}

}  // namespace ih1::fx
