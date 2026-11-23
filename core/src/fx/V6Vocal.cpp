// V6 feedback round: three vocal processors that fill gaps in the existing vocal set (Harmony, Pitch Shift with
// natural formants, Formant, Pitch Correct, Vocoder, Doubler, De-esser). Original IH1 designs. The V6 research
// supplement's papers and product manuals were read as DSP / workflow references only; nothing was copied.
//
//   Vowel Morph   talkbox-like vowel imposition on ANY source: a parallel bank of five resonators whose centre
//                 frequencies, bandwidths and levels morph continuously through A-E-I-O-U (published average formant
//                 data for an adult voice, interpolated in log-frequency and dB). The vowel is moved by hand, by an
//                 LFO, by the input's loudness or by a tempo-synced vowel sequence. Formant (st) shifts the whole
//                 vocal tract up or down. Distinct from Formant (which moves the input's OWN spectral envelope) and
//                 from the Vocoder (which needs a carrier): here the input itself is filtered into vowels.
//   Whisper       turns a sung or spoken voice into breath: STFT phase randomisation removes the periodic (pitched)
//                 part, spectral-envelope smoothing removes the harmonic comb, so what is left is noise shaped by the
//                 voice's own vowels and consonants. Transient (consonant) frames can keep their original phase,
//                 so the words stay intelligible. Frame energy is preserved, and the overlap-add level loss of
//                 incoherent frames is compensated analytically.
//   Vocal Throw   the adlib "throw": an echo that ducks under the dry voice and lives in the gaps, band-limited like
//                 a send through a filter, with an optional pitch drop on every repeat. In Last Word mode it listens
//                 for the end of each phrase and throws only the last word(s) into the echo.
//
// Stability: Vowel Morph has no recursion (its resonators are feed-forward filters; the adaptive make-up gain is
// capped at +18 dB and frozen during silence). Whisper is feed-forward and energy-preserving per frame. Vocal
// Throw's loop is feedback (<= 90 %) x band-pass (unity peak) x pitch shifter (two Hann-windowed taps whose gains
// sum to 1), followed by a soft clipper: its small-signal loop gain is < 1 at every frequency.
#include <algorithm>
#include <array>
#include <cmath>

#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Fft.h"
#include "ih1/dsp/Spectral.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

inline float reflect04(float x) {  // fold into [0, 4] (a mirror, so modulation never jumps from U back to A)
    x = std::fmod(std::fabs(x), 8.0f);
    return x > 4.0f ? 8.0f - x : x;
}
inline float levelDb(float rms) { return 20.0f * std::log10(std::max(rms, 1e-9f)); }

// ============================================================================================== Vowel Morph
enum { vVowel, vMotion, vRate, vSync, vDivision, vDepth, vPattern, vFormant, vResonance, vDrive, vSpread, vOut, vMix };

// Average formants of an adult (tenor) voice: centre Hz, relative level dB, bandwidth Hz (widely published
// reference values, e.g. the Csound manual's formant table). Order A E I O U.
struct Vowel { float f[5], a[5], b[5]; };
constexpr Vowel kVowels[5] = {
    {{650, 1080, 2650, 2900, 3250}, {0, -6, -7, -8, -22}, {80, 90, 120, 130, 140}},     // A
    {{400, 1700, 2600, 3200, 3580}, {0, -14, -12, -14, -20}, {70, 80, 100, 120, 120}},  // E
    {{290, 1870, 2800, 3250, 3540}, {0, -15, -18, -20, -30}, {40, 90, 100, 120, 120}},  // I
    {{400, 800, 2600, 2800, 3000}, {0, -10, -12, -12, -26}, {40, 80, 100, 120, 120}},   // O
    {{350, 600, 2700, 2900, 3300}, {0, -20, -17, -14, -26}, {40, 60, 100, 120, 120}},   // U
};
// Sequence patterns (vowel indices, 4 steps each)
constexpr int kPatterns[6][4] = {{0, 1, 2, 3}, {0, 3, 0, 4}, {2, 0, 4, 1}, {4, 3, 0, 1}, {0, 2, 0, 2}, {1, 4, 2, 3}};

