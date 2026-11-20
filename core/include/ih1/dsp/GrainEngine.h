// IH1 Grain Engine (V2.1 Phase 6): a buffer-based transformation engine. Incoming audio is continuously
// written into a bounded stereo capture ring; up to kMaxVoices overlapping grains read it back with their
// own start, length, pitch, direction, window and pan. Everything is preallocated in prepare(); process
// never allocates, locks or does I/O.
//
// Research basis (KADAL): KNW-01J68SK1RC (grain size/density/envelope/scheduling, and its validation list:
// flat envelope sum, pitch under stretch, bounded pool), KNW-Y48Y80179M + SRC-5Q7S4HYRH5 (overlap-add vs
// WSOLA vs phase vocoder for time-scale modification), KNW-M8Q0G3M7NM (interpolated reads),
// KNW-H0EPVHBYCE (real-time rules). Output's public Portal manual is a workflow reference only (documented
// controls: density, size, count, offset, ratio lock, pitch + scale, pan, stretch speed, retrigger, grain
// delay, feedback with HP/LP); none of its code or presets are used.
//
// Modes
//   Cloud   : grains start `position` behind the live input (+ random spray). Classic granular smear.
//   Stretch : a playhead scans the captured audio at `speed` x real time (-2..2). Grains are taken from the
//             playhead, so playback speed and pitch are independent (granular time stretch). A live buffer
//             has no future: at speed > 1 the playhead catches up with the input and then follows it.
//             `varispeed` makes pitch follow the speed instead (tape-like repitch).
//   Loop    : captures the last `loop` ms and holds it; the playhead cycles through that segment at `speed`
//             (negative = reversed). It arms first: the segment is latched only once that much real (non-silent)
//             audio has arrived, so a preset or a gesture made during silence never loops silence.
//   Freeze  : (any mode) stops writing, also after arming (0.75 s of real signal). Cloud reads a frozen
//             moment; Stretch scans the frozen buffer as an endless loop.
//   Live Stretch slower than 1x drifts behind the input; when it reaches the end of the buffer it wraps back
//   to the Offset point (an implicit retrigger), so long sessions keep stretching recent material.
//   Retrigger resets the playhead to its anchor on a (tempo-synced) grid and fires a grain: slice/stutter.
//
// Gain: each grain's window^2 is summed per sample; the output is scaled by 1/sqrt(max(1, E * K)) with E the
// smoothed envelope energy and K the measured coherence (power of the sum / sum of the grains' own powers,
// smoothed over ~120 ms). Uncorrelated grains (K ~ 1) get power normalisation; grains reading the same audio
// in phase (K -> N) get amplitude normalisation. So density / size / count / spray changes do not jump in
// level, and sparse grains are never boosted. A soft ceiling (+6 dBFS knee, < +10 dBFS) guards the rest.
//
// Splice alignment ("Smooth"): in Stretch/Loop/Freeze a new grain's start is moved by up to +-R samples to
// the offset whose waveform best matches (normalised cross-correlation) what the most recent grain is
// reading now (a WSOLA-style search). This removes most of the phasiness and the pitch slip that fixed-hop
// granular stretching produces on tonal material (KADAL CLM-EA2FBZCRGN).
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/dsp/DelayLine.h"
#include "ih1/dsp/Denormals.h"
#include "ih1/dsp/Svf.h"

namespace ih1::dsp {

class GrainEngine {
public:
    static constexpr int kMaxVoices = 64;
    static constexpr int kWindows = 5;
    static constexpr int kDisplayGrains = 24;
    enum Mode { Cloud = 0, Stretch = 1, Loop = 2 };
    enum Scale { ScaleOff = 0, Octaves, Fifths, Major, Minor, Pentatonic, Chromatic };

