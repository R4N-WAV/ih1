#include "FormationView.h"

#include <map>

#include "Ih1LookAndFeel.h"
#include "PluginProcessor.h"

namespace ih1::plugin {

using namespace juce::gl;

namespace {
constexpr float kExposure = 2.2f;
constexpr float kBloomGain = 1.6f;
constexpr double kMaxPixels = 2.4e6;
constexpr int kMaxSide = 4096;
constexpr double kResizeSettleMs = 150.0;

// =============================================================================================== GLSL
const char* kLineVS = R"(#version 150
in vec2 aPos; in vec3 aCol; uniform float uAspect; out vec3 vCol;
void main() { vCol = aCol; gl_Position = vec4(aPos.x / uAspect * 2.0 - 1.0, 1.0 - aPos.y * 2.0, 0.0, 1.0); })";
const char* kLineFS = R"(#version 150
in vec3 vCol; uniform float uGain; out vec4 o;
void main() { o = vec4(vCol * uGain, 1.0); })";
const char* kQuadVS = R"(#version 150
in vec2 aPos; out vec2 vUv;
void main() { vUv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); })";
const char* kDecayFS = R"(#version 150
uniform float uDecay; out vec4 o;
void main() { o = vec4(uDecay, uDecay, uDecay, 1.0); })";
const char* kCopyFS = R"(#version 150
uniform sampler2D uTex; uniform vec2 uTexel; in vec2 vUv; out vec4 o;
void main() { // 4-tap box downsample
  o = 0.25 * (texture(uTex, vUv + uTexel * vec2(-1.0, -1.0)) + texture(uTex, vUv + uTexel * vec2(1.0, -1.0))
            + texture(uTex, vUv + uTexel * vec2(-1.0, 1.0)) + texture(uTex, vUv + uTexel * vec2(1.0, 1.0))); })";
const char* kBlurFS = R"(#version 150
uniform sampler2D uTex; uniform vec2 uDir; in vec2 vUv; out vec4 o;
void main() {
  float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
  vec4 s = texture(uTex, vUv) * w[0];
  for (int i = 1; i < 5; ++i) { s += texture(uTex, vUv + uDir * float(i) * 1.5) * w[i]; s += texture(uTex, vUv - uDir * float(i) * 1.5) * w[i]; }
  o = s; })";
// Mirrors ih1::visual::toneMap (core/src/visual/Formation.cpp): hue-preserving, black stays exactly black.
const char* kCompFS = R"(#version 150
uniform sampler2D uAccum; uniform sampler2D uBloom; uniform float uExposure; uniform float uBloomGain; in vec2 vUv; out vec4 o;
vec3 tone(vec3 a) {
  a = max(a, vec3(0.0));
  float m = max(a.r, max(a.g, a.b));
  if (m <= 0.0) return vec3(0.0);
  float t = 1.0 - exp(-m * uExposure);
  vec3 c = a * (t / m);
  float u = clamp((t - 0.80) / 0.20, 0.0, 1.0);
  float w = u * u * (3.0 - 2.0 * u) * 0.55;
  return mix(c, vec3(0.95, 0.91, 1.0) * t, w);
}
void main() {
  vec3 acc = texture(uAccum, vUv).rgb + uBloomGain * texture(uBloom, vUv).rgb;
  o = vec4(tone(acc), 1.0);
})";

std::unique_ptr<juce::OpenGLShaderProgram> makeProgram(juce::OpenGLContext& ctx, const char* vs, const char* fs) {
    auto p = std::make_unique<juce::OpenGLShaderProgram>(ctx);
    if (!p->addVertexShader(vs) || !p->addFragmentShader(fs)) { DBG(p->getLastError()); return nullptr; }
    glBindAttribLocation(p->getProgramID(), 0, "aPos");
    glBindAttribLocation(p->getProgramID(), 1, "aCol");
    if (!p->link()) { DBG(p->getLastError()); return nullptr; }
    return p;
}

// Internal render size for a field of w x h physical pixels: bounded memory whatever the host asks for.
void cappedSize(int w, int h, int& tw, int& th) {
    w = std::max(1, w);
    h = std::max(1, h);
    double s = std::min({1.0, std::sqrt(kMaxPixels / (static_cast<double>(w) * h)), static_cast<double>(kMaxSide) / w, static_cast<double>(kMaxSide) / h});
    tw = std::max(1, static_cast<int>(w * s));
    th = std::max(1, static_cast<int>(h * s));
}
}  // namespace

