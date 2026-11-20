#include "Carousel.h"

#include "Ih1LookAndFeel.h"
#include "Widgets.h"
#include "ih1/fx/FxProcessor.h"

namespace ih1::plugin {

namespace {
juce::Path chevron(bool up) {
    juce::Path p;
    if (up) { p.startNewSubPath(0.0f, 7.0f); p.lineTo(7.0f, 0.0f); p.lineTo(14.0f, 7.0f); }
    else { p.startNewSubPath(0.0f, 0.0f); p.lineTo(7.0f, 7.0f); p.lineTo(14.0f, 0.0f); }
    return p;
}
int wrap(int i, int n) { return ((i % n) + n) % n; }
}  // namespace

Carousel::Carousel() {
    order_ = fxCarouselOrder();
    for (auto* b : {&up_, &down_}) {
        b->setShape(chevron(b == &up_), false, true, false);
        b->setOutline(Colours::textDim, 1.3f);
        b->setColours(juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack);
        b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
        b->setWantsKeyboardFocus(false);
        addAndMakeVisible(*b);
    }
    up_.setTooltip("Previous effect. You can also scroll over the list, use the arrow keys, or right-click for the full list.");
    down_.setTooltip("Next effect.");
    up_.onClick = [this] { step(-1); };
    down_.onClick = [this] { step(1); };
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    setTitle("Effect carousel");
    setDescription("Scroll or use the arrows to choose the effect to stage. Right-click for the full list.");
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
    // from "None", the first step lands on the effect the carousel was resting on
    const int pos = selected_ >= 0 ? wrap(orderPosOf(selected_) + dir, n) : lastPos_;
    if (onSelect) onSelect(order_[static_cast<size_t>(pos)]);
}

juce::Rectangle<int> Carousel::nameArea() const { return getLocalBounds().withTrimmedRight(40); }

void Carousel::resized() {
    auto r = getLocalBounds();
    auto arrows = r.removeFromRight(34).withSizeKeepingCentre(22, 60);
    up_.setBounds(arrows.removeFromTop(22).reduced(3));
    arrows.removeFromTop(16);
    down_.setBounds(arrows.removeFromTop(22).reduced(3));
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
    const float cy = area.getCentreY();
    const float spacing = std::min(40.0f, area.getHeight() / 4.6f);
    const int base = static_cast<int>(std::floor(anim_));
    const float frac = anim_ - static_cast<float>(base);
    const bool none = selected_ < 0;
    const auto mouse = getMouseXYRelative().toFloat();
    const bool mouseIn = isMouseOver(true);
    for (int k = -3; k <= 3; ++k) {
        const int idx = order_[static_cast<size_t>(wrap(base + k, n))];
        const float off = static_cast<float>(k) - frac;
        const float a = std::abs(off);
        if (a > 2.6f) continue;
        const bool centre = a < 0.5f && !none;
        if (none && a < 0.5f) continue;  // the centre reads "None"
        const float dist = a <= 1.0f ? a * spacing * 1.5f : (1.5f + (a - 1.0f)) * spacing;
        const float y = cy + (off < 0.0f ? -dist : dist);
        float alpha = centre ? 1.0f : juce::jmax(0.0f, 0.75f - 0.27f * a);
        const float size = centre ? juce::jmin(30.0f, area.getHeight() * 0.2f) : 15.0f - 1.6f * juce::jmin(a, 1.5f);
        auto row = juce::Rectangle<float>(area.getX(), y - size * 0.75f, area.getWidth(), size * 1.5f);
        if (!centre && mouseIn && row.contains(mouse)) alpha = juce::jmin(1.0f, alpha + 0.3f);
        const auto col = readable(fxColourForIndex(idx), centre ? 0.88f : 0.8f);
        g.setColour(col.withAlpha(alpha));
        g.setFont(LookAndFeel::font(size, centre).withExtraKerningFactor(centre ? 0.04f : 0.08f));
        g.drawText(juce::String(reg[static_cast<size_t>(idx)].name).toUpperCase(), row, juce::Justification::centred, false);
        if (centre) {
            g.setColour(Colours::textDim.withAlpha(0.8f));
            g.setFont(LookAndFeel::font(10.0f, true).withExtraKerningFactor(0.14f));
            g.drawText(juce::String(categoryName(reg[static_cast<size_t>(idx)].category)).toUpperCase(),
                       juce::Rectangle<float>(row.getX(), y + size * 0.62f, row.getWidth(), 14.0f), juce::Justification::centred, false);
        }
    }
    if (none) {
        g.setColour(Colours::text.withAlpha(0.72f));
        g.setFont(LookAndFeel::font(juce::jmin(26.0f, area.getHeight() * 0.18f)).withExtraKerningFactor(0.04f));
        g.drawText("None", juce::Rectangle<float>(area.getX(), cy - 18.0f, area.getWidth(), 36.0f), juce::Justification::centred, false);
    }
    // soft fades at the top/bottom edges (tone only, no lines)
    const float fade = spacing * 0.9f;
    juce::ColourGradient top(Colours::background, 0.0f, area.getY(), Colours::background.withAlpha(0.0f), 0.0f, area.getY() + fade, false);
    g.setGradientFill(top);
    g.fillRect(area.withHeight(fade));
    juce::ColourGradient bot(Colours::background.withAlpha(0.0f), 0.0f, area.getBottom() - fade, Colours::background, 0.0f, area.getBottom(), false);
    g.setGradientFill(bot);
    g.fillRect(area.withTrimmedTop(area.getHeight() - fade));
    // keyboard focus only (never on mouse clicks): a small accent tick at the left of the selected name
    if (keyFocus_ && hasKeyboardFocus(false)) {
        g.setColour(Colours::accent.withAlpha(0.8f));
        g.fillRect(area.getX() + 4.0f, cy - 9.0f, 2.0f, 18.0f);
    }
}

void Carousel::focusGained(FocusChangeType cause) {
    keyFocus_ = cause == focusChangedByTabKey;
    if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) keyFocus_ = false;
    repaint();
}

