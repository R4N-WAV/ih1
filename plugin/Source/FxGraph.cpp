#include "FxGraph.h"

#include <cmath>
#include <complex>
#include <limits>
#include <vector>

#include "Ih1LookAndFeel.h"
#include "ih1/dsp/Basics.h"
#include "ih1/dsp/Fft.h"
#include "ih1/dsp/Svf.h"

namespace ih1::plugin {

namespace {
enum class Kind { None, Taps, TapeTaps, Decay, Response, Transfer, Shaper, Lfo, Voices, Loop, Grains, GrainEngine };

Kind kindOf(const std::string& id) {
    if (id == "delay") return Kind::Taps;
    if (id == "tape_echo") return Kind::TapeTaps;
    if (id == "reverb" || id == "shimmer" || id == "convolution" || id == "resonator") return Kind::Decay;
    if (id == "filter" || id == "eq") return Kind::Response;
    if (id == "compressor" || id == "gate") return Kind::Transfer;
    if (id == "saturation" || id == "tape_colour" || id == "amp" || id == "bitcrush" || id == "exciter") return Kind::Shaper;
    if (id == "chorus" || id == "flanger" || id == "phaser" || id == "vibrato" || id == "tremolo" || id == "ring_mod") return Kind::Lfo;
    if (id == "harmony" || id == "pitch_shift") return Kind::Voices;
    if (id == "looper") return Kind::Loop;
    if (id == "granular") return Kind::Grains;
    if (id == "grain_engine") return Kind::GrainEngine;
    return Kind::None;
}

// Magnitude response (dB) of a chain of dsp::Svf sections at 48 kHz, measured from the real filters' impulse
// response (so the curve is exactly what the DSP does).
std::vector<float> svfResponse(std::vector<dsp::Svf>& chain, float outGain, int bins) {
    constexpr int N = 4096;
    dsp::Fft fft;
    fft.prepare(N);
    std::vector<dsp::cfloat> d(N);
    for (auto& f : chain) f.reset();
    for (int i = 0; i < N; ++i) {
        float x = i == 0 ? 1.0f : 0.0f;
        for (auto& f : chain) x = f.process(x);
        d[static_cast<size_t>(i)] = dsp::cfloat(x * outGain, 0.0f);
    }
    fft.forward(d.data());
    std::vector<float> out(static_cast<size_t>(bins));
    for (int b = 0; b < bins; ++b) {
        const double f = 20.0 * std::pow(1000.0, static_cast<double>(b) / (bins - 1));  // 20 Hz .. 20 kHz
        const double pos = f / 48000.0 * N;
        const int k = std::clamp(static_cast<int>(pos), 0, N / 2 - 1);
        const double t = pos - k;
        const double m = std::abs(d[static_cast<size_t>(k)]) * (1 - t) + std::abs(d[static_cast<size_t>(k + 1)]) * t;
        out[static_cast<size_t>(b)] = static_cast<float>(20.0 * std::log10(std::max(1e-6, m)));
    }
    return out;
}
}  // namespace

bool FxGraph::hasGraph(const std::string& id) { return kindOf(id) != Kind::None; }

void FxGraph::setType(int idx) {
    type_ = idx;
    typeId_ = idx >= 0 ? fxRegistry()[static_cast<size_t>(idx)].typeId : std::string();
    colour_ = readable(fxColourForIndex(idx), 0.8f);
    hasDisplay_ = false;
    values_.fill(std::numeric_limits<float>::quiet_NaN());  // forces the next setValues to recompute
    shX_.clear();
    shY_.clear();
    repaint();
}

void FxGraph::setValues(const ParamValues& v, double bpm) {
    if (v == values_ && std::fabs(bpm - bpm_) < 1e-3) return;
    values_ = v;
    bpm_ = bpm > 0.0 ? bpm : 120.0;
    measureShaper();
    repaint();
}

void FxGraph::setDisplay(const FxDisplay* d) {
    hasDisplay_ = d != nullptr;
    if (d) display_ = *d;
    repaint();
}

float FxGraph::val(const char* id, float fallback) const {
    if (type_ < 0) return fallback;
    const auto& d = fxRegistry()[static_cast<size_t>(type_)];
    const int i = d.paramIndex(id);
    return i >= 0 ? values_[static_cast<size_t>(i)] : fallback;
}

void FxGraph::caption(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& left, const juce::String& right) {
    g.setFont(LookAndFeel::font(11.0f));
    g.setColour(Colours::textDim);
    g.drawText(left, r.removeFromBottom(14.0f), juce::Justification::centredLeft);
    if (right.isNotEmpty()) g.drawText(right, getLocalBounds().toFloat().removeFromBottom(14.0f), juce::Justification::centredRight);
}

void FxGraph::paint(juce::Graphics& g) {
    if (type_ < 0) return;
    auto r = getLocalBounds().toFloat();
    auto plot = r.withTrimmedBottom(16.0f).reduced(1.0f, 4.0f);
    // baseline only (no box)
    g.setColour(Colours::border);
    g.fillRect(plot.getX(), plot.getBottom(), plot.getWidth(), 1.0f);
    switch (kindOf(typeId_)) {
        case Kind::Taps: drawTaps(g, plot, false); break;
        case Kind::TapeTaps: drawTaps(g, plot, true); break;
        case Kind::Decay: drawDecay(g, plot); break;
        case Kind::Response: drawResponse(g, plot); break;
        case Kind::Transfer: drawTransfer(g, plot); break;
        case Kind::Shaper: drawShaper(g, plot); break;
        case Kind::Lfo: drawLfo(g, plot); break;
        case Kind::Voices: drawVoices(g, plot); break;
        case Kind::Loop: drawLoop(g, plot); break;
        case Kind::Grains: drawGrains(g, plot); break;
        case Kind::GrainEngine: drawGrainEngine(g, plot); break;
        case Kind::None: break;
    }
}

// ------------------------------------------------------------------------------------------- delay taps
void FxGraph::drawTaps(juce::Graphics& g, juce::Rectangle<float> p, bool tape) {
    const bool sync = val("sync") >= 0.5f;
    const double T = std::max(0.001, sync ? dsp::divisionSeconds(static_cast<int>(val("division")), bpm_) : val("time_ms", 300.0f) * 0.001);
    const float fb = tape ? std::min(1.2f, val("intensity", 40.0f) * 0.01f) * 0.85f : val("feedback", 35.0f) * 0.01f;
    const float mix = val("mix", 35.0f) * 0.01f;
    const int stereo = tape ? 1 : static_cast<int>(val("stereo", 1.0f));  // 2 = ping-pong
    std::vector<double> heads = {1.0};
    if (tape) {
        static const std::vector<std::vector<double>> kHeads = {{1}, {2}, {3}, {1, 2}, {2, 3}, {1, 3}, {1, 2, 3}};
        heads = kHeads[static_cast<size_t>(std::clamp(static_cast<int>(val("heads")), 0, 6))];
    } else {
        const int pattern = static_cast<int>(val("pattern"));
        if (pattern == 1) heads = {1.0, 1.5};
        else if (pattern == 2) heads = {1.0, 4.0 / 3.0, 5.0 / 3.0};
        else if (pattern == 3) heads = {1.0, 1.25, 1.5, 1.75};
    }
    const double window = std::clamp(T * 8.0, 0.25, 6.0);
    const float cy = stereo == 2 ? p.getCentreY() : p.getBottom();
    const float hh = stereo == 2 ? p.getHeight() * 0.48f : p.getHeight() * 0.95f;
    if (stereo == 2) { g.setColour(Colours::border); g.fillRect(p.getX(), cy, p.getWidth(), 1.0f); }
    // dry hit
    g.setColour(Colours::textDim);
    g.fillRect(p.getX(), cy - hh * 0.9f, 1.5f, hh * 0.9f);
    int k = 0;
    for (int rep = 0; rep < 64; ++rep) {
        const float amp = mix * std::pow(std::max(0.0f, fb), static_cast<float>(rep));
        if (amp < 0.01f) break;
        for (double hd : heads) {
            const double t = T * (rep + 1) * (tape ? hd : 1.0) + (tape ? 0.0 : T * (hd - 1.0));
            if (t > window) continue;
            const float x = p.getX() + static_cast<float>(t / window) * p.getWidth();
            const float a = std::min(1.0f, amp / std::max(0.05f, mix)) * (tape ? 1.0f / static_cast<float>(heads.size()) * 1.6f : 1.0f);
            const bool down = stereo == 2 && (k % 2 == 1);
            g.setColour(colour_.withAlpha(0.35f + 0.65f * std::min(1.0f, a)));
            const float h = hh * std::min(1.0f, a) * 0.9f;
            g.fillRect(x - 1.0f, down ? cy : cy - h, 2.0f, h);
            ++k;
        }
    }
    caption(g, p.withHeight(p.getHeight() + 16.0f),
            juce::String(T * 1000.0, T < 1.0 ? 0 : 0) + " ms" + (sync ? " at " + juce::String(bpm_, 0) + " BPM" : juce::String()),
            stereo == 2 ? "ping-pong" : (stereo == 0 ? "mono" : "stereo"));
}

// ------------------------------------------------------------------------------------------- decay
void FxGraph::drawDecay(juce::Graphics& g, juce::Rectangle<float> p) {
    const bool resonator = typeId_ == "resonator";
    const float decay = std::max(0.1f, val("decay", 2.5f));
    float pre = val("predelay", 0.0f) * 0.001f;
    float lengthScale = 1.0f;
    if (typeId_ == "convolution") lengthScale = std::max(0.05f, val("length", 100.0f) * 0.01f);
    const bool freeze = val("freeze") >= 0.5f;
    const bool gated = typeId_ == "reverb" && static_cast<int>(val("mode")) == 5;
    const float rt = (typeId_ == "convolution" ? 2.5f : decay) * lengthScale;
    const float window = std::clamp(pre + rt * 1.15f, 0.3f, 32.0f);
    const float early = typeId_ == "reverb" ? val("early", 25.0f) * 0.01f : 0.0f;
    const float damp = typeId_ == "reverb" ? val("damping", 40.0f) * 0.01f : (resonator ? 1.0f - val("bright", 50.0f) * 0.01f : 0.4f);
    juce::Path full, high;
    const int N = 160;
    for (int i = 0; i <= N; ++i) {
        const float t = window * static_cast<float>(i) / N;
        float db;
        if (t < pre) db = -80.0f;
        else if (freeze) db = -6.0f;
        else if (gated) db = (t - pre) < rt * 0.35f ? -6.0f : -80.0f;
        else db = -60.0f * (t - pre) / rt;  // RT60 by definition of Decay
        const float y = p.getBottom() - std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f) * p.getHeight();
        const float yh = p.getBottom() - std::clamp((db * (1.0f + 1.5f * damp) + 60.0f) / 60.0f, 0.0f, 1.0f) * p.getHeight();  // highs fall faster with damping
        const float x = p.getX() + p.getWidth() * static_cast<float>(i) / N;
        if (i == 0) { full.startNewSubPath(x, p.getBottom()); high.startNewSubPath(x, p.getBottom()); }
        full.lineTo(x, y);
        high.lineTo(x, yh);
    }
    juce::Path area = full;
    area.lineTo(p.getRight(), p.getBottom());
    area.closeSubPath();
    g.setColour(colour_.withAlpha(0.12f));
    g.fillPath(area);
    g.setColour(colour_.withAlpha(0.9f));
    g.strokePath(full, juce::PathStrokeType(1.4f));
    g.setColour(colour_.withAlpha(0.35f));
    g.strokePath(high, juce::PathStrokeType(1.0f));
    if (early > 0.0f && !freeze) {  // early reflections
        for (int k = 1; k <= 6; ++k) {
            const float t = pre * 0.4f + 0.007f * static_cast<float>(k * k);
            const float x = p.getX() + p.getWidth() * t / window;
            g.setColour(Colours::text.withAlpha(0.25f + 0.5f * early));
            g.fillRect(x, p.getBottom() - p.getHeight() * early * 0.8f / static_cast<float>(k), 1.0f, p.getHeight() * early * 0.8f / static_cast<float>(k));
        }
    }
    caption(g, p.withHeight(p.getHeight() + 16.0f), freeze ? juce::String("frozen: infinite sustain") : (gated ? juce::String("gated") : "RT60 " + juce::String(rt, rt < 10 ? 1 : 0) + " s"),
            pre > 0.0f ? "pre-delay " + juce::String(pre * 1000.0f, 0) + " ms" : juce::String());
}

