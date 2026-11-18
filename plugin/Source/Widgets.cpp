#include "Widgets.h"

#include "Ih1LookAndFeel.h"

namespace ih1::plugin {

// =============================================================================================== overlay
MenuOverlay::MenuOverlay() {
    setVisible(false);
    setWantsKeyboardFocus(true);
    setInterceptsMouseClicks(true, false);
    setAlwaysOnTop(true);
    setTitle("Menu");
}

MenuOverlay* MenuOverlay::find(juce::Component* c) {
    for (auto* p = c; p != nullptr; p = p->getParentComponent())
        for (auto* ch : p->getChildren())
            if (auto* m = dynamic_cast<MenuOverlay*>(ch)) return m;
    return nullptr;
}

bool MenuOverlay::selectable(int i) const {
    return i >= 0 && i < static_cast<int>(items_.size()) && !items_[static_cast<size_t>(i)].header && items_[static_cast<size_t>(i)].enabled &&
           items_[static_cast<size_t>(i)].id != 0;
}

void MenuOverlay::open(std::vector<MenuItem> items, const Options& o, Callback cb, juce::Component* owner) {
    if (open_ || items.empty() || getParentComponent() == nullptr) return;  // one menu per gesture
    items_ = std::move(items);
    opts_ = o;
    cb_ = std::move(cb);
    owner_ = owner;
    previousFocus_ = juce::Component::getCurrentlyFocusedComponent();
    setBounds(getParentComponent()->getLocalBounds());

    // size
    const auto font = LookAndFeel::font(14.0f);
    float w = static_cast<float>(o.minWidth);
    for (const auto& it : items_) {
        const float tw = juce::GlyphArrangement::getStringWidth(it.header ? LookAndFeel::font(10.5f, true) : font, it.text) +
                         (it.hint.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth(LookAndFeel::font(11.5f), it.hint) + 24.0f) + 44.0f;
        w = std::max(w, tw);
    }
    const auto area = getLocalBounds().reduced(6);
    const int width = std::min(static_cast<int>(std::ceil(w)), area.getWidth());
    const int rowsFit = std::max(3, (area.getHeight() - 8) / o.rowHeight);
    visibleRows_ = std::min({static_cast<int>(items_.size()), o.maxVisibleRows, rowsFit});
    const int height = visibleRows_ * o.rowHeight + 8;

    // placement: beside the pointer (flipping left/up at the edges) or below/above the anchor
    juce::Rectangle<int> r;
    if (o.atPoint) {
        int x = o.point.x + 4, y = o.point.y + 2;
        if (x + width > area.getRight()) x = o.point.x - width - 4;
        if (y + height > area.getBottom()) y = o.point.y - height - 2;
        r = {x, y, width, height};
        openPoint_ = o.point;
    } else {
        int y = o.anchor.getBottom() + 2;
        if (y + height > area.getBottom() && o.anchor.getY() - height - 2 >= area.getY()) y = o.anchor.getY() - height - 2;
        r = {o.anchor.getX(), y, std::max(width, o.anchor.getWidth()), height};
        openPoint_ = o.anchor.getCentre();
    }
    list_ = r.constrainedWithin(area);

    hot_ = -1;
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) if (items_[static_cast<size_t>(i)].ticked && selectable(i)) hot_ = i;
    scroll_ = 0;
    if (hot_ >= 0) ensureVisible(hot_);
    pressed_ = -1;
    wheelAcc_ = 0.0f;
    typed_.clear();
    open_ = true;
    openedMs_ = juce::Time::getMillisecondCounterHiRes();
    setVisible(true);
    toFront(true);
    grabKeyboardFocus();
    repaint();
}

void MenuOverlay::close(int result) {
    if (!open_) return;
    open_ = false;
    setVisible(false);
    auto cb = std::move(cb_);
    cb_ = nullptr;
    items_.clear();
    owner_ = nullptr;
    pressed_ = hot_ = -1;
    if (auto* f = previousFocus_.getComponent(); f != nullptr && f->isShowing()) f->grabKeyboardFocus();
    previousFocus_ = nullptr;
    if (cb) cb(result);  // exactly once, after the overlay is closed (the callback may open another menu)
}

juce::Rectangle<int> MenuOverlay::rowBounds(int i) const {
    const int row = i - scroll_;
    return {list_.getX(), list_.getY() + 4 + row * opts_.rowHeight, list_.getWidth(), opts_.rowHeight};
}

int MenuOverlay::itemAt(juce::Point<int> p) const {
    if (!list_.contains(p)) return -1;
    const int row = (p.y - list_.getY() - 4) / std::max(1, opts_.rowHeight);
    if (row < 0 || row >= visibleRows_) return -1;
    const int i = row + scroll_;
    return i < static_cast<int>(items_.size()) ? i : -1;
}

