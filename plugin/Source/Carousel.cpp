#include "Carousel.h"

#include "Ih1LookAndFeel.h"
#include "ih1/fx/FxProcessor.h"

namespace ih1::plugin {

namespace {
juce::Path triangle(bool up) {
    juce::Path p;
    if (up) p.addTriangle(0.0f, 10.0f, 7.0f, 0.0f, 14.0f, 10.0f);
    else p.addTriangle(0.0f, 0.0f, 14.0f, 0.0f, 7.0f, 10.0f);
    return p;
}
int wrap(int i, int n) { return ((i % n) + n) % n; }
}  // namespace

Carousel::Carousel() {
    order_ = fxCarouselOrder();
    for (auto* b : {&up_, &down_}) {
        b->setShape(triangle(b == &up_), false, true, false);
        b->setOutline(Colours::textDim, 1.4f);
        b->setOnColours(Colours::accent, Colours::accent, Colours::accent);
        b->setColours(juce::Colours::transparentBlack, Colours::accent.withAlpha(0.25f), Colours::accent.withAlpha(0.5f));
        b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible(*b);
    }
    up_.setTooltip("Previous effect (or scroll the mouse wheel over the list).");
    down_.setTooltip("Next effect.");
    up_.onClick = [this] { step(-1); };
    down_.onClick = [this] { step(1); };
    setWantsKeyboardFocus(true);
    setTitle("Effect carousel");
    setDescription("Scroll or use the arrows to choose the effect to stage.");
}

int Carousel::orderPosOf(int typeIndex) const {
    for (size_t i = 0; i < order_.size(); ++i) if (order_[i] == typeIndex) return static_cast<int>(i);
    return -1;
}

void Carousel::setSelected(int typeIndex) {
    if (typeIndex == selected_) return;
    selected_ = typeIndex;
    const int pos = orderPosOf(typeIndex);
    if (pos >= 0 && !order_.empty()) {
        // choose the equivalent target nearest to the current animation (smooth wrap-around)
        const int n = static_cast<int>(order_.size());
        const float base = std::round(target_);
        const int cur = wrap(static_cast<int>(base), n);
        int delta = pos - cur;
        if (delta > n / 2) delta -= n;
        if (delta < -n / 2) delta += n;
        target_ = base + static_cast<float>(delta);
        lastPos_ = pos;
        startTimerHz(60);
    }
    repaint();
}

void Carousel::step(int dir) {
    if (order_.empty()) return;
    const int n = static_cast<int>(order_.size());
    const int pos = selected_ >= 0 ? wrap(orderPosOf(selected_) + dir, n) : (dir > 0 ? lastPos_ : lastPos_);
    if (onSelect) onSelect(order_[static_cast<size_t>(pos)]);
}

juce::Rectangle<int> Carousel::nameArea() const { return getLocalBounds().withTrimmedRight(44); }

void Carousel::resized() {
    auto r = getLocalBounds();
    auto arrows = r.removeFromRight(40).withSizeKeepingCentre(22, 64);
    up_.setBounds(arrows.removeFromTop(26).reduced(2));
    arrows.removeFromTop(12);
    down_.setBounds(arrows.removeFromTop(26).reduced(2));
}

void Carousel::timerCallback() {
    anim_ += (target_ - anim_) * 0.22f;
    if (std::fabs(target_ - anim_) < 0.002f) { anim_ = target_; stopTimer(); }
    repaint();
}

void Carousel::paint(juce::Graphics& g) {
    if (order_.empty()) return;
    const auto& reg = fxRegistry();
    const int n = static_cast<int>(order_.size());
    const auto area = nameArea().toFloat();
    const float cy = area.getCentreY() + 6.0f;
    const float spacing = std::min(44.0f, area.getHeight() / 4.2f);
    const int base = static_cast<int>(std::floor(anim_));
    const float frac = anim_ - static_cast<float>(base);
    const bool none = selected_ < 0;
    const auto mouse = getMouseXYRelative();
    for (int k = -3; k <= 3; ++k) {
        const int idx = order_[static_cast<size_t>(wrap(base + k, n))];
        const float off = static_cast<float>(k) - frac;
        const float a = std::abs(off);
        const float dist = a <= 1.0f ? a * spacing * 1.45f : (1.45f + (a - 1.0f)) * spacing;
        const float y = cy + (off < 0.0f ? -dist : dist);
        if (a > 2.6f) continue;
        const bool centre = a < 0.5f && !none;
        float alpha = centre ? 1.0f : juce::jmax(0.0f, 0.55f - 0.22f * a);
        if (none) alpha *= 0.55f;
        const float size = centre ? juce::jmin(32.0f, area.getHeight() * 0.22f) : 16.0f - 2.0f * juce::jmin(a, 1.5f);
        auto row = juce::Rectangle<float>(area.getX(), y - size * 0.75f, area.getWidth(), size * 1.5f);
        if (!centre && row.contains(mouse.toFloat())) alpha = juce::jmin(1.0f, alpha + 0.25f);
        g.setColour((centre ? Colours::text : Colours::textDim).withAlpha(alpha));
        g.setFont(LookAndFeel::font(size, centre));
        g.drawText(juce::String(reg[static_cast<size_t>(idx)].name).toUpperCase(), row, juce::Justification::centred, false);
        if (centre) {
            g.setColour(Colours::accent.withAlpha(0.75f));
            g.setFont(LookAndFeel::font(10.5f, true));
            g.drawText(juce::String(familyName(reg[static_cast<size_t>(idx)].family)).toUpperCase(), juce::Rectangle<float>(row.getX(), y + size * 0.62f, row.getWidth(), 14.0f), juce::Justification::centred, false);
        }
    }
    if (none) {
        g.setColour(Colours::text.withAlpha(0.85f));
        g.setFont(LookAndFeel::font(15.0f));
        auto r = juce::Rectangle<float>(area.getX(), cy - 14.0f, area.getWidth(), 28.0f);
        g.setColour(Colours::background.withAlpha(0.85f));
        g.fillRoundedRectangle(r.withSizeKeepingCentre(200.0f, 26.0f), 13.0f);
        g.setColour(Colours::text.withAlpha(0.8f));
        g.drawText("Choose an effect", r, juce::Justification::centred, false);
    }
    // soft fades at the top/bottom edges
    juce::ColourGradient top(Colours::background, 0.0f, area.getY(), Colours::background.withAlpha(0.0f), 0.0f, area.getY() + spacing * 0.9f, false);
    g.setGradientFill(top);
    g.fillRect(area.withHeight(spacing * 0.9f));
    juce::ColourGradient bot(Colours::background.withAlpha(0.0f), 0.0f, area.getBottom() - spacing * 0.9f, Colours::background, 0.0f, area.getBottom(), false);
    g.setGradientFill(bot);
    g.fillRect(area.withTrimmedTop(area.getHeight() - spacing * 0.9f));
    if (hasKeyboardFocus(false)) {
        g.setColour(Colours::accent.withAlpha(0.25f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 6.0f, 1.0f);
    }
}

void Carousel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
    const double now = juce::Time::getMillisecondCounterHiRes();
    wheelAcc_ += w.deltaY * (w.isReversed ? -1.0f : 1.0f);
    if (std::fabs(wheelAcc_) < 0.07f) return;
    if (now - lastWheelMs_ < 110.0) { wheelAcc_ = 0.0f; return; }  // not disorientingly fast
    step(wheelAcc_ > 0.0f ? -1 : 1);
    wheelAcc_ = 0.0f;
    lastWheelMs_ = now;
}

void Carousel::mouseUp(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    const auto area = nameArea().toFloat();
    const float cy = area.getCentreY() + 6.0f;
    const float spacing = std::min(44.0f, area.getHeight() / 4.2f);
    const float dy = static_cast<float>(e.y) - cy;
    const float ady = std::fabs(dy) / spacing;
    const float slots = ady <= 1.45f ? ady / 1.45f : 1.0f + (ady - 1.45f);
    const int k = static_cast<int>(std::round(dy < 0.0f ? -slots : slots));
    if (k == 0) {
        if (selected_ < 0) step(1);
        return;
    }
    if (std::abs(k) <= 2) {
        const int n = static_cast<int>(order_.size());
        const int pos = wrap((selected_ >= 0 ? orderPosOf(selected_) : lastPos_) + k, n);
        if (onSelect) onSelect(order_[static_cast<size_t>(pos)]);
    }
}

bool Carousel::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::upKey) { step(-1); return true; }
    if (key == juce::KeyPress::downKey) { step(1); return true; }
    const auto ch = key.getTextCharacter();
    if (juce::CharacterFunctions::isLetterOrDigit(ch) || ch == ' ') {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now - typedMs_ > 1000.0) typed_.clear();
        typedMs_ = now;
        typed_ += juce::String::charToString(ch);
        for (int idx : order_) {
            if (juce::String(fxRegistry()[static_cast<size_t>(idx)].name).startsWithIgnoreCase(typed_)) {
                if (onSelect) onSelect(idx);
                break;
            }
        }
        return true;
    }
    return false;
}

}  // namespace ih1::plugin
