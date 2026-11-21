// IH1 — audio-thread engine: committed chain → staged FX, SOLO/MUTE routing, bypass, telemetry.
#pragma once

#include <array>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

#include "ih1/dsp/OutputSafety.h"
#include "ih1/engine/Spsc.h"
#include "ih1/fx/FxProcessor.h"

namespace ih1 {

// Immutable snapshot of what the audio thread should run. Built and freed on the message thread.
struct Program {
    std::array<FxProcessor*, kMaxChain> chain{};
    std::array<uint64_t, kMaxChain> ids{};
    int chainSize = 0;
    FxProcessor* staged = nullptr;
    int stagedType = -1;
    int chainLatency = 0;
    int stagedLatency = 0;
    // Frozen snapshot of every committed instance. Re-applied on every install, so a committed
    // processor's parameters can never drift from its snapshot.
    std::array<ParamValues, kMaxChain> chainValues{};
    // True when this program results from COMMIT: the old staged processor became the last chain slot.
    bool commitHandover = false;
    // V4 Phase 6: per-instance enable / bypass. A separate boolean, never part of the frozen parameter snapshot.
    // A bypassed slot passes its input (delayed by the slot's own latency, so the chain timing never moves).
    // tailOnBypass: Time & Space effects stop receiving new input but let their echoes / reverb ring out; every other
    // effect switches off with a short crossfade (it keeps running underneath so re-enabling is seamless).
    std::array<uint8_t, kMaxChain> enabled{};
    std::array<uint8_t, kMaxChain> tailOnBypass{};
    std::array<int, kMaxChain> slotLatency{};
    // V4 Phase 1.5: preset macros. Each target adds `depth * macro` to one Float parameter of one slot at runtime,
    // clamped to the parameter range. The stored (committed) values never change; macro 0 = exactly the stored chain.
    struct MacroTarget { int macro = 0, slot = 0, param = 0; float depth = 0, lo = 0, hi = 1; };
    static constexpr int kMaxMacroTargets = 48;
    std::array<MacroTarget, kMaxMacroTargets> macroTargets{};
    int numMacroTargets = 0;
};

constexpr int kNumPresetMacros = 4;

struct Telemetry {
    std::atomic<float> inRms{0}, outRms{0}, outPeak{0};
    std::atomic<float> outLow{0}, outMid{0}, outHigh{0};  // coarse band RMS of the output
    std::atomic<float> outBalance{0};                     // (R - L) / (R + L) of the output RMS, -1..1
    std::atomic<float> stagedRms{0};
    std::array<std::atomic<float>, kMaxChain> slotRms{};  // output RMS after each committed slot
    std::array<std::atomic<uint64_t>, kMaxChain> slotIds{};
    std::array<std::atomic<float>, kMaxChain> slotEnabled{};  // V4: per-slot enable ramp 0 (bypassed) .. 1
    // V4 Phase 7.3: per-stage analysis (output of each committed slot): peak, crest factor (dB), onset strength 0..1
    std::array<std::atomic<float>, kMaxChain> slotPeak{};
    std::array<std::atomic<float>, kMaxChain> slotOnset{};
    std::atomic<int> chainSize{0};
    std::atomic<float> soloAmount{0}, muteAmount{0};      // current ramp positions (0..1)
    // Output Safety (V3 Phase 8.5)
    std::atomic<float> preSafetyPeak{0};                  // sample peak entering the safety stage (linear)
    std::atomic<float> safetyGainReduction{0};            // dB (>= 0) of the deepest reduction in the last block
    std::atomic<int> safetyMode{1};                       // mode actually running (OutputSafety::Mode)
    std::atomic<uint32_t> faults{0};                      // non-finite samples caught by the internal guards
};

class Engine {
public:
    Engine();
    ~Engine();

    // Audio stopped. Allocates scratch and compensation buffers.
    void prepare(double sampleRate, int maxBlock);
    double sampleRate() const { return sampleRate_; }
    int maxBlock() const { return maxBlock_; }

    // ---- message thread -------------------------------------------------------------------
    // Post a program; false if the previous one has not been picked up yet (retry later).
    bool post(Program* p);
    // Pull back programs the audio thread no longer uses.
    bool popRetired(Program*& p) { return retired_.pop(p); }
    // Audio stopped only: install immediately. Returns previously active + pending programs via out.
    void installDirect(Program* p, std::vector<Program*>& released);
    // Staged parameter publishing (type-tagged so values never reach the wrong processor).
    void publishStagedParams(int typeIndex, const ParamValues& v);
    void setSolo(bool on) { solo_.store(on); }
    void setMute(bool on) { mute_.store(on); }
    void setBypass(bool on) { bypass_.store(on); }
    void setInputGainDb(float db) { inGainDb_.store(db); }
    void setOutputGainDb(float db) { outGainDb_.store(db); }
    void setMix(float m01) { mix_.store(m01); }
    // V4: preset macro values 0..1 (host-automatable). Smoothed on the audio thread.
    void setMacro(int i, float v01) { if (i >= 0 && i < kNumPresetMacros) macro_[static_cast<size_t>(i)].store(std::clamp(v01, 0.0f, 1.0f)); }
    // Output Safety: mode (OutputSafety::Mode) and ceiling. The caller resolves Zero-latency sessions to Guard.
    void setSafety(int mode, float ceilingDb) { safetyMode_.store(mode); safetyCeil_.store(ceilingDb); }
    // Latency the TruePeak safety mode adds at the prepared rate (for host reporting on the message thread).
    int safetyTruePeakLatency() const { return safety_.truePeakLatency(); }
    Telemetry& telemetry() { return tele_; }
    int activeLatency() const { return activeLatency_.load(); }

