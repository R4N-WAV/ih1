// Modulation family, Batch B: Flanger, Phaser, Vibrato, Tremolo/Auto-Pan, Ring Modulator, Rotary Speaker.
// Shared primitives (fractional delay, LFO, SVF) but distinct topologies (brief §4.1 distinctions):
//   Flanger = very short swept delay + feedback mixed with dry (comb notches)
//   Phaser  = swept all-pass cascade mixed with dry (non-harmonic notches)
//   Vibrato = 100 % wet swept delay (pitch only, no dry anchor)
//   Tremolo = amplitude / constant-power pan modulation, tempo-locked to the host grid
//   Ring    = multiplication by a carrier (sum/difference sidebands)
//   Rotary  = crossover into horn + drum rotors, each with Doppler delay, AM and inertia, two mics
#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

float shapeAt(int shape, float ph) {  // ph in [0,1)
    switch (shape) {
        case 1: return 1.0f - 4.0f * std::fabs(ph - 0.5f);
        case 2: { const float s = std::sin(dsp::kTwoPi * ph); return std::tanh(4.0f * s) / std::tanh(4.0f); }  // soft square
        case 3: return 2.0f * ph - 1.0f;
        default: return std::sin(dsp::kTwoPi * ph);
    }
}

// =============================================================================================== Flanger
enum { flManual, flDepth, flRate, flFeedback, flStereo, flShape, flMix };
class Flanger final : public FxBase {
    void onPrepare() override {
        for (auto& l : line_) l.prepare(static_cast<int>(0.02 * sr_) + 16);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        manS_.prepare(sr_, 60.0f);
        depS_.prepare(sr_, 60.0f);
    }
    void reset() override { for (auto& l : line_) l.reset(); mix_.reset(); phase_ = 0.0; fb_[0] = fb_[1] = 0.0f; }
    void applyParams(bool snap) override {
        manS_.setTarget(fp(flManual) * 0.001f * srf());
        depS_.setTarget(fp(flDepth) * 0.001f * srf());
        if (snap) { manS_.snap(manS_.target()); depS_.snap(depS_.target()); }
        mix_.setMix(pct(flMix), snap);
    }
    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const double inc = fp(flRate) / sr_;
        const float fb = fp(flFeedback) * 0.01f * 0.95f;
        const float off = fp(flStereo) / 360.0f;
        const int shape = ip(flShape);
        for (int i = 0; i < n; ++i) {
            const float man = manS_.next(), dep = depS_.next();
            for (int c = 0; c < 2; ++c) {
                float ph = static_cast<float>(phase_) + (c ? off : 0.0f);
                ph -= std::floor(ph);
                const float d = std::max(1.0f, man + dep * (0.5f + 0.5f * shapeAt(shape, ph)));
                const float x = ch[c][i];
                line_[c].push(x + fb * fb_[c]);
                const float y = line_[c].readCubic(d);
                fb_[c] = 2.0f * std::tanh(0.5f * y);
                ch[c][i] = 0.5f * (x + y) * 1.41f;  // wet = flanged sum (comb), dry handled by mix
            }
            phase_ += inc;
            if (phase_ >= 1.0) phase_ -= 1.0;
        }
        mix_.apply(ch, n);
    }
    dsp::DelayLine line_[2];
    MixStage mix_;
    dsp::Smoother manS_, depS_;
    double phase_ = 0.0;
    float fb_[2] = {0, 0};
};

