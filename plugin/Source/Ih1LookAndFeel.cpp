#include "Ih1LookAndFeel.h"

#include "ih1/fx/FxProcessor.h"
#include "ih1/visual/Palette.h"

namespace ih1::plugin {

juce::Colour fxColour(const std::string& typeId, int shade) {
    const auto c = visual::fxColour(typeId, shade);
    return juce::Colour::fromFloatRGBA(c.r, c.g, c.b, 1.0f);
}

juce::Colour fxColourForIndex(int idx) {
    const auto& reg = fxRegistry();
    if (idx < 0 || idx >= static_cast<int>(reg.size())) return Colours::text;
    return fxColour(reg[static_cast<size_t>(idx)].typeId);
}

juce::Colour readable(juce::Colour c, float minBrightness) {
    return c.getBrightness() < minBrightness ? c.withBrightness(minBrightness) : c;
}

juce::Font LookAndFeel::font(float size, bool bold) {
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}

LookAndFeel::LookAndFeel() {
    using namespace juce;
    setColour(ResizableWindow::backgroundColourId, Colours::background);
    setColour(Label::textColourId, Colours::text);
    setColour(Slider::textBoxTextColourId, Colours::text);
    setColour(Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(Slider::textBoxBackgroundColourId, Colours::panel);
    setColour(Slider::textBoxHighlightColourId, Colours::accent.withAlpha(0.35f));
    setColour(ComboBox::backgroundColourId, Colours::panel);
    setColour(ComboBox::textColourId, Colours::text);
    setColour(ComboBox::outlineColourId, Colours::border);
    setColour(ComboBox::arrowColourId, Colours::textDim);
    setColour(PopupMenu::backgroundColourId, Colours::panel);
    setColour(PopupMenu::textColourId, Colours::text);
    setColour(PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha(0.18f));
    setColour(PopupMenu::highlightedTextColourId, Colours::text);
    setColour(TextButton::textColourOffId, Colours::text);
    setColour(TextButton::textColourOnId, Colours::text);
    setColour(TooltipWindow::textColourId, Colours::text);
    setColour(ScrollBar::thumbColourId, Colours::border);
    setColour(AlertWindow::backgroundColourId, Colours::panel);
    setColour(AlertWindow::textColourId, Colours::text);
    setColour(AlertWindow::outlineColourId, Colours::border);
    setColour(TextEditor::backgroundColourId, Colours::panelRaised);
    setColour(TextEditor::textColourId, Colours::text);
    setColour(TextEditor::outlineColourId, Colours::border);
    setColour(TextEditor::focusedOutlineColourId, Colours::accent.withAlpha(0.6f));
    setColour(TextEditor::highlightColourId, Colours::accent.withAlpha(0.3f));
    setColour(CaretComponent::caretColourId, Colours::accent);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) {
    using namespace juce;
    const auto bounds = Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)).reduced(3.0f);
    const float r = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float ang = start + pos * (end - start);
    const float stroke = std::clamp(r * 0.075f, 1.5f, 3.0f);
    const Colour arc = s.getProperties().contains("arcColour") ? Colour::fromString(s.getProperties()["arcColour"].toString()) : Colours::accent;
    Path track;
    track.addCentredArc(c.x, c.y, r - stroke, r - stroke, 0.0f, start, end, true);
    g.setColour(Colours::border);
    g.strokePath(track, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::butt));
    const bool bipolar = s.getProperties().contains("zeroNorm");
    const float from = bipolar ? start + static_cast<float>(static_cast<double>(s.getProperties()["zeroNorm"])) * (end - start) : start;
    Path val;
    val.addCentredArc(c.x, c.y, r - stroke, r - stroke, 0.0f, std::min(from, ang), std::max(from, ang), true);
    const bool hot = s.isMouseOverOrDragging();
    g.setColour(s.isEnabled() ? (hot ? arc.brighter(0.25f) : arc) : Colours::textFaint);
    g.strokePath(val, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::butt));
    // needle: a short precise line from the inner radius
    const float r0 = (r - stroke) * 0.35f, r1 = r - stroke * 2.4f;
    g.setColour(Colours::text.withAlpha(s.isEnabled() ? (hot ? 1.0f : 0.85f) : 0.3f));
    g.drawLine(c.x + r0 * std::sin(ang), c.y - r0 * std::cos(ang), c.x + r1 * std::sin(ang), c.y - r1 * std::cos(ang), std::max(1.2f, stroke * 0.7f));
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) {
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool primary = b.getProperties()["primary"];
    const bool quiet = b.getProperties()["quiet"];  // text-only buttons (S / M, preset arrows)
    const bool on = b.getToggleState();
    const auto active = b.getProperties().contains("activeColour") ? Colour::fromString(b.getProperties()["activeColour"].toString()) : Colours::accent;
    if (quiet) {
        if (on) { g.setColour(active.withAlpha(0.16f)); g.fillRect(r); }
        else if (over && b.isEnabled()) { g.setColour(Colours::text.withAlpha(0.05f)); g.fillRect(r); }
        return;
    }
    // primary (COMMIT): a precise outlined block that fills subtly on hover; secondary: hairline only
    const Colour line = primary ? Colours::accent : Colours::border.brighter(0.4f);
    float fillA = primary ? 0.10f : 0.0f;
    if (b.isEnabled()) { if (down) fillA += 0.18f; else if (over) fillA += 0.08f; }
    g.setColour((primary ? Colours::accent : Colours::text).withAlpha(fillA));
    g.fillRoundedRectangle(r, Tokens::radius);
    g.setColour(line.withAlpha(b.isEnabled() ? (over ? 1.0f : 0.8f) : 0.25f));
    g.drawRoundedRectangle(r, Tokens::radius, Tokens::hairline);
}