class VowelMorph final : public FxBase {
    static constexpr int K = 5, kCtl = 16;
    void onPrepare() override {
        const float ctlRate = srf() / kCtl;
        for (auto& p : pos_) { p.prepare(ctlRate, 25.0f); }
        follow_.prepare(sr_, 4.0f, 140.0f);
        inRms_.prepare(sr_, 300.0f);
        outRms_.prepare(sr_, 300.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        out_.prepare(sr_, 30.0f);
        for (auto* sm : {&drivePre_, &drivePost_, &driveOn_}) sm->prepare(sr_, 10.0f);
        for (auto& d : dc_) d.prepare(sr_);
    }
    void reset() override {
        for (auto& c : bank_) for (auto& f : c) f.reset();
        for (auto& d : dc_) d.reset();
        follow_.reset();
        inRms_.snap(0.0f);
        outRms_.snap(0.0f);
        makeup_ = 1.0f;
        phase_ = 0.0;
        step_ = 0;
        ctl_ = 0;
        const float v = reflect04(fp(vVowel));
        pos_[0].snap(v);
        pos_[1].snap(reflect04(v + pct(vSpread)));
        updateFilters(true);
        drivePre_.snap(1.0f + 8.0f * pct(vDrive));
        drivePost_.snap(0.5f / std::tanh((1.0f + 8.0f * pct(vDrive)) * 0.5f));
        driveOn_.snap(pct(vDrive) > 0.0f ? 1.0f : 0.0f);
        mix_.reset();
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(vMix), snap);
        out_.setTarget(dsp::dbToGain(fp(vOut)));
        if (snap) out_.snap(out_.target());
    }
    double tailSeconds() const override { return 0.2; }

    // centre / level / bandwidth of resonator k at vowel position p (0..4)
    void formantAt(float p, int k, float& f, float& a, float& b) const {
        const int i = std::min(3, static_cast<int>(p));
        const float t = std::clamp(p - static_cast<float>(i), 0.0f, 1.0f);
        const Vowel& v0 = kVowels[i];
        const Vowel& v1 = kVowels[i + 1];
        f = std::exp(std::log(v0.f[k]) + t * (std::log(v1.f[k]) - std::log(v0.f[k])));
        a = v0.a[k] + t * (v1.a[k] - v0.a[k]);
        b = v0.b[k] + t * (v1.b[k] - v0.b[k]);
    }
    void updateFilters(bool snap) {
        const float shift = dsp::semitonesToRatio(fp(vFormant));
        const float bwScale = 1.6f - 1.3f * pct(vResonance);  // more resonance = narrower formants
        for (int c = 0; c < 2; ++c) {
            const float p = pos_[c].current();
            for (int k = 0; k < K; ++k) {
                float f, a, b;
                formantAt(p, k, f, a, b);
                f = std::min(f * shift, 0.45f * srf());
                const float q = std::clamp(f / std::max(20.0f, b * bwScale * std::max(0.6f, shift)), 0.7f, 40.0f);
                bank_[c][k].setup(dsp::Svf::Bandpass, f, q, 0.0f, sr_);
                // unity peak (the band-pass peak gain is Q). The gain ramps linearly to its new value over the next
                // control tick: stepped every 16 samples it would put small stairs on the output (ih1_click_audit)
                const float g = dsp::dbToGain(a) / q;
                if (snap) { gain_[c][k] = g; gainStep_[c][k] = 0.0f; }
                else gainStep_[c][k] = (g - gain_[c][k]) / static_cast<float>(kCtl);
            }
        }
    }
    float target(float base) {
        const float depth = pct(vDepth);
        switch (ip(vMotion)) {
            case 1: return base + 2.0f * depth * std::sin(dsp::kTwoPi * static_cast<float>(phase_));      // LFO
            case 2: {                                                                                       // loudness
                const float n = std::clamp((levelDb(follow_.value()) + 48.0f) / 36.0f, 0.0f, 1.0f);
                return base + 4.0f * depth * n;
            }
            case 3: {                                                                                       // sequence
                const int pat = std::clamp(ip(vPattern), 0, 5);
                const float v = static_cast<float>(kPatterns[pat][step_ & 3]);
                return base + depth * (v - base);  // depth 100 % = exactly the pattern's vowel
            }
            default: return base;
        }
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const double rate = bp(vSync) ? 1.0 / std::max(1e-3, syncedSeconds(true, 0.0f, ip(vDivision), ctx.bpm)) : static_cast<double>(fp(vRate));
        const float drive = pct(vDrive);
        drivePre_.setTarget(1.0f + 8.0f * drive);
        drivePost_.setTarget(0.5f / std::tanh((1.0f + 8.0f * drive) * 0.5f));
        driveOn_.setTarget(drive > 0.0f ? 1.0f : 0.0f);
        for (int i = 0; i < n; ++i) {
            const float xl = ch[0][i], xr = ch[1][i];
            follow_.process(0.5f * (xl + xr));
            if (ctl_ == 0) {
                phase_ += rate * kCtl / sr_;
                if (phase_ >= 1.0) { phase_ -= std::floor(phase_); ++step_; }
                const float base = reflect04(fp(vVowel));
                const float t = reflect04(target(base));
                pos_[0].setTarget(t);
                pos_[1].setTarget(reflect04(t + pct(vSpread)));
                pos_[0].next();
                pos_[1].next();
                updateFilters(false);
            }
            ctl_ = (ctl_ + 1) % kCtl;
            float y[2] = {0.0f, 0.0f};
            const float pre = drivePre_.next(), post = drivePost_.next(), on = driveOn_.next();
            for (int c = 0; c < 2; ++c) {
                float x = c ? xr : xl;
                if (on > 0.0f) x += on * (std::tanh(x * pre) * post - x);
                float s = 0.0f;
                for (int k = 0; k < K; ++k) {
                    gain_[c][k] += gainStep_[c][k];
                    s += gain_[c][k] * bank_[c][k].process(x);
                }
                y[c] = dc_[c].process(s);
            }
            // adaptive make-up: the vowel bank passes a fraction of a broadband input; match the input level slowly
            // (capped at +18 dB, frozen while the input is silent so it never pumps noise up)
            const float in2 = 0.5f * (xl * xl + xr * xr), out2 = 0.5f * (y[0] * y[0] + y[1] * y[1]);
            inRms_.setTarget(in2);
            outRms_.setTarget(out2);
            const float ia = inRms_.next(), oa = outRms_.next();
            if (ia > 1e-8f && oa > 1e-12f) makeup_ = std::clamp(std::sqrt(ia / oa), 0.25f, 8.0f);
            const float g = makeup_ * out_.next();
            ch[0][i] = y[0] * g;
            ch[1][i] = y[1] * g;
        }
        for (int c = 0; c < 2; ++c) for (auto& f : bank_[c]) f.flush();
        mix_.apply(ch, n);
    }