// =============================================================================================== Phaser
enum { phStages, phCentre, phRange, phRate, phFeedback, phStereo, phMix };
class Phaser final : public FxBase {
    static constexpr int kMax = 12;
    void onPrepare() override { mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear); }
    void reset() override {
        for (auto& c : z_) for (auto& v : c) v = 0.0f;
        last_[0] = last_[1] = 0.0f;
        phase_ = 0.0;
        mix_.reset();
    }
    void applyParams(bool snap) override { mix_.setMix(pct(phMix), snap); }
    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const int stages = 2 * (ip(phStages) + 1);
        const double inc = fp(phRate) / sr_;
        const float fb = fp(phFeedback) * 0.01f * 0.9f;
        const float off = fp(phStereo) / 360.0f;
        const float lc = std::log2(fp(phCentre)), range = fp(phRange);
        for (int i = 0; i < n; ++i) {
            for (int c = 0; c < 2; ++c) {
                float ph = static_cast<float>(phase_) + (c ? off : 0.0f);
                ph -= std::floor(ph);
                const float fc = std::exp2(std::clamp(lc + 0.5f * range * std::sin(dsp::kTwoPi * ph), 4.5f, 14.2f));
                const float t = std::tan(dsp::kPi * fc / srf());
                const float a = (t - 1.0f) / (t + 1.0f);
                float x = ch[c][i] + fb * last_[c];
                for (int s = 0; s < stages; ++s) {  // first-order all-pass: y = a x + z; z = x - a y
                    const float y = a * x + z_[c][s];
                    z_[c][s] = x - a * y;
                    x = y;
                }
                last_[c] = std::tanh(x);
                ch[c][i] = 0.5f * (ch[c][i] + x) * 1.41f;
            }
            phase_ += inc;
            if (phase_ >= 1.0) phase_ -= 1.0;
        }
        for (auto& c : z_) for (auto& v : c) v = dsp::flushTiny(v);
        mix_.apply(ch, n);
    }
    float z_[2][kMax] = {};
    float last_[2] = {0, 0};
    double phase_ = 0.0;
    MixStage mix_;
};

// =============================================================================================== Vibrato
enum { viDepth, viRate, viShape, viStereo, viSync, viDiv, viMix };
class Vibrato final : public FxBase {
    void onPrepare() override {
        for (auto& l : line_) l.prepare(static_cast<int>(0.05 * sr_) + 16);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        depS_.prepare(sr_, 40.0f);
    }
    void reset() override { for (auto& l : line_) l.reset(); mix_.reset(); phase_ = 0.0; }
    void applyParams(bool snap) override { mix_.setMix(pct(viMix), snap); depS_.setTarget(fp(viDepth)); if (snap) depS_.snap(depS_.target()); }
    void process(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const float rate = bp(viSync) ? static_cast<float>(1.0 / dsp::divisionSeconds(ip(viDiv), ctx.bpm)) : fp(viRate);
        const double inc = rate / sr_;
        const float w = dsp::kTwoPi * rate / srf();  // rad/sample
        const float off = fp(viStereo) / 360.0f;
        const int shape = ip(viShape);
        for (int i = 0; i < n; ++i) {
            // pitch deviation of a swept delay = -dD/dt: amplitude A gives about A*w (ratio) -> cents
            const float cents = depS_.next();
            const float A = std::min((std::exp2(cents / 1200.0f) - 1.0f) / std::max(w, 1e-6f), 0.02f * srf());
            for (int c = 0; c < 2; ++c) {
                float ph = static_cast<float>(phase_) + (c ? off : 0.0f);
                ph -= std::floor(ph);
                line_[c].push(ch[c][i]);
                ch[c][i] = line_[c].readCubic(2.0f + A * (1.0f + shapeAt(shape, ph)));
            }
            phase_ += inc;
            if (phase_ >= 1.0) phase_ -= 1.0;
        }
        mix_.apply(ch, n);
    }
    dsp::DelayLine line_[2];
    MixStage mix_;
    dsp::Smoother depS_;
    double phase_ = 0.0;
};

