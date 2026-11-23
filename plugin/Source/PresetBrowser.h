// Preset selector (V1 feedback 4.15, 4.16): a conventional preset box, "< name >", in the header. The arrows
// step through the global preset order (factory categories in order, then user presets) and wrap at both
// ends. Clicking the name opens a compact in-editor browser: categories on the left, the presets of the
// selected category (or search results) on the right with a one-line description, and inline Save / Save As /
// Rename / Delete for user presets (factory presets are read-only). Loading replaces the committed chain in
// one validated step; nothing is changed if a file is malformed.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace ih1::plugin {

class Processor;

class PresetBar final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit PresetBar(Processor& p);
    void refresh();  // name / modified marker
    std::function<void()> onOpenBrowser;
    std::function<void(const juce::String& message)> onMessage;
    void paint(juce::Graphics&) override;
    void resized() override {}
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override { pressedSide_ = 0; repaint(); }
    void mouseMove(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    juce::String getTooltip() override;
    // V2.1 (feedback 10): the arrows are full-height zones, 30 px wide, aligned with the drawn chevrons. They act
    // on mouse-down, so every press steps exactly once: a fast second click (a double-click) is a second step,
    // and a press that drifts before release is never lost (the old 10 x 14 px buttons fired on mouse-up only).
    juce::Rectangle<int> prevArea() const { return getLocalBounds().removeFromLeft(kArrowW); }
    juce::Rectangle<int> nextArea() const { return getLocalBounds().removeFromRight(kArrowW); }
    juce::Rectangle<int> nameArea() const;
    int steps() const { return steps_; }  // tests: arrow presses handled

private:
    static constexpr int kArrowW = 30;
    void step(int dir);
    Processor& proc_;
    juce::String name_;
    bool modified_ = false;
    int pressedSide_ = 0, steps_ = 0;
};

class PresetBrowser final : public juce::Component {
public:
    explicit PresetBrowser(Processor& p);
    void open(juce::Rectangle<int> below);
    void close();
    bool isOpen() const { return isVisible(); }
    std::function<void(const juce::String& message)> onMessage;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void parentSizeChanged() override { if (isOpen()) close(); }
    bool hitTest(int, int) override { return isVisible(); }

private:
    struct Entry { juce::String key, name, category, description; bool factory; juce::String meta; };
    enum class Mode { Browse, SaveAs, Rename, ConfirmDelete, ConfirmOverwrite };
    void reload();
    void filter();
    int rowAt(juce::Point<int>) const;
    int categoryAt(juce::Point<int>) const;
    void load(int filteredIndex);
    void setMode(Mode m);
    void commitName();
    const Entry* current() const;

    Processor& proc_;
    std::vector<Entry> all_;
    std::vector<int> shown_;  // indices into all_
    juce::StringArray categories_;
    int category_ = 0, hot_ = -1, pressedRow_ = -1, scroll_ = 0, hotCat_ = -1;
    juce::Rectangle<int> panel_, catArea_, listArea_, footer_, descArea_;
    juce::TextEditor search_, nameEdit_;
    juce::TextButton save_{"Save"}, saveAs_{"Save As"}, rename_{"Rename"}, del_{"Delete"}, ok_{"OK"}, cancel_{"Cancel"};
    Mode mode_ = Mode::Browse;
    juce::String pendingName_;
    static constexpr int kRowH = 26;
    // V6 Phase 4: category column geometry, computed in resized() from the real names (single source of truth for
    // painting and hit-testing); rows shrink to 20 px before the column scrolls
    int catRowH_ = kRowH, catScroll_ = 0;
    int catVisible() const { return std::max(1, catArea_.getHeight() / std::max(1, catRowH_)); }
    std::vector<int> catCounts_;
};

}  // namespace ih1::plugin
