// V4 feedback round: the three STFT modules of the ten new processors (Phase 2). Original IH1 designs.
//
//   Spectral Shatter     the spectrum is cut into shards (log-spaced bands). On every tempo step a seeded map moves
//                        shards to other shards' frequency ranges (with exact phase rotation for the N/4 hop, so moved
//                        content stays coherent), freezes some and gates others. Map changes are crossfaded over a few
//                        frames. A frequency-space mosaic, not a delay or a blur.
//   Partial Recomposer   spectral peaks (partials) and their regions are tracked (KADAL SRC-3Q84R4ZMZG peak/region
//                        model, reused from SpectralAnalysis) and rebuilt at new frequencies: harmonic stretch /
//                        squeeze around the lowest strong partial, a linear Hz shift, odd/even balance, alternate
//                        partials panned, a partial sustain, and the residual (non-partial) part at its own level.
//   Spectral Graft       one-input cross-synthesis, honestly named: the spectral envelope of the live input is
//                        replaced (by Amount) with another envelope taken from the input's own past (a held snapshot,
//                        an echo of N ms ago) or from the opposite channel. Not a sidechain vocoder.
//
// Latency: each reports its STFT size (Quality 1024 / 2048 at 48 kHz, Low = 512). None claims zero latency.
#include <array>
#include <cmath>

#include "ih1/dsp/Fft.h"
#include "ih1/dsp/Spectral.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {
using dsp::cfloat;

inline float softLimit(float x, float knee = 1.2f) {
    const float a = std::fabs(x);
    return a <= knee ? x : std::copysign(knee + std::tanh(a - knee), x);
}
inline uint32_t mix32(uint32_t a, uint32_t b) {
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    h ^= h >> 16; h *= 0x85EBCA6Bu; h ^= h >> 13; h *= 0xC2B2AE35u; h ^= h >> 16;
    return h;
}
inline float unit(uint32_t h) { return static_cast<float>(h & 0xFFFFFF) / 16777216.0f; }

// =================================================================================================== Spectral Shatter
enum { shShards, shDiv, shReorder, shFreeze, shGate, shSeed, shSmooth, shTilt, shOut, shMix };
constexpr int kMaxShards = 32;

