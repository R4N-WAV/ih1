// Effect-specific visualisations for the settings panel (V1 feedback 4.19). Every graph is computed from
// the staged effect's real current parameter values (or, for the Looper, its live recorded buffer and
// playhead read lock-free from the audio thread). Nothing is decorative or simulated.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

#include "ih1/fx/FxProcessor.h"

namespace ih1::plugin {

class FxGraph final : public juce::Component {
public:
    // Whether the type has a graph (the panel only reserves space when it does).
    static bool hasGraph(const std::string& typeId);
    void setType(int registryIndex);
    void setValues(const ParamValues& v, double bpm);
    void setDisplay(const FxDisplay* d);  // live state (Looper) or nullptr
    void paint(juce::Graphics&) override;

private:
    float val(const char* id, float fallback = 0.0f) const;
    void drawTaps(juce::Graphics&, juce::Rectangle<float>, bool tape);
    void drawDecay(juce::Graphics&, juce::Rectangle<float>);
    void drawResponse(juce::Graphics&, juce::Rectangle<float>);
    void drawTransfer(juce::Graphics&, juce::Rectangle<float>);
    void drawShaper(juce::Graphics&, juce::Rectangle<float>);
    void drawLfo(juce::Graphics&, juce::Rectangle<float>);
    void drawVoices(juce::Graphics&, juce::Rectangle<float>);
    void drawLoop(juce::Graphics&, juce::Rectangle<float>);
    void drawGrains(juce::Graphics&, juce::Rectangle<float>);
    void measureShaper();
    void caption(juce::Graphics&, juce::Rectangle<float>, const juce::String& left, const juce::String& right = {});

    int type_ = -1;
    std::string typeId_;
    ParamValues values_{};
    double bpm_ = 120.0;
    FxDisplay display_;
    bool hasDisplay_ = false;
    juce::Colour colour_;
    std::vector<float> shX_, shY_;  // measured shaper transfer (cached per parameter change)
    float shPeak_ = 1.0f;
};

}  // namespace ih1::plugin