// =============================================================================================== chain inputs
std::vector<visual::InstanceInput> chainInputs(Processor& proc) {
    std::vector<visual::InstanceInput> out;
    const juce::ScopedLock sl(proc.sessionLock());
    const auto& c = proc.session().chain();
    // duplicate shade = rank of this instance's id among same-type instances (stable under reorder)
    std::map<int, std::vector<uint64_t>> byType;
    for (const auto& ci : c) byType[ci.typeIndex()].push_back(ci.id());
    for (auto& kv : byType) std::sort(kv.second.begin(), kv.second.end());
    for (const auto& ci : c) {
        const auto& ids = byType[ci.typeIndex()];
        const int shade = static_cast<int>(std::find(ids.begin(), ids.end(), ci.id()) - ids.begin());
        out.push_back({ci.id(), visual::fxColour(ci.descriptor().typeId, shade), ci.visualSeed(), 0.0f});
    }
    return out;
}

void fillLevels(Processor& proc, std::vector<visual::InstanceInput>& chain) {
    // committed-chain telemetry only: the staged FX never influences the formation
    auto& tel = proc.telemetry();
    const int n = std::clamp(tel.chainSize.load(), 0, kMaxChain);
    for (auto& ii : chain)
        for (int s = 0; s < n; ++s)
            if (tel.slotIds[static_cast<size_t>(s)].load() == ii.id) ii.level = tel.slotRms[static_cast<size_t>(s)].load();
}

// =============================================================================================== renderer
FormationRenderer::FormationRenderer(Processor& p, juce::OpenGLContext& ctx) : proc_(p), ctx_(ctx) {}
FormationRenderer::~FormationRenderer() = default;

void FormationRenderer::setArea(juce::Rectangle<int> a, int editorHeight) {
    const juce::SpinLock::ScopedLockType sl(lock_);
    area_ = a;
    editorHeight_ = editorHeight;
}

void FormationRenderer::setChain(std::vector<visual::InstanceInput> chain) {
    const juce::SpinLock::ScopedLockType sl(lock_);
    chain_ = std::move(chain);
}

void FormationRenderer::requestTransfer(uint64_t id, float originY) {
    const juce::SpinLock::ScopedLockType sl(lock_);
    if (transfers_.size() < 64) transfers_.push_back({id, originY});
}

void FormationRenderer::newOpenGLContextCreated() {
    // Machines without a real driver (VMs, remote desktop, CI) only offer GL 1.1: never call GL3 there.
    const auto v = ctx_.getVersion();
    if (v.major < 3 || (v.major == 3 && v.minor < 2) || glGenVertexArrays == nullptr || glCreateShader == nullptr ||
        glGenFramebuffers == nullptr || glBindAttribLocation == nullptr || glCheckFramebufferStatus == nullptr) {
        failed_ = true;
        return;
    }
    lineProg_ = makeProgram(ctx_, kLineVS, kLineFS);
    decayProg_ = makeProgram(ctx_, kQuadVS, kDecayFS);
    copyProg_ = makeProgram(ctx_, kQuadVS, kCopyFS);
    blurProg_ = makeProgram(ctx_, kQuadVS, kBlurFS);
    compProg_ = makeProgram(ctx_, kQuadVS, kCompFS);
    if (!lineProg_ || !decayProg_ || !copyProg_ || !blurProg_ || !compProg_) { failed_ = true; return; }
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &lineVbo_);
    glGenBuffers(1, &quadVbo_);
    const float quad[12] = {-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1};
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    verts_.reserve(400000);
    lastMs_ = juce::Time::getMillisecondCounterHiRes();
    wantW_ = wantH_ = 0;
}

bool FormationRenderer::ensureTarget(Target& t, int w, int h) {
    w = std::clamp(w, 1, kMaxSide);
    h = std::clamp(h, 1, kMaxSide);
    if (t.fbo && t.w == w && t.h == h) return true;
    freeTarget(t);
    glGenTextures(1, &t.tex);
    glBindTexture(GL_TEXTURE_2D, t.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &t.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE || glGetError() == GL_OUT_OF_MEMORY) {
        freeTarget(t);
        return false;
    }
    t.w = w;
    t.h = h;
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    return true;
}

