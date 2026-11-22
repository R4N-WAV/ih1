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
    uint64_t uiCommit();  // id of the new committed instance, 0 if nothing was committed
    bool uiMove(int from, int to);
    bool uiRemove(uint64_t id);
    void uiClearChain();
    // V4 Phase 6: click a committed FX name to bypass / enable that instance (its parameters never change)
    bool uiToggleEnabled(uint64_t id);
    // V4 Phase 1.5: preset macros (names of the recalled chain's macros; empty string = no live target)
    std::vector<juce::String> presetMacroNames();
    juce::String presetMacroDescription(int k);  // V5: "Bloom: Reverb Mix +30, Delay Mix +20" and its combination
    float presetMacroValue(int k) const;
    void uiSetPresetMacro(int k, float v01);
    void uiPresetMacroGesture(int k, bool begin);
    void uiSetSolo(bool on);
    void uiSetMute(bool on);
    // Latency mode (V2.1 Phase 16). Message thread. Rebuilds the processor variants off the audio thread and
    // reports the new latency to the host (JUCE -> VST3 restartComponent(kLatencyChanged)).
    void uiSetLatencyMode(LatencyMode m);
    struct LatencyStatus {
        LatencyMode mode = LatencyMode::Quality;
        int samples = 0;            // reported to the host (== what the engine runs)
        juce::String headline;      // e.g. "Zero Latency - 0 samples"
        juce::String detail;        // which effects add latency and why (from the registry), for the tooltip
        bool degraded = false;      // the requested mode could not be met fully
    };
    LatencyStatus latencyStatus();
    // Output Safety (V3 Phase 8.5). Message thread.
    struct SafetyStatus {
        int requested = 1, running = 1;   // dsp::OutputSafety::Mode
        float ceilingDb = -0.5f;
        int latencySamples = 0;           // added by the running mode (TruePeak lookahead), included in the host report
        bool zeroFallback = false;        // TruePeak requested in Zero latency mode: Guard runs instead
        bool riskyWhileOff = false;       // Off while the chain / staged FX uses high feedback or drive
        juce::String headline, detail;
    };
    SafetyStatus safetyStatus();
    void uiSetSafetyMode(int mode);
    void uiSetSafetyCeiling(float db);
    uint64_t revision() const { return revision_.load(); }

    // ---- presets (message thread) -------------------------------------------------------------
    // Keys: "f:<factory id>" or "u:<absolute file path>". Order: factory categories, then user presets.
    // meta (V4): intensity, intended sources and style inspiration; searched by the browser and shown under the text
    struct PresetEntry { juce::String key, name, category, description; bool factory = true; juce::String meta; };
    std::vector<PresetEntry> presetList();
    bool loadPresetKey(const juce::String& key, juce::String& message);   // atomic: all or nothing
    bool stepPreset(int direction, juce::String& message);                // wraps at both ends
    juce::String currentPresetKey() const { return presetKey_; }
    juce::String currentPresetName() const { return presetName_; }
    bool presetModified();                                                // chain differs from the loaded preset
    bool saveUserPreset(const juce::String& name, bool overwrite, juce::String& message, bool& exists);
    bool renameUserPreset(const juce::String& key, const juce::String& newName, juce::String& message);
    bool deleteUserPreset(const juce::String& key, juce::String& message);
    static juce::File userPresetDir();

    double hostBpm() const { return hostBpm_.load(); }
    juce::Point<int> lastEditorSize() const { return {editorW_.load(), editorH_.load()}; }
    void setLastEditorSize(int w, int h) { editorW_.store(w); editorH_.store(h); }

    juce::AudioProcessorValueTreeState& params() { return apvts_; }

private:
    void timerCallback() override;
    void pullHostParams(bool enforceExclusive);
    void afterModelChange();  // latency, tail, macro sync, revision (lock held)
    void syncMacrosFromDraft();
    uint64_t seenMacroGen_ = 0;
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
    std::atomic<float>* pSafetyMode_ = nullptr;
    std::atomic<float>* pSafetyCeil_ = nullptr;
    std::atomic<int> safetyRun_{1};   // effective safety mode for the audio thread
    int lastSafetyReq_ = -1;
    int effectiveSafetyMode() const;  // lock held
    bool chainIsRisky() const;        // lock held
    void applyRecallHostState();      // lock held: SOLO / MUTE off, staged macros re-synced, host told
    void resetGlobalsToNeutral();     // Init: input / output gain, mix, safety to their defaults
    std::array<juce::RangedAudioParameter*, kNumMacros> macros_{};
    std::array<juce::RangedAudioParameter*, kNumPresetMacros> presetMacros_{};
    std::array<std::atomic<float>*, kNumPresetMacros> pPresetMacro_{};
    std::array<float, kNumMacros> lastMacro_{};
    bool lastSolo_ = false, lastMute_ = false;

    std::vector<MidiEvent> midi_;
    std::vector<float> monoScratch_;
    std::atomic<double> tail_{0.0};
    std::atomic<uint64_t> revision_{1};
    int reportedLatency_ = 0;
    std::array<double, kNumMacros> uiEditMs_{};  // last UI edit per macro (host echoes are absorbed for 400 ms)
    std::atomic<double> hostBpm_{120.0};
    std::atomic<int> editorW_{0}, editorH_{0};
    juce::String presetKey_, presetName_;
    std::vector<PresetEntry> listCache_;
    double listCacheMs_ = -1e9;
    void invalidatePresetList() { listCacheMs_ = -1e9; }
    std::string presetSig_;      // chain signature when the preset was loaded / saved
    std::string chainSignature() const;  // lock held
    bool applyPresetJson(const std::string& text, const juce::String& key, const juce::String& name, juce::String& message);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Processor)
};

}  // namespace ih1::plugin
