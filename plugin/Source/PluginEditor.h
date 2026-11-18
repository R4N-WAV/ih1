// IH1 editor: left control column (carousel, staged settings, COMMIT/RESET) and the formation field.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>

#include "Carousel.h"
#include "FormationView.h"
#include "Ih1LookAndFeel.h"
#include "SettingsPanel.h"

namespace ih1::plugin {

class Processor;

class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit Editor(Processor& p);
    ~Editor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPresetMenu();

    Processor& proc_;
    LookAndFeel lnf_;
    juce::OpenGLContext gl_;
    FormationRenderer renderer_;
    Carousel carousel_;
    SettingsPanel settings_;
    FormationView formation_;
    juce::TextButton commit_{"COMMIT"}, reset_{"RESET"}, presets_{"Presets"};
    juce::TooltipWindow tooltips_{this, 600};
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::Rectangle<int> left_;
    double commitLockMs_ = 0.0;
    uint64_t seenRevision_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Editor)
};

}  // namespace ih1::plugin