    struct Params {
        int mode = Cloud;
        bool freeze = false;
        float sizeMs = 120.0f;
        float density = 20.0f;      // grains per second (per channel stream when independent)
        int voices = 24;            // simultaneous grain limit (<= kMaxVoices)
        float positionMs = 300.0f;  // Cloud: how far behind live; Stretch/Retrig anchor
        float sprayMs = 80.0f;      // random extra distance
        float sizeJitter = 0.0f;    // 0..1
        float pitchSt = 0.0f;
        float pitchJitterSt = 0.0f;
        int scale = ScaleOff;
        float reverseProb = 0.0f;   // 0..1
        int window = 0;             // 0 Hann, 1 Gauss, 2 Tukey, 3 Pluck, 4 Swell
        float spread = 0.5f;        // 0..1 pan randomisation
        bool independent = false;  // separate L/R grain streams
        float speed = 0.5f;         // Stretch/Loop playhead rate (x real time), -2..2
        bool varispeed = false;     // pitch follows the playhead speed
        float loopMs = 1000.0f;
        bool retrig = false;
        double retrigSamples = 24000.0;
        bool align = true;
        float delayMs = 0.0f;       // grain delay
        float feedback = 0.0f;      // 0..0.95
        float fbHp = 80.0f, fbLp = 9000.0f;
    };

    void prepare(double sr, float bufferSeconds, int maxBlock, uint32_t seed);
    void reset();
    // Process n samples. in/out may alias. Output is the wet signal only.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n, const Params& p);

    // Display (audio thread writes, message thread reads; lock-free, approximate).
    struct Display {
        std::array<std::atomic<float>, 96> peaks{};
        std::array<std::atomic<float>, kDisplayGrains> grainPos{};  // 0..1 across the displayed span, <0 = none
        std::atomic<float> playhead{-1.0f};
        std::atomic<int> active{0};
        std::atomic<bool> frozen{false};
        std::atomic<float> spanSeconds{4.0f};
        std::atomic<int> peakHead{0};  // next bin to be written: peaks[peakHead] is the oldest
    };
    const Display& display() const { return disp_; }
    int activeVoices() const;

private:
    struct Grain {
        bool active = false;
        double pos = 0.0;   // absolute sample index (fractional) into the capture timeline
        double inc = 1.0;   // read increment (negative = reverse)
        float t = 0.0f, len = 1.0f;
        float gl = 1.0f, gr = 1.0f;
        int win = 0;
        int chan = -1;      // -1 = stereo (linked), 0/1 = single channel (independent streams)
        uint32_t order = 0; // spawn order (for alignment: the most recent grain)
    };

    float readCh(int c, double pos) const;   // 4-point Hermite
    float readMono(double pos) const { return 0.5f * (readCh(0, pos) + readCh(1, pos)); }
    void spawn(const Params& p, int chan, bool forced);
    double alignStart(double start, const Grain& ref, float len) const;
    float quantise(float st, int scale) const;
    void updateDisplay(const Params& p);

    double sr_ = 48000.0;
    std::array<std::vector<float>, 2> buf_;
    int64_t mask_ = 0;
    int64_t head_ = 0;      // absolute index of the next sample to write (= live "now")
    int64_t filled_ = 0;
    std::array<Grain, kMaxVoices> grains_{};
    std::array<std::vector<float>, kWindows> win_;
    static constexpr int kWinSize = 1024;
    float countdown_[2] = {0.0f, 0.0f};
    double playhead_ = 0.0;
    double loopStart_ = 0.0, loopLen_ = 0.0;
    int lastMode_ = -1;
    bool lastFreeze_ = false;
    bool latched_ = false;   // Loop / Freeze: armed until enough real signal has been captured
    int64_t heard_ = 0;      // non-silent samples captured (since reset)
    double retrigCount_ = 0.0;
    float energy_ = 0.0f, energyA_ = 0.0f;
    // coherence: smoothed power of the summed grains vs the sum of each grain's own power. ~1 when grains
    // are uncorrelated, up to N when N grains read (nearly) the same audio in phase.
    float sumPow_ = 0.0f, indPow_ = 0.0f, coherA_ = 0.0f;
    uint32_t spawnOrder_ = 0;
    uint32_t seed_ = 1;
    Rng rng_;
    DelayLine delay_[2];
    Svf fbHp_[2], fbLp_[2];
    float fbState_[2] = {0.0f, 0.0f};
    float dcX_[2] = {0.0f, 0.0f}, dcY_[2] = {0.0f, 0.0f};
    float lastHp_ = -1.0f, lastLp_ = -1.0f;
    // display: running peak bins over the most recent span
    int binLen_ = 1, binCount_ = 0, binIndex_ = 0;
    float binPeak_ = 0.0f;
    Display disp_;
};

}  // namespace ih1::dsp
