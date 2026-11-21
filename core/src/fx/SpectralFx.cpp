// Spectral family: Harmony, Pitch Shift / Octaver, Formant Shift, Spectral Freeze / Blur.
// Shared engine: ih1/dsp/Spectral.h (Laroche-Dolson peak shifting, true envelope, YIN).
// Harmony is informed by the documented Vocalist Workstation EX vocabulary (brief §2.2): four MIDI- or
// scale-directed voices, formant character independent of interval, detune/vibrato/scoop(glide), a
// meaningful dry/harmony balance. It is an IH1 design, not a model of that hardware.
#include <array>

#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Spectral.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

using dsp::cfloat;

// ===================================================================================== scales
constexpr int kScales[8][12] = {
    {0, 2, 4, 5, 7, 9, 11, -1}, {0, 2, 3, 5, 7, 8, 10, -1}, {0, 2, 3, 5, 7, 9, 10, -1}, {0, 2, 4, 5, 7, 9, 10, -1},
    {0, 2, 3, 5, 7, 8, 11, -1}, {0, 2, 4, 7, 9, -1}, {0, 3, 5, 7, 10, -1}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}};
int scaleLen(int s) { int n = 0; while (n < 12 && kScales[s][n] >= 0 && (n == 0 || kScales[s][n] > kScales[s][n - 1])) ++n; return n; }

// degree index (can be negative) of the scale note nearest to midi m
int nearestDegree(float m, int key, int scale) {
    const int len = scaleLen(scale);
    int bestDeg = 0;
    float bestDist = 1e9f;
    const int oct0 = static_cast<int>(std::floor((m - key) / 12.0f)) - 1;
    for (int o = oct0; o <= oct0 + 2; ++o)
        for (int i = 0; i < len; ++i) {
            const float note = static_cast<float>(key + 12 * o + kScales[scale][i]);
            const float d = std::fabs(note - m);
            if (d < bestDist) { bestDist = d; bestDeg = o * len + i; }
        }
    return bestDeg;
}
int degreeToMidi(int deg, int key, int scale) {
    const int len = scaleLen(scale);
    const int o = static_cast<int>(std::floor(static_cast<float>(deg) / len));
    const int i = deg - o * len;
    return key + 12 * o + kScales[scale][i];
}

// ===================================================================================== Harmony
enum {
    hMode, hKey, hScale, hVoices, hInt1, hInt2, hInt3, hInt4, hLvl1, hLvl2, hLvl3, hLvl4, hPan1, hPan2, hPan3, hPan4,
    hCharacter, hPreserve, hDetune, hVibDepth, hVibRate, hGlide, hHumanize, hPriority, hDry, hWet
};

class Harmony final : public FxBase {
    static constexpr int kV = 4;
    void onPrepare() override {
        // Low latency: a quarter-length window (512 points at 44.1/48 kHz, ~11 ms instead of ~43 ms)
        n_ = dsp::stftSizeFor(sr_, latMode_ == LatencyMode::Quality ? 2048 : 512);
        hop_ = n_ / 4;
        stft_.prepare(n_, hop_);
        ana_.prepare(n_, hop_);
        shifter_.prepare(n_, hop_);
        yin_.prepare(sr_, sr_ > 60000 ? 4 : 2, 512);
        mix_.prepare(sr_, maxBlock_, n_, MixLaw::Linear);
        mix_.setMix(1.0f, true);
        mono_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        hl_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        hr_.assign(static_cast<size_t>(maxBlock_), 0.0f);
        for (int v = 0; v < kV; ++v) { drift_[v].prepare(sr_ / hop_, 500 + static_cast<uint32_t>(v)); drift_[v].setShape(dsp::Lfo::Drift); drift_[v].setRate(1.0f); }
        dryG_.prepare(sr_, 30.0f);
        wetG_.prepare(sr_, 30.0f);
    }
    int latencySamples() const override { return n_; }
    double tailSeconds() const override { return 0.1; }
    void reset() override {
        stft_.reset();
        ana_.reset();
        shifter_.reset();
        mix_.reset();
        for (auto& v : voice_) v = Voice{};
        sustain_ = false;
        bend_ = 0.0f;
        lastF0_ = 0.0f;
        unvoicedFrames_ = 1000;
        ageCounter_ = 0;
        for (int v = 0; v < kV; ++v) { logRatio_[v] = 0.0f; vibPhase_[v] = 0.25f * v; gain_[v] = 0.0f; }
        frameCount_ = 0;
    }
    void applyParams(bool snap) override {
        dryG_.setTarget(fp(hDry) <= -59.9f ? 0.0f : dsp::dbToGain(fp(hDry)));
        wetG_.setTarget(fp(hWet) <= -59.9f ? 0.0f : dsp::dbToGain(fp(hWet)));
        if (snap) { dryG_.snap(dryG_.target()); wetG_.snap(wetG_.target()); }
    }

