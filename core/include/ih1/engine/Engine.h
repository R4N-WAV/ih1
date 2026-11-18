// IH1 — audio-thread engine: committed chain → staged FX, SOLO/MUTE routing, bypass, telemetry.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

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
};

struct Telemetry {
    std::atomic<float> inRms{0}, outRms{0}, outPeak{0};
    std::atomic<float> outLow{0}, outMid{0}, outHigh{0};  // coarse band RMS of the output
    std::atomic<float> stagedRms{0};
    std::array<std::atomic<float>, kMaxChain> slotRms{};  // output RMS after each committed slot
    std::array<std::atomic<uint64_t>, kMaxChain> slotIds{};
    std::atomic<int> chainSize{0};
    std::atomic<float> soloAmount{0}, muteAmount{0};      // current ramp positions (0..1)
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
