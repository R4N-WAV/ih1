// Chorus and Doubler/MicroPitch (KADAL KNW-Y84ZDH4MWH delay-line modulation family; KNW-5AB5W7MA65 BBD
// colour; MTH-X3GM9FMMVA mono checks). Chorus = several LFO-modulated short delays with phase offsets
// blended with the dry signal. Doubler = a few voices with small FIXED pitch offsets + distinct delays
// + slow drift (no periodic LFO), giving double-tracking rather than a swirl.
#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Basics.h"
#include "ih1/dsp/Svf.h"
#include "ih1/dsp/TdPitch.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

// ------------------------------------------------------------------------------------------ Chorus
enum { cVoices, cRate, cDepth, cDelay, cSpread, cShape, cFeedback, cVintage, cTone, cMix };

class Chorus final : public FxBase {
    static constexpr int kMaxVoices = 4;
    void onPrepare() override {
        for (auto& l : line_) l.prepare(static_cast<int>(0.06 * sr_) + 64);
        for (int v = 0; v < kMaxVoices; ++v) {
            lfo_[v].prepare(sr_, 100 + static_cast<uint32_t>(v));
            lfoR_[v].prepare(sr_, 200 + static_cast<uint32_t>(v));
        }
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        depthS_.prepare(sr_, 50.0f);
        delayS_.prepare(sr_, 80.0f);
    }
    void reset() override {
        for (auto& l : line_) l.reset();
        for (auto& f : tone_) f.reset();
        for (auto& f : preLp_) f.reset();
        mix_.reset();
        fbz_[0] = fbz_[1] = 0.0f;
        for (int v = 0; v < kMaxVoices; ++v) lfo_[v].setPhase(static_cast<float>(v) / kMaxVoices);
        syncRightPhases();
    }
    void syncRightPhases() {
        for (int v = 0; v < kMaxVoices; ++v) lfoR_[v].setPhase(lfo_[v].phase() + fp(cSpread) / 360.0f);
        appliedSpread_ = fp(cSpread);
    }
    void applyParams(bool snap) override {
        const int voices = std::clamp(ip(cVoices) + 1, 1, kMaxVoices);  // choice index 0..3 -> 1..4 voices
        for (int v = 0; v < kMaxVoices; ++v) {
            lfo_[v].setRate(fp(cRate) * (1.0f + 0.07f * v));
            const int shape = ip(cShape) == 2 ? dsp::Lfo::Random : (ip(cShape) == 1 ? dsp::Lfo::Triangle : dsp::Lfo::Sine);
            lfo_[v].setShape(shape);
            lfoR_[v].setRate(fp(cRate) * (1.0f + 0.07f * v));
            lfoR_[v].setShape(shape);
        }
        if (fp(cSpread) != appliedSpread_) syncRightPhases();
        voices_ = voices;
        const float vin = pct(cVintage);
        for (int c = 0; c < 2; ++c) {
            tone_[c].setup(dsp::Svf::Lowpass, fp(cTone) * (1.0f - 0.55f * vin), 0.6f, 0, sr_);
            preLp_[c].setup(dsp::Svf::Lowpass, 18000.0f - 9000.0f * vin, 0.6f, 0, sr_);
        }
        depthS_.setTarget(pct(cDepth) * 0.004f * srf());  // up to 4 ms excursion
        delayS_.setTarget(fp(cDelay) * 0.001f * srf());
        if (snap) { depthS_.snap(depthS_.target()); delayS_.snap(delayS_.target()); }
        mix_.setMix(pct(cMix), snap);
    }

    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float fb = pct(cFeedback) * 0.7f;
        const float vin = pct(cVintage);
        const float vg = 1.0f / std::sqrt(static_cast<float>(voices_));
        for (int i = 0; i < n; ++i) {
            const float depth = depthS_.next(), base = delayS_.next();
            float lfoVal[2][kMaxVoices];
            for (int v = 0; v < kMaxVoices; ++v) { lfoVal[0][v] = lfo_[v].next(); lfoVal[1][v] = lfoR_[v].next(); }
            float out[2] = {0, 0};
            for (int c = 0; c < 2; ++c) {
                float x = ch[c][i] + fb * fbz_[c];
                if (vin > 0.0f) x = std::tanh(preLp_[c].process(x) * (1.0f + vin)) / (1.0f + 0.5f * vin);
                line_[c].push(x);
                float acc = 0.0f;
                for (int v = 0; v < voices_; ++v) {
                    const float l = lfoVal[c][v];  // right LFO runs locked at +spread degrees
                    const float d = base * (1.0f + 0.15f * v) + depth * (0.5f + 0.5f * l);
                    acc += line_[c].readCubic(std::max(2.0f, d));
                }
                out[c] = tone_[c].process(acc * vg);
                fbz_[c] = out[c];
            }
            ch[0][i] = out[0];
            ch[1][i] = out[1];
        }
        for (auto& f : tone_) f.flush();
        mix_.apply(ch, n);
    }

    dsp::DelayLine line_[2];
    dsp::Lfo lfo_[kMaxVoices], lfoR_[kMaxVoices];
    float appliedSpread_ = -1.0f;
    dsp::Svf tone_[2], preLp_[2];
    dsp::Smoother depthS_, delayS_;
    MixStage mix_;
    int voices_ = 2;
    float fbz_[2] = {0, 0};
};

// ------------------------------------------------------------------------------------------ Doubler
enum { dDetune, dDelay, dDrift, dWidth, dVoices, dLowCut, dMix };