void FormationRenderer::freeTarget(Target& t) {
    if (t.fbo) glDeleteFramebuffers(1, &t.fbo);
    if (t.tex) glDeleteTextures(1, &t.tex);
    t = Target{};
}

void FormationRenderer::freeAll() {
    freeTarget(accum_);
    freeTarget(bloomA_);
    freeTarget(bloomB_);
}

void FormationRenderer::drawQuad() {
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDisableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void FormationRenderer::renderOpenGL() {
    juce::OpenGLHelpers::clear(Colours::field);  // the whole surface starts pure black
    if (failed_ || !vao_) return;
    GLint screenFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &screenFbo);

    juce::Rectangle<int> area;
    int edH;
    visual::FrameInput in;
    {
        const juce::SpinLock::ScopedLockType sl(lock_);
        area = area_;
        edH = editorHeight_;
        in.chain = chain_;
        in.transfers.swap(transfers_);
    }
    const double scale = ctx_.getRenderingScale();
    if (area.isEmpty() || edH <= 0 || !std::isfinite(scale) || scale <= 0.0 || scale > 8.0) return;
    // screen viewport (physical pixels)
    const int x = juce::roundToInt(area.getX() * scale), w = juce::roundToInt(area.getWidth() * scale);
    const int h = juce::roundToInt(area.getHeight() * scale);
    const int y = juce::roundToInt((edH - area.getBottom()) * scale);
    if (w < 2 || h < 2 || w > 32768 || h > 32768) return;

    // ---- render-target size: capped, and coalesced while the host is resizing ----
    int tw, th;
    cappedSize(w, h, tw, th);
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (tw != wantW_ || th != wantH_) { wantW_ = tw; wantH_ = th; sizeChangedMs_ = now; }
    const bool haveTargets = accum_.fbo != 0;
    const bool settled = now - sizeChangedMs_ >= kResizeSettleMs;
    if (!haveTargets || (settled && (accum_.w != tw || accum_.h != th))) {
        if (!ensureTarget(accum_, tw, th) || !ensureTarget(bloomA_, std::max(1, tw / 4), std::max(1, th / 4)) ||
            !ensureTarget(bloomB_, std::max(1, tw / 4), std::max(1, th / 4))) {
            freeAll();
            failed_ = true;  // the editor switches to the software formation; audio is unaffected
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screenFbo));
            return;
        }
    }
    const int rw = accum_.w, rh = accum_.h;  // may differ from (tw, th) during a live resize: stretched

    // ---- simulation (committed chain + telemetry only) ----
    fillLevels(proc_, in.chain);
    in.dt = static_cast<float>(juce::jlimit(0.001, 0.1, (now - lastMs_) * 0.001));
    lastMs_ = now;
    in.aspect = static_cast<float>(w) / static_cast<float>(h);
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    formation_.update(in);
    const double sim = juce::Time::getMillisecondCounterHiRes() - t0;
    simMs_.store(sim);
    // adaptive workload: the visual gets cheaper on slow machines, never the audio
    if (sim > 12.0) { if (++slowFrames_ > 30 && formation_.quality() != visual::Quality::Low) { formation_.setQuality(visual::Quality::Low); slowFrames_ = 0; } }
    else slowFrames_ = 0;

    // ---- accumulate lines ----
    glBindVertexArray(vao_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, accum_.fbo);
    glViewport(0, 0, rw, rh);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);  // multiply: light persistence
    decayProg_->use();
    decayProg_->setUniform("uDecay", formation_.decay(in.dt));
    drawQuad();
    const auto& segs = formation_.segments();
    verts_.clear();
    for (const auto& s : segs) verts_.insert(verts_.end(), {s.x0, s.y0, s.r, s.g, s.b, s.x1, s.y1, s.r, s.g, s.b});
    if (!verts_.empty()) {
        glBlendFunc(GL_ONE, GL_ONE);  // additive light
        lineProg_->use();
        lineProg_->setUniform("uAspect", in.aspect);
        lineProg_->setUniform("uGain", static_cast<float>(std::sqrt(juce::jlimit(0.36, 2.6, 860.0 / rh))));
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts_.size() * sizeof(float)), verts_.data(), GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(verts_.size() / 5));
    }
    glDisable(GL_BLEND);

    // ---- bloom (quarter resolution): the smoky wisps around the strands ----
    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_FRAMEBUFFER, bloomA_.fbo);
    glViewport(0, 0, bloomA_.w, bloomA_.h);
    copyProg_->use();
    copyProg_->setUniform("uTex", 0);
    copyProg_->setUniform("uTexel", 1.0f / static_cast<float>(rw), 1.0f / static_cast<float>(rh));
    glBindTexture(GL_TEXTURE_2D, accum_.tex);
    drawQuad();
    blurProg_->use();
    blurProg_->setUniform("uTex", 0);
    for (int pass = 0; pass < 2; ++pass) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomB_.fbo);
        glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
        blurProg_->setUniform("uDir", 1.0f / static_cast<float>(bloomA_.w), 0.0f);
        drawQuad();
        glBindFramebuffer(GL_FRAMEBUFFER, bloomA_.fbo);
        glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
        blurProg_->setUniform("uDir", 0.0f, 1.0f / static_cast<float>(bloomA_.h));
        drawQuad();
    }

    // ---- composite to screen ----
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screenFbo));
    glViewport(x, y, w, h);
    compProg_->use();
    compProg_->setUniform("uAccum", 0);
    compProg_->setUniform("uBloom", 1);
    compProg_->setUniform("uExposure", kExposure);
    compProg_->setUniform("uBloomGain", kBloomGain);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, accum_.tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    drawQuad();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    if (glGetError() == GL_OUT_OF_MEMORY) { freeAll(); failed_ = true; }
}