// ------------------------------------------------------------------------------------------- filter / EQ
void FxGraph::drawResponse(juce::Graphics& g, juce::Rectangle<float> p) {
    constexpr int bins = 200;
    std::vector<dsp::Svf> chain;
    float outGain = 1.0f;
    float range = 24.0f;
    juce::String cap;
    float modLo = 0.0f, modHi = 0.0f;
    if (typeId_ == "eq") {
        auto add = [&](dsp::Svf::Mode m, float f, float q, float db) { dsp::Svf s; s.setup(m, f, q, db, 48000.0); chain.push_back(s); };
        if (val("hp", 20.0f) > 20.5f) add(dsp::Svf::Highpass, val("hp", 20.0f), 0.707f, 0);
        add(dsp::Svf::LowShelf, val("ls_freq", 120.0f), 0.707f, val("ls_gain"));
        add(dsp::Svf::Bell, val("b1_freq", 500.0f), val("b1_q", 1.0f), val("b1_gain"));
        add(dsp::Svf::Bell, val("b2_freq", 3000.0f), val("b2_q", 1.5f), val("b2_gain"));
        add(dsp::Svf::HighShelf, val("hs_freq", 8000.0f), 0.707f, val("hs_gain"));
        if (val("lp", 20000.0f) < 19900.0f) add(dsp::Svf::Lowpass, val("lp", 20000.0f), 0.707f, 0);
        outGain = std::pow(10.0f, val("out") / 20.0f);
        range = 18.0f;
        cap = "frequency response";
    } else {
        const int type = static_cast<int>(val("type", 1.0f));
        const float fc = val("cutoff", 1200.0f), res = val("resonance", 25.0f) * 0.01f;
        auto add = [&](dsp::Svf::Mode m, float q) { dsp::Svf s; s.setup(m, fc, q, 0.0f, 48000.0); chain.push_back(s); };
        switch (type) {  // mirrors the Filter / Wah processor exactly
            case 0: add(dsp::Svf::Lowpass, 0.5f + res * 19.5f); break;
            case 1: add(dsp::Svf::Lowpass, 0.541f); add(dsp::Svf::Lowpass, 1.307f + res * 18.0f); break;
            case 2: add(dsp::Svf::Highpass, 0.5f + res * 19.5f); break;
            case 3: add(dsp::Svf::Highpass, 0.541f); add(dsp::Svf::Highpass, 1.307f + res * 18.0f); break;
            case 4: add(dsp::Svf::Bandpass, 0.7f + res * 14.0f); outGain = 1.5f; break;
            case 5: add(dsp::Svf::Notch, 0.5f + res * 8.0f); break;
            default: add(dsp::Svf::Bandpass, 2.5f + res * 12.0f); outGain = 2.5f; break;
        }
        const int mod = static_cast<int>(val("mod"));
        if (mod == 1 || mod == 3) { modLo = fc * std::exp2(-val("depth", 1.5f)); modHi = fc * std::exp2(val("depth", 1.5f)); }
        if (mod >= 2) { const float e = val("env_amt", 2.5f); modLo = std::min(modLo > 0 ? modLo : fc, fc * std::exp2(std::min(0.0f, e))); modHi = std::max(modHi, fc * std::exp2(std::max(0.0f, e))); }
        cap = juce::String(fc >= 1000.0f ? juce::String(fc / 1000.0f, 2) + " kHz" : juce::String(fc, 0) + " Hz");
        range = 30.0f;
    }
    const auto db = svfResponse(chain, outGain, bins);
    auto xOf = [&](float f) { return p.getX() + p.getWidth() * std::log(std::clamp(f, 20.0f, 20000.0f) / 20.0f) / std::log(1000.0f); };
    auto yOf = [&](float d) { return p.getCentreY() - std::clamp(d, -range, range) / range * p.getHeight() * 0.5f; };
    // grid: 0 dB and decades (tone only)
    g.setColour(Colours::border.withAlpha(0.7f));
    g.fillRect(p.getX(), yOf(0.0f), p.getWidth(), 1.0f);
    for (float f : {100.0f, 1000.0f, 10000.0f}) g.fillRect(xOf(f), p.getY(), 1.0f, p.getHeight());
    if (modHi > modLo && modLo > 0.0f) {
        g.setColour(colour_.withAlpha(0.08f));
        g.fillRect(juce::Rectangle<float>(xOf(modLo), p.getY(), xOf(modHi) - xOf(modLo), p.getHeight()));
    }
    juce::Path path;
    for (int b = 0; b < bins; ++b) {
        const float x = p.getX() + p.getWidth() * static_cast<float>(b) / (bins - 1);
        if (b == 0) path.startNewSubPath(x, yOf(db[0]));
        else path.lineTo(x, yOf(db[static_cast<size_t>(b)]));
    }
    g.setColour(colour_);
    g.strokePath(path, juce::PathStrokeType(1.5f));
    caption(g, p.withHeight(p.getHeight() + 16.0f), cap, "20 Hz - 20 kHz, +-" + juce::String(range, 0) + " dB");
}