class Doubler final : public FxBase {
    void onPrepare() override {
        for (auto& s : shift_) s.prepare(sr_, 35.0f, 90.0f);
        for (int v = 0; v < 4; ++v) { drift_[v].prepare(sr_, 300 + static_cast<uint32_t>(v)); drift_[v].setShape(dsp::Lfo::Drift); }
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
    }
    void reset() override {
        for (auto& s : shift_) s.reset();
        for (auto& f : lc_) f.reset();
        mix_.reset();
    }
    void applyParams(bool snap) override {
        for (auto& f : lc_) f.setup(dsp::Svf::Highpass, fp(dLowCut), 0.6f, 0, sr_);
        mix_.setMix(pct(dMix), snap);
    }
    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const int voices = ip(dVoices) == 1 ? 4 : 2;
        const float cents = fp(dDetune);
        const float baseD = fp(dDelay) * 0.001f * srf();
        const float driftAmt = pct(dDrift) * 0.003f * srf();
        const float width = pct(dWidth);
        // voice layout: detune sign, delay factor, pan
        static constexpr float kDet[4] = {1.0f, -1.0f, 0.5f, -0.5f};
        static constexpr float kDel[4] = {1.0f, 1.37f, 0.62f, 1.81f};
        static constexpr float kPan[4] = {-1.0f, 1.0f, -0.5f, 0.5f};
        float gl[4], gr[4], ratio[4];
        for (int v = 0; v < 4; ++v) {
            dsp::panGains(kPan[v] * width, gl[v], gr[v]);
            ratio[v] = std::exp2(kDet[v] * cents / 1200.0f);
        }
        const float vg = voices == 4 ? 0.6f : 0.8f;
        for (int i = 0; i < n; ++i) {
            const float m = 0.5f * (ch[0][i] + ch[1][i]);
            float l = 0.0f, r = 0.0f;
            for (int v = 0; v < voices; ++v) {
                const float x = lc_[v].process(v % 2 ? ch[1][i] * (1 - width) + m * width : ch[0][i] * (1 - width) + m * width);
                const float d = std::max(0.0f, baseD * kDel[v] + driftAmt * (0.5f + 0.5f * drift_[v].next()));
                const float y = shift_[v].process(x, ratio[v], d);
                l += gl[v] * y;
                r += gr[v] * y;
            }
            ch[0][i] = l * vg * 1.41f;
            ch[1][i] = r * vg * 1.41f;
        }
        for (auto& f : lc_) f.flush();
        mix_.apply(ch, n);
    }

    dsp::TdPitchShifter shift_[4];
    dsp::Lfo drift_[4];
    dsp::Svf lc_[4];
    MixStage mix_;
};

}  // namespace

FxDescriptor describeChorus() {
    FxDescriptor d;
    d.typeId = "chorus";
    d.name = "Chorus";
    d.family = FxFamily::Modulation;
    d.tooltip = "Several slightly delayed copies that gently sway in pitch, making one sound feel like an ensemble.";
    d.params = {
        P::choice("voices", "Voices", {"1", "2", "3", "4"}, 2, "Number of swaying copies. More voices = lusher, thicker shimmer."),
        P::logf("rate", "Rate", "Hz", 0.05f, 5.0f, 0.5f, "Speed of the sway. Slow = lush, fast = warbling."),
        P::pct("depth", "Depth", 40.0f, "How far the copies drift in pitch."),
        P::flt("delay_ms", "Delay", "ms", 3.0f, 30.0f, 9.0f, "Distance of the copies from the original. Short = tighter, long = more doubled."),
        P::flt("spread", "Stereo", "deg", 0.0f, 180.0f, 120.0f, "How differently left and right sway. 120 degrees gives a wide, enveloping field."),
        P::choice("shape", "Shape", {"Smooth", "Triangle", "Random"}, 0, "Character of the sway: smooth, even, or unpredictable."),
        P::pct("feedback", "Feedback", 0.0f, "Feeds the copies back for a more metallic, resonant chorus."),
        P::pct("vintage", "Vintage", 20.0f, "Darker, slightly saturated tone like an old analog chorus pedal."),
        P::logf("tone", "Tone", "Hz", 2000.0f, 20000.0f, 14000.0f, "High-frequency brightness of the chorus copies."),
        P::pct("mix", "Mix", 50.0f, "Balance between original and chorus."),
    };
    for (int i : {cShape, cFeedback, cTone}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Chorus>(); };
    return d;
}

FxDescriptor describeDoubler() {
    FxDescriptor d;
    d.typeId = "doubler";
    d.name = "Doubler";
    d.family = FxFamily::PitchVoice;
    d.tooltip = "Makes one performance sound double-tracked: slightly detuned, slightly late copies left and right.";
    d.params = {
        P::flt("detune", "Detune", "ct", 0.0f, 50.0f, 9.0f, "Pitch difference of the copies, in cents. Small = natural double, large = chorus-like."),
        P::flt("delay_ms", "Delay", "ms", 0.0f, 60.0f, 18.0f, "How late the copies are. 10-30 ms sounds like a second take."),
        P::pct("drift", "Drift", 40.0f, "Random timing wander, like a human trying to play it twice."),
        P::pct("width", "Width", 100.0f, "How far apart the copies are placed."),
        P::choice("voices", "Voices", {"2", "4"}, 0, "Two copies (classic double) or four (choir-like thickness)."),
        P::logf("low_cut", "Low Cut", "Hz", 20.0f, 1000.0f, 120.0f, "Keeps the copies from making the low end muddy."),
        P::pct("mix", "Mix", 50.0f, "Balance between original and copies."),
    };
    d.params[dLowCut].advanced = true;
    d.create = [] { return std::make_unique<Doubler>(); };
    return d;
}

}  // namespace ih1::fx
