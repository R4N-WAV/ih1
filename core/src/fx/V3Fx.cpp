// V3 feedback round: new sound-design modules (Phases 4.3, 4.4, 5, 6). Original IH1 designs; product manuals were
// workflow references only (research supplement Part II), no code or presets were copied.
//
//   Degrade         sampler-era degradation: drive -> bandwidth collapse -> RateReducer (rate, jitter, AA, reconstruction,
//                   bits) -> clock wobble and noise floor. The standalone home of the shared rate-reduction component.
//   Fold & Crush    dynamic wavefolding (drive follows the envelope) with a transient-selective crusher.
//   Spectral Smear  STFT: frequency smear with an up/down bias, time smear (magnitude memory), phase scatter,
//                   spectral gate with hysteresis, bin disintegration (dropouts) and a spectral hold.
//   Spectral Delay  STFT: 24 log-spaced bands, each with its own delay (shape: rising, falling, V, comb, scatter),
//                   per-band feedback with high-band damping, stereo offset.
//   Time Warp       buffer playback with a moving read head: Varispeed, Tape Stop, Half-Time, Scrub, Backspin, with an
//                   optional pitch-keeping (granular) read and tempo-grid triggering.
//   Glitch Scatter  tempo-grid fragment sequencer: micro-loops with ratchets, reversed fragments, scan jumps, pitched
//                   fragments and gates, chosen per step from seeded probabilities (repeatable), crossfaded.
//
// Every module is real-time safe (allocation only in onPrepare), bounded (feedback <= 0.95 and soft-limited, fixed
// voice counts) and click-free (all jumps crossfade; every fragment has a fade).
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Fft.h"
#include "ih1/dsp/Nonlinear.h"
#include "ih1/dsp/RateReducer.h"
#include "ih1/dsp/Spectral.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {
using dsp::cfloat;

inline float softLimit(float x, float knee = 1.5f) {
    const float a = std::fabs(x);
    return a <= knee ? x : std::copysign(knee + std::tanh(a - knee), x);
}

// Level-matching gain for strong nonlinear stages (the V3 Saturation lesson): starts conservative, warms up fast,
// then tracks reductions quickly and increases slowly, so it can never spike upwards.
struct AutoLevel {
    void prepare(double sr) {
        sr_ = sr;
        aSlow_ = 1.0f - std::exp(-1.0f / (0.3f * static_cast<float>(sr)));
        aFast_ = 1.0f - std::exp(-1.0f / (0.005f * static_cast<float>(sr)));
        gWarm_ = 1.0f - std::exp(-1.0f / (0.001f * static_cast<float>(sr)));
        down_ = 1.0f - std::exp(-1.0f / (0.005f * static_cast<float>(sr)));
        up_ = 1.0f - std::exp(-1.0f / (0.06f * static_cast<float>(sr)));
        reset(1.0f);
    }
    // `lagSamples`: how far the processed signal trails its input (oversampling, filters); the input power is delayed
    // by that much so a rising fade-in is not mistaken for gain.
    void reset(float startGain, int lagSamples = 0) {
        inP_ = outP_ = 1e-6f;
        g_ = startGain;
        warm_ = static_cast<int>(0.04 * sr_);
        lag_ = std::clamp(lagSamples, 0, kLag - 1);
        ring_.fill(0.0f);
        rp_ = 0;
    }
    float next(float inPowNow, float outPow, bool on) {
        ring_[static_cast<size_t>(rp_)] = inPowNow;
        const float inPow = ring_[static_cast<size_t>((rp_ - lag_ + kLag) % kLag)];
        rp_ = (rp_ + 1) % kLag;
        const bool warm = warm_ > 0;
        if (warm && inPow > 1e-10f && --warm_ == 0) {
            // end of warm-up: re-seed the slow trackers so they agree with the gain reached (no inherited bias)
            outP_ = std::max(outP_, 1e-10f);
            inP_ = g_ * g_ * outP_;
        }
        const float a = warm ? aFast_ : aSlow_;
        inP_ += a * (inPow - inP_);
        outP_ += a * (outPow - outP_);
        const float t = on ? std::clamp(std::sqrt(inP_ / std::max(outP_, 1e-10f)), 0.03f, 2.0f) : 1.0f;
        g_ += (warm ? gWarm_ : (t < g_ ? down_ : up_)) * (t - g_);
        return g_;
    }
    double sr_ = 48000.0;
    float aSlow_ = 0, aFast_ = 0, gWarm_ = 0, down_ = 0, up_ = 0, inP_ = 1e-6f, outP_ = 1e-6f, g_ = 1.0f;
    int warm_ = 0, lag_ = 0, rp_ = 0;
    static constexpr int kLag = 128;
    std::array<float, kLag> ring_{};
};

// A read head over a capture ring with a crossfaded jump: the only way any module here moves its read position
// discontinuously.
struct XfadeReader {
    double pos = 0.0, oldPos = 0.0, rate = 1.0, oldRate = 1.0;
    float fade = 1.0f, fadeStep = 0.01f;
    // V4 Phase 4: a jump requested while a crossfade is still running is deferred until it ends (two heads cannot
    // represent three sources; replacing the old head mid-fade dropped an audible signal instantly)
    bool pending = false;
    double pTo = 0.0, pRate = 1.0;
    void service() { if (pending && fade >= 1.0f) { pending = false; jump(pTo, pRate); } }
    void jump(double to, double newRate) {
        if (fade < 1.0f) { pending = true; pTo = to; pRate = newRate; return; }
        oldPos = pos;
        oldRate = rate;
        pos = to;
        rate = newRate;
        fade = 0.0f;
    }
};

