// Reverse — three real reverse articulations (decision C2):
//   Buffer: two half-overlapped reversed readers with Hann windows (smooth backwards stream).
//   Delay : each completed chunk of length L is played backwards during the next L (tempo-aligned
//           when synced to a playing host), with feedback and tone in the loop.
//   Grains: a cloud of reversed grains from the shared grain engine.
#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Grains.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

enum { kMode, kSync, kLength, kDiv, kFeedback, kFade, kPitch, kTone, kSpread, kGrainSize, kDensity, kMix };
constexpr float kBufferLoopScale = 0.62f;  // V5: Buffer mode's two voices recirculate ~1.43x coherently
constexpr float kMaxLen = 4.0f;  // seconds

class Reverse final : public FxBase {
    void onPrepare() override {
        const int maxD = static_cast<int>(kMaxLen * 2.0f * 2.0f * srf()) + 64;  // length x pitch(2x) x spread
        for (auto& l : line_) l.prepare(maxD);
        grains_.prepare(sr_, 6.0f, 0x5eed);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
        lenS_.prepare(sr_, 0.0f);
    }
    void reset() override {
        for (auto& l : line_) l.reset();
        grains_.reset();
        for (auto& f : tone_) f.reset();
        mix_.reset();
        phase_ = 0.0;
        started_ = false;
        for (auto& a : anchor_) a = 0;
        written_ = 0;
        fbState_[0] = fbState_[1] = 0.0f;
    }
    void applyParams(bool snap) override {
        for (auto& f : tone_) f.setup(dsp::Svf::Lowpass, fp(kTone), 0.6f, 0, sr_);
        mix_.setMix(pct(kMix), snap);
    }
    double tailSeconds() const override {
        const double fb = std::clamp(p_[kFeedback] * 0.01, 0.0, 0.95);
        const double len = p_[kLength] * 0.001;
        return 2.0 * len + (fb > 0 ? len * std::log(1e-3) / std::log(fb) : 0.0);
    }

    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const int mode = ip(kMode);
        const double lenSec = std::clamp(syncedSeconds(bp(kSync), fp(kLength), ip(kDiv), ctx.bpm), 0.03, static_cast<double>(kMaxLen));
        const double L = lenSec * sr_;
        const float rate = dsp::semitonesToRatio(fp(kPitch));
        // V5: the recirculation also respects a 120 s ceiling on T60 for the chunk time (as Delay), so 95 % on a
        // two-second chunk is a long cascade, not a practically endless one
        // Delay mode re-reads material when pitched down (slower reversed playback), so its cap is 0.9
        const float fb = std::min({pct(kFeedback), mode == 1 ? 0.9f : 0.95f, std::pow(10.0f, -static_cast<float>(lenSec) / 40.0f)});
        const float spread = pct(kSpread);

        if (mode == 2) {  // ---------------------------------------------------------------- grains
            dsp::GrainCloud::Params gp;
            gp.sizeSamples = fp(kGrainSize) * 0.001f * srf();
            gp.density = fp(kDensity);
            gp.positionSamples = gp.sizeSamples;
            gp.spraySamples = static_cast<float>(L);
            gp.pitchRatio = rate;
            gp.reverseProb = 1.0f;
            gp.spread = spread;
            for (int i = 0; i < n; ++i) {
                float l, r;
                grains_.tick(gp, l, r);
                grains_.write(ch[0][i] + fb * fbState_[0], ch[1][i] + fb * fbState_[1]);
                // V5 Phase 5: feed back the output scaled by the cloud's weight bound (loop gain <= fb <= 0.95; the
                // unnormalised sum of many overlapping reversed grains ran away at high density x size)
                const float ks = grains_.feedbackScale();
                const float ol = tone_[0].process(l), orr = tone_[1].process(r);
                fbState_[0] = ol * ks;
                fbState_[1] = orr * ks;
                ch[0][i] = ol;
                ch[1][i] = orr;
            }
            mix_.apply(ch, n);
            return;
        }

