// FX carousel (UI sketch): the selected name large, neighbours faded above and below, up/down arrows, mouse
// wheel, arrow keys, type-to-jump, click a neighbour to select it. Each name is drawn in its FX identity
// colour (the same colour it takes in the formation). With nothing staged the centre reads "None".
// Right-click opens the full catalogue beside the pointer (V1 feedback 4.5, 4.11, 4.12, 4.13).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace ih1::plugin {

class Carousel final : public juce::Component, private juce::Timer {
public:
    static constexpr int kNoneMenuId = 1 << 20;
    static constexpr int kNoneEntry = -1;  // the None entry in the carousel ring (== Session::kNone)  // menu id of the "None" row (FX rows use registry index + 1)
    Carousel();
    void setSelected(int typeIndex);  // registry index or -1
    int selected() const { return selected_; }  // registry index or kNoneEntry
    std::function<void(int typeIndex)> onSelect;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    bool keyPressed(const juce::KeyPress&) override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override { keyFocus_ = false; repaint(); }
    void showCatalogueMenu(juce::Point<int> atLocal);

private:
    void timerCallback() override;
    void step(int dir);
    int orderPosOf(int typeIndex) const;
    juce::Rectangle<int> nameArea() const;

    std::vector<int> order_;
    int selected_ = -1;   // registry index
    float anim_ = 0.0f;   // animated (continuous) position
    float target_ = 0.0f;
    double lastStepMs_ = 0.0;
    float wheelAcc_ = 0.0f;
    bool keyFocus_ = false;
    bool rightDown_ = false;
    juce::String typed_;
    double typedMs_ = 0.0;
    juce::ShapeButton up_{"up", juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack};
    juce::ShapeButton down_{"down", juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack};
};

}  // namespace ih1::plugin