float hermite(const std::vector<float>& b, int64_t mask, double pos) {
    const double fl = std::floor(pos);
    const int64_t i = static_cast<int64_t>(fl);
    const float f = static_cast<float>(pos - fl);
    const float y0 = b[static_cast<size_t>((i - 1) & mask)], y1 = b[static_cast<size_t>(i & mask)];
    const float y2 = b[static_cast<size_t>((i + 1) & mask)], y3 = b[static_cast<size_t>((i + 2) & mask)];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

// =========================================================================================================== Degrade
enum { dgRate, dgBits, dgJitter, dgAa, dgRecon, dgBandwidth, dgDrive, dgWobble, dgNoise, dgStereo, dgOut, dgMix };

class Degrade final : public FxBase {
    void onPrepare() override {
        rr_.prepare(sr_, 0x5EEDu);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        rate_.prepare(sr_, 40.0f);
        drive_.prepare(sr_, 30.0f);
        out_.prepare(sr_, 30.0f);
        bw_.prepare(sr_, 60.0f);
        for (auto& d : dc_) d.prepare(sr_, 8.0f);
        lfo_.prepare(sr_, 0xD3u);
        lfo_.setShape(0);
        lfo_.setRate(0.63f);
    }
    void reset() override {
        rr_.reset();
        mix_.reset();
        for (int c = 0; c < 2; ++c) { hp_[c].reset(); lp_[c].reset(); lp2_[c].reset(); dc_[c].reset(); }
        noise_.seed_(0x1234u);
        lastBw_ = -1.0f;
        abs_ = 0;
        lfo_.setPhase(0.0f);
    }
    void applyParams(bool snap) override {
        rate_.setTarget(std::log(fp(dgRate)));
        drive_.setTarget(dsp::dbToGain(fp(dgDrive)));
        out_.setTarget(dsp::dbToGain(fp(dgOut)));
        bw_.setTarget(pct(dgBandwidth));
        if (snap) { rate_.snap(rate_.target()); drive_.snap(drive_.target()); out_.snap(out_.target()); bw_.snap(bw_.target()); }
        mix_.setMix(pct(dgMix), snap);
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        // control values move in 32-sample steps aligned to absolute time: identical output for any host block size
        int done = 0;
        while (done < n) {
            const int m = std::min(n - done, 32 - static_cast<int>(abs_ % 32));
            float* sub[2] = {ch[0] + done, ch[1] + done};
            chunk(sub, m, ctx);
            abs_ += m;
            done += m;
        }
        mix_.apply(ch, n);
    }
    void chunk(float* const* ch, int n, const ProcessContext&) {
        // control values (bandwidth, rate, wobble) are evaluated only at aligned 32-sample boundaries and held
        if (abs_ % 32 == 0) {
            for (int i = 0; i < 32; ++i) { ctlBw_ = bw_.next(); ctlRate_ = rate_.next(); ctlWob_ = lfo_.next(); }
        }
        const float bw = ctlBw_;
        if (std::fabs(bw - lastBw_) > 1e-4f) {
            const float hpF = 20.0f * std::pow(35.0f, bw);          // 20 Hz .. 700 Hz
            const float lpF = 20000.0f * std::pow(0.11f, bw);       // 20 kHz .. 2.2 kHz
            for (int c = 0; c < 2; ++c) {
                hp_[c].setup(dsp::Svf::Highpass, hpF, 0.7f, 0, sr_);
                lp_[c].setup(dsp::Svf::Lowpass, std::min(lpF, 0.45f * srf()), 0.85f, 0, sr_);
                lp2_[c].setup(dsp::Svf::Lowpass, std::min(lpF * 1.3f, 0.45f * srf()), 0.6f, 0, sr_);
            }
            lastBw_ = bw;
        }
        const float wob = pct(dgWobble);
        const float noiseAmp = pct(dgNoise) * pct(dgNoise) * 0.02f;
        // drive into an asymmetric soft clipper, then the bandwidth filters (per sample)
        for (int i = 0; i < n; ++i) {
            const float g = drive_.next();
            const float comp = 1.0f / std::sqrt(g);
            for (int c = 0; c < 2; ++c) {
                float x = ch[c][i] * g;
                x = std::tanh(x + 0.15f * x * x) * comp;
                x = dc_[c].process(x);
                if (bw > 0.001f) x = lp2_[c].process(lp_[c].process(hp_[c].process(x)));
                x += noiseAmp * noise_.bipolar();
                ch[c][i] = x;
            }
        }
        // rate with slow clock wobble (one value per block; the reducer's own jitter is per hold period)
        const float lr = ctlRate_, w = ctlWob_;
        dsp::RateReducer::Settings s;
        s.rateHz = std::exp(lr) * (1.0f + 0.18f * wob * w);
        s.bits = fp(dgBits);
        s.jitter = pct(dgJitter);
        s.antiAlias = ip(dgAa);
        s.reconstruct = ip(dgRecon);
        s.linked = ip(dgStereo) == 0;
        rr_.process(ch[0], ch[1], n, s);
        for (int i = 0; i < n; ++i) {
            const float o = out_.next();
            ch[0][i] = softLimit(ch[0][i] * o, 2.0f);
            ch[1][i] = softLimit(ch[1][i] * o, 2.0f);
        }
        for (int c = 0; c < 2; ++c) { hp_[c].flush(); lp_[c].flush(); lp2_[c].flush(); }
    }
    int64_t abs_ = 0;
    float ctlBw_ = 0.0f, ctlRate_ = 0.0f, ctlWob_ = 0.0f;
    dsp::RateReducer rr_;
    MixStage mix_;
    dsp::Smoother rate_, drive_, out_, bw_;
    dsp::Svf hp_[2], lp_[2], lp2_[2];
    dsp::DcBlocker dc_[2];
    dsp::Lfo lfo_;
    dsp::Rng noise_{0x1234u};
    float lastBw_ = -1.0f;
};

// ====================================================================================================== Fold & Crush
enum { fcDrive, fcFold, fcBias, fcDynamics, fcCrush, fcTarget, fcTone, fcAuto, fcOut, fcMix };

class FoldCrush final : public FxBase {
    void onPrepare() override {
        for (auto& o : os_) o.prepareVariant(maxBlock_, latMode_ == LatencyMode::Quality ? dsp::Oversampler2x::Variant::Fir : (latMode_ == LatencyMode::Low ? dsp::Oversampler2x::Variant::MinPhaseIir : dsp::Oversampler2x::Variant::Bypass));
        mix_.prepare(sr_, maxBlock_, os_[0].latency(), MixLaw::Linear);
        drive_.prepare(sr_, 30.0f);
        fold_.prepare(sr_, 30.0f);
        bias_.prepare(sr_, 30.0f);
        out_.prepare(sr_, 30.0f);
        work_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        env_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        inPow_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        for (auto& d : dc_) d.prepare(sr_ * os_[0].factor(), 8.0f);
        rr_.prepare(sr_, 0xFC01u);
        auto_.prepare(sr_);
        envF_.prepare(sr_, 8.0f, 140.0f);
        auto coef = [this](float ms) { return 1.0f - std::exp(-1.0f / (ms * 0.001f * srf())); };
        aF_ = coef(1.0f); rF_ = coef(25.0f); aS_ = coef(40.0f); rS_ = coef(150.0f);
        crushL_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        crushR_.assign(static_cast<size_t>(maxBlock_), 0.0f);
    }
    int latencySamples() const override { return os_[0].latency(); }
    void reset() override {
        for (auto& o : os_) o.reset();
        for (auto& d : dc_) d.reset();
        for (auto& t : tone_) t.reset();
        rr_.reset();
        mix_.reset();
        envF_.reset();
        fastP_ = slowP_ = 0.0f;
        auto_.reset(0.25f, os_[0].latency() + 24);  // oversampler + tone filter group delay (approx.)
        warmT_ = static_cast<int>(0.04 * sr_);
    }
    void applyParams(bool snap) override {
        drive_.setTarget(dsp::dbToGain(fp(fcDrive)));
        fold_.setTarget(pct(fcFold));
        bias_.setTarget(fp(fcBias) * 0.004f);
        out_.setTarget(dsp::dbToGain(fp(fcOut)));
        if (snap) { drive_.snap(drive_.target()); fold_.snap(fold_.target()); bias_.snap(bias_.target()); out_.snap(out_.target()); }
        for (auto& t : tone_) t.setup(dsp::Svf::Lowpass, std::min(fp(fcTone), 0.45f * srf()), 0.6f, 0, sr_);
        mix_.setMix(pct(fcMix), snap);
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float dyn = fp(fcDynamics) * 0.01f;
        // envelope (shared by both channels) and a transient weight
        float tw[4096];
        const int nn = std::min(n, 4096);
        for (int i = 0; i < nn; ++i) {
            const float a = std::max(std::fabs(ch[0][i]), std::fabs(ch[1][i]));
            inPow_[static_cast<size_t>(i)] = 0.5f * (ch[0][i] * ch[0][i] + ch[1][i] * ch[1][i]);
            const float e = envF_.process(a);
            env_[static_cast<size_t>(i)] = std::clamp(1.0f + dyn * (std::clamp(e / 0.25f, 0.0f, 2.0f) - 0.5f) * 1.5f, 0.15f, 3.0f);
            // transient weight on smoothed power, with a 25 % rise threshold: a steady tone's ripple never counts
            const float p = inPow_[static_cast<size_t>(i)];
            fastP_ += (p > fastP_ ? aF_ : rF_) * (p - fastP_);
            slowP_ += (p > slowP_ ? aS_ : rS_) * (p - slowP_);
            // warm-up: a freshly staged processor receives the engine's de-click fade-in, which is not an attack;
            // both followers track it together so the first real transient is measured from a settled level
            if (warmT_ > 0) { if (a > 1e-5f) --warmT_; slowP_ = fastP_; }
            tw[i] = std::clamp((fastP_ / std::max(slowP_, 1e-9f) - 1.25f) * 1.5f, 0.0f, 1.0f);
        }
        const float d0 = drive_.current(), f0 = fold_.current(), b0 = bias_.current();
        float d1 = d0, f1 = f0, b1 = b0;
        for (int i = 0; i < n; ++i) { d1 = drive_.next(); f1 = fold_.next(); b1 = bias_.next(); }
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < n; ++i) work_[static_cast<size_t>(i)] = ch[c][i] * env_[static_cast<size_t>(std::min(i, nn - 1))];
            float* hi = os_[c].up(work_.data(), n);
            const int m = os_[c].factor() * n;
            for (int k = 0; k < m; ++k) {
                const float t = static_cast<float>(k) / static_cast<float>(m);
                const float drv = d0 + t * (d1 - d0), fo = f0 + t * (f1 - f0), bi = b0 + t * (b1 - b0);
                const float x = hi[k] * drv + bi;
                const float kf = (1.0f + 2.5f * fo) * 1.2f;
                // subtract the curve's own response to the bias: silence stays exactly silent (no DC step)
                const float soft = std::tanh(x) - std::tanh(bi);
                const float folded = std::sin(x * kf) - std::sin(bi * kf);
                hi[k] = dc_[c].process(soft + fo * (folded - soft));
            }
            os_[c].down(hi, ch[c], n);
            for (int i = 0; i < n; ++i) ch[c][i] = tone_[c].process(ch[c][i]);
            tone_[c].flush();
        }
        // transient-selective crush: a reduced copy blended in by the transient (or sustain) weight
        const float crush = pct(fcCrush);
        if (crush > 0.001f) {
            std::copy(ch[0], ch[0] + n, crushL_.begin());
            std::copy(ch[1], ch[1] + n, crushR_.begin());
            dsp::RateReducer::Settings s;
            s.rateHz = srf() * std::pow(0.04f, crush);
            s.bits = 24.0f - 19.0f * crush;
            s.reconstruct = dsp::RateReducer::FilterOff;
            rr_.process(crushL_.data(), crushR_.data(), n, s);
            const bool sustain = ip(fcTarget) == 1;
            for (int i = 0; i < n; ++i) {
                const float w = sustain ? 1.0f - tw[std::min(i, nn - 1)] : tw[std::min(i, nn - 1)];
                ch[0][i] += w * (crushL_[static_cast<size_t>(i)] - ch[0][i]);
                ch[1][i] += w * (crushR_[static_cast<size_t>(i)] - ch[1][i]);
            }
        }
        const bool autoOn = bp(fcAuto);
        for (int i = 0; i < n; ++i) {
            const float op = 0.5f * (ch[0][i] * ch[0][i] + ch[1][i] * ch[1][i]);
            const float g = auto_.next(inPow_[static_cast<size_t>(std::min(i, nn - 1))], op, autoOn) * out_.next();
            ch[0][i] = softLimit(ch[0][i] * g, 2.0f);
            ch[1][i] = softLimit(ch[1][i] * g, 2.0f);
        }
        mix_.apply(ch, n);
    }
    dsp::Oversampler2x os_[2];
    dsp::DcBlocker dc_[2];
    dsp::Svf tone_[2];
    dsp::RateReducer rr_;
    MixStage mix_;
    dsp::Smoother drive_, fold_, bias_, out_;
    dsp::EnvelopeFollower envF_;
    float fastP_ = 0.0f, slowP_ = 0.0f, aF_ = 0.1f, rF_ = 0.01f, aS_ = 0.01f, rS_ = 0.001f;
    AutoLevel auto_;
    int warmT_ = 0;
    std::vector<float> work_, env_, inPow_, crushL_, crushR_;
};