class SpectralShatter final : public FxBase {
    struct Map { std::array<int, kMaxShards> src{}; std::array<uint8_t, kMaxShards> freeze{}, gate{}; };
    void onPrepare() override {
        n_ = dsp::stftSizeFor(sr_, latMode_ == LatencyMode::Quality ? 1024 : 512);
        hop_ = n_ / 4;
        B_ = n_ / 2 + 1;
        for (int c = 0; c < 2; ++c) {
            stft_[c].prepare(n_, hop_);
            frozen_[c].assign(static_cast<size_t>(B_), cfloat());
            y0_[c].assign(static_cast<size_t>(B_), cfloat());
            y1_[c].assign(static_cast<size_t>(B_), cfloat());
        }
        mix_.prepare(sr_, maxBlock_, n_, MixLaw::Linear);
        out_.prepare(sr_, 30.0f);
    }
    int latencySamples() const override { return n_; }
    double tailSeconds() const override { return static_cast<double>(n_) / sr_ + 0.1; }
    void reset() override {
        for (int c = 0; c < 2; ++c) { stft_[c].reset(); std::fill(frozen_[c].begin(), frozen_[c].end(), cfloat()); }
        frame_[0] = frame_[1] = 0;
        clock_ = 0;
        for (int c = 0; c < 2; ++c) { lastStep_[c] = INT64_MIN; blend_[c] = 1.0f; identity(cur_[c]); identity(prev_[c]); }
        mix_.reset();
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(shMix), snap);
        out_.setTarget(dsp::dbToGain(fp(shOut)));
        if (snap) out_.snap(out_.target());
    }
    void identity(Map& m) const { for (int s = 0; s < kMaxShards; ++s) { m.src[static_cast<size_t>(s)] = s; m.freeze[static_cast<size_t>(s)] = m.gate[static_cast<size_t>(s)] = 0; } }
    int shardCount() const { static const int k[4] = {8, 12, 20, 32}; return k[std::clamp(ip(shShards), 0, 3)]; }
    void band(int s, int S, int& lo, int& hi) const {
        // log-spaced shard edges from 50 Hz to 0.95 x Nyquist (shard 0 also takes the bins below 50 Hz)
        const float f0 = 50.0f, f1 = 0.475f * srf();
        auto edge = [&](int e) { return e <= 0 ? 0 : static_cast<int>(std::lround(f0 * std::pow(f1 / f0, static_cast<float>(e) / S) * n_ / srf())); };
        lo = std::clamp(edge(s), 0, B_ - 1);
        hi = std::clamp(edge(s + 1), lo + 1, B_);
    }
    void makeMap(Map& m, int64_t step, int S) const {
        identity(m);
        const uint32_t seed = static_cast<uint32_t>(ip(shSeed)) * 2654435761u + 0x5A7u;
        const uint32_t h0 = mix32(seed, static_cast<uint32_t>(step & 0xFFFFFFFF));
        // move a Reorder fraction of shards: random transpositions among the chosen ones (a permutation)
        std::array<int, kMaxShards> chosen{};
        int k = 0;
        for (int s = 0; s < S; ++s) if (unit(mix32(h0, static_cast<uint32_t>(s))) < pct(shReorder)) chosen[static_cast<size_t>(k++)] = s;
        for (int i = k - 1; i > 0; --i) {
            const int j = static_cast<int>(mix32(h0 ^ 0xBEEFu, static_cast<uint32_t>(i)) % static_cast<uint32_t>(i + 1));
            std::swap(m.src[static_cast<size_t>(chosen[static_cast<size_t>(i)])], m.src[static_cast<size_t>(chosen[static_cast<size_t>(j)])]);
        }
        for (int s = 0; s < S; ++s) {
            m.freeze[static_cast<size_t>(s)] = unit(mix32(h0 ^ 0xF00Du, static_cast<uint32_t>(s))) < pct(shFreeze) ? 1 : 0;
            m.gate[static_cast<size_t>(s)] = unit(mix32(h0 ^ 0x6A7Eu, static_cast<uint32_t>(s))) < pct(shGate) ? 1 : 0;
        }
        m.gate[0] = 0;  // the lowest shard is never gated (keeps the floor of the sound)
    }
    // render one map into Y (bins 0..N/2)
    void render(const Map& m, const cfloat* X, int c, std::vector<cfloat>& Y, int S, float tilt) {
        std::fill(Y.begin(), Y.end(), cfloat());
        const int64_t fr = frame_[c];
        for (int d = 0; d < S; ++d) {
            int dlo, dhi, slo, shi;
            band(d, S, dlo, dhi);
            const int s = m.src[static_cast<size_t>(d)];
            band(s, S, slo, shi);
            const float g = (m.gate[static_cast<size_t>(d)] ? 0.0f : 1.0f) * std::pow(10.0f, tilt * (static_cast<float>(d) / std::max(1, S - 1) - 0.5f) * 0.6f);
            if (g <= 0.0f) continue;
            // pure translation: destination bin k takes source bin slo + (k - dlo) (one source bin per destination
            // bin; a wider destination leaves its top empty, a narrower one keeps the bottom of the source band).
            // Resampling the band instead mapped several bins onto one with different phase rotations (broadband grit).
            const int width = shi - slo;
            for (int k = dlo; k < dhi; ++k) {
                if (m.freeze[static_cast<size_t>(d)]) {
                    // frozen: the captured value, its phase advanced at the bin's own frequency (hop N/4 -> k * pi/2)
                    Y[static_cast<size_t>(k)] = frozen_[c][static_cast<size_t>(k)] * g;
                    continue;
                }
                if (k - dlo >= width && !m.freeze[static_cast<size_t>(d)]) continue;
                const int sb = std::clamp(slo + (k - dlo), 0, B_ - 1);
                const int delta = k - sb;  // integer bin shift: phase rotation (pi/2) * delta per frame keeps it coherent
                const int q = static_cast<int>(((static_cast<int64_t>(delta) * fr) % 4 + 4) % 4);
                static const cfloat rot[4] = {cfloat(1, 0), cfloat(0, 1), cfloat(-1, 0), cfloat(0, -1)};
                Y[static_cast<size_t>(k)] = X[sb] * rot[q] * g;
            }
        }
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const int S = shardCount();
        const double stepLen = std::max(0.03, dsp::divisionSeconds(ip(shDiv), ctx.bpm)) * sr_;
        const float tilt = fp(shTilt) * 0.01f;
        const int smoothFrames = std::max(1, static_cast<int>(std::lround(fp(shSmooth))));
        // the step is evaluated per analysis frame at the frame's own sample time (frame k of a channel completes at
        // sample (k + 1) * hop after reset), so the result never depends on the host block size
        const bool host = ctx.ppqValid && ctx.hostPlaying;
        const double blockGrid = host ? ctx.ppqPosition * 60.0 / ctx.bpm * sr_ : static_cast<double>(clock_);
        auto stepAt = [&](int64_t frame) {
            const double t = static_cast<double>((frame + 1) * hop_) - static_cast<double>(clock_);  // offset in this block
            return static_cast<int64_t>(std::floor((blockGrid + t) / stepLen));
        };
        for (int c = 0; c < 2; ++c) {
            stft_[c].process(ch[c], n, [&](cfloat* X, int N) {
                {
                    // each channel steps at its own frames (same frame times, same maps: linked, block-size independent)
                    const int64_t step = stepAt(frame_[c]);
                    if (step != lastStep_[c]) {
                        lastStep_[c] = step;
                        prev_[c] = cur_[c];
                        makeMap(cur_[c], step, S);
                        blend_[c] = 0.0f;
                        captureNext_[c] = true;
                    }
                }
                // capture the spectrum that frozen shards hold (from the first frame of a step)
                if (captureNext_[c]) {
                    for (int k = 0; k < B_; ++k) frozen_[c][static_cast<size_t>(k)] = X[k];
                    captureNext_[c] = false;
                } else {
                    static const cfloat rot[4] = {cfloat(1, 0), cfloat(0, 1), cfloat(-1, 0), cfloat(0, -1)};
                    for (int k = 0; k < B_; ++k) frozen_[c][static_cast<size_t>(k)] *= rot[k & 3];  // keep spinning at the bin frequency
                }
                const float a = std::min(1.0f, blend_[c]);
                render(cur_[c], X, c, y1_[c], S, tilt);
                if (a < 1.0f) {
                    render(prev_[c], X, c, y0_[c], S, tilt);
                    for (int k = 0; k < B_; ++k) y1_[c][static_cast<size_t>(k)] = y0_[c][static_cast<size_t>(k)] * (1.0f - a) + y1_[c][static_cast<size_t>(k)] * a;
                }
                for (int k = 0; k < B_; ++k) X[k] = y1_[c][static_cast<size_t>(k)];
                dsp::Stft::mirror(X, N);
                ++frame_[c];
                blend_[c] = std::min(1.0f, blend_[c] + 1.0f / static_cast<float>(smoothFrames));
            });
        }
        for (int i = 0; i < n; ++i) {
            const float o = out_.next();
            ch[0][i] = softLimit(ch[0][i] * o, 2.0f);
            ch[1][i] = softLimit(ch[1][i] * o, 2.0f);
        }
        clock_ += n;
        mix_.apply(ch, n);
    }
    int n_ = 1024, hop_ = 256, B_ = 513;
    int64_t frame_[2] = {0, 0}, lastStep_[2] = {INT64_MIN, INT64_MIN}, clock_ = 0;
    float blend_[2] = {1.0f, 1.0f};
    bool captureNext_[2] = {false, false};
    Map cur_[2], prev_[2];
    dsp::Stft stft_[2];
    std::vector<cfloat> frozen_[2], y0_[2], y1_[2];
    MixStage mix_;
    dsp::Smoother out_;
};