    // ---- MIDI voice allocation (Isai rules: prefer silent voices; deterministic priority) -------------
    struct Voice { int note = -1; bool held = false; bool sustained = false; uint32_t age = 0; };
    void noteOn(int note) {
        const int count = ip(hVoices) + 1;
        for (int v = 0; v < count; ++v) if (voice_[v].note == note && (voice_[v].held || voice_[v].sustained)) { voice_[v].held = true; voice_[v].age = ++ageCounter_; return; }
        int slot = -1;
        for (int v = 0; v < count && slot < 0; ++v) if (!voice_[v].held && !voice_[v].sustained && gain_[v] < 0.01f) slot = v;  // silent
        for (int v = 0; v < count && slot < 0; ++v) if (!voice_[v].held && !voice_[v].sustained) slot = v;                      // releasing
        if (slot < 0) {
            const int prio = ip(hPriority);  // 0 newest, 1 oldest(keep), 2 highest, 3 lowest
            if (prio == 1) return;
            int pick = 0;
            for (int v = 1; v < count; ++v) {
                const Voice& a = voice_[v];
                const Voice& b = voice_[pick];
                if (prio == 0 && a.age < b.age) pick = v;
                if (prio == 2 && a.note < b.note) pick = v;
                if (prio == 3 && a.note > b.note) pick = v;
            }
            if (prio == 2 && note < voice_[pick].note) return;
            if (prio == 3 && note > voice_[pick].note) return;
            slot = pick;
        }
        voice_[slot] = Voice{note, true, false, ++ageCounter_};
    }
    void noteOff(int note) {
        for (auto& v : voice_) if (v.note == note && v.held) { v.held = false; v.sustained = sustain_; }
    }
    void handleMidi(const MidiEvent& e) {
        if (e.isNoteOn()) noteOn(e.data1);
        else if (e.isNoteOff()) noteOff(e.data1);
        else if (e.isAllNotesOff()) { for (auto& v : voice_) { v.held = false; v.sustained = false; } }
        else if (e.isSustain()) {
            sustain_ = e.data2 >= 64;
            if (!sustain_) for (auto& v : voice_) v.sustained = false;
        } else if (e.isPitchBend()) {
            const int val = (e.data2 << 7) | e.data1;
            bend_ = (static_cast<float>(val) - 8192.0f) / 8192.0f * 2.0f;
        }
    }

