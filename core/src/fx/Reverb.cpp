// Reverb — genuinely different algorithms behind one carousel entry (brief §4.3):
//   Room / Hall / Ambience / Cathedral: 8-line Householder FDN (KADAL KNW-ZWRYBGRD0Y): mutually prime
//     delays, per-line first-order absorbent filters from the RT60 targets (Jot), slow delay modulation,
//     input allpass diffusion and a separate early-reflection tap stage.
//   Plate: Dattorro figure-eight tank (KADAL KNW-5QZBASA3A6, delay/tap tables at 29761 Hz, scaled).
//   Gated: FDN tail held while the input is active, then cut quickly (nonlinear articulation).
//   Reverse: FDN tail reversed in overlapping windows -> swells that rise into the next note.
// Freeze makes the loop lossless and closes the input (KADAL CLM-EPHGYBABGD).
#include <array>

#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Denormals.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

enum { rMode, rDecay, rSize, rPredelay, rDamping, rDiffusion, rMod, rEarly, rLowCut, rHighCut, rWidth, rFreeze, rMix };
enum Mode { Room, Hall, Plate, Ambience, Cathedral, Gated, ReverseM };
constexpr float kModeScale[7] = {0.45f, 1.0f, 1.0f, 0.3f, 1.7f, 0.8f, 1.0f};
constexpr int N = 8;
constexpr float kFdnMs[N] = {29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.3f, 67.9f, 73.1f};
constexpr float kErMsL[8] = {4.3f, 9.7f, 14.9f, 21.1f, 29.3f, 37.9f, 47.3f, 61.7f};
constexpr float kErMsR[8] = {5.9f, 11.3f, 17.1f, 23.9f, 31.7f, 41.3f, 51.1f, 66.1f};
constexpr float kErGain[8] = {0.85f, -0.72f, 0.61f, -0.52f, 0.43f, -0.35f, 0.28f, -0.21f};

// ------------------------------------------------------------------------------- Dattorro plate tank
class DattorroTank {
public:
    void prepare(double sr) {
        sr_ = sr;
        const float s = static_cast<float>(sr / 29761.0) * 1.6f;  // allow size up to 1.6x
        auto len = [&](int x) { return static_cast<int>(x * s) + 40; };
        in1_.prepare(len(142)); in2_.prepare(len(107)); in3_.prepare(len(379)); in4_.prepare(len(277));
        apA1_.prepare(len(672 + 32)); apB1_.prepare(len(908 + 32));
        dA1_.prepare(len(4453)); dA2_.prepare(len(3720)); dB1_.prepare(len(4217)); dB2_.prepare(len(3163));
        apA2_.prepare(len(1800)); apB2_.prepare(len(2656));
        lfo_.prepare(sr, 77);
        lfo_.setRate(0.9f);
    }
    void reset() {
        for (auto* a : {&in1_, &in2_, &in3_, &in4_, &apA1_, &apB1_, &apA2_, &apB2_}) a->reset();
        for (auto* d : {&dA1_, &dA2_, &dB1_, &dB2_}) d->reset();
        dampA_ = dampB_ = bw_ = 0.0f;
        fbA_ = fbB_ = 0.0f;
    }
    void configure(float size, float decay, float damping, float diffusion, float mod) {
        k_ = static_cast<float>(sr_ / 29761.0) * size;
        decay_ = decay;
        damp_ = damping;
        diff_ = diffusion;
        exc_ = 16.0f * mod * k_;
        in1_.setDelay(L(142)); in2_.setDelay(L(107)); in3_.setDelay(L(379)); in4_.setDelay(L(277));
        apA2_.setDelay(L(1800)); apB2_.setDelay(L(2656));
    }
    float loopSeconds() const { return (672 + 4453 + 1800 + 3720 + 908 + 4217 + 2656 + 3163) / 29761.0f; }
    void process(float x, float& outL, float& outR) {
        bw_ += 0.9995f * (x - bw_);
        float v = in1_.process(bw_, 0.75f * diff_);
        v = in2_.process(v, 0.75f * diff_);
        v = in3_.process(v, 0.625f * diff_);
        v = in4_.process(v, 0.625f * diff_);
        const float m = lfo_.next() * exc_;
        // tank A
        float a = v + decay_ * fbB_;
        a = apA1_.processMod(a, -0.7f, static_cast<float>(L(672)) + exc_ + m);
        dA1_.push(a);
        float ta = dA1_.tap(L(4453));
        dampA_ += (1.0f - damp_) * (ta - dampA_);
        ta = apA2_.process(dampA_ * decay_, 0.5f);
        dA2_.push(ta);
        // tank B
        float b = v + decay_ * fbA_;
        b = apB1_.processMod(b, -0.7f, static_cast<float>(L(908)) + exc_ - m);
        dB1_.push(b);
        float tb = dB1_.tap(L(4217));
        dampB_ += (1.0f - damp_) * (tb - dampB_);
        tb = apB2_.process(dampB_ * decay_, 0.5f);
        dB2_.push(tb);
        fbA_ = dA2_.tap(L(3720));
        fbB_ = dB2_.tap(L(3163));
        // output taps (Dattorro Table 2); allpass-internal taps are approximated from the delay that follows
        const float g = 0.6f;
        outL = g * (dB1_.tap(L(266)) + dB1_.tap(L(2974)) - dB2_.tap(L(1913)) + dB2_.tap(L(1996)) - dA1_.tap(L(1990)) - dA2_.tap(L(187)) - dA2_.tap(L(1066)));
        outR = g * (dA1_.tap(L(353)) + dA1_.tap(L(3627)) - dA2_.tap(L(1228)) + dA2_.tap(L(2673)) - dB1_.tap(L(2111)) - dB2_.tap(L(335)) - dB2_.tap(L(121)));
    }
    void flush() {
        dampA_ = dsp::flushTiny(dampA_); dampB_ = dsp::flushTiny(dampB_);
        fbA_ = dsp::flushTiny(fbA_); fbB_ = dsp::flushTiny(fbB_); bw_ = dsp::flushTiny(bw_);
    }

private:
    int L(int x) const { return std::max(1, static_cast<int>(x * k_)); }
    double sr_ = 48000.0;
    float k_ = 1.0f, decay_ = 0.5f, damp_ = 0.0005f, diff_ = 1.0f, exc_ = 16.0f;
    dsp::Allpass in1_, in2_, in3_, in4_, apA1_, apB1_, apA2_, apB2_;
    dsp::DelayLine dA1_, dA2_, dB1_, dB2_;
    dsp::Lfo lfo_;
    float dampA_ = 0, dampB_ = 0, bw_ = 0, fbA_ = 0, fbB_ = 0;
};

