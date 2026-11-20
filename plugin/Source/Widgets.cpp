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

MenuOverlay::~MenuOverlay() { stopTimer(); }

MenuOverlay* MenuOverlay::find(juce::Component* c) {
    for (auto* p = c; p != nullptr; p = p->getParentComponent())
        for (auto* ch : p->getChildren())
            if (auto* m = dynamic_cast<MenuOverlay*>(ch)) return m;
    return nullptr;
}

bool MenuOverlay::selectableIn(const Level& L, int i) {
    if (i < 0 || i >= static_cast<int>(L.items.size())) return false;
    const auto& it = L.items[static_cast<size_t>(i)];
    return !it.header && it.enabled && (it.id != 0 || !it.sub.empty());
}

void MenuOverlay::layoutLevel(Level& L, juce::Rectangle<int> near, bool beside) {
    const auto font = LookAndFeel::font(14.0f);
    float w = static_cast<float>(beside ? 150 : opts_.minWidth);
    for (const auto& it : L.items) {
        const float tw = juce::GlyphArrangement::getStringWidth(it.header ? LookAndFeel::font(10.5f, true) : font, it.text) +
                         (it.hint.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth(LookAndFeel::font(11.5f), it.hint) + 24.0f) + 50.0f;
        w = std::max(w, tw);
    }
    const auto area = getLocalBounds().reduced(6);
    const int width = std::min(static_cast<int>(std::ceil(w)), area.getWidth());
    const int rowsFit = std::max(3, (area.getHeight() - 8) / opts_.rowHeight);
    L.visibleRows = std::min({static_cast<int>(L.items.size()), opts_.maxVisibleRows, rowsFit});
    const int height = L.visibleRows * opts_.rowHeight + 8;
    juce::Rectangle<int> r;
    if (beside) {
        // to the right of the parent list, top aligned with the parent row; flips left, shifts up at the edges
        int x = root_.list.getRight() - 2;
        if (x + width > area.getRight()) x = root_.list.getX() - width + 2;
        r = {x, near.getY() - 4, width, height};
    } else if (opts_.atPoint) {
        int x = opts_.point.x + 4, y = opts_.point.y + 2;
        if (x + width > area.getRight()) x = opts_.point.x - width - 4;
        if (y + height > area.getBottom()) y = opts_.point.y - height - 2;
        r = {x, y, width, height};
    } else {
        int y = near.getBottom() + 2;
        if (y + height > area.getBottom() && near.getY() - height - 2 >= area.getY()) y = near.getY() - height - 2;
        r = {near.getX(), y, std::max(width, near.getWidth()), height};
    }
    L.list = r.constrainedWithin(area);
}

void MenuOverlay::open(std::vector<MenuItem> items, const Options& o, Callback cb, juce::Component* owner) {
    if (open_ || items.empty() || getParentComponent() == nullptr) return;  // one menu per gesture
    root_ = Level{};
    sub_ = Level{};
    root_.items = std::move(items);
    opts_ = o;
    cb_ = std::move(cb);
    owner_ = owner;
    previousFocus_ = juce::Component::getCurrentlyFocusedComponent();
    setBounds(getParentComponent()->getLocalBounds());
    layoutLevel(root_, o.anchor, false);
    openPoint_ = o.atPoint ? o.point : o.anchor.getCentre();
    lastMouse_ = openPoint_;
    root_.hot = -1;
    for (int i = 0; i < static_cast<int>(root_.items.size()); ++i) {
        const auto& it = root_.items[static_cast<size_t>(i)];
        bool tick = it.ticked;
        for (const auto& c : it.sub) tick = tick || c.ticked;
        if (tick && selectableIn(root_, i)) root_.hot = i;
    }
    root_.open = true;
    if (root_.hot >= 0) ensureVisibleIn(root_, root_.hot);
    subParent_ = pendingParent_ = -1;
    keyInSub_ = false;
    pressed_ = pressedLevel_ = -1;
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
    stopTimer();
    setVisible(false);
    auto cb = std::move(cb_);
    cb_ = nullptr;
    root_ = Level{};
    sub_ = Level{};
    subParent_ = pendingParent_ = -1;
    owner_ = nullptr;
    pressed_ = pressedLevel_ = -1;
    if (auto* f = previousFocus_.getComponent(); f != nullptr && f->isShowing()) f->grabKeyboardFocus();
    previousFocus_ = nullptr;
    if (cb) cb(result);  // exactly once, after the overlay is closed (the callback may open another menu)
}

