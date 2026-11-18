#include "FormationView.h"

#include "Ih1LookAndFeel.h"
#include "PluginProcessor.h"

namespace ih1::plugin {

using namespace juce::gl;

// =============================================================================================== GLSL
namespace {
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
// Mirrors ih1::visual::toneMap() (core/src/visual/Formation.cpp).
const char* kCompFS = R"(#version 150
uniform sampler2D uAccum; uniform sampler2D uBloom; uniform vec2 uTexel; uniform float uExposure; in vec2 vUv; out vec4 o;
vec3 tone(vec3 a) {
  vec3 c = 1.0 - exp(-a * uExposure);
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  float w = clamp((l - 0.45) * 1.6, 0.0, 1.0);
  vec3 white = vec3(0.93, 0.86, 0.98);
  c += w * (white * max(l, 0.0) * 1.15 - c) * 0.6;
  return min(vec3(1.0), c + vec3(0.039, 0.031, 0.051));
}
void main() {
  vec3 a = texture(uAccum, vUv).rgb;
  float r = texture(uAccum, vUv + vec2(uTexel.x, 0.0)).r;  // slight chromatic fringe
  float b = texture(uAccum, vUv - vec2(uTexel.x, 0.0)).b;
  vec3 bl = texture(uBloom, vUv).rgb;
  vec3 acc = vec3(0.8 * a.r + 0.2 * r, a.g, 0.8 * a.b + 0.2 * b) + 0.9 * bl;
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
}  // namespace

// =============================================================================================== telemetry
static void fillFromTelemetry(Processor& proc, visual::FrameInput& in) {
    auto& tel = proc.telemetry();
    const int n = std::min(tel.chainSize.load(), kMaxChain);
    for (auto& ii : in.chain)
        for (int s = 0; s < n; ++s)
            if (tel.slotIds[static_cast<size_t>(s)].load() == ii.id) ii.level = tel.slotRms[static_cast<size_t>(s)].load();
    in.stagedLevel = tel.stagedRms.load();
    in.solo = tel.soloAmount.load();
    in.outLevel = tel.outRms.load();
    in.low = tel.outLow.load();
    in.mid = tel.outMid.load();
    in.high = tel.outHigh.load();
}

// =============================================================================================== renderer
FormationRenderer::FormationRenderer(Processor& p, juce::OpenGLContext& ctx) : proc_(p), ctx_(ctx) {}
FormationRenderer::~FormationRenderer() = default;

void FormationRenderer::setArea(juce::Rectangle<int> a, int editorHeight) {
    const juce::SpinLock::ScopedLockType sl(lock_);
    area_ = a;
    editorHeight_ = editorHeight;
}

void FormationRenderer::setChain(std::vector<visual::InstanceInput> chain, bool hasStaged) {
    const juce::SpinLock::ScopedLockType sl(lock_);
    chain_ = std::move(chain);
    hasStaged_ = hasStaged;
}

std::vector<visual::CoreInfo> FormationRenderer::cores() {
    const juce::SpinLock::ScopedLockType sl(lock_);
    return cores_;
}

void FormationRenderer::newOpenGLContextCreated() {
    // Machines without a real driver (VMs, remote desktop, CI) only offer GL 1.1: never call GL3 there.
    const auto v = ctx_.getVersion();
    if (v.major < 3 || (v.major == 3 && v.minor < 2) || glGenVertexArrays == nullptr || glCreateShader == nullptr ||
        glGenFramebuffers == nullptr || glBindAttribLocation == nullptr) {
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
    verts_.reserve(26000 * 10);
    lastMs_ = juce::Time::getMillisecondCounterHiRes();
}

void FormationRenderer::ensureTarget(Target& t, int w, int h) {
    if (t.fbo && t.w == w && t.h == h) return;
    freeTarget(t);
    t.w = w;
    t.h = h;
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
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void FormationRenderer::freeTarget(Target& t) {
    if (t.fbo) glDeleteFramebuffers(1, &t.fbo);
    if (t.tex) glDeleteTextures(1, &t.tex);
    t = Target{};
}

void FormationRenderer::drawQuad() {
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDisableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void FormationRenderer::renderOpenGL() {
    if (failed_) return;
    juce::OpenGLHelpers::clear(Colours::background);
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
        in.hasStaged = hasStaged_;
    }
    if (area.isEmpty()) return;
    const double scale = ctx_.getRenderingScale();
    const int x = juce::roundToInt(area.getX() * scale), w = juce::roundToInt(area.getWidth() * scale);
    const int h = juce::roundToInt(area.getHeight() * scale);
    const int y = juce::roundToInt((edH - area.getBottom()) * scale);

    // ---- simulation input from telemetry ----
    fillFromTelemetry(proc_, in);
    const double now = juce::Time::getMillisecondCounterHiRes();
    in.dt = static_cast<float>(juce::jlimit(0.001, 0.1, (now - lastMs_) * 0.001));
    lastMs_ = now;
    in.aspect = static_cast<float>(w) / static_cast<float>(std::max(1, h));
    formation_.update(in);
    {
        const juce::SpinLock::ScopedLockType sl(lock_);
        cores_ = formation_.cores();
    }

    // ---- accumulate lines ----
    ensureTarget(accum_, w, h);
    ensureTarget(bloomA_, std::max(1, w / 4), std::max(1, h / 4));
    ensureTarget(bloomB_, std::max(1, w / 4), std::max(1, h / 4));
    glBindVertexArray(vao_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, accum_.fbo);
    glViewport(0, 0, w, h);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);  // multiply: trail persistence
    decayProg_->use();
    decayProg_->setUniform("uDecay", formation_.decay(in.dt));
    drawQuad();
    const auto& segs = formation_.segments();
    verts_.clear();
    for (const auto& s : segs) {
        verts_.insert(verts_.end(), {s.x0, s.y0, s.r, s.g, s.b, s.x1, s.y1, s.r, s.g, s.b});
    }
    if (!verts_.empty()) {
        glBlendFunc(GL_ONE, GL_ONE);  // additive
        lineProg_->use();
        lineProg_->setUniform("uAspect", in.aspect);
        lineProg_->setUniform("uGain", static_cast<float>(juce::jlimit(0.4, 2.5, 600.0 / std::max(1, h))));
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts_.size() * sizeof(float)), verts_.data(), GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(verts_.size() / 5));
    }
    glDisable(GL_BLEND);

