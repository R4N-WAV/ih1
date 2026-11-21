// Dynamics family: Compressor/Limiter, Gate/Expander/Pump, De-Esser, Transient Shaper.
// KADAL: KNW-X0BEXVDR9Z (feed-forward core: soft-knee gain computer, dB-domain ballistics),
// KNW-HJ3JZGNYZY (gates: hysteresis, hold, side-chain filters), KNW-NFJ86FFD2E (level-independent
// transient shaping), IMP-C783HAF7B6 lesson 13 (level-relative thresholds for de-essers).
// FET/Opto modes are behavioural designs (fast program-dependent release / two-stage optical release),
// not models of specific hardware.
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

inline float toDb(float x) { return 20.0f * std::log10(std::max(x, 1e-9f)); }
inline float coef(float ms, double sr) { return 1.0f - std::exp(-1.0f / (std::max(ms, 0.01f) * 0.001f * static_cast<float>(sr))); }

// soft-knee static curve: returns gain reduction (<= 0 dB) for input level x (dB)
inline float gainComputer(float x, float thr, float ratio, float knee) {
    const float over = x - thr;
    if (2.0f * over < -knee) return 0.0f;
    if (knee > 0.0f && 2.0f * std::fabs(over) <= knee) {
        const float t = over + knee * 0.5f;
        return (1.0f / ratio - 1.0f) * t * t / (2.0f * knee);
    }
    return (1.0f / ratio - 1.0f) * over;
}

// =============================================================================================== Compressor
enum { cpMode, cpThr, cpRatio, cpKnee, cpAttack, cpRelease, cpMakeup, cpAuto, cpScHp, cpLink, cpMix };
class Compressor final : public FxBase {
    void onPrepare() override { mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear); makeup_.prepare(sr_, 30.0f); }
    void reset() override { for (auto& s : sc_) s.reset(); for (int c = 0; c < 2; ++c) { grFast_[c] = grSlow_[c] = 0.0f; rms_[c] = 0.0f; } mix_.reset(); }
    void applyParams(bool snap) override {
        for (auto& s : sc_) s.setup(dsp::Svf::Highpass, fp(cpScHp), 0.6f, 0, sr_);
        float mk = fp(cpMakeup);
        if (bp(cpAuto)) mk += -0.5f * gainComputer(0.0f, fp(cpThr), fp(cpRatio), fp(cpKnee));  // half the 0 dBFS reduction
        makeup_.setTarget(dsp::dbToGain(mk));
        if (snap) makeup_.snap(makeup_.target());
        mix_.setMix(pct(cpMix), snap);
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const int mode = ip(cpMode);  // 0 clean, 1 FET, 2 opto, 3 limit
        float thr = fp(cpThr), ratio = fp(cpRatio), knee = fp(cpKnee), att = fp(cpAttack), rel = fp(cpRelease);
        if (mode == 1) { att = std::min(att, 0.8f); }
        if (mode == 2) { att = std::max(att, 10.0f); ratio = std::min(ratio, 4.0f); knee = std::max(knee, 12.0f); }
        if (mode == 3) { ratio = 100.0f; att = 0.05f; knee = 1.0f; }
        const float aA = coef(att, sr_), aR = coef(rel, sr_), aSlow = coef(rel * 8.0f, sr_), aRms = coef(5.0f, sr_);
        const bool link = bp(cpLink);
        for (int i = 0; i < n; ++i) {
            float det[2];
            for (int c = 0; c < 2; ++c) {
                const float s = sc_[c].process(ch[c][i]);
                rms_[c] += aRms * (s * s - rms_[c]);
                det[c] = mode == 2 ? toDb(std::sqrt(rms_[c]) * 1.414f) : toDb(std::fabs(s));
            }
            if (link) det[0] = det[1] = std::max(det[0], det[1]);
            const float g = makeup_.next();
            for (int c = 0; c < 2; ++c) {
                const float target = gainComputer(det[c], thr, ratio, knee);  // <= 0
                float& gf = grFast_[c];
                float& gs = grSlow_[c];
                if (target < gf) gf += aA * (target - gf);
                else {
                    float r = aR;
                    if (mode == 1) r *= 1.0f + 3.0f * std::exp(gs / 6.0f) * (gs > -1.0f ? 1.0f : 0.3f);  // FET: quicker after short peaks
                    gf += r * (target - gf);
                }
                gs += aSlow * (gf - gs);  // slow memory of recent gain reduction (program dependence)
                float gr = gf;
                if (mode == 2) gr = 0.5f * gf + 0.5f * std::min(gf, gs);  // opto: fast stage + slow stage
                float y = ch[c][i] * dsp::dbToGain(gr) * g;
                if (mode == 3) y = std::tanh(y * 1.05f) / 1.05f;  // limit mode: soft safety on residual peaks
                ch[c][i] = y;
            }
        }
        for (auto& s : sc_) s.flush();
        mix_.apply(ch, n);
    }
    dsp::Svf sc_[2];
    float grFast_[2] = {0, 0}, grSlow_[2] = {0, 0}, rms_[2] = {0, 0};
    dsp::Smoother makeup_;
    MixStage mix_;
};

