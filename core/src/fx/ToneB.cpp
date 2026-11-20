// Tone family, Batch B/C: Bitcrusher, Tape / Console Colour, Exciter.
// KADAL: KNW-M8Q0G3M7NM (sampling / resampling), KNW-DKQ16F12WF + KNW-CZWZ65H13F (tape chain and level
// calibration), KNW-MQRRKX0DSE (transformer saturation), KNW-F7DN55HDGE / KNW-MCXD9ZQP83 (ADAA + 2x).
#include "ih1/dsp/Nonlinear.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

// =============================================================================================== Bitcrusher
enum { bcBits, bcRate, bcJitter, bcDither, bcPreFilter, bcDrive, bcMix };
class Bitcrusher final : public FxBase {
    void onPrepare() override { mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear); }
    void reset() override { for (int c = 0; c < 2; ++c) { held_[c] = 0.0f; pre_[c].reset(); pre2_[c].reset(); } phase_ = 0.0; rng_.seed_(99); jit_ = 0.0f; mix_.reset(); }
    void applyParams(bool snap) override {
        const float fc = std::min(fp(bcRate) * 0.45f, 0.45f * srf());
        for (int c = 0; c < 2; ++c) { pre_[c].setup(dsp::Svf::Lowpass, fc, 0.54f, 0, sr_); pre2_[c].setup(dsp::Svf::Lowpass, fc, 1.31f, 0, sr_); }
        mix_.setMix(pct(bcMix), snap);
    }
    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float bits = fp(bcBits);
        const float levels = std::exp2(bits - 1.0f);
        const bool dither = bp(bcDither), pre = bp(bcPreFilter);
        const double step = std::min(1.0, static_cast<double>(fp(bcRate)) / sr_);  // sample-and-hold clock
        const float jitter = pct(bcJitter);
        const float drive = dsp::dbToGain(fp(bcDrive));
        for (int i = 0; i < n; ++i) {
            phase_ += step * (1.0 + jitter * 0.5 * jit_);
            const bool tick = phase_ >= 1.0;
            if (tick) { phase_ -= std::floor(phase_); jit_ = rng_.bipolar(); }
            for (int c = 0; c < 2; ++c) {
                float x = ch[c][i] * drive;
                if (pre) x = pre2_[c].process(pre_[c].process(x));
                if (tick || step >= 1.0) {
                    float q = x;
                    if (bits < 23.5f) {
                        if (dither) q += (rng_.uniform() - rng_.uniform()) / levels;  // TPDF
                        q = std::round(std::clamp(q, -1.0f, 1.0f) * levels) / levels;
                    }
                    held_[c] = q;
                }
                ch[c][i] = held_[c];
            }
        }
        for (int c = 0; c < 2; ++c) { pre_[c].flush(); pre2_[c].flush(); }
        mix_.apply(ch, n);
    }
    float held_[2] = {0, 0}, jit_ = 0.0f;
    double phase_ = 0.0;
    dsp::Svf pre_[2], pre2_[2];
    dsp::Rng rng_{99};
    MixStage mix_;
};