// ================================================================================================ Partial Recomposer
enum { prStretch, prShift, prOddEven, prSpread, prSustain, prResidual, prThreshold, prOut, prMix };

class PartialRecomposer final : public FxBase {
    void onPrepare() override {
        n_ = dsp::stftSizeFor(sr_, latMode_ == LatencyMode::Quality ? 2048 : 512);
        hop_ = n_ / 4;
        B_ = n_ / 2 + 1;
        for (int c = 0; c < 2; ++c) {
            stft_[c].prepare(n_, hop_);
            an_[c].prepare(n_, hop_);
            outPh_[c].assign(static_cast<size_t>(B_), 0.0f);
            hold_[c].assign(static_cast<size_t>(B_), 0.0f);
            y_[c].assign(static_cast<size_t>(B_), cfloat());
            tonal_[c].assign(static_cast<size_t>(B_), 0);
        }
        mix_.prepare(sr_, maxBlock_, n_, MixLaw::Linear);
        out_.prepare(sr_, 30.0f);
    }
    int latencySamples() const override { return n_; }
    double tailSeconds() const override { return static_cast<double>(n_) / sr_ + 3.0 * pct(prSustain); }
    void reset() override {
        for (int c = 0; c < 2; ++c) {
            stft_[c].reset();
            an_[c].reset();
            std::fill(outPh_[c].begin(), outPh_[c].end(), 0.0f);
            std::fill(hold_[c].begin(), hold_[c].end(), 0.0f);
        }
        mix_.reset();
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(prMix), snap);
        out_.setTarget(dsp::dbToGain(fp(prOut)));
        if (snap) out_.snap(out_.target());
    }
    void processBlock(float* const* ch, int n, const ProcessContext&) override {
        mix_.capture(ch, n);
        const float stretch = fp(prStretch) * 0.01f;      // -50..+100 % -> exponent 0.5..2
        const float shiftHz = fp(prShift);
        const float oddEven = fp(prOddEven) * 0.01f;      // -1 odd only .. +1 even only
        const float spread = pct(prSpread);
        const float sustain = pct(prSustain);
        const float resid = dsp::dbToGain(fp(prResidual));
        const float thr = dsp::dbToGain(fp(prThreshold));
        const float holdCoef = sustain <= 0.0f ? 0.0f : std::exp(-static_cast<float>(hop_) / (sustain * 3.0f * srf()));
        const float binHz = srf() / static_cast<float>(n_);
        for (int c = 0; c < 2; ++c) {
            auto& A = an_[c];
            stft_[c].process(ch[c], n, [&](cfloat* X, int N) {
                A.analyse(X);
                auto& Y = y_[c];
                std::fill(Y.begin(), Y.end(), cfloat());
                auto& tonal = tonal_[c];
                std::fill(tonal.begin(), tonal.end(), uint8_t(0));
                float maxMag = 0.0f;
                for (int p : A.peaks) maxMag = std::max(maxMag, A.mag[static_cast<size_t>(p)]);
                // reference partial: the lowest strong one (fundamental-like); stretching keeps it in place
                float fref = 0.0f;
                for (int p : A.peaks) if (A.mag[static_cast<size_t>(p)] > 0.25f * maxMag) { fref = std::max(30.0f, A.instFreq[static_cast<size_t>(p)] * srf() / dsp::kTwoPi); break; }
                if (fref <= 0.0f) fref = 100.0f;
                int rank = 0;
                for (size_t pi = 0; pi < A.peaks.size(); ++pi) {
                    const int p = A.peaks[pi];
                    const float mp = A.mag[static_cast<size_t>(p)];
                    if (mp < thr * maxMag || mp <= 1e-9f) continue;
                    const float f = std::max(1.0f, A.instFreq[static_cast<size_t>(p)] * srf() / dsp::kTwoPi);
                    const float fn = fref * std::pow(f / fref, 1.0f + stretch) + shiftHz;
                    if (fn <= 20.0f || fn >= 0.47f * srf()) { ++rank; continue; }
                    const float r = fn / f;
                    const int target = static_cast<int>(std::lround(fn / binHz));
                    const int delta = target - p;
                    // partial gains: odd/even balance (by rank) and alternate-partial stereo placement
                    const bool odd = (rank % 2) == 0;
                    float g = 1.0f;
                    if (oddEven > 0.0f && odd) g *= 1.0f - oddEven;
                    if (oddEven < 0.0f && !odd) g *= 1.0f + oddEven;
                    g *= 1.0f - spread * ((c == 0) == odd ? 0.0f : 1.0f) * 0.85f;
                    ++rank;
                    const int lo = A.regionLo[pi], hi = A.regionHi[pi];
                    // identity phase locking: the peak's synthesis phase advances at its new frequency; region bins keep
                    // their phase relation to the peak
                    float& op = outPh_[c][static_cast<size_t>(std::clamp(target, 0, B_ - 1))];
                    op += A.instFreq[static_cast<size_t>(p)] * r * static_cast<float>(hop_);
                    op = std::remainder(op, dsp::kTwoPi);
                    for (int k = lo; k <= hi; ++k) {
                        const int kd = k + delta;
                        if (kd <= 0 || kd >= B_ - 1) continue;
                        tonal[static_cast<size_t>(k)] = 1;
                        const float ph = op + (A.phase[static_cast<size_t>(k)] - A.phase[static_cast<size_t>(p)]);
                        Y[static_cast<size_t>(kd)] += std::polar(A.mag[static_cast<size_t>(k)] * g, ph);
                    }
                }
                // partial sustain: output magnitudes hold and decay instead of following the input down
                if (holdCoef > 0.0f) {
                    auto& H = hold_[c];
                    for (int k = 0; k < B_; ++k) {
                        const float m = std::abs(Y[static_cast<size_t>(k)]);
                        H[static_cast<size_t>(k)] = std::max(m, H[static_cast<size_t>(k)] * holdCoef);
                        if (m > 1e-9f) Y[static_cast<size_t>(k)] *= H[static_cast<size_t>(k)] / m;
                        else if (H[static_cast<size_t>(k)] > 1e-6f) {
                            outPh_[c][static_cast<size_t>(k)] = std::remainder(outPh_[c][static_cast<size_t>(k)] + dsp::kTwoPi * k * hop_ / n_, dsp::kTwoPi);
                            Y[static_cast<size_t>(k)] = std::polar(H[static_cast<size_t>(k)], outPh_[c][static_cast<size_t>(k)]);
                        }
                    }
                }
                // residual: everything that is not part of a tracked partial, in place
                for (int k = 0; k < B_; ++k) if (!tonal[static_cast<size_t>(k)]) Y[static_cast<size_t>(k)] += X[k] * resid;
                for (int k = 0; k < B_; ++k) X[k] = Y[static_cast<size_t>(k)];
                X[0] = cfloat(X[0].real(), 0.0f);
                X[N / 2] = cfloat(X[N / 2].real(), 0.0f);
                dsp::Stft::mirror(X, N);
            });
        }
        for (int i = 0; i < n; ++i) {
            const float o = out_.next();
            ch[0][i] = softLimit(ch[0][i] * o, 2.0f);
            ch[1][i] = softLimit(ch[1][i] * o, 2.0f);
        }
        mix_.apply(ch, n);
    }
    int n_ = 2048, hop_ = 512, B_ = 1025;
    dsp::Stft stft_[2];
    dsp::SpectralAnalysis an_[2];
    std::vector<float> outPh_[2], hold_[2];
    std::vector<cfloat> y_[2];
    std::vector<uint8_t> tonal_[2];
    MixStage mix_;
    dsp::Smoother out_;
};

