// Batch C: Frequency Shifter, Doppler, Looper, Resonator, Convolution, Amp/Cabinet.
// KADAL: brief §5.15 (SSB frequency shift via analytic signal), KNW-SA0JJFX4HE (Doppler = moving delay,
// pitch = 1 - dT/dt), §5.14 + decision C3 (looper state = parameters only), KNW-1JA0N4393Q (tuned
// comb / Karplus-Strong resonators), KNW-Y5RC0H2E2M + §5.17 (partitioned FFT convolution, IRs prepared
// off the audio thread), KNW-MQRRKX0DSE / KNW-DGDSZTCAGP (tube-style stages), KNW-MCXD9ZQP83 (ADAA + 2x).
#include <array>
#include <climits>

#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Fft.h"
#include "ih1/dsp/Nonlinear.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

constexpr double kPiD = 3.14159265358979323846;

// =============================================================================================== Frequency Shifter
// 90-degree phase-difference network: two cascades of 2nd-order all-pass sections (coefficients from
// Olli Niemitalo's widely published IIR Hilbert design, ~0.05-0.95 Nyquist).
class Hilbert {
public:
    void reset() { for (auto& s : st_) s = State{}; delayed_ = 0.0f; }
    void process(float x, float& i, float& q) {
        static constexpr float A[4] = {0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f};
        static constexpr float B[4] = {0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f};
        float a = x, b = x;
        for (int k = 0; k < 4; ++k) {
            a = section(st_[k], A[k] * A[k], a);
            b = section(st_[4 + k], B[k] * B[k], b);
        }
        i = delayed_;  // path A delayed one sample: A(z) z^-1 and B(z) differ by 90 deg +-0.7 deg, 50 Hz..20 kHz
        q = b;
        delayed_ = a;
    }

private:
    struct State { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    static float section(State& s, float c, float x) {
        const float y = c * (x + s.y2) - s.x2;
        s.x2 = s.x1; s.x1 = x; s.y2 = s.y1; s.y1 = dsp::flushTiny(y);
        return y;
    }
    State st_[8];
    float delayed_ = 0.0f;
};

enum { fsShift, fsFine, fsStereo, fsFeedback, fsDelay, fsMix };
class FreqShifter final : public FxBase {
    void onPrepare() override {
        for (auto& l : fbLine_) l.prepare(static_cast<int>(1.1 * sr_));
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        shiftS_.prepare(sr_, 40.0f);
    }
    void reset() override { for (auto& h : hil_) h.reset(); for (auto& l : fbLine_) l.reset(); ph_[0] = ph_[1] = 0.0; mix_.reset(); }
    void applyParams(bool snap) override { shiftS_.setTarget(fp(fsShift) + fp(fsFine)); if (snap) shiftS_.snap(shiftS_.target()); mix_.setMix(pct(fsMix), snap); }
    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float fb = pct(fsFeedback) * 0.85f;
        const float dly = std::max(1.0f, fp(fsDelay) * 0.001f * srf());
        for (int i = 0; i < n; ++i) {
            const float s = shiftS_.next();
            for (int c = 0; c < 2; ++c) {
                const double hz = s + (c ? fp(fsStereo) : 0.0f);
                float I, Q;
                hil_[c].process(ch[c][i] + fb * fbLine_[c].readCubic(dly), I, Q);
                const float co = std::cos(static_cast<float>(ph_[c]) * dsp::kTwoPi), si = std::sin(static_cast<float>(ph_[c]) * dsp::kTwoPi);
                const float y = I * co + Q * si;  // upper sideband: every component moves by +hz
                fbLine_[c].push(2.0f * std::tanh(0.5f * y));
                ch[c][i] = y;
                ph_[c] += hz / sr_;
                ph_[c] -= std::floor(ph_[c]);
            }
        }
        mix_.apply(ch, n);
    }
    Hilbert hil_[2];
    dsp::DelayLine fbLine_[2];
    double ph_[2] = {0, 0};
    dsp::Smoother shiftS_;
    MixStage mix_;
};

