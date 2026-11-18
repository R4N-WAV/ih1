// IH1 look and feel and design tokens: dark, precise, sharp. Corner radius 0-2 px, 1 px hairlines used
// sparingly, colour reserved for FX identities and interaction highlights (V1 feedback 4.12, 4.18).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>

namespace ih1::plugin {

namespace Colours {
const juce::Colour background{0xff09080c};   // control column
const juce::Colour field{0xff000000};        // formation void: pure black
const juce::Colour panel{0xff0e0c12};        // subtle raised tone (menus, dialogs)
const juce::Colour panelRaised{0xff15121b};
const juce::Colour border{0xff262130};       // hairlines (rare)
const juce::Colour text{0xffe6e0f0};
const juce::Colour textDim{0xff8d849e};
const juce::Colour textFaint{0xff57506a};
const juce::Colour accent{0xffbfaef0};       // lavender interaction highlight
const juce::Colour accent2{0xffd59ad0};
const juce::Colour danger{0xffe0566a};       // DELETE only
}  // namespace Colours

namespace Tokens {
constexpr float radius = 1.5f;     // the only corner radius used
constexpr float hairline = 1.0f;
constexpr int gutter = 22;         // outer padding of the control column
constexpr int gap = 12;
constexpr int rowH = 28;
}  // namespace Tokens

// FX identity colour (shared with the formation): visual::fxColour as a juce::Colour.
juce::Colour fxColour(const std::string& typeId, int shade = 0);
juce::Colour fxColourForIndex(int registryIndex);
// A readable version of an identity colour for small text on the dark column.
juce::Colour readable(juce::Colour c, float minBrightness = 0.78f);

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
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
    void fillTextEditorBackground(juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawAlertBox(juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea, juce::TextLayout&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(14.0f); }
    juce::Font getPopupMenuFont() override { return font(14.0f); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return font(13.0f, true); }
    juce::Font getAlertWindowMessageFont() override { return font(14.0f); }
    juce::Font getAlertWindowTitleFont() override { return font(17.0f, true); }

    static juce::Font font(float size, bool bold = false);
};

}  // namespace ih1::plugin