void MenuOverlay::ensureVisible(int i) {
    if (i < scroll_) scroll_ = i;
    if (i >= scroll_ + visibleRows_) scroll_ = i - visibleRows_ + 1;
    scroll_ = std::clamp(scroll_, 0, std::max(0, static_cast<int>(items_.size()) - visibleRows_));
}

void MenuOverlay::moveHot(int dir) {
    const int n = static_cast<int>(items_.size());
    if (n == 0) return;
    int i = hot_;
    for (int k = 0; k < n; ++k) {
        i = i < 0 ? (dir > 0 ? 0 : n - 1) : i + dir;
        if (i < 0 || i >= n) { i = hot_; break; }
        if (selectable(i)) break;
    }
    if (selectable(i)) { hot_ = i; ensureVisible(i); repaint(); }
}

void MenuOverlay::paint(juce::Graphics& g) {
    if (!open_) return;
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRect(list_.toFloat().translated(0.0f, 3.0f).expanded(1.0f));
    g.setColour(Colours::panel);
    g.fillRect(list_);
    g.setColour(Colours::border.brighter(0.2f));
    g.drawRect(list_.toFloat(), Tokens::hairline);
    g.saveState();
    g.reduceClipRegion(list_.reduced(1));
    for (int row = 0; row < visibleRows_; ++row) {
        const int i = row + scroll_;
        if (i >= static_cast<int>(items_.size())) break;
        const auto& it = items_[static_cast<size_t>(i)];
        auto r = rowBounds(i).reduced(1, 0);
        if (it.header) {
            g.setColour(Colours::textFaint);
            g.setFont(LookAndFeel::font(10.5f, true).withExtraKerningFactor(0.12f));
            g.drawText(it.text.toUpperCase(), r.withTrimmedLeft(12).withTrimmedTop(6), juce::Justification::centredLeft);
            continue;
        }
        const bool hot = i == hot_;
        if (hot && it.enabled) {
            g.setColour(Colours::text.withAlpha(0.07f));
            g.fillRect(r);
            g.setColour(Colours::accent);
            g.fillRect(r.getX(), r.getY() + 5, 2, r.getHeight() - 10);
        }
        juce::Colour c = it.colour.isTransparent() ? Colours::text : readable(it.colour);
        if (!it.enabled) c = Colours::textFaint;
        else if (!hot) c = c.withMultipliedAlpha(0.88f);
        g.setColour(c);
        g.setFont(LookAndFeel::font(14.0f, it.ticked));
        g.drawText(it.text, r.withTrimmedLeft(14).withTrimmedRight(it.hint.isEmpty() ? 26 : 12), juce::Justification::centredLeft);
        if (it.hint.isNotEmpty()) {
            g.setColour(Colours::textFaint);
            g.setFont(LookAndFeel::font(11.5f));
            g.drawText(it.hint, r.withTrimmedRight(26), juce::Justification::centredRight);
        }
        if (it.ticked) {
            g.setColour(c);
            g.fillRect(juce::Rectangle<float>(4.0f, 4.0f).withCentre({static_cast<float>(r.getRight()) - 13.0f, static_cast<float>(r.getCentreY())}));
        }
    }
    g.restoreState();
    // scroll hints
    if (scroll_ > 0) { g.setColour(Colours::textDim); g.fillRect(list_.getCentreX() - 8, list_.getY() + 2, 16, 1); }
    if (scroll_ + visibleRows_ < static_cast<int>(items_.size())) { g.setColour(Colours::textDim); g.fillRect(list_.getCentreX() - 8, list_.getBottom() - 3, 16, 1); }
}

void MenuOverlay::mouseMove(const juce::MouseEvent& e) {
    const int i = itemAt(e.getPosition());
    if (i != hot_ && (selectable(i) || i < 0)) { if (i >= 0) hot_ = i; repaint(); }
}

void MenuOverlay::mouseDown(const juce::MouseEvent& e) {
    if (!open_) return;
    const int i = itemAt(e.getPosition());
    if (!list_.contains(e.getPosition())) { close(0); return; }  // outside: cancel, consume (no click-through)
    pressed_ = selectable(i) ? i : -1;
    if (pressed_ >= 0) { hot_ = pressed_; repaint(); }
}

void MenuOverlay::mouseDrag(const juce::MouseEvent& e) {
    const int i = itemAt(e.getPosition());
    if (selectable(i) && i != hot_) { hot_ = i; repaint(); }
}