        // V4 Phase 4: every reversed voice (two half-overlapped voices per channel in Buffer mode, one per channel in
        // Delay mode) has its own phase and latches the chunk length and pitch rate when IT wraps (window = 0). The
        // read offset is phase x (1 + rate), so a change mid-chunk used to move the read point by hundreds of samples.
        // The right channel's spread offset glides (a slight speed change, never a jump).
        const bool aligned = mode == 1 && bp(kSync) && ctx.ppqValid && ctx.hostPlaying;
        const int voices = mode == 0 ? 2 : 1;
        if (!started_) {
            started_ = true;
            for (int c = 0; c < 2; ++c)
                for (int v = 0; v < 2; ++v) { vL_[c][v] = L; vRate_[c][v] = rate; vPh_[c][v] = v * 0.5 * L; }
            offCur_ = 0.0;
        }
        const double offTarget = spread * 0.5 * L;
        for (int i = 0; i < n; ++i) {
            float out[2] = {0, 0};
            const double slew = std::clamp(offTarget - offCur_, -0.02, 0.02);
            offCur_ += slew;
            for (int c = 0; c < 2; ++c) {
                for (int v = 0; v < voices; ++v) {
                    double& ph = vPh_[c][v];
                    const double Lv = vL_[c][v];
                    const double pc = std::fmod(ph + (c == 1 ? offCur_ : 0.0) + Lv, Lv);
                    float w;
                    double d;
                    if (mode == 0) {
                        w = 0.5f - 0.5f * std::cos(dsp::kTwoPi * static_cast<float>(pc / Lv));
                        d = pc * (1.0 + vRate_[c][v]) + 1.0;  // read point walks back from the anchor
                    } else {
                        const double fadeLen = std::max(32.0, Lv * std::clamp(pct(kFade), 0.01f, 0.5f));
                        w = 1.0f;
                        if (pc < fadeLen) w = 0.5f - 0.5f * std::cos(dsp::kPi * static_cast<float>(pc / fadeLen));
                        else if (pc > Lv - fadeLen) w = 0.5f - 0.5f * std::cos(dsp::kPi * static_cast<float>((Lv - pc) / fadeLen));
                        d = pc * (1.0 + vRate_[c][v]) + 1.0;
                    }
                    out[c] += w * line_[c].readCubic(static_cast<float>(std::min<double>(d, line_[c].capacity() - 4)));
                    // advance; latch the new length / rate exactly where this voice's window is zero
                    const double prev = pc;
                    ph += 1.0;
                    const double next = std::fmod(ph + (c == 1 ? offCur_ : 0.0) + Lv, Lv);
                    if (next < prev) {
                        // wrapped: start the new chunk at window 0 with the current settings
                        // (each voice restarts at its own window zero; the half-cycle overlap of the two Buffer voices
                        // is kept because each latches at its own wrap, half a chunk apart)
                        vL_[c][v] = L;
                        vRate_[c][v] = rate;
                        ph = next - (c == 1 ? offCur_ : 0.0);
                    }
                }
            }
            for (int c = 0; c < 2; ++c) {
                const float f = tone_[c].process(out[c]);
                // V5 Phase 5: in Buffer mode both half-overlapped voices re-read every sample, and their copies
                // recirculate coherently: the measured per-lap gain is ~1.43 x Feedback (ih1_feedback_audit: growth
                // from 75 % up, stable at 70 %). The Buffer recirculation is scaled by 0.62, so 95 % gives a loop
                // gain of ~0.84 (a long but decaying cascade). Delay mode (one voice, flat window) is already <= fb.
                line_[c].push(ch[c][i] + fb * (mode == 0 ? kBufferLoopScale : 1.0f) * f);
                ch[c][i] = f;
            }
            // Reverse Delay on a playing host: re-phase to the grid only at the start of a chunk (window ~0)
            if (aligned) {
                const double Lv = vL_[0][0];
                const double samplesFromBar = (ctx.ppqPosition * 60.0 / ctx.bpm) * sr_ + i;
                const double snapped = std::fmod(samplesFromBar, Lv);
                const double fadeLen = std::max(32.0, Lv * std::clamp(pct(kFade), 0.01f, 0.5f));
                if (vPh_[0][0] < 1.0 && snapped < 0.1 * fadeLen) { vPh_[0][0] = snapped; vPh_[1][0] = snapped; }
            }
        }
        for (auto& f : tone_) f.flush();
        mix_.apply(ch, n);
    }

    dsp::DelayLine line_[2];
    dsp::GrainCloud grains_;
    dsp::Svf tone_[2];
    dsp::Smoother lenS_;
    MixStage mix_;
    double phase_ = 0.0;
    bool started_ = false;
    double vPh_[2][2] = {{0, 0}, {0, 0}}, vL_[2][2] = {{1, 1}, {1, 1}}, offCur_ = 0.0;
    float vRate_[2][2] = {{1, 1}, {1, 1}};
    int64_t anchor_[2] = {0, 0};
    int64_t written_ = 0;
    float fbState_[2] = {0, 0};
};

}  // namespace

FxDescriptor describeReverse() {
    FxDescriptor d;
    d.typeId = "reverse";
    d.name = "Reverse";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "Plays what you just played backwards: as a smooth backwards stream, as reversed echoes, or as reversed grains.";
    d.params = {
        P::choice("mode", "Mode", {"Reverse Buffer", "Reverse Delay", "Reverse Grains"}, 0, "Buffer: a continuous backwards wash. Delay: each chunk returns backwards on the beat. Grains: a cloud of tiny backwards fragments."),
        P::toggle("sync", "Sync", true, "Lock the chunk length to the song tempo."),
        P::flt("length_ms", "Length", "ms", 50.0f, 4000.0f, 500.0f, "How much audio is flipped each time. Longer = more recognisable backwards phrases.", ParamCurve::Log),
        P::choice("division", "Division", divisionLabels(), 10, "Chunk length as a note value when Sync is on."),
        P::pct("feedback", "Feedback", 0.0f, "Sends the reversed sound back in, so it flips again (and again)."),
        P::flt("fade", "Fade", "%", 1.0f, 50.0f, 12.0f, "Softness of each chunk's start and end (Reverse Delay). Low = choppy, high = smooth swells."),
        P::flt("pitch", "Pitch", "st", -12.0f, 12.0f, 0.0f, "Plays the backwards audio faster (higher) or slower (lower)."),
        P::logf("tone", "Tone", "Hz", 800.0f, 20000.0f, 12000.0f, "Darkens the reversed sound."),
        P::pct("spread", "Spread", 25.0f, "Offsets left and right so the backwards sound fills the stereo field."),
        P::logf("grain_ms", "Grain Size", "ms", 20.0f, 500.0f, 120.0f, "Length of each backwards fragment (Grains mode)."),
        P::logf("density", "Density", "/s", 1.0f, 60.0f, 12.0f, "How many fragments per second (Grains mode)."),
        P::pct("mix", "Mix", 50.0f, "Balance of original and reversed sound. 50% keeps both at full level."),
    };
    for (int i : {kFade, kTone, kSpread, kGrainSize, kDensity}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Reverse>(); };
    return d;
}

}  // namespace ih1::fx