    // ---- audio thread ---------------------------------------------------------------------
    void process(float* left, float* right, int numSamples, const ProcessContext& ctx);

    static constexpr int kCompSize = 1 << 17;  // max compensated latency (samples)

private:
    void processChunk(float* L, float* R, int n, const ProcessContext& ctx);
    void beginTransitionIfPending();
    void installNow(Program* p);
    void applyStagedParamsIfChanged();
    void markFresh(const Program* old, const Program* p);
    void applyMacros(bool force);
    void processSlot(int s, float* cL, float* cR, int n, const ProcessContext& ctx);

    enum class Transition { None, OutputFadeOut, OutputFadeIn, StagedFadeOut, StagedFadeIn };

    double sampleRate_ = 48000.0;
    int maxBlock_ = 0;

    Program* active_ = nullptr;
    Program* incoming_ = nullptr;
    std::atomic<Program*> mailbox_{nullptr};
    SpscQueue<Program*, 64> retired_;
    std::atomic<int> activeLatency_{0};

    std::array<std::atomic<float>, kMaxParams> stagedParams_{};
    std::atomic<int> stagedParamType_{-1};
    std::atomic<uint32_t> stagedParamVersion_{0};
    uint32_t appliedVersion_ = 0;

    std::atomic<bool> solo_{false}, mute_{false}, bypass_{false};
    std::atomic<float> inGainDb_{0}, outGainDb_{0}, mix_{1};
    std::atomic<int> safetyMode_{dsp::OutputSafety::Guard};
    std::atomic<float> safetyCeil_{0.0f};
    dsp::OutputSafety safety_;
    // De-click for freshly built processors (V3 Phase 8.2): a new processor's delay lines, latency buffers and
    // envelopes are empty, so its first delayed copy would start mid-waveform. Its input is faded in instead.
    std::array<float, kMaxChain> slotInRamp_{};
    float stagedInRamp_ = 1.0f;
    // V4 per-slot bypass: enable ramp (1 = on), idle flag (bypassed tail has decayed: stop computing it), idle timer,
    // a latency-matched copy of each slot's input, and scratch for the slot's dry / feed signals.
    static constexpr int kSlotDly = 1 << 14;
    std::array<float, kMaxChain> slotOn_{};
    std::array<uint8_t, kMaxChain> slotIdle_{};
    std::array<int, kMaxChain> slotQuiet_{};
    std::vector<float> slotDly_;  // kMaxChain * 2 * kSlotDly
    int slotDlyPos_ = 0;
    std::vector<float> slotTmp_;  // 4 * maxBlock: dry L/R, out L/R
    // macros
    std::array<std::atomic<float>, kNumPresetMacros> macro_{};
    std::array<float, kNumPresetMacros> macroSm_{};
    std::array<float, kNumPresetMacros> macroApplied_{};
    // V4 Phase 4: a discrete staged-parameter change (mode, pattern, sync...) is applied under a 6 ms dip of the staged
    // wet signal into its own (latency-aligned) input, so a switch inside the effect is never heard as a step
    ParamValues pendingStaged_{}, lastStaged_{};
    bool switchPending_ = false, haveLastStaged_ = false;
    float switchAmt_ = 1.0f;
    // stage analysis state
    std::array<float, kMaxChain> slotEnvFast_{}, slotEnvSlow_{};

    // ramps (per-sample linear)
    float soloAmt_ = 0, muteAmt_ = 0, bypassAmt_ = 0, transGain_ = 1, stagedFade_ = 1;
    float inGain_ = 1, outGain_ = 1, mixAmt_ = 1;
    Transition trans_ = Transition::None;

    // buffers
    std::vector<float> chainBuf_, stagedBuf_, sinBuf_;  // 2 * maxBlock each (L then R)
    std::vector<float> rampSolo_, rampWet_, rampBypass_, rampTrans_, rampMix_, rampIn_, rampOut_;
    std::array<Program*, 64> overflow_{};
    int overflowCount_ = 0;
    std::vector<float> dryHist_;                         // 2 * kCompSize ring (input history)
    std::vector<float> sinHist_;                         // 2 * kCompSize ring (staged input history)
    int dryPos_ = 0, sinPos_ = 0;

    // telemetry state
    float bandLowState_[2] = {0, 0}, bandHighState_[2] = {0, 0};
    Telemetry tele_;
};

// Replace NaN/Inf with 0 and clamp to +-limit. Returns true if anything non-finite was found.
bool sanitize(float* x, int n, float limit);

}  // namespace ih1