void MenuOverlay::openSub(int parent, bool focusFirst) {
    if (!isParent(root_, parent)) return;
    if (sub_.open && subParent_ == parent) {
        if (focusFirst) { keyInSub_ = true; sub_.hot = -1; moveHotIn(sub_, 1); }
        return;
    }
    sub_ = Level{};
    sub_.items = root_.items[static_cast<size_t>(parent)].sub;
    subParent_ = parent;
    layoutLevel(sub_, rowBoundsIn(root_, parent), true);
    sub_.open = true;
    sub_.hot = -1;
    for (int i = 0; i < static_cast<int>(sub_.items.size()); ++i) if (sub_.items[static_cast<size_t>(i)].ticked && selectableIn(sub_, i)) sub_.hot = i;
    if (sub_.hot >= 0) ensureVisibleIn(sub_, sub_.hot);
    keyInSub_ = focusFirst;
    if (focusFirst && sub_.hot < 0) moveHotIn(sub_, 1);
    pendingParent_ = -1;
    repaint();
}

void MenuOverlay::closeSub() {
    if (!sub_.open) return;
    sub_ = Level{};
    subParent_ = -1;
    keyInSub_ = false;
    repaint();
}

juce::Rectangle<int> MenuOverlay::rowBoundsIn(const Level& L, int i) const {
    const int row = i - L.scroll;
    return {L.list.getX(), L.list.getY() + 4 + row * opts_.rowHeight, L.list.getWidth(), opts_.rowHeight};
}

int MenuOverlay::itemAtIn(const Level& L, juce::Point<int> p, int rowH) {
    if (!L.open || !L.list.contains(p)) return -1;
    const int row = (p.y - L.list.getY() - 4) / std::max(1, rowH);
    if (row < 0 || row >= L.visibleRows) return -1;
    const int i = row + L.scroll;
    return i < static_cast<int>(L.items.size()) ? i : -1;
}

void MenuOverlay::ensureVisibleIn(Level& L, int i) {
    if (i < L.scroll) L.scroll = i;
    if (i >= L.scroll + L.visibleRows) L.scroll = i - L.visibleRows + 1;
    L.scroll = std::clamp(L.scroll, 0, std::max(0, static_cast<int>(L.items.size()) - L.visibleRows));
}

void MenuOverlay::moveHotIn(Level& L, int dir) {
    const int n = static_cast<int>(L.items.size());
    if (n == 0) return;
    int i = L.hot;
    for (int k = 0; k < n; ++k) {
        i = i < 0 ? (dir > 0 ? 0 : n - 1) : i + dir;
        if (i < 0 || i >= n) { i = L.hot; break; }
        if (selectableIn(L, i)) break;
    }
    if (selectableIn(L, i)) { L.hot = i; ensureVisibleIn(L, i); repaint(); }
}

bool MenuOverlay::aimingAtSub(juce::Point<int> p) const {
    // the pointer is inside the triangle (last position, submenu near top corner, submenu near bottom corner)
    if (!sub_.open) return false;
    const bool right = sub_.list.getX() >= root_.list.getCentreX();
    const float nx = static_cast<float>(right ? sub_.list.getX() : sub_.list.getRight());
    const juce::Point<float> a = lastMouse_.toFloat(), b{nx, static_cast<float>(sub_.list.getY() - 6)}, c{nx, static_cast<float>(sub_.list.getBottom() + 6)};
    const auto q = p.toFloat();
    auto sign = [](juce::Point<float> p1, juce::Point<float> p2, juce::Point<float> p3) { return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y); };
    const float d1 = sign(q, a, b), d2 = sign(q, b, c), d3 = sign(q, c, a);
    const bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0), pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
}

void MenuOverlay::timerCallback() {
    stopTimer();
    if (!open_) return;
    // hover intent settled: switch to whatever root row the pointer is on now
    const auto p = getMouseXYRelative();
    if (sub_.list.contains(p)) return;
    const int i = itemAtIn(root_, p, opts_.rowHeight);
    if (i >= 0 && selectableIn(root_, i)) {
        root_.hot = i;
        if (isParent(root_, i)) openSub(i, false);
        else closeSub();
    }
    repaint();
}