// ===================================================================================================== Spectral Smear
enum { ssSmear, ssBias, ssTime, ssScatter, ssGate, ssDisint, ssHold, ssHighCut, ssOut, ssMix };

class SpectralSmear final : public FxBase {
    void onPrepare() override {
        n_ = dsp::stftSizeFor(sr_, latMode_ == LatencyMode::Quality ? 2048 : 512);
        hop_ = n_ / 4;
        const size_t B = static_cast<size_t>(n_ / 2 + 1);
        for (int c = 0; c < 2; ++c) {
            stft_[c].prepare(n_, hop_);
            ana_[c].prepare(n_, hop_);
            mem_[c].assign(B, 0.0f);
            held_[c].assign(B, 0.0f);
            heldW_[c].assign(B, 0.0f);
            outPh_[c].assign(B, 0.0f);
            up_[c].assign(B, 0.0f);
            dn_[c].assign(B, 0.0f);
            ownPh_[c].assign(B, 0.0f);
        }
        alive_.assign(B, 1.0f);
        jit_.assign(B, 0.0f);
        aliveT_.assign(B, 1);
        gateOpen_.assign(B, 0);
        mix_.prepare(sr_, maxBlock_, n_, MixLaw::Linear);
        out_.prepare(sr_, 30.0f);
    }
    int latencySamples() const override { return n_; }
    double tailSeconds() const override { return bp(ssHold) ? 30.0 : 0.5 + 6.0 * pct(ssTime); }
    void reset() override {
        for (int c = 0; c < 2; ++c) {
            stft_[c].reset();
            ana_[c].reset();
            std::fill(mem_[c].begin(), mem_[c].end(), 0.0f);
            std::fill(held_[c].begin(), held_[c].end(), 0.0f);
            std::fill(outPh_[c].begin(), outPh_[c].end(), 0.0f);
            std::fill(ownPh_[c].begin(), ownPh_[c].end(), 0.0f);
        }
        std::fill(alive_.begin(), alive_.end(), 1.0f);
        std::fill(aliveT_.begin(), aliveT_.end(), 1);
        std::fill(gateOpen_.begin(), gateOpen_.end(), 0);
        rng_.seed_(0x55AAu);
        holdAmt_ = 0.0f;
        captured_ = false;
        loud_ = 0;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(ssMix), snap);
        out_.setTarget(dsp::dbToGain(fp(ssOut)));
        if (snap) out_.snap(out_.target());
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float hopSec = static_cast<float>(hop_) / srf();
        // time smear: magnitude memory with a time constant up to ~4 s
        const float tc = 0.002f + 4.0f * std::pow(pct(ssTime), 2.0f);
        const float aT = std::exp(-hopSec / tc);
        // frequency smear: one-pole passes across bins, biased upwards / downwards
        const float sm = pct(ssSmear);
        const float bias = fp(ssBias) * 0.01f;
        const float base = 0.995f * std::pow(sm, 0.45f) * std::min(1.0f, static_cast<float>(n_) / 1024.0f);
        const float cu = base * std::clamp(1.0f + bias, 0.0f, 1.0f), cd = base * std::clamp(1.0f - bias, 0.0f, 1.0f);
        const float norm = 1.0f / (1.0f / (1.0f - cu) + 1.0f / (1.0f - cd) - 1.0f);
        const float scatter = pct(ssScatter) * dsp::kPi;
        const float gateDb = fp(ssGate);
        const bool gateOn = gateDb > -79.5f;
        const float gateLin = dsp::dbToGain(gateDb) * static_cast<float>(n_) * 0.25f;  // frame-magnitude scale
        const float disint = pct(ssDisint);
        const bool hold = bp(ssHold);
        if (!hold) captured_ = false;
        const float holdStep = hopSec / 0.25f;
        const int cutBin = static_cast<int>(fp(ssHighCut) / srf() * static_cast<float>(n_));
        bool captureNow = false;
        for (int c = 0; c < 2; ++c) {
            stft_[c].process(ch[c], n, [&](cfloat* X, int N) {
                auto& an = ana_[c];
                an.analyse(X);
                const int B = N / 2 + 1;
                auto& m = mem_[c];
                for (int k = 0; k < B; ++k) m[static_cast<size_t>(k)] = aT * m[static_cast<size_t>(k)] + (1.0f - aT) * an.mag[static_cast<size_t>(k)];
                // frequency smear (energy-normalised)
                auto& u = up_[c];
                auto& d = dn_[c];
                float acc = 0.0f;
                for (int k = 0; k < B; ++k) { acc = m[static_cast<size_t>(k)] + cu * acc; u[static_cast<size_t>(k)] = acc; }
                acc = 0.0f;
                for (int k = B - 1; k >= 0; --k) { acc = m[static_cast<size_t>(k)] + cd * acc; d[static_cast<size_t>(k)] = acc; }
                // per-frame state shared by both channels (stereo-coherent dropouts / gate) is advanced on c == 0
                if (c == 0) {
                    for (int k = 0; k < B; ++k) {
                        if (disint > 0.001f) {
                            // a bin drops out with a probability that grows with Disintegrate and returns after a while
                            if (aliveT_[static_cast<size_t>(k)] && rng_.uniform() < 0.25f * disint * disint) aliveT_[static_cast<size_t>(k)] = 0;
                            else if (!aliveT_[static_cast<size_t>(k)] && rng_.uniform() < 0.18f * (1.0f - 0.8f * disint)) aliveT_[static_cast<size_t>(k)] = 1;
                        } else {
                            aliveT_[static_cast<size_t>(k)] = 1;
                        }
                        alive_[static_cast<size_t>(k)] += 0.5f * (static_cast<float>(aliveT_[static_cast<size_t>(k)]) - alive_[static_cast<size_t>(k)]);
                        if (gateOn) {
                            const float v = m[static_cast<size_t>(k)];
                            if (!gateOpen_[static_cast<size_t>(k)] && v > gateLin * 1.6f) gateOpen_[static_cast<size_t>(k)] = 1;  // hysteresis
                            else if (gateOpen_[static_cast<size_t>(k)] && v < gateLin) gateOpen_[static_cast<size_t>(k)] = 0;
                        }
                    }
                    if (hold && !captured_) {
                        double e = 0.0;
                        for (int k = 0; k < B; ++k) e += m[static_cast<size_t>(k)];
                        loud_ = e > 1e-3 * N ? loud_ + 1 : 0;
                        if (loud_ >= 4) captureNow = true;
                    }
                }
                if (captureNow) {
                    for (int k = 0; k < B; ++k) {
                        held_[c][static_cast<size_t>(k)] = (u[static_cast<size_t>(k)] + d[static_cast<size_t>(k)] - m[static_cast<size_t>(k)]) * norm;
                        heldW_[c][static_cast<size_t>(k)] = an.instFreq[static_cast<size_t>(k)];
                        outPh_[c][static_cast<size_t>(k)] = an.phase[static_cast<size_t>(k)];
                    }
                }
                const float ha = holdAmt_;
                const float binW = dsp::kTwoPi / static_cast<float>(N);
                for (int k = 0; k < B; ++k) {
                    const size_t kk = static_cast<size_t>(k);
                    float mag = (u[kk] + d[kk] - m[kk]) * norm;
                    outPh_[c][kk] += static_cast<float>(hop_) * heldW_[c][kk];
                    // energy a bin borrowed from its neighbours sounds at the bin's own frequency (its own phase
                    // track); a bin's own energy keeps the analysis phase. Without this, smeared energy of a tone
                    // would re-collapse onto the tone in resynthesis.
                    ownPh_[c][kk] += static_cast<float>(hop_) * binW * static_cast<float>(k);
                    if (ownPh_[c][kk] > 1000.0f) ownPh_[c][kk] = std::fmod(ownPh_[c][kk], dsp::kTwoPi);
                    const float own = an.mag[kk];
                    const float borrow = mag > 1e-12f ? std::clamp(1.0f - own / mag, 0.0f, 1.0f) : 0.0f;
                    // stereo-coherent scatter: one random phase per bin and frame, shared by both channels (mono-safe)
                    if (c == 0) jit_[kk] = rng_.bipolar();
                    const float jit = scatter * jit_[kk];
                    cfloat y = std::polar(mag * (1.0f - borrow), an.phase[kk] + jit) + std::polar(mag * borrow, ownPh_[c][kk] + jit);
                    if (ha > 0.0f) y = (1.0f - ha) * y + ha * std::polar(held_[c][kk], outPh_[c][kk] + jit);
                    float g = alive_[kk];
                    if (gateOn && !gateOpen_[kk]) g *= 0.02f;
                    if (k > cutBin) g *= std::max(0.0f, 1.0f - static_cast<float>(k - cutBin) / (0.15f * static_cast<float>(B)));
                    X[k] = y * g;
                }
                dsp::Stft::mirror(X, N);
                if (c == 1) {
                    if (captureNow) { captured_ = true; captureNow = false; }
                    const float tgt = hold && captured_ ? 1.0f : 0.0f;
                    holdAmt_ = tgt > holdAmt_ ? std::min(tgt, holdAmt_ + holdStep) : std::max(tgt, holdAmt_ - holdStep);
                }
            });
        }
        for (int i = 0; i < n; ++i) {
            const float o = out_.next();
            ch[0][i] = softLimit(ch[0][i] * o, 2.0f);
            ch[1][i] = softLimit(ch[1][i] * o, 2.0f);
        }
        mix_.apply(ch, n);
    }
    int n_ = 2048, hop_ = 512;
    dsp::Stft stft_[2];
    dsp::SpectralAnalysis ana_[2];
    std::vector<float> mem_[2], held_[2], heldW_[2], outPh_[2], up_[2], dn_[2], ownPh_[2], alive_, jit_;
    std::vector<uint8_t> aliveT_, gateOpen_;
    dsp::Rng rng_{0x55AAu};
    float holdAmt_ = 0.0f;
    bool captured_ = false;
    int loud_ = 0;
    MixStage mix_;
    dsp::Smoother out_;
};