// =============================================================================================== Doppler
enum { dpPath, dpSpeed, dpDistance, dpPeriod, dpSync, dpDiv, dpMix };
class Doppler final : public FxBase {
    void onPrepare() override { line_.prepare(static_cast<int>(0.6 * sr_)); mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear); }
    void reset() override { line_.reset(); t_ = 0.0; mix_.reset(); }
    void applyParams(bool snap) override { mix_.setMix(pct(dpMix), snap); }
    void process(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const int path = ip(dpPath);  // 0 pass-by L->R, 1 pass-by R->L, 2 circle
        const double v = fp(dpSpeed), d0 = fp(dpDistance), c = 343.0;
        const double period = bp(dpSync) ? dsp::divisionSeconds(ip(dpDiv), ctx.bpm) : fp(dpPeriod);
        const double maxDelay = 0.55 * sr_;
        for (int i = 0; i < n; ++i) {
            line_.push(0.5f * (ch[0][i] + ch[1][i]));
            const double ph = std::fmod(t_ / period, 1.0);
            double x, y;
            if (path == 2) { const double a = ph * 2.0 * kPiD; const double R = std::max(d0, v * period / (2.0 * kPiD)); x = R * std::cos(a); y = d0 + R * (1.0 + std::sin(a)) * 0.5; }
            else { x = (path == 0 ? 1.0 : -1.0) * v * (ph - 0.5) * period; y = d0; }
            const double r = std::sqrt(x * x + y * y);
            const double delay = std::min(maxDelay, r / c * sr_);
            const float g = static_cast<float>(d0 / std::max(r, 1.0));  // inverse distance, unity at closest approach
            // a smooth fade at the wrap of each pass hides the jump back to the start of the path
            const float edge = path == 2 ? 1.0f : static_cast<float>(std::min(1.0, std::min(ph, 1.0 - ph) * 20.0));
            float gl, gr;
            dsp::panGains(static_cast<float>(x / std::max(r, 1e-6)), gl, gr);
            const float s = line_.readCubic(static_cast<float>(std::max(2.0, delay))) * g * edge;
            ch[0][i] = s * gl * 1.41f;
            ch[1][i] = s * gr * 1.41f;
            t_ += 1.0 / sr_;
        }
        mix_.apply(ch, n);
    }
    dsp::DelayLine line_;
    double t_ = 0.0;
    MixStage mix_;
};

