// Grain Engine (V2.1 Phase 6): IH1's buffer-based transformation instrument. See ih1/dsp/GrainEngine.h for
// the engine, the modes and the research basis. This file maps the parameter schema onto the engine.
//
// Latency: zero. Every grain reads audio that has already been captured, so nothing is buffered ahead of
// the output and no lookahead exists. What you hear is deliberately taken from the past (Offset, Stretch,
// Loop); that is the effect's sound, not plug-in latency.
#include "ih1/dsp/GrainEngine.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

enum {
    geMode, geSize, geDensity, gePosition, gePitch, geSpeed, geFeedback, geMix,
    geFreeze, geLock, geOverlap, geVoices, geSpray, geSizeJit, gePitchJit, geScale, geReverse, geWindow,
    geSpread, geStereo, geVarispeed, geLoop, geRetrig, geSync, geRetrigDiv, geRetrigMs, geAlign,
    geDelay, geFbHp, geFbLp, geGain
};

class GrainEngineFx final : public FxBase {
    void onPrepare() override {
        engine_.prepare(sr_, 12.0f, maxBlock_, 0x6A41u);
        wetL_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        wetR_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
        out_.prepare(sr_, 30.0f);
    }
    void reset() override {
        engine_.reset();
        mix_.reset();
    }
    double tailSeconds() const override { return 4.0; }
    void applyParams(bool snap) override {
        mix_.setMix(pct(geMix), snap);
        out_.setTarget(dsp::dbToGain(fp(geGain)));
        if (snap) out_.snap(out_.target());
    }
    void process(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        dsp::GrainEngine::Params p;
        p.mode = std::clamp(ip(geMode), 0, 2);
        p.freeze = bp(geFreeze);
        p.sizeMs = fp(geSize);
        // Ratio lock: density follows grain size so the overlap (grains sounding at once) stays constant
        p.density = bp(geLock) ? fp(geOverlap) / std::max(0.005f, fp(geSize) * 0.001f) : fp(geDensity);
        p.voices = static_cast<int>(std::lround(fp(geVoices)));
        p.positionMs = fp(gePosition);
        p.sprayMs = fp(geSpray);
        p.sizeJitter = pct(geSizeJit);
        p.pitchSt = fp(gePitch);
        p.pitchJitterSt = fp(gePitchJit);
        p.scale = ip(geScale);
        p.reverseProb = pct(geReverse);
        p.window = ip(geWindow);
        p.spread = pct(geSpread);
        p.independent = ip(geStereo) == 1;
        p.speed = fp(geSpeed);
        p.varispeed = bp(geVarispeed);
        p.loopMs = fp(geLoop);
        p.retrig = bp(geRetrig);
        const double bpm = (std::isfinite(ctx.bpm) && ctx.bpm > 1.0) ? ctx.bpm : 120.0;
        p.retrigSamples = syncedSeconds(bp(geSync), fp(geRetrigMs), ip(geRetrigDiv), bpm) * sr_;
        p.align = bp(geAlign);
        p.delayMs = fp(geDelay);
        p.feedback = pct(geFeedback) * 0.95f;
        p.fbHp = fp(geFbHp);
        p.fbLp = fp(geFbLp);
        engine_.process(ch[0], ch[1], wetL_.data(), wetR_.data(), n, p);
        for (int i = 0; i < n; ++i) {
            const float g = out_.next();
            ch[0][i] = wetL_[static_cast<size_t>(i)] * g;
            ch[1][i] = wetR_[static_cast<size_t>(i)] * g;
        }
        mix_.apply(ch, n);
    }
    bool readDisplay(FxDisplay& d) const override {
        const auto& s = engine_.display();
        d.kind = 1;
        d.state = s.frozen.load(std::memory_order_relaxed) ? 1 : 0;
        d.seconds = s.spanSeconds.load(std::memory_order_relaxed);
        d.playhead = s.playhead.load(std::memory_order_relaxed);
        d.activeGrains = s.active.load(std::memory_order_relaxed);
        for (size_t i = 0; i < d.peaks.size() && i < s.peaks.size(); ++i) d.peaks[i] = s.peaks[i].load(std::memory_order_relaxed);
        for (size_t i = 0; i < d.grains.size() && i < s.grainPos.size(); ++i) d.grains[i] = s.grainPos[i].load(std::memory_order_relaxed);
        return true;
    }

    dsp::GrainEngine engine_;
    std::vector<float> wetL_, wetR_;
    MixStage mix_;
    dsp::Smoother out_;
};

}  // namespace

