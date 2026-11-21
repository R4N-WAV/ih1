// Tone family: Filter/Wah, EQ/Dynamic EQ, Saturation, Stereo Width.
// KADAL: KNW-R8PP5DNBY0 (trapezoidal SVF), KNW-97AZWH7FXW/KNW-Y03ZNFMHTD (EQ character),
// KNW-W26C85ANW0 + KNW-MCXD9ZQP83 (saturation design, ADAA + 2x), IMP-C783HAF7B6 lesson 16 (auto gain),
// KNW-1VXWHJAGBV + KNW-61XX879RSM (M/S width, mono maker, decorrelation).
#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Nonlinear.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

constexpr int kCtrl = 16;  // control-rate interval (samples)

// ==================================================================================== Filter / Wah
enum { fType, fCutoff, fRes, fDrive, fMod, fSync, fRate, fDiv, fDepth, fShape, fEnvAmt, fAttack, fRelease, fPhase, fMix };

class Filter final : public FxBase {
    void onPrepare() override {
        lfoL_.prepare(sr_, 21);
        lfoR_.prepare(sr_, 22);
        env_.prepare(sr_, 10.0f, 150.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        cutS_.prepare(sr_ / kCtrl, 30.0f);
    }
    void reset() override {
        for (auto& f : a_) f.reset();
        for (auto& f : b_) f.reset();
        for (auto& s : sh_) s.reset();
        env_.reset();
        mix_.reset();
        ctr_ = 0;
        lfoL_.setPhase(0.0f);
        lfoR_.setPhase(fp(fPhase) / 360.0f);
    }
    void applyParams(bool snap) override {
        env_.set(fp(fAttack), fp(fRelease));
        lfoL_.setShape(ip(fShape));
        lfoR_.setShape(ip(fShape));
        cutS_.setTarget(std::log2(fp(fCutoff)));
        if (snap) cutS_.snap(cutS_.target());
        mix_.setMix(pct(fMix), snap);
        for (auto& s : sh_) s.setShape(dsp::Shape::Tanh);
        if (snap) { lfoR_.setPhase(lfoL_.phase() + fp(fPhase) / 360.0f); }
    }
    void setup(int c, float fc) {
        const int type = ip(fType);
        const float res = pct(fRes);
        switch (type) {
            case 0: a_[c].setup(dsp::Svf::Lowpass, fc, 0.5f + res * 19.5f, 0, sr_); break;
            case 1: a_[c].setup(dsp::Svf::Lowpass, fc, 0.541f, 0, sr_); b_[c].setup(dsp::Svf::Lowpass, fc, 1.307f + res * 18.0f, 0, sr_); break;
            case 2: a_[c].setup(dsp::Svf::Highpass, fc, 0.5f + res * 19.5f, 0, sr_); break;
            case 3: a_[c].setup(dsp::Svf::Highpass, fc, 0.541f, 0, sr_); b_[c].setup(dsp::Svf::Highpass, fc, 1.307f + res * 18.0f, 0, sr_); break;
            case 4: a_[c].setup(dsp::Svf::Bandpass, fc, 0.7f + res * 14.0f, 0, sr_); break;
            case 5: a_[c].setup(dsp::Svf::Notch, fc, 0.5f + res * 8.0f, 0, sr_); break;
            default: a_[c].setup(dsp::Svf::Bandpass, fc, 2.5f + res * 12.0f, 0, sr_); break;  // wah
        }
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const int mod = ip(fMod);  // 0 off, 1 LFO, 2 envelope, 3 both
        const float rate = bp(fSync) ? static_cast<float>(1.0 / dsp::divisionSeconds(ip(fDiv), ctx.bpm)) : fp(fRate);
        lfoL_.setRate(rate);
        lfoR_.setRate(rate);
        const float drive = 1.0f + pct(fDrive) * 15.0f;
        const float comp = 1.0f / std::sqrt(drive);
        const int type = ip(fType);
        const bool cascade = type == 1 || type == 3;
        const float wahGain = type == 6 ? 2.5f : (type == 4 ? 1.5f : 1.0f);
        for (int i = 0; i < n; ++i) {
            const float l = lfoL_.next(), r = lfoR_.next();
            const float e = env_.process(0.5f * (ch[0][i] + ch[1][i]));
            if (ctr_ == 0) {
                const float base = cutS_.next();
                const float envOct = (mod >= 2) ? fp(fEnvAmt) * dsp::clamp01((dsp::gainToDb(e) + 48.0f) / 42.0f) : 0.0f;
                for (int c = 0; c < 2; ++c) {
                    const float lfoOct = (mod == 1 || mod == 3) ? fp(fDepth) * (c ? r : l) : 0.0f;
                    setup(c, std::exp2(std::clamp(base + lfoOct + envOct, 4.3f, 14.3f)));
                }
            }
            ctr_ = (ctr_ + 1) % kCtrl;
            for (int c = 0; c < 2; ++c) {
                float x = ch[c][i];
                if (drive > 1.001f) x = sh_[c].process(x * drive) * comp * 1.2f;
                float y = a_[c].process(x);
                if (cascade) y = b_[c].process(y);
                ch[c][i] = y * wahGain;
            }
        }
        for (auto& f : a_) f.flush();
        for (auto& f : b_) f.flush();
        mix_.apply(ch, n);
    }

    dsp::Svf a_[2], b_[2];
    dsp::AdaaShaper sh_[2];
    dsp::Lfo lfoL_, lfoR_;
    dsp::EnvelopeFollower env_;
    dsp::Smoother cutS_;
    MixStage mix_;
    int ctr_ = 0;
};

// ==================================================================================== EQ / Dynamic EQ
enum { eHp, eLsF, eLsG, eB1F, eB1G, eB1Q, eB2F, eB2G, eB2Q, eHsF, eHsG, eLp, eDynRange, eDynThr, eDynAtt, eDynRel, eOut };

class Eq final : public FxBase {
    void onPrepare() override {
        env_.prepare(sr_, 5.0f, 120.0f);
        out_.prepare(sr_, 20.0f);
    }
    void reset() override {
        for (int c = 0; c < 2; ++c) for (auto& f : f_[c]) f.reset();
        det_.reset();
        env_.reset();
        ctr_ = 0;
        dynDb_ = 0.0f;
    }
    void applyParams(bool snap) override {
        env_.set(fp(eDynAtt), fp(eDynRel));
        out_.setTarget(dsp::dbToGain(fp(eOut)));
        for (int k = 0; k < 9; ++k) {
            static const int ids[9] = {eHp, eLsF, eLsG, eB1F, eB1G, eB1Q, eB2F, eB2G, eB2Q};
            (void)ids;
        }
        tgt_ = {std::log2(fp(eHp)), std::log2(fp(eLsF)), fp(eLsG), std::log2(fp(eB1F)), fp(eB1G), fp(eB1Q),
                std::log2(fp(eB2F)), fp(eB2G), fp(eB2Q), std::log2(fp(eHsF)), fp(eHsG), std::log2(fp(eLp))};
        if (snap || !init_) { cur_ = tgt_; out_.snap(out_.target()); init_ = true; updateCoefs(); }
    }
    void updateCoefs() {
        for (int c = 0; c < 2; ++c) {
            f_[c][0].setup(dsp::Svf::Highpass, std::exp2(cur_[0]), 0.707f, 0, sr_);
            f_[c][1].setup(dsp::Svf::LowShelf, std::exp2(cur_[1]), 0.707f, cur_[2], sr_);
            f_[c][2].setup(dsp::Svf::Bell, std::exp2(cur_[3]), cur_[5], cur_[4], sr_);
            f_[c][3].setup(dsp::Svf::Bell, std::exp2(cur_[6]), cur_[8], cur_[7] + dynDb_, sr_);
            f_[c][4].setup(dsp::Svf::HighShelf, std::exp2(cur_[9]), 0.707f, cur_[10], sr_);
            f_[c][5].setup(dsp::Svf::Lowpass, std::exp2(cur_[11]), 0.707f, 0, sr_);
        }
        det_.setup(dsp::Svf::Bandpass, std::exp2(cur_[6]), std::max(0.7f, cur_[8]), 0, sr_);
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        const float range = fp(eDynRange), thr = fp(eDynThr);
        const bool dyn = std::fabs(range) > 0.05f;
        const bool hpOn = fp(eHp) > 20.5f, lpOn = fp(eLp) < 19900.0f;
        for (int i = 0; i < n; ++i) {
            if (dyn) {
                const float d = det_.process(0.5f * (ch[0][i] + ch[1][i]));
                env_.process(d);
            }
            if (ctr_ == 0) {
                for (size_t k = 0; k < cur_.size(); ++k) cur_[k] += 0.15f * (tgt_[k] - cur_[k]);
                float tgtDyn = 0.0f;
                if (dyn) {
                    const float over = dsp::gainToDb(env_.value()) - thr;
                    tgtDyn = range * dsp::clamp01(over / 12.0f);
                }
                dynDb_ += 0.5f * (tgtDyn - dynDb_);
                updateCoefs();
            }
            ctr_ = (ctr_ + 1) % kCtrl;
            const float g = out_.next();
            for (int c = 0; c < 2; ++c) {
                float x = ch[c][i];
                if (hpOn) x = f_[c][0].process(x);
                for (int k = 1; k <= 4; ++k) x = f_[c][k].process(x);
                if (lpOn) x = f_[c][5].process(x);
                ch[c][i] = x * g;
            }
        }
        for (int c = 0; c < 2; ++c) for (auto& f : f_[c]) f.flush();
    }