// ===================================================================================================== Spectral Delay
enum { sdSync, sdTime, sdDiv, sdShape, sdSpread, sdFeedback, sdDamp, sdOffset, sdTilt, sdOut, sdMix };
constexpr int kSdBands = 24;

class SpectralDelay final : public FxBase {
    void onPrepare() override {
        n_ = dsp::stftSizeFor(sr_, latMode_ == LatencyMode::Quality ? 1024 : 512);
        hop_ = n_ / 4;
        B_ = n_ / 2 + 1;
        frames_ = static_cast<int>(std::ceil(2.1 * sr_ / hop_)) + 2;
        for (int c = 0; c < 2; ++c) {
            stft_[c].prepare(n_, hop_);
            hist_[c].assign(static_cast<size_t>(frames_) * static_cast<size_t>(B_), cfloat());
        }
        bandOf_.assign(static_cast<size_t>(B_), 0);
        const float fLo = 40.0f, fHi = 0.45f * srf();
        for (int k = 0; k < B_; ++k) {
            const float f = std::max(fLo, static_cast<float>(k) * srf() / static_cast<float>(n_));
            const float t = std::log(f / fLo) / std::log(fHi / fLo);
            bandOf_[static_cast<size_t>(k)] = std::clamp(static_cast<int>(t * kSdBands), 0, kSdBands - 1);
        }
        mix_.prepare(sr_, maxBlock_, n_, MixLaw::Linear);
        out_.prepare(sr_, 30.0f);
    }
    int latencySamples() const override { return n_; }
    double tailSeconds() const override { return 2.0 + 10.0 * pct(sdFeedback); }
    void reset() override {
        for (int c = 0; c < 2; ++c) { stft_[c].reset(); std::fill(hist_[c].begin(), hist_[c].end(), cfloat()); }
        w_[0] = w_[1] = 0;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(sdMix), snap);
        out_.setTarget(dsp::dbToGain(fp(sdOut)));
        if (snap) out_.snap(out_.target());
    }
    float shape(int b, int shp) const {
        const float x = (static_cast<float>(b) + 0.5f) / kSdBands;
        switch (shp) {
            case 1: return 1.0f - x;                                   // falling: lows arrive last
            case 2: return std::fabs(2.0f * x - 1.0f);                 // V: mids first
            case 3: return (b % 2) ? 1.0f : 0.35f;                     // comb: alternating bands
            case 4: {                                                  // scatter: a fixed seeded pattern
                uint32_t h = static_cast<uint32_t>(b) * 2654435761u + 0x9E37u;
                h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
                return static_cast<float>(h & 0xFFFF) / 65535.0f;
            }
            default: return x;                                          // rising: highs arrive last
        }
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const double secs = syncedSeconds(bp(sdSync), fp(sdTime), ip(sdDiv), ctx.bpm);
        const float maxFrames = static_cast<float>(std::clamp(secs * sr_ / hop_, 1.0, static_cast<double>(frames_ - 2)));
        const float spread = pct(sdSpread);
        const float fb = std::min(0.95f, pct(sdFeedback) * 0.95f);
        const float damp = pct(sdDamp);
        const float off = pct(sdOffset);
        const float tilt = fp(sdTilt) * 0.01f;
        const int shp = ip(sdShape);
        int dly[2][kSdBands];
        float bfb[kSdBands], bg[kSdBands];
        for (int b = 0; b < kSdBands; ++b) {
            const float s = shape(b, shp);
            const float f = maxFrames * ((1.0f - spread) + spread * s);
            dly[0][b] = std::clamp(static_cast<int>(std::lround(f)), 1, frames_ - 2);
            dly[1][b] = std::clamp(static_cast<int>(std::lround(f * (1.0f + 0.25f * off * (b % 2 ? 1.0f : -0.6f)))), 1, frames_ - 2);
            const float x = (static_cast<float>(b) + 0.5f) / kSdBands;
            bfb[b] = fb * (1.0f - 0.6f * damp * x);
            bg[b] = std::clamp(1.0f + tilt * (2.0f * x - 1.0f), 0.0f, 2.0f);
        }
        for (int c = 0; c < 2; ++c) {
            auto& H = hist_[c];
            stft_[c].process(ch[c], n, [&](cfloat* X, int N) {
                const int wrow = w_[c];
                for (int k = 0; k < B_; ++k) {
                    const int b = bandOf_[static_cast<size_t>(k)];
                    const int r = ((wrow - dly[c][b]) % frames_ + frames_) % frames_;
                    const cfloat delayed = H[static_cast<size_t>(r) * static_cast<size_t>(B_) + static_cast<size_t>(k)];
                    cfloat in = X[k] + bfb[b] * delayed;
                    // bounded recirculation: soft-limit each bin's magnitude (no runaway at maximum feedback)
                    const float mag = std::abs(in), lim = 0.5f * static_cast<float>(N);
                    if (mag > lim) in *= (lim + std::tanh((mag - lim) / lim) * lim * 0.25f) / mag;
                    H[static_cast<size_t>(wrow) * static_cast<size_t>(B_) + static_cast<size_t>(k)] = in;
                    X[k] = delayed * bg[b];
                }
                dsp::Stft::mirror(X, N);
                w_[c] = (w_[c] + 1) % frames_;
            });
        }
        for (int i = 0; i < n; ++i) {
            const float o = out_.next();
            ch[0][i] = softLimit(ch[0][i] * o, 2.0f);
            ch[1][i] = softLimit(ch[1][i] * o, 2.0f);
        }
        mix_.apply(ch, n);
    }
    int n_ = 1024, hop_ = 256, B_ = 513, frames_ = 400;
    int w_[2] = {0, 0};
    dsp::Stft stft_[2];
    std::vector<cfloat> hist_[2];
    std::vector<int> bandOf_;
    MixStage mix_;
    dsp::Smoother out_;
};

