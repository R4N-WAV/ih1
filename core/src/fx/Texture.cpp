// Texture family: Granular Cloud and Stutter/Glitch (KADAL KNW-01J68SK1RC; brief §5.12, §5.14).
// Granular = many overlapping micro-grains from a rolling buffer (texture, not echoes).
// Stutter  = a slice captured on the musical grid and retriggered (rhythmic repeats).
#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Grains.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

// ===================================================================================== Granular Cloud
enum { gSize, gDensity, gPosition, gSpray, gPitch, gScatter, gReverse, gSpread, gEnv, gJitter, gFreeze, gFeedback, gDiffusion, gMix };

class Granular final : public FxBase {
    void onPrepare() override {
        cloud_.prepare(sr_, 8.0f, 0xC10D);
        static const float msL[4] = {5.3f, 7.9f, 11.1f, 17.3f}, msR[4] = {6.1f, 8.7f, 12.7f, 19.1f};
        for (int k = 0; k < 4; ++k) {
            apL_[k].prepare(static_cast<int>(msL[k] * 0.001f * srf()));
            apR_[k].prepare(static_cast<int>(msR[k] * 0.001f * srf()));
        }
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
    }
    void reset() override {
        cloud_.reset();
        for (auto& a : apL_) a.reset();
        for (auto& a : apR_) a.reset();
        for (auto& f : fbLp_) f.reset();
        mix_.reset();
        fb_[0] = fb_[1] = 0.0f;
        heard_ = 0;
    }
    void applyParams(bool snap) override {
        for (auto& f : fbLp_) f.setup(dsp::Svf::Lowpass, 8000.0f, 0.6f, 0, sr_);
        mix_.setMix(pct(gMix), snap);
    }
    double tailSeconds() const override { return 2.5; }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        dsp::GrainCloud::Params gp;
        gp.sizeSamples = fp(gSize) * 0.001f * srf();
        gp.density = fp(gDensity);
        gp.positionSamples = fp(gPosition) * 0.001f * srf();
        gp.spraySamples = fp(gSpray) * 0.001f * srf();
        gp.pitchRatio = dsp::semitonesToRatio(fp(gPitch));
        gp.pitchScatterSt = fp(gScatter);
        gp.reverseProb = pct(gReverse);
        gp.spread = pct(gSpread);
        gp.envelope = ip(gEnv);
        gp.timingJitter = pct(gJitter);
        // Freeze holds the first real sound it hears: keep recording until ~0.75 s of signal has arrived.
        const bool want = bp(gFreeze);
        if (!want) heard_ = 0;
        const float fb = pct(gFeedback) * 0.9f;
        const float diff = pct(gDiffusion);
        const int need = static_cast<int>(0.75 * sr_);
        for (int i = 0; i < n; ++i) {
            if (want && heard_ < need && std::fabs(ch[0][i]) + std::fabs(ch[1][i]) > 0.002f) ++heard_;
            cloud_.setFrozen(want && heard_ >= need);
            float l, r;
            cloud_.tick(gp, l, r);
            cloud_.write(ch[0][i] + fb * fb_[0], ch[1][i] + fb * fb_[1]);
            // V5 Phase 5: the recirculated signal is the cloud output scaled by its own weight bound (taken before
            // the output diffusion), so the loop gain is <= fb (0.9 at 100 %) at any density / size.
            const float ks = cloud_.feedbackScale();
            fb_[0] = 2.0f * std::tanh(0.5f * fbLp_[0].process(l * ks));
            fb_[1] = 2.0f * std::tanh(0.5f * fbLp_[1].process(r * ks));
            float dl = l, dr = r;
            for (auto& a : apL_) dl = a.process(dl, 0.55f);
            for (auto& a : apR_) dr = a.process(dr, 0.55f);
            l = (1.0f - diff) * l + diff * dl;
            r = (1.0f - diff) * r + diff * dr;
            ch[0][i] = l;
            ch[1][i] = r;
        }
        for (auto& f : fbLp_) f.flush();
        mix_.apply(ch, n);
    }

    dsp::GrainCloud cloud_;
    dsp::Allpass apL_[4], apR_[4];
    dsp::Svf fbLp_[2];
    MixStage mix_;
    float fb_[2] = {0, 0};
    int heard_ = 0;
};

// ===================================================================================== Stutter
enum { tInterval, tOffset, tGate, tSlice, tChance, tPitch, tDecay, tReverse, tMix };
constexpr double kIntervalBeats[4] = {1.0, 2.0, 4.0, 8.0};
constexpr double kGateFrac[6] = {1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 3.0 / 4, 1.0};
constexpr double kSliceBeats[6] = {0.125, 1.0 / 6, 0.25, 1.0 / 3, 0.5, 1.0};

