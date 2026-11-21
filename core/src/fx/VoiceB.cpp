// Voice / space family, Batch B: Shimmer, Pitch Correction, Vocoder.
// KADAL: KNW-ZWRYBGRD0Y (FDN), KNW-Q798GYV78J (period-tracking pitch correction: cycle repeat/drop read
// pointer, note hysteresis, retune speed), KNW-Q1PTF3K9KM (YIN), crossover/filter-bank primitives.
// Vocoder is a true carrier/modulator filter bank (brief: NOT the Vocalist EX "vocoder" preset).
#include <array>

#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Spectral.h"
#include "ih1/dsp/Svf.h"
#include "ih1/dsp/TdPitch.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

// =============================================================================================== Shimmer
enum { shDecay, shSize, shShift1, shShift2, shBalance, shRegen, shRouting, shTone, shLowCut, shMod, shPredelay, shMix };
class Shimmer final : public FxBase {
    static constexpr int N = 8;
    void onPrepare() override {
        static const float ms[N] = {31.1f, 37.9f, 43.3f, 49.7f, 57.1f, 63.7f, 71.3f, 79.9f};
        for (int i = 0; i < N; ++i) {
            baseMs_[i] = ms[i];
            line_[i].prepare(static_cast<int>(ms[i] * 2.0f * 0.001f * srf()) + 64);
            lfo_[i].prepare(sr_, 60 + static_cast<uint32_t>(i));
            lfo_[i].setRate(0.11f + 0.067f * static_cast<float>(i));
        }
        for (int k = 0; k < 4; ++k) diff_[k].prepare(static_cast<int>((3.1f + 2.3f * static_cast<float>(k)) * 0.002f * srf()));
        pre_.prepare(static_cast<int>(0.3 * sr_));
        ps1_.prepare(sr_, 60.0f, 0.0f);
        ps2_.prepare(sr_, 60.0f, 0.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
    }
    void reset() override {
        for (auto& l : line_) l.reset();
        for (auto& a : diff_) a.reset();
        pre_.reset(); ps1_.reset(); ps2_.reset(); hp_.reset(); lp_.reset();
        for (auto& a : absorb_) a = 0.0f;
        fb_ = 0.0f;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        const float size = 0.5f + 1.5f * pct(shSize);
        const float t60 = fp(shDecay);
        for (int i = 0; i < N; ++i) {
            delay_[i] = baseMs_[i] * size * 0.001f * srf();
            const float dSec = delay_[i] / srf();
            const float gL = std::pow(10.0f, -3.0f * dSec / t60), gH = std::pow(10.0f, -3.0f * dSec / (t60 * 0.5f));
            const float ratio = gH / gL;
            absB_[i] = (1.0f - ratio) / (1.0f + ratio);
            absK_[i] = gL;
        }
        for (int k = 0; k < 4; ++k) diff_[k].setDelay(std::max(1, static_cast<int>((3.1f + 2.3f * static_cast<float>(k)) * 0.001f * srf() * size)));
        hp_.setup(dsp::Svf::Highpass, fp(shLowCut), 0.6f, 0, sr_);
        lp_.setup(dsp::Svf::Lowpass, fp(shTone), 0.6f, 0, sr_);
        mix_.setMix(pct(shMix), snap);
    }
    double tailSeconds() const override { return std::min(60.0, 2.0 * p_[shDecay] + 1.0); }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float r1 = dsp::semitonesToRatio(fp(shShift1)), r2 = dsp::semitonesToRatio(fp(shShift2));
        const float bal = pct(shBalance);
        const float regen = std::min(pct(shRegen), 1.0f) * 0.55f;  // capped: pitch in the loop must stay bounded
        const int routing = ip(shRouting);  // 0 tank, 1 input, 2 both
        const float modD = pct(shMod) * 0.0006f * srf();
        const float pre = fp(shPredelay) * 0.001f * srf();
        for (int i = 0; i < n; ++i) {
            pre_.push(0.5f * (ch[0][i] + ch[1][i]));
            float x = pre_.readLinear(std::max(0.0f, pre));
            if (routing >= 1) x = 0.5f * x + 0.5f * ((1.0f - bal) * ps1_.process(x, r1, 0.0f) + bal * ps2_.process(x, r2, 0.0f));
            float in = x + (routing != 1 ? fb_ : 0.0f);
            for (auto& a : diff_) in = a.process(in, 0.6f);
            float s[N], sum = 0.0f;
            for (int j = 0; j < N; ++j) {
                const float v = line_[j].readCubic(std::max(2.0f, delay_[j] + (j % 2 ? -1.0f : 1.0f) * modD * (0.5f + 0.5f * lfo_[j].next())));
                absorb_[j] = absK_[j] * (1.0f - absB_[j]) * v + absB_[j] * absorb_[j];
                s[j] = absorb_[j];
                sum += s[j];
            }
            const float h = 2.0f / N * sum;
            for (int j = 0; j < N; ++j) line_[j].push(s[j] - h + in * (j % 4 < 2 ? 0.45f : -0.45f));
            const float wl = (s[0] - s[2] + s[4] - s[6]) * 0.5f, wr = (s[1] - s[3] + s[5] - s[7]) * 0.5f;
            if (routing != 1) {
                const float w = 0.5f * (wl + wr);
                const float shifted = (1.0f - bal) * ps1_.process(w, r1, 0.0f) + bal * ps2_.process(w, r2, 0.0f);
                fb_ = std::tanh(lp_.process(hp_.process(shifted)) * regen * 1.5f) / 1.5f;
            }
            ch[0][i] = wl;
            ch[1][i] = wr;
        }
        for (auto& a : absorb_) a = dsp::flushTiny(a);
        hp_.flush(); lp_.flush();
        mix_.apply(ch, n);
    }
    dsp::DelayLine line_[N], pre_;
    dsp::Lfo lfo_[N];
    dsp::Allpass diff_[4];
    dsp::TdPitchShifter ps1_, ps2_;
    dsp::Svf hp_, lp_;
    float baseMs_[N] = {}, delay_[N] = {}, absB_[N] = {}, absK_[N] = {}, absorb_[N] = {};
    float fb_ = 0.0f;
    MixStage mix_;
};