// =============================================================================================== Tremolo / Auto-Pan
enum { trMode, trSync, trRate, trDiv, trDepth, trShape, trStereo };
class Tremolo final : public FxBase {
    void onPrepare() override { depS_.prepare(sr_, 30.0f); }
    void reset() override { phase_ = 0.0; }
    void applyParams(bool snap) override { depS_.setTarget(pct(trDepth)); if (snap) depS_.snap(depS_.target()); }
    void process(float* const* ch, int n, const ProcessContext& ctx) override {
        const bool sync = bp(trSync);
        const double cycleBeats = dsp::kDivisions[std::clamp(ip(trDiv), 0, dsp::kNumDivisions - 1)].beats;
        const double inc = sync ? 1.0 / (dsp::divisionSeconds(ip(trDiv), ctx.bpm) * sr_) : fp(trRate) / sr_;
        // lock to the host grid while playing so the pulse lands on the beat
        if (sync && ctx.ppqValid && ctx.hostPlaying) phase_ = std::fmod(ctx.ppqPosition / cycleBeats, 1.0);
        const int mode = ip(trMode), shape = ip(trShape);
        const float off = fp(trStereo) / 360.0f;
        for (int i = 0; i < n; ++i) {
            const float d = depS_.next();
            const float ph = static_cast<float>(phase_);
            if (mode == 1) {  // auto-pan, constant power
                float gl, gr;
                dsp::panGains(d * shapeAt(shape, ph), gl, gr);
                const float m = 0.5f * (ch[0][i] + ch[1][i]);
                const float s = 0.5f * (ch[0][i] - ch[1][i]);
                ch[0][i] = (m * gl * 1.41421356f) + s * (1.0f - d);
                ch[1][i] = (m * gr * 1.41421356f) - s * (1.0f - d);
            } else {
                for (int c = 0; c < 2; ++c) {
                    float p = ph + (c ? off : 0.0f);
                    p -= std::floor(p);
                    ch[c][i] *= 1.0f - d * (0.5f - 0.5f * shapeAt(shape, p));
                }
            }
            phase_ += inc;
            if (phase_ >= 1.0) phase_ -= 1.0;
        }
    }
    dsp::Smoother depS_;
    double phase_ = 0.0;
};

// =============================================================================================== Ring Modulator
enum { rmFreq, rmWave, rmModDepth, rmModRate, rmStereo, rmMix };
class RingMod final : public FxBase {
    void onPrepare() override { mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear); freqS_.prepare(sr_, 30.0f); }
    void reset() override { ph_[0] = ph_[1] = 0.0; lfo_ = 0.0; mix_.reset(); }
    void applyParams(bool snap) override { mix_.setMix(pct(rmMix), snap); freqS_.setTarget(std::log2(fp(rmFreq))); if (snap) freqS_.snap(freqS_.target()); }
    static float polyBlep(double t, double dt) {
        if (t < dt) { t /= dt; return static_cast<float>(t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return static_cast<float>(t * t + t + t + 1.0); }
        return 0.0f;
    }
    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const int wave = ip(rmWave);
        const double lfoInc = fp(rmModRate) / sr_;
        const float modOct = pct(rmModDepth) * 2.0f;
        for (int i = 0; i < n; ++i) {
            const float lf = std::sin(static_cast<float>(lfo_) * dsp::kTwoPi);
            lfo_ += lfoInc;
            if (lfo_ >= 1.0) lfo_ -= 1.0;
            const float base = std::exp2(freqS_.next() + modOct * lf);
            for (int c = 0; c < 2; ++c) {
                const double f = base + (c ? fp(rmStereo) : 0.0f);
                const double dt = std::clamp(f / sr_, 0.0, 0.45);
                const double t = ph_[c];
                float car;
                if (wave == 1) car = 1.0f - 4.0f * std::fabs(static_cast<float>(t) - 0.5f);                                  // triangle
                else if (wave == 2) {                                                                                     // band-limited square
                    car = t < 0.5 ? 1.0f : -1.0f;
                    car += polyBlep(t, dt);
                    car -= polyBlep(std::fmod(t + 0.5, 1.0), dt);
                } else car = std::sin(static_cast<float>(t) * dsp::kTwoPi);
                ch[c][i] *= car;
                ph_[c] += dt;
                if (ph_[c] >= 1.0) ph_[c] -= 1.0;
            }
        }
        mix_.apply(ch, n);
    }
    double ph_[2] = {0, 0}, lfo_ = 0.0;
    MixStage mix_;
    dsp::Smoother freqS_;
};