// ====================================================================================================== Spectral Graft
enum { sgSource, sgAmount, sgResolution, sgEcho, sgCapture, sgRefresh, sgFormant, sgSmooth, sgKeepLevel, sgOut, sgMix };
constexpr int kMaxBands = 64;
constexpr int kEchoFrames = 1200;

class SpectralGraft final : public FxBase {
    void onPrepare() override {
        n_ = dsp::stftSizeFor(sr_, latMode_ == LatencyMode::Quality ? 1024 : 512);
        hop_ = n_ / 4;
        B_ = n_ / 2 + 1;
        echoFrames_ = std::min(kEchoFrames, static_cast<int>(std::ceil(4.1 * sr_ / hop_)) + 2);
        for (int c = 0; c < 2; ++c) {
            stft_[c].prepare(n_, hop_);
            hist_[c].assign(static_cast<size_t>(echoFrames_) * kMaxBands, 0.0f);
            gainSm_[c].assign(kMaxBands, 1.0f);
            env_[c].assign(kMaxBands, 0.0f);
            held_[c].assign(kMaxBands, 0.0f);
            gbin_[c].assign(static_cast<size_t>(B_), 1.0f);
        }
        bandOf_.assign(static_cast<size_t>(B_), 0.0f);
        mix_.prepare(sr_, maxBlock_, n_, MixLaw::Linear);
        out_.prepare(sr_, 30.0f);
        lvl_[0] = lvl_[1] = 1.0f;
    }
    int latencySamples() const override { return n_; }
    double tailSeconds() const override { return static_cast<double>(n_) / sr_ + 0.2; }
    void reset() override {
        for (int c = 0; c < 2; ++c) {
            stft_[c].reset();
            std::fill(hist_[c].begin(), hist_[c].end(), 0.0f);
            std::fill(gainSm_[c].begin(), gainSm_[c].end(), 1.0f);
            std::fill(env_[c].begin(), env_[c].end(), 0.0f);
            std::fill(held_[c].begin(), held_[c].end(), 0.0f);
            hasHeld_[c] = false;
            loud_[c] = 0;
            w_[c] = 0;
            inE_[c] = outE_[c] = 1e-6f;
            lvl_[c] = 1.0f;
        }
        lastCapture_ = false;
        lastRefresh_ = INT64_MIN;
        clock_ = 0;
        mix_.reset();
    }
    void applyParams(bool snap) override {
        mix_.setMix(pct(sgMix), snap);
        out_.setTarget(dsp::dbToGain(fp(sgOut)));
        if (snap) out_.snap(out_.target());
    }
    int bands() const { return std::clamp(static_cast<int>(std::lround(fp(sgResolution))), 8, kMaxBands); }
    // log-band index (fractional) of a bin, for interpolating band gains back onto bins
    void mapBands(int nb) {
        if (nb == mappedBands_) return;
        mappedBands_ = nb;
        const float f0 = 40.0f, f1 = 0.48f * srf();
        for (int k = 0; k < B_; ++k) {
            const float f = std::max(f0, k * srf() / n_);
            bandOf_[static_cast<size_t>(k)] = std::clamp(std::log(f / f0) / std::log(f1 / f0) * nb - 0.5f, 0.0f, static_cast<float>(nb - 1));
        }
    }
    void envelope(const cfloat* X, int nb, float* e) const {
        std::fill(e, e + nb, 0.0f);
        float cnt[kMaxBands] = {};
        for (int k = 1; k < B_; ++k) {
            const int b = std::clamp(static_cast<int>(std::lround(bandOf_[static_cast<size_t>(k)])), 0, nb - 1);
            e[b] += std::norm(X[k]);
            cnt[b] += 1.0f;
        }
        for (int b = 0; b < nb; ++b) e[b] = std::sqrt(e[b] / std::max(1.0f, cnt[b]));
        // empty low bands (fewer bins than bands at low frequencies) borrow from their neighbours
        for (int b = 1; b < nb; ++b) if (cnt[b] == 0.0f) e[b] = e[b - 1];
    }
    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const int nb = bands();
        mapBands(nb);
        const int source = ip(sgSource);  // 0 held, 1 echo, 2 cross L/R
        const float amount = pct(sgAmount);
        const int echo = std::clamp(static_cast<int>(fp(sgEcho) * 0.001f * srf() / hop_), 1, echoFrames_ - 2);
        const float formant = std::exp2(fp(sgFormant) / 12.0f);
        const float sm = 1.0f - std::exp(-static_cast<float>(hop_) / (std::max(1.0f, fp(sgSmooth)) * 0.001f * srf()));
        const bool keep = bp(sgKeepLevel);
        // capture: the Capture switch turning on, or a tempo refresh, takes a new held snapshot
        bool capture = bp(sgCapture) && !lastCapture_;
        lastCapture_ = bp(sgCapture);
        const int refresh = ip(sgRefresh);
        if (refresh > 0) {
            static const double kBeats[4] = {0, 4, 8, 16};
            const double g = (ctx.ppqValid && ctx.hostPlaying) ? ctx.ppqPosition : clock_ / sr_ * ctx.bpm / 60.0;
            const int64_t k = static_cast<int64_t>(std::floor(g / kBeats[std::clamp(refresh, 1, 3)]));
            if (k != lastRefresh_) { lastRefresh_ = k; capture = true; }
        }
        if (capture) { captureReq_[0] = captureReq_[1] = true; }
        for (int c = 0; c < 2; ++c) {
            stft_[c].process(ch[c], n, [&](cfloat* X, int N) {
                float cur[kMaxBands], mod[kMaxBands];
                envelope(X, nb, cur);
                // store this frame's envelope (echo source and the other channel's cross source)
                float* row = hist_[c].data() + static_cast<size_t>(w_[c]) * kMaxBands;
                std::copy(cur, cur + nb, row);
                float level = 0.0f;
                for (int b = 0; b < nb; ++b) level += cur[b];
                // the held snapshot waits for settled sound: a whole analysis window of signal (5 sounding frames in a
                // row), never silence and never the broadband edge of an onset
                loud_[c] = level > 1e-3f * nb ? loud_[c] + 1 : 0;
                if ((captureReq_[c] || !hasHeld_[c]) && loud_[c] >= 5) {
                    std::copy(cur, cur + nb, held_[c].begin());
                    hasHeld_[c] = true;
                    captureReq_[c] = false;
                }
                const float* src;
                if (source == 0) src = hasHeld_[c] ? held_[c].data() : cur;
                else if (source == 1) src = hist_[c].data() + static_cast<size_t>((w_[c] - echo + echoFrames_) % echoFrames_) * kMaxBands;
                else {
                    const int o = 1 - c;  // the other channel's most recent stored envelope
                    src = hist_[o].data() + static_cast<size_t>((w_[o] - (c == 0 ? 1 : 0) + echoFrames_) % echoFrames_) * kMaxBands;
                }
                // formant shift: read the modulator envelope on a scaled band axis (bands are log-spaced)
                const float shiftBands = std::log(formant) / std::log(0.48f * srf() / 40.0f) * nb;
                for (int b = 0; b < nb; ++b) {
                    const float x = std::clamp(static_cast<float>(b) - shiftBands, 0.0f, static_cast<float>(nb - 1));
                    const int b0 = static_cast<int>(x);
                    const int b1 = std::min(nb - 1, b0 + 1);
                    mod[b] = src[b0] + (x - b0) * (src[b1] - src[b0]);
                }
                // per-band gain: (modulator / carrier) ^ amount, bounded (-36 .. +24 dB), smoothed over time
                float gb[kMaxBands];
                for (int b = 0; b < nb; ++b) {
                    const float cb = cur[b], mb = mod[b];
                    float g = 1.0f;
                    if (cb > 1e-7f) g = std::pow(std::max(mb, 1e-7f) / cb, amount);
                    g = std::clamp(g, 0.0158f, 15.85f);
                    if (cb < 1e-5f) g = std::min(g, 1.0f);  // never lift noise out of silence
                    auto& s = gainSm_[c][static_cast<size_t>(b)];
                    s += sm * (g - s);
                    gb[b] = s;
                }
                auto& G = gbin_[c];
                float eIn = 0.0f, eOut = 0.0f;
                for (int k = 0; k < B_; ++k) {
                    const float x = bandOf_[static_cast<size_t>(k)];
                    const int b0 = static_cast<int>(x);
                    const int b1 = std::min(nb - 1, b0 + 1);
                    G[static_cast<size_t>(k)] = gb[b0] + (x - b0) * (gb[b1] - gb[b0]);
                    eIn += std::norm(X[k]);
                    X[k] *= G[static_cast<size_t>(k)];
                    eOut += std::norm(X[k]);
                }
                // optional level keeping: slow energy match so a dark source does not simply get quieter
                if (keep) {
                    inE_[c] += 0.05f * (eIn - inE_[c]);
                    outE_[c] += 0.05f * (eOut - outE_[c]);
                    const float t = std::clamp(std::sqrt(inE_[c] / std::max(outE_[c], 1e-9f)), 0.25f, 4.0f);
                    lvl_[c] += 0.1f * (t - lvl_[c]);
                    for (int k = 0; k < B_; ++k) X[k] *= lvl_[c];
                }
                dsp::Stft::mirror(X, N);
                w_[c] = (w_[c] + 1) % echoFrames_;
            });
        }
        for (int i = 0; i < n; ++i) {
            const float o = out_.next();
            ch[0][i] = softLimit(ch[0][i] * o, 2.0f);
            ch[1][i] = softLimit(ch[1][i] * o, 2.0f);
        }
        clock_ += n;
        mix_.apply(ch, n);
    }
    int n_ = 1024, hop_ = 256, B_ = 513, echoFrames_ = 200, mappedBands_ = -1;
    int w_[2] = {0, 0}, loud_[2] = {0, 0};
    bool hasHeld_[2] = {false, false}, captureReq_[2] = {false, false}, lastCapture_ = false;
    int64_t lastRefresh_ = INT64_MIN, clock_ = 0;
    float inE_[2] = {1e-6f, 1e-6f}, outE_[2] = {1e-6f, 1e-6f}, lvl_[2] = {1.0f, 1.0f};
    dsp::Stft stft_[2];
    std::vector<float> hist_[2], gainSm_[2], env_[2], held_[2], gbin_[2], bandOf_;
    MixStage mix_;
    dsp::Smoother out_;
};

}  // namespace