    std::array<std::array<dsp::Svf, K>, 2> bank_{};
    std::array<std::array<float, K>, 2> gain_{}, gainStep_{};
    dsp::Smoother drivePre_, drivePost_, driveOn_;
    std::array<dsp::Smoother, 2> pos_{};
    std::array<dsp::DcBlocker, 2> dc_{};
    dsp::EnvelopeFollower follow_;
    dsp::Smoother inRms_, outRms_, out_;
    float makeup_ = 1.0f;
    double phase_ = 0.0;
    int step_ = 0, ctl_ = 0;
    MixStage mix_;
};

// ================================================================================================== Whisper
enum { wWhisper, wSmooth, wConsonants, wAir, wLowCut, wOut, wMix };

class Whisper final : public FxBase {
    void onPrepare() override {
        // Low latency: 512-point frames (~11 ms at 48 kHz) instead of 1024 (~21 ms)
        n_ = dsp::stftSizeFor(sr_, latMode_ == LatencyMode::Quality ? 1024 : 512);
        hop_ = n_ / 4;
        const size_t B = static_cast<size_t>(n_ / 2 + 1);
        for (int c = 0; c < 2; ++c) {
            stft_[c].prepare(n_, hop_);
            mag_[c].assign(B, 0.0f);
            env_[c].assign(B, 0.0f);
            cum_[c].assign(B + 1, 0.0);
            rng_[c].seed_(0x5A15u + static_cast<uint32_t>(c) * 977u);
        }
        mix_.prepare(sr_, maxBlock_, n_, MixLaw::Linear);
        out_.prepare(sr_, 30.0f);
        inP_.prepare(sr_, 250.0f);
        outP_.prepare(sr_, 250.0f);
    }
    int latencySamples() const override { return n_; }
    double tailSeconds() const override { return static_cast<double>(n_) / sr_; }
    void reset() override {
        for (int c = 0; c < 2; ++c) { stft_[c].reset(); prevE_[c] = 0.0f; rng_[c].seed_(0x5A15u + static_cast<uint32_t>(c) * 977u); }
        inP_.snap(0.0f);
        outP_.snap(0.0f);
        makeup_ = 1.0f;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(wMix), snap);
        out_.setTarget(dsp::dbToGain(fp(wOut)));
        if (snap) out_.snap(out_.target());
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float amount = pct(wWhisper), smooth = pct(wSmooth), keep = pct(wConsonants);
        const float airDb = fp(wAir), lowCut = fp(wLowCut);
        for (int c = 0; c < 2; ++c) {
            stft_[c].process(ch[c], n, [&](dsp::cfloat* X, int N) { frame(c, X, N, amount, smooth, keep, airDb, lowCut); });
        }
        // level match: output power follows the (latency-aligned) input power; capped at +8 dB, frozen in silence
        const float* d0 = mix_.dry(0);
        const float* d1 = mix_.dry(1);
        for (int i = 0; i < n; ++i) {
            inP_.setTarget(0.5f * (d0[i] * d0[i] + d1[i] * d1[i]));
            outP_.setTarget(0.5f * (ch[0][i] * ch[0][i] + ch[1][i] * ch[1][i]));
            const float ip = inP_.next(), op = outP_.next();
            if (ip > 1e-6f && op > 1e-9f) makeup_ = std::clamp(std::sqrt(ip / op), 0.4f, 2.5f);  // frozen below -60 dB
            const float g = out_.next() * makeup_;
            ch[0][i] *= g;
            ch[1][i] *= g;
        }
        mix_.apply(ch, n);
    }
    void frame(int c, dsp::cfloat* X, int N, float amount, float smooth, float keep, float airDb, float lowCut) {
        const int B = N / 2 + 1;
        auto& mag = mag_[c];
        auto& env = env_[c];
        auto& cum = cum_[c];
        double e = 0.0;
        cum[0] = 0.0;
        for (int k = 0; k < B; ++k) {
            mag[static_cast<size_t>(k)] = std::abs(X[k]);
            e += static_cast<double>(mag[static_cast<size_t>(k)]) * mag[static_cast<size_t>(k)];
            cum[static_cast<size_t>(k + 1)] = cum[static_cast<size_t>(k)] + mag[static_cast<size_t>(k)];
        }
        // consonant / onset frame: energy jumps by more than ~6 dB over the running level
        const float ef = static_cast<float>(e);
        const bool onset = ef > 4.0f * prevE_[c] + 1e-10f;
        prevE_[c] = 0.7f * prevE_[c] + 0.3f * ef;
        const float a = amount * (onset ? (1.0f - keep) : 1.0f);  // phase randomisation for this frame
        const float s = smooth * (onset ? (1.0f - keep) : 1.0f);   // envelope smoothing for this frame
        // spectral envelope: moving average over +-w bins (w grows with Smooth; about one harmonic spacing of a low
        // voice at 100 %), so the harmonic comb disappears and only the vowel shape is left
        const int w = std::max(1, static_cast<int>(std::lround(smooth * 10.0f * static_cast<float>(N) / 1024.0f)));
        double e2 = 0.0;
        for (int k = 0; k < B; ++k) {
            const int lo = std::max(0, k - w), hi = std::min(B - 1, k + w);
            const float avg = static_cast<float>((cum[static_cast<size_t>(hi + 1)] - cum[static_cast<size_t>(lo)]) / (hi - lo + 1));
            env[static_cast<size_t>(k)] = mag[static_cast<size_t>(k)] + s * (avg - mag[static_cast<size_t>(k)]);
            e2 += static_cast<double>(env[static_cast<size_t>(k)]) * env[static_cast<size_t>(k)];
        }
        // keep the frame's energy (smoothing a peaky spectrum loses power). The overlap-add of incoherent (random-phase)
        // frames loses level too: for a stationary input, four Hann frames at 75 % overlap add in power, 0.25 of the
        // coherent power (-6 dB). That figure assumes equally loud frames; at onsets it over-compensates (V6 audit:
        // +3 dB and 2x peaks on speech and snares), so the loss is corrected in the time domain instead, by matching
        // the output's smoothed power to the latency-aligned input's (processBlock).
        const float energyFix = e2 > 1e-20 ? static_cast<float>(std::sqrt(e / e2)) : 1.0f;
        const float binHz = srf() / static_cast<float>(N);
        for (int k = 1; k < B - 1; ++k) {
            const float f = static_cast<float>(k) * binHz;
            float g = energyFix;
            if (f > 2000.0f) g *= dsp::dbToGain(airDb * std::min(2.0f, std::log2(f / 2000.0f)));  // air above 2 kHz
            const float r = f / std::max(20.0f, lowCut);
            g *= (r * r) / std::sqrt(1.0f + r * r * r * r);                                        // 2nd-order low cut
            const float ph = std::arg(X[k]) + a * dsp::kPi * rng_[c].bipolar();
            X[k] = std::polar(env[static_cast<size_t>(k)] * g, ph);
        }
        X[0] = 0.0f;
        X[B - 1] = 0.0f;
        dsp::Stft::mirror(X, N);
    }

    int n_ = 1024, hop_ = 256;
    dsp::Stft stft_[2];
    std::vector<float> mag_[2], env_[2];
    std::vector<double> cum_[2];
    float prevE_[2]{};
    dsp::Rng rng_[2];
    MixStage mix_;
    dsp::Smoother out_, inP_, outP_;
    float makeup_ = 1.0f;
};