    // ---- bloom (quarter resolution) ----
    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_FRAMEBUFFER, bloomA_.fbo);
    glViewport(0, 0, bloomA_.w, bloomA_.h);
    copyProg_->use();
    copyProg_->setUniform("uTex", 0);
    copyProg_->setUniform("uTexel", 1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h));
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
    compProg_->setUniform("uTexel", 1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h));
    compProg_->setUniform("uExposure", 20.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, accum_.tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    drawQuad();
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(0);
}

void FormationRenderer::openGLContextClosing() {
    if (failed_ && !vao_) { lineProg_.reset(); decayProg_.reset(); copyProg_.reset(); blurProg_.reset(); compProg_.reset(); return; }
    freeTarget(accum_);
    freeTarget(bloomA_);
    freeTarget(bloomB_);
    if (lineVbo_) glDeleteBuffers(1, &lineVbo_);
    if (quadVbo_) glDeleteBuffers(1, &quadVbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    lineVbo_ = quadVbo_ = vao_ = 0;
    lineProg_.reset(); decayProg_.reset(); copyProg_.reset(); blurProg_.reset(); compProg_.reset();
}

// =============================================================================================== confirm
class FormationView::Confirm final : public juce::Component {
public:
    Confirm(const juce::String& name, std::function<void(bool)> done) : done_(std::move(done)), name_(name) {
        cancel_.setButtonText("Cancel");
        del_.setButtonText("Delete");
        del_.getProperties().set("primary", true);
        cancel_.onClick = [this] { done_(false); };
        del_.onClick = [this] { done_(true); };
        addAndMakeVisible(cancel_);
        addAndMakeVisible(del_);
        setWantsKeyboardFocus(true);
        setTitle("Delete " + name);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::black.withAlpha(0.55f));
        auto box = box_.toFloat();
        g.setColour(Colours::panelRaised);
        g.fillRoundedRectangle(box, 8.0f);
        g.setColour(Colours::danger.withAlpha(0.6f));
        g.drawRoundedRectangle(box, 8.0f, 1.2f);
        g.setColour(Colours::text);
        g.setFont(LookAndFeel::font(19.0f, true));
        g.drawText("Delete " + name_ + "?", box.reduced(20.0f).removeFromTop(30.0f), juce::Justification::centredLeft);
        g.setColour(Colours::textDim);
        g.setFont(LookAndFeel::font(13.5f));
        g.drawFittedText("This removes this " + name_ + " from the committed chain. Other effects, including copies of the same type, are not affected.",
                         box.reduced(20.0f).withTrimmedTop(36.0f).withHeight(40.0f).toNearestInt(), juce::Justification::topLeft, 3);
    }
    void resized() override {
        box_ = getLocalBounds().withSizeKeepingCentre(std::min(420, getWidth() - 40), 160);
        auto b = box_.reduced(20).removeFromBottom(34);
        del_.setBounds(b.removeFromRight(110));
        b.removeFromRight(10);
        cancel_.setBounds(b.removeFromRight(110));
    }
    void mouseDown(const juce::MouseEvent& e) override { if (!box_.contains(e.getPosition())) done_(false); }
    bool keyPressed(const juce::KeyPress& k) override {
        if (k == juce::KeyPress::escapeKey) { done_(false); return true; }
        return true;  // swallow everything else while open
    }
    void parentHierarchyChanged() override { if (isShowing()) grabKeyboardFocus(); }

private:
    std::function<void(bool)> done_;
    juce::String name_;
    juce::Rectangle<int> box_;
    juce::TextButton cancel_, del_;
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
    setupMini(inGain_, "Input level into the whole plug-in (dB). Double-click to reset.");
    setupMini(mix_, "Global mix of everything IH1 does versus the untouched input.");
    setupMini(outGain_, "Final output level (dB). Double-click to reset.");
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

void FormationView::setSoftwareFallback(bool on) {
    software_ = on;
    setOpaque(on);
    if (on && !cpuForm_) cpuForm_ = std::make_unique<visual::Formation>(12000);
    repaint();
}

void FormationView::renderSoftware() {
    const int w = std::max(1, getWidth() / 2), h = std::max(1, getHeight() / 2);
    if (acc_.size() != static_cast<size_t>(w * h * 3)) { acc_.assign(static_cast<size_t>(w * h * 3), 0.0f); image_ = juce::Image(juce::Image::RGB, w, h, true); }
    visual::FrameInput in;
    in.chain = chain_;
    in.hasStaged = hasStaged_;
    fillFromTelemetry(proc_, in);
    in.dt = 1.0f / 30.0f;
    in.aspect = static_cast<float>(w) / static_cast<float>(h);
    cpuForm_->update(in);
    const float d = cpuForm_->decay(in.dt);
    for (auto& v : acc_) v *= d;
    const float scale = static_cast<float>(h), gain = 0.55f * 600.0f / static_cast<float>(h);  // no bloom, half res: tame to match the GL look
    for (const auto& sg : cpuForm_->segments()) {
        const float x0 = sg.x0 * scale, y0 = sg.y0 * scale, x1 = sg.x1 * scale, y1 = sg.y1 * scale;
        const int steps = std::max(1, static_cast<int>(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0))));
        for (int k = 0; k <= steps; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(steps);
            const int x = static_cast<int>(x0 + (x1 - x0) * t), y = static_cast<int>(y0 + (y1 - y0) * t);
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float* px = &acc_[static_cast<size_t>((y * w + x) * 3)];
            px[0] += sg.r * gain; px[1] += sg.g * gain; px[2] += sg.b * gain;
        }
    }
    juce::Image::BitmapData bd(image_, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float* px = &acc_[static_cast<size_t>((y * w + x) * 3)];
            const auto c = visual::toneMap({px[0], px[1], px[2]}, 20.0f);
            bd.setPixelColour(x, y, juce::Colour::fromFloatRGBA(std::min(1.0f, c.r), std::min(1.0f, c.g), std::min(1.0f, c.b), 1.0f));
        }
    softCores_ = cpuForm_->cores();
}

