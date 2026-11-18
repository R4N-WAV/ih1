// IH1 interaction widgets.
//
// MenuOverlay is the one popup surface used by every dropdown, list and context menu in IH1 (V1 feedback
// 4.6, 4.13). It is a child component covering the editor, not a separate desktop window. JUCE's desktop
// PopupMenu polls focus and dismisses itself when no JUCE window owns keyboard focus, and it triggers items
// by polling real-time mouse state. Both are fragile inside a host-owned (or bridged) plug-in window, and
// either can make a menu close without applying the clicked item. Here the flow is explicit:
//   * open() once per gesture; a second open while one is showing is ignored;
//   * an item is applied only when the mouse is pressed AND released on that same enabled item (or the
//     opening press is dragged onto it and released), or when Enter is pressed on the highlighted item;
//   * a click outside the list, Escape, an editor resize or a lost owner cancel without changing anything;
//   * the result callback runs exactly once (id, or 0 = cancelled) after the overlay has closed;
//   * every mouse event and wheel event while open is consumed (no click-through, no carousel scroll).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace ih1::plugin {

struct MenuItem {
    int id = 0;               // stable, non-zero for selectable items
    juce::String text;
    juce::Colour colour;      // transparent = default text colour
    bool header = false;      // section heading (not selectable)
    bool enabled = true;
    bool ticked = false;      // current value
    juce::String hint;        // small right-aligned detail text
};

class MenuOverlay final : public juce::Component {
public:
    struct Options {
        juce::Rectangle<int> anchor;     // place below (or above) this rectangle (overlay coordinates)
        bool atPoint = false;            // ... or beside this point (right-click lists)
        juce::Point<int> point;
        int minWidth = 150;
        int maxVisibleRows = 16;
        int rowHeight = 24;
    };
    using Callback = std::function<void(int id)>;  // 0 = cancelled

    MenuOverlay();
    static MenuOverlay* find(juce::Component* anyChildOfEditor);

    void open(std::vector<MenuItem> items, const Options& o, Callback cb, juce::Component* owner);
    void close(int result);
    bool isOpen() const { return open_; }
    bool isOpenFor(const juce::Component* owner) const { return open_ && owner_.getComponent() == owner; }
    // The owner forwards the mouse-up of its opening press: releasing over an item after dragging selects it.
    void ownerMouseUp(const juce::MouseEvent& eInOwner);
    int highlighted() const { return hot_; }
    juce::Rectangle<int> listBounds() const { return list_; }
    juce::Rectangle<int> rowBounds(int itemIndex) const;  // overlay coordinates (for tests)
    int indexOfText(const juce::String& t) const {  // tests
        for (size_t i = 0; i < items_.size(); ++i) if (items_[i].text == t && !items_[i].header) return static_cast<int>(i);
        return -1;
    }
    int itemCount() const { return static_cast<int>(items_.size()); }

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void parentSizeChanged() override;
    bool hitTest(int, int) override { return open_; }

private:
    int itemAt(juce::Point<int> p) const;
    void moveHot(int dir);
    void ensureVisible(int index);
    bool selectable(int i) const;

    std::vector<MenuItem> items_;
    Options opts_;
    Callback cb_;
    juce::Component::SafePointer<juce::Component> owner_, previousFocus_;
    juce::Rectangle<int> list_;
    juce::Point<int> openPoint_;
    bool open_ = false;
    int hot_ = -1, pressed_ = -1, scroll_ = 0, visibleRows_ = 0;
    double openedMs_ = 0.0;
    float wheelAcc_ = 0.0f;
    juce::String typed_;
    double typedMs_ = 0.0;
};

// A compact dropdown: current value as text with a small chevron; opens MenuOverlay on press.
class Dropdown final : public juce::Component, public juce::SettableTooltipClient {
public:
    Dropdown();
    void setItems(const juce::StringArray& items);
    void setSelectedIndex(int index, bool notify);
    int getSelectedIndex() const { return selected_; }
    void setAccent(juce::Colour c) { accent_ = c; repaint(); }
    std::function<void(int index)> onChange;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    void showMenu();
    void focusGained(FocusChangeType c) override { keyFocus_ = c == focusChangedByTabKey; repaint(); }
    void focusLost(FocusChangeType) override { keyFocus_ = false; repaint(); }

private:
    juce::StringArray items_;
    int selected_ = -1;
    juce::Colour accent_;
    bool keyFocus_ = false;
};

// A row of text segments for short choice lists (2-5 options): one click, no popup.
class Segmented final : public juce::Component, public juce::SettableTooltipClient {
public:
    Segmented();
    void setItems(const juce::StringArray& items);
    void setSelectedIndex(int index, bool notify);
    int getSelectedIndex() const { return selected_; }
    void setAccent(juce::Colour c) { accent_ = c; repaint(); }
    std::function<void(int index)> onChange;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    bool keyPressed(const juce::KeyPress&) override;
    static bool suits(const juce::StringArray& items, int width);
    void focusGained(FocusChangeType c) override { keyFocus_ = c == focusChangedByTabKey; repaint(); }
    void focusLost(FocusChangeType) override { keyFocus_ = false; repaint(); }

private:
    bool keyFocus_ = false;
    int segmentAt(int x) const;
    juce::StringArray items_;
    int selected_ = -1;
    juce::Colour accent_;
};

}  // namespace ih1::plugin
