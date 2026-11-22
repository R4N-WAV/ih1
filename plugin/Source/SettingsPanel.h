// Staged FX settings (UI sketch): S (SOLO FX) / M (MUTE FX) at the top-right, a one-line description, an
// effect-specific graph where one helps, then the controls generated from the FX's parameter schema:
// selectors first, then the primary sound-shaping knobs sized to the space available, then the advanced
// controls, always visible under an "Advanced" label (V2.1 feedback 8). Controls that depend on another one
// (a free time knob while Sync is on) are greyed out, ignore every gesture and say why in their tooltip.
// While the staged FX is MUTED the whole panel is greyed and inactive except S / M (V2.1 9.7).
// No border; only shown while an effect is staged (V1 feedback 4.11, 4.12, 4.19).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "FxGraph.h"
#include "Widgets.h"

namespace ih1::plugin {

class Processor;

class SettingsPanel final : public juce::Component {
public:
    explicit SettingsPanel(Processor& p);
    ~SettingsPanel() override;
    void sync();  // editor timer: rebuild on type change, refresh values, S/M state, graph
    juce::Component* controlFor(int paramIndex) const;  // tests

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Ctl {
        int index = 0;
        bool advanced = false;
        std::unique_ptr<juce::Label> label, value;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<Dropdown> drop;
        std::unique_ptr<Segmented> seg;
        std::unique_ptr<juce::ToggleButton> toggle;
        bool dragging = false;
        bool active = true;
        juce::String tooltip;
    };
    void rebuild(int typeIndex);
    void layoutControls();
    void refreshValues();
    void applyEnablement(const ParamValues& v, bool muted);
    void setFromUi(int index, float plain);

    Processor& proc_;
    int type_ = -2;
    std::vector<std::unique_ptr<Ctl>> ctls_;
    juce::Viewport viewport_;
    juce::Component content_;
    FxGraph graph_;
    bool showGraph_ = false;
    juce::TextButton solo_{"S"}, mute_{"M"};
    juce::Label desc_, advHeading_;
    bool muted_ = false;
    bool standby_ = false;  // V5: muted by COMMIT; controls stay usable and editing one starts the audition
    juce::String descText_;
};

}  // namespace ih1::plugin