// ------------------------------------------------------------------------------- main reverb
class Reverb final : public FxBase {
    void onPrepare() override {
        const float maxScale = 1.6f * 1.7f;
        for (int i = 0; i < N; ++i) {
            line_[i].prepare(static_cast<int>(kFdnMs[i] * maxScale * 0.001f * srf()) + 64);
            lfo_[i].prepare(sr_, 40 + static_cast<uint32_t>(i));
            lfo_[i].setRate(0.13f + 0.071f * static_cast<float>(i));
        }
        for (int k = 0; k < 4; ++k) diff_[0][k].prepare(static_cast<int>((3.7f + 2.9f * k) * 0.001f * srf() * maxScale)), diff_[1][k].prepare(static_cast<int>((4.3f + 3.1f * k) * 0.001f * srf() * maxScale));
        pre_.prepare(static_cast<int>(0.26 * sr_));
        er_.prepare(static_cast<int>(0.07f * maxScale * srf()));
        plate_.prepare(sr_);
        for (auto& r : rev_) r.prepare(static_cast<int>(4.2 * sr_));
        gateEnv_.prepare(sr_, 1.0f, 60.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
        inGain_.prepare(sr_, 40.0f);
    }
    void reset() override {
        for (auto& l : line_) l.reset();
        for (auto& d : diff_) for (auto& a : d) a.reset();
        pre_.reset();
        er_.reset();
        plate_.reset();
        for (auto& r : rev_) r.reset();
        for (auto& s : absorb_) s = 0.0f;
        for (auto& f : lc_) f.reset();
        for (auto& f : hc_) f.reset();
        gateEnv_.reset();
        gate_ = 0.0f;
        hold_ = 0;
        revPhase_ = 0.0;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        mode_ = static_cast<Mode>(std::clamp(ip(rMode), 0, 6));
        const float size = (0.4f + 1.2f * pct(rSize)) * kModeScale[mode_];
        const bool freeze = bp(rFreeze);
        float t60 = fp(rDecay);
        if (mode_ == Gated) t60 = std::max(t60, 2.5f);
        const float damping = pct(rDamping);
        for (int i = 0; i < N; ++i) {
            delay_[i] = kFdnMs[i] * size * 0.001f * srf();
            const float dSec = delay_[i] / srf();
            float gLow = std::pow(10.0f, -3.0f * dSec / t60);
            float gHigh = std::pow(10.0f, -3.0f * dSec / (t60 * (1.0f - 0.85f * damping)));
            if (freeze) gLow = gHigh = 1.0f;
            const float ratio = gHigh / gLow;
            absB_[i] = (1.0f - ratio) / (1.0f + ratio);  // Jot first-order absorbent filter
            absK_[i] = gLow;
        }
        for (int k = 0; k < 4; ++k) {
            diffD_[0][k] = static_cast<int>((3.7f + 2.9f * k) * 0.001f * srf() * size);
            diffD_[1][k] = static_cast<int>((4.3f + 3.1f * k) * 0.001f * srf() * size);
            diff_[0][k].setDelay(std::max(1, diffD_[0][k]));
            diff_[1][k].setDelay(std::max(1, diffD_[1][k]));
        }
        erScale_ = size;
        // plate: decay coefficient from T60 (KADAL: T60 = -3 T_loop / log10(decay^4))
        const float plateSize = 0.6f + 0.8f * pct(rSize);
        float pd = freeze ? 1.0f : std::pow(10.0f, -3.0f * plate_.loopSeconds() * plateSize / (4.0f * std::max(0.1f, t60)));
        plate_.configure(plateSize, std::min(pd, freeze ? 1.0f : 0.9995f), 0.0005f + 0.6f * damping, 0.4f + 0.6f * pct(rDiffusion), pct(rMod));
        for (int c = 0; c < 2; ++c) {
            lc_[c].setup(dsp::Svf::Highpass, fp(rLowCut), 0.6f, 0, sr_);
            hc_[c].setup(dsp::Svf::Lowpass, fp(rHighCut), 0.6f, 0, sr_);
        }
        inGain_.setTarget(freeze ? 0.0f : 1.0f);
        if (snap) inGain_.snap(inGain_.target());
        mix_.setMix(pct(rMix), snap);
    }
    double tailSeconds() const override { return bp(rFreeze) ? 60.0 : std::min(60.0, 1.6 * p_[rDecay] + 0.3); }

    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float pre = fp(rPredelay) * 0.001f * srf();
        const float diffusion = 0.4f + 0.35f * pct(rDiffusion);
        const float modDepth = pct(rMod) * 0.0006f * srf();  // up to 0.6 ms
        const float early = pct(rEarly);
        const float late = 1.0f - 0.7f * early;
        const float width = pct(rWidth);
        const bool plate = mode_ == Plate;
        const double revLen = std::clamp(static_cast<double>(fp(rDecay)) * 0.5, 0.15, 2.0) * sr_;
        for (int i = 0; i < n; ++i) {
            const float g = inGain_.next();
            const float inL = ch[0][i] * g, inR = ch[1][i] * g;
            pre_.push(0.5f * (inL + inR));
            const float x = pre_.readLinear(std::max(0.0f, pre));
            er_.push(x);
            float wl = 0.0f, wr = 0.0f;
            if (plate) {
                plate_.process(x, wl, wr);
                wl *= late; wr *= late;
            } else {
                // input diffusion (separate chains -> decorrelated injection)
                float dl = x, dr = x;
                for (int k = 0; k < 4; ++k) { dl = diff_[0][k].process(dl, diffusion); dr = diff_[1][k].process(dr, diffusion); }
                float s[N];
                float sum = 0.0f;
                for (int j = 0; j < N; ++j) {
                    const float d = delay_[j] + (j % 2 ? -1.0f : 1.0f) * modDepth * (0.5f + 0.5f * lfo_[j].next());
                    float v = line_[j].readCubic(std::max(2.0f, d));
                    // Jot absorbent filter: H = k (1-b) / (1 - b z^-1)
                    absorb_[j] = absK_[j] * (1.0f - absB_[j]) * v + absB_[j] * absorb_[j];
                    s[j] = absorb_[j];
                    sum += s[j];
                }
                const float h = 2.0f / N * sum;  // Householder reflection
                for (int j = 0; j < N; ++j) {
                    const float inj = (j % 2 ? dr : dl) * (j % 4 < 2 ? 0.5f : -0.5f);
                    line_[j].push(s[j] - h + inj);
                }
                wl = (s[0] - s[2] + s[4] - s[6]) * 0.5f * late;
                wr = (s[1] - s[3] + s[5] - s[7]) * 0.5f * late;
            }
            // early reflections
            if (early > 0.0f) {
                float el = 0.0f, eR = 0.0f;
                for (int k = 0; k < 8; ++k) {
                    el += kErGain[k] * er_.readLinear(kErMsL[k] * erScale_ * 0.001f * srf());
                    eR += kErGain[k] * er_.readLinear(kErMsR[k] * erScale_ * 0.001f * srf());
                }
                wl += early * 0.6f * el;
                wr += early * 0.6f * eR;
            }
            // articulation
            if (mode_ == Gated) {
                const float env = gateEnv_.process(x);
                const int holdS = static_cast<int>(std::clamp(fp(rDecay) * 0.12f, 0.08f, 0.8f) * srf());
                if (env > 0.0032f) hold_ = holdS;  // ~ -50 dBFS
                const float tgt = hold_ > 0 ? 1.0f : 0.0f;
                if (hold_ > 0) --hold_;
                gate_ += (tgt > gate_ ? 0.05f : 0.0015f) * (tgt - gate_);
                wl *= gate_; wr *= gate_;
            } else if (mode_ == ReverseM) {
                rev_[0].push(wl);
                rev_[1].push(wr);
                float ol = 0.0f, orr = 0.0f;
                for (int v = 0; v < 2; ++v) {
                    const double ph = std::fmod(revPhase_ + v * 0.5 * revLen, revLen);
                    const float w = 0.5f - 0.5f * std::cos(dsp::kTwoPi * static_cast<float>(ph / revLen));
                    const float d = static_cast<float>(2.0 * ph + 1.0);
                    ol += w * rev_[0].readLinear(d);
                    orr += w * rev_[1].readLinear(d);
                }
                revPhase_ += 1.0;
                if (revPhase_ >= revLen) revPhase_ -= revLen;
                wl = ol; wr = orr;
            }
            // width (M/S) and tone
            const float M = 0.5f * (wl + wr), S = 0.5f * (wl - wr) * width;
            ch[0][i] = hc_[0].process(lc_[0].process(M + S));
            ch[1][i] = hc_[1].process(lc_[1].process(M - S));
        }
        for (auto& a : absorb_) a = dsp::flushTiny(a);
        plate_.flush();
        for (int c = 0; c < 2; ++c) { lc_[c].flush(); hc_[c].flush(); }
        mix_.apply(ch, n);
    }

