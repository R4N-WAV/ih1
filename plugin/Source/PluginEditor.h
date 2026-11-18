// IH1 editor: left control column (wordmark + preset box, carousel, staged settings, COMMIT / RESET) and the
// formation field. One MenuOverlay and the preset browser sit on top as child components.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>

#include "Carousel.h"
#include "FormationView.h"
#include "Ih1LookAndFeel.h"
#include "PresetBrowser.h"
#include "SettingsPanel.h"
#include "Widgets.h"

namespace ih1::plugin {

class Processor;

class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit Editor(Processor& p);
    ~Editor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

    // default first-run size (logical px) for a display user area: ~1500 x 900, clamped to fit
    static juce::Point<int> defaultSize(juce::Rectangle<int> userArea);
    static constexpr int kMinW = 1120, kMinH = 700, kMaxW = 3840, kMaxH = 2400;

    // test hooks (ih1_hosttest)
    Carousel& carousel() { return carousel_; }
    SettingsPanel& settings() { return settings_; }
    MenuOverlay& menu() { return menu_; }
    FormationView& formation() { return formation_; }
    bool usingSoftwareFormation() const { return formation_.softwareFallback(); }

private:
    void timerCallback() override;
    void doCommit();

    Processor& proc_;
    LookAndFeel lnf_;  // declared first: destroyed last
    juce::OpenGLContext gl_;
    FormationRenderer renderer_;
    Carousel carousel_;
    SettingsPanel settings_;
    FormationView formation_;
    PresetBar presetBar_;
    juce::TextButton commit_{"COMMIT"}, reset_{"RESET"};
    MenuOverlay menu_;
    PresetBrowser browser_;
    juce::TooltipWindow tooltips_{this, 650};
    juce::Rectangle<int> left_, header_;
    double commitLockMs_ = 0.0;
    uint64_t seenRevision_ = 0;
    bool glAttached_ = false;
};

}  // namespace ih1::plugin