    // ---- per-frame harmony computation -----------------------------------------------------------------
    void frame(const cfloat* X, cfloat* outL, cfloat* outR) {
        ++frameCount_;
        const float f0 = yin_.estimate();
        if (f0 > 0.0f) { lastF0_ = f0; unvoicedFrames_ = 0; } else ++unvoicedFrames_;
        ana_.analyse(X);
        const bool preserve = bp(hPreserve);
        const float character = dsp::semitonesToRatio(fp(hCharacter));
        if (preserve || std::fabs(fp(hCharacter)) > 0.01f) {
            const float ref = lastF0_ > 0.0f ? lastF0_ : 220.0f;
            const float binHz = srf() / static_cast<float>(n_);
            ana_.estimateEnvelope(static_cast<int>(0.5f * srf() / ref), 3, std::max(2, static_cast<int>(0.6f * ref / binHz)));
        }
        const int mode = ip(hMode);  // 0 interval, 1 scale, 2 MIDI
        const int key = ip(hKey), scale = ip(hScale);
        const int count = ip(hVoices) + 1;
        const float hopSec = static_cast<float>(hop_) / srf();
        const float glide = fp(hGlide) * 0.001f;
        const float gcoef = glide <= 0.0f ? 1.0f : 1.0f - std::exp(-hopSec / glide);
        const float inMidi = lastF0_ > 0.0f ? 69.0f + 12.0f * std::log2(lastF0_ / 440.0f) : 60.0f;
        static constexpr float kDetunePattern[kV] = {1.0f, -1.0f, 0.5f, -0.5f};
        for (int v = 0; v < kV; ++v) {
            bool on = v < count;
            float targetSt = 0.0f;
            if (mode == 0) {
                targetSt = std::round(fp(hInt1 + v));
            } else if (mode == 1) {
                const int deg = nearestDegree(inMidi, key, scale);
                const int snapped = degreeToMidi(deg, key, scale);
                targetSt = static_cast<float>(degreeToMidi(deg + ip(hInt1 + v), key, scale) - snapped);
                on = on && unvoicedFrames_ < 30;
            } else {
                on = on && voice_[v].note >= 0 && (voice_[v].held || voice_[v].sustained);
                if (voice_[v].note >= 0) targetSt = static_cast<float>(voice_[v].note) + bend_ - inMidi;
                if (lastF0_ <= 0.0f) targetSt = static_cast<float>(voice_[v].note) - 60.0f;
            }
            vibPhase_[v] += fp(hVibRate) * hopSec;
            vibPhase_[v] -= std::floor(vibPhase_[v]);
            const float cents = kDetunePattern[v] * fp(hDetune) + fp(hVibDepth) * std::sin(dsp::kTwoPi * vibPhase_[v]) +
                                pct(hHumanize) * 15.0f * drift_[v].next();
            const float tgtLog = std::clamp(targetSt + cents * 0.01f, -24.0f, 24.0f) / 12.0f;
            // first frame of a new note jumps (glide only between notes and for scoops)
            logRatio_[v] += gcoef * (tgtLog - logRatio_[v]);
            const float tgtGain = on ? dsp::dbToGain(fp(hLvl1 + v)) : 0.0f;
            gain_[v] += (tgtGain > gain_[v] ? 0.6f : 0.5f) * (tgtGain - gain_[v]);
            if (gain_[v] < 1e-4f) { gain_[v] = 0.0f; if (on) logRatio_[v] = tgtLog; }
            float gl, gr;
            dsp::panGains(fp(hPan1 + v) * 0.01f, gl, gr);
            shifter_.addVoice(ana_, std::exp2(logRatio_[v]), character, preserve, gain_[v], outL, outR, gl * 1.41f, gr * 1.41f, v);
        }
    }

    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);  // latency-aligned dry
        int ev = 0;
        for (int i = 0; i < n; ++i) {
            while (ev < ctx.numMidi && ctx.midi[ev].sampleOffset - ctx.blockOffset <= i) handleMidi(ctx.midi[ev++]);
            mono_[static_cast<size_t>(i)] = 0.5f * (ch[0][i] + ch[1][i]);
            yin_.push(mono_[static_cast<size_t>(i)]);
            // one sample at a time keeps MIDI changes sample-accurate relative to frame boundaries
            stft_.process(&mono_[static_cast<size_t>(i)], &hl_[static_cast<size_t>(i)], &hr_[static_cast<size_t>(i)], 1,
                          [this](const cfloat* X, cfloat* yl, cfloat* yr, int) { frame(X, yl, yr); });
        }
        while (ev < ctx.numMidi) handleMidi(ctx.midi[ev++]);
        for (int i = 0; i < n; ++i) {
            const float dg = dryG_.next(), wg = wetG_.next();
            ch[0][i] = dg * mix_.dry(0)[i] + wg * hl_[static_cast<size_t>(i)];
            ch[1][i] = dg * mix_.dry(1)[i] + wg * hr_[static_cast<size_t>(i)];
        }
    }

    int n_ = 2048, hop_ = 512;
    dsp::StftMonoToStereo stft_;
    dsp::SpectralAnalysis ana_;
    dsp::PeakShifter shifter_;
    dsp::Yin yin_;
    dsp::Lfo drift_[kV];
    dsp::Smoother dryG_, wetG_;
    MixStage mix_;
    std::vector<float> mono_, hl_, hr_;
    Voice voice_[kV];
    bool sustain_ = false;
    float bend_ = 0.0f, lastF0_ = 0.0f;
    int unvoicedFrames_ = 1000;
    uint32_t ageCounter_ = 0;
    float logRatio_[kV] = {}, vibPhase_[kV] = {}, gain_[kV] = {};
    int64_t frameCount_ = 0;
};