// ============================================================================================== Vocal Throw
enum { tMode, tTime, tSync, tDivision, tFeedback, tDrop, tTone, tFocus, tDuck, tPingPong, tCatch, tThreshold, tMix };

// Delay-line pitch shifter (two Hann-windowed taps half a window apart; their gains sum to exactly 1).
class TapShifter {
public:
    void prepare(double sr) { win_ = static_cast<float>(0.05 * sr); line_.prepare(static_cast<int>(win_) + 16); }
    void reset() { line_.reset(); ph_ = 0.0f; }
    float latency() const { return 0.5f * win_ + 2.0f; }
    float process(float x, float ratio) {
        line_.push(x);
        ph_ += (1.0f - ratio) / win_;
        ph_ -= std::floor(ph_);
        const float p2 = ph_ + 0.5f - std::floor(ph_ + 0.5f);
        const float g1 = std::sin(dsp::kPi * ph_), g2 = std::sin(dsp::kPi * p2);
        return g1 * g1 * line_.readLinear(2.0f + ph_ * win_) + g2 * g2 * line_.readLinear(2.0f + p2 * win_);
    }

private:
    dsp::DelayLine line_;
    float win_ = 2400.0f, ph_ = 0.0f;
};

class VocalThrow final : public FxBase {
    void onPrepare() override {
        const int maxD = static_cast<int>(2.2 * sr_) + 64;
        for (int c = 0; c < 2; ++c) {
            line_[c].prepare(maxD);
            shift_[c].prepare(sr_);
            capture_[c].prepare(static_cast<int>(1.3 * sr_) + 16);
        }
        time_.prepare(sr_, 80.0f);
        xfInc_ = 1.0f / (0.04f * srf());
        shiftMix_.prepare(sr_, 40.0f);
        bpGain_.prepare(sr_, 8.0f);
        dryEnv_.prepare(sr_, 5.0f, 250.0f);
        gate_.prepare(sr_, 2.0f, 60.0f);
        duck_.prepare(sr_, 20.0f);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
    }
    void reset() override {
        for (int c = 0; c < 2; ++c) { line_[c].reset(); shift_[c].reset(); capture_[c].reset(); bp_[c].reset(); hp_[c].reset(); bp2_[c].reset(); }
        dryEnv_.reset();
        gate_.reset();
        duck_.snap(1.0f);
        shiftMix_.snap(std::fabs(fp(tDrop)) > 0.02f ? 1.0f : 0.0f);
        active_ = false;
        quiet_ = 0;
        inject_ = 0;
        timeSnapped_ = false;  // the first block jumps to the repeat time instead of gliding up from 0
        mix_.reset();
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(tMix), snap);
        const float f = fp(tTone);
        const float q = 0.5f + 2.5f * pct(tFocus);
        for (int c = 0; c < 2; ++c) {
            bp_[c].setup(dsp::Svf::Bandpass, f, q, 0.0f, sr_);
            bp2_[c].setup(dsp::Svf::Bandpass, f, q, 0.0f, sr_);
            hp_[c].setup(dsp::Svf::Highpass, 120.0f, 0.7f, 0.0f, sr_);
        }
        // the band-pass normalisation (1 / Q) glides per sample: recomputed only per glide sub-block it would step the
        // echo level in ~2 % stairs when Tone and Focus move together (found by ih1_click_audit, case 17)
        bpGain_.setTarget(1.0f / q);
        if (snap) bpGain_.snap(bpGain_.target());
        shiftMix_.setTarget(std::fabs(fp(tDrop)) > 0.02f ? 1.0f : 0.0f);
        if (snap) shiftMix_.snap(shiftMix_.target());
    }
    double tailSeconds() const override {
        const double fb = std::min(0.9, static_cast<double>(p_[tFeedback]) * 0.01 * 0.9);
        const double t = static_cast<double>(std::max(40.0f, p_[tTime])) * 0.001;
        return fb > 1e-3 ? std::min(30.0, t * std::log(1e-3) / std::log(fb) + 1.4) : t + 1.4;
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const float shiftLat = shift_[0].latency();
        // V6 (ih1_click_audit): the repeat time is decided on the parameter's FINAL value (FxBase glides continuous
        // parameters in 32-sample steps, which must not be mistaken for many small moves). A small change glides (a
        // gentle, tape-like bend); a large one crossfades between two read heads instead of sweeping one head through
        // hundreds of milliseconds of history at several times speed. A new destination waits for a running crossfade.
        const double finalSecs = syncedSeconds(bp(tSync), target_[tTime], ip(tDivision), ctx.bpm);
        const float want = std::clamp(static_cast<float>(finalSecs * sr_), shiftLat + 8.0f, 2.1f * srf());
        if (!timeSnapped_) { time_.snap(want); timeSnapped_ = true; xf_ = 1.0f; }
        if (xf_ >= 1.0f && want != time_.target()) {
            const bool large = std::fabs(want - time_.current()) > std::max(0.01f * srf(), 0.05f * time_.current());
            if (large) { tNew_ = want; xf_ = 0.0f; time_.snap(time_.current()); }
            else time_.setTarget(want);
        }
        const float fb = std::min(0.9f, pct(tFeedback) * 0.9f);
        const float ratio = dsp::semitonesToRatio(fp(tDrop));
        const bool lastWord = ip(tMode) == 1;
        const bool pingPong = bp(tPingPong);
        const float duckAmt = pct(tDuck);
        const float thr = dsp::dbToGain(fp(tThreshold));
        const int catchN = std::clamp(static_cast<int>(fp(tCatch) * 0.001f * srf()), 64, static_cast<int>(1.2 * sr_));
        const int holdN = static_cast<int>(0.08 * sr_);
        for (int i = 0; i < n; ++i) {
            const float xl = ch[0][i], xr = ch[1][i];
            const float mono = 0.5f * (xl + xr);
            capture_[0].push(xl);
            capture_[1].push(xr);
            // --- send: everything (Echo), or only the end of each phrase (Last Word)
            float sl = xl, sr = xr;
            if (lastWord) {
                const float g = gate_.process(mono);
                if (g > thr) { active_ = true; quiet_ = 0; }
                else if (active_ && ++quiet_ >= holdN) {  // the phrase has ended: throw its last catchN samples
                    active_ = false;
                    inject_ = catchN;
                    injectLen_ = catchN;
                }
                sl = sr = 0.0f;
                if (inject_ > 0) {
                    const int d = holdN + inject_;  // reads the captured audio forward, from its start to the phrase end
                    const float u = 1.0f - static_cast<float>(inject_) / static_cast<float>(injectLen_);
                    const float edge = std::min({1.0f, u * injectLen_ / (0.01f * srf()), (1.0f - u) * injectLen_ / (0.01f * srf())});
                    sl = capture_[0].tap(d) * edge;
                    sr = capture_[1].tap(d) * edge;
                    --inject_;
                }
            }
            // --- echo loop: delay -> (pitch drop) -> tone -> feedback
            const float T = time_.next();
            const float bg = bpGain_.next();
            const float sm = shiftMix_.next();
            float yl = line_[0].readCubic(T - sm * shiftLat), yr = line_[1].readCubic(T - sm * shiftLat);
            if (xf_ < 1.0f) {
                xf_ = std::min(1.0f, xf_ + xfInc_);
                const float a = std::cos(0.5f * dsp::kPi * xf_), b = std::sin(0.5f * dsp::kPi * xf_);
                yl = a * yl + b * line_[0].readCubic(tNew_ - sm * shiftLat);
                yr = a * yr + b * line_[1].readCubic(tNew_ - sm * shiftLat);
                if (xf_ >= 1.0f) time_.snap(tNew_);
            }
            auto loop = [&](int c, float y) {
                float v = y;
                if (sm > 0.0f) v = (1.0f - sm) * y + sm * shift_[c].process(y, ratio);
                else shift_[c].process(y, 1.0f);  // keep the shifter's line current, so engaging it is seamless
                v = bg * bp_[c].process(hp_[c].process(v));
                v *= fb;
                return v / (1.0f + std::fabs(v) * 0.25f);  // soft ceiling: the loop can never build up
            };
            const float fl = loop(0, yl), fr = loop(1, yr);
            if (pingPong) {
                line_[0].push(0.5f * (sl + sr) + fr);
                line_[1].push(fl);
            } else {
                line_[0].push(sl + fl);
                line_[1].push(sr + fr);
            }
            // --- ducking: the echoes step back while the voice sings and come forward in the gaps
            const float lv = std::clamp((levelDb(dryEnv_.process(mono)) + 50.0f) / 30.0f, 0.0f, 1.0f);
            duck_.setTarget(1.0f - duckAmt * lv);
            const float dg = duck_.next();
            // wet: the echo returns only (no dry); the first repeat is band-limited the same way as the rest
            ch[0][i] = bg * bp2_[0].process(yl) * dg;
            ch[1][i] = bg * bp2_[1].process(yr) * dg;
        }
        for (int c = 0; c < 2; ++c) { bp_[c].flush(); hp_[c].flush(); bp2_[c].flush(); }
        mix_.apply(ch, n);
    }

    dsp::DelayLine line_[2], capture_[2];
    TapShifter shift_[2];
    dsp::Svf bp_[2], hp_[2], bp2_[2];
    dsp::Smoother time_, shiftMix_, duck_, bpGain_;
    dsp::EnvelopeFollower dryEnv_, gate_;
    bool active_ = false;
    int quiet_ = 0, inject_ = 0, injectLen_ = 1;
    bool timeSnapped_ = false;
    float xf_ = 1.0f, xfInc_ = 0.0005f, tNew_ = 0.0f;
    MixStage mix_;
};

}  // namespace

