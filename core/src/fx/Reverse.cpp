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

    void process(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const int mode = ip(kMode);
        const double lenSec = std::clamp(syncedSeconds(bp(kSync), fp(kLength), ip(kDiv), ctx.bpm), 0.03, static_cast<double>(kMaxLen));
        const double L = lenSec * sr_;
        const float rate = dsp::semitonesToRatio(fp(kPitch));
        const float fb = std::min(pct(kFeedback), 0.95f);
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
                fbState_[0] = tone_[0].process(l);
                fbState_[1] = tone_[1].process(r);
                ch[0][i] = fbState_[0];
                ch[1][i] = fbState_[1];
            }
            mix_.apply(ch, n);
            return;
        }

        // tempo alignment for Reverse Delay: re-phase at chunk boundaries to the host grid
        const bool aligned = mode == 1 && bp(kSync) && ctx.ppqValid && ctx.hostPlaying;
        const double fadeLen = std::max(32.0, L * std::clamp(pct(kFade), 0.01f, 0.5f));
        for (int i = 0; i < n; ++i) {
            float out[2] = {0, 0};
            for (int c = 0; c < 2; ++c) {
                // right channel optionally offset by up to half a chunk for width
                const double ph0 = std::fmod(phase_ + (c == 1 ? spread * 0.5 * L : 0.0), L);
                if (mode == 0) {
                    for (int v = 0; v < 2; ++v) {
                        const double ph = std::fmod(ph0 + v * 0.5 * L, L);
                        const float w = 0.5f - 0.5f * std::cos(dsp::kTwoPi * static_cast<float>(ph / L));
                        const double d = 2.0 * ph * 0.5 * (1.0 + rate) + 1.0;  // read point walks back from the anchor
                        out[c] += w * line_[c].readCubic(static_cast<float>(std::min<double>(d, line_[c].capacity() - 4)));
                    }
                } else {
                    const double ph = ph0;
                    float w = 1.0f;
                    if (ph < fadeLen) w = 0.5f - 0.5f * std::cos(dsp::kPi * static_cast<float>(ph / fadeLen));
                    else if (ph > L - fadeLen) w = 0.5f - 0.5f * std::cos(dsp::kPi * static_cast<float>((L - ph) / fadeLen));
                    const double d = ph * (1.0 + rate) + 1.0;
                    out[c] = w * line_[c].readCubic(static_cast<float>(std::min<double>(d, line_[c].capacity() - 4)));
                }
            }
            for (int c = 0; c < 2; ++c) {
                const float f = tone_[c].process(out[c]);
                line_[c].push(ch[c][i] + fb * f);
                ch[c][i] = f;
            }
            phase_ += 1.0;
            if (phase_ >= L) {
                phase_ -= L;
                if (aligned) {
                    const double samplesFromBar = (ctx.ppqPosition * 60.0 / ctx.bpm) * sr_ + i;
                    phase_ = std::fmod(samplesFromBar, L);
                    if (phase_ > fadeLen) phase_ = 0.0;  // only snap when near a boundary
                }
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
