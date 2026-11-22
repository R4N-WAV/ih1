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
    // V4 Phase 6: enabled / bypassed. A separate boolean: bypass never edits the frozen parameter values.
    bool enabled() const { return enabled_; }

private:
    friend class Session;
    bool enabled_ = true;
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
    // `None` (V3 Phase 1/2) is a first-class selection: no uncommitted effect is auditioned and no staged processor
    // exists in the audio program. It is never a registry index, so it can never alias the first FX.
    static constexpr int kNone = -1;
    bool hasStaged() const { return selected_ != kNone && staged_ != nullptr; }
    int selected() const { return selected_; }       // registry index or kNone
    const FxProcessor* stagedProcessor() const { return staged_; }  // message thread only (display read-outs)
    void select(int typeIndex);                       // -1 clears the staged FX
    void step(int direction);                         // carousel up (-1) / down (+1)
    const ParamValues& draft(int typeIndex) const { return drafts_[static_cast<size_t>(typeIndex)]; }
    void setDraftParam(int paramIndex, float plainValue);  // edits the selected type's draft only
    void resetDraft();                                     // RESET: selected type's draft -> defaults
    // COMMIT (V5 Phase 7): the staged processor (warmed up, with the current draft) becomes the immutable committed
    // instance at the end of the chain. The carousel keeps the same effect selected; a fresh staged processor of that
    // type is prepared at the effect's defaults, the draft returns to the defaults, and the staged stage is MUTED
    // from the first sample ("standby"), so the committed copy is never doubled. Returns false if nothing is staged
    // or the chain is full.
    bool commit();
    // Standby (V5): muted by COMMIT rather than by the MUTE button. The browser and controls stay usable; any
    // deliberate audition gesture (MUTE off, selecting an effect, editing a control through auditionEdit) ends it.
    bool standby() const { return standby_ && mute_; }
    // A user edit of a staged control (UI gesture, not host automation): ends standby and un-mutes. Returns true if
    // the mute state changed (the caller syncs its host MUTE parameter).
    bool auditionEdit();

    // ---- committed chain ------------------------------------------------------------------
    const std::vector<CommittedInstance>& chain() const { return chain_; }
    int indexOf(uint64_t id) const;
    bool move(int from, int to);  // reorder (changes DSP order)
    bool remove(uint64_t id);     // delete one instance (confirmation is the UI's job)
    void clearChain();
    // V4 Phase 6: enable / bypass one committed instance (click-free in the engine). Saved with presets and projects.
    bool setEnabled(uint64_t id, bool on);
    bool toggleEnabled(uint64_t id);

    // ---- preset macros (V4 Phase 1.5) -------------------------------------------------------------
    // A macro is a modulation source owned by the recalled chain: each target adds depth * value (value 0..1) to one
    // continuous parameter of one committed instance at runtime, clamped to its range. The committed values are
    // never modified (invariant 4); macro value 0 reproduces the stored chain exactly. Targets follow instance ids
    // through reorder; a deleted instance's targets disappear. Values are host parameters (the plug-in sets them).
    struct MacroTargetDef { uint64_t id = 0; std::string param; float depth = 0.0f; };
    struct MacroDef { std::string name; std::vector<MacroTargetDef> targets; };
    const std::vector<MacroDef>& macros() const { return macros_; }
    int activeMacroCount() const;  // macros with at least one live target
    // V5 Phase 8: macros are either AUTHORED (a factory preset's curated set, kept while any of their targets lives)
    // or GENERATED from the committed chain's effect combination (comboMacros), recomputed after every chain change.
    // No eligible combination = no macros (the knobs are hidden). generation() changes whenever the set changes, so
    // the host-facing macro values can return to 0 for a new set.
    bool macrosGenerated() const { return macrosAuto_; }
    uint64_t macroGeneration() const { return macroGen_; }
    const std::string& macroCombination() const { return comboName_; }  // e.g. "Drum Bus + Atmosphere"
    // "Bloom: Reverb Mix +30, Delay Mix +20" (plain-language mapping for tooltips)
    std::string macroDescription(int m) const;
    // Init (V3 Phase 7.1): the explicit clean starting state. No committed FX, staged = None, every draft back to its
    // defaults (the staged cache is cleared only here and by RESET), SOLO / MUTE off. Every processor is retired,
    // so no loop, freeze, feedback or modulation memory survives. The latency mode is a host-latency preference and
    // is kept.
    void initialise();

    // ---- routing ----------------------------------------------------------------------------
    void setSolo(bool on);  // mutually exclusive with MUTE
    void setMute(bool on);
    bool solo() const { return solo_; }
    bool mute() const { return mute_; }

    // ---- latency mode (V2.1 Phase 16) -------------------------------------------------------
    // Requested globally; each FX builds the best variant it really supports (FxDescriptor::resolveLatency).
    // Changing it rebuilds every processor (chain + staged) here on the message thread with the same
    // frozen values and swaps them in through the normal program hand-over (5 ms output crossfade). Tails
    // restart. Never call from the audio thread.
    void setLatencyMode(LatencyMode m);
    LatencyMode latencyMode() const { return latencyMode_; }
    // Effects whose current variant still reports latency (in processing order, staged last).
    struct LatencyContributor { uint64_t id; int typeIndex; int samples; LatencyMode resolved; bool staged; };
    std::vector<LatencyContributor> latencyContributors() const;

    int latencySamples() const;  // committed + staged
    double tailSeconds() const;
    uint64_t revision() const { return revision_; }  // bumps on any model change (for the UI)

    // ---- state ---------------------------------------------------------------------------
    // includeSession = DAW project state (drafts, latency mode). false = user preset.
    std::string toJson(bool includeSession) const;
    // On failure the current state is left untouched and `error` says why.
    // V3 Phase 1: every recall (factory / user preset, host program or project state) applies the chain and sets the
    // staged selection to None, so what is heard is exactly the recalled chain. Uncommitted drafts are kept (a preset
    // never touches them; a project state restores the drafts it saved), so a previously staged FX still has its
    // cached settings when the user returns to it.
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
    bool solo_ = false, mute_ = false, standby_ = false;
    LatencyMode latencyMode_ = LatencyMode::Quality;
    std::vector<CommittedInstance> chain_;
    std::vector<MacroDef> macros_;
    bool macrosAuto_ = true;
    uint64_t macroGen_ = 0;
    std::string comboName_;
    void refreshMacros();  // regenerate combination macros unless an authored set is live

    std::unordered_map<FxProcessor*, std::unique_ptr<FxProcessor>> owned_;
    std::unordered_map<FxProcessor*, int> refs_;  // references from programs not yet retired
    Program* pending_ = nullptr;                  // built but not yet accepted by the engine
    uint64_t revision_ = 1;
    uint64_t idState_ = 0;
};

// V5 Phase 8 (ComboMacros.cpp): combination-specific macros for a chain, and the macro-safe maximum of a parameter
std::vector<Session::MacroDef> comboMacros(const std::vector<CommittedInstance>& chain, std::string* comboName = nullptr);
float macroSafeMax(const std::string& type, const std::string& param, float hi);

std::string idToHex(uint64_t id);
uint64_t idFromHex(const std::string& s);

}  // namespace ih1