void MenuOverlay::paintLevel(juce::Graphics& g, const Level& L, bool isRoot) const {
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRect(L.list.toFloat().translated(0.0f, 3.0f).expanded(1.0f));
    g.setColour(Colours::panel);
    g.fillRect(L.list);
    g.setColour(Colours::border.brighter(0.2f));
    g.drawRect(L.list.toFloat(), Tokens::hairline);
    g.saveState();
    g.reduceClipRegion(L.list.reduced(1));
    for (int row = 0; row < L.visibleRows; ++row) {
        const int i = row + L.scroll;
        if (i >= static_cast<int>(L.items.size())) break;
        const auto& it = L.items[static_cast<size_t>(i)];
        auto r = rowBoundsIn(L, i).reduced(1, 0);
        if (it.header) {
            g.setColour(Colours::textFaint);
            g.setFont(LookAndFeel::font(10.5f, true).withExtraKerningFactor(0.12f));
            g.drawText(it.text.toUpperCase(), r.withTrimmedLeft(12).withTrimmedTop(6), juce::Justification::centredLeft);
            continue;
        }
        const bool parentOpen = isRoot && sub_.open && i == subParent_;
        const bool hot = i == L.hot || parentOpen;
        if (hot && it.enabled) {
            g.setColour(Colours::text.withAlpha(parentOpen && i != L.hot ? 0.05f : 0.07f));
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
            g.drawText(it.hint, r.withTrimmedRight(it.sub.empty() ? 26 : 30), juce::Justification::centredRight);
        }
        if (!it.sub.empty()) {  // submenu chevron
            juce::Path chev;
            const float cx = static_cast<float>(r.getRight()) - 13.0f, cy = static_cast<float>(r.getCentreY());
            chev.startNewSubPath(cx - 2.0f, cy - 4.0f);
            chev.lineTo(cx + 2.0f, cy);
            chev.lineTo(cx - 2.0f, cy + 4.0f);
            g.setColour(hot ? Colours::text : Colours::textDim);
            g.strokePath(chev, juce::PathStrokeType(1.2f));
        } else if (it.ticked) {
            g.setColour(c);
            g.fillRect(juce::Rectangle<float>(4.0f, 4.0f).withCentre({static_cast<float>(r.getRight()) - 13.0f, static_cast<float>(r.getCentreY())}));
        }
    }
    g.restoreState();
    if (L.scroll > 0) { g.setColour(Colours::textDim); g.fillRect(L.list.getCentreX() - 8, L.list.getY() + 2, 16, 1); }
    if (L.scroll + L.visibleRows < static_cast<int>(L.items.size())) { g.setColour(Colours::textDim); g.fillRect(L.list.getCentreX() - 8, L.list.getBottom() - 3, 16, 1); }
}

void MenuOverlay::paint(juce::Graphics& g) {
    if (!open_) return;
    paintLevel(g, root_, true);
    if (sub_.open) paintLevel(g, sub_, false);
}

void MenuOverlay::mouseMove(const juce::MouseEvent& e) {
    const auto p = e.getPosition();
    if (sub_.open && sub_.list.contains(p)) {
        const int i = itemAtIn(sub_, p, opts_.rowHeight);
        if (selectableIn(sub_, i) && i != sub_.hot) { sub_.hot = i; repaint(); }
        keyInSub_ = true;
        stopTimer();
        lastMouse_ = p;
        return;
    }
    const int i = itemAtIn(root_, p, opts_.rowHeight);
    if (i >= 0 && selectableIn(root_, i)) {
        const bool switching = sub_.open ? i != subParent_ : isParent(root_, i);
        if (switching && sub_.open && aimingAtSub(p)) {
            // moving towards the open submenu: give it time, switch only if the pointer rests here
            startTimer(260);
        } else if (switching) {
            root_.hot = i;
            if (isParent(root_, i)) openSub(i, false);
            else closeSub();
            keyInSub_ = false;
        } else if (i != root_.hot) {
            root_.hot = i;
            keyInSub_ = false;
        }
        repaint();
    }
    lastMouse_ = p;
}

void MenuOverlay::activate(Level& L, int i) {
    if (!selectableIn(L, i)) return;
    if (&L == &root_ && isParent(root_, i)) { root_.hot = i; openSub(i, true); return; }
    close(L.items[static_cast<size_t>(i)].id);
}

void MenuOverlay::mouseDown(const juce::MouseEvent& e) {
    if (!open_) return;
    const auto p = e.getPosition();
    if (sub_.open && sub_.list.contains(p)) {
        const int i = itemAtIn(sub_, p, opts_.rowHeight);
        pressedLevel_ = 1;
        pressed_ = selectableIn(sub_, i) ? i : -1;
        if (pressed_ >= 0) { sub_.hot = pressed_; repaint(); }
        return;
    }
    if (!root_.list.contains(p)) { close(0); return; }  // outside: cancel, consume (no click-through)
    const int i = itemAtIn(root_, p, opts_.rowHeight);
    pressedLevel_ = 0;
    pressed_ = selectableIn(root_, i) ? i : -1;
    if (pressed_ >= 0) {
        root_.hot = pressed_;
        if (isParent(root_, pressed_)) openSub(pressed_, false);  // a click on a category opens it at once
        repaint();
    }
}