    dsp::Svf f_[2][6];
    dsp::Svf det_;
    dsp::EnvelopeFollower env_;
    dsp::Smoother out_;
    std::array<float, 12> tgt_{}, cur_{};
    bool init_ = false;
    float dynDb_ = 0.0f;
    int ctr_ = 0;
};

// ==================================================================================== Saturation
enum { sStyle, sDrive, sBias, sTone, sLowCut, sAuto, sOut, sMix };
constexpr dsp::Shape kStyleShape[6] = {dsp::Shape::Asym, dsp::Shape::SoftCubic, dsp::Shape::Tanh, dsp::Shape::HardClip, dsp::Shape::Fold, dsp::Shape::Arctan};

class Saturation final : public FxBase {
    void onPrepare() override {
        for (auto& o : os_) o.prepareVariant(maxBlock_, latMode_ == LatencyMode::Quality ? dsp::Oversampler2x::Variant::Fir : (latMode_ == LatencyMode::Low ? dsp::Oversampler2x::Variant::MinPhaseIir : dsp::Oversampler2x::Variant::Bypass));
        mix_.prepare(sr_, maxBlock_, os_[0].latency(), MixLaw::Linear);
        drive_.prepare(sr_, 30.0f);
        out_.prepare(sr_, 30.0f);
        bias_.prepare(sr_, 30.0f);
        work_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        inPow_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        for (auto& d : dc_) d.prepare(sr_ * os_[0].factor(), 8.0f);
    }
    int latencySamples() const override { return os_[0].latency(); }
    void reset() override {
        for (auto& o : os_) o.reset();
        for (auto& s : sh_) s.reset();
        for (auto& f : pre_) f.reset();
        for (auto& f : post_) f.reset();
        for (auto& f : lc_) f.reset();
        for (auto& d : dc_) d.reset();
        mix_.reset();
        rmsIn_ = rmsOut_ = 1e-6f;
        autoG_ = 1.0f;
        primed_ = false;
        warm_ = static_cast<int>(0.04 * sr_);
    }
    void applyParams(bool snap) override {
        for (int c = 0; c < 2; ++c) {
            sh_[c].setShape(kStyleShape[std::clamp(ip(sStyle), 0, 5)]);
            pre_[c].setup(dsp::Svf::HighShelf, 900.0f, 0.5f, fp(sTone), sr_);
            post_[c].setup(dsp::Svf::HighShelf, 900.0f, 0.5f, -0.6f * fp(sTone), sr_);
            lc_[c].setup(dsp::Svf::Highpass, fp(sLowCut), 0.6f, 0, sr_);
        }
        drive_.setTarget(dsp::dbToGain(fp(sDrive)));
        out_.setTarget(dsp::dbToGain(fp(sOut)));
        bias_.setTarget(pct(sBias) * 0.5f);
        if (snap) { drive_.snap(drive_.target()); out_.snap(out_.target()); bias_.snap(bias_.target()); }
        mix_.setMix(pct(sMix), snap);
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const bool autoGain = bp(sAuto);
        const float aRms = 1.0f - std::exp(-1.0f / (0.3f * srf()));
        // drive/bias ramps computed once per block (per-sample inside the oversampled loop)
        const float d0 = drive_.current(), b0 = bias_.current();
        float d1 = d0, b1 = b0;
        for (int i = 0; i < n; ++i) { d1 = drive_.next(); b1 = bias_.next(); }
        for (int i = 0; i < n; ++i) inPow_[static_cast<size_t>(i)] = 0.0f;
        for (int c = 0; c < 2; ++c) {
            float* x = ch[c];
            for (int i = 0; i < n; ++i) {
                const float in = lc_[c].process(x[i]);
                inPow_[static_cast<size_t>(i)] += 0.5f * in * in;
                work_[static_cast<size_t>(i)] = pre_[c].process(in);
            }
            float* hi = os_[c].up(work_.data(), n);
            for (int k = 0; k < os_[c].factor() * n; ++k) {
                const float t = static_cast<float>(k) / static_cast<float>(os_[c].factor() * n);
                const float drv = d0 + t * (d1 - d0), bias = b0 + t * (b1 - b0);
                hi[k] = dc_[c].process(sh_[c].process(hi[k] * drv + bias));
            }
            os_[c].down(hi, x, n);
            for (int i = 0; i < n; ++i) x[i] = post_[c].process(x[i]);
        }
        // auto gain: sample-ordered RMS tracking (block-size independent).
        // V3 safety fix: a freshly staged / reset Saturation used to start at unity gain with empty 300 ms RMS
        // trackers, so the first ~0.3 s played the full drive gain (+9 dB at defaults, +23 dB measured with a hot
        // cached draft, hitting the engine's +12 dBFS clamp). Now the gain starts at a conservative 1 / drive, and
        // for the first 40 ms of real signal (warm-up) both trackers run at 5 ms and the gain follows within 1 ms.
        // After that, reductions are still tracked fast (5 ms) and increases slowly (42 ms): auto gain may lag
        // downwards, never upwards into a spike.
        const float fastDown = 1.0f - std::exp(-1.0f / (0.005f * srf()));
        const float aWarm = 1.0f - std::exp(-1.0f / (0.005f * srf()));
        const float gWarm = 1.0f - std::exp(-1.0f / (0.001f * srf()));
        if (autoGain && !primed_) { autoG_ = std::clamp(1.0f / std::max(1.0f, drive_.current()), 0.05f, 1.0f); primed_ = true; }
        for (int i = 0; i < n; ++i) {
            const float outPow = 0.5f * (ch[0][i] * ch[0][i] + ch[1][i] * ch[1][i]);
            const bool warm = warm_ > 0;
            if (warm && inPow_[static_cast<size_t>(i)] > 1e-10f) --warm_;
            const float a = warm ? aWarm : aRms;
            rmsIn_ += a * (inPow_[static_cast<size_t>(i)] - rmsIn_);
            rmsOut_ += a * (outPow - rmsOut_);
            const float target = autoGain ? std::clamp(std::sqrt(rmsIn_ / std::max(rmsOut_, 1e-10f)), 0.05f, 4.0f) : 1.0f;
            autoG_ += (warm ? gWarm : (target < autoG_ ? fastDown : 0.0005f)) * (target - autoG_);
            const float g = autoG_ * out_.next();
            ch[0][i] *= g;
            ch[1][i] *= g;
        }
        for (int c = 0; c < 2; ++c) { pre_[c].flush(); post_[c].flush(); lc_[c].flush(); }
        mix_.apply(ch, n);
    }