FxDescriptor describeSpectralShatter() {
    FxDescriptor d;
    d.typeId = "spectral_shatter";
    d.name = "Spectral Shatter";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Cuts the spectrum into shards and rearranges them on the beat: bands jump to other registers, freeze or drop out, a frequency mosaic of the source.";
    d.params = {
        P::choice("shards", "Shards", {"8", "12", "20", "32"}, 1, "How many frequency bands the spectrum is cut into."),
        P::choice("division", "Step", divisionLabels(), 7, "How often the arrangement changes (tempo-synced)."),
        P::pct("reorder", "Reorder", 60.0f, "How many shards move to another shard's register on each step."),
        P::pct("freeze", "Freeze", 15.0f, "Chance that a shard holds its spectrum for the step."),
        P::pct("gate", "Gate", 15.0f, "Chance that a shard is silenced for the step."),
        P::flt("seed", "Seed", "", 0.0f, 99.0f, 11.0f, "Chooses the arrangements. The same seed always gives the same sequence on the same beats."),
        P::flt("smooth", "Smooth", "frames", 1.0f, 12.0f, 3.0f, "How many analysis frames each change is crossfaded over (1 = abrupt but click-free)."),
        P::flt("tilt", "Tilt", "%", -100.0f, 100.0f, 0.0f, "Emphasises the high (+) or low (-) shards."),
        P::flt("out", "Output", "dB", -24.0f, 12.0f, 0.0f, "Level of the shattered signal."),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and the shattered spectrum."),
    };
    for (int i : {shSeed, shSmooth, shTilt, shOut}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<SpectralShatter>(); };
    return d;
}