// =============================================================================================== Looper (buffer hold)
enum { lpLength, lpCapture, lpOverdub, lpSpeed, lpReverse, lpFade, lpMix };
constexpr double kLoopBeats[5] = {1.0, 2.0, 4.0, 8.0, 16.0};
class Looper final : public FxBase {
    static constexpr double kMaxSeconds = 32.0;
    void onPrepare() override {
        int size = 1;
        while (size < static_cast<int>(kMaxSeconds * sr_)) size <<= 1;
        for (auto& b : buf_) b.assign(static_cast<size_t>(size), 0.0f);
        mask_ = size - 1;
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
    }
    void reset() override {
        for (auto& b : buf_) std::fill(b.begin(), b.end(), 0.0f);
        for (auto& pk : peaks_) pk.store(0.0f, std::memory_order_relaxed);
        dState_.store(0); dPos_.store(0.0f); dFilled_.store(0.0f);
        state_ = Waiting;
        pos_ = 0.0;
        recorded_ = 0;
        loops_ = 0;
        lastBar_ = INT64_MIN;
        mix_.reset();
    }
    void applyParams(bool snap) override { mix_.setMix(pct(lpMix), snap); }
    void process(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const double bpm = std::clamp(ctx.bpm, 30.0, 300.0);
        const double seconds = std::min(kMaxSeconds - 0.1, kLoopBeats[std::clamp(ip(lpLength), 0, 4)] * 60.0 / bpm);
        const int L = std::max(64, static_cast<int>(seconds * sr_));  // loop length in samples
        const int capture = ip(lpCapture);  // 0 first sound, 1 bar start (host), 2 re-capture every 4 loops
        const float overdub = pct(lpOverdub);
        const double speed = ip(lpSpeed) == 0 ? 0.5 : (ip(lpSpeed) == 2 ? 2.0 : 1.0);
        const bool rev = bp(lpReverse);
        const int fade = std::max(16, static_cast<int>(fp(lpFade) * 0.001f * srf()));
        for (int i = 0; i < n; ++i) {
            const float in[2] = {ch[c0][i], ch[c1][i]};
            if (state_ == Waiting) {
                bool start = false;
                if (capture == 1) {
                    // start exactly when the host's position crosses a bar line (4/4)
                    if (ctx.ppqValid && ctx.hostPlaying) {
                        const double beat = ctx.ppqPosition + i * bpm / 60.0 / sr_;
                        const int64_t bar = static_cast<int64_t>(std::floor(beat / 4.0));
                        start = lastBar_ != INT64_MIN && bar != lastBar_;
                        lastBar_ = bar;
                    }
                } else start = std::fabs(in[0]) + std::fabs(in[1]) > 0.004f;
                if (start) { state_ = Recording; recorded_ = 0; }
            }
            float out[2] = {0, 0};
            if (state_ == Recording) {
                for (int c = 0; c < 2; ++c) buf_[c][static_cast<size_t>(recorded_)] = in[c];
                // coarse waveform for the settings panel (lock-free, one bin per 1/96 of the loop)
                const int bin = std::min(FxDisplay::kPeaks - 1, static_cast<int>(static_cast<int64_t>(recorded_) * FxDisplay::kPeaks / L));
                if (recorded_ == 0) for (auto& pk : peaks_) pk.store(0.0f, std::memory_order_relaxed);
                const float a = std::max(std::fabs(in[0]), std::fabs(in[1]));
                if (a > peaks_[static_cast<size_t>(bin)].load(std::memory_order_relaxed)) peaks_[static_cast<size_t>(bin)].store(a, std::memory_order_relaxed);
                if (++recorded_ >= L) { state_ = Playing; pos_ = 0.0; loops_ = 0; }
            } else if (state_ == Playing) {
                const double p = rev ? (L - 1 - pos_) : pos_;
                const int i0 = static_cast<int>(p);
                const float f = static_cast<float>(p - i0);
                // crossfade the loop seam
                const double edgeDist = std::min(pos_, static_cast<double>(L) - pos_);
                const float w = edgeDist < fade ? static_cast<float>(0.5 - 0.5 * std::cos(kPiD * edgeDist / fade)) : 1.0f;
                for (int c = 0; c < 2; ++c) {
                    const auto& b = buf_[c];
                    const float s = b[static_cast<size_t>(i0 % L)] + f * (b[static_cast<size_t>((i0 + 1) % L)] - b[static_cast<size_t>(i0 % L)]);
                    out[c] = s * w;
                    // overdub: layer the incoming sound onto the loop (feedback < 1 keeps it bounded)
                    if (overdub > 0.0f && speed == 1.0) {
                        float& slot = buf_[c][static_cast<size_t>(i0 % L)];
                        slot = slot * (1.0f - 0.25f * overdub) + in[c] * overdub * 0.5f;
                    }
                }
                pos_ += speed;
                if (pos_ >= L) {
                    pos_ -= L;
                    if (capture == 2 && ++loops_ >= 4) { state_ = Recording; recorded_ = 0; }
                }
            }
            ch[0][i] = out[0];
            ch[1][i] = out[1];
        }
        mix_.apply(ch, n);
        dState_.store(static_cast<int>(state_), std::memory_order_relaxed);
        dPos_.store(state_ == Playing ? static_cast<float>(pos_ / L) : 0.0f, std::memory_order_relaxed);
        dFilled_.store(state_ == Recording ? static_cast<float>(recorded_) / static_cast<float>(L) : (state_ == Playing ? 1.0f : 0.0f), std::memory_order_relaxed);
        dSeconds_.store(static_cast<float>(L / sr_), std::memory_order_relaxed);
    }
    bool readDisplay(FxDisplay& d) const override {
        d.state = dState_.load(std::memory_order_relaxed);
        d.position = dPos_.load(std::memory_order_relaxed);
        d.filled = dFilled_.load(std::memory_order_relaxed);
        d.seconds = dSeconds_.load(std::memory_order_relaxed);
        for (int i = 0; i < FxDisplay::kPeaks; ++i) d.peaks[static_cast<size_t>(i)] = peaks_[static_cast<size_t>(i)].load(std::memory_order_relaxed);
        return true;
    }
    std::array<std::atomic<float>, FxDisplay::kPeaks> peaks_{};
    std::atomic<int> dState_{0};
    std::atomic<float> dPos_{0.0f}, dFilled_{0.0f}, dSeconds_{0.0f};
    static constexpr int c0 = 0, c1 = 1;
    enum State { Waiting, Recording, Playing };
    std::vector<float> buf_[2];
    int mask_ = 0, recorded_ = 0, loops_ = 0;
    double pos_ = 0.0;
    State state_ = Waiting;
    int64_t lastBar_ = INT64_MIN;
    MixStage mix_;
};

