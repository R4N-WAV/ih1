#include "PresetBrowser.h"

#include "Ih1LookAndFeel.h"
#include "PluginProcessor.h"
#include "ih1/presets/Presets.h"

namespace ih1::plugin {

namespace {
juce::Path chevron(bool left) {
    juce::Path p;
    if (left) { p.startNewSubPath(6.0f, 0.0f); p.lineTo(0.0f, 6.0f); p.lineTo(6.0f, 12.0f); }
    else { p.startNewSubPath(0.0f, 0.0f); p.lineTo(6.0f, 6.0f); p.lineTo(0.0f, 12.0f); }
    return p;
}
void styleQuiet(juce::TextButton& b) {
    b.getProperties().set("quiet", true);
    b.getProperties().set("fontSize", 12.0);
    b.setWantsKeyboardFocus(false);
}
}  // namespace

// =============================================================================================== bar
PresetBar::PresetBar(Processor& p) : proc_(p) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setRepaintsOnMouseActivity(true);
    refresh();
}

juce::String PresetBar::getTooltip() {
    const auto m = getMouseXYRelative();
    if (prevArea().contains(m)) return "Previous preset";
    if (nextArea().contains(m)) return "Next preset";
    return "Presets: click the name to browse by category; the arrows step through every preset in the browser's order. Presets hold the committed chain.";
}

void PresetBar::step(int dir) {
    juce::String msg;
    ++steps_;
    if (!proc_.stepPreset(dir, msg) && onMessage) onMessage(msg);
    refresh();
}

void PresetBar::refresh() {
    const auto n = proc_.currentPresetName();
    const bool m = proc_.presetModified();
    if (n != name_ || m != modified_) { name_ = n; modified_ = m; repaint(); }
}

juce::Rectangle<int> PresetBar::nameArea() const { return getLocalBounds().reduced(kArrowW, 0); }

void PresetBar::paint(juce::Graphics& g) {
    const auto mouse = getMouseXYRelative();
    const bool over = isMouseOver(false);
    // chevrons, centred in their (generous) hit zones
    for (int side : {-1, 1}) {
        const auto zone = (side < 0 ? prevArea() : nextArea()).toFloat();
        const bool hot = over && zone.contains(mouse.toFloat());
        const bool down = pressedSide_ == side;
        juce::Path p;
        const float cx = zone.getCentreX(), cy = zone.getCentreY();
        if (side < 0) { p.startNewSubPath(cx + 3.0f, cy - 6.0f); p.lineTo(cx - 3.0f, cy); p.lineTo(cx + 3.0f, cy + 6.0f); }
        else { p.startNewSubPath(cx - 3.0f, cy - 6.0f); p.lineTo(cx + 3.0f, cy); p.lineTo(cx - 3.0f, cy + 6.0f); }
        g.setColour(down ? Colours::accent : (hot ? Colours::text : Colours::textDim));
        g.strokePath(p, juce::PathStrokeType(1.4f));
    }
    auto r = nameArea().toFloat();
    const bool hot = over && r.contains(mouse.toFloat());
    g.setColour(Colours::border.brighter(hot ? 0.6f : 0.2f));
    g.fillRect(r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);
    const bool none = name_.isEmpty();
    g.setColour(none ? Colours::textDim : Colours::text);
    g.setFont(LookAndFeel::font(14.0f, !none));
    auto text = r.withTrimmedRight(modified_ ? 12.0f : 0.0f);
    g.drawFittedText(none ? juce::String("No preset") : name_, text.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    if (modified_) {  // subtle "edited since loaded" marker
        g.setColour(Colours::accent2);
        g.fillRect(juce::Rectangle<float>(4.0f, 4.0f).withCentre({r.getRight() - 8.0f, r.getCentreY()}));
    }
}

void PresetBar::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) return;
    const auto p = e.getPosition();
    if (prevArea().contains(p)) { pressedSide_ = -1; step(-1); return; }
    if (nextArea().contains(p)) { pressedSide_ = 1; step(1); return; }
    if (nameArea().contains(p) && onOpenBrowser) onOpenBrowser();
}