// ------------------------------------------------------------------------------------------- dynamics
void FxGraph::drawTransfer(juce::Graphics& g, juce::Rectangle<float> p) {
    const float lo = typeId_ == "gate" ? -80.0f : -60.0f;
    auto box = p.withSizeKeepingCentre(std::min(p.getWidth(), p.getHeight() * 1.6f), p.getHeight());
    auto xOf = [&](float d) { return box.getX() + (d - lo) / -lo * box.getWidth(); };
    auto yOf = [&](float d) { return box.getBottom() - std::clamp((d - lo) / -lo, 0.0f, 1.0f) * box.getHeight(); };
    g.setColour(Colours::border.withAlpha(0.7f));
    g.drawLine(xOf(lo), yOf(lo), xOf(0), yOf(0), 1.0f);  // unity
    juce::Path path;
    const float thr = val("threshold", -20.0f);
    juce::String cap;
    for (int i = 0; i <= 120; ++i) {
        const float in = lo + -lo * static_cast<float>(i) / 120.0f;
        float out = in;
        if (typeId_ == "compressor") {
            float ratio = val("ratio", 4.0f), knee = val("knee", 6.0f);
            const int mode = static_cast<int>(val("mode"));
            if (mode == 2) { ratio = std::min(ratio, 4.0f); knee = std::max(knee, 12.0f); }
            if (mode == 3) { ratio = 100.0f; knee = 1.0f; }
            const float over = in - thr;
            if (2.0f * over < -knee) out = in;
            else if (2.0f * std::fabs(over) <= knee) out = in + (1.0f / ratio - 1.0f) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * std::max(knee, 1e-3f));
            else out = thr + over / ratio;
            cap = juce::String(ratio >= 99.0f ? juce::String("limit") : juce::String(ratio, 1) + ":1") + " above " + juce::String(thr, 0) + " dB";
        } else {
            const int mode = static_cast<int>(val("mode"));
            const float range = val("range", 40.0f), ratio = val("ratio", 2.0f);
            if (in < thr) out = mode == 0 ? in - range : std::max(in - range, thr + (in - thr) * ratio);
            cap = juce::String(mode == 0 ? "gate" : (mode == 1 ? "expander" : "pump")) + " below " + juce::String(thr, 0) + " dB";
        }
        if (i == 0) path.startNewSubPath(xOf(in), yOf(out));
        else path.lineTo(xOf(in), yOf(out));
    }
    g.setColour(colour_.withAlpha(0.35f));
    g.fillRect(xOf(thr), box.getY(), 1.0f, box.getHeight());
    g.setColour(colour_);
    g.strokePath(path, juce::PathStrokeType(1.6f));
    caption(g, p.withHeight(p.getHeight() + 16.0f), cap, "in -> out (dB)");
}