FxDescriptor describeVowelMorph() {
    FxDescriptor d;
    d.typeId = "vowel_morph";
    d.name = "Vowel Morph";
    d.family = FxFamily::PitchVoice;
    d.tooltip = "Talkbox-style vowels on any sound: a bank of vocal-tract resonances glides through A, E, I, O and U by hand, with an LFO, with your loudness or as a synced vowel sequence.";
    d.params = {
        P::flt("vowel", "Vowel", "", 0.0f, 4.0f, 0.0f, "Where the mouth shape sits: 0 = A, 1 = E, 2 = I, 3 = O, 4 = U (in between = a blend)."),
        P::choice("motion", "Motion", {"Manual", "LFO", "Loudness", "Sequence"}, 1, "What moves the vowel: nothing, a slow wobble, how loud you sing, or a tempo-synced vowel pattern."),
        P::logf("rate", "Rate", "Hz", 0.05f, 12.0f, 0.8f, "Speed of the LFO or the sequence."),
        P::toggle("sync", "Sync", false, "Lock the LFO / sequence to the song tempo."),
        P::choice("division", "Division", divisionLabels(), 7, "Step length (sequence) or cycle (LFO) when Sync is on."),
        P::pct("depth", "Depth", 60.0f, "How far the motion moves the vowel. In Sequence mode 100 % = exactly the pattern's vowels."),
        P::choice("pattern", "Pattern", {"A E I O", "A O A U", "I A U E", "U O A E", "A I A I", "E U I O"}, 0, "The vowel sequence (Sequence motion)."),
        P::flt("formant", "Formant", "st", -12.0f, 12.0f, 0.0f, "Moves the whole mouth up or down: lower = a bigger, darker voice; higher = smaller and brighter."),
        P::pct("resonance", "Resonance", 50.0f, "How pronounced the vowels are: higher = narrower, more talkbox-like peaks."),
        P::pct("drive", "Drive", 15.0f, "Saturates the sound before the vowels, for a gritty talkbox edge (brings out more harmonics to shape)."),
        P::pct("spread", "Stereo Spread", 0.0f, "Shifts the right channel's vowel against the left for a wide, talking stereo image."),
        P::adv(P::flt("out", "Output", "dB", -12.0f, 12.0f, 0.0f, "Level after the vowels (the effect already matches its level to the input).")),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and the vowel-shaped sound."),
    };
    d.create = [] { return std::make_unique<VowelMorph>(); };
    return d;
}