// =============================================================================================== Rotary Speaker
enum { roSpeed, roAccel, roBalance, roDrive, roMicWidth, roMix };
class Rotary final : public FxBase {
    void onPrepare() override {
        xo_.setup(800.0f, sr_);
        for (auto& l : horn_) l.prepare(static_cast<int>(0.01 * sr_) + 16);
        for (auto& l : drum_) l.prepare(static_cast<int>(0.01 * sr_) + 16);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
    }
    void reset() override {
        xo_.reset();
        for (auto& l : horn_) l.reset();
        for (auto& l : drum_) l.reset();
        hornPh_ = drumPh_ = 0.0;
        hornHz_ = drumHz_ = -1.0f;
        mix_.reset();
    }
    void applyParams(bool snap) override { mix_.setMix(pct(roMix), snap); }
    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const int speed = ip(roSpeed);  // 0 slow, 1 fast, 2 brake
        const float hornT = speed == 0 ? 0.8f : (speed == 1 ? 6.7f : 0.0f);
        const float drumT = speed == 0 ? 0.65f : (speed == 1 ? 5.9f : 0.0f);
        if (hornHz_ < 0.0f) { hornHz_ = hornT; drumHz_ = drumT; }
        const float acc = std::max(0.1f, fp(roAccel));
        const float kh = 1.0f - std::exp(-1.0f / (acc * 0.35f * srf())), kd = 1.0f - std::exp(-1.0f / (acc * srf()));  // drum is heavier
        const float bal = pct(roBalance);
        const float drive = 1.0f + 4.0f * pct(roDrive);
        const float width = pct(roMicWidth);
        const float hornDepth = 0.00035f * srf(), drumDepth = 0.00012f * srf();
        for (int i = 0; i < n; ++i) {
            hornHz_ += kh * (hornT - hornHz_);
            drumHz_ += kd * (drumT - drumHz_);
            hornPh_ += hornHz_ / sr_;
            drumPh_ += drumHz_ / sr_;
            hornPh_ -= std::floor(hornPh_);
            drumPh_ -= std::floor(drumPh_);
            float x = 0.5f * (ch[0][i] + ch[1][i]);
            if (drive > 1.01f) x = std::tanh(x * drive) / std::tanh(drive) * 0.9f;
            float lo, hi;
            xo_.process(x, lo, hi);
            float out[2];
            for (int m = 0; m < 2; ++m) {  // two mics, +-90 degrees around the cabinet (scaled by width)
                const float mic = (m == 0 ? -0.25f : 0.25f) * width;
                const float hp = static_cast<float>(hornPh_) + mic, dp = static_cast<float>(drumPh_) + mic;
                const float hs = std::sin(dsp::kTwoPi * hp), ds = std::sin(dsp::kTwoPi * dp);
                horn_[m].push(hi);
                drum_[m].push(lo);
                const float h = horn_[m].readCubic(2.0f + hornDepth * (1.0f + hs)) * (1.0f - 0.45f * (0.5f - 0.5f * std::cos(dsp::kTwoPi * hp)));
                const float d = drum_[m].readCubic(2.0f + drumDepth * (1.0f + ds)) * (1.0f - 0.25f * (0.5f - 0.5f * std::cos(dsp::kTwoPi * dp)));
                out[m] = (1.0f - bal) * d * 1.3f + bal * h * 1.3f;
            }
            ch[0][i] = out[0];
            ch[1][i] = out[1];
        }
        mix_.apply(ch, n);
    }
    dsp::Crossover xo_;
    dsp::DelayLine horn_[2], drum_[2];
    double hornPh_ = 0.0, drumPh_ = 0.0;
    float hornHz_ = -1.0f, drumHz_ = -1.0f;
    MixStage mix_;
};

}  // namespace