// ------------------------------------------------------------------------------------------- shapers
void FxGraph::measureShaper() {
    // Measured, not drawn: a slow full-scale sine through a private instance of the real processor with the
    // current settings; the output is plotted against the input. Recomputed only when parameters change.
    shX_.clear();
    shY_.clear();
    shPeak_ = 1.0f;
    if (type_ < 0 || kindOf(typeId_) != Kind::Shaper) return;
    const auto& d = fxRegistry()[static_cast<size_t>(type_)];
    auto proc = d.create();
    proc->prepare(48000.0, 480);
    proc->setParams(d.clamp(values_), true);
    std::vector<float> L(480), R(480), in(480);
    ProcessContext ctx;
    ctx.sampleRate = 48000.0;
    double ph = 0.0;
    for (int b = 0; b < 30; ++b) {  // 0.3 s: filters and auto-gain settle
        float* ch[2] = {L.data(), R.data()};
        for (size_t i = 0; i < 480; ++i) { in[i] = static_cast<float>(std::sin(ph)); L[i] = R[i] = in[i]; ph += 2.0 * 3.14159265358979 * 50.0 / 48000.0; }
        proc->process(ch, 480, ctx);
        if (b >= 28) for (size_t i = 0; i < 480; i += 2) { shX_.push_back(in[i]); shY_.push_back(L[i]); }
    }
    for (float y : shY_) shPeak_ = std::max(shPeak_, std::isfinite(y) ? std::fabs(y) : 0.0f);
}