void FormationRenderer::openGLContextClosing() {
    // called by JUCE with the context current, before it is destroyed (editor close, host hide/show)
    freeAll();
    if (lineVbo_) glDeleteBuffers(1, &lineVbo_);
    if (quadVbo_) glDeleteBuffers(1, &quadVbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    lineVbo_ = quadVbo_ = vao_ = 0;
    lineProg_.reset(); decayProg_.reset(); copyProg_.reset(); blurProg_.reset(); compProg_.reset();
    wantW_ = wantH_ = 0;
}

// =============================================================================================== confirm
class FormationView::Confirm final : public juce::Component {
public:
    Confirm(const juce::String& name, juce::Colour colour, std::function<void(bool)> done) : done_(std::move(done)), name_(name), colour_(colour) {
        cancel_.setButtonText("Cancel");
        del_.setButtonText("Delete");
        cancel_.getProperties().set("quiet", true);
        del_.getProperties().set("quiet", true);
        del_.setColour(juce::TextButton::textColourOffId, Colours::danger);
        cancel_.onClick = [this] { finish(false); };
        del_.onClick = [this] { finish(true); };
        addAndMakeVisible(cancel_);
        addAndMakeVisible(del_);
        setWantsKeyboardFocus(true);
        setTitle("Delete " + name);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::black.withAlpha(0.6f));
        g.setColour(Colours::panel);
        g.fillRect(box_);
        g.setColour(Colours::border);
        g.drawRect(box_.toFloat(), 1.0f);
        auto r = box_.reduced(22, 18);
        g.setFont(LookAndFeel::font(18.0f, true));
        g.setColour(Colours::text);
        juce::AttributedString t;
        t.append("Delete ", LookAndFeel::font(18.0f, true), Colours::text);
        t.append(name_, LookAndFeel::font(18.0f, true), readable(colour_));
        t.append("?", LookAndFeel::font(18.0f, true), Colours::text);
        t.draw(g, r.removeFromTop(28).toFloat());
        g.setColour(Colours::textDim);
        g.setFont(LookAndFeel::font(13.0f));
        g.drawFittedText("This removes this one " + name_ + " from the committed chain. Every other effect, including other " + name_ +
                             " instances, stays as it is.",
                         r.withHeight(40), juce::Justification::topLeft, 3);
    }
    void resized() override {
        box_ = getLocalBounds().withSizeKeepingCentre(std::min(430, getWidth() - 40), 150);
        auto b = box_.reduced(16, 14).removeFromBottom(30);
        del_.setBounds(b.removeFromRight(84));
        b.removeFromRight(8);
        cancel_.setBounds(b.removeFromRight(84));
    }
    void mouseDown(const juce::MouseEvent& e) override { if (!box_.contains(e.getPosition())) finish(false); }
    bool keyPressed(const juce::KeyPress& k) override {
        if (k == juce::KeyPress::escapeKey) { finish(false); return true; }
        if (k == juce::KeyPress::returnKey && del_.hasKeyboardFocus(false)) { finish(true); return true; }
        return true;  // swallow everything else while open
    }
    void parentHierarchyChanged() override { if (isShowing()) cancel_.grabKeyboardFocus(); }

private:
    void finish(bool del) {
        if (finished_) return;  // exactly once
        finished_ = true;
        done_(del);
    }
    std::function<void(bool)> done_;
    juce::String name_;
    juce::Colour colour_;
    juce::Rectangle<int> box_;
    juce::TextButton cancel_, del_;
    bool finished_ = false;
};