// ===================================================================================== Pitch Shift
enum { pShiftA, pFineA, pShiftB, pLevelB, pPreserve, pCharacter, pDelay, pFeedback, pWidth, pMix };

class PitchShift final : public FxBase {
    void onPrepare() override {
        // Low latency: a quarter-length window (512 points at 44.1/48 kHz, ~11 ms instead of ~43 ms)
        n_ = dsp::stftSizeFor(sr_, latMode_ == LatencyMode::Quality ? 2048 : 512);
        hop_ = n_ / 4;
        for (int c = 0; c < 2; ++c) {
            stft_[c].prepare(n_, hop_);
            ana_[c].prepare(n_, hop_);
            shiftA_[c].prepare(n_, hop_);
        }
        for (auto& d : fbLine_) d.prepare(static_cast<int>(1.1 * sr_));
        mix_.prepare(sr_, maxBlock_, n_, MixLaw::Linear);
    }
    int latencySamples() const override { return n_; }
    double tailSeconds() const override { return 0.1 + (p_[pFeedback] > 0 ? 4.0 : 0.0); }
    void reset() override {
        for (int c = 0; c < 2; ++c) { stft_[c].reset(); ana_[c].reset(); shiftA_[c].reset(); fbLine_[c].reset(); }
        mix_.reset();
    }
    void applyParams(bool snap) override { mix_.setMix(pct(pMix), snap); }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float ra = dsp::semitonesToRatio(fp(pShiftA) + fp(pFineA) * 0.01f);
        const float rb = dsp::semitonesToRatio(fp(pShiftB));
        const float gb = fp(pLevelB) <= -59.9f ? 0.0f : dsp::dbToGain(fp(pLevelB));
        const bool preserve = bp(pPreserve);
        const float character = dsp::semitonesToRatio(fp(pCharacter));
        const float w = pct(pWidth);
        const float fb = std::min(pct(pFeedback), 0.9f);
        const float dly = std::max(1.0f, fp(pDelay) * 0.001f * srf());
        for (int c = 0; c < 2; ++c) {
            float gA, gAo, gB, gBo;
            dsp::panGains(-w, gA, gAo);  // voice A leans left, voice B right
            dsp::panGains(w, gB, gBo);
            const float ga = (c == 0 ? gA : gAo) * 1.41f, gbb = (c == 0 ? gB : gBo) * 1.41f * gb;
            for (int i = 0; i < n; ++i) {
                float x = ch[c][i] + fb * fbLine_[c].readCubic(dly);
                stft_[c].process(&x, 1, [&](cfloat* X, int N) {
                    ana_[c].analyse(X);
                    if (preserve || std::fabs(fp(pCharacter)) > 0.01f) ana_[c].estimateEnvelope(48, 3);
                    std::fill(X, X + N, cfloat());
                    shiftA_[c].addVoice(ana_[c], ra, character, preserve, ga, X, nullptr, 1.0f, 0.0f, 0);
                    if (gbb > 0.0f) shiftA_[c].addVoice(ana_[c], rb, character, preserve, gbb, X, nullptr, 1.0f, 0.0f, 1);
                    dsp::Stft::mirror(X, N);
                });
                fbLine_[c].push(2.0f * std::tanh(0.5f * x));
                ch[c][i] = x;
            }
        }
        mix_.apply(ch, n);
    }
    int n_ = 2048, hop_ = 512;
    dsp::Stft stft_[2];
    dsp::SpectralAnalysis ana_[2];
    dsp::PeakShifter shiftA_[2];
    dsp::DelayLine fbLine_[2];
    MixStage mix_;
};

// ===================================================================================== Formant Shift
enum { oShift, oDetail, oOut, oMix };

