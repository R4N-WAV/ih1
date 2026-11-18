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

#include "ih1/visual/Formation.h"
#include "ih1/visual/Raster.h"

namespace ih1::plugin {

class Processor;

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

    std::unique_ptr<juce::OpenGLShaderProgram> lineProg_, decayProg_, copyProg_, blurProg_, compProg_;
    unsigned vao_ = 0, lineVbo_ = 0, quadVbo_ = 0;
    Target accum_, bloomA_, bloomB_;
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
    juce::Rectangle<float> itemBounds(int chainIndex) const {  // tests
        for (const auto& it : items_) if (it.index == chainIndex) return it.r;
        return {};
    }
    juce::Rectangle<float> deleteZone() const { return deleteZone_; }
    juce::Component* confirmDialog() const;

private:
    struct Item { uint64_t id; int index; juce::String name; juce::Colour colour; juce::Rectangle<float> r; };
    class Confirm;

    void timerCallback() override;
    void syncChain();
    void layoutStrip();
    int hitItem(juce::Point<float>) const;
    int insertionFor(juce::Point<float>) const;  // gap index 0..n
    void openConfirm(uint64_t id, const juce::String& name);
    void closeConfirm();
    void cancelDrag();
    juce::String describe(uint64_t id);
    void renderSoftware();

    Processor& proc_;
    FormationRenderer& renderer_;
    uint64_t seenRevision_ = 0;
    std::vector<Item> items_;
    juce::Rectangle<float> strip_, deleteZone_;
    // drag state (the dragged instance is tracked by id, never by a stale index)
    uint64_t dragId_ = 0;
    bool pressed_ = false, dragging_ = false, deleteHover_ = false;
    juce::Point<float> dragPos_, downPos_;
    int insertion_ = -1;
    juce::String toast_;
    double toastMs_ = 0.0;
    std::unique_ptr<Confirm> confirm_;
    juce::Slider inGain_, mix_, outGain_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAtt_, mixAtt_, outAtt_;
    // software fallback (no OpenGL 3.2)
    bool software_ = false;
    std::unique_ptr<visual::Formation> cpuForm_;
    visual::Canvas canvas_;
    std::vector<uint8_t> rgb_;
    juce::Image image_;
    std::vector<visual::InstanceInput> chain_;
    std::vector<visual::TransferRequest> softTransfers_;
};

// Shared: per-instance colours (type identity + duplicate shade by instance id order, so reordering never
// recolours anything) and audio levels from telemetry. Message thread.
std::vector<visual::InstanceInput> chainInputs(Processor& p);
void fillLevels(Processor& p, std::vector<visual::InstanceInput>& chain);

}  // namespace ih1::plugin