void FormationView::syncChain() {
    std::vector<visual::InstanceInput> chain;
    pills_.clear();
    {
        const juce::ScopedLock sl(proc_.sessionLock());
        const auto& c = proc_.session().chain();
        hasStaged_ = proc_.session().selected() >= 0;
        stagedName_ = hasStaged_ ? juce::String(fxRegistry()[static_cast<size_t>(proc_.session().selected())].name) : juce::String();
        for (size_t i = 0; i < c.size(); ++i) {
            const auto& d = c[i].descriptor();
            const float hue = visual::hueForType(d.typeId);
            chain.push_back({c[i].id(), hue, c[i].visualSeed(), 0.0f});
            const auto rgb = visual::hsvToRgb(hue, 0.48f, 1.0f);
            pills_.push_back({c[i].id(), static_cast<int>(i), juce::String(d.name), juce::Colour::fromFloatRGBA(rgb.r, rgb.g, rgb.b, 1.0f), {}});
        }
    }
    chain_ = chain;
    renderer_.setChain(std::move(chain), hasStaged_);
    layoutLane();
}

void FormationView::layoutLane() {
    const float pad = 14.0f, h = 24.0f, gap = 6.0f;
    float x = pad, y = static_cast<float>(getHeight()) - pad - h;
    const float maxW = static_cast<float>(getWidth()) - 2 * pad;
    const auto font = LookAndFeel::font(12.0f, true);
    // flow layout bottom-up (rows wrap upward) so the lane stays compact
    std::vector<float> widths;
    for (auto& p : pills_) widths.push_back(juce::GlyphArrangement::getStringWidth(font, juce::String(p.index + 1) + "  " + p.name) + 26.0f);
    int rowStart = 0;
    std::vector<std::pair<int, int>> rows;
    float rowW = 0;
    for (int i = 0; i < static_cast<int>(pills_.size()); ++i) {
        if (rowW + widths[static_cast<size_t>(i)] > maxW && i > rowStart) { rows.push_back({rowStart, i}); rowStart = i; rowW = 0; }
        rowW += widths[static_cast<size_t>(i)] + gap;
    }
    if (rowStart < static_cast<int>(pills_.size())) rows.push_back({rowStart, static_cast<int>(pills_.size())});
    y -= static_cast<float>(std::max(0, static_cast<int>(rows.size()) - 1)) * (h + gap);
    lane_ = {pad, y - 4.0f, maxW, static_cast<float>(rows.size()) * (h + gap) + 4.0f};
    for (const auto& row : rows) {
        x = pad;
        for (int i = row.first; i < row.second; ++i) {
            pills_[static_cast<size_t>(i)].r = {x, y, widths[static_cast<size_t>(i)], h};
            x += widths[static_cast<size_t>(i)] + gap;
        }
        y += h + gap;
    }
    deleteZone_ = {static_cast<float>(getWidth()) - 150.0f, 16.0f, 134.0f, 44.0f};
}