void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool over, bool) {
    const bool primary = b.getProperties()["primary"];
    const bool on = b.getToggleState();
    const auto active = b.getProperties().contains("activeColour") ? juce::Colour::fromString(b.getProperties()["activeColour"].toString()) : Colours::accent;
    juce::Colour c = primary ? Colours::accent : Colours::text;
    if (on) c = active;
    else if (b.getProperties()["quiet"]) c = over ? Colours::text : Colours::textDim;
    g.setColour(c.withAlpha(b.isEnabled() ? 1.0f : 0.3f));
    const float size = b.getProperties().contains("fontSize") ? static_cast<float>(static_cast<double>(b.getProperties()["fontSize"])) : 13.0f;
    g.setFont(font(size, true).withExtraKerningFactor(primary ? 0.12f : 0.06f));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(2, 1), juce::Justification::centred, 1);
}

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool over, bool) {
    using namespace juce;
    // a small square switch: filled when on (no pills)
    auto r = b.getLocalBounds().toFloat();
    const float s = std::min(14.0f, r.getHeight() - 6.0f);
    auto box = Rectangle<float>(s, s).withCentre({r.getX() + s * 0.5f + 2.0f, r.getCentreY()});
    const bool on = b.getToggleState();
    const auto col = b.getProperties().contains("arcColour") ? Colour::fromString(b.getProperties()["arcColour"].toString()) : Colours::accent;
    g.setColour((on ? col : Colours::textDim).withAlpha(b.isEnabled() ? (over ? 1.0f : 0.85f) : 0.3f));
    g.drawRect(box, Tokens::hairline);
    if (on) g.fillRect(box.reduced(3.0f));
    g.setColour(Colours::text.withAlpha(b.isEnabled() ? 0.9f : 0.3f));
    g.setFont(font(13.0f));
    g.drawFittedText(on ? "On" : "Off", b.getLocalBounds().withTrimmedLeft(static_cast<int>(s) + 9), Justification::centredLeft, 1);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
    auto r = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)).reduced(0.5f);
    g.setColour(Colours::border.brighter(box.isMouseOver() ? 0.4f : 0.0f));
    g.drawRect(r, Tokens::hairline);
}

void LookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int w, int h) {
    g.fillAll(Colours::panel);
    g.setColour(Colours::border);
    g.drawRect(0, 0, w, h);
}

void LookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) {
    juce::Rectangle<int> t = vertical ? juce::Rectangle<int>(x + w / 2 - 1, thumbStart, 2, thumbSize) : juce::Rectangle<int>(thumbStart, y + h / 2 - 1, thumbSize, 2);
    g.setColour(Colours::textFaint.withAlpha(over || down ? 0.9f : 0.5f));
    g.fillRect(t);
}

void LookAndFeel::fillTextEditorBackground(juce::Graphics& g, int w, int h, juce::TextEditor&) {
    g.setColour(Colours::panelRaised);
    g.fillRect(0, 0, w, h);
}

void LookAndFeel::drawTextEditorOutline(juce::Graphics& g, int w, int h, juce::TextEditor& t) {
    g.setColour(t.hasKeyboardFocus(true) ? Colours::accent.withAlpha(0.6f) : Colours::border);
    g.drawRect(0, 0, w, h);
}

void LookAndFeel::drawAlertBox(juce::Graphics& g, juce::AlertWindow& a, const juce::Rectangle<int>& textArea, juce::TextLayout& tl) {
    g.fillAll(Colours::panel);
    g.setColour(Colours::border);
    g.drawRect(a.getLocalBounds());
    tl.draw(g, textArea.toFloat());
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds(const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent) {
    juce::AttributedString s;
    s.append(tip, font(13.0f), Colours::text);
    juce::TextLayout tl;
    tl.createLayout(s, 280.0f);
    const int w = static_cast<int>(tl.getWidth()) + 22, h = static_cast<int>(tl.getHeight()) + 16;
    // offset away from the pointer so the tooltip never sits on the control being adjusted
    return juce::Rectangle<int>(pos.x > parent.getCentreX() ? pos.x - (w + 16) : pos.x + 18, pos.y > parent.getCentreY() ? pos.y - (h + 14) : pos.y + 22, w, h)
        .constrainedWithin(parent);
}

void LookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int w, int h) {
    const auto r = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h));
    g.setColour(Colours::panel.withAlpha(0.98f));
    g.fillRect(r);
    g.setColour(Colours::border);
    g.drawRect(r, 1.0f);
    juce::AttributedString s;
    s.append(text, font(13.0f), Colours::text);
    juce::TextLayout tl;
    tl.createLayout(s, static_cast<float>(w) - 20.0f);
    tl.draw(g, r.reduced(10.0f, 8.0f));
}

}  // namespace ih1::plugin