FxDescriptor describePartialRecomposer() {
    FxDescriptor d;
    d.typeId = "partial_recompose";
    d.name = "Partial Recomposer";
    d.family = FxFamily::PitchVoice;
    d.tooltip = "Finds the sound's partials and rebuilds them at new spacings: stretched into bells, squeezed, shifted into inharmonic metal, split across the stereo field.";
    d.params = {
        P::flt("stretch", "Stretch", "%", -50.0f, 100.0f, 25.0f, "Spreads the partials apart (+, bell-like, piano-stiff) or squeezes them together (-), around the lowest strong partial."),
        P::flt("shift", "Shift", "Hz", -500.0f, 500.0f, 0.0f, "Moves every partial by the same number of Hz: harmonic sounds become inharmonic."),
        P::flt("odd_even", "Odd / Even", "%", -100.0f, 100.0f, 0.0f, "Removes even (-) or odd (+) partials: hollow clarinet-like or octave-like colours."),
        P::pct("spread", "Partial Pan", 0.0f, "Alternate partials go left and right: the sound splits into two interleaved halves."),
        P::pct("sustain", "Sustain", 0.0f, "Partials hold and fade slowly instead of stopping with the source."),
        P::flt("residual", "Residual", "dB", -60.0f, 6.0f, -6.0f, "Level of everything that is not a partial (breath, noise, attacks), left in place."),
        P::flt("threshold", "Threshold", "dB", -60.0f, -6.0f, -40.0f, "Only partials louder than this (relative to the strongest) are moved."),
        P::flt("out", "Output", "dB", -24.0f, 12.0f, 0.0f, "Level of the recomposed signal."),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and the recomposed sound."),
    };
    for (int i : {prSpread, prThreshold, prOut}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<PartialRecomposer>(); };
    return d;
}