// =============================================================================================== Tape / Console Colour
enum { tcMode, tcDrive, tcBump, tcHfLoss, tcAuto, tcOut, tcMix };
class TapeColour final : public FxBase {
    void onPrepare() override {
        for (auto& o : os_) o.prepareVariant(maxBlock_, latMode_ == LatencyMode::Quality ? dsp::Oversampler2x::Variant::Fir : (latMode_ == LatencyMode::Low ? dsp::Oversampler2x::Variant::MinPhaseIir : dsp::Oversampler2x::Variant::Bypass));
        mix_.prepare(sr_, maxBlock_, os_[0].latency(), MixLaw::Linear);
        work_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        inPow_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        drive_.prepare(sr_, 30.0f);
        out_.prepare(sr_, 30.0f);
        for (auto& d : dc_) d.prepare(sr_ * os_[0].factor(), 6.0f);
    }
    int latencySamples() const override { return os_[0].latency(); }
    void reset() override {
        for (auto& o : os_) o.reset();
        for (int c = 0; c < 2; ++c) { sh_[c].reset(); bump_[c].reset(); hf_[c].reset(); lift_[c].reset(); dc_[c].reset(); }
        rmsIn_ = rmsOut_ = 1e-6f;
        autoG_ = 1.0f;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        const bool console = ip(tcMode) == 1;
        for (int c = 0; c < 2; ++c) {
            sh_[c].setShape(console ? dsp::Shape::Asym : dsp::Shape::Tanh);
            bump_[c].setup(dsp::Svf::Bell, console ? 40.0f : 85.0f, 0.9f, fp(tcBump), sr_);
            hf_[c].setup(dsp::Svf::HighShelf, console ? 16000.0f : 9000.0f, 0.6f, -fp(tcHfLoss), sr_);
            lift_[c].setup(dsp::Svf::LowShelf, 120.0f, 0.6f, console ? 1.0f : 0.0f, sr_);
        }
        // calibrated level: 0 dBFS ~ +18 dBu nominal, so 'Drive' is how far above nominal we push the stage
        drive_.setTarget(dsp::dbToGain(fp(tcDrive) - 6.0f));
        out_.setTarget(dsp::dbToGain(fp(tcOut)));
        if (snap) { drive_.snap(drive_.target()); out_.snap(out_.target()); }
        mix_.setMix(pct(tcMix), snap);
    }
    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float d0 = drive_.current();
        float d1 = d0;
        for (int i = 0; i < n; ++i) d1 = drive_.next();
        const float aRms = 1.0f - std::exp(-1.0f / (0.3f * srf()));
        for (int i = 0; i < n; ++i) inPow_[static_cast<size_t>(i)] = 0.5f * (ch[0][i] * ch[0][i] + ch[1][i] * ch[1][i]);
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < n; ++i) work_[static_cast<size_t>(i)] = lift_[c].process(bump_[c].process(ch[c][i]));
            float* hi = os_[c].up(work_.data(), n);
            for (int k = 0; k < os_[c].factor() * n; ++k) {
                const float drv = d0 + (d1 - d0) * static_cast<float>(k) / static_cast<float>(os_[c].factor() * n);
                hi[k] = dc_[c].process(sh_[c].process(hi[k] * drv) / std::max(0.25f, drv));
            }
            os_[c].down(hi, ch[c], n);
            for (int i = 0; i < n; ++i) ch[c][i] = hf_[c].process(ch[c][i]);
        }
        const bool autoGain = bp(tcAuto);
        for (int i = 0; i < n; ++i) {
            rmsIn_ += aRms * (inPow_[static_cast<size_t>(i)] - rmsIn_);
            rmsOut_ += aRms * (0.5f * (ch[0][i] * ch[0][i] + ch[1][i] * ch[1][i]) - rmsOut_);
            const float tgt = autoGain ? std::clamp(std::sqrt(rmsIn_ / std::max(rmsOut_, 1e-10f)), 0.25f, 4.0f) : 1.0f;
            autoG_ += 0.0005f * (tgt - autoG_);
            const float g = autoG_ * out_.next();
            ch[0][i] *= g;
            ch[1][i] *= g;
        }
        for (int c = 0; c < 2; ++c) { bump_[c].flush(); hf_[c].flush(); lift_[c].flush(); }
        mix_.apply(ch, n);
    }
    dsp::Oversampler2x os_[2];
    dsp::AdaaShaper sh_[2];
    dsp::Svf bump_[2], hf_[2], lift_[2];
    dsp::DcBlocker dc_[2];
    dsp::Smoother drive_, out_;
    std::vector<float> work_, inPow_;
    float rmsIn_ = 1e-6f, rmsOut_ = 1e-6f, autoG_ = 1.0f;
    MixStage mix_;
};

// =============================================================================================== Exciter
enum { exFreq, exDrive, exAmount, exMode };
class Exciter final : public FxBase {
    void onPrepare() override { for (auto& o : os_) o.prepareVariant(maxBlock_, latMode_ == LatencyMode::Quality ? dsp::Oversampler2x::Variant::Fir : (latMode_ == LatencyMode::Low ? dsp::Oversampler2x::Variant::MinPhaseIir : dsp::Oversampler2x::Variant::Bypass)); work_.assign(static_cast<size_t>(maxBlock_), 0.0f); band_.assign(static_cast<size_t>(maxBlock_) * 2, 0.0f); amt_.prepare(sr_, 30.0f); }
    int latencySamples() const override { return os_[0].latency(); }
    void reset() override {
        for (auto& o : os_) o.reset();
        for (int c = 0; c < 2; ++c) { hp_[c].reset(); hp2_[c].reset(); post_[c].reset(); sh_[c].reset(); dly_[c].assign(64, 0.0f); }
        pos_ = 0;
    }
    void applyParams(bool snap) override {
        for (int c = 0; c < 2; ++c) {
            hp_[c].setup(dsp::Svf::Highpass, fp(exFreq), 0.7f, 0, sr_);
            hp2_[c].setup(dsp::Svf::Highpass, fp(exFreq), 0.7f, 0, sr_);
            post_[c].setup(dsp::Svf::Highpass, fp(exFreq) * 1.5f, 0.7f, 0, sr_);
            sh_[c].setShape(ip(exMode) == 0 ? dsp::Shape::Asym : dsp::Shape::Tanh);
        }
        amt_.setTarget(pct(exAmount));
        if (snap) amt_.snap(amt_.target());
        if (dly_[0].empty()) for (auto& d : dly_) d.assign(64, 0.0f);
    }
    void process(float* const* ch, int n, const ProcessContext&) override {
        // dry path delayed by the oversampler latency; add only the *generated* upper harmonics
        const int lat = os_[0].latency();
        const float drive = dsp::dbToGain(fp(exDrive));
        float* band = band_.data();
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < n; ++i) work_[static_cast<size_t>(i)] = hp2_[c].process(hp_[c].process(ch[c][i])) * drive;
            float* hi = os_[c].up(work_.data(), n);
            for (int k = 0; k < os_[c].factor() * n; ++k) hi[k] = sh_[c].process(hi[k]) - hi[k] * 0.8f;  // keep the harmonics, drop most of the fundamental
            os_[c].down(hi, band + c * maxBlock_, n);
        }
        for (int i = 0; i < n; ++i) {
            const float a = amt_.next() / std::max(1.0f, drive);
            for (int c = 0; c < 2; ++c) {
                auto& d = dly_[c];
                d[static_cast<size_t>((pos_ + i) & 63)] = ch[c][i];
                const float dry = d[static_cast<size_t>((pos_ + i - lat) & 63)];
                ch[c][i] = dry + a * 2.0f * post_[c].process(band[c * maxBlock_ + i]);
            }
        }
        pos_ = (pos_ + n) & 63;
        for (int c = 0; c < 2; ++c) { hp_[c].flush(); hp2_[c].flush(); post_[c].flush(); }
    }
    dsp::Oversampler2x os_[2];
    dsp::AdaaShaper sh_[2];
    dsp::Svf hp_[2], hp2_[2], post_[2];
    std::vector<float> work_, band_, dly_[2];
    int pos_ = 0;
    dsp::Smoother amt_;
};

}  // namespace

