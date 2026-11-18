// Delay — clean "length-type" digital delay (KADAL KNW-SA0JJFX4HE): time changes crossfade between two
// read heads instead of bending pitch (that is Tape Echo's job). Multi-tap patterns, stereo/ping-pong,
// filtered + optionally saturated feedback with a safety soft-clip, gentle modulation, ducking.
#include <array>

#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

enum { kSync, kTime, kDiv, kStereo, kSpread, kFeedback, kLowCut, kHighCut, kModDepth, kModRate, kDrive, kDuck, kPattern, kMix };

struct Tap { float pos, gain; };
constexpr std::array<std::array<Tap, 4>, 4> kPatterns = {{
    {{{1.0f, 1.0f}, {0, 0}, {0, 0}, {0, 0}}},
    {{{0.5f, 0.6f}, {1.0f, 1.0f}, {0, 0}, {0, 0}}},
    {{{1.0f / 3, 0.5f}, {2.0f / 3, 0.7f}, {1.0f, 1.0f}, {0, 0}}},
    {{{0.25f, 0.4f}, {0.5f, 0.55f}, {0.75f, 0.75f}, {1.0f, 1.0f}}},
}};
constexpr float kMaxSeconds = 3.0f;
constexpr float kXfadeMs = 40.0f;

class Delay final : public FxBase {
    void onPrepare() override {
        const int maxD = static_cast<int>(kMaxSeconds * 1.5f * srf()) + 512;
        for (auto& l : line_) l.prepare(maxD);
        lfo_.prepare(sr_, 11);
        duckEnv_.prepare(sr_, 5.0f, 250.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
        fbS_.prepare(sr_, 30.0f);
    }
    void reset() override {
        for (auto& l : line_) l.reset();
        for (auto& f : lc_) f.reset();
        for (auto& f : hc_) f.reset();
        duckEnv_.reset();
        mix_.reset();
        for (auto& c : cur_) c.xf = 1.0f;
    }
    void applyParams(bool snap) override {
        for (int c = 0; c < 2; ++c) {
            lc_[c].setup(dsp::Svf::Highpass, fp(kLowCut), 0.707f, 0, sr_);
            hc_[c].setup(dsp::Svf::Lowpass, fp(kHighCut), 0.707f, 0, sr_);
        }
        lfo_.setRate(fp(kModRate));
        mix_.setMix(pct(kMix), snap);
        fbS_.setTarget(std::min(pct(kFeedback), 1.0f));
        if (snap) {
            fbS_.snap(std::min(pct(kFeedback), 1.0f));
            for (auto& c : cur_) { c.a = c.b = -1.0f; c.xf = 1.0f; }
        }
    }
    double tailSeconds() const override {
        const double fb = std::clamp(p_[kFeedback] * 0.01, 0.0, 0.999);
        const double t = p_[kTime] * 0.001;
        return fb <= 0.0 ? t : std::min(30.0, t * std::log(1e-3) / std::log(fb) + t);
    }

    void process(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const double sec = std::min<double>(syncedSeconds(bp(kSync), fp(kTime), ip(kDiv), ctx.bpm), kMaxSeconds);
        const int mode = ip(kStereo);  // 0 mono, 1 stereo, 2 ping-pong
        const float tgt[2] = {static_cast<float>(sec * sr_),
                              static_cast<float>(sec * sr_ * (mode == 1 ? 1.0 + pct(kSpread) : 1.0))};
        const auto& pat = kPatterns[static_cast<size_t>(std::clamp(ip(kPattern), 0, 3))];
        const float modAmt = pct(kModDepth) * 0.002f * srf();  // up to 2 ms
        const float drive = pct(kDrive);
        const float duck = pct(kDuck);
        const float xfStep = 1.0f / (kXfadeMs * 0.001f * srf());

        for (int i = 0; i < n; ++i) {
            const float m = lfo_.next() * modAmt;
            const float fbg = fbS_.next();
            float in[2] = {ch[0][i], ch[1][i]};
            if (mode != 1) { const float mono = 0.5f * (in[0] + in[1]); in[0] = mono; in[1] = mode == 2 ? 0.0f : mono; }
            const float env = duckEnv_.process(0.5f * (ch[0][i] + ch[1][i]));
            const float duckGain = 1.0f / (1.0f + duck * 8.0f * env);

            float out[2], fbSig[2];
            for (int c = 0; c < 2; ++c) {
                Head& h = cur_[c];
                if (h.a < 0.0f) { h.a = h.b = tgt[c]; h.xf = 1.0f; }
                if (h.xf >= 1.0f && std::fabs(tgt[c] - h.b) > 0.5f) { h.a = h.b; h.b = tgt[c]; h.xf = 0.0f; }
                const float xf = h.xf;
                if (h.xf < 1.0f) h.xf = std::min(1.0f, h.xf + xfStep);
                const float ga = std::cos(xf * dsp::kPi * 0.5f), gb = std::sin(xf * dsp::kPi * 0.5f);
                const float mod = c == 0 ? m : -m;
                float acc = 0.0f, last = 0.0f;
                for (const Tap& t : pat) {
                    if (t.gain <= 0.0f) break;
                    const float da = std::max(1.0f, h.a * t.pos + mod), db = std::max(1.0f, h.b * t.pos + mod);
                    const float v = (xf < 1.0f ? ga * line_[c].readCubic(da) : 0.0f) + gb * line_[c].readCubic(db);
                    acc += t.gain * v;
                    last = v;
                }
                out[c] = acc;
                float f = hc_[c].process(lc_[c].process(last));
                if (drive > 0.0f) { const float k = 1.0f + 6.0f * drive; f = std::tanh(f * k) / k * (1.0f + drive); }
                fbSig[c] = 2.0f * std::tanh(0.5f * f * fbg);  // safety soft-clip bounds runaway at 100 %
            }
            if (mode == 2) {  // ping-pong: L -> R -> L
                line_[0].push(in[0] + fbSig[1]);
                line_[1].push(in[1] + fbSig[0]);
            } else {
                line_[0].push(in[0] + fbSig[0]);
                line_[1].push(in[1] + fbSig[1]);
            }
            ch[0][i] = out[0] * duckGain;
            ch[1][i] = out[1] * duckGain;
        }
        for (auto& f : lc_) f.flush();
        for (auto& f : hc_) f.flush();
        mix_.apply(ch, n);
    }

    struct Head { float a = -1.0f, b = -1.0f, xf = 1.0f; };
    dsp::DelayLine line_[2];
    dsp::Svf lc_[2], hc_[2];
    dsp::Lfo lfo_;
    dsp::EnvelopeFollower duckEnv_;
    dsp::Smoother fbS_;
    Head cur_[2];
    MixStage mix_;
};

}  // namespace

