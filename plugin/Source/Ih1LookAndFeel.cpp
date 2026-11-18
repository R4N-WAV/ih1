#include "Ih1LookAndFeel.h"

namespace ih1::plugin {

juce::Font LookAndFeel::font(float size, bool bold) {
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}

LookAndFeel::LookAndFeel() {
    using namespace juce;
    setColour(ResizableWindow::backgroundColourId, Colours::background);
    setColour(Label::textColourId, Colours::text);
    setColour(Slider::textBoxTextColourId, Colours::text);
    setColour(Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(ComboBox::backgroundColourId, Colours::panelRaised);
    setColour(ComboBox::textColourId, Colours::text);
    setColour(ComboBox::outlineColourId, Colours::border);
    setColour(ComboBox::arrowColourId, Colours::textDim);
    setColour(PopupMenu::backgroundColourId, Colours::panelRaised);
    setColour(PopupMenu::textColourId, Colours::text);
    setColour(PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha(0.25f));
    setColour(PopupMenu::highlightedTextColourId, Colours::text);
    setColour(TextButton::textColourOffId, Colours::text);
    setColour(TextButton::textColourOnId, Colours::background);
    setColour(TooltipWindow::textColourId, Colours::text);
    setColour(ScrollBar::thumbColourId, Colours::border);
    setColour(AlertWindow::backgroundColourId, Colours::panelRaised);
    setColour(AlertWindow::textColourId, Colours::text);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) {
    using namespace juce;
    const auto bounds = Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)).reduced(4.0f);
    const float r = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float ang = start + pos * (end - start);
    const float stroke = std::max(2.0f, r * 0.09f);
    Path track;
    track.addCentredArc(c.x, c.y, r - stroke, r - stroke, 0.0f, start, end, true);
    g.setColour(Colours::border);
    g.strokePath(track, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::rounded));
    // bipolar params draw from the centre
    const bool bipolar = s.getProperties().contains("zeroNorm");
    const float from = bipolar ? start + static_cast<float>(static_cast<double>(s.getProperties()["zeroNorm"])) * (end - start) : start;
    Path val;
    val.addCentredArc(c.x, c.y, r - stroke, r - stroke, 0.0f, std::min(from, ang), std::max(from, ang), true);
    const bool hot = s.isMouseOverOrDragging();
    g.setColour(s.isEnabled() ? (hot ? Colours::text : Colours::accent) : Colours::textFaint);
    g.strokePath(val, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::rounded));
    // pointer
    const Point<float> p(c.x + (r - stroke * 2.6f) * std::sin(ang), c.y - (r - stroke * 2.6f) * std::cos(ang));
    g.setColour(Colours::text.withAlpha(s.isEnabled() ? 0.9f : 0.3f));
    g.fillEllipse(Rectangle<float>(stroke * 1.2f, stroke * 1.2f).withCentre(p));
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) {
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const bool primary = b.getProperties()["primary"];
    const bool on = b.getToggleState();
    const auto activeColour = b.getProperties().contains("activeColour") ? Colour::fromString(b.getProperties()["activeColour"].toString()) : Colours::accent;
    Colour fill = Colours::panelRaised;
    if (primary) fill = Colours::accent;
    if (on) fill = activeColour;
    if (!b.isEnabled()) fill = fill.withAlpha(primary ? 0.25f : 0.5f);
    else if (down) fill = fill.brighter(0.15f);
    else if (over) fill = fill.brighter(0.07f);
    g.setColour(fill);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(primary || on ? fill.brighter(0.2f) : Colours::border.brighter(over ? 0.3f : 0.0f));
    g.drawRoundedRectangle(r, 4.0f, 1.0f);
}

void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) {
    const bool dark = b.getProperties()["primary"] || b.getToggleState();
    g.setColour((dark ? Colours::background : Colours::text).withAlpha(b.isEnabled() ? 1.0f : 0.45f));
    g.setFont(font(b.getProperties().contains("fontSize") ? static_cast<float>(static_cast<double>(b.getProperties()["fontSize"])) : 14.0f, true));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(4, 2), juce::Justification::centred, 1);
}

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool over, bool) {
    using namespace juce;
    auto r = b.getLocalBounds().toFloat();
    const float h = std::min(18.0f, r.getHeight() - 4.0f), w = h * 1.8f;
    auto pill = Rectangle<float>(w, h).withCentre({r.getX() + w * 0.5f + 2.0f, r.getCentreY()});
    const bool on = b.getToggleState();
    g.setColour(on ? Colours::accent : Colours::panelRaised);
    g.fillRoundedRectangle(pill, h * 0.5f);
    g.setColour(Colours::border.brighter(over ? 0.3f : 0.0f));
    g.drawRoundedRectangle(pill, h * 0.5f, 1.0f);
    const float k = h - 6.0f;
    g.setColour(on ? Colours::background : Colours::textDim);
    g.fillEllipse(on ? pill.getRight() - k - 3.0f : pill.getX() + 3.0f, pill.getY() + 3.0f, k, k);
    g.setColour(Colours::text);
    g.setFont(font(13.0f));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().withTrimmedLeft(static_cast<int>(w) + 8), Justification::centredLeft, 1);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
    using namespace juce;
    auto r = Rectangle<float>(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)).reduced(0.5f);
    g.setColour(Colours::panelRaised);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(Colours::border.brighter(box.isMouseOver() ? 0.3f : 0.0f));
    g.drawRoundedRectangle(r, 4.0f, 1.0f);
    Path arrow;
    const float ax = static_cast<float>(w) - 14.0f, ay = static_cast<float>(h) * 0.5f;
    arrow.addTriangle(ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour(Colours::textDim);
    g.fillPath(arrow);
}

void LookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int w, int h) {
    g.fillAll(Colours::panelRaised);
    g.setColour(Colours::border);
    g.drawRect(0, 0, w, h);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds(const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent) {
    juce::AttributedString s;
    s.append(tip, font(13.0f), Colours::text);
    juce::TextLayout tl;
    tl.createLayout(s, 280.0f);
    const int w = static_cast<int>(tl.getWidth()) + 22, h = static_cast<int>(tl.getHeight()) + 16;
    return juce::Rectangle<int>(pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 14, pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h)
        .constrainedWithin(parent);
}

void LookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int w, int h) {
    const auto r = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h));
    g.setColour(Colours::panelRaised.withAlpha(0.97f));
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(Colours::accent.withAlpha(0.35f));
    g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.0f);
    juce::AttributedString s;
    s.append(text, font(13.0f), Colours::text);
    juce::TextLayout tl;
    tl.createLayout(s, static_cast<float>(w) - 20.0f);
    tl.draw(g, r.reduced(10.0f, 8.0f));
}

}  // namespace ih1::plugin