class FormantShift final : public FxBase {
    void onPrepare() override {
        // Low latency: a quarter-length window (512 points at 44.1/48 kHz, ~11 ms instead of ~43 ms)
        n_ = dsp::stftSizeFor(sr_, latMode_ == LatencyMode::Quality ? 2048 : 512);
        hop_ = n_ / 4;
        for (int c = 0; c < 2; ++c) { stft_[c].prepare(n_, hop_); ana_[c].prepare(n_, hop_); }
        mix_.prepare(sr_, maxBlock_, n_, MixLaw::Linear);
        out_.prepare(sr_, 30.0f);
    }
    int latencySamples() const override { return n_; }
    void reset() override { for (int c = 0; c < 2; ++c) { stft_[c].reset(); ana_[c].reset(); } mix_.reset(); }
    void applyParams(bool snap) override {
        mix_.setMix(pct(oMix), snap);
        out_.setTarget(dsp::dbToGain(fp(oOut)));
        if (snap) out_.snap(out_.target());
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float ratio = dsp::semitonesToRatio(fp(oShift));
        const int order = static_cast<int>(20.0f + pct(oDetail) * 100.0f) * n_ / 2048;
        for (int c = 0; c < 2; ++c) {
            stft_[c].process(ch[c], n, [&](cfloat* X, int N) {
                auto& a = ana_[c];
                a.analyse(X);
                a.estimateEnvelope(order, 3);
                const int B = N / 2 + 1;
                for (int k = 1; k < B; ++k) {
                    const float src = static_cast<float>(k) / ratio;
                    const int i0 = std::min(static_cast<int>(src), B - 2);
                    const float f = std::clamp(src - static_cast<float>(i0), 0.0f, 1.0f);
                    const float e = a.env[static_cast<size_t>(i0)] + f * (a.env[static_cast<size_t>(i0 + 1)] - a.env[static_cast<size_t>(i0)]);
                    const float g = std::min(e / std::max(1e-9f, a.env[static_cast<size_t>(k)]), 16.0f);
                    X[k] *= (src < B - 1) ? g : 0.0f;
                }
                dsp::Stft::mirror(X, N);
            });
        }
        for (int i = 0; i < n; ++i) { const float g = out_.next(); ch[0][i] *= g; ch[1][i] *= g; }
        mix_.apply(ch, n);
    }
    int n_ = 2048, hop_ = 512;
    dsp::Stft stft_[2];
    dsp::SpectralAnalysis ana_[2];
    MixStage mix_;
    dsp::Smoother out_;
};

// ===================================================================================== Spectral Freeze / Blur
enum { zFreeze, zFade, zBlur, zSmear, zShimmer, zHighCut, zMix };