FxDescriptor describeDelay() {
    FxDescriptor d;
    d.typeId = "delay";
    d.name = "Delay";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "Clean echoes that repeat your sound in time. Changing the time never bends the pitch.";
    d.params = {
        P::toggle("sync", "Sync", true, "Lock the echo time to the song tempo."),
        P::flt("time_ms", "Time", "ms", 1.0f, 2000.0f, 375.0f, "Gap between echoes when Sync is off. Short = slap, long = distinct repeats.", ParamCurve::Log),
        P::choice("division", "Division", divisionLabels(), 8, "Echo time as a note length when Sync is on."),
        P::choice("stereo", "Stereo", {"Mono", "Stereo", "Ping-Pong"}, 1, "Mono: same echo both sides. Stereo: keeps your image. Ping-Pong: echoes bounce left and right."),
        P::pct("spread", "Spread", 0.0f, "Makes the right echo a little later than the left for a wider space (Stereo mode)."),
        P::flt("feedback", "Feedback", "%", 0.0f, 100.0f, 35.0f, "How many times the echo repeats. High values build into long trails."),
        P::logf("low_cut", "Low Cut", "Hz", 20.0f, 2000.0f, 120.0f, "Removes bass from the repeats so they stay out of the way."),
        P::logf("high_cut", "High Cut", "Hz", 1000.0f, 20000.0f, 7000.0f, "Darkens each repeat, like echoes fading into a room."),
        P::pct("mod_depth", "Movement", 10.0f, "Gently wobbles the echo time for a softer, less static sound."),
        P::logf("mod_rate", "Movement Rate", "Hz", 0.05f, 5.0f, 0.6f, "Speed of the wobble."),
        P::pct("drive", "Grit", 0.0f, "Adds warm saturation that builds up with each repeat."),
        P::pct("duck", "Duck", 0.0f, "Turns echoes down while you play and lets them bloom in the gaps."),
        P::choice("pattern", "Pattern", {"Single", "Dual", "Triplet", "Cascade"}, 0, "Rhythmic pattern of taps inside each echo period."),
        P::pct("mix", "Mix", 35.0f, "Balance of original and echoes. 50% keeps both at full level."),
    };
    for (int i : {kSpread, kModRate, kDrive, kDuck}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Delay>(); };
    return d;
}

}  // namespace ih1::fx