void FormationView::timerCallback() {
    if (proc_.revision() != seenRevision_) {
        seenRevision_ = proc_.revision();
        syncChain();
    }
    if (software_ && cpuForm_ && getWidth() > 0) renderSoftware();
    // labels follow the cores the renderer reports
    labels_.clear();
    const auto cores = software_ ? softCores_ : renderer_.cores();
    const float hgt = static_cast<float>(getHeight());
    for (const auto& c : cores) {
        for (const auto& p : pills_) {
            if (p.id != c.id) continue;
            const float px = c.x * hgt, py = c.y * hgt;
            labels_.push_back({p.id, p.index, p.name, p.colour, juce::Rectangle<float>(px - 50.0f, py - 9.0f, 100.0f, 18.0f)});
        }
    }
    repaint();
}

void FormationView::resized() {
    layoutLane();
    auto r = getLocalBounds().removeFromTop(66).removeFromRight(170).reduced(8, 8);
    outGain_.setBounds(r.removeFromRight(44));
    mix_.setBounds(r.removeFromRight(50));
    inGain_.setBounds(r.removeFromRight(44));
}

void FormationView::paint(juce::Graphics& g) {
    // GL path: no background, the GL formation shows through. Software fallback: draw the CPU image.
    if (software_) {
        g.fillAll(Colours::background);
        if (image_.isValid()) g.drawImage(image_, getLocalBounds().toFloat());
    }
    const bool empty = pills_.empty();
    if (empty && !hasStaged_) {
        g.setColour(Colours::textFaint);
        g.setFont(LookAndFeel::font(14.0f));
        g.drawText("Commit an effect to grow the formation", getLocalBounds().toFloat(), juce::Justification::centred);
    }
    // staged preview caption (the neutral halo drawn by the renderer at 16% / 80% of the field)
    if (hasStaged_ && !stagedName_.isEmpty()) {
        const float hgt = static_cast<float>(getHeight());
        g.setColour(Colours::textDim.withAlpha(0.7f));
        g.setFont(LookAndFeel::font(11.0f));
        g.drawText("staged: " + stagedName_, juce::Rectangle<float>(static_cast<float>(getWidth()) * 0.16f - 80.0f, 0.80f * hgt + 0.06f * hgt, 160.0f, 16.0f), juce::Justification::centred);
    }
    // global control captions
    g.setColour(Colours::textFaint);
    g.setFont(LookAndFeel::font(10.0f, true));
    for (auto* s : {&inGain_, &mix_, &outGain_}) {
        const juce::String cap = s == &inGain_ ? "IN" : (s == &mix_ ? "MIX" : "OUT");
        g.drawText(cap, s->getBounds().translated(0, 34).withHeight(12), juce::Justification::centred);
    }
    // core labels (faint; brighter on hover/drag)
    const auto mouse = getMouseXYRelative().toFloat();
    for (const auto& l : labels_) {
        const bool hot = l.r.contains(mouse) || (dragging_ && l.id == dragId_);
        g.setColour(l.colour.withAlpha(hot ? 0.95f : 0.45f));
        g.setFont(LookAndFeel::font(11.0f, hot));
        g.drawText(l.name, l.r, juce::Justification::centred);
    }
    // chain lane
    if (!pills_.empty()) {
        g.setColour(Colours::background.withAlpha(0.55f));
        g.fillRoundedRectangle(lane_.expanded(6.0f, 4.0f), 8.0f);
    }
    const auto font = LookAndFeel::font(12.0f, true);
    for (const auto& p : pills_) {
        const bool src = dragging_ && p.id == dragId_;
        const bool hot = p.r.contains(mouse);
        g.setColour(Colours::panelRaised.withAlpha(src ? 0.4f : 0.9f));
        g.fillRoundedRectangle(p.r, 12.0f);
        g.setColour((hot ? p.colour : p.colour.withAlpha(0.5f)).withMultipliedAlpha(src ? 0.5f : 1.0f));
        g.drawRoundedRectangle(p.r.reduced(0.5f), 12.0f, 1.0f);
        g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre({p.r.getX() + 12.0f, p.r.getCentreY()}));
        g.setColour(Colours::text.withAlpha(src ? 0.4f : 0.9f));
        g.setFont(font);
        g.drawText(juce::String(p.index + 1) + "  " + p.name, p.r.withTrimmedLeft(20.0f), juce::Justification::centredLeft);
    }
    if (dragging_) {
        // insertion caret
        if (insertion_ >= 0 && !deleteHover_ && !pills_.empty()) {
            juce::Rectangle<float> ref;
            float cx;
            if (insertion_ < static_cast<int>(pills_.size())) { ref = pills_[static_cast<size_t>(insertion_)].r; cx = ref.getX() - 3.0f; }
            else { ref = pills_.back().r; cx = ref.getRight() + 3.0f; }
            g.setColour(Colours::accent);
            g.fillRoundedRectangle(cx - 1.5f, ref.getY() - 4.0f, 3.0f, ref.getHeight() + 8.0f, 1.5f);
            g.setFont(LookAndFeel::font(11.0f, true));
            g.drawText("Lands at position " + juce::String(insertion_ > dragFrom_ ? insertion_ : insertion_ + 1),
                       lane_.withY(lane_.getY() - 24.0f).withHeight(18.0f), juce::Justification::centredLeft);
        }
        // delete target (consistent top-right corner)
        g.setColour((deleteHover_ ? Colours::danger : Colours::panelRaised).withAlpha(deleteHover_ ? 0.9f : 0.85f));
        g.fillRoundedRectangle(deleteZone_, 8.0f);
        g.setColour(Colours::danger);
        g.drawRoundedRectangle(deleteZone_.reduced(0.5f), 8.0f, 1.2f);
        g.setColour(deleteHover_ ? Colours::background : Colours::danger);
        g.setFont(LookAndFeel::font(13.0f, true));
        g.drawText("DELETE", deleteZone_, juce::Justification::centred);
        // dragged ghost
        for (const auto& p : pills_)
            if (p.id == dragId_) {
                const auto ghost = p.r.withCentre(dragPos_);
                g.setColour(Colours::panelRaised.withAlpha(0.95f));
                g.fillRoundedRectangle(ghost, 12.0f);
                g.setColour(p.colour);
                g.drawRoundedRectangle(ghost, 12.0f, 1.2f);
                g.setColour(Colours::text);
                g.setFont(font);
                g.drawText(p.name, ghost.withTrimmedLeft(12.0f), juce::Justification::centredLeft);
            }
    }
    // toast
    const double age = juce::Time::getMillisecondCounterHiRes() - toastMs_;
    if (toast_.isNotEmpty() && age < 2600.0) {
        const float a = static_cast<float>(age < 2000.0 ? 1.0 : 1.0 - (age - 2000.0) / 600.0);
        auto r = juce::Rectangle<float>(0.0f, 0.0f, 320.0f, 32.0f).withCentre({static_cast<float>(getWidth()) * 0.5f, 44.0f});
        g.setColour(Colours::panelRaised.withAlpha(0.9f * a));
        g.fillRoundedRectangle(r, 16.0f);
        g.setColour(Colours::accent.withAlpha(a));
        g.setFont(LookAndFeel::font(13.5f, true));
        g.drawText(toast_, r, juce::Justification::centred);
    }
}

