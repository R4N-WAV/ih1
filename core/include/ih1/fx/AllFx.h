// Registration entry points of every FX module. Order here = registry order within a family.
#pragma once

#include <vector>

#include "ih1/fx/FxProcessor.h"

namespace ih1::fx {

void registerAll(std::vector<FxDescriptor>& r);

// individual modules
FxDescriptor describeDelay();
FxDescriptor describeTapeEcho();
FxDescriptor describeReverb();
FxDescriptor describeReverse();
FxDescriptor describeHarmony();
FxDescriptor describePitchShift();
FxDescriptor describeFormant();
FxDescriptor describeDoubler();
FxDescriptor describeChorus();
FxDescriptor describeFilter();
FxDescriptor describeGranular();
FxDescriptor describeSpectral();
FxDescriptor describeStutter();
FxDescriptor describeEq();
FxDescriptor describeSaturation();
FxDescriptor describeWidth();
FxDescriptor describeFlanger();
FxDescriptor describePhaser();
FxDescriptor describeVibrato();
FxDescriptor describeTremolo();
FxDescriptor describeRingMod();
FxDescriptor describeRotary();
FxDescriptor describeCompressor();
FxDescriptor describeGate();
FxDescriptor describeDeEsser();
FxDescriptor describeTransient();
FxDescriptor describeBitcrusher();
FxDescriptor describeTapeColour();
FxDescriptor describeExciter();
FxDescriptor describeShimmer();
FxDescriptor describePitchCorrect();
FxDescriptor describeVocoder();
FxDescriptor describeFreqShifter();
FxDescriptor describeDoppler();
FxDescriptor describeLooper();
FxDescriptor describeResonator();
FxDescriptor describeConvolution();
FxDescriptor describeAmp();
FxDescriptor describeTestGain();  // hidden test effect used to prove the engine semantics

}  // namespace ih1::fx