FxDescriptor describeWhisper() {
    FxDescriptor d;
    d.typeId = "whisper";
    d.name = "Whisper";
    d.family = FxFamily::PitchVoice;
    d.tooltip = "Turns singing or speech into breath: the pitch is removed and only the voice's vowel shape and consonants remain, from a breathy edge to a full whisper.";
    d.params = {
        P::pct("whisper", "Whisper", 100.0f, "How much of the pitched voice becomes breath (phase randomisation)."),
        P::pct("smooth", "Smooth", 70.0f, "Removes the harmonic comb of the pitch: higher = a purer, airier whisper; lower = a rough, raspy one."),
        P::pct("consonants", "Consonants", 60.0f, "Keeps consonant attacks (t, k, s...) as they were, so the words stay clear."),
        P::flt("air", "Air", "dB", 0.0f, 12.0f, 3.0f, "Lifts the breathy top end above 2 kHz."),
        P::logf("low_cut", "Low Cut", "Hz", 40.0f, 1000.0f, 160.0f, "A whisper has no low body: removes rumble below this frequency."),
        P::adv(P::flt("out", "Output", "dB", -12.0f, 12.0f, 0.0f, "Level after the effect (it keeps the input's energy).")),
        P::pct("mix", "Mix", 100.0f, "Blend of the sung voice and its whisper (50 % = a breathy double)."),
    };
    d.create = [] { return std::make_unique<Whisper>(); };
    return d;
}