FxDescriptor describeSpectralGraft() {
    FxDescriptor d;
    d.typeId = "spectral_graft";
    d.name = "Spectral Graft";
    d.family = FxFamily::TextureSpectral;
    d.tooltip = "Grafts another spectral shape onto the live sound: a held moment of the input, its own echo from the past, or the opposite channel. (One input: not a sidechain vocoder.)";
    d.params = {
        P::choice("source", "Source", {"Held Moment", "Echo", "Cross L/R"}, 0, "Held Moment: a snapshot of the input (Capture, a tempo refresh, or the first sound). Echo: the input's own spectrum from Echo Time ago. Cross: each channel takes the other's shape."),
        P::pct("amount", "Amount", 80.0f, "How much of the source's spectral shape replaces the live one."),
        P::flt("resolution", "Resolution", "bands", 8.0f, 64.0f, 32.0f, "Detail of the transferred shape: few bands = broad colour, many = formants and resonances."),
        P::logf("echo_ms", "Echo Time", "ms", 20.0f, 4000.0f, 500.0f, "Echo source: how far in the past the shape comes from."),
        P::toggle("capture", "Capture", false, "Turn on to take a new held snapshot (Held Moment)."),
        P::choice("refresh", "Refresh", {"Off", "Every Bar", "Every 2 Bars", "Every 4 Bars"}, 0, "Takes a new held snapshot on a tempo grid."),
        P::flt("formant", "Shape Shift", "st", -12.0f, 12.0f, 0.0f, "Moves the grafted shape up or down the spectrum (bigger / smaller body)."),
        P::logf("smooth", "Smoothing", "ms", 5.0f, 500.0f, 40.0f, "How quickly the transfer follows changes."),
        P::toggle("keep_level", "Keep Level", true, "Keeps the output close to the input level."),
        P::flt("out", "Output", "dB", -24.0f, 12.0f, 0.0f, "Level of the grafted signal."),
        P::pct("mix", "Mix", 100.0f, "Blend of the original and the grafted sound."),
    };
    for (int i : {sgSmooth, sgKeepLevel, sgOut}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<SpectralGraft>(); };
    return d;
}

}  // namespace ih1::fx