// =============================================================================================== Resonator
enum { rsKey, rsOctave, rsChord, rsDecay, rsBright, rsSpread, rsMix };
constexpr int kChord[7][5] = {{0, 0, 0, 0, 0}, {0, 12, 24, -12, 0}, {0, 7, 12, 19, 24}, {0, 4, 7, 12, 16}, {0, 3, 7, 12, 15}, {0, 5, 7, 12, 17}, {0, 2, 4, 7, 9}};
class Resonator final : public FxBase {
    static constexpr int kV = 5;
    void onPrepare() override {
        for (auto& c : comb_) for (auto& l : c) l.prepare(static_cast<int>(sr_ / 25.0) + 8);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
    }
    void reset() override { for (auto& c : comb_) for (auto& l : c) l.reset(); for (auto& c : lp_) for (auto& z : c) z = 0.0f; for (auto& d : dc_) d.reset(); mix_.reset(); }
    void applyParams(bool snap) override {
        for (auto& d : dc_) d.prepare(sr_, 20.0f);
        const int root = 12 * (ip(rsOctave) + 2) + ip(rsKey);  // choice '3' (index 2) -> octave 3 (A3 = MIDI 57)
        const auto& ch = kChord[std::clamp(ip(rsChord), 0, 6)];
        for (int v = 0; v < kV; ++v) {
            const float f = 440.0f * std::exp2((static_cast<float>(root + ch[v]) - 69.0f) / 12.0f);
            period_[v] = std::clamp(srf() / f, 4.0f, static_cast<float>(sr_ / 25.0));
            g_[v] = std::pow(10.0f, -3.0f * period_[v] / (fp(rsDecay) * srf()));
        }
        lpA_ = 0.15f + 0.85f * pct(rsBright);
        mix_.setMix(pct(rsMix), snap);
    }
    double tailSeconds() const override { return p_[rsDecay] * 1.2; }
    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float spread = pct(rsSpread);
        for (int i = 0; i < n; ++i) {
            float out[2] = {0, 0};
            for (int c = 0; c < 2; ++c) {
                const float x = ch[c][i] * 0.25f;
                for (int v = 0; v < kV; ++v) {
                    // tuned comb with loop low-pass (Karplus-Strong style); fractional period for exact tuning
                    const float y = comb_[c][v].readCubic(period_[v] - 0.5f);
                    lp_[c][v] += lpA_ * (y - lp_[c][v]);
                    comb_[c][v].push(x + g_[v] * lp_[c][v]);
                    const float pan = (static_cast<float>(v) / (kV - 1) - 0.5f) * spread * (c ? -1.0f : 1.0f);
                    out[c] += y * (1.0f + pan);
                }
            }
            ch[0][i] = dc_[0].process(out[0] * 0.5f);
            ch[1][i] = dc_[1].process(out[1] * 0.5f);
        }
        for (auto& c : lp_) for (auto& z : c) z = dsp::flushTiny(z);
        mix_.apply(ch, n);
    }
    dsp::DelayLine comb_[2][kV];
    float lp_[2][kV] = {}, period_[kV] = {}, g_[kV] = {}, lpA_ = 0.5f;
    dsp::DcBlocker dc_[2];
    MixStage mix_;
};