void MenuOverlay::mouseUp(const juce::MouseEvent& e) {
    if (!open_) return;
    const int i = itemAt(e.getPosition());
    const int p = pressed_;
    pressed_ = -1;
    if (p >= 0 && i == p && selectable(i)) close(items_[static_cast<size_t>(i)].id);
}

void MenuOverlay::ownerMouseUp(const juce::MouseEvent& eInOwner) {
    if (!open_) return;
    const auto pos = getLocalPoint(eInOwner.eventComponent, eInOwner.getPosition());
    const int i = itemAt(pos);
    // press on the control, drag onto an item, release: selects. A plain click (no travel) just leaves the
    // menu open for the next click.
    if (selectable(i) && pos.getDistanceFrom(openPoint_) > 6 && juce::Time::getMillisecondCounterHiRes() - openedMs_ > 120.0)
        close(items_[static_cast<size_t>(i)].id);
}

void MenuOverlay::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
    if (!open_) return;  // consumed either way: the carousel beneath never scrolls while a menu is open
    wheelAcc_ += w.deltaY * (w.isReversed ? -1.0f : 1.0f);
    const int steps = static_cast<int>(wheelAcc_ / 0.12f);
    if (steps != 0) {
        wheelAcc_ -= static_cast<float>(steps) * 0.12f;
        scroll_ = std::clamp(scroll_ - steps, 0, std::max(0, static_cast<int>(items_.size()) - visibleRows_));
        repaint();
    }
}

bool MenuOverlay::keyPressed(const juce::KeyPress& k) {
    if (!open_) return false;
    if (k == juce::KeyPress::escapeKey) { close(0); return true; }
    if (k == juce::KeyPress::upKey) { moveHot(-1); return true; }
    if (k == juce::KeyPress::downKey) { moveHot(1); return true; }
    if (k == juce::KeyPress::pageUpKey) { for (int i = 0; i < visibleRows_ - 1; ++i) moveHot(-1); return true; }
    if (k == juce::KeyPress::pageDownKey) { for (int i = 0; i < visibleRows_ - 1; ++i) moveHot(1); return true; }
    if (k == juce::KeyPress::homeKey) { hot_ = -1; moveHot(1); return true; }
    if (k == juce::KeyPress::endKey) { hot_ = -1; moveHot(-1); return true; }
    if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey) {
        if (selectable(hot_)) close(items_[static_cast<size_t>(hot_)].id);
        return true;
    }
    const auto ch = k.getTextCharacter();
    if (juce::CharacterFunctions::isLetterOrDigit(ch)) {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now - typedMs_ > 900.0) typed_.clear();
        typedMs_ = now;
        typed_ += juce::String::charToString(ch);
        for (int i = 0; i < static_cast<int>(items_.size()); ++i)
            if (selectable(i) && items_[static_cast<size_t>(i)].text.startsWithIgnoreCase(typed_)) { hot_ = i; ensureVisible(i); repaint(); break; }
    }
    return true;  // keyboard stays with the menu while it is open
}

void MenuOverlay::parentSizeChanged() {
    if (open_) close(0);
}

// =============================================================================================== dropdown
Dropdown::Dropdown() {
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    accent_ = Colours::accent;
}

void Dropdown::setItems(const juce::StringArray& items) {
    items_ = items;
    if (selected_ >= items_.size()) selected_ = -1;
    repaint();
}

void Dropdown::setSelectedIndex(int index, bool notify) {
    if (index < -1 || index >= items_.size() || index == selected_) return;
    selected_ = index;
    repaint();
    if (notify && onChange) onChange(index);
}

void Dropdown::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    const bool hot = isMouseOver() || (MenuOverlay::find(this) && MenuOverlay::find(this)->isOpenFor(this));
    g.setColour((hot ? Colours::text : Colours::border.brighter(0.5f)).withAlpha(isEnabled() ? 1.0f : 0.3f));
    g.fillRect(r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);  // underline only, no box
    g.setColour(Colours::text.withAlpha(isEnabled() ? 1.0f : 0.35f));
    g.setFont(LookAndFeel::font(14.0f));
    g.drawFittedText(selected_ >= 0 ? items_[selected_] : juce::String("-"), getLocalBounds().withTrimmedRight(18).withTrimmedLeft(2), juce::Justification::centredLeft, 1, 0.8f);
    juce::Path chev;
    const float cx = r.getRight() - 8.0f, cy = r.getCentreY();
    chev.startNewSubPath(cx - 4.0f, cy - 2.0f);
    chev.lineTo(cx, cy + 2.0f);
    chev.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour((hot ? accent_ : Colours::textDim).withAlpha(isEnabled() ? 1.0f : 0.3f));
    g.strokePath(chev, juce::PathStrokeType(1.2f));
    if (keyFocus_ && hasKeyboardFocus(false)) { g.setColour(accent_); g.fillRect(r.getX(), r.getBottom() - 2.0f, r.getWidth(), 2.0f); }
}