// =============================================================================================== browser
PresetBrowser::PresetBrowser(Processor& p) : proc_(p) {
    setVisible(false);
    setWantsKeyboardFocus(true);
    setAlwaysOnTop(true);
    for (auto* t : {&search_, &nameEdit_}) {
        t->setFont(LookAndFeel::font(13.5f));
        t->setIndents(8, 5);
        t->setSelectAllWhenFocused(true);
        addChildComponent(*t);
    }
    search_.setTextToShowWhenEmpty("Search presets", Colours::textFaint);
    search_.onTextChange = [this] { filter(); repaint(); };
    search_.onEscapeKey = [this] { close(); };
    search_.onReturnKey = [this] { if (!shown_.empty()) load(std::max(0, hot_)); };
    nameEdit_.setTextToShowWhenEmpty("Preset name", Colours::textFaint);
    nameEdit_.onReturnKey = [this] { commitName(); };
    nameEdit_.onEscapeKey = [this] { setMode(Mode::Browse); };
    nameEdit_.setInputRestrictions(60);
    for (auto* b : {&save_, &saveAs_, &rename_, &del_, &ok_, &cancel_}) { styleQuiet(*b); addChildComponent(*b); }
    del_.setColour(juce::TextButton::textColourOffId, Colours::danger);
    save_.setTooltip("Save the current chain over the selected user preset (asks first).");
    saveAs_.setTooltip("Save the current committed chain as a new user preset.");
    rename_.setTooltip("Rename the selected user preset.");
    del_.setTooltip("Delete the selected user preset (asks first). Factory presets cannot be deleted.");
    save_.onClick = [this] { if (const auto* e = current(); e && !e->factory) { pendingName_ = e->name; setMode(Mode::ConfirmOverwrite); } };
    saveAs_.onClick = [this] { setMode(Mode::SaveAs); };
    rename_.onClick = [this] { if (const auto* e = current(); e && !e->factory) setMode(Mode::Rename); };
    del_.onClick = [this] { if (const auto* e = current(); e && !e->factory) setMode(Mode::ConfirmDelete); };
    cancel_.onClick = [this] { setMode(Mode::Browse); };
    ok_.onClick = [this] {
        juce::String msg;
        const auto* e = current();
        if (mode_ == Mode::ConfirmDelete && e) {
            const bool ok = proc_.deleteUserPreset(e->key, msg);
            if (onMessage) onMessage(ok ? "Preset deleted" : msg);
        } else if (mode_ == Mode::ConfirmOverwrite) {
            bool exists = false;
            const bool ok = proc_.saveUserPreset(pendingName_, true, msg, exists);
            if (onMessage) onMessage(ok ? "Preset saved" : msg);
        } else {
            commitName();
            return;
        }
        setMode(Mode::Browse);
        reload();
    };
}

const PresetBrowser::Entry* PresetBrowser::current() const {
    const auto key = proc_.currentPresetKey();
    if (hot_ >= 0 && hot_ < static_cast<int>(shown_.size())) return &all_[static_cast<size_t>(shown_[static_cast<size_t>(hot_)])];
    for (const auto& e : all_) if (e.key == key) return &e;
    return nullptr;
}

void PresetBrowser::reload() {
    all_.clear();
    for (const auto& e : proc_.presetList()) all_.push_back({e.key, e.name, e.category, e.description, e.factory});
    categories_.clear();
    for (const auto& c : presetCategories()) categories_.add(c);
    filter();
}

void PresetBrowser::filter() {
    shown_.clear();
    const auto q = search_.getText().trim();
    for (size_t i = 0; i < all_.size(); ++i) {
        const auto& e = all_[i];
        if (q.isNotEmpty()) {
            if (e.name.containsIgnoreCase(q) || e.category.containsIgnoreCase(q) || e.description.containsIgnoreCase(q)) shown_.push_back(static_cast<int>(i));
        } else if (category_ >= 0 && category_ < categories_.size() && e.category == categories_[category_]) {
            shown_.push_back(static_cast<int>(i));
        }
    }
    scroll_ = 0;
    hot_ = -1;
    const auto key = proc_.currentPresetKey();
    for (size_t i = 0; i < shown_.size(); ++i) if (all_[static_cast<size_t>(shown_[i])].key == key) hot_ = static_cast<int>(i);
    resized();
}