void Carousel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
    // accumulate high-resolution wheels and trackpads; one step per notch, at most one every 70 ms
    wheelAcc_ += w.deltaY * (w.isReversed ? -1.0f : 1.0f);
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (std::fabs(wheelAcc_) < 0.1f) return;
    if (now - lastStepMs_ < 70.0) { wheelAcc_ = juce::jlimit(-0.1f, 0.1f, wheelAcc_); return; }
    step(wheelAcc_ > 0.0f ? -1 : 1);
    wheelAcc_ = 0.0f;
    lastStepMs_ = now;
}

void Carousel::showCatalogueMenu(juce::Point<int> atLocal) {
    auto* ov = MenuOverlay::find(this);
    if (ov == nullptr || ov->isOpen()) return;
    // V2.1: categories (data-driven from the registry) each open a submenu of their effects, every name in its
    // family colour; choosing stages the effect (never commits). The mouse wheel / arrow carousel is unchanged.
    std::vector<MenuItem> items;
    const auto& reg = fxRegistry();
    for (int cat = 0; cat < kNumCategories; ++cat) {
        MenuItem parent;
        parent.text = categoryName(static_cast<FxCategory>(cat));
        parent.colour = categoryColour(cat);
        for (int idx : order_) {
            const auto& d = reg[static_cast<size_t>(idx)];
            if (static_cast<int>(d.category) != cat) continue;
            MenuItem it;
            it.id = idx + 1;
            it.text = d.name;
            it.colour = fxColourForIndex(idx);
            it.ticked = idx == selected_;
            parent.sub.push_back(it);
        }
        if (parent.sub.empty()) continue;
        parent.hint = juce::String(static_cast<int>(parent.sub.size()));
        items.push_back(std::move(parent));
    }
    MenuOverlay::Options o;
    o.atPoint = true;
    o.point = ov->getLocalPoint(this, atLocal);
    o.minWidth = 190;
    o.maxVisibleRows = 22;
    o.rowHeight = 25;
    ov->open(std::move(items), o, [safe = juce::Component::SafePointer<Carousel>(this)](int id) {
        if (safe != nullptr && id > 0 && safe->onSelect) safe->onSelect(id - 1);
    }, this);
}

void Carousel::mouseDown(const juce::MouseEvent& e) {
    rightDown_ = e.mods.isPopupMenu();
    if (rightDown_) showCatalogueMenu(e.getPosition());  // opens beside the pointer on the press
}

void Carousel::mouseUp(const juce::MouseEvent& e) {
    if (rightDown_) {
        rightDown_ = false;
        if (auto* ov = MenuOverlay::find(this); ov && ov->isOpenFor(this)) ov->ownerMouseUp(e);
        return;
    }
    if (!nameArea().contains(e.getPosition()) || e.mouseWasDraggedSinceMouseDown()) return;
    const auto area = nameArea().toFloat();
    const float cy = area.getCentreY();
    const float spacing = std::min(40.0f, area.getHeight() / 4.6f);
    const float dy = static_cast<float>(e.y) - cy;
    const float ady = std::fabs(dy) / spacing;
    const float slots = ady <= 1.5f ? ady / 1.5f : 1.0f + (ady - 1.5f);
    const int k = static_cast<int>(std::round(dy < 0.0f ? -slots : slots));
    if (k == 0) {
        if (selected_ < 0) step(1);
        return;
    }
    if (std::abs(k) <= 2 && !order_.empty()) {
        const int n = static_cast<int>(order_.size());
        const int pos = wrap((selected_ >= 0 ? orderPosOf(selected_) : lastPos_) + k, n);
        if (onSelect) onSelect(order_[static_cast<size_t>(pos)]);
    }
}

bool Carousel::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::upKey) { keyFocus_ = true; step(-1); return true; }
    if (key == juce::KeyPress::downKey) { keyFocus_ = true; step(1); return true; }
    if (key == juce::KeyPress::escapeKey && selected_ >= 0) { if (onSelect) onSelect(-1); return true; }
    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) return false;
    const auto ch = key.getTextCharacter();
    if (juce::CharacterFunctions::isLetterOrDigit(ch)) {
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