void FormationView::showToast(const juce::String& text) {
    toast_ = text;
    toastMs_ = juce::Time::getMillisecondCounterHiRes();
}

int FormationView::hitPill(juce::Point<float> p) const {
    for (size_t i = 0; i < pills_.size(); ++i) if (pills_[i].r.contains(p)) return static_cast<int>(i);
    return -1;
}

int FormationView::hitLabel(juce::Point<float> p) const {
    for (const auto& l : labels_) if (l.r.contains(p)) return l.index;
    return -1;
}

int FormationView::insertionFor(juce::Point<float> p) const {
    if (pills_.empty()) return -1;
    // nearest gap in the lane, measured along the reading order of the (possibly wrapped) rows
    int best = 0;
    float bestD = 1e9f;
    for (size_t i = 0; i <= pills_.size(); ++i) {
        juce::Point<float> gap;
        if (i < pills_.size()) gap = {pills_[i].r.getX() - 3.0f, pills_[i].r.getCentreY()};
        else gap = {pills_.back().r.getRight() + 3.0f, pills_.back().r.getCentreY()};
        const float d = std::abs(gap.x - p.x) + 3.0f * std::abs(gap.y - p.y);
        if (d < bestD) { bestD = d; best = static_cast<int>(i); }
    }
    return best;
}