void PresetBrowser::open(juce::Rectangle<int> below) {
    if (getParentComponent() == nullptr) return;
    setBounds(getParentComponent()->getLocalBounds());
    const int w = std::min(600, getWidth() - 24), h = std::min(440, getHeight() - below.getBottom() - 16);
    panel_ = juce::Rectangle<int>(below.getX(), below.getBottom() + 6, w, std::max(260, h)).constrainedWithin(getLocalBounds().reduced(8));
    // start on the category of the current preset
    reload();
    const auto key = proc_.currentPresetKey();
    for (const auto& e : all_) if (e.key == key) category_ = std::max(0, categories_.indexOf(e.category));
    search_.clear();
    setMode(Mode::Browse);
    filter();
    setVisible(true);
    toFront(true);
    search_.setVisible(true);
    search_.grabKeyboardFocus();
}

void PresetBrowser::close() {
    setMode(Mode::Browse);
    setVisible(false);
}

void PresetBrowser::setMode(Mode m) {
    mode_ = m;
    const bool browse = m == Mode::Browse;
    const auto* e = current();
    const bool user = e && !e->factory;
    save_.setVisible(browse);
    saveAs_.setVisible(browse);
    rename_.setVisible(browse);
    del_.setVisible(browse);
    save_.setEnabled(user);
    rename_.setEnabled(user);
    del_.setEnabled(user);
    nameEdit_.setVisible(m == Mode::SaveAs || m == Mode::Rename);
    ok_.setVisible(!browse);
    cancel_.setVisible(!browse);
    ok_.setButtonText(m == Mode::ConfirmDelete ? "Delete" : (m == Mode::ConfirmOverwrite ? "Overwrite" : "Save"));
    if (m == Mode::Rename) ok_.setButtonText("Rename");
    ok_.setColour(juce::TextButton::textColourOffId, m == Mode::ConfirmDelete ? Colours::danger : Colours::text);
    if (m == Mode::SaveAs) { nameEdit_.setText(proc_.currentPresetName().isNotEmpty() ? proc_.currentPresetName() : "My Chain", false); nameEdit_.grabKeyboardFocus(); }
    if (m == Mode::Rename && e) { nameEdit_.setText(e->name, false); nameEdit_.grabKeyboardFocus(); }
    resized();
    repaint();
}

void PresetBrowser::commitName() {
    const auto name = nameEdit_.getText().trim();
    if (name.isEmpty()) return;
    juce::String msg;
    if (mode_ == Mode::SaveAs) {
        bool exists = false;
        if (proc_.saveUserPreset(name, false, msg, exists)) { if (onMessage) onMessage("Preset saved"); }
        else if (exists) { pendingName_ = name; setMode(Mode::ConfirmOverwrite); return; }  // never overwrite silently
        else if (onMessage) onMessage(msg);
    } else if (mode_ == Mode::Rename) {
        if (const auto* e = current(); e && !e->factory) {
            if (!proc_.renameUserPreset(e->key, name, msg) && onMessage) onMessage(msg);
        }
    }
    setMode(Mode::Browse);
    category_ = std::max(0, categories_.indexOf("User"));
    reload();
}

void PresetBrowser::resized() {
    if (panel_.isEmpty()) return;
    auto r = panel_.reduced(14, 12);
    search_.setBounds(r.removeFromTop(28));
    r.removeFromTop(10);
    footer_ = r.removeFromBottom(30);
    descArea_ = r.removeFromBottom(34);
    catArea_ = r.removeFromLeft(158);
    r.removeFromLeft(12);
    listArea_ = r;
    auto f = footer_;
    if (mode_ == Mode::Browse) {
        for (auto* b : {&saveAs_, &save_, &rename_, &del_}) { b->setBounds(f.removeFromLeft(74)); f.removeFromLeft(4); }
    } else {
        cancel_.setBounds(f.removeFromRight(76));
        f.removeFromRight(4);
        ok_.setBounds(f.removeFromRight(86));
        f.removeFromRight(10);
        nameEdit_.setBounds(f.withSizeKeepingCentre(f.getWidth(), 26));
    }
}