// ========================================================================================================= Time Warp
enum { twMode, twSpeed, twStopMs, twCurve, twTrigger, twEngage, twDiv, twScrubRate, twScrubDepth, twKeepPitch, twTone, twMix };

class TimeWarp final : public FxBase {
    void onPrepare() override {
        int64_t size = 1;
        while (size < static_cast<int64_t>(12.0 * sr_)) size <<= 1;
        for (auto& b : buf_) b.assign(static_cast<size_t>(size), 0.0f);
        mask_ = size - 1;
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        speedS_.prepare(sr_, 25.0f);
        xfStep_ = static_cast<float>(1.0 / (0.012 * sr_));
        grain_ = static_cast<float>(0.07 * sr_);
    }
    void reset() override {
        for (auto& b : buf_) std::fill(b.begin(), b.end(), 0.0f);
        head_ = 0;
        rd_ = XfadeReader{};
        rd_.pos = -1.0;
        rd_.fadeStep = xfStep_;
        speed_ = 1.0f;
        stopT_ = 0.0f;
        engaged_ = false;
        segT_ = 0.0;
        segLen_ = 0.0;
        scrubPh_ = 0.0;
        samples_ = 0;
        gPh_ = 0.0f;
        anchor_[0] = anchor_[1] = 0.0;
        lastMode_ = -1;
        atLive_ = true;
        oldLive_ = false;
        exactAmt_ = 1.0f;
        keepAmt_ = 0.0f;
        for (auto& lp : tone_) lp.reset();
        mix_.reset();
        speedS_.snap(1.0f);
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(twMix), snap);
        for (auto& lp : tone_) lp.setup(dsp::Svf::Lowpass, std::min(fp(twTone), 0.45f * srf()), 0.6f, 0, sr_);
    }
    // is the effect engaged at this host position? (manual toggle or the last `div` of every trigger period)
    bool engagedAt(const ProcessContext& ctx, int i) const {
        const int trig = ip(twTrigger);
        if (trig == 0) return bp(twEngage);
        const double beats[4] = {1.0, 4.0, 8.0, 16.0};
        const double period = beats[std::clamp(trig - 1, 0, 3)];
        const double len = std::min(period * 0.75, dsp::divisionSeconds(ip(twDiv), ctx.bpm) * ctx.bpm / 60.0);
        double ppq;
        if (ctx.ppqValid) ppq = ctx.ppqPosition + i / sr_ * ctx.bpm / 60.0;
        else ppq = static_cast<double>(samples_ + i) / sr_ * ctx.bpm / 60.0;
        const double ph = std::fmod(ppq, period);
        return ph >= period - len;
    }
    float read(int c, double pos) const { return hermite(buf_[static_cast<size_t>(c)], mask_, pos); }
    // pitch-keeping read: two Hann grains, 50 % overlapped. Each grain starts at the playhead position of the moment
    // it (re)starts and then reads forward at rate 1, so pitch is kept while the playhead itself moves slower.
    void advanceGrains(double pos) {
        for (int v = 0; v < 2; ++v) {
            float ph = gPh_ + 0.5f * static_cast<float>(v);
            ph -= std::floor(ph);
            if (ph < 1.0f / grain_) anchor_[v] = pos - 1.0;  // this voice wraps: re-anchor at the playhead
        }
    }
    float readKeep(int c) const {
        float y = 0.0f;
        for (int v = 0; v < 2; ++v) {
            float ph = gPh_ + 0.5f * static_cast<float>(v);
            ph -= std::floor(ph);
            const float w = 0.5f - 0.5f * std::cos(dsp::kTwoPi * ph);
            y += w * read(c, std::min(anchor_[v] + static_cast<double>(ph * grain_), static_cast<double>(head_) - 4.0));
        }
        return y;
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const int mode = ip(twMode);
        const bool keep = bp(twKeepPitch) && (mode == 0 || mode == 2);
        const float stopSamples = std::max(32.0f, fp(twStopMs) * 0.001f * srf());
        const float curve = 0.5f + 2.5f * pct(twCurve);
        const double minLag = 6.0, maxLag = static_cast<double>(mask_) * 0.85;
        for (int i = 0; i < n; ++i) {
            // capture first; the read head always stays behind the write head
            buf_[0][static_cast<size_t>(head_ & mask_)] = ch[0][i];
            buf_[1][static_cast<size_t>(head_ & mask_)] = ch[1][i];
            ++head_;
            const double live = static_cast<double>(head_) - minLag;
            if (rd_.pos < 0.0) { rd_.pos = live; rd_.oldPos = live; }
            const bool on = engagedAt(ctx, i);
            double target = 1.0;
            if (mode == 0) {  // Varispeed
                target = on || ip(twTrigger) == 0 ? fp(twSpeed) : 1.0;
                if (ip(twTrigger) == 0 && !bp(twEngage)) target = fp(twSpeed);  // free varispeed when nothing triggers
            } else if (mode == 1 || mode == 4) {  // Tape Stop / Backspin
                if (on && !engaged_) { engaged_ = true; stopT_ = 0.0f; }
                if (!on && engaged_) {
                    engaged_ = false;
                    rd_.jump(live, 1.0);  // release: back to live, crossfaded
                }
                if (engaged_) {
                    stopT_ += 1.0f;
                    const float t = std::min(1.0f, stopT_ / stopSamples);
                    target = mode == 1 ? std::pow(1.0f - t, curve) : 1.0 - 3.5 * std::pow(t, 1.0f / curve);
                    if (mode == 4 && t >= 1.0f) target = 0.0;
                }
            } else if (mode == 2) {  // Half-Time: each segment of length L is played over 2L at half speed
                const double L = std::max(0.02, dsp::divisionSeconds(ip(twDiv), ctx.bpm)) * sr_;
                if (segT_ <= 0.0 || segT_ >= 2.0 * L || std::fabs(segLen_ - L) > 1.0) {
                    segLen_ = L;
                    segT_ = 0.0;
                    rd_.jump(std::max({live - L, static_cast<double>(head_) - maxLag, 4.0}), 0.5);
                }
                segT_ += 1.0;
                target = 0.5;
                if (!on && ip(twTrigger) != 0) target = 1.0;
            }
            speedS_.setTarget(static_cast<float>(target));
            speed_ = speedS_.next();
            double rate = speed_;
            if (mode == 3) {  // Scrub: the read position swings behind live like a hand on a record
                const double depth = std::max(1.0, fp(twScrubDepth) * 0.001 * sr_);
                scrubPh_ += fp(twScrubRate) / sr_;
                if (scrubPh_ >= 1.0) scrubPh_ -= 1.0;
                const double want = std::max(4.0, live - depth * (0.55 + 0.45 * std::sin(dsp::kTwoPi * scrubPh_)));
                if (lastMode_ != 3) rd_.jump(want, 1.0);  // start on the swing (crossfaded), not a 4x catch-up
                rate = 1.0 + (want - rd_.pos) * 0.002;  // follow smoothly (a pitch-bending scratch)
                rate = std::clamp(rate, -4.0, 4.0);
            }
            if (lastMode_ >= 0 && lastMode_ != mode && mode != 3) rd_.jump(live, 1.0);  // mode change: back to live, crossfaded
            lastMode_ = mode;
            rd_.rate = rate;
            rd_.pos += rate;
            rd_.oldPos += rd_.oldRate;
            // bounds: never read the future; lagging too far behind re-anchors to live (crossfaded)
            if (rd_.pos > live) { rd_.pos = live; }
            // V4 Phase 4: never read before the first recorded sample (the empty ring is silence, so swinging across
            // that edge was a jump from nothing to signal right after a start or reset)
            const double oldest = std::max(static_cast<double>(head_) - maxLag, 4.0);
            if (rd_.pos < oldest) {
                if (static_cast<double>(head_) - maxLag > 4.0) rd_.jump(live, 1.0);
                else rd_.pos = oldest;
            }
            if (rd_.oldPos < 4.0) rd_.oldPos = 4.0;
            if (rd_.oldPos > live) rd_.oldPos = live;
            // varispeed faster than 1x has caught up with the input: it stays at live (no future to read)
            float yl, yr;
            // at rest (a triggered mode that is not engaged, back at live): the input itself, zero latency
            const bool rest = (mode == 1 || mode == 4 || (mode == 2 && !on && ip(twTrigger) != 0)) && !engaged_ && std::fabs(rate - 1.0) < 1e-6;
            rd_.service();
            if (rest) {
                if (!atLive_) { if (rd_.fade >= 1.0f && !rd_.pending) rd_.jump(live, 1.0); oldLive_ = false; atLive_ = true; }
                rd_.pos = live;
                yl = ch[0][i];
                yr = ch[1][i];
            } else {
                // the pitch-keeping grains always run (fresh anchors), and are crossfaded in and out with a 12 ms ramp
                // instead of switching when the smoothed speed crosses 1x
                gPh_ += 1.0f / grain_;
                if (gPh_ >= 1.0f) gPh_ -= 1.0f;
                advanceGrains(rd_.pos);
                const float kt = (keep && std::fabs(rate) < 0.999) ? 1.0f : 0.0f;
                keepAmt_ = kt > keepAmt_ ? std::min(kt, keepAmt_ + xfStep_) : std::max(kt, keepAmt_ - xfStep_);
                yl = read(0, rd_.pos);
                yr = read(1, rd_.pos);
                if (keepAmt_ > 0.0f) {
                    const float k = 0.5f - 0.5f * std::cos(dsp::kPi * keepAmt_);
                    yl += k * (readKeep(0) - yl);
                    yr += k * (readKeep(1) - yr);
                }
            }
            if (!rest && atLive_) {  // leaving live: crossfade from the direct input into the buffer read
                rd_.fade = 0.0f;
                oldLive_ = true;
                atLive_ = false;
            }
            if (rd_.fade < 1.0f) {
                const float a = 0.5f - 0.5f * std::cos(dsp::kPi * rd_.fade);  // zero-slope equal-power time base
                const float ga = std::sin(0.5f * dsp::kPi * a), gb = std::cos(0.5f * dsp::kPi * a);
                const float ol = oldLive_ ? ch[0][i] : read(0, rd_.oldPos);
                const float orr = oldLive_ ? ch[1][i] : read(1, rd_.oldPos);
                yl = ga * yl + gb * ol;
                yr = ga * yr + gb * orr;
                rd_.fade = std::min(1.0f, rd_.fade + xfStep_);
                // V4 Phase 4: "the outgoing source is the live input" holds only for the crossfade that left live;
                // it was never cleared, so a later release crossfaded from the input instead of the playhead (a jump)
                if (rd_.fade >= 1.0f) oldLive_ = false;
            }
            // a stopping tape loses level as it slows (and a held tape is silent, not a DC step)
            const float amp = (mode == 1 || mode == 4) ? std::clamp(static_cast<float>(std::fabs(rate)) * 3.0f, 0.0f, 1.0f) : 1.0f;
            // Tone colours the warped playback only; at rest the input passes untouched (the filter keeps running so
            // it is warm when the warp starts)
            const float tl = tone_[0].process(yl * amp), tr = tone_[1].process(yr * amp);
            const bool exact = rest && rd_.fade >= 1.0f;
            exactAmt_ = exact ? std::min(1.0f, exactAmt_ + xfStep_) : std::max(0.0f, exactAmt_ - xfStep_);
            ch[0][i] = exactAmt_ * yl + (1.0f - exactAmt_) * tl;
            ch[1][i] = exactAmt_ * yr + (1.0f - exactAmt_) * tr;
        }
        samples_ += n;
        for (auto& lp : tone_) lp.flush();
        mix_.apply(ch, n);
    }
    std::array<std::vector<float>, 2> buf_;
    int64_t mask_ = 0, head_ = 0, samples_ = 0;
    XfadeReader rd_;
    float speed_ = 1.0f, stopT_ = 0.0f, xfStep_ = 0.01f, grain_ = 3000.0f, gPh_ = 0.0f, exactAmt_ = 1.0f, keepAmt_ = 0.0f;
    bool engaged_ = false, atLive_ = false, oldLive_ = false;
    int lastMode_ = -1;
    double segT_ = 0.0, segLen_ = 0.0, scrubPh_ = 0.0;
    double anchor_[2] = {0.0, 0.0};
    dsp::Svf tone_[2];
    dsp::Smoother speedS_;
    MixStage mix_;
};

