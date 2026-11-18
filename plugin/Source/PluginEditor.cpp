#include "PluginEditor.h"

#include "PluginProcessor.h"

namespace ih1::plugin {

Editor::Editor(Processor& p)
    : AudioProcessorEditor(p), proc_(p), renderer_(p, gl_), settings_(p), formation_(p, renderer_) {
    setLookAndFeel(&lnf_);
    juce::LookAndFeel::setDefaultLookAndFeel(&lnf_);

    carousel_.onSelect = [this](int type) { proc_.uiSelect(type); };
    addAndMakeVisible(carousel_);
    addAndMakeVisible(settings_);
    addAndMakeVisible(formation_);

    commit_.getProperties().set("primary", true);
    commit_.getProperties().set("fontSize", 16.0);
    reset_.getProperties().set("fontSize", 14.0);
    commit_.setTooltip("Add the staged effect, with its current settings, to the end of the committed chain. Committed effects can be moved or deleted but never edited.");
    reset_.setTooltip("Return the staged effect's settings to their defaults. Nothing already committed is touched.");
    commit_.onClick = [this] {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now < commitLockMs_) return;  // guards against a double-click committing twice
        commitLockMs_ = now + 350.0;
        juce::String name;
        {
            const juce::ScopedLock sl(proc_.sessionLock());
            const int sel = proc_.session().selected();
            if (sel >= 0) name = fxRegistry()[static_cast<size_t>(sel)].name;
        }
        if (proc_.uiCommit()) formation_.showToast(name + " joined the formation");
        else if (name.isNotEmpty()) formation_.showToast("The chain is full (" + juce::String(kMaxChain) + " effects)");
    };
    reset_.onClick = [this] { proc_.uiReset(); };
    presets_.setTooltip("Save or load a committed chain (user presets store the committed chain only).");
    presets_.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(commit_);
    addAndMakeVisible(reset_);
    addAndMakeVisible(presets_);

    if (juce::SystemStats::getEnvironmentVariable("IH1_NO_OPENGL", {}).isNotEmpty()) {
        formation_.setSoftwareFallback(true);
    } else {
    gl_.setRenderer(&renderer_);
    gl_.setPreferredVersion(juce::OpenGLContext::Version{3, 2});
    gl_.setComponentPaintingEnabled(true);
    gl_.setContinuousRepainting(true);
    gl_.attachTo(*this);
    }

    setResizable(true, true);
    setResizeLimits(980, 620, 2560, 1600);
    setSize(1280, 780);
    startTimerHz(30);
}

Editor::~Editor() {
    stopTimer();
    if (gl_.isAttached()) gl_.detach();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void Editor::paint(juce::Graphics& g) {
    // only the left column is opaque; the formation area is drawn by OpenGL underneath
    g.setColour(Colours::background);
    g.fillRect(left_);
    g.setColour(Colours::border);
    g.fillRect(left_.getRight(), 0, 1, getHeight());
    g.setColour(Colours::text);
    g.setFont(LookAndFeel::font(20.0f, true));
    g.drawText("IH1", left_.reduced(18, 0).withHeight(46).withY(10), juce::Justification::centredLeft);
}

void Editor::resized() {
    const int lw = juce::jlimit(360, 470, static_cast<int>(getWidth() * 0.33));
    left_ = getLocalBounds().removeFromLeft(lw);
    auto r = left_.reduced(18, 12);
    auto header = r.removeFromTop(44);
    presets_.setBounds(header.removeFromRight(96).withSizeKeepingCentre(96, 30));
    r.removeFromTop(4);
    carousel_.setBounds(r.removeFromTop(juce::jlimit(170, 240, getHeight() / 4)));
    r.removeFromTop(10);
    auto buttons = r.removeFromBottom(46);
    commit_.setBounds(buttons.removeFromLeft(buttons.getWidth() / 2 - 6));
    buttons.removeFromLeft(12);
    reset_.setBounds(buttons);
    r.removeFromBottom(14);
    settings_.setBounds(r);
    const auto field = getLocalBounds().withTrimmedLeft(lw + 1);
    formation_.setBounds(field);
    renderer_.setArea(field, getHeight());
}

void Editor::timerCallback() {
    if (renderer_.glFailed() && gl_.isAttached()) {
        // OpenGL 3.2 unavailable (old driver, VM, remote desktop): fall back to software painting
        gl_.detach();
        formation_.setSoftwareFallback(true);
        repaint();
    }
    if (proc_.revision() != seenRevision_) {
        seenRevision_ = proc_.revision();
        int sel, chain;
        {
            const juce::ScopedLock sl(proc_.sessionLock());
            sel = proc_.session().selected();
            chain = static_cast<int>(proc_.session().chain().size());
        }
        carousel_.setSelected(sel);
        commit_.setEnabled(sel >= 0 && chain < kMaxChain);
        reset_.setEnabled(sel >= 0);
        settings_.sync();
    }
}

void Editor::showPresetMenu() {
    juce::PopupMenu m;
    m.addItem(1, "Save committed chain as preset...");
    m.addItem(2, "Load chain preset...");
    m.addSeparator();
    m.addItem(3, "Clear committed chain...");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presets_), [this](int r) {
        const auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("IH1").getChildFile("Presets");
        dir.createDirectory();
        if (r == 1) {
            chooser_ = std::make_unique<juce::FileChooser>("Save chain preset", dir.getChildFile("My Chain.ih1chain"), "*.ih1chain");
            chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this](const juce::FileChooser& fc) {
                                      const auto f = fc.getResult();
                                      if (f == juce::File()) return;
                                      formation_.showToast(proc_.savePreset(f.withFileExtension("ih1chain")) ? "Preset saved" : "Could not save the preset");
                                  });
        } else if (r == 2) {
            chooser_ = std::make_unique<juce::FileChooser>("Load chain preset", dir, "*.ih1chain");
            chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
                const auto f = fc.getResult();
                if (f == juce::File()) return;
                juce::String msg;
                const bool ok = proc_.loadPreset(f, msg);
                formation_.showToast(ok ? (msg.isEmpty() ? "Preset loaded" : "Loaded with warnings: " + msg) : "Could not load: " + msg);
            });
        } else if (r == 3) {
            auto opts = juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon).withTitle("Clear committed chain?")
                            .withMessage("This removes every committed effect. The staged effect and its settings are kept.")
                            .withButton("Clear", 1).withButton("Cancel", 0).withAssociatedComponent(this);
            juce::AlertWindow::showAsync(opts, [this](int result) { if (result == 1) proc_.uiClearChain(); });
        }
    });
}

}  // namespace ih1::plugin