    dsp::Oversampler2x os_[2];
    dsp::AdaaShaper sh_[2];
    dsp::Svf pre_[2], post_[2], lc_[2];
    dsp::DcBlocker dc_[2];
    dsp::Smoother drive_, out_, bias_;
    std::vector<float> work_, inPow_;
    MixStage mix_;
    float rmsIn_ = 1e-6f, rmsOut_ = 1e-6f, autoG_ = 1.0f;
    bool primed_ = false;
    int warm_ = 0;
};

// ==================================================================================== Stereo Width
enum { wWidth, wMono, wSpread, wBalance, wOut };

class Width final : public FxBase {
    void onPrepare() override {
        static const float msL[4] = {3.1f, 5.3f, 7.9f, 11.3f}, msR[4] = {4.3f, 6.7f, 9.7f, 13.1f};
        for (int k = 0; k < 4; ++k) {
            apL_[k].prepare(static_cast<int>(msL[k] * 0.001f * srf()));
            apR_[k].prepare(static_cast<int>(msR[k] * 0.001f * srf()));
        }
        width_.prepare(sr_, 30.0f);
        spread_.prepare(sr_, 30.0f);
        bal_.prepare(sr_, 30.0f);
        out_.prepare(sr_, 30.0f);
    }
    void reset() override {
        for (auto& a : apL_) a.reset();
        for (auto& a : apR_) a.reset();
        xo_.reset();
    }
    void applyParams(bool snap) override {
        width_.setTarget(pct(wWidth));
        spread_.setTarget(pct(wSpread));
        bal_.setTarget(fp(wBalance) * 0.01f);
        out_.setTarget(dsp::dbToGain(fp(wOut)));
        if (snap) { width_.snap(width_.target()); spread_.snap(spread_.target()); bal_.snap(bal_.target()); out_.snap(out_.target()); }
        monoOn_ = fp(wMono) > 20.5f;
        if (monoOn_) xo_.setup(fp(wMono), sr_);
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        for (int i = 0; i < n; ++i) {
            const float w = width_.next(), sp = spread_.next(), b = bal_.next(), g = out_.next();
            const float M = 0.5f * (ch[0][i] + ch[1][i]);
            float S = 0.5f * (ch[0][i] - ch[1][i]) * w;
            if (monoOn_) { float lo, hi; xo_.process(S, lo, hi); S = hi; }
            // decorrelated difference added to the side only: L+R stays exactly 2M (mono-safe)
            float dl = M, dr = M;
            for (auto& a : apL_) dl = a.process(dl, 0.6f);
            for (auto& a : apR_) dr = a.process(dr, 0.6f);
            S += sp * 0.5f * (dl - dr) * std::min(1.0f, w);
            float L = M + S, R = M - S;
            L *= std::min(1.0f, 1.0f - b) * g;
            R *= std::min(1.0f, 1.0f + b) * g;
            ch[0][i] = L;
            ch[1][i] = R;
        }
    }