class Stutter final : public FxBase {
    void onPrepare() override {
        for (auto& l : line_) l.prepare(static_cast<int>(10.0 * sr_));
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
    }
    void reset() override {
        for (auto& l : line_) l.reset();
        mix_.reset();
        freeQ_ = 0.0;
        active_ = false;
        tailing_ = false;
        lastTrig_ = -1;
        abs_ = 0;
        wet_ = 0.0f;
    }
    void applyParams(bool snap) override { mix_.setMix(pct(tMix), snap); }

    static uint32_t hash(int64_t x) {
        uint64_t z = static_cast<uint64_t>(x) * 0x9E3779B97F4A7C15ull + 0x1234567ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return static_cast<uint32_t>(z >> 32);
    }

    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const double bpm = std::clamp(ctx.bpm, 20.0, 400.0);
        const double spb = 60.0 / bpm * sr_;  // samples per beat
        const double interval = kIntervalBeats[std::clamp(ip(tInterval), 0, 3)];
        const double offset = std::fmod(ip(tOffset) * 0.25, interval);
        const double gate = kGateFrac[std::clamp(ip(tGate), 0, 5)] * interval;
        const double sliceSamples = std::max(64.0, kSliceBeats[std::clamp(ip(tSlice), 0, 5)] * spb);
        const float chance = pct(tChance);
        const float pitchStep = fp(tPitch);
        const float decay = pct(tDecay);
        const bool rev = bp(tReverse);
        const bool hostClock = ctx.ppqValid && ctx.hostPlaying;
        const double fade = std::min(0.003 * sr_, sliceSamples * 0.25);
        const float wetStep = 1.0f / static_cast<float>(fade);

        for (int i = 0; i < n; ++i) {
            const double q = hostClock ? ctx.ppqPosition + i / spb : freeQ_;
            freeQ_ = q + 1.0 / spb;
            const double rel = q - offset;
            const int64_t trig = static_cast<int64_t>(std::floor(rel / interval));
            const double inEvent = rel - static_cast<double>(trig) * interval;  // beats since trigger
            if (trig != lastTrig_) {
                lastTrig_ = trig;
                const bool fire = (hash(trig) & 0xFFFFFF) / 16777216.0f < chance;
                // V4 Phase 4: a new trigger while the previous event is still audible keeps rendering that event
                // (its own slice, window and pitch) until its wet level has faded out; it is never swapped mid-level
                if (wet_ > 0.0f) { tailStart_ = startAbs_; tailing_ = true; }
                active_ = fire;
                startAbs_ = abs_ - static_cast<int64_t>(inEvent * spb);
            }
            for (int c = 0; c < 2; ++c) line_[c].push(ch[c][i]);
            const double e = static_cast<double>(abs_ - startAbs_);  // samples since trigger
            const bool repeating = !tailing_ && active_ && inEvent < gate && e >= sliceSamples;
            wet_ = repeating ? std::min(1.0f, wet_ + wetStep) : std::max(0.0f, wet_ - wetStep);
            if (wet_ <= 0.0f) tailing_ = false;
            if (wet_ > 0.0f) {
                const int64_t src = tailing_ ? tailStart_ : startAbs_;
                const double es = static_cast<double>(abs_ - src);
                const int64_t k = static_cast<int64_t>(es / sliceSamples);
                const double inSlice = es - static_cast<double>(k) * sliceSamples;
                // cumulative transposition is capped at +-24 st: compounding steps never run the read head into
                // aliasing hash (or past the live input)
                // the pitch step is latched per repetition (a change lands on the next repeat, never mid-slice)
                if (k != lastK_ || tailing_ != lastTail_) { lastK_ = k; lastTail_ = tailing_; latchedStep_ = pitchStep; }
                const float rate = std::exp2(std::clamp(static_cast<float>(k) * latchedStep_, -24.0f, 24.0f) / 12.0f);
                // pitched repeats read further into the source instead of wrapping inside the slice (no jump)
                double rp = inSlice * rate;
                if (rev) rp = sliceSamples - 1.0 - rp;
                // raised-cosine window on every repetition (no slope corner at the slice boundaries)
                float w = 1.0f;
                if (inSlice < fade) w = 0.5f - 0.5f * std::cos(dsp::kPi * static_cast<float>(inSlice / fade));
                else if (inSlice > sliceSamples - fade) w = 0.5f - 0.5f * std::cos(dsp::kPi * static_cast<float>((sliceSamples - inSlice) / fade));
                const float g = std::pow(1.0f - decay * 0.5f, static_cast<float>(std::max<int64_t>(0, k - 1))) * w;
                const double d = es - rp;  // delay back to the slice sample
                for (int c = 0; c < 2; ++c) {
                    const float s = line_[c].readLinear(static_cast<float>(std::clamp(d, 1.0, static_cast<double>(line_[c].capacity() - 2))));
                    ch[c][i] = (1.0f - wet_) * ch[c][i] + wet_ * g * s;
                }
            }
            ++abs_;
        }
        mix_.apply(ch, n);
    }

    dsp::DelayLine line_[2];
    MixStage mix_;
    double freeQ_ = 0.0;
    bool active_ = false;
    int64_t lastTrig_ = -1, abs_ = 0, startAbs_ = 0, tailStart_ = 0, lastK_ = -1;
    bool tailing_ = false, lastTail_ = false;
    float latchedStep_ = 0.0f;
    float wet_ = 0.0f;
};

}  // namespace