// =============================================================================================== Convolution
// Uniform partitioned overlap-save convolution (partition P = 256 -> latency 256). All built-in IRs and
// their partition spectra are generated in prepare(), so switching IR on the audio thread is a crossfade
// between two precomputed responses, never an allocation.
enum { cvIr, cvLength, cvPredelay, cvTone, cvWidth, cvMix };
class Convolution final : public FxBase {
    static constexpr int P = 256, NF = 2 * P;
    static constexpr int kIrs = 6;
    static constexpr double kIrSeconds = 2.5;
    void onPrepare() override {
        fft_.prepare(NF);
        parts_ = static_cast<int>(std::ceil(kIrSeconds * sr_ / P));
        const size_t B = NF;
        for (int t = 0; t < kIrs; ++t)
            for (int c = 0; c < 2; ++c) h_[t][c].assign(static_cast<size_t>(parts_) * B, dsp::cfloat());
        // ---- procedurally generated, deterministic IRs (no files, no licences) ----
        std::vector<float> ir(static_cast<size_t>(parts_) * P);
        dsp::Rng rng(1234);
        for (int t = 0; t < kIrs; ++t) {
            for (int c = 0; c < 2; ++c) {
                dsp::Svf lp, hp;
                const float decay[kIrs] = {0.45f, 1.6f, 2.2f, 0.06f, 2.0f, 1.2f};
                const float cut[kIrs] = {7000.0f, 9000.0f, 4500.0f, 4500.0f, 8000.0f, 3000.0f};
                lp.setup(dsp::Svf::Lowpass, cut[t], 0.6f, 0, sr_);
                hp.setup(dsp::Svf::Highpass, t == 3 ? 90.0f : 120.0f, 0.6f, 0, sr_);
                const size_t len = std::min(ir.size(), static_cast<size_t>(decay[t] * 3.0f * srf()) + 1);
                std::fill(ir.begin(), ir.end(), 0.0f);
                for (size_t i = 0; i < len; ++i) {
                    const float tt = static_cast<float>(i) / srf();
                    float env = std::exp(-6.9f * tt / decay[t]);
                    float s = rng.bipolar();
                    if (t == 2) s *= 0.6f + 0.4f * std::sin(dsp::kTwoPi * 2400.0f * tt * tt * 3.0f);       // spring: dispersive chirp
                    if (t == 3) { s = (i == 0 ? 6.0f : 0.0f) + s * 0.3f; }                                 // cabinet: impulse + short body
                    if (t == 4) env = std::min(1.0f, tt / 1.6f) * std::exp(-6.9f * std::max(0.0f, tt - 1.6f) / 0.4f);  // bloom: reverse swell
                    if (t == 5) s *= std::sin(dsp::kTwoPi * 0.3f * tt * (c ? 1.1f : 1.0f));              // ghostly: slow beating
                    ir[i] = hp.process(lp.process(s)) * env;
                }
                if (t == 3) { dsp::Svf bell; bell.setup(dsp::Svf::Bell, 1800.0f, 1.2f, 6.0f, sr_); for (size_t i = 0; i < len; ++i) ir[i] = bell.process(ir[i]); }
                double e = 0;
                for (size_t i = 0; i < len; ++i) e += static_cast<double>(ir[i]) * ir[i];
                const float norm = static_cast<float>(1.0 / std::sqrt(std::max(e, 1e-12)) * 0.5);
                std::vector<dsp::cfloat> buf(B);
                for (int p = 0; p < parts_; ++p) {
                    std::fill(buf.begin(), buf.end(), dsp::cfloat());
                    for (int k = 0; k < P; ++k) buf[static_cast<size_t>(k)] = dsp::cfloat(ir[static_cast<size_t>(p * P + k)] * norm, 0.0f);
                    fft_.forward(buf.data());
                    std::copy(buf.begin(), buf.end(), h_[t][c].begin() + static_cast<long>(p) * static_cast<long>(B));
                }
            }
        }
        for (auto& x : fdl_) x.assign(static_cast<size_t>(parts_) * B, dsp::cfloat());
        for (auto& x : inBuf_) x.assign(NF, 0.0f);
        for (auto& x : outBuf_) x.assign(P, 0.0f);
        work_.assign(B, dsp::cfloat());
        acc_.assign(B, dsp::cfloat());
        acc2_.assign(B, dsp::cfloat());
        pre_.prepare(static_cast<int>(0.26 * sr_));
        mix_.prepare(sr_, maxBlock_, P, MixLaw::Flat);
    }
    int latencySamples() const override { return P; }
    double tailSeconds() const override { return kIrSeconds; }
    void reset() override {
        for (auto& x : fdl_) std::fill(x.begin(), x.end(), dsp::cfloat());
        for (auto& x : inBuf_) std::fill(x.begin(), x.end(), 0.0f);
        for (auto& x : outBuf_) std::fill(x.begin(), x.end(), 0.0f);
        fill_ = 0;
        head_ = 0;
        cur_ = prev_ = std::clamp(ip(cvIr), 0, kIrs - 1);
        xf_ = 1.0f;
        pre_.reset();
        for (auto& f : tone_) f.reset();
        mix_.reset();
    }
    void applyParams(bool snap) override {
        const int t = std::clamp(ip(cvIr), 0, kIrs - 1);
        if (snap) { cur_ = prev_ = t; xf_ = 1.0f; }
        else if (t != cur_) { prev_ = cur_; cur_ = t; xf_ = 0.0f; }  // crossfade over the next blocks
        for (auto& f : tone_) f.setup(dsp::Svf::Lowpass, fp(cvTone), 0.6f, 0, sr_);
        mix_.setMix(pct(cvMix), snap);
    }
    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float pre = fp(cvPredelay) * 0.001f * srf();
        const int useParts = std::max(1, static_cast<int>(std::ceil(parts_ * std::clamp(pct(cvLength), 0.02f, 1.0f))));
        const float width = pct(cvWidth);
        for (int i = 0; i < n; ++i) {
            pre_.push(0.5f * (ch[0][i] + ch[1][i]));
            const float x = pre_.readLinear(pre);
            float y[2];
            for (int c = 0; c < 2; ++c) {
                inBuf_[c][static_cast<size_t>(P + fill_)] = x;
                y[c] = outBuf_[c][static_cast<size_t>(fill_)];
            }
            if (++fill_ == P) { fill_ = 0; block(useParts); }
            const float M = 0.5f * (y[0] + y[1]), S = 0.5f * (y[0] - y[1]) * width;
            ch[0][i] = tone_[0].process(M + S);
            ch[1][i] = tone_[1].process(M - S);
        }
        for (auto& f : tone_) f.flush();
        mix_.apply(ch, n);
    }
    void block(int useParts) {
        head_ = (head_ + parts_ - 1) % parts_;
        const size_t B = NF;
        const float xfStep = static_cast<float>(P) / (0.08f * srf());
        const float a = xf_, b = std::min(1.0f, xf_ + xfStep);
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < NF; ++k) work_[static_cast<size_t>(k)] = dsp::cfloat(inBuf_[c][static_cast<size_t>(k)], 0.0f);
            fft_.forward(work_.data());
            std::copy(work_.begin(), work_.end(), fdl_[c].begin() + static_cast<long>(head_) * static_cast<long>(B));
            auto accumulate = [&](int t, std::vector<dsp::cfloat>& acc) {
                std::fill(acc.begin(), acc.end(), dsp::cfloat());
                for (int p = 0; p < useParts; ++p) {
                    const dsp::cfloat* X = &fdl_[c][static_cast<size_t>((head_ + p) % parts_) * B];
                    const dsp::cfloat* H = &h_[t][c][static_cast<size_t>(p) * B];
                    for (size_t k = 0; k <= static_cast<size_t>(P); ++k) acc[k] += X[k] * H[k];
                }
                for (int k = 1; k < P; ++k) acc[static_cast<size_t>(NF - k)] = std::conj(acc[static_cast<size_t>(k)]);
                fft_.inverse(acc.data());
            };
            accumulate(cur_, acc_);
            if (a < 1.0f) accumulate(prev_, acc2_);
            const float inv = 1.0f / static_cast<float>(NF);
            for (int k = 0; k < P; ++k) {
                const float w = a + (b - a) * static_cast<float>(k) / P;  // linear crossfade old -> new IR
                float v = acc_[static_cast<size_t>(P + k)].real() * inv;
                if (a < 1.0f) v = w * v + (1.0f - w) * acc2_[static_cast<size_t>(P + k)].real() * inv;
                outBuf_[c][static_cast<size_t>(k)] = v;
            }
            std::copy(inBuf_[c].begin() + P, inBuf_[c].end(), inBuf_[c].begin());  // slide (overlap-save)
        }
        xf_ = b;
    }
    dsp::Fft fft_;
    int parts_ = 1, fill_ = 0, head_ = 0, cur_ = 0, prev_ = 0;
    float xf_ = 1.0f;
    std::vector<dsp::cfloat> h_[kIrs][2], fdl_[2], work_, acc_, acc2_;
    std::vector<float> inBuf_[2], outBuf_[2];
    dsp::DelayLine pre_;
    dsp::Svf tone_[2];
    MixStage mix_;
};