// =============================================================================================== overlay
FormationView::FormationView(Processor& p, FormationRenderer& r) : proc_(p), renderer_(r) {
    setOpaque(false);
    setWantsKeyboardFocus(true);
    auto setupMini = [this](juce::Slider& s, const char* tip) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setTooltip(tip);
        addAndMakeVisible(s);
    };
    setupMini(inGain_, "IN: level into the whole plug-in (dB). Double-click to reset to 0 dB.");
    setupMini(mix_, "MIX: everything IH1 does against the untouched input. Double-click for 100 %.");
    setupMini(outGain_, "OUT: final output level (dB). Double-click to reset to 0 dB.");
    inGain_.getProperties().set("zeroNorm", 0.5);
    outGain_.getProperties().set("zeroNorm", 0.5);
    inAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.params(), "in_gain", inGain_);
    mixAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.params(), "mix", mix_);
    outAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.params(), "out_gain", outGain_);
    inGain_.setDoubleClickReturnValue(true, 0.0);
    outGain_.setDoubleClickReturnValue(true, 0.0);
    mix_.setDoubleClickReturnValue(true, 100.0);
    startTimerHz(30);
}

FormationView::~FormationView() { stopTimer(); }

juce::Rectangle<int> FormationView::artworkArea() const { return getLocalBounds().withTrimmedBottom(kStripH); }

void FormationView::setSoftwareFallback(bool on) {
    software_ = on;
    setOpaque(on);
    if (on && !cpuForm_) cpuForm_ = std::make_unique<visual::Formation>(visual::Quality::Low);
    repaint();
}

void FormationView::requestTransfer(uint64_t id, float originY) {
    if (software_) softTransfers_.push_back({id, originY});
    else renderer_.requestTransfer(id, originY);
}

void FormationView::renderSoftware() {
    const auto art = artworkArea();
    const int w = std::clamp(art.getWidth() / 2, 1, 1600), h = std::clamp(art.getHeight() / 2, 1, 1000);
    if (canvas_.width() != w || canvas_.height() != h) { canvas_.resize(w, h); image_ = juce::Image(juce::Image::RGB, w, h, true); }
    visual::FrameInput in;
    in.chain = chain_;
    fillLevels(proc_, in.chain);
    in.transfers.swap(softTransfers_);
    in.dt = 1.0f / 30.0f;
    in.aspect = static_cast<float>(w) / static_cast<float>(h);
    cpuForm_->update(in);
    canvas_.decay(cpuForm_->decay(in.dt));
    canvas_.draw(cpuForm_->segments(), static_cast<float>(h), std::sqrt(juce::jlimit(0.36f, 2.6f, 860.0f / static_cast<float>(h))));
    canvas_.composite(rgb_, kExposure, kBloomGain);
    juce::Image::BitmapData bd(image_, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y) {
        auto* line = bd.getLinePointer(y);
        for (int x = 0; x < w; ++x) {
            const uint8_t* s = &rgb_[static_cast<size_t>((y * w + x) * 3)];
            auto* px = reinterpret_cast<juce::PixelRGB*>(line + x * bd.pixelStride);
            px->setARGB(255, s[0], s[1], s[2]);
        }
    }
}