void Dropdown::showMenu() {
    auto* ov = MenuOverlay::find(this);
    if (ov == nullptr || ov->isOpen() || items_.isEmpty() || !isEnabled()) return;
    std::vector<MenuItem> items;
    for (int i = 0; i < items_.size(); ++i) {
        MenuItem it;
        it.id = i + 1;
        it.text = items_[i];
        it.ticked = i == selected_;
        items.push_back(it);
    }
    MenuOverlay::Options o;
    o.anchor = ov->getLocalArea(this, getLocalBounds());
    o.minWidth = std::max(120, getWidth());
    ov->open(std::move(items), o, [safe = juce::Component::SafePointer<Dropdown>(this)](int id) {
        if (safe == nullptr || id <= 0) return;  // cancelled, or this control no longer exists
        safe->setSelectedIndex(id - 1, true);
        safe->repaint();
    }, this);
    repaint();
}

void Dropdown::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) return;
    showMenu();  // opens on press, exactly once
}

void Dropdown::mouseUp(const juce::MouseEvent& e) {
    if (auto* ov = MenuOverlay::find(this); ov && ov->isOpenFor(this)) ov->ownerMouseUp(e);
}

bool Dropdown::keyPressed(const juce::KeyPress& k) {
    if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey || k == juce::KeyPress(juce::KeyPress::downKey, juce::ModifierKeys::altModifier, 0)) {
        showMenu();
        return true;
    }
    return false;
}

// =============================================================================================== segmented
Segmented::Segmented() {
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    accent_ = Colours::accent;
}

bool Segmented::suits(const juce::StringArray& items, int width) {
    if (items.size() < 2 || items.size() > 5) return false;
    float total = 0.0f;
    for (const auto& s : items) total += juce::GlyphArrangement::getStringWidth(LookAndFeel::font(13.0f), s) + 14.0f;
    return total <= static_cast<float>(width);
}

void Segmented::setItems(const juce::StringArray& items) { items_ = items; repaint(); }

void Segmented::setSelectedIndex(int index, bool notify) {
    if (index < 0 || index >= items_.size() || index == selected_) return;
    selected_ = index;
    repaint();
    if (notify && onChange) onChange(index);
}

int Segmented::segmentAt(int x) const {
    if (items_.isEmpty()) return -1;
    return std::clamp(x * items_.size() / std::max(1, getWidth()), 0, items_.size() - 1);
}

void Segmented::paint(juce::Graphics& g) {
    const int n = items_.size();
    if (n == 0) return;
    const float w = static_cast<float>(getWidth()) / static_cast<float>(n);
    const int hot = isMouseOver() ? segmentAt(getMouseXYRelative().x) : -1;
    g.setColour(Colours::border);
    g.fillRect(0.0f, static_cast<float>(getHeight()) - 1.0f, static_cast<float>(getWidth()), 1.0f);
    for (int i = 0; i < n; ++i) {
        const auto r = juce::Rectangle<float>(w * static_cast<float>(i), 0.0f, w, static_cast<float>(getHeight()));
        const bool on = i == selected_;
        g.setColour(on ? accent_ : (i == hot ? Colours::text : Colours::textDim));
        g.setFont(LookAndFeel::font(13.0f, on));
        g.drawFittedText(items_[i], r.toNearestInt().reduced(2, 0), juce::Justification::centred, 1, 0.75f);
        if (on) g.fillRect(r.getX() + 4.0f, r.getBottom() - 2.0f, r.getWidth() - 8.0f, 2.0f);
    }
    if (keyFocus_ && hasKeyboardFocus(false)) { g.setColour(accent_.withAlpha(0.5f)); g.fillRect(0, 0, 2, getHeight()); }
}

void Segmented::mouseDown(const juce::MouseEvent& e) {
    if (!isEnabled() || e.mods.isPopupMenu()) return;
    setSelectedIndex(segmentAt(e.x), true);
}

bool Segmented::keyPressed(const juce::KeyPress& k) {
    if (k == juce::KeyPress::leftKey) { setSelectedIndex(std::max(0, selected_ - 1), true); return true; }
    if (k == juce::KeyPress::rightKey) { setSelectedIndex(std::min(items_.size() - 1, selected_ + 1), true); return true; }
    return false;
}

}  // namespace ih1::plugin