FxDescriptor describeBitcrusher() {
    FxDescriptor d;
    d.typeId = "bitcrush";
    d.name = "Bitcrusher";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Lo-fi digital grit: fewer bits make it noisy and crunchy, a lower sample rate makes it aliased and metallic.";
    d.params = {
        P::flt("bits", "Bits", "", 1.0f, 24.0f, 8.0f, "Bit depth. Fewer bits = rougher, noisier, crunchier."),
        P::logf("rate", "Sample Rate", "Hz", 200.0f, 48000.0f, 11025.0f, "Resampling rate. Lower = more aliasing and old-sampler grit."),
        P::pct("jitter", "Jitter", 0.0f, "Unsteady sample clock: adds a wobbly, broken digital edge."),
        P::toggle("dither", "Dither", false, "Smooths quantisation into soft noise instead of harsh distortion."),
        P::toggle("prefilter", "Anti-alias", false, "Filters before downsampling for a cleaner, darker lo-fi (less metallic aliasing)."),
        P::flt("drive", "Drive", "dB", 0.0f, 24.0f, 0.0f, "Pushes the signal into the crusher's clipping."),
        P::pct("mix", "Mix", 100.0f, "Blend of clean and crushed sound."),
    };
    d.create = [] { return std::make_unique<Bitcrusher>(); };
    return d;
}

FxDescriptor describeTapeColour() {
    FxDescriptor d;
    d.typeId = "tape_colour";
    d.name = "Tape / Console";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Gentle analogue colour: tape's warm low-end bump and soft top, or a console's subtle weight and glue.";
    d.params = {
        P::choice("mode", "Machine", {"Tape", "Console"}, 0, "Tape: soft saturation, head bump, gentle treble loss. Console: transformer-style warmth and low lift."),
        P::flt("drive", "Drive", "dB", 0.0f, 24.0f, 6.0f, "How hard the stage is driven. Small amounts add glue; more adds audible warmth."),
        P::flt("bump", "Low Bump", "dB", 0.0f, 6.0f, 1.5f, "The low-frequency bump typical of tape playback heads."),
        P::flt("hf_loss", "Top Loss", "dB", 0.0f, 12.0f, 2.0f, "Softens the extreme top end."),
        P::toggle("auto_gain", "Auto Gain", true, "Keeps the level steady so you only hear the colour."),
        P::flt("out", "Output", "dB", -18.0f, 12.0f, 0.0f, "Final level."),
        P::pct("mix", "Mix", 100.0f, "Blend of clean and coloured sound."),
    };
    d.create = [] { return std::make_unique<TapeColour>(); };
    return d;
}

FxDescriptor describeExciter() {
    FxDescriptor d;
    d.typeId = "exciter";
    d.name = "Exciter";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Adds airy, shimmering upper harmonics for presence without simply turning up the treble.";
    d.params = {
        P::logf("freq", "Frequency", "Hz", 1000.0f, 12000.0f, 3500.0f, "Only content above this generates new harmonics."),
        P::flt("drive", "Drive", "dB", 0.0f, 24.0f, 9.0f, "How rich the generated harmonics are."),
        P::pct("amount", "Amount", 30.0f, "How much of the new sparkle is added."),
        P::choice("mode", "Colour", {"Warm (even)", "Bright (odd)"}, 0, "Warm adds smoother even harmonics; bright adds edgier odd ones."),
    };
    d.create = [] { return std::make_unique<Exciter>(); };
    return d;
}

}  // namespace ih1::fx