// =============================================================================================== Amp / Cabinet
enum { amGain, amBass, amMid, amTreble, amPresence, amMaster, amCab, amOut, amMix };
class Amp final : public FxBase {
    void onPrepare() override {
        for (auto& o : os_) o.prepare(maxBlock_);
        mix_.prepare(sr_, maxBlock_, os_[0].latency(), MixLaw::Linear);
        work_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        out_.prepare(sr_, 30.0f);
    }
    int latencySamples() const override { return os_[0].latency(); }
    void reset() override {
        for (auto& o : os_) o.reset();
        for (int c = 0; c < 2; ++c) {
            s1_[c].reset(); s2_[c].reset(); pw_[c].reset();
            for (auto& f : f_[c]) f.reset();
            for (auto& f : cab_[c]) f.reset();
            sag_[c] = 0.0f;
        }
        mix_.reset();
    }
    void applyParams(bool snap) override {
        const double hi = sr_ * 2.0;  // the amp stages run at 2x
        for (int c = 0; c < 2; ++c) {
            s1_[c].setShape(dsp::Shape::Asym);
            s2_[c].setShape(dsp::Shape::Tanh);
            pw_[c].setShape(dsp::Shape::SoftCubic);
            f_[c][0].setup(dsp::Svf::Highpass, 90.0f, 0.7f, 0, hi);                 // input coupling (tight)
            f_[c][1].setup(dsp::Svf::Lowpass, 9000.0f, 0.7f, 0, hi);                // interstage
            f_[c][2].setup(dsp::Svf::LowShelf, 120.0f, 0.7f, fp(amBass), hi);       // tone stack
            f_[c][3].setup(dsp::Svf::Bell, 700.0f, 0.8f, fp(amMid), hi);
            f_[c][4].setup(dsp::Svf::HighShelf, 2500.0f, 0.7f, fp(amTreble), hi);
            f_[c][5].setup(dsp::Svf::HighShelf, 4500.0f, 0.7f, fp(amPresence), hi);  // presence after the power stage
            const int cab = ip(amCab);  // 0 off, 1 1x12 open, 2 2x12, 3 4x12 closed
            const float lpF[4] = {20000.0f, 6000.0f, 5200.0f, 4300.0f}, resF[4] = {100.0f, 110.0f, 95.0f, 85.0f}, resG[4] = {0.0f, 3.0f, 4.0f, 5.0f};
            cab_[c][0].setup(dsp::Svf::Highpass, cab ? 70.0f : 20.0f, 0.7f, 0, sr_);
            cab_[c][1].setup(dsp::Svf::Bell, resF[cab], 1.2f, resG[cab], sr_);
            cab_[c][2].setup(dsp::Svf::Bell, 2800.0f, 1.5f, cab ? -4.0f : 0.0f, sr_);
            cab_[c][3].setup(dsp::Svf::Lowpass, lpF[cab], 0.9f, 0, sr_);
            cab_[c][4].setup(dsp::Svf::Lowpass, lpF[cab] * 1.3f, 0.6f, 0, sr_);
        }
        out_.setTarget(dsp::dbToGain(fp(amOut)));
        if (snap) out_.snap(out_.target());
        mix_.setMix(pct(amMix), snap);
    }
    void process(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float g1 = dsp::dbToGain(fp(amGain) * 0.6f), g2 = dsp::dbToGain(fp(amGain) * 0.4f);
        const float master = dsp::dbToGain(fp(amMaster));
        const float sagA = 1.0f - std::exp(-1.0f / (0.06f * 2.0f * srf()));
        const bool cabOn = ip(amCab) > 0;
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < n; ++i) work_[static_cast<size_t>(i)] = ch[c][i];
            float* hi = os_[c].up(work_.data(), n);
            for (int k = 0; k < 2 * n; ++k) {
                float x = f_[c][0].process(hi[k]) * g1;
                x = s1_[c].process(x);                         // first preamp stage (asymmetric: even harmonics)
                x = f_[c][1].process(x) * g2;
                x = s2_[c].process(x);                         // second stage
                x = f_[c][4].process(f_[c][3].process(f_[c][2].process(x)));
                // power stage with sag: a loud passage lowers the effective headroom briefly
                sag_[c] += sagA * (std::fabs(x) - sag_[c]);
                const float head = 1.0f / (1.0f + 0.4f * sag_[c]);
                x = pw_[c].process(x * master * head) / std::max(0.3f, head);
                hi[k] = f_[c][5].process(x);
            }
            os_[c].down(hi, ch[c], n);
            if (cabOn) for (int i = 0; i < n; ++i) { float y = ch[c][i]; for (auto& f : cab_[c]) y = f.process(y); ch[c][i] = y; }
        }
        for (int i = 0; i < n; ++i) { const float g = out_.next() * 0.5f; ch[0][i] *= g; ch[1][i] *= g; }
        for (int c = 0; c < 2; ++c) { for (auto& f : f_[c]) f.flush(); for (auto& f : cab_[c]) f.flush(); }
        mix_.apply(ch, n);
    }
    dsp::Oversampler2x os_[2];
    dsp::AdaaShaper s1_[2], s2_[2], pw_[2];
    dsp::Svf f_[2][6], cab_[2][5];
    float sag_[2] = {0, 0};
    std::vector<float> work_;
    dsp::Smoother out_;
    MixStage mix_;
};

}  // namespace