void FormationView::mouseMove(const juce::MouseEvent&) { repaint(); }

void FormationView::mouseDown(const juce::MouseEvent& e) {
    if (confirm_) return;
    downPos_ = e.position;
    dragFrom_ = hitPill(e.position);
    if (dragFrom_ < 0) dragFrom_ = hitLabel(e.position);
    dragId_ = dragFrom_ >= 0 ? pills_[static_cast<size_t>(dragFrom_)].id : 0;
    dragging_ = false;
    grabKeyboardFocus();
}

void FormationView::mouseDrag(const juce::MouseEvent& e) {
    if (dragFrom_ < 0 || confirm_) return;
    if (!dragging_ && e.position.getDistanceFrom(downPos_) < 5.0f) return;
    dragging_ = true;
    dragPos_ = e.position;
    deleteHover_ = deleteZone_.expanded(6.0f).contains(e.position);
    insertion_ = insertionFor(e.position);
    repaint();
}

void FormationView::mouseUp(const juce::MouseEvent& e) {
    if (!dragging_) { dragFrom_ = -1; return; }
    const int from = dragFrom_;
    const uint64_t id = dragId_;
    dragging_ = false;
    dragFrom_ = -1;
    if (deleteZone_.expanded(6.0f).contains(e.position)) {
        for (const auto& p : pills_) if (p.id == id) openConfirm(id, p.name);
    } else if (insertion_ >= 0 && (lane_.expanded(40.0f, 60.0f).contains(e.position) || hitLabel(e.position) >= 0)) {
        const int to = insertion_ > from ? insertion_ - 1 : insertion_;
        if (to != from) proc_.uiMove(from, to);
    }
    insertion_ = -1;
    deleteHover_ = false;
    repaint();
}

