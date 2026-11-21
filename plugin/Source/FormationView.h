// Formation field.
//
// FormationRenderer runs the ih1::visual::Formation simulation on the OpenGL render thread and draws it into a
// float accumulation target, then bloom and a hue-preserving tone map onto a pure black field. Resource rules
// (V1 feedback 4.8, the FL Studio resize crash):
//   * every GL call happens in the OpenGLRenderer callbacks, with JUCE's context current;
//   * the message thread only publishes plain values (area, scale, chain, commit events) under a SpinLock;
//   * render targets are reallocated only once the requested size has been stable for 150 ms (a live resize
//     keeps drawing into the existing targets, stretched), are capped at ~2.4 megapixels and 4096 px a side,
//     and are checked for framebuffer completeness; any failure switches the editor to the software formation;
//   * zero, negative and non-finite sizes are never passed to GL.
//
// FormationView is the transparent overlay above it: the chain strip (reorder by drag), the plain red DELETE
// drop target at the upper-left while dragging, the delete confirmation, read-only details on hover, global
// IN / MIX / OUT, and the software fallback image. It never draws FX names over the artwork (4.3) and knows
// nothing about the staged FX (4.2).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>

#include <atomic>
#include <memory>
#include <vector>

#include "ih1/engine/Engine.h"
#include "ih1/visual/FieldVolume.h"
#include "ih1/visual/Formation.h"
#include "ih1/visual/Raster.h"
#include "PluginProcessor.h"

namespace ih1::plugin {

class Processor;

// Audio -> visual analysis bridge (V2.1 9.6). Reads only the engine's telemetry atomics (never audio buffers)
// at frame rate and turns them into smoothed, bounded features: energy, onset strength, bands, balance.
struct AudioAnalyser {
    float rms = 0.0f, slow = 0.0f, peakEnv = 0.0f, low = 0.0f, mid = 0.0f, high = 0.0f, lastOnset = 1.0f;
    void update(const Telemetry& t, float dt, visual::AudioFeatures& out);
};

class FormationRenderer final : public juce::OpenGLRenderer {
public:
    FormationRenderer(Processor& p, juce::OpenGLContext& ctx);
    ~FormationRenderer() override;
    // message thread
    void setArea(juce::Rectangle<int> areaInEditor, int editorHeight);
    void setChain(std::vector<visual::InstanceInput> chain);
    void requestTransfer(uint64_t id, float originY);
    bool glFailed() const { return failed_.load(); }
    double lastSimMs() const { return simMs_.load(); }
    double lastVolumeMs() const { return volMs_.load(); }  // last timed volume pass (GPU), tests / diagnostics
    int volumeLevel() const { return volLevel_; }

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    struct Target { unsigned fbo = 0, tex = 0; int w = 0, h = 0; };
    bool ensureTarget(Target& t, int w, int h);
    void freeTarget(Target& t);
    void freeAll();
    void drawQuad();

    Processor& proc_;
    juce::OpenGLContext& ctx_;
    visual::Formation formation_{visual::Quality::Medium};
    juce::SpinLock lock_;
    juce::Rectangle<int> area_;
    int editorHeight_ = 0;
    std::vector<visual::InstanceInput> chain_;
    std::vector<visual::TransferRequest> transfers_;