void FxGraph::drawShaper(juce::Graphics& g, juce::Rectangle<float> p) {
    const auto& xs = shX_;
    const auto& ys = shY_;
    const float peak = shPeak_;
    auto box = p.withSizeKeepingCentre(std::min(p.getWidth(), p.getHeight() * 1.8f), p.getHeight());
    g.setColour(Colours::border.withAlpha(0.7f));
    g.fillRect(box.getX(), box.getCentreY(), box.getWidth(), 1.0f);
    g.fillRect(box.getCentreX(), box.getY(), 1.0f, box.getHeight());
    juce::Path path;
    for (size_t i = 0; i < xs.size(); ++i) {
        const float x = box.getCentreX() + xs[i] * box.getWidth() * 0.5f;
        const float y = box.getCentreY() - ys[i] / peak * box.getHeight() * 0.5f;
        if (i == 0) path.startNewSubPath(x, y);
        else path.lineTo(x, y);
    }
    g.setColour(colour_);
    g.strokePath(path, juce::PathStrokeType(1.4f));
    caption(g, p.withHeight(p.getHeight() + 16.0f), "measured transfer (full-scale input)", peak > 1.01f ? "peak " + juce::String(20.0f * std::log10(peak), 1) + " dB" : juce::String());
}

// ------------------------------------------------------------------------------------------- modulation
void FxGraph::drawLfo(juce::Graphics& g, juce::Rectangle<float> p) {
    float rate = val("rate", 1.0f);
    float depth = 1.0f;
    int shape = static_cast<int>(val("shape"));
    juce::String what = "LFO";
    if (typeId_ == "chorus") { depth = val("depth", 40.0f) * 0.01f; shape = shape == 1 ? 1 : (shape == 2 ? 4 : 0); }
    else if (typeId_ == "flanger") depth = val("depth", 2.5f) / 8.0f;
    else if (typeId_ == "phaser") { depth = val("range", 3.0f) / 5.0f; shape = 0; }
    else if (typeId_ == "vibrato") { depth = val("depth", 20.0f) / 100.0f; if (val("sync") >= 0.5f) rate = static_cast<float>(1.0 / dsp::divisionSeconds(static_cast<int>(val("division")), bpm_)); }
    else if (typeId_ == "tremolo") {
        depth = val("depth", 70.0f) * 0.01f;
        if (val("sync") >= 0.5f) rate = static_cast<float>(1.0 / dsp::divisionSeconds(static_cast<int>(val("division")), bpm_));
        what = static_cast<int>(val("mode")) == 1 ? "pan" : "level";
    } else if (typeId_ == "ring_mod") {
        rate = val("freq", 220.0f);
        shape = static_cast<int>(val("wave"));
        depth = 1.0f;
        what = "carrier";
    }
    const float cycles = typeId_ == "ring_mod" ? 4.0f : 2.0f;
    const float stereo = val("stereo", typeId_ == "chorus" ? val("spread") : 0.0f);
    auto wave = [&](float ph) {  // ph in cycles
        ph -= std::floor(ph);
        switch (shape) {
            case 1: return 1.0f - 4.0f * std::fabs(ph - 0.5f);                        // triangle
            case 2: return ph < 0.5f ? 1.0f : -1.0f;                                  // square
            case 3: return 2.0f * ph - 1.0f;                                          // ramp / saw
            case 4: { const int k = static_cast<int>(ph * 8.0f); return std::sin(static_cast<float>(k) * 2.3f); }  // random steps
            default: return std::sin(ph * 2.0f * 3.14159265f);                        // sine
        }
    };
    for (int pass = 0; pass < (stereo > 0.5f ? 2 : 1); ++pass) {
        const float off = pass == 1 ? stereo / 360.0f : 0.0f;
        juce::Path path;
        for (int i = 0; i <= 200; ++i) {
            const float t = static_cast<float>(i) / 200.0f;
            const float y = p.getCentreY() - wave(t * cycles + off) * depth * p.getHeight() * 0.46f;
            const float x = p.getX() + t * p.getWidth();
            if (i == 0) path.startNewSubPath(x, y);
            else path.lineTo(x, y);
        }
        g.setColour(pass == 0 ? colour_ : colour_.withAlpha(0.4f));
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }
    caption(g, p.withHeight(p.getHeight() + 16.0f), what + " " + (rate >= 10.0f ? juce::String(rate, 0) : juce::String(rate, 2)) + " Hz",
            stereo > 0.5f ? "stereo offset " + juce::String(stereo, 0) + " deg" : juce::String("depth " + juce::String(depth * 100.0f, 0) + " %"));
}