FxDescriptor describeFreqShifter() {
    FxDescriptor d;
    d.typeId = "freq_shift";
    d.name = "Frequency Shifter";
    d.family = FxFamily::Modulation;
    d.tooltip = "Moves every frequency by the same number of Hz: small shifts beat and swirl, large ones turn sounds clangorous and alien.";
    d.params = {
        P::flt("shift", "Shift", "Hz", -1000.0f, 1000.0f, 7.0f, "How far every frequency moves. Unlike pitch shifting, harmonies bend out of tune: that's the point."),
        P::flt("fine", "Fine", "Hz", -10.0f, 10.0f, 0.0f, "Fine adjustment for slow beating effects."),
        P::flt("stereo", "Stereo", "Hz", -20.0f, 20.0f, 0.0f, "Extra shift on the right channel for a rotating stereo swirl."),
        P::pct("feedback", "Feedback", 0.0f, "Feeds the shifted sound back so it keeps climbing (barber-pole effect)."),
        P::logf("delay_ms", "Delay", "ms", 1.0f, 1000.0f, 120.0f, "Time between climbing feedback steps."),
        P::pct("mix", "Mix", 100.0f, "Blend of original and shifted sound."),
    };
    d.create = [] { return std::make_unique<FreqShifter>(); };
    return d;
}

FxDescriptor describeDoppler() {
    FxDescriptor d;
    d.typeId = "doppler";
    d.name = "Doppler";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "Makes the sound fly past or circle the listener, with the real pitch drop of a passing car.";
    d.params = {
        P::choice("path", "Path", {"Pass by (left to right)", "Pass by (right to left)", "Circle"}, 0, "How the sound travels around you."),
        P::logf("speed", "Speed", "m/s", 1.0f, 120.0f, 25.0f, "Faster movement = bigger pitch swoop."),
        P::logf("distance", "Distance", "m", 0.5f, 40.0f, 4.0f, "Closest distance to the listener. Closer = more dramatic."),
        P::logf("period", "Cycle", "s", 0.5f, 16.0f, 4.0f, "How often the fly-by repeats."),
        P::toggle("sync", "Sync", false, "Lock the cycle to the song tempo."),
        P::choice("division", "Division", divisionLabels(), 15, "Cycle as a note length when Sync is on."),
        P::pct("mix", "Mix", 100.0f, "Blend of original and moving sound."),
    };
    d.create = [] { return std::make_unique<Doppler>(); };
    return d;
}