class SpectralFreeze final : public FxBase {
    void onPrepare() override {
        // Low latency: a quarter-length window (512 points at 44.1/48 kHz, ~11 ms instead of ~43 ms)
        n_ = dsp::stftSizeFor(sr_, latMode_ == LatencyMode::Quality ? 2048 : 512);
        hop_ = n_ / 4;
        const size_t B = static_cast<size_t>(n_ / 2 + 1);
        for (int c = 0; c < 2; ++c) {
            stft_[c].prepare(n_, hop_);
            ana_[c].prepare(n_, hop_);
            blur_[c].assign(B, 0.0f);
            frozen_[c].assign(B, 0.0f);
            frozenW_[c].assign(B, 0.0f);
            outPh_[c].assign(B, 0.0f);
            tmp_[c].assign(B, 0.0f);
        }
        mix_.prepare(sr_, maxBlock_, n_, MixLaw::Flat);
    }
    int latencySamples() const override { return n_; }
    double tailSeconds() const override { return bp(zFreeze) ? 60.0 : 1.0 + pct(zBlur) * 4.0; }
    void reset() override {
        for (int c = 0; c < 2; ++c) {
            stft_[c].reset(); ana_[c].reset();
            std::fill(blur_[c].begin(), blur_[c].end(), 0.0f);
            std::fill(frozen_[c].begin(), frozen_[c].end(), 0.0f);
            std::fill(outPh_[c].begin(), outPh_[c].end(), 0.0f);
        }
        captured_ = false;
        amt_ = 0.0f;
        loudFrames_ = 0;
        rng_.seed_(77);
        mix_.reset();
    }
    void applyParams(bool snap) override { mix_.setMix(pct(zMix), snap); }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const bool freeze = bp(zFreeze);
        if (!freeze) captured_ = false;
        const float hopSec = static_cast<float>(hop_) / srf();
        const float fadeStep = hopSec / std::max(0.01f, fp(zFade) * 0.001f);
        const float a = std::pow(pct(zBlur), 0.35f) * 0.985f;  // time blur coefficient per frame
        const int smear = static_cast<int>(pct(zSmear) * 24.0f * n_ / 2048);
        const float shimmer = pct(zShimmer);
        const int cutBin = static_cast<int>(fp(zHighCut) / srf() * n_);
        // both channels share one capture decision so the frozen image stays stereo-coherent
        bool captureNow = false;
        for (int c = 0; c < 2; ++c) {
            stft_[c].process(ch[c], n, [&](cfloat* X, int N) {
                auto& an = ana_[c];
                an.analyse(X);
                const int B = N / 2 + 1;
                auto& bl = blur_[c];
                for (int k = 0; k < B; ++k) bl[static_cast<size_t>(k)] = a * bl[static_cast<size_t>(k)] + (1.0f - a) * an.mag[static_cast<size_t>(k)];
                const std::vector<float>* M = &bl;
                if (smear > 0) {
                    auto& t = tmp_[c];
                    float acc = 0.0f;
                    int lo = 0, hi = -1;
                    for (int k = 0; k < B; ++k) {
                        while (hi < std::min(B - 1, k + smear)) acc += bl[static_cast<size_t>(++hi)];
                        while (lo < k - smear) acc -= bl[static_cast<size_t>(lo++)];
                        t[static_cast<size_t>(k)] = acc / static_cast<float>(hi - lo + 1);
                    }
                    M = &t;
                }
                if (c == 0 && freeze && !captured_) {
                    double e = 0.0;
                    for (int k = 0; k < B; ++k) e += (*M)[static_cast<size_t>(k)];
                    // wait for real, settled sound (6 consecutive loud frames) before freezing
                    loudFrames_ = (e > 1e-3 * N) ? loudFrames_ + 1 : 0;
                    if (loudFrames_ >= 6) captureNow = true;
                }
                if (captureNow) {
                    for (int k = 0; k < B; ++k) {
                        frozen_[c][static_cast<size_t>(k)] = (*M)[static_cast<size_t>(k)];
                        frozenW_[c][static_cast<size_t>(k)] = an.instFreq[static_cast<size_t>(k)];
                        outPh_[c][static_cast<size_t>(k)] = an.phase[static_cast<size_t>(k)];
                    }
                }
                const float fa = amt_;
                for (int k = 0; k < B; ++k) {
                    outPh_[c][static_cast<size_t>(k)] += static_cast<float>(hop_) * frozenW_[c][static_cast<size_t>(k)] + shimmer * 3.14159f * rng_.bipolar();
                    const cfloat live = std::polar((*M)[static_cast<size_t>(k)], an.phase[static_cast<size_t>(k)]);
                    const cfloat fr = std::polar(frozen_[c][static_cast<size_t>(k)], outPh_[c][static_cast<size_t>(k)]);
                    float g = 1.0f;
                    if (k > cutBin) g = std::max(0.0f, 1.0f - static_cast<float>(k - cutBin) / (0.15f * static_cast<float>(B)));
                    X[k] = ((1.0f - fa) * live + fa * fr) * g;
                }
                dsp::Stft::mirror(X, N);
                if (c == 1) {
                    if (captureNow) { captured_ = true; captureNow = false; }
                    const float tgt = (freeze && captured_) ? 1.0f : 0.0f;
                    amt_ = tgt > amt_ ? std::min(tgt, amt_ + fadeStep) : std::max(tgt, amt_ - fadeStep);
                }
            });
        }
        mix_.apply(ch, n);
    }
    int n_ = 2048, hop_ = 512;
    dsp::Stft stft_[2];
    dsp::SpectralAnalysis ana_[2];
    std::vector<float> blur_[2], frozen_[2], frozenW_[2], outPh_[2], tmp_[2];
    bool captured_ = false;
    float amt_ = 0.0f;
    int loudFrames_ = 0;
    dsp::Rng rng_{77};
    MixStage mix_;
};

}  // namespace

