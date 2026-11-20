// IH1 — FX processor interface and FX type descriptor.
#pragma once

#include <array>
#include <cmath>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ih1/model/Param.h"

namespace ih1 {

struct MidiEvent {
    int sampleOffset = 0;
    uint8_t status = 0, data1 = 0, data2 = 0;
    int type() const { return status & 0xF0; }
    bool isNoteOn() const { return type() == 0x90 && data2 > 0; }
    bool isNoteOff() const { return type() == 0x80 || (type() == 0x90 && data2 == 0); }
    bool isAllNotesOff() const { return type() == 0xB0 && (data1 == 123 || data1 == 120); }
    bool isSustain() const { return type() == 0xB0 && data1 == 64; }
    bool isPitchBend() const { return type() == 0xE0; }
};

struct ProcessContext {
    double sampleRate = 48000.0;
    double bpm = 120.0;              // host tempo, or 120 if unknown
    bool hostPlaying = false;
    bool ppqValid = false;
    double ppqPosition = 0.0;        // quarter notes at the first sample of this block
    const MidiEvent* midi = nullptr; // events for this block, sorted by sampleOffset
    int numMidi = 0;
    int blockOffset = 0;             // subtract from MidiEvent::sampleOffset to get the index in this call
};

// Read-only live state for the settings panel (written by the audio thread through atomics, read by the
// message thread). Only effects with meaningful internal state (the Looper) provide it.
struct FxDisplay {
    static constexpr int kPeaks = 96;
    int state = 0;          // Looper: 0 waiting, 1 recording, 2 playing
    float position = 0.0f;  // 0..1 playback position in the loop
    float filled = 0.0f;    // 0..1 of the loop recorded
    float seconds = 0.0f;   // loop length
    std::array<float, kPeaks> peaks{};
    // Grain Engine: grain read positions and playhead across the displayed span (0..1, < 0 = none)
    int kind = 0;  // 0 Looper, 1 Grain Engine
    float playhead = -1.0f;
    int activeGrains = 0;
    std::array<float, 24> grains{};
};

// Latency modes (V2.1 Phase 16). Requested globally; each FX type resolves it to the best mode it really
// supports (see LatencyCaps). Quality = full-quality algorithms with their true reported latency.
// Low = shortest sensible analysis windows (reported honestly, may be non-zero). Zero = no lookahead, no
// analysis window, no linear-phase/oversampling buffers: the processor reports 0 and its wet path is
// sample-aligned with its input (verified by tests with an impulse).
enum class LatencyMode : uint8_t { Quality = 0, Low = 1, Zero = 2 };
const char* latencyModeName(LatencyMode m);

// One instance of an effect. All methods except prepare() and the destructor must be real-time safe.
// Audio is always stereo, processed in place: ch[0] = left, ch[1] = right.
class FxProcessor {
public:
    virtual ~FxProcessor() = default;

    // Message thread, before prepare(): which latency variant to build. Default: only one variant exists.
    virtual void setLatencyMode(LatencyMode) {}

    // Message thread (or audio thread stopped): allocate everything for up to maxBlock samples.
    virtual void prepare(double sampleRate, int maxBlock) = 0;
    // Clear internal state (buffers, filters, envelopes). RT-safe, but may be O(buffer size).
    virtual void reset() = 0;
    // Set new parameter targets (plain values, already clamped). snap = jump without smoothing.
    virtual void setParams(const ParamValues& values, bool snap) = 0;
    // Process numSamples (<= maxBlock) in place.
    virtual void process(float* const* ch, int numSamples, const ProcessContext& ctx) = 0;

    virtual int latencySamples() const { return 0; }
    virtual double tailSeconds() const { return 0.0; }
    // Message thread, lock-free. False if this effect has no live display state.
    virtual bool readDisplay(FxDisplay&) const { return false; }
};

enum class FxFamily : uint8_t { TimeSpace, PitchVoice, Modulation, TextureSpectral, ToneDynamics };
const char* familyName(FxFamily f);

// Catalogue categories (V2.1 Phase 4): the nested FX menu and the formation colour families are both driven
// by these, from the registry. Order = menu order.
enum class FxCategory : uint8_t { TimeSpace, PitchVocal, Modulation, GranularTime, Spectral, RhythmGlitch, Dynamics, ToneCharacter, Experimental, Count };
constexpr int kNumCategories = static_cast<int>(FxCategory::Count);
const char* categoryName(FxCategory c);

// What an FX type can do about latency (V2.1 Phase 16). Machine-readable; the UI builds its latency status
// and explanations from this, never from hand-maintained lists.
struct LatencyCaps {
    bool zero = true;                // has a variant with zero reported (and measured) latency
    bool low = true;                 // has a Low variant (if zero is true, Low == Zero unless lowDiffers)
    bool analysisWindow = false;     // Quality variant needs an STFT/analysis window
    bool lookahead = false;          // Quality variant uses lookahead
    bool oversampling = false;       // Quality variant uses linear-phase oversampling filters
    uint8_t cpuClass = 1;            // 1 light, 2 moderate, 3 heavy
    std::string lowNote;             // what changes in Low mode (empty = nothing audible)
    std::string zeroNote;            // what changes in Zero mode (empty = nothing audible)
};

struct FxDescriptor {
    std::string typeId;   // stable machine id (e.g. "tape_echo"); never changes
    std::string name;     // carousel display name
    FxFamily family = FxFamily::TimeSpace;
    std::string tooltip;  // one-line sonic contract shown in the UI
    int version = 1;      // per-type parameter schema version (state migration)
    std::vector<ParamSpec> params;
    std::function<std::unique_ptr<FxProcessor>()> create;
    bool experimental = false;  // hidden from the production carousel
    bool wantsMidi = false;
    FxCategory category = FxCategory::Experimental;
    LatencyCaps latency;

    // Resolve a requested latency mode to the variant this type actually builds.
    LatencyMode resolveLatency(LatencyMode requested) const {
        if (requested == LatencyMode::Zero && !latency.zero) return latency.low ? LatencyMode::Low : LatencyMode::Quality;
        if (requested == LatencyMode::Low && !latency.low) return LatencyMode::Quality;
        return requested;
    }
    // True when parameter i is active for these values (see ParamSpec::enabledBy).
    bool paramEnabled(int i, const ParamValues& v) const {
        if (i < 0 || i >= static_cast<int>(params.size())) return false;
        const auto& s = params[static_cast<size_t>(i)];
        if (s.enabledBy.empty()) return true;
        const int k = paramIndex(s.enabledBy);
        if (k < 0) return true;
        return std::fabs(v[static_cast<size_t>(k)] - s.enabledWhen) < 0.5f;
    }

    int paramIndex(const std::string& id) const {
        for (size_t i = 0; i < params.size(); ++i)
            if (params[i].id == id) return static_cast<int>(i);
        return -1;
    }
    ParamValues defaults() const {
        ParamValues v{};
        for (size_t i = 0; i < params.size(); ++i) v[i] = params[i].defaultValue;
        return v;
    }
    ParamValues clamp(const ParamValues& in) const {
        ParamValues v{};
        for (size_t i = 0; i < params.size(); ++i) v[i] = params[i].clamp(in[i]);
        return v;
    }
};

// Registry of all FX types, in carousel order.
const std::vector<FxDescriptor>& fxRegistry();
int fxTypeIndex(const std::string& typeId);  // -1 if unknown
// Indices of types visible in the production carousel, in order.
const std::vector<int>& fxCarouselOrder();

}  // namespace ih1