bool FormationView::keyPressed(const juce::KeyPress& k) {
    if (k == juce::KeyPress::escapeKey && dragging_) {  // cancel a drag: nothing changes
        dragging_ = false;
        dragFrom_ = -1;
        insertion_ = -1;
        deleteHover_ = false;
        repaint();
        return true;
    }
    return false;
}

void FormationView::openConfirm(uint64_t id, const juce::String& name) {
    confirm_ = std::make_unique<Confirm>(name, [this, id](bool doDelete) {
        if (doDelete) proc_.uiRemove(id);
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<FormationView>(this)] { if (safe) safe->closeConfirm(); });
    });
    addAndMakeVisible(*confirm_);
    confirm_->setBounds(getLocalBounds());
    confirm_->grabKeyboardFocus();
}

void FormationView::closeConfirm() {
    confirm_.reset();
    repaint();
}

juce::String FormationView::describe(int chainIndex) {
    const juce::ScopedLock sl(proc_.sessionLock());
    const auto& c = proc_.session().chain();
    if (chainIndex < 0 || chainIndex >= static_cast<int>(c.size())) return {};
    const auto& ci = c[static_cast<size_t>(chainIndex)];
    const auto& d = ci.descriptor();
    juce::String s = juce::String(d.name) + "  (position " + juce::String(chainIndex + 1) + " of " + juce::String(static_cast<int>(c.size())) + ", committed)\n";
    int shown = 0;
    for (size_t i = 0; i < d.params.size() && shown < 7; ++i) {
        if (d.params[i].advanced) continue;
        s << d.params[i].name << ": " << d.params[i].format(ci.values()[i]) << "\n";
        ++shown;
    }
    s << "Drag to reorder. Drag to DELETE to remove.";
    return s;
}

juce::String FormationView::getTooltip() {
    if (dragging_ || confirm_) return {};
    const auto m = getMouseXYRelative().toFloat();
    int idx = hitPill(m);
    if (idx < 0) idx = hitLabel(m);
    return idx >= 0 ? describe(idx) : juce::String();
}

}  // namespace ih1::plugin