    dsp::Allpass apL_[4], apR_[4];
    dsp::Crossover xo_;
    dsp::Smoother width_, spread_, bal_, out_;
    bool monoOn_ = false;
};

}  // namespace

FxDescriptor describeFilter() {
    FxDescriptor d;
    d.typeId = "filter";
    d.name = "Filter / Wah";
    d.family = FxFamily::Modulation;
    d.tooltip = "Sweeps a resonant filter through your sound by hand, with an LFO, or following how hard you play (auto-wah).";
    d.params = {
        P::choice("type", "Type", {"Low-pass 12", "Low-pass 24", "High-pass 12", "High-pass 24", "Band-pass", "Notch", "Wah"}, 1, "Which part of the sound passes. Low-pass darkens, high-pass thins, band/wah keeps a vocal-like slice."),
        P::logf("cutoff", "Cutoff", "Hz", 20.0f, 20000.0f, 1200.0f, "Where the filter acts. Sweeping it is the classic filter move."),
        P::pct("resonance", "Resonance", 25.0f, "Emphasis at the cutoff. High values whistle and sing."),
        P::pct("drive", "Drive", 0.0f, "Pushes the signal into gentle saturation before the filter."),
        P::choice("mod", "Movement", {"Off", "LFO", "Envelope", "LFO + Env"}, 0, "What moves the cutoff: a steady wave (LFO), your playing dynamics (Envelope), or both."),
        P::toggle("sync", "Sync", false, "Lock the LFO to the song tempo."),
        P::logf("rate", "LFO Rate", "Hz", 0.02f, 20.0f, 1.0f, "Speed of the LFO sweep."),
        P::choice("division", "LFO Division", divisionLabels(), 10, "LFO cycle as a note length when Sync is on."),
        P::flt("depth", "LFO Depth", "oct", 0.0f, 4.0f, 1.5f, "How far the LFO sweeps, in octaves."),
        P::choice("shape", "LFO Shape", {"Sine", "Triangle", "Saw", "Square", "Random", "Drift"}, 0, "Shape of the LFO movement."),
        P::flt("env_amt", "Env Amount", "oct", -4.0f, 4.0f, 2.5f, "How far louder playing opens (or closes) the filter. This is the auto-wah."),
        P::logf("attack", "Env Attack", "ms", 0.5f, 200.0f, 8.0f, "How quickly the envelope reacts to new notes."),
        P::logf("release", "Env Release", "ms", 10.0f, 2000.0f, 180.0f, "How slowly the envelope falls back."),
        P::flt("phase", "Stereo Phase", "deg", 0.0f, 180.0f, 0.0f, "Offsets the right channel's LFO for a moving stereo sweep."),
        P::pct("mix", "Mix", 100.0f, "Balance between original and filtered sound."),
    };
    for (int i : {fDrive, fShape, fAttack, fRelease, fPhase}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Filter>(); };
    return d;
}