// =============================================================================================== Gate / Expander / Pump
enum { gtMode, gtThr, gtRange, gtRatio, gtAttack, gtHold, gtRelease, gtHyst, gtKeyHp, gtKeyLp, gtDiv, gtCurve };
class Gate final : public FxBase {
    void onPrepare() override {}
    void reset() override { env_ = 0.0f; gain_ = gain2_ = 1.0f; open_ = false; hold_ = 0; keyHp_.reset(); keyLp_.reset(); phase_ = 0.0; pz_[0] = pz_[1] = 1.0f; }
    void applyParams(bool) override {
        keyHp_.setup(dsp::Svf::Highpass, fp(gtKeyHp), 0.6f, 0, sr_);
        keyLp_.setup(dsp::Svf::Lowpass, fp(gtKeyLp), 0.6f, 0, sr_);
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        const int mode = ip(gtMode);  // 0 gate, 1 expander, 2 pump (tempo ducking)
        const float floorG = dsp::dbToGain(-std::fabs(fp(gtRange)));
        if (mode == 2) {
            const double beats = dsp::kDivisions[std::clamp(ip(gtDiv), 0, dsp::kNumDivisions - 1)].beats;
            const double inc = 1.0 / (dsp::divisionSeconds(ip(gtDiv), ctx.bpm) * sr_);
            if (ctx.ppqValid && ctx.hostPlaying) phase_ = std::fmod(ctx.ppqPosition / beats, 1.0);
            const float curve = 0.5f + 4.0f * pct(gtCurve);
            const float edge = 1.0f - std::exp(-1.0f / (0.0012f * srf()));  // V4: ~2.5 ms 2-pole drop, no step at the beat
            for (int i = 0; i < n; ++i) {
                // duck at each beat, recover along a power curve (sidechain-style pump without a sidechain)
                const float t = static_cast<float>(phase_);
                const float tgt = floorG + (1.0f - floorG) * std::pow(std::min(1.0f, t * 1.15f), 1.0f / curve);
                pz_[0] += edge * (tgt - pz_[0]);
                pz_[1] += edge * (pz_[0] - pz_[1]);
                const float g = pz_[1];  // the curve, through a ~2.5 ms 2-pole smoother: the drop is a ramp
                ch[0][i] *= g;
                ch[1][i] *= g;
                phase_ += inc;
                if (phase_ >= 1.0) phase_ -= 1.0;
            }
            return;
        }
        const float thr = fp(gtThr), hyst = fp(gtHyst), ratio = fp(gtRatio);
        // V4 Phase 4: a 0.4 ms floor on attack and a second smoothing pole: the gate still snaps open on a hit, but
        // opening or closing on a sustained sound is a short ramp, not a step
        const float aA = coef(std::max(0.4f, fp(gtAttack)), sr_), aR = coef(fp(gtRelease), sr_), aEnv = coef(2.0f, sr_);
        const float a2 = coef(0.25f, sr_);
        const int holdS = static_cast<int>(fp(gtHold) * 0.001f * srf());
        for (int i = 0; i < n; ++i) {
            const float k = keyLp_.process(keyHp_.process(0.5f * (ch[0][i] + ch[1][i])));
            const float a = std::fabs(k);
            env_ = a > env_ ? a : env_ + aEnv * (a - env_);
            const float lvl = toDb(env_);
            float target;
            if (mode == 0) {
                if (!open_ && lvl > thr) open_ = true;
                if (open_ && lvl < thr - hyst) { if (hold_ <= 0) open_ = false; }
                if (lvl > thr - hyst) hold_ = holdS; else if (hold_ > 0) --hold_;
                target = open_ ? 1.0f : floorG;
            } else {
                const float below = std::min(0.0f, lvl - thr);
                target = std::max(floorG, dsp::dbToGain(below * (ratio - 1.0f)));
            }
            gain_ += (target > gain_ ? aA : aR) * (target - gain_);
            gain2_ += a2 * (gain_ - gain2_);
            ch[0][i] *= gain2_;
            ch[1][i] *= gain2_;
        }
        keyHp_.flush();
        keyLp_.flush();
    }
    dsp::Svf keyHp_, keyLp_;
    float env_ = 0.0f, gain_ = 1.0f, gain2_ = 1.0f, pz_[2] = {1.0f, 1.0f};
    bool open_ = false;
    int hold_ = 0;
    double phase_ = 0.0;
};

