// IH1 audio processor: thin JUCE wrapper around ih1_core (Engine on the audio thread, Session on the
// message thread). Host parameters are a fixed, stable set (decision D1); committed instances are never
// host parameters.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "ih1/engine/Engine.h"
#include "ih1/model/Session.h"

namespace ih1::plugin {

constexpr int kNumMacros = 8;

class Processor final : public juce::AudioProcessor, private juce::Timer {
public:
    Processor();
    ~Processor() override;

    // ---- AudioProcessor -----------------------------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "IH1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return tail_.load(); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass_; }

    // ---- UI API (message thread only) ----------------------------------------------------------
    juce::CriticalSection& sessionLock() { return lock_; }
    Session& session() { return *session_; }         // hold sessionLock() while using
    Telemetry& telemetry() { return engine_.telemetry(); }
    void uiSelect(int typeIndex);
    void uiStep(int direction);
    void uiSetParam(int paramIndex, float plainValue);
    void uiGesture(int paramIndex, bool begin);
    void uiReset();
    bool uiCommit();
    bool uiMove(int from, int to);
    bool uiRemove(uint64_t id);
    void uiClearChain();
    void uiSetSolo(bool on);
    void uiSetMute(bool on);
    bool savePreset(const juce::File& file);
    bool loadPreset(const juce::File& file, juce::String& message);
    uint64_t revision() const { return revision_.load(); }

    juce::AudioProcessorValueTreeState& params() { return apvts_; }

private:
    void timerCallback() override;
    void pullHostParams(bool enforceExclusive);
    void afterModelChange();  // latency, tail, macro sync, revision (lock held)
    void syncMacrosFromDraft();
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    Engine engine_;
    std::unique_ptr<Session> session_;
    juce::CriticalSection lock_;
    juce::AudioProcessorValueTreeState apvts_;

    juce::AudioParameterBool* bypass_ = nullptr;
    std::atomic<float>* pBypass_ = nullptr;
    std::atomic<float>* pSolo_ = nullptr;
    std::atomic<float>* pMute_ = nullptr;
    std::atomic<float>* pIn_ = nullptr;
    std::atomic<float>* pOut_ = nullptr;
    std::atomic<float>* pMix_ = nullptr;
    std::array<juce::RangedAudioParameter*, kNumMacros> macros_{};
    std::array<float, kNumMacros> lastMacro_{};
    bool lastSolo_ = false, lastMute_ = false;

    std::vector<MidiEvent> midi_;
    std::vector<float> monoScratch_;
    std::atomic<double> tail_{0.0};
    std::atomic<uint64_t> revision_{1};
    int reportedLatency_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Processor)
};

}  // namespace ih1::plugin
