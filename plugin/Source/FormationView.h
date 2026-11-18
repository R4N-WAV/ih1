// Formation field: OpenGL renderer (runs the ih1::visual::Formation simulation on the GL thread) and a
// transparent overlay component for interaction (chain lane, drag reorder, DELETE target + confirmation,
// read-only hover info, toasts, global gain/mix).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>

#include <atomic>
#include <vector>

#include "ih1/visual/Formation.h"

namespace ih1::plugin {

class Processor;

class FormationRenderer final : public juce::OpenGLRenderer {
public:
    FormationRenderer(Processor& p, juce::OpenGLContext& ctx);
    ~FormationRenderer() override;
    // message thread
    void setArea(juce::Rectangle<int> areaInEditor, int editorHeight);
    void setChain(std::vector<visual::InstanceInput> chain, bool hasStaged);
    std::vector<visual::CoreInfo> cores();
    bool glFailed() const { return failed_.load(); }

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    struct Target { unsigned fbo = 0, tex = 0; int w = 0, h = 0; };
    void ensureTarget(Target& t, int w, int h);
    void freeTarget(Target& t);
    void drawQuad();

    Processor& proc_;
    juce::OpenGLContext& ctx_;
    visual::Formation formation_{26000};
    juce::SpinLock lock_;
    juce::Rectangle<int> area_;
    int editorHeight_ = 0;
    std::vector<visual::InstanceInput> chain_;
    bool hasStaged_ = false;
    std::vector<visual::CoreInfo> cores_;

    std::unique_ptr<juce::OpenGLShaderProgram> lineProg_, decayProg_, copyProg_, blurProg_, compProg_;
    unsigned vao_ = 0, lineVbo_ = 0, quadVbo_ = 0;
    Target accum_, bloomA_, bloomB_;
    std::vector<float> verts_;
    double lastMs_ = 0.0;
    std::atomic<bool> failed_{false};
};

class FormationView final : public juce::Component, public juce::TooltipClient, private juce::Timer {
public:
    FormationView(Processor& p, FormationRenderer& r);
    ~FormationView() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    juce::String getTooltip() override;
    void showToast(const juce::String& text);
    bool dialogOpen() const { return confirm_ != nullptr; }
    void setSoftwareFallback(bool on);  // used when OpenGL 3.2 is unavailable

private:
    struct Pill { uint64_t id; int index; juce::String name; juce::Colour colour; juce::Rectangle<float> r; };
    struct Label { uint64_t id; int index; juce::String name; juce::Colour colour; juce::Rectangle<float> r; };
    class Confirm;

    void timerCallback() override;
    void syncChain();
    void layoutLane();
    int hitPill(juce::Point<float>) const;
    int hitLabel(juce::Point<float>) const;
    int insertionFor(juce::Point<float>) const;  // gap index 0..n
    void openConfirm(uint64_t id, const juce::String& name);
    void closeConfirm();
    juce::String describe(int chainIndex);

    Processor& proc_;
    FormationRenderer& renderer_;
    uint64_t seenRevision_ = 0;
    std::vector<Pill> pills_;
    std::vector<Label> labels_;
    juce::Rectangle<float> lane_, deleteZone_;
    // drag state
    int dragFrom_ = -1;
    uint64_t dragId_ = 0;
    bool dragging_ = false, deleteHover_ = false;
    juce::Point<float> dragPos_, downPos_;
    int insertion_ = -1;
    // toast
    juce::String toast_;
    double toastMs_ = 0.0;
    std::unique_ptr<Confirm> confirm_;
    // global controls
    juce::Slider inGain_, mix_, outGain_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAtt_, mixAtt_, outAtt_;
    bool hasStaged_ = false;
    juce::String stagedName_;
    // software fallback (no OpenGL 3.2)
    void renderSoftware();
    bool software_ = false;
    std::unique_ptr<visual::Formation> cpuForm_;
    std::vector<float> acc_;
    juce::Image image_;
    std::vector<visual::InstanceInput> chain_;
    std::vector<visual::CoreInfo> softCores_;
};

}  // namespace ih1::plugin
