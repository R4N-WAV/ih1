// IH1 look and feel: dark, quiet, legible. Colours come from the measured reference palette.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ih1::plugin {

namespace Colours {
const juce::Colour background{0xff0a080d};
const juce::Colour panel{0xff110e17};
const juce::Colour panelRaised{0xff17131f};
const juce::Colour border{0xff2b2438};
const juce::Colour text{0xffe2dbef};
const juce::Colour textDim{0xff8a809c};
const juce::Colour textFaint{0xff554d63};
const juce::Colour accent{0xffb9a4e8};    // lavender
const juce::Colour accent2{0xffd090c6};   // muted magenta
const juce::Colour ice{0xff9fc4e6};       // icy blue (sparingly)
const juce::Colour danger{0xffd77a9a};
}  // namespace Colours

class LookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawPopupMenuBackground(juce::Graphics&, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(14.0f); }
    juce::Font getPopupMenuFont() override { return font(14.0f); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return font(14.0f, true); }

    static juce::Font font(float size, bool bold = false);
};

}  // namespace ih1::plugin