FxDescriptor describeFlanger() {
    FxDescriptor d;
    d.typeId = "flanger";
    d.name = "Flanger";
    d.family = FxFamily::Modulation;
    d.tooltip = "A jet-like sweep: a very short, moving delay combs the sound into whooshing notches.";
    d.params = {
        P::flt("manual", "Manual", "ms", 0.1f, 10.0f, 1.0f, "Where the sweep is centred. Lower = higher, brighter whoosh.", ParamCurve::Log),
        P::flt("depth", "Depth", "ms", 0.0f, 8.0f, 2.5f, "How far the sweep travels."),
        P::logf("rate", "Rate", "Hz", 0.02f, 10.0f, 0.25f, "Speed of the sweep."),
        P::flt("feedback", "Feedback", "%", -100.0f, 100.0f, 55.0f, "Resonance of the notches. Negative values give a hollower, metallic flavour."),
        P::flt("stereo", "Stereo", "deg", 0.0f, 180.0f, 90.0f, "Offsets the right sweep for a moving stereo jet."),
        P::choice("shape", "Shape", {"Sine", "Triangle", "Square", "Ramp"}, 1, "Shape of the sweep motion."),
        P::pct("mix", "Mix", 100.0f, "100% = classic flanging (equal dry and swept). Lower values soften it."),
    };
    d.params[flShape].advanced = true;
    d.create = [] { return std::make_unique<Flanger>(); };
    return d;
}

FxDescriptor describePhaser() {
    FxDescriptor d;
    d.typeId = "phaser";
    d.name = "Phaser";
    d.family = FxFamily::Modulation;
    d.tooltip = "A swirling, liquid sweep made by moving notches; softer and more vocal than a flanger.";
    d.params = {
        P::choice("stages", "Stages", {"2", "4", "6", "8", "10", "12"}, 2, "More stages = more notches and a thicker, more pronounced swirl."),
        P::logf("centre", "Centre", "Hz", 100.0f, 5000.0f, 800.0f, "Middle of the sweep."),
        P::flt("range", "Range", "oct", 0.0f, 5.0f, 3.0f, "How many octaves the notches travel."),
        P::logf("rate", "Rate", "Hz", 0.02f, 10.0f, 0.4f, "Speed of the swirl."),
        P::flt("feedback", "Resonance", "%", -100.0f, 100.0f, 40.0f, "Emphasis around the notches. High values sing and whistle."),
        P::flt("stereo", "Stereo", "deg", 0.0f, 180.0f, 90.0f, "Offsets the right channel's sweep."),
        P::pct("mix", "Mix", 100.0f, "100% = full phasing (equal dry and shifted). Lower softens."),
    };
    d.create = [] { return std::make_unique<Phaser>(); };
    return d;
}

FxDescriptor describeVibrato() {
    FxDescriptor d;
    d.typeId = "vibrato";
    d.name = "Vibrato";
    d.family = FxFamily::Modulation;
    d.tooltip = "Pure pitch wobble with no dry copy underneath: from a gentle singer's vibrato to warped tape.";
    d.params = {
        P::flt("depth", "Depth", "ct", 0.0f, 100.0f, 20.0f, "How far the pitch swings, in cents (100 = a semitone)."),
        P::logf("rate", "Rate", "Hz", 0.1f, 14.0f, 5.5f, "Speed of the wobble."),
        P::choice("shape", "Shape", {"Sine", "Triangle", "Square", "Ramp"}, 0, "Shape of the pitch movement."),
        P::flt("stereo", "Stereo", "deg", 0.0f, 180.0f, 0.0f, "Lets left and right wobble out of step for width."),
        P::toggle("sync", "Sync", false, "Lock the rate to the song tempo."),
        P::choice("division", "Division", divisionLabels(), 4, "Wobble cycle as a note length when Sync is on."),
        P::pct("mix", "Mix", 100.0f, "100% = pure vibrato. Lower values blend in the steady original (becomes chorus-like)."),
    };
    for (int i : {viShape, viStereo}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Vibrato>(); };
    return d;
}