FxDescriptor describeEq() {
    FxDescriptor d;
    d.typeId = "eq";
    d.name = "EQ";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Shapes the tone with shelves, bells and cuts. Bell 2 can react to the music (dynamic EQ).";
    d.params = {
        P::logf("hp", "Low Cut", "Hz", 20.0f, 1000.0f, 20.0f, "Removes rumble below this frequency (20 Hz = off)."),
        P::logf("ls_freq", "Low Freq", "Hz", 30.0f, 600.0f, 120.0f, "Corner of the low shelf."),
        P::flt("ls_gain", "Low", "dB", -18.0f, 18.0f, 0.0f, "Boosts or reduces the low end (body, warmth)."),
        P::logf("b1_freq", "Bell 1 Freq", "Hz", 40.0f, 18000.0f, 500.0f, "Centre of the first bell."),
        P::flt("b1_gain", "Bell 1", "dB", -18.0f, 18.0f, 0.0f, "Boost or cut around Bell 1 Freq."),
        P::flt("b1_q", "Bell 1 Q", "", 0.2f, 12.0f, 1.0f, "Width of Bell 1. Low = broad and musical, high = narrow and surgical.", ParamCurve::Log),
        P::logf("b2_freq", "Bell 2 Freq", "Hz", 40.0f, 18000.0f, 3000.0f, "Centre of the second (dynamic-capable) bell."),
        P::flt("b2_gain", "Bell 2", "dB", -18.0f, 18.0f, 0.0f, "Static boost or cut around Bell 2 Freq."),
        P::flt("b2_q", "Bell 2 Q", "", 0.2f, 12.0f, 1.5f, "Width of Bell 2.", ParamCurve::Log),
        P::logf("hs_freq", "High Freq", "Hz", 1000.0f, 18000.0f, 8000.0f, "Corner of the high shelf."),
        P::flt("hs_gain", "High", "dB", -18.0f, 18.0f, 0.0f, "Adds or removes air and brightness."),
        P::logf("lp", "High Cut", "Hz", 1000.0f, 20000.0f, 20000.0f, "Removes everything above this frequency (20 kHz = off)."),
        P::flt("dyn_range", "Dynamic", "dB", -18.0f, 18.0f, 0.0f, "Extra cut (negative) or boost (positive) on Bell 2 only when that band gets loud. 0 = static EQ."),
        P::flt("dyn_thr", "Dyn Threshold", "dB", -60.0f, 0.0f, -24.0f, "Level in Bell 2's band where the dynamic action starts."),
        P::logf("dyn_att", "Dyn Attack", "ms", 0.5f, 100.0f, 5.0f, "How fast the dynamic band reacts."),
        P::logf("dyn_rel", "Dyn Release", "ms", 10.0f, 1000.0f, 120.0f, "How fast it lets go."),
        P::flt("out", "Output", "dB", -18.0f, 18.0f, 0.0f, "Overall level after EQ."),
    };
    for (int i : {eB1Q, eB2Q, eDynAtt, eDynRel}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Eq>(); };
    return d;
}