FxDescriptor describeGranular() {
    FxDescriptor d;
    d.typeId = "granular";
    d.name = "Granular Cloud";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Breaks the recent sound into many tiny overlapping grains and scatters them into a shimmering cloud.";
    d.params = {
        P::logf("size_ms", "Grain Size", "ms", 10.0f, 500.0f, 90.0f, "Length of each grain. Short = buzzy, glittery. Long = smeared phrases."),
        P::logf("density", "Density", "/s", 1.0f, 100.0f, 18.0f, "Grains per second. Low = sparse sparkles, high = a continuous cloud."),
        P::flt("position_ms", "Position", "ms", 0.0f, 2000.0f, 250.0f, "How far back in time the grains are taken from."),
        P::flt("spray_ms", "Spray", "ms", 0.0f, 2000.0f, 300.0f, "Random scatter of where grains are taken from. More = more blurred in time."),
        P::flt("pitch", "Pitch", "st", -24.0f, 24.0f, 0.0f, "Transposes every grain."),
        P::flt("scatter", "Pitch Scatter", "st", 0.0f, 12.0f, 0.0f, "Random pitch for each grain: a glittering, detuned cloud."),
        P::pct("reverse", "Reverse", 0.0f, "Chance that a grain plays backwards."),
        P::pct("spread", "Spread", 60.0f, "How widely grains are scattered across the stereo field."),
        P::choice("envelope", "Envelope", {"Smooth", "Flat", "Pluck"}, 0, "Shape of each grain: soft swell, fuller body, or a plucked attack."),
        P::pct("jitter", "Jitter", 40.0f, "Irregular grain timing. 0% gives a steady pulse; high values sound natural and airy."),
        P::toggle("freeze", "Freeze", false, "Stops recording so the cloud keeps playing the captured moment forever."),
        P::pct("feedback", "Feedback", 0.0f, "Feeds the cloud back into itself for evolving, denser textures."),
        P::pct("diffusion", "Diffusion", 30.0f, "Blurs grains together into a soft haze."),
        P::pct("mix", "Mix", 50.0f, "Balance between original and cloud. 50% keeps both at full level."),
    };
    for (int i : {gScatter, gEnv, gJitter, gFeedback}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Granular>(); };
    return d;
}

FxDescriptor describeStutter() {
    FxDescriptor d;
    d.typeId = "stutter";
    d.name = "Stutter";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Grabs a slice on the beat and repeats it rhythmically: glitch rolls, buffer repeats and stutters.";
    d.params = {
        P::choice("interval", "Every", {"1 beat", "2 beats", "1 bar", "2 bars"}, 2, "How often a stutter may happen."),
        P::flt("offset", "Offset", "16th", 0.0f, 31.0f, 8.0f, "Where in the interval it happens, in sixteenth notes."),
        P::choice("gate", "Length", {"1/16 of interval", "1/8", "1/4", "1/2", "3/4", "Whole"}, 3, "How long the stutter lasts."),
        P::choice("slice", "Slice", {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4"}, 2, "Size of the repeated piece. Smaller = faster, buzzier rolls."),
        P::pct("chance", "Chance", 100.0f, "Probability that each interval stutters (repeatable: the same bars stutter every playback)."),
        P::flt("pitch", "Pitch Step", "st", -12.0f, 12.0f, 0.0f, "Each repeat moves up or down by this many semitones."),
        P::pct("decay", "Decay", 0.0f, "Each repeat gets quieter."),
        P::toggle("reverse", "Reverse", false, "Repeats play backwards."),
        P::pct("mix", "Mix", 100.0f, "100% replaces the sound during a stutter; lower values layer it."),
    };
    d.params[tOffset].kind = ParamKind::Choice;
    for (int i = 0; i < 32; ++i) d.params[tOffset].choices.push_back(std::to_string(i));
    d.create = [] { return std::make_unique<Stutter>(); };
    return d;
}

}  // namespace ih1::fx
