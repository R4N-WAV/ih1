// Radix-2 complex FFT (precomputed twiddles, no allocation after prepare) and an STFT overlap-add
// frame scheduler with exactly known latency (fftSize).
#pragma once

#include <complex>
#include <vector>

namespace ih1::dsp {

using cfloat = std::complex<float>;

class Fft {
public:
    void prepare(int size);  // power of two
    int size() const { return n_; }
    void forward(cfloat* data) const { transform(data, false); }
    void inverse(cfloat* data) const { transform(data, true); }  // unscaled

private:
    void transform(cfloat* d, bool inv) const;
    int n_ = 0;
    std::vector<cfloat> tw_;
    std::vector<int> rev_;
};

// Per-channel STFT. Call process() with a frame callback: void(cfloat* spectrum, int fftSize).
// The callback receives the full complex spectrum of a Hann-windowed frame and may modify it
// (keep Hermitian symmetry, or only edit bins 0..N/2 and call mirror()).
class Stft {
public:
    void prepare(int fftSize, int hop);
    int latency() const { return n_; }          // measured: an impulse re-appears exactly fftSize later
    int fifoOffset() const { return n_ - hop_; }
    int fftSize() const { return n_; }
    int hop() const { return hop_; }
    void reset();
    static void mirror(cfloat* spec, int n);  // rebuild bins N/2+1..N-1 from 0..N/2

    template <typename FrameFn>
    void process(float* io, int numSamples, FrameFn&& fn) {
        for (int i = 0; i < numSamples; ++i) {
            inFifo_[static_cast<size_t>(rover_)] = io[i];
            io[i] = outFifo_[static_cast<size_t>(rover_ - fifoOffset())];
            if (++rover_ >= n_) {
                rover_ = fifoOffset();
                for (int k = 0; k < n_; ++k) frame_[static_cast<size_t>(k)] = cfloat(inFifo_[static_cast<size_t>(k)] * win_[static_cast<size_t>(k)], 0.0f);
                fft_.forward(frame_.data());
                fn(frame_.data(), n_);
                fft_.inverse(frame_.data());
                for (int k = 0; k < n_; ++k) accum_[static_cast<size_t>(k)] += frame_[static_cast<size_t>(k)].real() * win_[static_cast<size_t>(k)] * norm_;
                for (int k = 0; k < hop_; ++k) outFifo_[static_cast<size_t>(k)] = accum_[static_cast<size_t>(k)];
                for (int k = 0; k < n_; ++k) accum_[static_cast<size_t>(k)] = (k + hop_ < n_) ? accum_[static_cast<size_t>(k + hop_)] : 0.0f;
                for (int k = 0; k < fifoOffset(); ++k) inFifo_[static_cast<size_t>(k)] = inFifo_[static_cast<size_t>(k + hop_)];
            }
        }
    }

private:
    int n_ = 0, hop_ = 0, rover_ = 0;
    float norm_ = 1.0f;
    Fft fft_;
    std::vector<float> win_, inFifo_, outFifo_, accum_;
    std::vector<cfloat> frame_;
};

}  // namespace ih1::dsp