void FormationView::syncChain() {
    chain_ = chainInputs(proc_);
    renderer_.setChain(chain_);
    items_.clear();
    {
        const juce::ScopedLock sl(proc_.sessionLock());
        const auto& c = proc_.session().chain();
        for (size_t i = 0; i < c.size(); ++i) {
            const auto col = juce::Colour::fromFloatRGBA(chain_[i].colour.r, chain_[i].colour.g, chain_[i].colour.b, 1.0f);
            items_.push_back({c[i].id(), static_cast<int>(i), juce::String(c[i].descriptor().name), col, {}});
        }
    }
    // a drag whose instance disappeared (preset load, delete elsewhere) is cancelled, never retargeted
    if (pressed_ && std::none_of(items_.begin(), items_.end(), [this](const Item& it) { return it.id == dragId_; })) cancelDrag();
    layoutStrip();
}

void FormationView::layoutStrip() {
    // the chain strip lives in a reserved band below the artwork: minimal text items, no pills or boxes
    const float pad = 14.0f, gap = 20.0f;
    const auto font = LookAndFeel::font(12.5f, true);
    strip_ = juce::Rectangle<float>(0.0f, static_cast<float>(getHeight() - kStripH), static_cast<float>(getWidth()), static_cast<float>(kStripH));
    float x = pad;
    const float y = strip_.getY() + 12.0f;
    const float maxX = static_cast<float>(getWidth()) - pad;
    bool overflow = false;
    for (auto& it : items_) {
        const float w = juce::GlyphArrangement::getStringWidth(font, it.name) + 18.0f;
        if (overflow || x + w > maxX - 30.0f) { overflow = true; it.r = {}; continue; }  // the rest: "+n" marker
        it.r = {x, y, w, 22.0f};
        x += w + gap;
    }
    deleteZone_ = {16.0f, 14.0f, 120.0f, 34.0f};  // upper-left corner of the field
}

void FormationView::timerCallback() {
    if (proc_.revision() != seenRevision_) {
        seenRevision_ = proc_.revision();
        syncChain();
        repaint();
    }
    if (software_ && cpuForm_ && getWidth() > 0) { renderSoftware(); repaint(artworkArea()); }
    const double age = juce::Time::getMillisecondCounterHiRes() - toastMs_;
    if (toast_.isNotEmpty() && age < 3000.0) repaint();
}

void FormationView::resized() {
    layoutStrip();
    auto r = getLocalBounds().removeFromTop(64).removeFromRight(160).reduced(10, 10);
    outGain_.setBounds(r.removeFromRight(40));
    r.removeFromRight(6);
    mix_.setBounds(r.removeFromRight(40));
    r.removeFromRight(6);
    inGain_.setBounds(r.removeFromRight(40));
    if (confirm_) confirm_->setBounds(getLocalBounds());
}