// ------------------------------------------------------------------------------------------- voices
void FxGraph::drawVoices(juce::Graphics& g, juce::Rectangle<float> p) {
    struct V { float st, db, pan; };
    std::vector<V> voices;
    juce::String cap;
    if (typeId_ == "harmony") {
        const int mode = static_cast<int>(val("mode"));
        const int n = static_cast<int>(val("voices", 1.0f)) + 1;
        const char* ints[] = {"int1", "int2", "int3", "int4"};
        const char* lvls[] = {"lvl1", "lvl2", "lvl3", "lvl4"};
        const char* pans[] = {"pan1", "pan2", "pan3", "pan4"};
        for (int k = 0; k < n; ++k) voices.push_back({val(ints[k]), val(lvls[k]), val(pans[k]) * 0.01f});
        cap = mode == 0 ? "intervals (semitones)" : (mode == 1 ? "scale steps in key" : "voices follow MIDI notes");
    } else {
        voices.push_back({val("shift_a", 12.0f) + val("fine_a") * 0.01f, 0.0f, -0.3f});
        if (val("level_b", -60.0f) > -59.0f) voices.push_back({val("shift_b", -12.0f), val("level_b", -60.0f), 0.3f});
        cap = "shift (semitones)";
    }
    // lead at 0; vertical = interval, horizontal = pan, brightness = level
    g.setColour(Colours::border.withAlpha(0.7f));
    const float cy = p.getCentreY();
    g.fillRect(p.getX(), cy, p.getWidth(), 1.0f);
    g.setColour(Colours::text.withAlpha(0.7f));
    g.fillRect(p.getCentreX() - 10.0f, cy - 1.5f, 20.0f, 3.0f);
    for (const auto& v : voices) {
        const float y = cy - std::clamp(v.st, -24.0f, 24.0f) / 24.0f * p.getHeight() * 0.48f;
        const float x = p.getCentreX() + std::clamp(v.pan, -1.0f, 1.0f) * p.getWidth() * 0.4f;
        const float a = std::clamp((v.db + 60.0f) / 66.0f, 0.1f, 1.0f);
        g.setColour(colour_.withAlpha(a));
        g.fillRect(x - 12.0f, y - 1.5f, 24.0f, 3.0f);
        g.setFont(LookAndFeel::font(10.5f));
        g.drawText((v.st > 0 ? "+" : "") + juce::String(v.st, 0), juce::Rectangle<float>(x + 14.0f, y - 7.0f, 34.0f, 14.0f), juce::Justification::centredLeft);
    }
    caption(g, p.withHeight(p.getHeight() + 16.0f), cap, "left - pan - right");
}

