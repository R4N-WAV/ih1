// IH1 — FX processor interface and FX type descriptor.
#pragma once

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

// One instance of an effect. All methods except prepare() and the destructor must be real-time safe.
// Audio is always stereo, processed in place: ch[0] = left, ch[1] = right.
class FxProcessor {
public:
    virtual ~FxProcessor() = default;

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
};

enum class FxFamily : uint8_t { TimeSpace, PitchVoice, Modulation, TextureSpectral, ToneDynamics };
const char* familyName(FxFamily f);

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