void FormationView::paint(juce::Graphics& g) {
    // GL path: no background, the GL formation shows through. Software fallback: draw the CPU image.
    if (software_) {
        g.fillAll(Colours::field);
        if (image_.isValid()) g.drawImage(image_, artworkArea().toFloat());
    }
    // global control captions
    g.setFont(LookAndFeel::font(9.5f, true).withExtraKerningFactor(0.12f));
    for (auto* s : {&inGain_, &mix_, &outGain_}) {
        const juce::String cap = s == &inGain_ ? "IN" : (s == &mix_ ? "MIX" : "OUT");
        g.setColour(s->isMouseOverOrDragging() ? Colours::textDim : Colours::textFaint);
        g.drawText(cap, s->getBounds().translated(0, 34).withHeight(12), juce::Justification::centred);
    }
    // chain strip (the reorder / delete affordance; below the artwork, never over it)
    const auto mouse = getMouseXYRelative().toFloat();
    const auto font = LookAndFeel::font(12.5f, true);
    int hidden = 0;
    for (const auto& it : items_) {
        if (it.r.isEmpty()) { ++hidden; continue; }
        const bool src = dragging_ && it.id == dragId_;
        const bool hot = !dragging_ && it.r.contains(mouse);
        const auto c = readable(it.colour, 0.75f);
        g.setColour(Colours::textFaint.withAlpha(src ? 0.35f : 0.9f));
        g.setFont(LookAndFeel::font(10.0f));
        g.drawText(juce::String(it.index + 1), it.r.withWidth(14.0f), juce::Justification::centredLeft);
        g.setColour(c.withAlpha(src ? 0.3f : (hot ? 1.0f : 0.78f)));
        g.setFont(font);
        g.drawText(it.name, it.r.withTrimmedLeft(16.0f), juce::Justification::centredLeft);
        if (hot) g.fillRect(it.r.getX() + 16.0f, it.r.getBottom() - 2.0f, it.r.getWidth() - 18.0f, 1.0f);
    }
    if (hidden > 0) {
        g.setColour(Colours::textDim);
        g.setFont(LookAndFeel::font(11.0f));
        g.drawText("+" + juce::String(hidden), strip_.withTrimmedRight(14.0f).withTrimmedTop(12.0f).withHeight(22.0f), juce::Justification::centredRight);
    }
    if (dragging_) {
        // insertion marker: a thin vertical line in the gap where the effect would land
        if (insertion_ >= 0 && !deleteHover_) {
            float cx = 14.0f;
            juce::Rectangle<float> ref;
            for (const auto& it : items_) if (!it.r.isEmpty()) ref = it.r;
            const auto& visibleItems = items_;
            int k = 0;
            for (const auto& it : visibleItems) {
                if (it.r.isEmpty()) continue;
                if (k == insertion_) { cx = it.r.getX() - 10.0f; break; }
                cx = it.r.getRight() + 10.0f;
                ++k;
            }
            g.setColour(Colours::accent);
            g.fillRect(cx - 0.5f, strip_.getY() + 10.0f, 1.5f, 26.0f);
        }
        // DELETE: plain red text in the upper-left corner, brighter while hovered (no box, no button)
        g.setFont(LookAndFeel::font(deleteHover_ ? 15.0f : 14.0f, true).withExtraKerningFactor(deleteHover_ ? 0.22f : 0.16f));
        g.setColour(Colours::danger.withAlpha(deleteHover_ ? 1.0f : 0.62f));
        g.drawText("DELETE", deleteZone_, juce::Justification::centredLeft);
        // dragged item preview, next to the pointer
        for (const auto& it : items_)
            if (it.id == dragId_) {
                g.setColour(readable(it.colour).withAlpha(0.95f));
                g.setFont(font);
                g.drawText(it.name, juce::Rectangle<float>(dragPos_.x + 12.0f, dragPos_.y - 11.0f, 220.0f, 22.0f), juce::Justification::centredLeft);
            }
    }
    // toast (status messages only; never an FX name over the field)
    const double age = juce::Time::getMillisecondCounterHiRes() - toastMs_;
    if (toast_.isNotEmpty() && age < 2600.0) {
        const float a = static_cast<float>(age < 2000.0 ? 1.0 : 1.0 - (age - 2000.0) / 600.0);
        g.setColour(Colours::textDim.withAlpha(a));
        g.setFont(LookAndFeel::font(12.5f));
        g.drawText(toast_, strip_.withTrimmedRight(16.0f).withTrimmedTop(12.0f).withHeight(22.0f), juce::Justification::centredRight);
    }
}

void FormationView::showToast(const juce::String& text) {
    toast_ = text;
    toastMs_ = juce::Time::getMillisecondCounterHiRes();
    repaint();
}

int FormationView::hitItem(juce::Point<float> p) const {
    for (size_t i = 0; i < items_.size(); ++i) if (!items_[i].r.isEmpty() && items_[i].r.expanded(4.0f, 6.0f).contains(p)) return static_cast<int>(i);
    return -1;
}

int FormationView::insertionFor(juce::Point<float> p) const {
    // nearest gap among the visible items (0 = before the first, n = after the last)
    int best = 0, k = 0;
    float bestD = 1e9f;
    std::vector<float> gaps;
    for (const auto& it : items_) if (!it.r.isEmpty()) gaps.push_back(it.r.getX() - 10.0f);
    for (auto it = items_.rbegin(); it != items_.rend(); ++it) if (!it->r.isEmpty()) { gaps.push_back(it->r.getRight() + 10.0f); break; }
    for (float gx : gaps) {
        const float d = std::fabs(gx - p.x);
        if (d < bestD) { bestD = d; best = k; }
        ++k;
    }
    return best;
}

void FormationView::mouseMove(const juce::MouseEvent&) { repaint(strip_.toNearestInt()); }
void FormationView::mouseExit(const juce::MouseEvent&) { repaint(strip_.toNearestInt()); }