FxDescriptor describeVocalThrow() {
    FxDescriptor d;
    d.typeId = "vocal_throw";
    d.name = "Vocal Throw";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "The adlib throw: band-limited echoes that duck under the voice and bloom in the gaps, with an optional pitch drop on every repeat. Last Word mode throws only the end of each phrase.";
    d.params = {
        P::choice("mode", "Mode", {"Echo", "Last Word"}, 0, "Echo: everything is sent. Last Word: only the end of each phrase is caught and thrown."),
        P::logf("time_ms", "Time", "ms", 40.0f, 2000.0f, 375.0f, "Time between repeats."),
        P::toggle("sync", "Sync", true, "Lock the repeats to the song tempo."),
        P::choice("division", "Division", divisionLabels(), 10, "Repeat time as a note length when Sync is on."),
        P::pct("feedback", "Feedback", 45.0f, "How many repeats (stays bounded at 100 %)."),
        P::flt("drop", "Pitch per Repeat", "st", -12.0f, 12.0f, 0.0f, "Each repeat is pitched by this much: negative = falling throws, positive = rising."),
        P::logf("tone", "Tone", "Hz", 300.0f, 8000.0f, 1800.0f, "Centre of the echo's band: low = dark and distant, high = thin and telephone-like."),
        P::pct("focus", "Focus", 35.0f, "How narrow the echo's band is (0 % = full range, 100 % = a narrow radio band)."),
        P::pct("duck", "Duck", 65.0f, "How far the echoes step back while the voice is singing, so they fill the gaps instead of the words."),
        P::toggle("ping_pong", "Ping-Pong", true, "Repeats bounce between left and right."),
        P::adv(P::logf("catch_ms", "Catch", "ms", 80.0f, 1000.0f, 350.0f, "Last Word: how much of the phrase end is thrown.")),
        P::adv(P::flt("threshold", "Sensitivity", "dB", -60.0f, -20.0f, -40.0f, "Last Word: level below which the phrase counts as ended.")),
        P::pct("mix", "Mix", 50.0f, "Echo level against the voice (50 % = both at full level)."),
    };
    d.create = [] { return std::make_unique<VocalThrow>(); };
    return d;
}

}  // namespace ih1::fx