FxDescriptor describeLooper() {
    FxDescriptor d;
    d.typeId = "looper";
    d.name = "Looper";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Captures a phrase and holds it as a repeating loop that you can layer onto, reverse or slow down.";
    d.params = {
        P::choice("length", "Length", {"1 beat", "2 beats", "1 bar", "2 bars", "4 bars"}, 2, "How long the captured loop is (follows the song tempo)."),
        P::choice("capture", "Capture", {"On first sound", "On the next bar", "Every 4 loops"}, 0, "When recording starts: when sound arrives, on the bar line (host playing), or refreshed every 4 loops."),
        P::pct("overdub", "Overdub", 0.0f, "Layers new sound onto the loop each time round; older layers slowly fade."),
        P::choice("speed", "Speed", {"Half", "Normal", "Double"}, 1, "Playback speed (and pitch) of the loop."),
        P::toggle("reverse", "Reverse", false, "Plays the loop backwards."),
        P::flt("fade_ms", "Seam Fade", "ms", 1.0f, 100.0f, 10.0f, "Smooths the join where the loop wraps around."),
        P::pct("mix", "Mix", 50.0f, "Balance of the live sound and the loop. 50% keeps both at full level."),
    };
    d.create = [] { return std::make_unique<Looper>(); };
    return d;
}

FxDescriptor describeResonator() {
    FxDescriptor d;
    d.typeId = "resonator";
    d.name = "Resonator";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Tuned strings that ring in sympathy: any sound (even drums or noise) starts singing a chord.";
    d.params = {
        P::choice("key", "Root", {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 9, "The root note the resonators are tuned to."),
        P::choice("octave", "Octave", {"1", "2", "3", "4", "5"}, 2, "Register of the strings."),
        P::choice("chord", "Chord", {"Unison", "Octaves", "Fifths", "Major", "Minor", "Sus4", "Pentatonic"}, 3, "Which notes ring together."),
        P::logf("decay", "Decay", "s", 0.1f, 15.0f, 2.5f, "How long the strings ring."),
        P::pct("bright", "Brightness", 50.0f, "Darker = soft, muted strings; brighter = metallic and glassy."),
        P::pct("spread", "Spread", 50.0f, "Spreads the strings across the stereo field."),
        P::pct("mix", "Mix", 50.0f, "Blend of original and ringing strings."),
    };
    d.create = [] { return std::make_unique<Resonator>(); };
    return d;
}

FxDescriptor describeConvolution() {
    FxDescriptor d;
    d.typeId = "convolution";
    d.name = "Convolution";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "Places the sound inside a captured-style space or object: rooms, plates, a spring, a speaker, a backwards bloom.";
    d.params = {
        P::choice("ir", "Space", {"Small Room", "Plate", "Spring", "Speaker Box", "Bloom", "Ghost"}, 0, "Which response the sound is convolved with. All are built in, generated by IH1."),
        P::pct("length", "Length", 100.0f, "Shortens the response for tighter spaces."),
        P::flt("predelay", "Pre-delay", "ms", 0.0f, 250.0f, 10.0f, "Gap before the space starts."),
        P::logf("tone", "Tone", "Hz", 1000.0f, 20000.0f, 16000.0f, "Darkens the response."),
        P::flt("width", "Width", "%", 0.0f, 150.0f, 100.0f, "Stereo width of the space."),
        P::pct("mix", "Mix", 35.0f, "Balance of dry and convolved sound. 50% keeps both at full level."),
    };
    d.create = [] { return std::make_unique<Convolution>(); };
    return d;
}

FxDescriptor describeAmp() {
    FxDescriptor d;
    d.typeId = "amp";
    d.name = "Amp / Cabinet";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "A guitar-amp style preamp, tone stack and power stage into a speaker cabinet: from edge-of-breakup to thick drive.";
    d.params = {
        P::flt("gain", "Gain", "dB", 0.0f, 48.0f, 18.0f, "Preamp drive. Low = clean with bite, high = saturated and compressed."),
        P::flt("bass", "Bass", "dB", -12.0f, 12.0f, 0.0f, "Low end of the tone stack."),
        P::flt("mid", "Mid", "dB", -12.0f, 12.0f, 0.0f, "Mids: scoop for heavy tones, boost to cut through."),
        P::flt("treble", "Treble", "dB", -12.0f, 12.0f, 0.0f, "Top end of the tone stack."),
        P::flt("presence", "Presence", "dB", -12.0f, 12.0f, 2.0f, "Upper bite after the power stage."),
        P::flt("master", "Master", "dB", -24.0f, 12.0f, 0.0f, "Drives the power stage: more = sag and compression."),
        P::choice("cab", "Cabinet", {"Off (DI)", "1x12 Open", "2x12", "4x12 Closed"}, 2, "Speaker cabinet character; Off sends the raw amp sound."),
        P::flt("out", "Output", "dB", -24.0f, 12.0f, 0.0f, "Final level."),
        P::pct("mix", "Mix", 100.0f, "Blend of clean and amp sound."),
    };
    d.create = [] { return std::make_unique<Amp>(); };
    return d;
}

}  // namespace ih1::fx