// ------------------------------------------------------------------------------------------- looper
void FxGraph::drawLoop(juce::Graphics& g, juce::Rectangle<float> p) {
    const auto& d = display_;
    const int state = hasDisplay_ ? d.state : 0;
    const float w = p.getWidth() / static_cast<float>(FxDisplay::kPeaks);
    for (int i = 0; i < FxDisplay::kPeaks; ++i) {
        const float x = p.getX() + w * static_cast<float>(i);
        const bool recorded = state == 2 || (state == 1 && static_cast<float>(i) / FxDisplay::kPeaks < d.filled);
        if (!recorded || !hasDisplay_) continue;
        const float a = std::min(1.0f, d.peaks[static_cast<size_t>(i)] * 1.4f);
        const float h = std::max(1.0f, a * p.getHeight() * 0.48f);
        g.setColour(colour_.withAlpha(state == 1 ? 0.55f : 0.85f));
        g.fillRect(x + 0.5f, p.getCentreY() - h, std::max(1.0f, w - 1.0f), h * 2.0f);
    }
    if (state == 1) { g.setColour(Colours::danger.withAlpha(0.8f)); g.fillRect(p.getX() + p.getWidth() * d.filled, p.getY(), 1.5f, p.getHeight()); }
    if (state == 2) { g.setColour(Colours::text); g.fillRect(p.getX() + p.getWidth() * d.position, p.getY(), 1.5f, p.getHeight()); }
    if (state == 0) {
        g.setColour(Colours::border);
        g.fillRect(p.getX(), p.getCentreY(), p.getWidth(), 1.0f);
    }
    const int capture = static_cast<int>(val("capture"));
    const juce::String waiting = capture == 1 ? "empty: records from the next bar" : "empty: records on the first sound";
    caption(g, p.withHeight(p.getHeight() + 16.0f), state == 0 ? waiting : (state == 1 ? juce::String("recording") : juce::String("playing")),
            hasDisplay_ && d.seconds > 0.0f ? juce::String(d.seconds, 2) + " s loop" : juce::String());
}

// ------------------------------------------------------------------------------------------- granular
void FxGraph::drawGrains(juce::Graphics& g, juce::Rectangle<float> p) {
    // the 2 s capture buffer (now = right edge), where grains are read from (position +- spray), their size
    // and how many start per second
    const float pos = val("position_ms", 250.0f), spray = val("spray_ms", 300.0f), size = val("size_ms", 90.0f);
    const float density = val("density", 18.0f);
    const bool freeze = val("freeze") >= 0.5f;
    auto xOf = [&](float msAgo) { return p.getRight() - std::clamp(msAgo, 0.0f, 2000.0f) / 2000.0f * p.getWidth(); };
    g.setColour(colour_.withAlpha(0.1f));
    g.fillRect(juce::Rectangle<float>::leftTopRightBottom(xOf(pos + spray * 0.5f + size), p.getY(), xOf(std::max(0.0f, pos - spray * 0.5f)), p.getBottom()));
    // a deterministic sample of grain windows that matches the density (grains in the last second)
    const int n = std::clamp(static_cast<int>(density), 1, 60);
    for (int k = 0; k < n; ++k) {
        const float u = std::fmod(static_cast<float>(k) * 0.61803398f, 1.0f) - 0.5f;
        const float start = pos + u * spray;
        const float x0 = xOf(start + size), x1 = xOf(start);
        const float y = p.getY() + p.getHeight() * (0.15f + 0.7f * std::fmod(static_cast<float>(k) * 0.3819660f, 1.0f));
        g.setColour(colour_.withAlpha(0.75f));
        g.fillRect(juce::Rectangle<float>::leftTopRightBottom(x0, y - 1.0f, std::max(x0 + 1.0f, x1), y + 1.0f));
    }
    g.setColour(Colours::textDim);
    g.fillRect(p.getRight() - 1.0f, p.getY(), 1.0f, p.getHeight());
    caption(g, p.withHeight(p.getHeight() + 16.0f), freeze ? juce::String("buffer frozen") : juce::String("grains read from the last 2 s"),
            juce::String(density, 0) + " grains/s, " + juce::String(size, 0) + " ms");
}