// =============================================================================================== De-Esser
enum { deFreq, deThr, deRange, deMode, deRelative, deListen };
class DeEsser final : public FxBase {
    void onPrepare() override {}
    void reset() override { band_.reset(); for (int c = 0; c < 2; ++c) { split_[c].reset(); bell_[c].reset(); } envB_ = envF_ = 0.0f; gr_ = 0.0f; ctr_ = 0; }
    void applyParams(bool) override {
        band_.setup(dsp::Svf::Bandpass, fp(deFreq), 1.4f, 0, sr_);
        for (auto& s : split_) s.setup(dsp::Svf::Bandpass, fp(deFreq), 1.4f, 0, sr_);
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        const float aA = coef(0.5f, sr_), aR = coef(60.0f, sr_), aF = coef(20.0f, sr_);
        const float range = std::fabs(fp(deRange)), thr = fp(deThr);
        const bool split = ip(deMode) == 0, rel = bp(deRelative), listen = bp(deListen);
        for (int i = 0; i < n; ++i) {
            const float m = 0.5f * (ch[0][i] + ch[1][i]);
            const float b = std::fabs(band_.process(m));
            envB_ += (b > envB_ ? aA : aR) * (b - envB_);
            envF_ += aF * (std::fabs(m) - envF_);
            // relative mode: threshold follows the programme level, so quiet and loud passages are treated alike
            const float lvl = toDb(envB_) - (rel ? toDb(envF_) : 0.0f);
            const float over = std::max(0.0f, lvl - thr);
            const float target = -std::min(range, over * 0.75f);
            gr_ += (target < gr_ ? 0.5f : aR) * (target - gr_);
            const float g = dsp::dbToGain(gr_);
            for (int c = 0; c < 2; ++c) {
                const float s = split_[c].process(ch[c][i]);  // the sibilant band
                if (listen) ch[c][i] = s;
                else if (split) ch[c][i] = ch[c][i] - s + s * g;  // reduce only the band
                else ch[c][i] *= g;                               // wideband
            }
        }
        band_.flush();
        for (auto& s : split_) s.flush();
    }
    dsp::Svf band_, split_[2], bell_[2];
    float envB_ = 0.0f, envF_ = 0.0f, gr_ = 0.0f;
    int ctr_ = 0;
};

