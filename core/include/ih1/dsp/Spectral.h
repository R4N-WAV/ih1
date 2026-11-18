// Spectral analysis/synthesis helpers for pitch, harmony and formant processing.
//  * PeakShifter: Laroche-Dolson "new phase-vocoder" pitch shifting (KADAL SRC-3Q84R4ZMZG): peaks and
//    their regions of influence are translated to beta*omega with phase-locked synthesis; several voices
//    reuse one analysis, so harmonizing costs one synthesis pass per voice, not one STFT per voice.
//  * True-envelope estimation (KADAL SRC-VWKQGV9FYC, CLM-ZQQSPJT8PN): iterated cepstral smoothing,
//    A_i = max(A_{i-1}, V_{i-1}); order from f0 (Pc ~ R / (2 f0)).
//  * Yin: YIN-style f0 tracker (KADAL KNW-Q1PTF3K9KM) on a decimated mono stream.
//  * StftMonoToStereo: one analysis, two overlap-added outputs (harmony voices panned in L/R).
#pragma once

#include <vector>

#include "ih1/dsp/Fft.h"

namespace ih1::dsp {

class SpectralAnalysis {
public:
    void prepare(int fftSize, int hop);
    void reset();
    // Analyse one windowed frame spectrum (full N bins). Fills magnitude, phase, inst. frequency, peaks.
    void analyse(const cfloat* X);
    // True envelope (linear magnitude, N/2+1 bins) with the given cepstral order. The log spectrum is
    // first max-filtered over +-maxHalfWidth bins (about one harmonic spacing; the acceleration noted in
    // KADAL SRC-VWKQGV9FYC) so the iterations start near the upper envelope and converge in 2-3 passes.
    void estimateEnvelope(int order, int iterations = 3, int maxHalfWidth = 4);

    int n() const { return n_; }
    int bins() const { return n_ / 2 + 1; }
    std::vector<float> mag, phase, instFreq, env;  // instFreq in radians/sample
    std::vector<int> peaks, regionLo, regionHi;

private:
    int n_ = 0, hop_ = 0;
    std::vector<float> prevPhase_;
    Fft fft_;
    std::vector<cfloat> cep_;
    std::vector<float> logA_;
};

class PeakShifter {
public:
    void prepare(int fftSize, int hop);
    void reset();
    // Accumulate one shifted voice into out (N/2+1 bins). formantRatio: 1 = keep formants where they
    // were (with preserveFormants), >1 moves formants up. gain is linear.
    void addVoice(const SpectralAnalysis& a, float ratio, float formantRatio, bool preserveFormants,
                  float gain, cfloat* outL, cfloat* outR, float gl, float gr, int voice);

private:
    int n_ = 0, hop_ = 0;
    static constexpr int kMaxVoices = 4;
    std::vector<float> outPhase_[kMaxVoices];
};

class Yin {
public:
    void prepare(double sr, int decimation = 2, int window = 512);
    void push(float x);         // full-rate input (decimated internally)
    float estimate();           // run detection on the latest window; returns f0 Hz or 0 if unvoiced
    float lastF0() const { return f0_; }
    float confidence() const { return conf_; }

private:
    double sr_ = 48000.0;
    int dec_ = 2, w_ = 512, maxTau_ = 400, minTau_ = 20, pos_ = 0, decCount_ = 0;
    std::vector<float> buf_, d_;
    float lp_ = 0.0f, lp2_ = 0.0f, f0_ = 0.0f, conf_ = 0.0f;
};

class StftMonoToStereo {
public:
    void prepare(int fftSize, int hop);
    void reset();
    int latency() const { return n_; }
    // fn(const cfloat* inSpectrum, cfloat* outL, cfloat* outR, int n): outL/outR (N bins) are zeroed;
    // fill bins 0..N/2 — mirroring is done here.
    template <typename Fn>
    void process(const float* in, float* outL, float* outR, int numSamples, Fn&& fn) {
        const int off = n_ - hop_;
        for (int i = 0; i < numSamples; ++i) {
            inFifo_[static_cast<size_t>(rover_)] = in[i];
            outL[i] = outFifoL_[static_cast<size_t>(rover_ - off)];
            outR[i] = outFifoR_[static_cast<size_t>(rover_ - off)];
            if (++rover_ >= n_) {
                rover_ = off;
                for (int k = 0; k < n_; ++k) frame_[static_cast<size_t>(k)] = cfloat(inFifo_[static_cast<size_t>(k)] * win_[static_cast<size_t>(k)], 0.0f);
                fft_.forward(frame_.data());
                std::fill(yl_.begin(), yl_.end(), cfloat());
                std::fill(yr_.begin(), yr_.end(), cfloat());
                fn(frame_.data(), yl_.data(), yr_.data(), n_);
                Stft::mirror(yl_.data(), n_);
                Stft::mirror(yr_.data(), n_);
                fft_.inverse(yl_.data());
                fft_.inverse(yr_.data());
                for (int k = 0; k < n_; ++k) {
                    accL_[static_cast<size_t>(k)] += yl_[static_cast<size_t>(k)].real() * win_[static_cast<size_t>(k)] * norm_;
                    accR_[static_cast<size_t>(k)] += yr_[static_cast<size_t>(k)].real() * win_[static_cast<size_t>(k)] * norm_;
                }
                for (int k = 0; k < hop_; ++k) { outFifoL_[static_cast<size_t>(k)] = accL_[static_cast<size_t>(k)]; outFifoR_[static_cast<size_t>(k)] = accR_[static_cast<size_t>(k)]; }
                for (int k = 0; k < n_; ++k) {
                    const bool in = k + hop_ < n_;
                    accL_[static_cast<size_t>(k)] = in ? accL_[static_cast<size_t>(k + hop_)] : 0.0f;
                    accR_[static_cast<size_t>(k)] = in ? accR_[static_cast<size_t>(k + hop_)] : 0.0f;
                }
                for (int k = 0; k < off; ++k) inFifo_[static_cast<size_t>(k)] = inFifo_[static_cast<size_t>(k + hop_)];
            }
        }
    }

private:
    int n_ = 0, hop_ = 0, rover_ = 0;
    float norm_ = 1.0f;
    Fft fft_;
    std::vector<float> win_, inFifo_, outFifoL_, outFifoR_, accL_, accR_;
    std::vector<cfloat> frame_, yl_, yr_;
};

// STFT size for a sample rate: 2048 at 44.1/48 kHz, scaled by powers of two above.
int stftSizeFor(double sr, int base48k = 2048);

}  // namespace ih1::dsp