int PresetBrowser::rowAt(juce::Point<int> p) const {
    if (!listArea_.contains(p)) return -1;
    const int i = (p.y - listArea_.getY()) / kRowH + scroll_;
    return i >= 0 && i < static_cast<int>(shown_.size()) ? i : -1;
}

int PresetBrowser::categoryAt(juce::Point<int> p) const {
    if (!catArea_.contains(p)) return -1;
    const int i = (p.y - catArea_.getY()) / kRowH;
    return i >= 0 && i < categories_.size() ? i : -1;
}

void PresetBrowser::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.35f));
    g.setColour(Colours::panel);
    g.fillRect(panel_);
    g.setColour(Colours::border);
    g.drawRect(panel_.toFloat(), 1.0f);
    const bool searching = search_.getText().trim().isNotEmpty();
    // categories
    for (int i = 0; i < categories_.size(); ++i) {
        auto row = juce::Rectangle<int>(catArea_.getX(), catArea_.getY() + i * kRowH, catArea_.getWidth(), kRowH);
        const bool sel = !searching && i == category_;
        if (sel) { g.setColour(Colours::accent); g.fillRect(row.getX(), row.getY() + 6, 2, kRowH - 12); }
        g.setColour(sel ? Colours::text : (i == hotCat_ ? Colours::text.withAlpha(0.85f) : Colours::textDim));
        g.setFont(LookAndFeel::font(13.5f, sel));
        g.drawText(categories_[i], row.withTrimmedLeft(12), juce::Justification::centredLeft);
    }
    // presets
    const auto key = proc_.currentPresetKey();
    const int visible = listArea_.getHeight() / kRowH;
    if (shown_.empty()) {
        g.setColour(Colours::textFaint);
        g.setFont(LookAndFeel::font(13.0f));
        g.drawText(searching ? "No preset matches" : (category_ >= 0 && categories_[category_] == "User" ? "No user presets yet: Save As to add one" : "Empty"),
                   listArea_.withHeight(kRowH), juce::Justification::centredLeft);
    }
    for (int row = 0; row < visible; ++row) {
        const int i = row + scroll_;
        if (i >= static_cast<int>(shown_.size())) break;
        const auto& e = all_[static_cast<size_t>(shown_[static_cast<size_t>(i)])];
        auto r = juce::Rectangle<int>(listArea_.getX(), listArea_.getY() + row * kRowH, listArea_.getWidth(), kRowH);
        const bool cur = e.key == key;
        if (i == hot_) { g.setColour(Colours::text.withAlpha(0.06f)); g.fillRect(r); }
        if (cur) { g.setColour(Colours::accent); g.fillRect(r.getX(), r.getY() + 6, 2, kRowH - 12); }
        g.setColour(cur ? Colours::accent : Colours::text.withAlpha(i == hot_ ? 1.0f : 0.85f));
        g.setFont(LookAndFeel::font(13.5f, cur));
        g.drawText(e.name, r.withTrimmedLeft(12).withTrimmedRight(90), juce::Justification::centredLeft);
        if (searching) {
            g.setColour(Colours::textFaint);
            g.setFont(LookAndFeel::font(11.0f));
            g.drawText(e.category, r.withTrimmedRight(8), juce::Justification::centredRight);
        }
    }
    // description of the hovered / current preset
    const Entry* d = nullptr;
    if (hot_ >= 0 && hot_ < static_cast<int>(shown_.size())) d = &all_[static_cast<size_t>(shown_[static_cast<size_t>(hot_)])];
    if (d) {
        g.setColour(Colours::textDim);
        g.setFont(LookAndFeel::font(12.5f));
        g.drawFittedText(d->description.isNotEmpty() ? d->description : (d->factory ? juce::String() : juce::String("User preset")), descArea_, juce::Justification::centredLeft, 2);
    }
    if (mode_ == Mode::ConfirmDelete || mode_ == Mode::ConfirmOverwrite) {
        g.setColour(Colours::text);
        g.setFont(LookAndFeel::font(13.0f));
        const auto* e = current();
        g.drawText(mode_ == Mode::ConfirmDelete ? "Delete \"" + (e ? e->name : juce::String()) + "\"?" : "Replace the user preset \"" + pendingName_ + "\"?",
                   footer_.withTrimmedRight(180), juce::Justification::centredLeft);
    }
}