// =============================================================================================== Pitch Correction
enum { pcKey, pcScale, pcSpeed, pcTolerance, pcAmount, pcMix };
constexpr int kScaleSets[8][12] = {
    {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1}, {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0}, {1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0}, {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0},
    {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1}, {1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0}, {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};
class PitchCorrect final : public FxBase {
    static constexpr int kBuf = 1 << 14;
    void onPrepare() override {
        yin_.prepare(sr_, sr_ > 60000 ? 4 : 2, 512);
        for (auto& b : buf_) b.assign(static_cast<size_t>(kBuf), 0.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
    }
    void reset() override {
        for (auto& b : buf_) std::fill(b.begin(), b.end(), 0.0f);
        w_ = 0;
        period_ = static_cast<float>(sr_ / 200.0);
        rp_ = -static_cast<double>(2.0 * period_);
        xfPos_ = 0.0;
        xf_ = 0.0f;
        logR_ = 0.0f;
        target_ = -1;
        ctr_ = 0;
        mix_.reset();
    }
    void applyParams(bool snap) override { mix_.setMix(pct(pcMix), snap); }
    int nearestInScale(float midi) const {
        const int key = ip(pcKey), sc = std::clamp(ip(pcScale), 0, 7);
        int best = static_cast<int>(std::round(midi));
        float bd = 1e9f;
        for (int m = static_cast<int>(midi) - 2; m <= static_cast<int>(midi) + 2; ++m)
            if (kScaleSets[sc][((m - key) % 12 + 12) % 12] && std::fabs(static_cast<float>(m) - midi) < bd) { bd = std::fabs(static_cast<float>(m) - midi); best = m; }
        return best;
    }
    float readAt(int c, double pos) const {
        const int i0 = static_cast<int>(std::floor(pos));
        const float f = static_cast<float>(pos - i0);
        const auto& b = buf_[c];
        const float xm1 = b[static_cast<size_t>((i0 - 1) & (kBuf - 1))], x0 = b[static_cast<size_t>(i0 & (kBuf - 1))];
        const float x1 = b[static_cast<size_t>((i0 + 1) & (kBuf - 1))], x2 = b[static_cast<size_t>((i0 + 2) & (kBuf - 1))];
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float speedMs = fp(pcSpeed);
        const float k = speedMs <= 0.5f ? 1.0f : 1.0f - std::exp(-1.0f / (speedMs * 0.001f * srf()));
        const float tol = fp(pcTolerance) * 0.01f;
        const float amount = pct(pcAmount);
        const int xfLen = 96;
        for (int i = 0; i < n; ++i) {
            const float m = 0.5f * (ch[0][i] + ch[1][i]);
            yin_.push(m);
            if (++ctr_ >= 256) {  // control rate: detect, choose the note (with hysteresis)
                ctr_ = 0;
                const float f0 = yin_.estimate();
                if (f0 > 0.0f) {
                    period_ = static_cast<float>(sr_ / f0);
                    const float midi = 69.0f + 12.0f * std::log2(f0 / 440.0f);
                    if (target_ < 0 || std::fabs(midi - static_cast<float>(target_)) > 0.5f + tol) target_ = nearestInScale(midi);
                    tgtLog_ = amount * (static_cast<float>(target_) - midi) / 12.0f;
                } else {
                    tgtLog_ = 0.0f;  // unvoiced / silent: pass through unshifted
                }
            }
            logR_ += k * (tgtLog_ - logR_);
            const double r = std::exp2(static_cast<double>(logR_));
            for (int c = 0; c < 2; ++c) buf_[c][static_cast<size_t>(w_ & (kBuf - 1))] = ch[c][i];
            ++w_;
            rp_ += r;
            const double P = std::clamp(static_cast<double>(period_), 32.0, 2000.0);
            const double dist = static_cast<double>(w_) - rp_;
            // repeat or drop whole cycles at matching phase (KADAL KNW-Q798GYV78J); crossfade the jump
            if (dist < P + 4.0) { xfPos_ = rp_; rp_ -= P; xf_ = 1.0f; }
            else if (dist > 3.0 * P + 8.0) { xfPos_ = rp_; rp_ += P; xf_ = 1.0f; }
            for (int c = 0; c < 2; ++c) {
                float y = readAt(c, rp_);
                if (xf_ > 0.0f) y = (1.0f - xf_) * y + xf_ * readAt(c, xfPos_);
                ch[c][i] = y;
            }
            if (xf_ > 0.0f) { xfPos_ += r; xf_ = std::max(0.0f, xf_ - 1.0f / static_cast<float>(xfLen)); }
        }
        mix_.apply(ch, n);
    }
    dsp::Yin yin_;
    std::vector<float> buf_[2];
    int64_t w_ = 0;
    double rp_ = 0.0, xfPos_ = 0.0;
    float xf_ = 0.0f, period_ = 240.0f, logR_ = 0.0f, tgtLog_ = 0.0f;
    int target_ = -1, ctr_ = 0;
    MixStage mix_;
};

// =============================================================================================== Vocoder
enum { voBands, voCarrier, voNote, voUseMidi, voLow, voHigh, voAttack, voRelease, voShift, voUnvoiced, voMix };
class Vocoder final : public FxBase {
    static constexpr int kMaxBands = 32;
    static constexpr int kBandChoices[5] = {8, 12, 16, 24, 32};
    static constexpr float kVocoderLevel = 1.6f;  // calibrated: output RMS ~ input RMS for speech-like input
    void onPrepare() override { mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear); }
    void reset() override {
        for (auto& f : ana_) f.reset();
        for (auto& f : syn_) f.reset();
        for (auto& f : synR_) f.reset();
        for (auto& e : env_) e = 0.0f;
        for (auto& p : ph_) p = 0.0;
        for (auto& nn : notes_) nn = -1;
        rng_.seed_(5);
        mix_.reset();
    }
    void applyParams(bool snap) override {
        bands_ = kBandChoices[std::clamp(ip(voBands), 0, 4)];
        const float lo = fp(voLow), hi = std::max(fp(voHigh), lo * 2.0f);
        const float ratio = std::pow(hi / lo, 1.0f / static_cast<float>(bands_ - 1));
        const float q = std::sqrt(ratio) / (ratio - 1.0f) * 1.3f;
        const float shift = dsp::semitonesToRatio(fp(voShift));
        for (int b = 0; b < bands_; ++b) {
            const float f = lo * std::pow(ratio, static_cast<float>(b));
            ana_[b].setup(dsp::Svf::Bandpass, f, q, 0, sr_);
            syn_[b].setup(dsp::Svf::Bandpass, std::min(f * shift, 0.45f * srf()), q, 0, sr_);
            synR_[b].setup(dsp::Svf::Bandpass, std::min(f * shift, 0.45f * srf()), q, 0, sr_);
        }
        // The SVF band-pass peaks at Q. Analysis and synthesis both pass through one, so normalise by 1/Q^2
        // (unity-peak bands). Summing N narrow bands of a broadband carrier, each driven by an envelope
        // ~ sqrt(1/N) of a broadband modulator, then needs sqrt(N) to keep the output level independent of
        // the band count. V1 scaled with ~bands^2 and hit the safety limiter at 16+ bands.
        gain_ = kVocoderLevel * std::sqrt(static_cast<float>(bands_)) / (q * q);
        mix_.setMix(pct(voMix), snap);
    }
    void handleMidi(const MidiEvent& e) {
        if (e.isNoteOn()) {
            for (auto& nn : notes_) if (nn == e.data1) return;
            for (auto& nn : notes_) if (nn < 0) { nn = e.data1; return; }
            notes_[0] = notes_[1]; notes_[1] = notes_[2]; notes_[2] = notes_[3]; notes_[3] = e.data1;  // newest wins
        } else if (e.isNoteOff()) {
            for (auto& nn : notes_) if (nn == e.data1) nn = -1;
        } else if (e.isAllNotesOff()) {
            for (auto& nn : notes_) nn = -1;
        }
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const int car = ip(voCarrier);  // 0 saw, 1 pulse, 2 noise, 3 saw+noise
        const float aA = 1.0f - std::exp(-1.0f / (fp(voAttack) * 0.001f * srf()));
        const float aR = 1.0f - std::exp(-1.0f / (fp(voRelease) * 0.001f * srf()));
        const float unv = pct(voUnvoiced);
        const bool useMidi = bp(voUseMidi);
        int ev = 0;
        for (int i = 0; i < n; ++i) {
            while (ev < ctx.numMidi && ctx.midi[ev].sampleOffset - ctx.blockOffset <= i) handleMidi(ctx.midi[ev++]);
            // carrier: up to 4 MIDI voices (or the fixed note), band-limited by polyBLEP
            int active[4], na = 0;
            if (useMidi) for (int nn : notes_) if (nn >= 0) active[na++] = nn;
            if (na == 0) active[na++] = ip(voNote);
            float carrier = 0.0f;
            for (int v = 0; v < na; ++v) {
                const double dt = 440.0 * std::exp2((active[v] - 69) / 12.0) / sr_;
                double& p = ph_[v];
                float s;
                if (car == 1) {
                    s = p < 0.5 ? 1.0f : -1.0f;
                    s += blep(p, dt) - blep(std::fmod(p + 0.5, 1.0), dt);
                } else {
                    s = static_cast<float>(2.0 * p - 1.0) - blep(p, dt);
                }
                carrier += s;
                p += dt;
                if (p >= 1.0) p -= 1.0;
            }
            carrier /= std::sqrt(static_cast<float>(na));
            const float noise = rng_.bipolar();
            if (car == 2) carrier = noise;
            else if (car == 3) carrier = 0.75f * carrier + 0.35f * noise;
            const float mod = 0.5f * (ch[0][i] + ch[1][i]);
            float outL = 0.0f, outR = 0.0f;
            for (int b = 0; b < bands_; ++b) {
                const float a = std::fabs(ana_[b].process(mod));
                env_[b] += (a > env_[b] ? aA : aR) * (a - env_[b]);
                // consonants: top quarter of the bands also receives noise so 's' and 't' stay intelligible
                const float src = (b >= bands_ * 3 / 4) ? carrier + unv * 1.5f * noise : carrier;
                const float y = env_[b] * gain_;
                outL += syn_[b].process(src) * y * (b % 2 ? 0.8f : 1.2f);
                outR += synR_[b].process(src) * y * (b % 2 ? 1.2f : 0.8f);
            }
            ch[0][i] = outL;
            ch[1][i] = outR;
        }
        while (ev < ctx.numMidi) handleMidi(ctx.midi[ev++]);
        for (int b = 0; b < bands_; ++b) { ana_[b].flush(); syn_[b].flush(); synR_[b].flush(); env_[b] = dsp::flushTiny(env_[b]); }
        mix_.apply(ch, n);
    }
    static float blep(double t, double dt) {
        if (t < dt) { t /= dt; return static_cast<float>(t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return static_cast<float>(t * t + t + t + 1.0); }
        return 0.0f;
    }
    dsp::Svf ana_[kMaxBands], syn_[kMaxBands], synR_[kMaxBands];
    float env_[kMaxBands] = {};
    int bands_ = 16;
    float gain_ = 1.0f;
    double ph_[4] = {0, 0, 0, 0};
    int notes_[4] = {-1, -1, -1, -1};
    dsp::Rng rng_{5};
    MixStage mix_;
};

}  // namespace

FxDescriptor describeShimmer() {
    FxDescriptor d;
    d.typeId = "shimmer";
    d.name = "Shimmer";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "A huge reverb whose tail climbs in pitch: octaves and fifths blooming into a luminous halo.";
    d.params = {
        P::logf("decay", "Decay", "s", 0.5f, 30.0f, 7.0f, "Length of the reverb tail."),
        P::pct("size", "Size", 60.0f, "How vast the space feels."),
        P::flt("shift1", "Shift 1", "st", 0.0f, 24.0f, 12.0f, "First pitch voice in the tail (12 = an octave up)."),
        P::flt("shift2", "Shift 2", "st", -12.0f, 24.0f, 19.0f, "Second pitch voice (19 = octave + fifth)."),
        P::pct("balance", "Balance", 30.0f, "Mix between the two pitch voices."),
        P::pct("regen", "Shimmer", 55.0f, "How strongly the shifted sound feeds back into the reverb: more = brighter, climbing tail."),
        P::choice("routing", "Routing", {"Into the tail", "Into the input", "Both"}, 0, "Where pitch shifting happens: in the regenerating tail (classic climbing shimmer), only on the way in, or both."),
        P::logf("tone", "Tone", "Hz", 1000.0f, 18000.0f, 7000.0f, "Keeps the shimmer from getting harsh."),
        P::logf("low_cut", "Low Cut", "Hz", 50.0f, 1000.0f, 250.0f, "Keeps low mud out of the regenerating tail."),
        P::pct("modulation", "Modulation", 30.0f, "Gentle movement inside the reverb."),
        P::flt("predelay", "Pre-delay", "ms", 0.0f, 250.0f, 30.0f, "Gap before the reverb starts."),
        P::pct("mix", "Mix", 40.0f, "Balance of dry sound and shimmer. 50% keeps both at full level."),
    };
    for (int i : {shBalance, shLowCut, shMod, shPredelay}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Shimmer>(); };
    return d;
}

FxDescriptor describePitchCorrect() {
    FxDescriptor d;
    d.typeId = "pitch_correct";
    d.name = "Pitch Correction";
    d.family = FxFamily::PitchVoice;
    d.tooltip = "Pulls a single voice or instrument onto the notes of a key: from natural tightening to the hard, stepped robotic effect.";
    d.params = {
        P::choice("key", "Key", {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0, "The song's key."),
        P::choice("scale", "Scale", {"Major", "Minor", "Dorian", "Mixolydian", "Harmonic Minor", "Major Pentatonic", "Minor Pentatonic", "Chromatic"}, 0, "Which notes are allowed."),
        P::flt("speed", "Retune Speed", "ms", 0.0f, 400.0f, 40.0f, "How fast notes are pulled in tune. 0 = hard, robotic steps; slower keeps natural slides and vibrato."),
        P::flt("tolerance", "Tolerance", "ct", 0.0f, 50.0f, 10.0f, "How far past halfway the voice must go before switching notes. Prevents flicker between notes."),
        P::pct("amount", "Amount", 100.0f, "How much of the correction is applied."),
        P::pct("mix", "Mix", 100.0f, "Blend of the corrected and original voice."),
    };
    d.create = [] { return std::make_unique<PitchCorrect>(); };
    return d;
}

FxDescriptor describeVocoder() {
    FxDescriptor d;
    d.typeId = "vocoder";
    d.name = "Vocoder";
    d.family = FxFamily::PitchVoice;
    d.tooltip = "The classic talking synth: your sound's articulation shapes a synth carrier played from MIDI or a fixed note.";
    d.wantsMidi = true;
    d.params = {
        P::choice("bands", "Bands", {"8", "12", "16", "24", "32"}, 2, "More bands = clearer speech; fewer = vintage, robotic."),
        P::choice("carrier", "Carrier", {"Saw", "Pulse", "Noise", "Saw + Noise"}, 3, "The synth sound that does the singing. Noise gives a whispery robot."),
        P::flt("note", "Note", "", 24.0f, 84.0f, 48.0f, "Carrier pitch (MIDI note) when no MIDI notes are held."),
        P::toggle("use_midi", "MIDI Notes", true, "Play the carrier from incoming MIDI (up to 4 notes)."),
        P::logf("low", "Low", "Hz", 50.0f, 500.0f, 100.0f, "Lowest band."),
        P::logf("high", "High", "Hz", 2000.0f, 16000.0f, 10000.0f, "Highest band."),
        P::logf("attack", "Attack", "ms", 0.5f, 100.0f, 3.0f, "How quickly bands respond to the voice."),
        P::logf("release", "Release", "ms", 5.0f, 1000.0f, 50.0f, "How long bands ring after the voice stops."),
        P::flt("shift", "Formant", "st", -12.0f, 12.0f, 0.0f, "Shifts the vocoded vowel colour up or down."),
        P::pct("unvoiced", "Consonants", 40.0f, "Adds noise to the top bands so 's' and 't' remain clear."),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and vocoded sound."),
    };
    d.params[voNote].kind = ParamKind::Float;
    for (int i : {voLow, voHigh, voAttack, voRelease}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Vocoder>(); };
    return d;
}

}  // namespace ih1::fx