FxDescriptor describeHarmony() {
    FxDescriptor d;
    d.typeId = "harmony";
    d.name = "Harmony";
    d.family = FxFamily::PitchVoice;
    d.tooltip = "Up to four harmony voices made from your own voice, following fixed intervals, a key and scale, or notes you play on a MIDI keyboard.";
    d.wantsMidi = true;
    d.params = {
        P::choice("mode", "Mode", {"Intervals", "Key & Scale", "MIDI Notes"}, 1, "Intervals: fixed semitone shifts. Key & Scale: harmonies stay in key. MIDI Notes: voices sing the notes you hold on a keyboard."),
        P::choice("key", "Key", {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0, "The song's key (Key & Scale mode)."),
        P::choice("scale", "Scale", {"Major", "Minor", "Dorian", "Mixolydian", "Harmonic Minor", "Major Pentatonic", "Minor Pentatonic", "Chromatic"}, 0, "Which notes are allowed for the harmonies."),
        P::choice("voices", "Voices", {"1", "2", "3", "4"}, 1, "How many harmony voices sing."),
        P::flt("int1", "Voice 1", "", -24.0f, 24.0f, 2.0f, "Voice 1's interval: semitones in Intervals mode, scale steps in Key & Scale mode (2 = a third, 4 = a fifth, 7 = an octave)."),
        P::flt("int2", "Voice 2", "", -24.0f, 24.0f, 4.0f, "Voice 2's interval (see Voice 1)."),
        P::flt("int3", "Voice 3", "", -24.0f, 24.0f, -3.0f, "Voice 3's interval (see Voice 1)."),
        P::flt("int4", "Voice 4", "", -24.0f, 24.0f, 7.0f, "Voice 4's interval (see Voice 1)."),
        P::flt("lvl1", "Level 1", "dB", -60.0f, 6.0f, 0.0f, "Loudness of voice 1."),
        P::flt("lvl2", "Level 2", "dB", -60.0f, 6.0f, -2.0f, "Loudness of voice 2."),
        P::flt("lvl3", "Level 3", "dB", -60.0f, 6.0f, -4.0f, "Loudness of voice 3."),
        P::flt("lvl4", "Level 4", "dB", -60.0f, 6.0f, -6.0f, "Loudness of voice 4."),
        P::flt("pan1", "Pan 1", "%", -100.0f, 100.0f, -60.0f, "Stereo position of voice 1."),
        P::flt("pan2", "Pan 2", "%", -100.0f, 100.0f, 60.0f, "Stereo position of voice 2."),
        P::flt("pan3", "Pan 3", "%", -100.0f, 100.0f, -30.0f, "Stereo position of voice 3."),
        P::flt("pan4", "Pan 4", "%", -100.0f, 100.0f, 30.0f, "Stereo position of voice 4."),
        P::flt("character", "Character", "st", -12.0f, 12.0f, 0.0f, "Moves the vocal formants: down = bigger, darker voices; up = smaller, brighter ones. Independent of the notes."),
        P::toggle("preserve", "Natural Formants", true, "Keeps the harmonies sounding like a real voice instead of chipmunk/giant."),
        P::flt("detune", "Detune", "ct", 0.0f, 50.0f, 6.0f, "Spreads the voices slightly out of tune for a thicker, choir-like sound."),
        P::flt("vib_depth", "Vibrato", "ct", 0.0f, 100.0f, 0.0f, "Adds vibrato to the harmony voices."),
        P::flt("vib_rate", "Vibrato Rate", "Hz", 0.5f, 8.0f, 5.0f, "Speed of the vibrato."),
        P::flt("glide", "Glide", "ms", 0.0f, 500.0f, 40.0f, "How smoothly voices slide between notes. Higher values give a scooped, singerly approach."),
        P::pct("humanize", "Humanize", 20.0f, "Tiny independent pitch wanders per voice, like separate singers."),
        P::choice("priority", "Note Priority", {"Newest", "Oldest", "Highest", "Lowest"}, 0, "MIDI mode: which notes win when you hold more notes than there are voices."),
        P::flt("dry", "Lead", "dB", -60.0f, 6.0f, 0.0f, "Level of your original voice."),
        P::flt("wet", "Harmonies", "dB", -60.0f, 6.0f, 0.0f, "Level of all harmony voices together."),
    };
    for (int i = hPan1; i <= hPan4; ++i) d.params[static_cast<size_t>(i)].advanced = true;
    for (int i : {hVibRate, hHumanize, hPriority, hPreserve}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<Harmony>(); };
    return d;
}

FxDescriptor describePitchShift() {
    FxDescriptor d;
    d.typeId = "pitch_shift";
    d.name = "Pitch Shift";
    d.family = FxFamily::PitchVoice;
    d.tooltip = "Transposes the sound by musical intervals; add a second voice for octaves, or feedback for climbing echoes.";
    d.params = {
        P::flt("shift_a", "Shift", "st", -24.0f, 24.0f, 12.0f, "Main transposition in semitones (12 = one octave)."),
        P::flt("fine_a", "Fine", "ct", -100.0f, 100.0f, 0.0f, "Fine tuning in cents."),
        P::flt("shift_b", "Voice 2", "st", -24.0f, 24.0f, -12.0f, "Second voice transposition (classic octaver: -12)."),
        P::flt("level_b", "Voice 2 Level", "dB", -60.0f, 6.0f, -60.0f, "Level of the second voice (-60 = off)."),
        P::toggle("preserve", "Natural Formants", true, "Keeps voices and instruments sounding natural after shifting."),
        P::flt("character", "Character", "st", -12.0f, 12.0f, 0.0f, "Shifts the formants separately from the pitch."),
        P::flt("delay_ms", "Delay", "ms", 0.0f, 1000.0f, 0.0f, "Delay in the feedback loop. With Feedback, each echo is shifted again."),
        P::pct("feedback", "Feedback", 0.0f, "Feeds the shifted sound back in: spiralling, climbing echoes."),
        P::pct("width", "Width", 30.0f, "Places the two voices apart in stereo."),
        P::pct("mix", "Mix", 50.0f, "Balance between original and shifted sound."),
    };
    for (int i : {pFineA, pCharacter, pWidth}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<PitchShift>(); };
    return d;
}

FxDescriptor describeFormant() {
    FxDescriptor d;
    d.typeId = "formant";
    d.name = "Formant Shift";
    d.family = FxFamily::PitchVoice;
    d.tooltip = "Changes the size and colour of a voice or instrument without changing its notes.";
    d.params = {
        P::flt("shift", "Formant", "st", -12.0f, 12.0f, 3.0f, "Up = smaller, brighter, younger-sounding. Down = larger, darker, deeper body. The pitch stays the same."),
        P::pct("detail", "Detail", 30.0f, "How finely the vocal tract shape is followed. Low = smooth, high = more detailed (can sound buzzy)."),
        P::flt("out", "Output", "dB", -18.0f, 12.0f, 0.0f, "Final level."),
        P::pct("mix", "Mix", 100.0f, "Balance between original and reshaped sound."),
    };
    d.params[oDetail].advanced = true;
    d.create = [] { return std::make_unique<FormantShift>(); };
    return d;
}

FxDescriptor describeSpectral() {
    FxDescriptor d;
    d.typeId = "spectral";
    d.name = "Spectral Freeze";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Holds or blurs the sound's spectrum: endless frozen chords, smeared trails and dissolving pads.";
    d.params = {
        P::toggle("freeze", "Freeze", false, "Captures the next sound it hears and holds it forever."),
        P::logf("fade_ms", "Fade", "ms", 10.0f, 4000.0f, 400.0f, "How slowly the freeze fades in and out."),
        P::pct("blur", "Blur", 40.0f, "Smears the sound over time so notes melt into each other."),
        P::pct("smear", "Smear", 0.0f, "Blurs across pitch: from glassy to a soft spectral fog."),
        P::pct("shimmer", "Shimmer", 0.0f, "Adds gentle shimmering motion to frozen sound."),
        P::logf("high_cut", "High Cut", "Hz", 1000.0f, 20000.0f, 16000.0f, "Softens the top of the spectral texture."),
        P::pct("mix", "Mix", 60.0f, "Balance between original and spectral texture. 50% keeps both at full level."),
    };
    d.params[zHighCut].advanced = true;
    d.create = [] { return std::make_unique<SpectralFreeze>(); };
    return d;
}

}  // namespace ih1::fx