FxDescriptor describeGrainEngine() {
    FxDescriptor d;
    d.typeId = "grain_engine";
    d.name = "Grain Engine";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Captures what it hears and plays it back as overlapping grains: smear, stretch, freeze, loop, scan, reverse and re-pitch the recent past.";
    d.params = {
        P::choice("mode", "Mode", {"Cloud", "Stretch", "Loop"}, 0, "Cloud: grains from a fixed distance behind the live sound. Stretch: a playhead scans the captured audio at the Speed you set (slower = stretched, pitch kept). Loop: holds the last Loop Length and cycles through it."),
        P::logf("size_ms", "Grain Size", "ms", 5.0f, 1000.0f, 120.0f, "Length of each grain. Short = buzzy, granular texture. Long = recognisable phrases that overlap and smear."),
        P::logf("density", "Density", "/s", 0.5f, 200.0f, 18.0f, "New grains per second. Low = sparse fragments; high = a continuous, dense texture."),
        P::flt("position_ms", "Offset", "ms", 0.0f, 8000.0f, 250.0f, "How far back in the captured audio grains are taken from (Cloud), or where the playhead restarts (Stretch, Retrigger)."),
        P::flt("pitch", "Pitch", "st", -24.0f, 24.0f, 0.0f, "Transposes every grain, in semitones."),
        P::flt("speed", "Speed", "x", -2.0f, 2.0f, 0.5f, "Stretch / Loop: how fast the playhead moves through the captured audio. 0.5 = half speed (twice as long), negative = backwards, 0 = a held moment."),
        P::pct("feedback", "Feedback", 0.0f, "Feeds the grains back into the capture buffer so they are re-grained again and again: evolving, resampled textures. Safely limited."),
        P::pct("mix", "Mix", 50.0f, "Balance between the original and the grains. 50% keeps both at full level."),
        P::toggle("freeze", "Freeze", false, "Stops capturing: the grains keep working on the moment you froze. In Stretch the playhead scans the frozen audio endlessly."),
        P::toggle("lock", "Ratio Lock", false, "Ties density to grain size so the number of overlapping grains stays constant when you change the size."),
        P::flt("overlap", "Overlap", "x", 1.0f, 16.0f, 4.0f, "With Ratio Lock: how many grains overlap at once."),
        P::flt("voices", "Voices", "", 1.0f, 64.0f, 32.0f, "Maximum grains sounding at once (a CPU and density ceiling). New grains wait when all voices are busy."),
        P::flt("spray_ms", "Spray", "ms", 0.0f, 2000.0f, 60.0f, "Random extra distance for each grain's start: blurs the sound in time."),
        P::pct("size_jit", "Size Jitter", 0.0f, "Random variation of each grain's length."),
        P::flt("pitch_jit", "Pitch Jitter", "st", 0.0f, 12.0f, 0.0f, "Random pitch per grain. Combine with Scale for musical clouds of notes."),
        P::choice("scale", "Scale", {"Off", "Octaves", "Fifths", "Major", "Minor", "Pentatonic", "Chromatic"}, 0, "Snaps each grain's pitch (Pitch + Jitter) to these intervals."),
        P::pct("reverse", "Reverse", 0.0f, "Chance that a grain plays backwards."),
        P::choice("window", "Window", {"Smooth", "Gaussian", "Full", "Pluck", "Swell"}, 0, "Shape of each grain: smooth (Hann), soft and narrow (Gaussian), full-bodied (Tukey), plucked attack, or a reversed swell."),
        P::pct("spread", "Spread", 50.0f, "How widely grains are scattered across the stereo field."),
        P::choice("stereo", "Stereo", {"Linked", "Independent"}, 0, "Linked: each grain plays both channels. Independent: separate grain streams for left and right (wide, decorrelated)."),
        P::toggle("varispeed", "Varispeed", false, "Stretch / Loop: pitch follows the playhead speed like tape (half speed = an octave down). Off = speed and pitch are independent."),
        P::logf("loop_ms", "Loop Length", "ms", 50.0f, 8000.0f, 1000.0f, "Loop: how much of the recent audio is captured and cycled."),
        P::toggle("retrig", "Retrigger", false, "Restarts the playhead at the Offset (or the loop start) on a regular grid and fires a grain: slices, stutters and rhythmic repeats."),
        P::toggle("sync", "Sync", true, "Locks the retrigger interval to the song tempo."),
        P::choice("retrig_div", "Retrig Division", divisionLabels(), 10, "Retrigger interval as a note length when Sync is on."),
        P::logf("retrig_ms", "Retrig Time", "ms", 20.0f, 4000.0f, 500.0f, "Retrigger interval when Sync is off."),
        P::toggle("align", "Smooth Splice", true, "Stretch / Loop / Freeze: nudges each grain to where its waveform best continues the previous one (WSOLA-style). Cleaner tones, less phasing."),
        P::flt("delay_ms", "Grain Delay", "ms", 0.0f, 2000.0f, 0.0f, "Delays the grains before they are fed back (and adds that echo to the output). 0 = feedback goes straight back into the capture."),
        P::logf("fb_hp", "Feedback HP", "Hz", 20.0f, 2000.0f, 90.0f, "Removes lows from each feedback pass, so repeats thin out instead of booming."),
        P::logf("fb_lp", "Feedback LP", "Hz", 500.0f, 20000.0f, 9000.0f, "Removes highs from each feedback pass, so repeats get darker."),
        P::flt("gain", "Output", "dB", -24.0f, 12.0f, 0.0f, "Level of the grain signal."),
    };
    auto dep = [&](int i, const char* by, float when, const char* why) {
        auto& s = d.params[static_cast<size_t>(i)];
        s.enabledBy = by;
        s.enabledWhen = when;
        s.disabledReason = why;
    };
    dep(geDensity, "lock", 0.0f, "Inactive while Ratio Lock is on: density follows Grain Size x Overlap.");
    dep(geOverlap, "lock", 1.0f, "Only used while Ratio Lock is on.");
    for (int i : {geVoices, geSpray, geSizeJit, gePitchJit, geScale, geReverse, geWindow, geSpread, geStereo, geVarispeed,
                  geLoop, geRetrig, geSync, geRetrigDiv, geRetrigMs, geAlign, geDelay, geFbHp, geFbLp, geGain, geLock, geOverlap})
        d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<GrainEngineFx>(); };
    return d;
}

}  // namespace ih1::fx