// ====================================================================================================== Glitch Scatter
enum { glDiv, glRepeat, glRatchet, glReverse, glJump, glRange, glPitch, glGate, glLofi, glSeed, glMix };

class GlitchScatter final : public FxBase {
    enum Action { Pass, Repeat, Reverse, Jump, PitchUp, PitchDown, Gate };
    void onPrepare() override {
        int64_t size = 1;
        while (size < static_cast<int64_t>(10.0 * sr_)) size <<= 1;
        for (auto& b : buf_) b.assign(static_cast<size_t>(size), 0.0f);
        mask_ = size - 1;
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Linear);
        rr_.prepare(sr_, 0x61u);
        xfLen_ = static_cast<float>(0.003 * sr_);
        tmpL_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        tmpR_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        lofL_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        lofR_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        lofiRamp_.assign(static_cast<size_t>(maxBlock_), 0.0f);
    }
    void reset() override {
        for (auto& b : buf_) std::fill(b.begin(), b.end(), 0.0f);
        head_ = 0;
        samples_ = 0;
        lastStep_ = -1;
        act_ = Pass;
        pos_ = oldPos_ = 0.0;
        rate_ = oldRate_ = 1.0;
        fade_ = 1.0f;
        fragT_ = 0.0;
        fragLen_ = 1.0;
        fragStart_ = 0.0;
        lofi_ = false;
        lofiAmt_ = 0.0f;
        curGate_ = oldGain_ = 1.0f;
        gateU_ = 0.0f;
        rr_.reset();
        mix_.reset();
    }
    void applyParams(bool snap) override { mix_.setMix(pct(glMix), snap); }
    static uint32_t hash(uint32_t a, uint32_t b) {
        uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
        h ^= h >> 16; h *= 0x85EBCA6Bu; h ^= h >> 13; h *= 0xC2B2AE35u; h ^= h >> 16;
        return h;
    }
    void startFragment(Action a, double newPos, double newRate, double len) {
        // V4 Phase 4: the outgoing signal keeps the gain it was actually heard at (a Gate step is silent), so the
        // crossfade never starts from a level that was not audible
        oldGain_ = 1.0f;  // the gate level is applied after the crossfade (gateU_), so both sides stay at unity here
        oldLive_ = act_ == Pass;
        oldPos_ = pos_;
        oldRate_ = rate_;
        pos_ = newPos;
        rate_ = newRate;
        fade_ = 0.0f;
        act_ = a;
        fragT_ = 0.0;
        fragLen_ = std::max(64.0, len);
        fragStart_ = newPos;
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const double stepSec = std::max(0.01, dsp::divisionSeconds(ip(glDiv), ctx.bpm));
        const double step = stepSec * sr_;
        const uint32_t seed = static_cast<uint32_t>(ip(glSeed));
        const float pRep = pct(glRepeat), pRev = pct(glReverse), pJmp = pct(glJump), pPit = pct(glPitch), pGat = pct(glGate);
        const int maxRatchet = std::clamp(ip(glRatchet), 1, 8);
        const double range = std::max(1.0, static_cast<double>(fp(glRange))) * 60.0 / ctx.bpm * sr_;
        const float lofi = pct(glLofi);
        for (int i = 0; i < n; ++i) {
            const float inL = ch[0][i], inR = ch[1][i];
            buf_[0][static_cast<size_t>(head_ & mask_)] = inL;
            buf_[1][static_cast<size_t>(head_ & mask_)] = inR;
            ++head_;
            const double live = static_cast<double>(head_) - 4.0;
            // grid position: the host timeline when it plays (repeatable), a free clock otherwise
            double beatPos;
            if (ctx.ppqValid && ctx.hostPlaying) beatPos = (ctx.ppqPosition + i / sr_ * ctx.bpm / 60.0) * 60.0 / ctx.bpm * sr_ / step;
            else beatPos = static_cast<double>(samples_ + i) / step;
            const int64_t stepIdx = static_cast<int64_t>(std::floor(beatPos));
            if (stepIdx != lastStep_) {
                lastStep_ = stepIdx;
                const uint32_t h = hash(seed, static_cast<uint32_t>(stepIdx & 0xFFFFFFFF));
                const float r = static_cast<float>(h & 0xFFFF) / 65536.0f;
                const float r2 = static_cast<float>((h >> 16) & 0xFFFF) / 65536.0f;
                const float sum = pRep + pRev + pJmp + pPit + pGat;
                float u = r * std::max(1.0f, sum);
                lofi_ = r2 < lofi;
                const bool havePast = head_ > static_cast<int64_t>(2.5 * step) + 64;
                if (havePast && u < pRep) {
                    const int rat = 1 + static_cast<int>(r2 * static_cast<float>(maxRatchet));
                    startFragment(Repeat, live - step, 1.0, step / rat);
                } else if (havePast && (u -= pRep) < pRev) {
                    startFragment(Reverse, live - 1.0, -1.0, step);
                } else if (havePast && (u -= pRev) < pJmp) {
                    const double back = std::min(range, static_cast<double>(head_) - step - 64.0) * (0.25 + 0.75 * r2);
                    startFragment(Jump, live - back, 1.0, step);
                } else if (havePast && (u -= pJmp) < pPit) {
                    const bool upP = r2 < 0.5f;
                    startFragment(upP ? PitchUp : PitchDown, live - (upP ? 2.0 * step : step), upP ? 2.0 : 0.5, step);
                } else if (havePast && (u -= pPit) < pGat) {
                    startFragment(Gate, live, 1.0, step);
                } else if (act_ != Pass) {
                    startFragment(Pass, live, 1.0, step);
                    lofi_ = false;
                }
            }
            // micro-loop: a Repeat fragment restarts every fragLen (ratchets), with its own crossfade
            if (act_ == Repeat && fragT_ >= fragLen_) {
                oldLive_ = false;
                oldPos_ = pos_;
                oldRate_ = rate_;
                pos_ = fragStart_;
                fade_ = 0.0f;
                fragT_ = 0.0;
            }
            // Pass is the live input itself (zero latency); fragments read the capture buffer
            float yl, yr;
            if (act_ == Pass) { yl = inL; yr = inR; pos_ = live; }
            else { yl = hermite(buf_[0], mask_, std::min(pos_, live)); yr = hermite(buf_[1], mask_, std::min(pos_, live)); }
            if (fade_ < 1.0f) {
                // equal-power law on a raised-cosine time base: zero slope at both ends (no corner at the splice)
                const float a = 0.5f - 0.5f * std::cos(dsp::kPi * fade_);
                const float ga = std::sin(0.5f * dsp::kPi * a), gb = std::cos(0.5f * dsp::kPi * a);
                float ol, orr;
                if (oldLive_) { ol = inL; orr = inR; }
                else { ol = hermite(buf_[0], mask_, std::min(oldPos_, live)); orr = hermite(buf_[1], mask_, std::min(oldPos_, live)); }
                ol *= oldGain_;
                orr *= oldGain_;
                yl = ga * yl + gb * ol;
                yr = ga * yr + gb * orr;
                fade_ = std::min(1.0f, fade_ + 1.0f / xfLen_);
                oldPos_ += oldRate_;
            }
            // gate level is a continuous state (raised-cosine ramp towards silence on Gate steps, back up otherwise):
            // consecutive Gate steps stay silent instead of restarting from full level
            gateU_ = act_ == Gate ? std::min(1.0f, gateU_ + 1.0f / xfLen_) : std::max(0.0f, gateU_ - 1.0f / xfLen_);
            const float g = 0.5f + 0.5f * std::cos(dsp::kPi * gateU_);
            curGate_ = g;
            tmpL_[static_cast<size_t>(i)] = yl * g;
            tmpR_[static_cast<size_t>(i)] = yr * g;
            // lo-fi amount follows the step decision per sample with a 3 ms ramp (was switched per block)
            const float lofiTarget = (lofi_ && act_ != Pass) ? 1.0f : 0.0f;
            lofiAmt_ += (lofiTarget > lofiAmt_ ? 1.0f : -1.0f) * std::min(std::fabs(lofiTarget - lofiAmt_), 1.0f / xfLen_);
            lofiRamp_[static_cast<size_t>(i)] = lofiAmt_;
            if (act_ != Pass) pos_ += rate_;
            fragT_ += 1.0;
        }
        // lo-fi character only on glitched steps (the shared RateReducer), crossfaded per sample: the reducer runs on a
        // copy whenever any sample of the block wants it, so switching it in or out is never a step
        // the reducer runs continuously on a copy (warm hold / reconstruction state: a cold start inside the fade-in was
        // a step response) and is crossfaded in per sample
        {
            std::copy(tmpL_.begin(), tmpL_.begin() + n, lofL_.begin());
            std::copy(tmpR_.begin(), tmpR_.begin() + n, lofR_.begin());
            dsp::RateReducer::Settings s;
            s.rateHz = 6000.0f;
            s.bits = 9.0f;
            s.reconstruct = dsp::RateReducer::FilterSoft;
            rr_.process(lofL_.data(), lofR_.data(), n, s);
            for (int i = 0; i < n; ++i) {
                const float a = lofiRamp_[static_cast<size_t>(i)];
                if (a <= 0.0f) continue;
                tmpL_[static_cast<size_t>(i)] += a * (lofL_[static_cast<size_t>(i)] - tmpL_[static_cast<size_t>(i)]);
                tmpR_[static_cast<size_t>(i)] += a * (lofR_[static_cast<size_t>(i)] - tmpR_[static_cast<size_t>(i)]);
            }
        }
        std::copy(tmpL_.begin(), tmpL_.begin() + n, ch[0]);
        std::copy(tmpR_.begin(), tmpR_.begin() + n, ch[1]);
        samples_ += n;
        mix_.apply(ch, n);
    }
    std::array<std::vector<float>, 2> buf_;
    std::vector<float> tmpL_, tmpR_, lofL_, lofR_, lofiRamp_;
    float lofiAmt_ = 0.0f, curGate_ = 1.0f, oldGain_ = 1.0f, gateU_ = 0.0f;
    int64_t mask_ = 0, head_ = 0, samples_ = 0, lastStep_ = -1;
    Action act_ = Pass;
    double pos_ = 0, oldPos_ = 0, rate_ = 1, oldRate_ = 1, fragT_ = 0, fragLen_ = 1, fragStart_ = 0;
    float fade_ = 1.0f, xfLen_ = 144.0f;
    bool lofi_ = false, oldLive_ = true;
    dsp::RateReducer rr_;
    MixStage mix_;
};

}  // namespace