    std::unique_ptr<juce::OpenGLShaderProgram> volProg_, passProg_, copyProg_, blurProg_, compProg_;
    AudioAnalyser analyser_;
    std::vector<float> glowPos_, glowCol_;
    unsigned vao_ = 0, lineVbo_ = 0, quadVbo_ = 0;
    Target accum_, volA_, bloomA_, bloomB_;  // V3: accum_ = temporal history of the half-resolution volume (volA_)
    unsigned frame_ = 0;
    int volLevel_ = 0;              // adaptive volume quality (0 = best)
    std::atomic<double> volMs_{0.0};
    int wantW_ = 0, wantH_ = 0;
    double sizeChangedMs_ = 0.0;
    std::vector<float> verts_;
    double lastMs_ = 0.0;
    int slowFrames_ = 0;
    std::atomic<bool> failed_{false};
    std::atomic<double> simMs_{0.0};
};

class FormationView final : public juce::Component, public juce::TooltipClient, private juce::Timer {
public:
    FormationView(Processor& p, FormationRenderer& r);
    ~FormationView() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    juce::String getTooltip() override;
    void showToast(const juce::String& text);  // never used for FX names
    bool dialogOpen() const { return confirm_ != nullptr; }
    void setSoftwareFallback(bool on);  // used when OpenGL 3.2 is unavailable or fails
    bool softwareFallback() const { return software_; }
    // COMMIT accepted: animate the transfer of this instance into the formation.
    void requestTransfer(uint64_t id, float originYInField);
    juce::Rectangle<int> artworkArea() const;  // the part of this component the formation is drawn in
    static constexpr int kStripH = 46;
    bool itemEnabled(int chainIndex) const {  // tests: the enabled state the strip draws
        for (const auto& it : items_) if (it.index == chainIndex) return it.enabled;
        return false;
    }
    juce::Rectangle<float> itemBounds(int chainIndex) const {  // tests
        for (const auto& it : items_) if (it.index == chainIndex) return it.r;
        return {};
    }
    juce::Rectangle<float> deleteZone() const { return deleteZone_; }
    juce::Component* confirmDialog() const;
    // Phase 15 / 16 (tests): the discreet chain utility and the clear-chain confirmation
    juce::Rectangle<float> chainMenuBounds() const { return chainMenu_; }
    juce::Rectangle<float> meterBounds() const { return meter_; }
    void showChainMenu();
    void openClearConfirm();
    juce::Button* confirmButton() const;
    juce::Button* cancelButton() const;

private:
    struct Item { uint64_t id; int index; juce::String name; juce::Colour colour; juce::Rectangle<float> r; bool enabled = true; };
    class Confirm;

    void timerCallback() override;
    void syncChain();
    void layoutStrip();
    int hitItem(juce::Point<float>) const;
    int insertionFor(juce::Point<float>) const;  // gap index 0..n
    void openConfirm(uint64_t id, const juce::String& name);
    void closeConfirm();
    void mountConfirm();
    void cancelDrag();
    juce::String describe(uint64_t id);
    void renderSoftware();
    void refreshLatencyStatus();

    Processor& proc_;
    FormationRenderer& renderer_;
    uint64_t seenRevision_ = 0;
    std::vector<Item> items_;
    juce::Rectangle<float> strip_, deleteZone_, chainMenu_, status_, meter_;
    Processor::LatencyStatus latency_;
    Processor::SafetyStatus safety_;
    float meterPeak_ = 0.0f, meterGr_ = 0.0f;  // UI-smoothed output peak (linear) and safety gain reduction (dB)
    bool riskWarned_ = false;
    // drag state (the dragged instance is tracked by id, never by a stale index)
    uint64_t dragId_ = 0;
    bool pressed_ = false, dragging_ = false, deleteHover_ = false;
    juce::Point<float> dragPos_, downPos_;
    int insertion_ = -1;
    juce::String toast_;
    double toastMs_ = 0.0;
    std::unique_ptr<Confirm> confirm_;
    juce::Slider inGain_, mix_, outGain_;
    // V4 preset macros (shown only for macros the recalled preset defines)
    std::array<juce::Slider, kNumPresetMacros> pmacro_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, kNumPresetMacros> pmacroAtt_;
    std::array<juce::String, kNumPresetMacros> pmacroName_;
    void syncMacros();
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAtt_, mixAtt_, outAtt_;
    // software fallback (no OpenGL 3.2)
    bool software_ = false;
    std::unique_ptr<visual::Formation> cpuForm_;
    visual::Canvas canvas_;
    std::vector<float> softLin_, softHist_;  // V3 software volume (linear) and its temporal average
    // V3: the software volume renders on its own thread; the message thread only posts inputs and blits the result
    class SoftThread;
    std::unique_ptr<SoftThread> softThread_;
    std::vector<uint8_t> rgb_;
    juce::Image image_;
    std::vector<visual::InstanceInput> chain_;
    std::vector<visual::TransferRequest> softTransfers_;
    AudioAnalyser softAnalyser_;
};

// Shared: per-instance colours (type identity + duplicate shade by instance id order, so reordering never
// recolours anything) and audio levels from telemetry. Message thread.
std::vector<visual::InstanceInput> chainInputs(Processor& p);
void fillLevels(Processor& p, std::vector<visual::InstanceInput>& chain);

}  // namespace ih1::plugin
