// ih1_uitest: drives the real IH1 editor with synthetic mouse / wheel / key events delivered through its
// native peer (the same entry point the OS uses), and checks the model after every action, not only what the
// screen shows. Covers V1 feedback 4.1, 4.6, 4.11, 4.13, 4.15 and QoL 5.1/5.2. Needs a display (Xvfb on Linux).
// Usage: ih1_uitest            (exit code = number of failures)
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include <cstdio>
#include <functional>
#include <random>

#include "../../plugin/Source/PluginEditor.h"
#include "../../plugin/Source/PluginProcessor.h"
#include "ih1/presets/Presets.h"

using namespace juce;
using namespace ih1;
using namespace ih1::plugin;

namespace {
int failures = 0;
void check(bool ok, const String& what) {
    std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8());
    if (!ok) ++failures;
}
void pump(int ms = 80) { MessageManager::getInstance()->runDispatchLoopUntil(ms); }
// V6: wait for a condition the editor reaches on its own timer (up to timeoutMs) instead of a fixed pump, so a loaded
// machine (CI, sanitizers) cannot make a correct editor miss a fixed window. Measured before: the V5 test missed one
// such window in 2 of 6 idle runs.
bool waitFor(const std::function<bool()>& cond, int timeoutMs = 1500) {
    for (int t = 0; t < timeoutMs; t += 20) {
        if (cond()) return true;
        pump(20);
    }
    return cond();
}

struct Driver {
    Editor& ed;
    int64 t = Time::currentTimeMillis();
    explicit Driver(Editor& e) : ed(e) {}
    ComponentPeer* peer() { return ed.getPeer(); }
    Point<float> toPeer(Component& c, Point<int> p) { return ed.getLocalPoint(&c, p).toFloat(); }
    // JUCE asks the OS for the real cursor position when deciding whether the mouse is over a button (Windows), so
    // the real cursor follows every synthetic event; otherwise wherever the runner's cursor happens to sit decides
    // whether a click lands
    // (only there: on X11 the warp's own motion events would interrupt synthetic drags). The warp is pumped before
    // the synthetic event so the OS's own move (buttons up) never lands inside a press / drag.
    void warp([[maybe_unused]] Point<float> p) {
#if JUCE_WINDOWS
        Desktop::setMousePosition(ed.localPointToGlobal(p).roundToInt());
        pump(15);
#endif
    }
    void move(Point<float> p) { warp(p); t += 30; peer()->handleMouseEvent(MouseInputSource::InputSourceType::mouse, p, ModifierKeys(), 0.0f, 0.0f, t); }
    void press(Point<float> p, ModifierKeys::Flags button = ModifierKeys::leftButtonModifier) {
        move(p);
        t += 30;
        peer()->handleMouseEvent(MouseInputSource::InputSourceType::mouse, p, ModifierKeys(button), 0.0f, 0.0f, t);
    }
    void release(Point<float> p) { t += 60; peer()->handleMouseEvent(MouseInputSource::InputSourceType::mouse, p, ModifierKeys(), 0.0f, 0.0f, t); }
    // a click: press + release, spaced in time so successive clicks are not double-clicks
    void click(Point<float> p, ModifierKeys::Flags button = ModifierKeys::leftButtonModifier, int settleMs = 60) {
        t += 600;
        press(p, button);
        release(p);
        pump(settleMs);
    }
    void clickOn(Component& c, Point<int> local, ModifierKeys::Flags b = ModifierKeys::leftButtonModifier) { click(toPeer(c, local), b); }
    void dragTo(Point<float> from, Point<float> to, std::function<void()> beforeRelease = {}) {
        t += 600;
        press(from);
        for (int k = 1; k <= 10; ++k) {
            t += 20;
            const auto p = from + (to - from) * (static_cast<float>(k) / 10.0f);
            peer()->handleMouseEvent(MouseInputSource::InputSourceType::mouse, p, ModifierKeys(ModifierKeys::leftButtonModifier), 0.0f, 0.0f, t);
            pump(5);
        }
        if (beforeRelease) beforeRelease();
        release(to);
        pump(60);
    }
    void wheel(Point<float> p, float dy) {
        move(p);
        MouseWheelDetails w;
        w.deltaX = 0.0f;
        w.deltaY = dy;
        w.isReversed = false;
        w.isSmooth = false;
        w.isInertial = false;
        t += 100;
        peer()->handleMouseWheel(MouseInputSource::InputSourceType::mouse, p, t, w);
        pump(20);
    }
    void key(const KeyPress& k) { peer()->handleKeyPress(k); pump(40); }
};

int selected(Processor& p) { const ScopedLock sl(p.sessionLock()); return p.session().selected(); }
size_t chainSize(Processor& p) { const ScopedLock sl(p.sessionLock()); return p.session().chain().size(); }
float draft(Processor& p, int type, int idx) { const ScopedLock sl(p.sessionLock()); return p.session().draft(type)[static_cast<size_t>(idx)]; }
}  // namespace