// ======================================================================================================= descriptors
FxDescriptor describeDegrade() {
    FxDescriptor d;
    d.typeId = "degrade";
    d.name = "Degrade";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Old-sampler and broken-device degradation: narrows the bandwidth, lowers the sample rate and bit depth, and lets the clock wobble.";
    d.params = {
        P::logf("rate", "Sample Rate", "Hz", 300.0f, 48000.0f, 12000.0f, "Effective sample rate of the hold stage. Lower = grittier; with Anti-alias Off, highs fold back as metallic aliases."),
        P::flt("bits", "Bits", "", 2.0f, 24.0f, 12.0f, "Amplitude resolution, separate from the rate. Fewer bits = rougher quantisation noise and distortion."),
        P::pct("jitter", "Clock Jitter", 10.0f, "An unstable sample clock: each held sample lasts a slightly different time."),
        P::choice("anti_alias", "Anti-alias", {"Off", "Soft", "Strong"}, 0, "Off: raw aliasing (gritty, inharmonic). Soft / Strong: filter before the rate reduction, the clean (darker) decimation path."),
        P::choice("reconstruct", "Reconstruction", {"Off", "Soft", "Strong"}, 1, "Smooths the hold steps afterwards, like a sampler's output filter. Off = raw staircase edge."),
        P::pct("bandwidth", "Bandwidth Collapse", 30.0f, "Squeezes the spectrum from both ends towards a narrow, boxy band (telephone, radio, worn sampler)."),
        P::flt("drive", "Drive", "dB", 0.0f, 24.0f, 6.0f, "Pushes the input into an asymmetric clipper before it is degraded."),
        P::pct("wobble", "Wobble", 10.0f, "Slow drift of the sample rate: an unsteady, warped digital texture."),
        P::pct("noise", "Noise", 8.0f, "A low noise floor, like old converters and tape transfers."),
        P::choice("stereo", "Stereo", {"Linked", "Independent"}, 0, "Independent clocks for left and right make the degradation wider and less stable."),
        P::flt("out", "Output", "dB", -24.0f, 12.0f, 0.0f, "Final level."),
        P::pct("mix", "Mix", 100.0f, "Blend of clean and degraded sound."),
    };
    for (int i : {dgJitter, dgRecon, dgWobble, dgNoise, dgStereo}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Degrade>(); };
    return d;
}