FxDescriptor describeTremolo() {
    FxDescriptor d;
    d.typeId = "tremolo";
    d.name = "Tremolo / Pan";
    d.family = FxFamily::Modulation;
    d.tooltip = "Rhythmic volume pulsing, or sound moving left and right, locked to your song's tempo.";
    d.params = {
        P::choice("mode", "Mode", {"Tremolo", "Auto-Pan"}, 0, "Tremolo pulses the volume; Auto-Pan swings the sound between speakers."),
        P::toggle("sync", "Sync", true, "Lock the pulse to the song tempo (and to the beat while playing)."),
        P::logf("rate", "Rate", "Hz", 0.1f, 20.0f, 4.0f, "Pulse speed when Sync is off."),
        P::choice("division", "Division", divisionLabels(), 4, "Pulse length as a note value when Sync is on."),
        P::pct("depth", "Depth", 70.0f, "How strong the pulse or movement is."),
        P::choice("shape", "Shape", {"Smooth", "Triangle", "Square", "Ramp"}, 0, "Smooth swells, even ramps, choppy gating or saw-like pulses."),
        P::flt("stereo", "Stereo", "deg", 0.0f, 180.0f, 0.0f, "Offsets the right channel's pulse (Tremolo mode)."),
    };
    d.create = [] { return std::make_unique<Tremolo>(); };
    return d;
}

FxDescriptor describeRingMod() {
    FxDescriptor d;
    d.typeId = "ring_mod";
    d.name = "Ring Modulator";
    d.family = FxFamily::Modulation;
    d.tooltip = "Multiplies the sound by a tone: bell-like, metallic, robotic and inharmonic colours.";
    d.params = {
        P::logf("freq", "Frequency", "Hz", 1.0f, 5000.0f, 220.0f, "The tone the sound is multiplied by. Low = tremolo-like warble, high = clangorous metal."),
        P::choice("wave", "Wave", {"Sine", "Triangle", "Square"}, 0, "Carrier shape. Sine is purest; square is buzzier."),
        P::pct("mod_depth", "Drift", 0.0f, "Makes the carrier frequency wander (up to two octaves)."),
        P::logf("mod_rate", "Drift Rate", "Hz", 0.05f, 10.0f, 0.5f, "Speed of the wander."),
        P::flt("stereo", "Stereo", "Hz", 0.0f, 50.0f, 0.0f, "Offsets the right carrier for a beating stereo effect."),
        P::pct("mix", "Mix", 60.0f, "Blend of original and ring-modulated sound."),
    };
    d.create = [] { return std::make_unique<RingMod>(); };
    return d;
}

FxDescriptor describeRotary() {
    FxDescriptor d;
    d.typeId = "rotary";
    d.name = "Rotary Speaker";
    d.family = FxFamily::Modulation;
    d.tooltip = "A spinning horn and drum speaker cabinet: swirling Doppler pitch, throbbing volume and real spin-up.";
    d.params = {
        P::choice("speed", "Speed", {"Slow", "Fast", "Brake"}, 0, "Rotor speed. Switching speeds speeds up or slows down gradually, like the real cabinet."),
        P::flt("accel", "Inertia", "s", 0.2f, 6.0f, 1.8f, "How long the rotors take to change speed. The heavier drum always lags the horn."),
        P::pct("balance", "Horn/Drum", 55.0f, "Balance of the treble horn and the bass drum rotor."),
        P::pct("drive", "Drive", 15.0f, "Overdrives the amplifier in front of the speakers."),
        P::pct("mic_width", "Mic Width", 80.0f, "Distance between the two microphones: wider = bigger stereo swirl."),
        P::pct("mix", "Mix", 100.0f, "Blend of original and rotary sound."),
    };
    d.create = [] { return std::make_unique<Rotary>(); };
    return d;
}

}  // namespace ih1::fx