int main() {
    ScopedJuceInitialiser_GUI init;
    auto proc = std::make_unique<Processor>();
    proc->prepareToPlay(48000.0, 512);
    auto* edRaw = dynamic_cast<Editor*>(proc->createEditor());
    std::unique_ptr<Editor> ed(edRaw);
    ed->addToDesktop(ComponentPeer::windowHasTitleBar);
    ed->setTopLeftPosition(10, 10);
    ed->setVisible(true);
    pump(600);
    Driver d(*ed);
    auto& menu = ed->menu();
    auto& car = ed->carousel();

    // ---- 4.9 / 4.11 startup state ----
    {
        Rectangle<int> user;
        if (const auto* disp = Desktop::getInstance().getDisplays().getPrimaryDisplay()) user = disp->userBounds.toNearestInt();
        const auto want = Editor::defaultSize(user);
        // as a top-level test window the OS may trim it to the screen when the screen is smaller than the
        // editor's minimum (CI runners: 1024 x 768); inside a host the editor is a child window and is not trimmed
        const bool trimmedByOs = want.y > user.getHeight() * 0.92 && ed->getHeight() < want.y && ed->getHeight() >= user.getHeight() / 2;
        check(std::abs(ed->getWidth() - want.x) <= 2 && (std::abs(ed->getHeight() - want.y) <= 2 || trimmedByOs),
              "opens at the default size for this display: " + String(ed->getWidth()) + " x " + String(ed->getHeight()) + " (expected " + String(want.x) + " x " + String(want.y) + ")");
    }
    check(selected(*proc) == -1 && car.selected() == -1, "fresh instance: nothing staged (carousel reads None)");
    check(!ed->settings().isVisible(), "no settings panel while nothing is staged");

    // ---- 4.13 right-click list beside the cursor ----
    const int reverb = fxTypeIndex("reverb"), delay = fxTypeIndex("delay");
    const Point<int> rc(120, car.getHeight() / 2);
    d.clickOn(car, rc, ModifierKeys::rightButtonModifier);
    check(menu.isOpen(), "right-click on the carousel opens the FX list");
    const auto at = ed->getLocalPoint(&car, rc);
    check(menu.listBounds().getX() >= at.x && menu.listBounds().getX() <= at.x + 10 && std::abs(menu.listBounds().getY() - at.y) <= 10,
          "the list opens beside the pointer");
    {
        // V2.1 Phase 4: categories, each with a submenu; together they hold the whole catalogue
        int total = 0, cats = 0;
        for (int c = 0; c < kNumCategories; ++c) {
            int n = 0;
            for (int idx : fxCarouselOrder()) n += static_cast<int>(fxRegistry()[static_cast<size_t>(idx)].category) == c;
            total += n;
            cats += n > 0;
        }
        // V3 Phase 2: "None" is the first row, above the categories
        check(menu.itemCount() == cats + 1 && total == static_cast<int>(fxCarouselOrder().size()),
              "the list shows None + " + String(menu.itemCount() - 1) + " categories holding all " + String(total) + " effects");
        check(menu.indexOfText("None") == 0, "None is the first row of the right-click list");
    }
    check(selected(*proc) == -1, "opening the list stages nothing");
    d.key(KeyPress(KeyPress::escapeKey));
    check(!menu.isOpen() && selected(*proc) == -1, "Escape closes the list without staging");
    // pick Reverb: hover its category, the submenu opens beside it; click Reverb
    d.clickOn(car, rc, ModifierKeys::rightButtonModifier);
    {
        const int cat = menu.indexOfText("Time & Space");
        d.move(menu.rowBounds(cat).getCentre().toFloat());
        pump(60);
        check(menu.subOpen() && menu.subParent() == cat, "hovering a category opens its submenu");
        check(menu.subListBounds().getX() >= menu.listBounds().getRight() - 4 || menu.subListBounds().getRight() <= menu.listBounds().getX() + 4,
              "the submenu sits beside its parent list");
        const int i = menu.subIndexOfText("Reverb");
        const auto rb = menu.subRowBounds(i).getCentre().toFloat();
        // move diagonally from the category to the item, crossing other category rows: the submenu must stay
        const auto from = menu.rowBounds(cat).getCentre().toFloat();
        bool stayed = true;
        for (int k = 1; k <= 12; ++k) {
            d.move(from + (rb - from) * (static_cast<float>(k) / 12.0f));
            pump(8);
            stayed = stayed && menu.subOpen() && menu.subParent() == cat;
        }
        check(stayed, "moving diagonally into the submenu never dismisses or switches it");
        d.click(rb);
    }
    pump(150);
    check(selected(*proc) == reverb, "choosing Reverb stages Reverb");
    check(chainSize(*proc) == 0, "...and never commits");
    check(ed->settings().isVisible(), "the settings panel appears once an effect is staged");
    // V3 Phase 2: choosing None from the right-click list un-stages through the same path as the wheel's None
    {
        proc->uiSetParam(fxRegistry()[static_cast<size_t>(reverb)].paramIndex("decay"), 7.5f);
        d.clickOn(car, rc, ModifierKeys::rightButtonModifier);
        d.click(menu.rowBounds(menu.indexOfText("None")).getCentre().toFloat());
        pump(150);
        check(selected(*proc) == -1 && car.selected() == -1, "choosing None un-stages the effect (carousel reads None)");
        check(!ed->settings().isVisible(), "...and hides the settings panel");
        float decay;
        {
            const ScopedLock sl(proc->sessionLock());
            decay = proc->session().draft(reverb)[static_cast<size_t>(fxRegistry()[static_cast<size_t>(reverb)].paramIndex("decay"))];
        }
        check(std::abs(decay - 7.5f) < 1e-3f, "...and keeps Reverb's cached draft");
        for (int k = 0; k < 6; ++k) {  // rapid repeated None / Reverb selection through the menu
            d.clickOn(car, rc, ModifierKeys::rightButtonModifier);
            d.click(menu.rowBounds(menu.indexOfText("None")).getCentre().toFloat());
            pump(20);
        }
        check(selected(*proc) == -1 && !menu.isOpen(), "rapid None selections stay on None and close the list");
        proc->uiSelect(reverb);
        pump(150);
    }
    // resting on another category switches the submenu (hover intent)
    {
        d.clickOn(car, rc, ModifierKeys::rightButtonModifier);
        const int a = menu.indexOfText("Modulation"), b = menu.indexOfText("Dynamics");
        d.move(menu.rowBounds(a).getCentre().toFloat());
        pump(60);
        d.move(menu.rowBounds(b).getCentre().toFloat());
        pump(400);
        check(menu.subOpen() && menu.subParent() == b, "resting on another category switches the submenu");
        d.key(KeyPress(KeyPress::leftKey));
        check(menu.isOpen() && !menu.subOpen(), "Left closes only the submenu");
        d.key(KeyPress(KeyPress::escapeKey));
        check(!menu.isOpen() && selected(*proc) == reverb, "Escape closes the menu; nothing changed");
    }
    // with something staged (COMMIT live): open the list, click where COMMIT sits -> list closes, no commit
    {
        d.clickOn(car, rc, ModifierKeys::rightButtonModifier);
        d.click({110.0f, static_cast<float>(ed->getHeight() - 36)});
        pump(400);
        check(!menu.isOpen() && chainSize(*proc) == 0 && selected(*proc) == reverb, "an outside click cancels the list and never clicks through to COMMIT");
    }
    // near the bottom-right edge: the list and its submenu flip to stay inside the editor
    {
        d.clickOn(car, {car.getWidth() - 50, car.getHeight() - 4}, ModifierKeys::rightButtonModifier);
        check(menu.isOpen() && ed->getLocalBounds().contains(menu.listBounds()), "the list stays inside the window near its edges");
        const int last = menu.itemCount() - 1;
        d.click(menu.rowBounds(last).getCentre().toFloat());
        check(menu.subOpen() && ed->getLocalBounds().contains(menu.subListBounds()), "a submenu opened near the edge stays inside the window");
        d.key(KeyPress(KeyPress::escapeKey));
        d.key(KeyPress(KeyPress::escapeKey));
    }
    // keyboard: Down, Right opens the category, Enter stages its first entry
    {
        d.clickOn(car, rc, ModifierKeys::rightButtonModifier);
        d.key(KeyPress(KeyPress::downKey));
        d.key(KeyPress(KeyPress::rightKey));
        const bool subByKey = menu.subOpen();
        d.key(KeyPress(KeyPress::returnKey));
        check(subByKey && !menu.isOpen() && selected(*proc) >= 0, "keyboard: Down, Right, Enter stages an entry from the submenu");
    }

    // ---- 4.6 every dropdown applies exactly the clicked item ----
    proc->uiSelect(reverb);
    pump(200);
    const auto& rd = fxRegistry()[static_cast<size_t>(reverb)];
    const int pMode = rd.paramIndex("mode");
    auto* dd = dynamic_cast<Dropdown*>(ed->settings().controlFor(pMode));
    check(dd != nullptr, "Reverb's Space selector is an IH1 dropdown");
    if (dd != nullptr) {
        const int n = static_cast<int>(rd.params[static_cast<size_t>(pMode)].choices.size());
        std::mt19937 rng(3);
        int okCount = 0, trials = 0;
        auto pickByClick = [&](int item) {
            d.clickOn(*dd, dd->getLocalBounds().getCentre());
            if (!menu.isOpen()) return false;
            // scroll the list until the row is visible, using the wheel inside the list
            for (int guard = 0; guard < 40 && !menu.listBounds().contains(menu.rowBounds(item).getCentre()); ++guard)
                d.wheel(menu.listBounds().getCentre().toFloat(), menu.rowBounds(item).getY() > menu.listBounds().getY() ? -0.12f : 0.12f);
            d.click(menu.rowBounds(item).getCentre().toFloat());
            pump(80);
            return !menu.isOpen();
        };
        for (int item : {0, n / 2, n - 1, n - 1, 0}) {  // first, middle, last, the current one again, first
            ++trials;
            const bool closed = pickByClick(item);
            const bool ok = closed && static_cast<int>(draft(*proc, reverb, pMode)) == item && dd->getSelectedIndex() == item;
            okCount += ok;
            check(ok, "dropdown: clicked item " + String(item) + " -> model " + String(draft(*proc, reverb, pMode)) + ", shown " + String(dd->getSelectedIndex()));
        }
        for (int k = 0; k < 30; ++k) {  // rapid repeated selections
            const int item = static_cast<int>(rng() % static_cast<unsigned>(n));
            ++trials;
            okCount += pickByClick(item) && static_cast<int>(draft(*proc, reverb, pMode)) == item && dd->getSelectedIndex() == item;
        }
        check(okCount == trials, "dropdown: " + String(okCount) + " / " + String(trials) + " selections applied exactly the clicked item");
        // cancel paths never change the value
        const int before = static_cast<int>(draft(*proc, reverb, pMode));
        d.clickOn(*dd, dd->getLocalBounds().getCentre());
        d.key(KeyPress(KeyPress::escapeKey));
        d.clickOn(*dd, dd->getLocalBounds().getCentre());
        d.click({static_cast<float>(ed->getWidth() - 40), static_cast<float>(ed->getHeight() / 2)});
        check(!menu.isOpen() && static_cast<int>(draft(*proc, reverb, pMode)) == before, "Escape and outside clicks cancel without changing the value");
        // the popup never closes by itself right after opening, and a second press does not reopen it
        d.clickOn(*dd, dd->getLocalBounds().getCentre());
        pump(600);
        check(menu.isOpen(), "the dropdown stays open after it opens (no instant dismissal)");
        d.key(KeyPress(KeyPress::escapeKey));
        // press-drag-release onto an item selects it
        {
            const auto from = d.toPeer(*dd, dd->getLocalBounds().getCentre());
            d.t += 600;
            d.press(from);
            pump(150);
            const int target = (before + 1) % n;
            const auto to = menu.rowBounds(target).getCentre().toFloat();
            for (int k = 1; k <= 8; ++k) { d.t += 20; d.peer()->handleMouseEvent(MouseInputSource::InputSourceType::mouse, from + (to - from) * (k / 8.0f), ModifierKeys(ModifierKeys::leftButtonModifier), 0.0f, 0.0f, d.t); pump(10); }
            d.release(to);
            pump(80);
            check(!menu.isOpen() && static_cast<int>(draft(*proc, reverb, pMode)) == target, "press, drag onto an item, release: selects it");
        }
    }
    // segmented selectors apply on one click
    {
        const auto& dlD = fxRegistry()[static_cast<size_t>(delay)];
        proc->uiSelect(delay);
        pump(200);
        const int pStereo = dlD.paramIndex("stereo");
        if (auto* seg = dynamic_cast<Segmented*>(ed->settings().controlFor(pStereo))) {
            const int w = seg->getWidth();
            d.clickOn(*seg, {w * 5 / 6, seg->getHeight() / 2});
            const bool a = static_cast<int>(draft(*proc, delay, pStereo)) == 2;
            d.clickOn(*seg, {w / 6, seg->getHeight() / 2});
            check(a && static_cast<int>(draft(*proc, delay, pStereo)) == 0, "segmented selector applies the clicked option");
        } else {
            check(false, "Delay's Stereo selector is segmented");
        }
    }

    // ---- 5.1 wheel: confined to the carousel, accumulated, inert while a menu is open ----
    {
        proc->uiSelect(delay);
        pump(150);
        const auto c = d.toPeer(car, {120, car.getHeight() / 2});
        for (int k = 0; k < 8; ++k) d.wheel(c, -0.015f);  // a high-resolution wheel: tiny deltas
        const int afterSmall = selected(*proc);
        check(afterSmall != delay, "small wheel deltas accumulate into one step");
        const int s0 = selected(*proc);
        d.clickOn(car, {120, car.getHeight() / 2}, ModifierKeys::rightButtonModifier);
        d.wheel(c, -0.5f);
        d.wheel(c, -0.5f);
        check(selected(*proc) == s0, "wheel over the carousel does nothing while a list is open");
        d.key(KeyPress(KeyPress::escapeKey));
        const auto onSettings = d.toPeer(ed->settings(), {ed->settings().getWidth() / 2, 20});
        d.wheel(onSettings, -0.5f);
        check(selected(*proc) == s0, "wheel over the settings panel never changes the staged effect");
    }

    // ---- commit, drag-to-DELETE (4.1), reorder ----
    {
        proc->uiSelect(reverb);
        pump(100);
        ed->formation();  // ensure constructed
        // three commits: Reverb, Delay, Reverb (a duplicate)
        auto commitClick = [&] {
            auto* commitBtn = [&]() -> Component* {
                for (auto* c : ed->getChildren()) if (auto* b = dynamic_cast<TextButton*>(c); b && b->getButtonText() == "COMMIT") return b;
                return nullptr;
            }();
            if (commitBtn) {
                waitFor([&] { return commitBtn->isEnabled(); });  // the editor timer enables COMMIT after an audition
                d.clickOn(*commitBtn, commitBtn->getLocalBounds().getCentre());
            }
            pump(450);  // past the double-commit guard
        };
        commitClick();
        // V5 Phase 7: the browser stays on the committed effect with a fresh, muted ("standby") copy
        check(chainSize(*proc) == 1 && selected(*proc) == reverb, "COMMIT adds one instance and the carousel stays on the effect (V5)");
        check(ed->settings().isVisible(), "...the settings panel stays visible");
        bool standby = false, muted = false;
        {
            const ScopedLock sl(proc->sessionLock());
            standby = proc->session().standby();
            muted = proc->session().mute();
        }
        check(standby && muted, "...and the fresh copy is muted (standby), so the committed one is heard once");
        {
            bool carouselOn = false, commitOn = true;
            for (auto* c : ed->getChildren()) {
                if (auto* b = dynamic_cast<TextButton*>(c); b && b->getButtonText() == "COMMIT") commitOn = b->isEnabled();
            }
            carouselOn = ed->carousel().isEnabled();
            check(carouselOn && !commitOn, "...the carousel stays usable; COMMIT waits for an audition (no silent double commit)");
        }
        // a UI edit of a control auditions the fresh copy
        proc->uiSetParam(0, fxRegistry()[static_cast<size_t>(reverb)].params[0].defaultValue);
        pump(100);
        {
            const ScopedLock sl(proc->sessionLock());
            check(!proc->session().mute() && !proc->session().standby(), "...touching a control ends standby (MUTE off)");
        }
        proc->uiSelect(delay); pump(100); commitClick();
        proc->uiSelect(reverb); pump(100); commitClick();
        pump(300);
        check(chainSize(*proc) == 3, "three committed instances (one duplicate)");
        auto& fv = ed->formation();
        uint64_t ids[3];
        { const ScopedLock sl(proc->sessionLock()); for (int i = 0; i < 3; ++i) ids[i] = proc->session().chain()[static_cast<size_t>(i)].id(); }
        // drag the duplicate Reverb (index 2) to DELETE, cancel with Escape
        const auto item2 = d.toPeer(fv, fv.itemBounds(2).getCentre().toInt());
        const auto del = d.toPeer(fv, fv.deleteZone().getCentre().toInt());
        bool deleteVisibleWhileDragging = false;
        d.dragTo(item2, del, [&] { deleteVisibleWhileDragging = true; });
        check(fv.confirmDialog() != nullptr, "dropping on DELETE asks for confirmation");
        check(chainSize(*proc) == 3, "...and has not deleted anything yet");
        d.key(KeyPress(KeyPress::escapeKey));
        pump(120);
        check(fv.confirmDialog() == nullptr && chainSize(*proc) == 3, "Escape cancels the delete");
        // drop outside the delete target: nothing happens
        d.dragTo(item2, {static_cast<float>(fv.getX() + fv.getWidth() / 2), static_cast<float>(fv.getHeight() / 2)});
        check(fv.confirmDialog() == nullptr && chainSize(*proc) == 3, "dropping elsewhere never deletes");
        // drag again and confirm: exactly that instance goes, not the other Reverb
        d.dragTo(item2, del);
        if (auto* dlg = fv.confirmDialog()) {
            for (auto* c : dlg->getChildren())
                if (auto* b = dynamic_cast<TextButton*>(c); b && b->getButtonText() == "Delete") d.clickOn(*b, b->getLocalBounds().getCentre());
        }
        pump(200);
        bool rightOne = false;
        { const ScopedLock sl(proc->sessionLock()); const auto& c = proc->session().chain(); rightOne = c.size() == 2 && c[0].id() == ids[0] && c[1].id() == ids[1]; }
        check(rightOne, "Delete removes exactly the dragged duplicate");
        // reorder: drag item 0 after item 1
        const auto i0 = d.toPeer(fv, fv.itemBounds(0).getCentre().toInt());
        const auto after1 = d.toPeer(fv, {static_cast<int>(fv.itemBounds(1).getRight()) + 14, static_cast<int>(fv.itemBounds(1).getCentreY())});
        d.dragTo(i0, after1);
        pump(150);
        bool reordered = false;
        { const ScopedLock sl(proc->sessionLock()); const auto& c = proc->session().chain(); reordered = c.size() == 2 && c[0].id() == ids[1] && c[1].id() == ids[0]; }
        check(reordered, "dragging along the strip reorders the audio chain");
        (void) deleteVisibleWhileDragging;
        // V4 Phase 6: a click (no drag) on a committed name bypasses that instance; its values never change; a second
        // click enables it again; the label greys while bypassed (the item records the state it draws)
        {
            ih1::ParamValues before;
            uint64_t id0;
            { const ScopedLock sl(proc->sessionLock()); id0 = proc->session().chain()[0].id(); before = proc->session().chain()[0].values(); }
            d.click(d.toPeer(fv, fv.itemBounds(0).getCentre().toInt()));
            pump(150);
            bool off = false, same = false;
            { const ScopedLock sl(proc->sessionLock()); const auto& c = proc->session().chain(); off = c[0].id() == id0 && !c[0].enabled(); same = c[0].values() == before; }
            check(off && !fv.itemEnabled(0), "V4: clicking a committed FX name bypasses exactly that instance (label greyed)");
            check(same, "V4: ...without changing its committed settings");
            d.click(d.toPeer(fv, fv.itemBounds(0).getCentre().toInt()));
            pump(150);
            bool on = false;
            { const ScopedLock sl(proc->sessionLock()); on = proc->session().chain()[0].enabled() && proc->session().chain()[0].values() == before; }
            check(on && fv.itemEnabled(0), "V4: a second click enables it again with the same settings");
        }
    }

    // ---- V4 Phase 3: None is an ordinary carousel entry: the wheel scrolls into and out of it in both directions ----
    {
        proc->uiSelect(-1);
        pump(150);
        const auto c = d.toPeer(car, {120, car.getHeight() / 2});
        d.wheel(c, -0.5f);  // down: the first effect
        pump(120);
        const int first = selected(*proc);
        check(first >= 0 && car.selected() == first, "V4: from None the wheel steps to the next entry");
        d.wheel(c, 0.5f);  // back up: None again (not skipped, not a ghost)
        pump(120);
        check(selected(*proc) == -1 && car.selected() == -1 && !ed->settings().isVisible(), "V4: scrolling back lands on None (no staged FX, settings hidden)");
        d.wheel(c, 0.5f);  // further up: past None to the last effect
        pump(120);
        const int last = selected(*proc);
        check(last >= 0 && last != first, "V4: scrolling up from None continues to the previous (last) entry");
        d.wheel(c, -0.5f);
        pump(120);
        check(selected(*proc) == -1, "V4: and down again lands on None");
        int lands = 0;
        for (int k = 0; k < 12; ++k) { d.wheel(c, k % 2 ? 0.5f : -0.5f); pump(90); lands += (selected(*proc) == car.selected()) ? 1 : 0; }
        check(lands == 12, "V4: rapid wheel steps: the carousel always shows the staged state (" + String(lands) + "/12)");
        proc->uiSelect(-1);
        pump(100);
    }

    // ---- 4.15 preset box ----
    {
        String msg;
        const auto list = proc->presetList();
        int factory = 0;
        for (const auto& e : list) factory += e.factory;
        check(factory == static_cast<int>(factoryPresets().size()), "browser lists every factory preset (" + String(factory) + ")");
        bool orderOk = true;
        for (int k = 0; k < static_cast<int>(list.size()); ++k) {
            proc->stepPreset(1, msg);
            orderOk = orderOk && proc->currentPresetKey() == list[static_cast<size_t>(k)].key;
        }
        proc->stepPreset(1, msg);
        check(orderOk && proc->currentPresetKey() == list.front().key, "next walks the whole list in order and wraps to the start");
        proc->stepPreset(-1, msg);
        check(proc->currentPresetKey() == list.back().key, "previous wraps to the end");
        check(!proc->presetModified(), "a freshly loaded preset is not marked modified");
        proc->uiSelect(delay);
        proc->uiCommit();
        check(proc->presetModified(), "committing on top of a preset marks it modified");
        // malformed user file: rejected, chain untouched
        auto bad = File::getSpecialLocation(File::tempDirectory).getChildFile("ih1_bad.ih1chain");
        bad.replaceWithText("{\"format\":\"ih1-state\",\"version\":99,\"chain\":[]}");
        const auto n0 = chainSize(*proc);
        check(!proc->loadPresetKey("u:" + bad.getFullPathName(), msg) && chainSize(*proc) == n0, "a malformed preset file is rejected; the chain is untouched (" + msg + ")");
        bad.deleteFile();
    }

    // ---- V2.1 Phase 10: preset arrows, 100 rapid clicks, every click exactly one step ----
    {
        String msg;
        proc->loadPresetKey(proc->presetList().front().key, msg);
        pump(100);
        auto& bar = ed->presetBar();
        const auto list = proc->presetList();
        const int n = static_cast<int>(list.size());
        auto indexOfCurrent = [&] {
            for (int i = 0; i < n; ++i) if (list[static_cast<size_t>(i)].key == proc->currentPresetKey()) return i;
            return -1;
        };
        std::mt19937 rng(17);
        int expected = indexOfCurrent(), dropped = 0, wrong = 0;
        String log;
        for (int k = 0; k < 120; ++k) {
            const bool next = rng() % 3 != 0;  // mostly forward, some back
            const auto zone = next ? bar.nextArea() : bar.prevArea();
            // vary the click position across the whole visible zone (edges included), and click fast: many of
            // these presses arrive within the double-click interval
            const Point<int> p(zone.getX() + 2 + static_cast<int>(rng() % static_cast<unsigned>(std::max(1, zone.getWidth() - 4))),
                               zone.getY() + 2 + static_cast<int>(rng() % static_cast<unsigned>(std::max(1, zone.getHeight() - 4))));
            const auto pp = d.toPeer(bar, p);
            d.t += 70;
            d.press(pp);
            d.release(pp);
            pump(5);
            expected = ((expected + (next ? 1 : -1)) % n + n) % n;
            const int actual = indexOfCurrent();
            if (actual != expected) { ++wrong; log << k << ":" << expected << "/" << actual << " "; expected = actual; }
            (void) dropped;
        }
        check(wrong == 0, "preset arrows: 120 rapid clicks (incl. double-clicks, zone edges) -> " + String(120 - wrong) + " exact single steps" + (log.isEmpty() ? String() : " mismatches " + log));
        check(bar.steps() >= 120, "every press was handled (" + String(bar.steps()) + ")");
    }

    // ---- V3 Phase 1: every preset route lands the wheel on None; the stage is never left audible ----
    {
        String msg;
        auto& bar = ed->presetBar();
        const int sat = fxTypeIndex("saturation");
        const int driveIdx = fxRegistry()[static_cast<size_t>(sat)].paramIndex("drive");
        int okArrow = 0, okLoad = 0;
        for (int k = 0; k < 10; ++k) {
            proc->uiSelect(k % 2 ? sat : delay);
            if (k % 2) proc->uiSetParam(driveIdx, 21.0f);
            proc->uiSetSolo(k % 3 == 0);
            pump(40);
            const auto zone = bar.nextArea();
            const auto pp = d.toPeer(bar, zone.getCentre());
            d.t += 400;
            d.press(pp);
            d.release(pp);
            auto landed = [&] {
                return selected(*proc) == -1 && car.selected() == -1 && !ed->settings().isVisible() &&
                       proc->params().getRawParameterValue("solo")->load() < 0.5f;
            };
            const bool okK = waitFor(landed, 1000);
            if (!okK)
                std::printf("   iteration %d: selected %d carousel %d settings %d solo %.1f preset %s\n", k, selected(*proc), car.selected(),
                            ed->settings().isVisible() ? 1 : 0, proc->params().getRawParameterValue("solo")->load(), proc->currentPresetKey().toRawUTF8());
            okArrow += okK;
        }
        check(okArrow == 10, "preset arrow with an FX staged (and SOLO on): wheel None, settings hidden, SOLO off (" + String(okArrow) + "/10)");
        for (int k = 0; k < 5; ++k) {
            proc->uiSelect(sat);
            pump(20);
            proc->loadPresetKey(proc->presetList()[static_cast<size_t>(3 + k * 5)].key, msg);  // the browser's load path
            okLoad += waitFor([&] { return selected(*proc) == -1 && car.selected() == -1; }, 1000);
        }
        check(okLoad == 5, "loading from the browser path lands on None (" + String(okLoad) + "/5)");
        float drive;
        {
            const ScopedLock sl(proc->sessionLock());
            drive = proc->session().draft(sat)[static_cast<size_t>(driveIdx)];
        }
        check(std::abs(drive - 21.0f) < 1e-3f, "the staged Saturation draft is still cached after preset changes");
        // Init: completely empty, None, drafts reset, globals neutral
        proc->uiSelect(sat);
        proc->params().getParameter("out_gain")->setValueNotifyingHost(0.9f);
        check(proc->loadPresetKey("f:init", msg), "Init loads");
        pump(60);
        {
            const ScopedLock sl(proc->sessionLock());
            drive = proc->session().draft(sat)[static_cast<size_t>(driveIdx)];
        }
        check(chainSize(*proc) == 0 && selected(*proc) == -1 && car.selected() == -1, "Init: empty chain and None");
        check(std::abs(drive - fxRegistry()[static_cast<size_t>(sat)].params[static_cast<size_t>(driveIdx)].defaultValue) < 1e-4f, "Init: drafts back to defaults");
        check(std::abs(proc->params().getRawParameterValue("out_gain")->load()) < 1e-4f, "Init: output gain back to 0 dB");
        check(proc->presetList().front().key == "f:init", "Init is pinned first in the preset list");
    }

    // ---- V2.1 Phase 15: discreet Clear FX Chain with confirmation ----
    {
        auto& fv = ed->formation();
        proc->uiSelect(delay); proc->uiCommit();
        proc->uiSelect(delay); proc->uiCommit();
        proc->uiSelect(reverb); proc->uiCommit();
        proc->uiSelect(fxTypeIndex("chorus"));
        proc->uiSetParam(0, 0.9f);
        pump(200);
        const auto n0 = chainSize(*proc);
        const int stagedBefore = selected(*proc);
        const auto dots = d.toPeer(fv, fv.chainMenuBounds().getCentre().toInt());
        auto openClear = [&] {
            d.click(dots);
            if (!menu.isOpen()) return false;
            d.click(menu.rowBounds(menu.indexOfText("Clear FX Chain...")).getCentre().toFloat());
            pump(80);
            return fv.confirmDialog() != nullptr;
        };
        check(openClear(), "the chain options (...) offer Clear FX Chain, which asks first");
        check(chainSize(*proc) == n0, "...and nothing is cleared yet");
        if (auto* c = fv.cancelButton()) d.clickOn(*c, c->getLocalBounds().getCentre());
        pump(150);
        check(fv.confirmDialog() == nullptr && chainSize(*proc) == n0, "Cancel leaves the chain untouched");
        openClear();
        d.key(KeyPress(KeyPress::escapeKey));
        pump(150);
        check(fv.confirmDialog() == nullptr && chainSize(*proc) == n0, "Escape leaves the chain untouched");
        openClear();
        d.click({4.0f, 4.0f});  // outside the dialog box
        pump(150);
        check(fv.confirmDialog() == nullptr && chainSize(*proc) == n0, "clicking outside leaves the chain untouched");
        openClear();
        if (auto* ok = fv.confirmButton()) {
            check(ok->getButtonText() == "Clear Chain", "the destructive action is labelled Clear Chain");
            d.clickOn(*ok, ok->getLocalBounds().getCentre());
        }
        pump(200);
        check(chainSize(*proc) == 0, "Clear Chain removes every committed FX (duplicates included)");
        check(selected(*proc) == stagedBefore && ed->settings().isVisible(), "...and keeps the staged FX and its settings");
        // repeated rapid open / cancel is safe
        for (int k = 0; k < 10; ++k) { openClear(); d.key(KeyPress(KeyPress::escapeKey)); }
        check(fv.confirmDialog() == nullptr && !menu.isOpen(), "rapid open / cancel leaves no dialog behind");
    }

    // ---- V2.1 Phase 16: latency mode through the chain options ----
    {
        auto& fv = ed->formation();
        proc->uiSelect(-1);
        proc->uiSelect(fxTypeIndex("saturation")); proc->uiCommit();
        proc->uiSelect(fxTypeIndex("convolution")); proc->uiCommit();
        pump(150);
        const int full = proc->getLatencySamples();
        const auto dots = d.toPeer(fv, fv.chainMenuBounds().getCentre().toInt());
        d.click(dots);
        d.click(menu.rowBounds(menu.indexOfText("Zero Latency")).getCentre().toFloat());
        pump(150);
        LatencyMode m;
        { const ScopedLock sl(proc->sessionLock()); m = proc->session().latencyMode(); }
        check(m == LatencyMode::Zero && full > 0 && proc->getLatencySamples() == 0,
              "Zero Latency: host latency " + String(full) + " -> " + String(proc->getLatencySamples()) + " samples");
        proc->uiSelect(fxTypeIndex("harmony")); proc->uiCommit();
        pump(100);
        const auto st = proc->latencyStatus();
        check(proc->getLatencySamples() > 0 && st.degraded && st.headline.contains("Harmony"),
              "a chain with Harmony names it as the reason: \"" + st.headline + "\"");
        proc->uiSetLatencyMode(LatencyMode::Quality);
        proc->uiClearChain();
        pump(100);
    }

    // ---- V2.1 Phase 2 / 8 / 9.7: sync-disabled controls, advanced always visible, mute greys the column ----
    {
        proc->uiSelect(delay);
        pump(200);
        const auto& dl = fxRegistry()[static_cast<size_t>(delay)];
        const int pTime = dl.paramIndex("time_ms"), pSync = dl.paramIndex("sync");
        proc->uiSetParam(pSync, 1.0f);
        pump(150);
        auto* timeKnob = ed->settings().controlFor(pTime);
        const float before = draft(*proc, delay, pTime);
        check(timeKnob && !timeKnob->isEnabled(), "Sync on: the free Time knob is disabled");
        if (timeKnob) {
            const auto c = d.toPeer(*timeKnob, timeKnob->getLocalBounds().getCentre());
            d.dragTo(c, c + Point<float>(0.0f, -60.0f));
            d.wheel(c, 0.5f);
            check(draft(*proc, delay, pTime) == before, "...and ignores drags and the wheel (value kept: " + String(before) + ")");
            auto* tc = dynamic_cast<SettableTooltipClient*>(timeKnob);
            check(tc && tc->getTooltip().containsIgnoreCase("Sync"), "...and its tooltip says why");
        }
        auto* div = ed->settings().controlFor(dl.paramIndex("division"));
        check(div && div->isEnabled(), "the Division selector stays active");
        proc->uiSetParam(pSync, 0.0f);
        pump(150);
        check(timeKnob && timeKnob->isEnabled() && draft(*proc, delay, pTime) == before, "Sync off: Time is active again with its kept value");
        // advanced controls are always visible (no More / Less)
        bool allVisible = true;
        for (size_t i = 0; i < dl.params.size(); ++i) if (auto* c = ed->settings().controlFor(static_cast<int>(i))) allVisible = allVisible && c->isVisible();
        check(allVisible, "every Delay control, advanced included, is visible without any toggle");
        // MUTE greys and disables the whole staged column except S / M
        proc->uiSetMute(true);
        pump(200);
        bool allOff = !car.isEnabled();
        for (size_t i = 0; i < dl.params.size(); ++i) if (auto* c = ed->settings().controlFor(static_cast<int>(i))) allOff = allOff && !c->isEnabled();
        bool commitOff = true;
        for (auto* c : ed->getChildren()) if (auto* b = dynamic_cast<TextButton*>(c); b && (b->getButtonText() == "COMMIT" || b->getButtonText() == "RESET")) commitOff = commitOff && !b->isEnabled();
        check(allOff && commitOff, "MUTE: carousel, every control, COMMIT and RESET are greyed and inactive");
        proc->uiSetMute(false);
        pump(200);
        check(car.isEnabled() && ed->settings().controlFor(pTime)->isEnabled(), "unmuting restores the column");
    }

    // ---- the OpenGL formation compiles its shaders (incl. the V2.1 glow pass) and keeps rendering ----
    if (SystemStats::getEnvironmentVariable("IH1_NO_OPENGL", {}).isEmpty()) {
        String msg;
        proc->uiClearChain();
        proc->loadPresetKey("f:gm.cascade", msg);
        for (const char* extra : {"chorus", "delay", "harmony", "saturation", "reverb"}) { proc->uiSelect(fxTypeIndex(extra)); proc->uiCommit(); }
        proc->uiSelect(fxTypeIndex("grain_engine"));
        pump(3000);
        // Machines without OpenGL 3.2 (GPU-less Windows CI runners offer GL 1.1) correctly fall back to the
        // software formation. Only where GL is known to exist (IH1_REQUIRE_GL=1, e.g. Linux CI with Mesa) is a
        // fallback a failure: there it would mean a shader or render-target error.
        if (SystemStats::getEnvironmentVariable("IH1_REQUIRE_GL", {}).isNotEmpty())
            check(!ed->usingSoftwareFormation(), "the OpenGL formation renders (shaders and targets OK, no software fallback)");
        else
            check(true, ed->usingSoftwareFormation() ? "no OpenGL 3.2 here: the software formation took over (expected fallback)"
                                                     : "the OpenGL formation renders (shaders and targets OK)");
        if (const int hold = SystemStats::getEnvironmentVariable("IH1_GL_HOLD_MS", "0").getIntValue(); hold > 0) {
            std::printf("GL_HOLD_START\n");
            std::fflush(stdout);
            pump(hold);
        }
    }

    // ---- optional: snapshots for visual review (IH1_SNAPSHOT_DIR=<dir>, best with IH1_NO_OPENGL=1) ----
    if (const auto dir = SystemStats::getEnvironmentVariable("IH1_SNAPSHOT_DIR", {}); dir.isNotEmpty()) {
        const File out(dir);
        out.createDirectory();
        auto snap = [&](const String& name) {
            pump(100);
            const auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
            PNGImageFormat png;
            FileOutputStream os(out.getChildFile(name + ".png"));
            if (os.openedOk()) { os.setPosition(0); os.truncate(); png.writeImageToStream(img, os); }
        };
        String msg;
        // V4: a V4 preset (macros shown beside IN / MIX / OUT), one committed FX bypassed (greyed name and field)
        proc->loadPresetKey("f:v4.signal_collapse", msg);
        for (const char* extra : {"reverb", "chorus", "modal"}) { proc->uiSelect(fxTypeIndex(extra)); proc->uiCommit(); }
        { uint64_t id1; { const ScopedLock sl(proc->sessionLock()); id1 = proc->session().chain()[1].id(); } proc->uiToggleEnabled(id1); }
        pump(5000);
        snap("v4_editor");
        proc->uiClearChain();
        proc->loadPresetKey("f:gm.particle_drift", msg);
        for (const char* extra : {"chorus", "delay", "harmony", "saturation"}) { proc->uiSelect(fxTypeIndex(extra)); proc->uiCommit(); }
        proc->uiSelect(fxTypeIndex("grain_engine"));
        pump(4500);
        snap("v21_editor_grain_engine");
        d.clickOn(car, {120, car.getHeight() / 2}, ModifierKeys::rightButtonModifier);
        d.move(menu.rowBounds(menu.indexOfText("Granular & Time")).getCentre().toFloat());
        pump(200);
        snap("v21_nested_menu");
        d.key(KeyPress(KeyPress::escapeKey));
        d.key(KeyPress(KeyPress::escapeKey));
        proc->uiSelect(fxTypeIndex("delay"));
        proc->uiSetMute(true);
        pump(300);
        snap("v21_muted_delay");
        proc->uiSetMute(false);
        ed->formation().openClearConfirm();
        snap("v21_clear_dialog");
        d.key(KeyPress(KeyPress::escapeKey));
        pump(200);
    }

    // ---- V6 P0: the FL Studio access violation (IH1.vst3 + 0x27F8D, Carousel::showCatalogueMenu read reg[-1] for the
    // None entry of the carousel ring). The user's sequence: a preset with macros recalled (staged controls 3-8 and
    // preset macros 1-3 re-synced, as in FL's undo list), macro and staged-control tweaks, then the right-click FX list.
    // The out-of-bounds read is silent in most heap layouts, so this block is also run under AddressSanitizer in CI.
    {
        int withMacros = -1;
        const auto& fp = factoryPresets();
        for (size_t i = 0; i < fp.size() && withMacros < 0; ++i) if (!fp[i].macros.empty()) withMacros = static_cast<int>(i);
        check(withMacros >= 0, "V6: a factory preset with macros exists");
        const int doppler = fxTypeIndex("doppler");
        int menuOk = 0;
        const int rounds = 25;
        for (int round = 0; round < rounds; ++round) {
            String msg;
            if (withMacros >= 0) proc->loadPresetKey("f:" + String(fp[static_cast<size_t>(withMacros)].id), msg);
            pump(40);
            for (int k = 0; k < kNumPresetMacros; ++k) {
                proc->uiPresetMacroGesture(k, true);
                for (int s = 0; s <= 8; ++s) proc->uiSetPresetMacro(k, static_cast<float>((s + round) % 9) / 8.0f);
                proc->uiPresetMacroGesture(k, false);
            }
            proc->uiSelect(round % 2 ? doppler : fxTypeIndex("reverb"));
            pump(40);
            const int sel = selected(*proc);
            const int np = sel >= 0 ? static_cast<int>(fxRegistry()[static_cast<size_t>(sel)].params.size()) : 0;
            for (int k = 2; k < kNumMacros; ++k) {  // staged controls 3..8 (including indices beyond this effect's count)
                proc->uiGesture(k, true);
                if (k < np) {
                    const auto& sp = fxRegistry()[static_cast<size_t>(sel)].params[static_cast<size_t>(k)];
                    proc->uiSetParam(k, sp.fromNormalised(static_cast<float>(round % 5) / 4.0f));
                } else {
                    proc->uiSetParam(k, 0.5f);  // no such control: must be ignored
                }
                proc->uiGesture(k, false);
            }
            pump(40);
            d.clickOn(car, {120, car.getHeight() / 2}, ModifierKeys::rightButtonModifier);
            pump(30);
            // None once at the top, then one row per non-empty category, each FX exactly once across the submenus
            int total = 0, cats = 0;
            for (int c = 0; c < kNumCategories; ++c) {
                int inCat = 0;
                for (int idx : fxCarouselOrder()) inCat += static_cast<int>(fxRegistry()[static_cast<size_t>(idx)].category) == c ? 1 : 0;
                total += inCat;
                cats += inCat > 0 ? 1 : 0;
            }
            if (menu.isOpen() && menu.indexOfText("None") == 0 && menu.itemCount() == cats + 1 && total == static_cast<int>(fxCarouselOrder().size())) ++menuOk;
            d.key(KeyPress(KeyPress::escapeKey));
            pump(20);
        }
        check(menuOk == rounds, "V6 P0: preset recall + macro / staged-control tweaks + right-click FX list, " + String(rounds) + " rounds: list correct every time (" + String(menuOk) + ")");
        check(selected(*proc) >= 0, "...and the session is still usable");
    }

    // ---- V6 Phase 4: menu / browser layout snapshots (IH1_SNAPSHOT_DIR), at the minimum and a large editor size ----
    if (const auto dir = SystemStats::getEnvironmentVariable("IH1_SNAPSHOT_DIR", {}); dir.isNotEmpty()) {
        const File out(dir);
        out.createDirectory();
        auto snap = [&](const String& name, float scale) {
            pump(150);
            const auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, scale);
            PNGImageFormat png;
            FileOutputStream os(out.getChildFile(name + ".png"));
            if (os.openedOk()) { os.setPosition(0); os.truncate(); png.writeImageToStream(img, os); }
        };
        for (auto size : {Point<int>(Editor::kMinW, Editor::kMinH), Point<int>(1600, 1000)}) {
            ed->setSize(size.x, size.y);
            pump(300);
            const String tag = String(size.x) + "x" + String(size.y);
            String msg;
            proc->loadPresetKey("f:" + String(factoryPresets().back().id), msg);
            proc->uiSelect(fxTypeIndex("reflections"));
            pump(200);
            d.clickOn(car, {120, car.getHeight() / 2}, ModifierKeys::rightButtonModifier);
            for (int i = 0; i < menu.itemCount(); ++i)
                if (menu.rowBounds(i).getHeight() > 0 && menu.indexOfText("Time & Space") == i) d.move(menu.rowBounds(i).getCentre().toFloat());
            pump(250);
            snap("v6_fx_menu_" + tag, 1.0f);
            snap("v6_fx_menu_" + tag + "_x2", 2.0f);
            d.key(KeyPress(KeyPress::escapeKey));
            d.key(KeyPress(KeyPress::escapeKey));
            if (ed->presetBar().onOpenBrowser) ed->presetBar().onOpenBrowser();
            pump(200);
            {
                const auto pb = ed->getLocalArea(&ed->presetBar(), ed->presetBar().getLocalBounds());
                d.move(Point<float>(static_cast<float>(pb.getX() + 260), static_cast<float>(pb.getBottom() + 6 + 50 + 13)));
            }
            snap("v6_presets_" + tag, 1.0f);
            for (auto ch : String("drum")) d.key(KeyPress(static_cast<int>(ch), ModifierKeys(), ch));
            pump(200);
            snap("v6_presets_search_" + tag, 1.0f);
            d.key(KeyPress(KeyPress::escapeKey));
            ed->browser().close();
            pump(100);
        }
    }

    // ---- editor close with a menu open, reopen ----
    {
        d.clickOn(car, {120, car.getHeight() / 2}, ModifierKeys::rightButtonModifier);
        ed.reset();
        pump(200);
        edRaw = dynamic_cast<Editor*>(proc->createEditor());
        ed.reset(edRaw);
        ed->addToDesktop(ComponentPeer::windowHasTitleBar);
        ed->setVisible(true);
        pump(400);
        check(ed->getWidth() >= 1120, "closing the editor with a list open, then reopening, is safe");
        ed.reset();
    }
    pump(200);
    proc.reset();
    std::printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures;
}