FxDescriptor describeFoldCrush() {
    FxDescriptor d;
    d.typeId = "fold_crush";
    d.name = "Fold & Crush";
    d.family = FxFamily::ToneDynamics;
    d.tooltip = "Dynamic wavefolding that bites harder as the sound gets louder, with a crusher aimed at the transients or the sustain.";
    d.params = {
        P::flt("drive", "Drive", "dB", 0.0f, 30.0f, 12.0f, "How hard the signal hits the folder."),
        P::pct("fold", "Fold", 50.0f, "0% = soft clipping only; more folds the waveform back on itself: metallic, synth-like overtones."),
        P::flt("bias", "Asymmetry", "%", -100.0f, 100.0f, 15.0f, "Offsets the folding point: uneven harmonics, a fuzzier, gated edge."),
        P::flt("dynamics", "Dynamics", "%", -100.0f, 100.0f, 40.0f, "Positive: loud parts fold harder. Negative: quiet parts break up more (a blooming, swelling distortion)."),
        P::pct("crush", "Crush", 25.0f, "Amount of sample-rate and bit crushing blended in on the chosen part of the sound."),
        P::choice("target", "Crush Target", {"Transients", "Sustain"}, 0, "Crush only the attacks (splintered hits) or only the body between them."),
        P::logf("tone", "Tone", "Hz", 1000.0f, 20000.0f, 11000.0f, "Low-pass after the folder: tames the fizz."),
        P::toggle("auto_gain", "Auto Gain", true, "Keeps the output close to the input level so the character, not the volume, changes."),
        P::flt("out", "Output", "dB", -24.0f, 12.0f, 0.0f, "Final level."),
        P::pct("mix", "Mix", 100.0f, "Blend of clean and folded sound."),
    };
    for (int i : {fcBias, fcTarget, fcAuto}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<FoldCrush>(); };
    return d;
}

FxDescriptor describeSpectralSmear() {
    FxDescriptor d;
    d.typeId = "spectral_smear";
    d.name = "Spectral Smear";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Pulls the sound apart in the frequency domain: smears it across pitch and time, scatters its phase, gates and disintegrates its partials.";
    d.params = {
        P::pct("smear", "Smear", 45.0f, "Spreads each frequency into its neighbours: from a soft blur to a dense cloud with no clear pitch."),
        P::flt("bias", "Smear Bias", "%", -100.0f, 100.0f, 0.0f, "Spread more upwards (+, airy) or downwards (-, darker)."),
        P::pct("time", "Time Smear", 40.0f, "Spectral memory: each frequency lingers and melts into the next moment (up to seconds)."),
        P::pct("scatter", "Phase Scatter", 25.0f, "Randomises phases: dissolves attacks and harmonic identity into a breathy, diffuse texture."),
        P::flt("gate", "Spectral Gate", "dB", -80.0f, 0.0f, -80.0f, "Silences frequencies below this level (with hysteresis): leaves only the strongest partials. -80 dB = off."),
        P::pct("disintegrate", "Disintegrate", 0.0f, "Random partials drop out and return: the sound tears and sparkles apart."),
        P::toggle("hold", "Hold", false, "Holds the current smeared spectrum (waits for real sound first, crossfades in and out)."),
        P::logf("high_cut", "High Cut", "Hz", 1000.0f, 20000.0f, 18000.0f, "Softens the top of the spectral texture."),
        P::flt("out", "Output", "dB", -24.0f, 12.0f, 0.0f, "Level of the spectral signal."),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and the smeared spectrum."),
    };
    for (int i : {ssBias, ssHighCut, ssOut}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<SpectralSmear>(); };
    return d;
}

FxDescriptor describeSpectralDelay() {
    FxDescriptor d;
    d.typeId = "spectral_delay";
    d.name = "Spectral Delay";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Every frequency band gets its own delay: echoes that rise, fall or scatter across the spectrum, with per-band feedback.";
    d.params = {
        P::toggle("sync", "Sync", true, "Locks the longest band delay to the song tempo."),
        P::logf("time_ms", "Time", "ms", 20.0f, 2000.0f, 450.0f, "The longest band delay when Sync is off."),
        P::choice("division", "Division", divisionLabels(), 10, "The longest band delay as a note value when Sync is on."),
        P::choice("shape", "Shape", {"Rising", "Falling", "V", "Comb", "Scatter"}, 0, "How delay times are spread over the bands: highs last, lows last, mids first, alternating bands, or a fixed scatter."),
        P::pct("spread", "Spread", 80.0f, "How different the band delays are. 0% = all bands together (a plain spectral echo)."),
        P::pct("feedback", "Feedback", 45.0f, "Per-band repeats. Safely limited, never runs away."),
        P::pct("damping", "High Damping", 40.0f, "High bands repeat less than low bands."),
        P::pct("offset", "Stereo Offset", 30.0f, "Left and right band delays differ: a wide, moving image."),
        P::flt("tilt", "Tilt", "%", -100.0f, 100.0f, 0.0f, "Emphasises the high (+) or low (-) echoes."),
        P::flt("out", "Output", "dB", -24.0f, 12.0f, 0.0f, "Level of the echoes."),
        P::pct("mix", "Mix", 60.0f, "Blend of original and spectral echoes."),
    };
    for (int i : {sdOffset, sdTilt, sdOut}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<SpectralDelay>(); };
    return d;
}

FxDescriptor describeTimeWarp() {
    FxDescriptor d;
    d.typeId = "time_warp";
    d.name = "Time Warp";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Bends time on a moving playhead: varispeed, tape stops, half-time, record scrubbing and backspins.";
    d.params = {
        P::choice("mode", "Mode", {"Varispeed", "Tape Stop", "Half-Time", "Scrub", "Backspin"}, 0, "Varispeed: speed and pitch move together like tape. Tape Stop: winds down to silence. Half-Time: each slice plays at half speed. Scrub: swings back and forth like a hand on a record. Backspin: a fast reverse spin-out."),
        P::flt("speed", "Speed", "x", -2.0f, 2.0f, 0.5f, "Varispeed playback rate (negative = backwards). A live input has no future: faster than 1x catches up and then follows."),
        P::logf("stop_ms", "Stop Time", "ms", 50.0f, 4000.0f, 700.0f, "How long a tape stop or backspin takes."),
        P::pct("curve", "Curve", 50.0f, "Shape of the slow-down: early (heavy reel) to late (sudden brake)."),
        P::choice("trigger", "Trigger", {"Manual", "Every Beat", "Every Bar", "Every 2 Bars", "Every 4 Bars"}, 0, "Manual: Engage starts it. Grid: it happens at the end of every period, for the length of Division."),
        P::toggle("engage", "Engage", false, "Manual trigger: on = stop / spin / half-time now; off = return to live (crossfaded)."),
        P::choice("division", "Division", divisionLabels(), 10, "Half-Time slice length, and how long a grid-triggered event lasts."),
        P::logf("scrub_rate", "Scrub Rate", "Hz", 0.1f, 12.0f, 1.5f, "How fast the scrub swings."),
        P::logf("scrub_depth", "Scrub Depth", "ms", 5.0f, 800.0f, 120.0f, "How far back the scrub swings."),
        P::toggle("keep_pitch", "Keep Pitch", false, "Varispeed / Half-Time: slow down without dropping in pitch (granular read; grainier)."),
        P::logf("tone", "Tone", "Hz", 1000.0f, 20000.0f, 18000.0f, "Darkens the warped playback."),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and the warped playback."),
    };
    for (int i : {twCurve, twTone}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<TimeWarp>(); };
    return d;
}

FxDescriptor describeGlitchScatter() {
    FxDescriptor d;
    d.typeId = "glitch";
    d.name = "Glitch Scatter";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Chops the sound on the beat grid into micro-loops, reversed pieces, jumps, pitched fragments and gaps. Repeatable: the same step always does the same thing.";
    d.params = {
        P::choice("division", "Step", divisionLabels(), 6, "Grid step length."),
        P::pct("repeat", "Repeat", 30.0f, "Chance a step becomes a micro-loop (with ratchets)."),
        P::flt("ratchet", "Ratchet", "x", 1.0f, 8.0f, 4.0f, "Most repeats inside one repeated step: faster rolls and buzzes.", ParamCurve::Linear),
        P::pct("reverse", "Reverse", 15.0f, "Chance a step plays backwards."),
        P::pct("jump", "Jump", 15.0f, "Chance a step jumps back to an earlier moment (within Range)."),
        P::flt("range", "Range", "beats", 1.0f, 16.0f, 4.0f, "How far back a jump may go."),
        P::pct("pitch", "Pitch", 10.0f, "Chance a step is replayed an octave up or down."),
        P::pct("gate", "Gate", 10.0f, "Chance a step drops out."),
        P::pct("lofi", "Lo-Fi", 20.0f, "Chance a glitched step is also sample-rate and bit crushed."),
        P::flt("seed", "Pattern", "", 0.0f, 99.0f, 7.0f, "Which glitch pattern: each number is a different, repeatable sequence.", ParamCurve::Linear),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and the glitched sound."),
    };
    for (int i : {glRange, glLofi, glSeed}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<GlitchScatter>(); };
    return d;
}

}  // namespace ih1::fx
