// Staged FX settings panel (UI sketch): bordered box, S (SOLO FX) / M (MUTE FX) at the top-right,
// controls generated from the staged FX's parameter schema with plain-language hover tooltips.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace ih1::plugin {

class Processor;

class SettingsPanel final : public juce::Component {
public:
    explicit SettingsPanel(Processor& p);
    ~SettingsPanel() override;
    void sync();  // called by the editor timer: rebuild on type change, refresh values, S/M state

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Ctl {
        int index = 0;
        bool advanced = false;
        std::unique_ptr<juce::Label> label, value;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ToggleButton> toggle;
        bool dragging = false;
    };
    void rebuild(int typeIndex);
    void layoutControls();
    void refreshValues();

    Processor& proc_;
    int type_ = -2;
    std::vector<std::unique_ptr<Ctl>> ctls_;
    juce::Viewport viewport_;
    juce::Component content_;
    juce::TextButton solo_{"S"}, mute_{"M"}, more_{"More"};
    juce::Label desc_, empty_;
    bool showAdvanced_ = false;
};

}  // namespace ih1::plugin
