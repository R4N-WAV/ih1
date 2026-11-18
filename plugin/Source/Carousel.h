// FX carousel (UI sketch): selected name large, neighbours faded above/below, up/down arrows,
// mouse wheel, keyboard arrows, type-to-jump, click a neighbour to select it.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace ih1::plugin {

class Carousel final : public juce::Component, private juce::Timer {
public:
    Carousel();
    void setSelected(int typeIndex);  // registry index or -1
    std::function<void(int typeIndex)> onSelect;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    bool keyPressed(const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void step(int dir);
    int orderPosOf(int typeIndex) const;
    juce::Rectangle<int> nameArea() const;

    std::vector<int> order_;
    int selected_ = -1;   // registry index
    int lastPos_ = 0;     // last carousel position (kept when nothing is selected)
    float anim_ = 0.0f;   // animated (continuous) position
    float target_ = 0.0f;
    double lastWheelMs_ = 0.0;
    float wheelAcc_ = 0.0f;
    juce::String typed_;
    double typedMs_ = 0.0;
    juce::ShapeButton up_{"up", juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack};
    juce::ShapeButton down_{"down", juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack};
};

}  // namespace ih1::plugin