void FormationView::mouseDown(const juce::MouseEvent& e) {
    if (confirm_ || e.mods.isPopupMenu()) return;
    downPos_ = e.position;
    const int i = hitItem(e.position);
    pressed_ = i >= 0;
    dragId_ = pressed_ ? items_[static_cast<size_t>(i)].id : 0;
    dragging_ = false;
}

void FormationView::mouseDrag(const juce::MouseEvent& e) {
    if (!pressed_ || confirm_) return;
    if (!dragging_ && e.position.getDistanceFrom(downPos_) < 6.0f) return;  // a click is not a drag
    dragging_ = true;
    dragPos_ = e.position;
    deleteHover_ = deleteZone_.expanded(10.0f).contains(e.position);
    insertion_ = insertionFor(e.position);
    repaint();
}

void FormationView::cancelDrag() {
    pressed_ = dragging_ = deleteHover_ = false;
    insertion_ = -1;
    dragId_ = 0;
    repaint();
}

void FormationView::mouseUp(const juce::MouseEvent& e) {
    if (!dragging_) { cancelDrag(); return; }
    const uint64_t id = dragId_;
    const bool overDelete = deleteZone_.expanded(10.0f).contains(e.position);
    const bool inStrip = strip_.expanded(0.0f, 30.0f).contains(e.position);
    const int ins = insertion_;
    cancelDrag();
    // resolve by id now: the chain may have changed during the drag
    int from = -1;
    juce::String name;
    juce::Colour colour;
    for (const auto& it : items_) if (it.id == id) { from = it.index; name = it.name; colour = it.colour; }
    if (from < 0) return;
    if (overDelete) {
        openConfirm(id, name);
    } else if (inStrip && ins >= 0) {
        // map the visible-gap index to a chain index
        int k = 0, gapIndex = -1, lastVisible = -1;
        for (const auto& it : items_) {
            if (it.r.isEmpty()) continue;
            if (k == ins) { gapIndex = it.index; break; }
            lastVisible = it.index;
            ++k;
        }
        if (gapIndex < 0) gapIndex = lastVisible + 1;  // after the last visible item
        const int to = gapIndex > from ? gapIndex - 1 : gapIndex;
        if (to != from) proc_.uiMove(from, to);
    }
    // dropping anywhere else does nothing
}

bool FormationView::keyPressed(const juce::KeyPress& k) {
    if (k == juce::KeyPress::escapeKey && dragging_) { cancelDrag(); return true; }  // nothing changes
    return false;
}

void FormationView::openConfirm(uint64_t id, const juce::String& name) {
    juce::Colour colour = Colours::text;
    for (const auto& it : items_) if (it.id == id) colour = it.colour;
    confirm_ = std::make_unique<Confirm>(name, colour, [this, id](bool doDelete) {
        if (doDelete) proc_.uiRemove(id);  // by stable id: duplicates are never confused
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<FormationView>(this)] { if (safe) safe->closeConfirm(); });
    });
    addAndMakeVisible(*confirm_);
    confirm_->setBounds(getLocalBounds());
}

juce::Component* FormationView::confirmDialog() const { return confirm_.get(); }

void FormationView::closeConfirm() {
    confirm_.reset();
    repaint();
}

juce::String FormationView::describe(uint64_t id) {
    const juce::ScopedLock sl(proc_.sessionLock());
    const auto& c = proc_.session().chain();
    const int i = proc_.session().indexOf(id);
    if (i < 0) return {};
    const auto& ci = c[static_cast<size_t>(i)];
    const auto& d = ci.descriptor();
    juce::String s = juce::String(d.name) + "  (" + juce::String(i + 1) + " of " + juce::String(static_cast<int>(c.size())) + ", committed)\n";
    int shown = 0;
    for (size_t k = 0; k < d.params.size() && shown < 7; ++k) {
        if (d.params[k].advanced) continue;
        s << d.params[k].name << ": " << d.params[k].format(ci.values()[k]) << "\n";
        ++shown;
    }
    s << "Drag along the strip to reorder, or onto DELETE to remove.";
    return s;
}

juce::String FormationView::getTooltip() {
    if (dragging_ || confirm_) return {};
    const int i = hitItem(getMouseXYRelative().toFloat());
    return i >= 0 ? describe(items_[static_cast<size_t>(i)].id) : juce::String();
}

}  // namespace ih1::plugin