void PresetBrowser::load(int i) {
    if (i < 0 || i >= static_cast<int>(shown_.size())) return;
    const auto& e = all_[static_cast<size_t>(shown_[static_cast<size_t>(i)])];
    juce::String msg;
    const bool ok = proc_.loadPresetKey(e.key, msg);
    if (onMessage) onMessage(ok ? (msg.isEmpty() ? juce::String() : "Loaded with warnings: " + msg) : "Could not load: " + msg);
    hot_ = i;
    setMode(Mode::Browse);
    repaint();
}

void PresetBrowser::mouseDown(const juce::MouseEvent& e) {
    const auto p = e.getPosition();
    if (!panel_.contains(p)) { close(); return; }  // outside: close, consume (no click-through)
    if (mode_ != Mode::Browse) return;
    if (const int c = categoryAt(p); c >= 0) {
        category_ = c;
        search_.clear();
        filter();
        repaint();
        return;
    }
    pressedRow_ = rowAt(p);
}

void PresetBrowser::mouseUp(const juce::MouseEvent& e) {
    const int r = rowAt(e.getPosition());
    if (r >= 0 && r == pressedRow_) load(r);  // a preset loads straight away (the browser stays open to audition)
    pressedRow_ = -1;
}

void PresetBrowser::mouseDoubleClick(const juce::MouseEvent& e) {
    if (rowAt(e.getPosition()) >= 0) close();  // double-click: load (done on the first click) and close
}

void PresetBrowser::mouseMove(const juce::MouseEvent& e) {
    const int r = rowAt(e.getPosition()), c = categoryAt(e.getPosition());
    if ((r >= 0 && r != hot_) || c != hotCat_) {
        if (r >= 0 && mode_ == Mode::Browse) hot_ = r;
        hotCat_ = c;
        repaint();
    }
}

void PresetBrowser::mouseExit(const juce::MouseEvent&) { hotCat_ = -1; repaint(); }

void PresetBrowser::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
    const int visible = std::max(1, listArea_.getHeight() / kRowH);
    const int step = w.deltaY > 0 ? -1 : (w.deltaY < 0 ? 1 : 0);
    scroll_ = std::clamp(scroll_ + step, 0, std::max(0, static_cast<int>(shown_.size()) - visible));
    repaint();  // consumed: nothing beneath scrolls
}

bool PresetBrowser::keyPressed(const juce::KeyPress& k) {
    if (k == juce::KeyPress::escapeKey) { if (mode_ != Mode::Browse) setMode(Mode::Browse); else close(); return true; }
    const int visible = std::max(1, listArea_.getHeight() / kRowH);
    if (k == juce::KeyPress::downKey || k == juce::KeyPress::upKey) {
        if (shown_.empty()) return true;
        hot_ = std::clamp(hot_ + (k == juce::KeyPress::downKey ? 1 : -1), 0, static_cast<int>(shown_.size()) - 1);
        if (hot_ < scroll_) scroll_ = hot_;
        if (hot_ >= scroll_ + visible) scroll_ = hot_ - visible + 1;
        repaint();
        return true;
    }
    if (k == juce::KeyPress::returnKey) { load(hot_); return true; }
    if (k == juce::KeyPress::leftKey || k == juce::KeyPress::rightKey) {
        category_ = std::clamp(category_ + (k == juce::KeyPress::rightKey ? 1 : -1), 0, categories_.size() - 1);
        search_.clear();
        filter();
        repaint();
        return true;
    }
    return true;  // keyboard stays in the browser while open
}

}  // namespace ih1::plugin