    Mode mode_ = Hall;
    dsp::DelayLine line_[N];
    dsp::Lfo lfo_[N];
    float delay_[N] = {}, absB_[N] = {}, absK_[N] = {}, absorb_[N] = {};
    dsp::Allpass diff_[2][4];
    int diffD_[2][4] = {};
    dsp::DelayLine pre_, er_, rev_[2];
    float erScale_ = 1.0f;
    DattorroTank plate_;
    dsp::Svf lc_[2], hc_[2];
    dsp::EnvelopeFollower gateEnv_;
    float gate_ = 0.0f;
    int hold_ = 0;
    double revPhase_ = 0.0;
    dsp::Smoother inGain_;
    MixStage mix_;
};

}  // namespace

FxDescriptor describeReverb() {
    FxDescriptor d;
    d.typeId = "reverb";
    d.name = "Reverb";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "Places the sound in a space, from a small room to a vast hall, a bright plate, a gated burst or a reverse swell.";
    d.params = {
        P::choice("mode", "Space", {"Room", "Hall", "Plate", "Ambience", "Cathedral", "Gated", "Reverse"}, 1, "Type of space. Each mode is a different reverb design, not just a different size."),
        P::logf("decay", "Decay", "s", 0.1f, 30.0f, 2.5f, "How long the reverb takes to die away."),
        P::pct("size", "Size", 50.0f, "How big the space feels: spacing of reflections and echo density."),
        P::flt("predelay", "Pre-delay", "ms", 0.0f, 250.0f, 20.0f, "Gap before the reverb starts. Keeps the dry sound clear and upfront."),
        P::pct("damping", "Damping", 40.0f, "How quickly the highs fade compared with the lows. High = darker, softer rooms."),
        P::pct("diffusion", "Diffusion", 75.0f, "How smooth the reverb is. Low = audible separate echoes, high = a dense wash."),
        P::pct("modulation", "Modulation", 30.0f, "Slow movement inside the reverb that removes metallic ringing and adds lushness."),
        P::pct("early", "Early", 25.0f, "Level of the first reflections that tell the ear how big the room is."),
        P::logf("low_cut", "Low Cut", "Hz", 20.0f, 1000.0f, 80.0f, "Removes bass from the reverb to avoid mud."),
        P::logf("high_cut", "High Cut", "Hz", 1000.0f, 20000.0f, 9000.0f, "Softens the top of the reverb."),
        P::flt("width", "Width", "%", 0.0f, 150.0f, 100.0f, "Stereo width of the reverb."),
        P::toggle("freeze", "Freeze", false, "Holds the current reverb forever and stops new sound entering it."),
        P::pct("mix", "Mix", 30.0f, "Balance between dry sound and reverb. 50% keeps both at full level."),
    };
    for (int i : {rDiffusion, rMod, rEarly, rLowCut, rWidth}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Reverb>(); };
    return d;
}

}  // namespace ih1::fx