// =============================================================================================== Transient Shaper
enum { tsAttack, tsSustain, tsSpeed, tsOut, tsMix };
class TransientShaper final : public FxBase {
    void onPrepare() override { mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear); out_.prepare(sr_, 30.0f); }
    // V3 safety fix: the detectors used to start (and fall back after digital silence) at -120 dB, so the first
    // signal read as a ~100 dB "attack" and was boosted by the full +24 dB clamp for tens of ms (+14 dB measured
    // when landing on the default Transient Shaper). The detectors now start at the first sample's level and never
    // fall below kFloorDb, and the boost is limited to +-18 dB.
    static constexpr float kFloorDb = -60.0f;
    // A 20 ms warm-up after reset follows the level without detecting anything: a processor that has just been
    // staged receives the engine's 12 ms de-click fade-in, which must not read as a giant attack.
    void reset() override { fa_ = sa_ = fr_ = sr2_ = kFloorDb; g_ = 0.0f; warm_ = false; warmLeft_ = static_cast<int>(0.02 * sr_); mix_.reset(); }
    void applyParams(bool snap) override { mix_.setMix(pct(tsMix), snap); out_.setTarget(dsp::dbToGain(fp(tsOut))); if (snap) out_.snap(out_.target()); }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float sp = 0.5f + pct(tsSpeed) * 1.5f;  // speed scales all time constants
        const float aFast = coef(1.0f / sp, sr_), aSlowA = coef(30.0f / sp, sr_);
        const float rFast = coef(40.0f / sp, sr_), rSlow = coef(400.0f / sp, sr_);
        const float P = fp(tsAttack) * 0.01f, S = fp(tsSustain) * 0.01f;
        const float gs = coef(1.0f, sr_);
        for (int i = 0; i < n; ++i) {
            const float x = std::max(kFloorDb, toDb(std::max(std::fabs(ch[0][i]), std::fabs(ch[1][i]))));
            if (!warm_ && x > kFloorDb) warm_ = true;
            if (warm_ && warmLeft_ > 0) { --warmLeft_; fa_ = sa_ = fr_ = sr2_ = std::max(fa_ - 0.002f, x); }
            // attack detector: fast vs slow rise; sustain detector: slow vs fast fall (all in dB: level-independent)
            fa_ += (x > fa_ ? aFast : rFast) * (x - fa_);
            sa_ += (x > sa_ ? aSlowA : rFast) * (x - sa_);
            fr_ += (x > fr_ ? aFast : rFast) * (x - fr_);
            sr2_ += (x > sr2_ ? aFast : rSlow) * (x - sr2_);
            const float att = std::max(0.0f, fa_ - sa_);
            const float sus = std::max(0.0f, sr2_ - fr_);
            const float tgt = std::clamp(P * att + S * sus, -18.0f, 18.0f);
            g_ += gs * (tgt - g_);
            const float g = dsp::dbToGain(g_) * out_.next();
            ch[0][i] *= g;
            ch[1][i] *= g;
        }
        mix_.apply(ch, n);
    }
    float fa_ = kFloorDb, sa_ = kFloorDb, fr_ = kFloorDb, sr2_ = kFloorDb, g_ = 0;
    bool warm_ = false;
    int warmLeft_ = 0;
    dsp::Smoother out_;
    MixStage mix_;
};

}  // namespace

FxDescriptor describeCompressor() {
    FxDescriptor d;
    d.typeId = "compressor";
    d.name = "Compressor";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Evens out loud and quiet moments: from transparent levelling to punchy, pumping squash.";
    d.params = {
        P::choice("mode", "Character", {"Clean", "FET", "Opto", "Limit"}, 0, "Clean: transparent. FET: very fast and aggressive. Opto: slow, smooth, program-dependent. Limit: catches peaks hard."),
        P::flt("threshold", "Threshold", "dB", -60.0f, 0.0f, -20.0f, "Level above which compression starts. Lower = more compression."),
        P::flt("ratio", "Ratio", ":1", 1.0f, 20.0f, 4.0f, "How strongly levels above the threshold are reduced.", ParamCurve::Log),
        P::flt("knee", "Knee", "dB", 0.0f, 24.0f, 6.0f, "Soft knee eases into compression gradually; hard knee clamps sharply."),
        P::logf("attack", "Attack", "ms", 0.05f, 200.0f, 10.0f, "How quickly it clamps down. Slower lets the hit's front edge through (more punch)."),
        P::logf("release", "Release", "ms", 10.0f, 3000.0f, 150.0f, "How quickly it lets go. Fast releases can pump."),
        P::flt("makeup", "Makeup", "dB", -12.0f, 24.0f, 0.0f, "Brings the level back up after compression."),
        P::toggle("auto_makeup", "Auto Makeup", true, "Estimates makeup gain from threshold and ratio."),
        P::logf("sc_hp", "Detector HP", "Hz", 20.0f, 500.0f, 20.0f, "Stops low bass from triggering compression (less pumping on kicks and bass)."),
        P::toggle("link", "Stereo Link", true, "Both sides compress together so the stereo image doesn't wander."),
        P::pct("mix", "Mix", 100.0f, "Parallel compression: blend the squashed sound with the original."),
    };
    for (int i : {cpKnee, cpScHp, cpLink}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Compressor>(); };
    return d;
}