FxDescriptor describeSaturation() {
    FxDescriptor d;
    d.typeId = "saturation";
    d.name = "Saturation";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Adds harmonics, from subtle warmth to heavy distortion. Oversampled to keep it clean of digital fizz.";
    d.params = {
        P::choice("style", "Style", {"Tube", "Tape", "Transistor", "Clip", "Fold", "Smooth"}, 0, "Tube: warm, even harmonics. Tape: soft and round. Transistor: firmer. Clip: hard edge. Fold: metallic, synth-like. Smooth: gentle."),
        P::flt("drive", "Drive", "dB", 0.0f, 36.0f, 9.0f, "How hard the sound hits the saturation. More = more harmonics and grit."),
        P::flt("bias", "Bias", "%", -100.0f, 100.0f, 0.0f, "Makes the distortion lopsided for extra warmth and fuzz-like texture."),
        P::flt("tone", "Tone", "dB", -12.0f, 12.0f, 0.0f, "Feeds more treble (+) or bass (-) into the distortion, then partly corrects it afterwards."),
        P::logf("low_cut", "Low Cut", "Hz", 10.0f, 500.0f, 10.0f, "Stops low bass from making the distortion muddy."),
        P::toggle("auto_gain", "Auto Gain", true, "Keeps the output as loud as the input so you hear character, not just level."),
        P::flt("out", "Output", "dB", -24.0f, 12.0f, 0.0f, "Final level."),
        P::pct("mix", "Mix", 100.0f, "Blend clean and saturated (parallel saturation)."),
    };
    for (int i : {sBias, sLowCut}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Saturation>(); };
    return d;
}

FxDescriptor describeWidth() {
    FxDescriptor d;
    d.typeId = "width";
    d.name = "Stereo Width";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Narrows or widens the stereo image. Spread adds width even to mono sources without hurting mono playback.";
    d.params = {
        P::flt("width", "Width", "%", 0.0f, 200.0f, 100.0f, "0% = mono, 100% = unchanged, 200% = extra wide."),
        P::logf("mono_below", "Mono Bass", "Hz", 20.0f, 500.0f, 20.0f, "Keeps everything below this frequency in mono for a solid low end (20 Hz = off)."),
        P::pct("spread", "Spread", 0.0f, "Creates width from a mono sound. The mono sum is left untouched."),
        P::flt("balance", "Balance", "%", -100.0f, 100.0f, 0.0f, "Shifts the image left or right."),
        P::flt("out", "Output", "dB", -18.0f, 12.0f, 0.0f, "Final level."),
    };
    d.create = [] { return std::make_unique<Width>(); };
    return d;
}

}  // namespace ih1::fx
