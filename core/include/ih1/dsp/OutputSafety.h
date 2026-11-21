// IH1 Output Safety (V3 Phase 8.5): the final user-visible output protection, after every committed FX, the staged
// FX, global mix and output gain.
//
//   Off      : no limiting. The engine's internal fault guards (finite-value checks, bounded feedback, de-click
//              transitions, the +12 dBFS last-resort clamp) stay active regardless.
//   Guard    : zero latency. Instant-attack, smooth-release sample-peak limiter: no sample leaves above the ceiling.
//              It cannot see inter-sample peaks and is NOT true-peak protection. Heavy overs are effectively clipped
//              on the first sample (no lookahead exists to do better without latency).
//   TruePeak : lookahead limiter with a 4x oversampled (BS.1770-style) peak detector. The gain ramps down across the
//              lookahead window so it reaches its target before the peak arrives (min-hold + box average, KADAL
//              KNW-S6PPRRX2EC / CLM-5HTBR5YBFQ). Adds latency(); the plug-in reports it to the host. A residual
//              under-read of the 4x estimate (worst case ~0.7 dB near Nyquist, CLM-DGG9BKGM5K) is covered by a
//              0.5 dB internal margin; this is an estimate, not a guarantee for the downstream converter.
//
// A plug-in cannot guarantee speaker or hearing safety: interface, amplifier and monitor gain are outside it.
// Real-time safe: everything is sized in prepare(); process() never allocates or locks.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ih1::dsp {

class OutputSafety {
public:
    enum Mode { Off = 0, Guard = 1, TruePeak = 2 };
    static constexpr int kTaps = 32;      // 4 phases x 8 taps (interpolation FIR for the detector)
    static constexpr int kPhases = 4;
    static constexpr int kMaxLookahead = 512;

    void prepare(double sr, int maxBlock);
    void reset();
    // Message thread: request a mode / ceiling. A mode change is applied by the audio thread through a 5 ms fade.
    void setMode(int m) { reqMode_ = std::clamp(m, 0, 2); }
    void setCeilingDb(float db) { reqCeil_ = std::clamp(db, -24.0f, 0.0f); }
    void setReleaseMs(float ms) { releaseMs_ = std::clamp(ms, 5.0f, 2000.0f); }
    int mode() const { return mode_; }
    // Latency of the TruePeak mode at the prepared sample rate (0 in the other modes).
    int truePeakLatency() const { return lookahead_ + kDelayFir; }
    int latency() const { return mode_ == TruePeak ? truePeakLatency() : 0; }

    // Audio thread, in place. Returns the smallest gain applied in this call (1 = no reduction).
    float process(float* L, float* R, int n);
    // Sample peak of the last processed call's output, linear.
    float lastPeak() const { return lastTp_; }

    // 4x interpolated true-peak estimate of a buffer (offline helper for tests and the audit tool).
    static float estimateTruePeak(const float* x, int n);

private:
    static constexpr int kDelayFir = kTaps / kPhases / 2;  // detector group delay in base samples (4)
    static const std::array<float, kTaps>& fir();
    float detect(int c, float x);  // pushes x, returns the max |interpolated| around it

    double sr_ = 48000.0;
    int reqMode_ = Guard, mode_ = Guard;
    float reqCeil_ = 0.0f, ceil_ = 1.0f, releaseMs_ = 120.0f;
    float relCoef_ = 0.999f;
    float fade_ = 1.0f, fadeStep_ = 0.004f;
    bool fadingOut_ = false;
    // Guard
    float gGuard_ = 1.0f;
    // TruePeak
    int lookahead_ = 72;
    std::array<std::array<float, kTaps / kPhases>, 2> hist_{};  // detector input history per channel
    int histPos_ = 0;
    std::vector<float> delayL_, delayR_;  // audio delay line (lookahead + detector delay)
    int dPos_ = 0, dSize_ = 1;
    std::vector<float> gtRing_;           // target gains over the lookahead window (for the sliding min)
    std::vector<int> dq_;                 // monotonic deque (indices) for the sliding minimum
    int dqHead_ = 0, dqTail_ = 0;
    int64_t t_ = 0;
    std::vector<float> boxRing_;
    double boxSum_ = 0.0;
    int boxPos_ = 0;
    float hRel_ = 1.0f;
    float lastTp_ = 0.0f;
};

}  // namespace ih1::dsp