// ------------------------------------------------------------------------------------------- grain engine
void FxGraph::drawGrainEngine(juce::Graphics& g, juce::Rectangle<float> p) {
    // Live, from the audio thread (lock-free): the capture buffer (now = right edge), each sounding grain at
    // its real read position drawn with the selected window shape, and the playhead. Inset: the window.
    const auto& d = display_;
    const int mode = static_cast<int>(val("mode"));
    const int win = static_cast<int>(val("window"));
    const bool frozen = hasDisplay_ && d.state == 1;
    auto window = [win](float x) {
        x = std::clamp(x, 0.0f, 1.0f);
        switch (win) {
            case 1: { const float e = std::exp(-0.5f * 9.766f), gg = std::exp(-0.5f * (x - 0.5f) * (x - 0.5f) / 0.0256f); return (gg - e) / (1.0f - e); }
            case 2: return x < 0.125f ? 0.5f - 0.5f * std::cos(3.14159265f * x / 0.125f) : (x > 0.875f ? 0.5f - 0.5f * std::cos(3.14159265f * (1.0f - x) / 0.125f) : 1.0f);
            case 3: return (x < 0.03f ? 0.5f - 0.5f * std::cos(3.14159265f * x / 0.03f) : std::exp(-4.0f * (x - 0.03f)));
            case 4: { const float y = 1.0f - x; return (y < 0.03f ? 0.5f - 0.5f * std::cos(3.14159265f * y / 0.03f) : std::exp(-4.0f * (y - 0.03f))); }
            default: return 0.5f - 0.5f * std::cos(6.2831853f * x);
        }
    };
    if (frozen) {  // held material: a faint tint over the whole buffer
        g.setColour(colour_.withAlpha(0.06f));
        g.fillRect(p);
    }
    if (hasDisplay_) {
        const int n = FxDisplay::kPeaks;
        const float w = p.getWidth() / static_cast<float>(n);
        g.setColour(colour_.withAlpha(0.45f));
        for (int i = 0; i < n; ++i) {
            const float a = std::min(1.0f, d.peaks[static_cast<size_t>((d.peakHead + i) % n)] * 1.6f);
            const float h = std::max(0.5f, a * p.getHeight() * 0.42f);
            g.fillRect(p.getX() + w * static_cast<float>(i) + 0.5f, p.getCentreY() - h, std::max(1.0f, w - 1.0f), h * 2.0f);
        }
        // grains: window shapes at their read positions (width = grain size on the display's time scale)
        const float span = std::max(0.5f, d.seconds);
        const float gw = std::clamp(val("size_ms", 120.0f) * 0.001f / span * p.getWidth(), 4.0f, p.getWidth() * 0.5f);
        for (float gx : d.grains) {
            if (gx < 0.0f) continue;
            juce::Path env;
            const float x0 = p.getX() + gx * p.getWidth();
            for (int k = 0; k <= 16; ++k) {
                const float u = static_cast<float>(k) / 16.0f;
                const float y = p.getBottom() - window(u) * p.getHeight() * 0.9f;
                if (k == 0) env.startNewSubPath(x0, y); else env.lineTo(x0 + u * gw, y);
            }
            g.setColour(colour_.withAlpha(0.8f));
            g.strokePath(env, juce::PathStrokeType(1.0f));
        }
        if (d.playhead >= 0.0f) {
            g.setColour(Colours::text.withAlpha(0.9f));
            g.fillRect(p.getX() + d.playhead * p.getWidth() - 0.75f, p.getY(), 1.5f, p.getHeight());
        }
    }
    g.setColour(Colours::textDim);
    g.fillRect(p.getRight() - 1.0f, p.getY(), 1.0f, p.getHeight());  // now
    const char* modes[3] = {"cloud", "stretch", "loop"};
    juce::String left = juce::String(modes[std::clamp(mode, 0, 2)]) + (frozen ? ", held" : "");
    if (mode != 0) left << ", " << juce::String(val("speed", 0.5f), 2) << "x" << (val("varispeed") >= 0.5f ? " varispeed" : "");
    const juce::String right = hasDisplay_ ? juce::String(d.activeGrains) + " grains sounding, last " + juce::String(d.seconds, 1) + " s" : juce::String();
    caption(g, p.withHeight(p.getHeight() + 16.0f), left, right);
}

}  // namespace ih1::plugin
