// IH1 — message-thread session model: carousel drafts, staged FX, immutable committed chain, state.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ih1/engine/Engine.h"
#include "ih1/fx/FxProcessor.h"

namespace ih1 {

// A committed FX instance. Immutable: there are no setters; only reorder/delete via Session.
class CommittedInstance {
public:
    CommittedInstance(uint64_t id, int typeIndex, const ParamValues& values, uint32_t seed, FxProcessor* proc)
        : id_(id), type_(typeIndex), values_(values), seed_(seed), proc_(proc) {}

    uint64_t id() const { return id_; }
    int typeIndex() const { return type_; }
    const FxDescriptor& descriptor() const { return fxRegistry()[static_cast<size_t>(type_)]; }
    const ParamValues& values() const { return values_; }
    uint32_t visualSeed() const { return seed_; }
    FxProcessor* processor() const { return proc_; }

private:
    uint64_t id_;
    int type_;
    ParamValues values_;
    uint32_t seed_;
    FxProcessor* proc_;
};

class Session {
public:
    explicit Session(Engine& engine);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Audio stopped (host prepareToPlay). Re-prepares every processor and installs directly.
    void prepare(double sampleRate, int maxBlock);
    // Call regularly on the message thread (timer): posts pending programs, frees retired ones.
    void service();

    // ---- staged / carousel ---------------------------------------------------------------
    int selected() const { return selected_; }       // registry index or -1 (nothing staged)
    void select(int typeIndex);                       // -1 clears the staged FX
    void step(int direction);                         // carousel up (-1) / down (+1)
    const ParamValues& draft(int typeIndex) const { return drafts_[static_cast<size_t>(typeIndex)]; }
    void setDraftParam(int paramIndex, float plainValue);  // edits the selected type's draft only
    void resetDraft();                                     // RESET: selected type's draft -> defaults
    bool commit();                                         // false if nothing staged or chain full

    // ---- committed chain ------------------------------------------------------------------
    const std::vector<CommittedInstance>& chain() const { return chain_; }
    int indexOf(uint64_t id) const;
    bool move(int from, int to);  // reorder (changes DSP order)
    bool remove(uint64_t id);     // delete one instance (confirmation is the UI's job)
    void clearChain();

    // ---- routing ----------------------------------------------------------------------------
    void setSolo(bool on);  // mutually exclusive with MUTE
    void setMute(bool on);
    bool solo() const { return solo_; }
    bool mute() const { return mute_; }

    int latencySamples() const;  // committed + staged
    double tailSeconds() const;
    uint64_t revision() const { return revision_; }  // bumps on any model change (for the UI)

    // ---- state ---------------------------------------------------------------------------
    // includeSession = DAW project state (drafts, selection, SOLO/MUTE). false = user preset.
    std::string toJson(bool includeSession) const;
    // On failure the current state is left untouched and `error` says why.
    bool fromJson(const std::string& text, bool includeSession, std::string* error = nullptr);

    double sampleRate() const { return sampleRate_; }

private:
    std::unique_ptr<FxProcessor> makeProcessor(int typeIndex, const ParamValues& values);
    FxProcessor* adopt(std::unique_ptr<FxProcessor> p);
    void rebuild(bool commitHandover, bool post = true);
    void tryPost();
    void releaseProgram(Program* p);
    void collectGarbage();
    void syncRouting();
    uint64_t newId();

    Engine& engine_;
    double sampleRate_ = 48000.0;
    int maxBlock_ = 512;

    std::vector<ParamValues> drafts_;
    int selected_ = -1;
    FxProcessor* staged_ = nullptr;
    bool solo_ = false, mute_ = false;
    std::vector<CommittedInstance> chain_;

    std::unordered_map<FxProcessor*, std::unique_ptr<FxProcessor>> owned_;
    std::unordered_map<FxProcessor*, int> refs_;  // references from programs not yet retired
    Program* pending_ = nullptr;                  // built but not yet accepted by the engine
    uint64_t revision_ = 1;
    uint64_t idState_ = 0;
};

std::string idToHex(uint64_t id);
uint64_t idFromHex(const std::string& s);

}  // namespace ih1