FxDescriptor describeGate() {
    FxDescriptor d;
    d.typeId = "gate";
    d.name = "Gate / Expander";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Silences or softens quiet parts between notes, or pumps the sound rhythmically on the beat.";
    d.params = {
        P::choice("mode", "Mode", {"Gate", "Expander", "Pump"}, 0, "Gate: shuts below the threshold. Expander: gently turns quiet parts down. Pump: ducks on every beat."),
        P::flt("threshold", "Threshold", "dB", -80.0f, 0.0f, -45.0f, "Level below which the sound is reduced."),
        P::flt("range", "Range", "dB", 0.0f, 80.0f, 40.0f, "How much quiet parts (or the pump dip) are turned down."),
        P::flt("ratio", "Ratio", ":1", 1.0f, 8.0f, 2.0f, "Expander strength."),
        P::logf("attack", "Attack", "ms", 0.05f, 50.0f, 0.5f, "How fast it opens."),
        P::flt("hold", "Hold", "ms", 0.0f, 500.0f, 30.0f, "Keeps the gate open briefly so short gaps don't chatter."),
        P::logf("release", "Release", "ms", 5.0f, 2000.0f, 120.0f, "How fast it closes."),
        P::flt("hysteresis", "Hysteresis", "dB", 0.0f, 12.0f, 4.0f, "Closing threshold sits this far below the opening one, preventing flutter."),
        P::logf("key_hp", "Key HP", "Hz", 20.0f, 2000.0f, 20.0f, "Ignore low frequencies when deciding to open."),
        P::logf("key_lp", "Key LP", "Hz", 500.0f, 20000.0f, 20000.0f, "Ignore high frequencies when deciding to open."),
        P::choice("division", "Pump Rate", divisionLabels(), 10, "Pump: how often it ducks."),
        P::pct("curve", "Pump Curve", 40.0f, "Pump: how quickly the level recovers after each dip."),
    };
    for (int i : {gtHyst, gtKeyHp, gtKeyLp}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Gate>(); };
    return d;
}

FxDescriptor describeDeEsser() {
    FxDescriptor d;
    d.typeId = "deesser";
    d.name = "De-Esser";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Tames harsh 's' and 't' sounds without dulling the rest of the voice.";
    d.params = {
        P::logf("freq", "Frequency", "Hz", 2000.0f, 14000.0f, 6500.0f, "Where the sibilance lives. Use Listen to find it."),
        P::flt("threshold", "Threshold", "dB", -40.0f, 10.0f, -6.0f, "How sensitive it is. Lower = more de-essing."),
        P::flt("range", "Range", "dB", 0.0f, 24.0f, 10.0f, "Maximum reduction."),
        P::choice("mode", "Mode", {"Split-band", "Wideband"}, 0, "Split-band turns down only the hissy band; wideband turns the whole sound down briefly."),
        P::toggle("relative", "Relative", true, "Threshold follows the singer's level, so loud and soft passages are treated alike."),
        P::toggle("listen", "Listen", false, "Hear only the sibilant band to tune the frequency."),
    };
    d.create = [] { return std::make_unique<DeEsser>(); };
    return d;
}

FxDescriptor describeTransient() {
    FxDescriptor d;
    d.typeId = "transient";
    d.name = "Transient Shaper";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Makes hits snappier or softer and tails longer or shorter, at any volume.";
    d.params = {
        P::flt("attack", "Attack", "%", -100.0f, 100.0f, 30.0f, "Positive = punchier, sharper hits. Negative = softer, rounder hits."),
        P::flt("sustain", "Sustain", "%", -100.0f, 100.0f, 0.0f, "Positive = longer ring and room. Negative = tighter, drier."),
        P::pct("speed", "Speed", 50.0f, "How short the shaped attack and sustain regions are."),
        P::flt("out", "Output", "dB", -18.0f, 12.0f, 0.0f, "Final level."),
        P::pct("mix", "Mix", 100.0f, "Blend with the original."),
    };
    d.create = [] { return std::make_unique<TransientShaper>(); };
    return d;
}

}  // namespace ih1::fx