void MenuOverlay::mouseDrag(const juce::MouseEvent& e) {
    const auto p = e.getPosition();
    if (sub_.open && sub_.list.contains(p)) {
        const int i = itemAtIn(sub_, p, opts_.rowHeight);
        if (selectableIn(sub_, i) && i != sub_.hot) { sub_.hot = i; repaint(); }
        return;
    }
    const int i = itemAtIn(root_, p, opts_.rowHeight);
    if (selectableIn(root_, i) && i != root_.hot) {
        root_.hot = i;
        if (isParent(root_, i)) openSub(i, false);
        repaint();
    }
}

void MenuOverlay::mouseUp(const juce::MouseEvent& e) {
    if (!open_) return;
    const auto p = e.getPosition();
    const int pl = pressedLevel_, pi = pressed_;
    pressed_ = pressedLevel_ = -1;
    if (sub_.open && sub_.list.contains(p)) {
        const int i = itemAtIn(sub_, p, opts_.rowHeight);
        // a press on a category followed by a release on one of its items also selects (press-drag-release)
        if (selectableIn(sub_, i) && ((pl == 1 && pi == i) || (pl == 0 && pi == subParent_))) close(sub_.items[static_cast<size_t>(i)].id);
        return;
    }
    const int i = itemAtIn(root_, p, opts_.rowHeight);
    if (pl == 0 && pi >= 0 && i == pi && selectableIn(root_, i) && !isParent(root_, i)) close(root_.items[static_cast<size_t>(i)].id);
}

void MenuOverlay::ownerMouseUp(const juce::MouseEvent& eInOwner) {
    if (!open_) return;
    const auto pos = getLocalPoint(eInOwner.eventComponent, eInOwner.getPosition());
    // press on the control, drag onto an item, release: selects. A plain click (no travel) just leaves the
    // menu open for the next click.
    if (pos.getDistanceFrom(openPoint_) <= 6 || juce::Time::getMillisecondCounterHiRes() - openedMs_ <= 120.0) return;
    if (sub_.open && sub_.list.contains(pos)) {
        const int i = itemAtIn(sub_, pos, opts_.rowHeight);
        if (selectableIn(sub_, i)) close(sub_.items[static_cast<size_t>(i)].id);
        return;
    }
    const int i = itemAtIn(root_, pos, opts_.rowHeight);
    if (selectableIn(root_, i) && !isParent(root_, i)) close(root_.items[static_cast<size_t>(i)].id);
}

void MenuOverlay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    if (!open_) return;  // consumed either way: the carousel beneath never scrolls while a menu is open
    Level& L = (sub_.open && sub_.list.contains(e.getPosition())) ? sub_ : root_;
    wheelAcc_ += w.deltaY * (w.isReversed ? -1.0f : 1.0f);
    const int steps = static_cast<int>(wheelAcc_ / 0.12f);
    if (steps != 0) {
        wheelAcc_ -= static_cast<float>(steps) * 0.12f;
        L.scroll = std::clamp(L.scroll - steps, 0, std::max(0, static_cast<int>(L.items.size()) - L.visibleRows));
        repaint();
    }
}

bool MenuOverlay::keyPressed(const juce::KeyPress& k) {
    if (!open_) return false;
    Level& L = (keyInSub_ && sub_.open) ? sub_ : root_;
    if (k == juce::KeyPress::escapeKey) {
        if (&L == &sub_) closeSub();
        else close(0);
        return true;
    }
    if (k == juce::KeyPress::leftKey) { if (sub_.open) closeSub(); return true; }
    if (k == juce::KeyPress::rightKey) { if (&L == &root_ && isParent(root_, root_.hot)) openSub(root_.hot, true); return true; }
    if (k == juce::KeyPress::upKey) { moveHotIn(L, -1); return true; }
    if (k == juce::KeyPress::downKey) { moveHotIn(L, 1); return true; }
    if (k == juce::KeyPress::pageUpKey) { for (int i = 0; i < L.visibleRows - 1; ++i) moveHotIn(L, -1); return true; }
    if (k == juce::KeyPress::pageDownKey) { for (int i = 0; i < L.visibleRows - 1; ++i) moveHotIn(L, 1); return true; }
    if (k == juce::KeyPress::homeKey) { L.hot = -1; moveHotIn(L, 1); return true; }
    if (k == juce::KeyPress::endKey) { L.hot = -1; moveHotIn(L, -1); return true; }
    if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey) {
        activate(L, L.hot);
        return true;
    }
    const auto ch = k.getTextCharacter();
    if (juce::CharacterFunctions::isLetterOrDigit(ch)) {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now - typedMs_ > 900.0) typed_.clear();
        typedMs_ = now;
        typed_ += juce::String::charToString(ch);
        for (int i = 0; i < static_cast<int>(L.items.size()); ++i)
            if (selectableIn(L, i) && L.items[static_cast<size_t>(i)].text.startsWithIgnoreCase(typed_)) { L.hot = i; ensureVisibleIn(L, i); repaint(); break; }
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
