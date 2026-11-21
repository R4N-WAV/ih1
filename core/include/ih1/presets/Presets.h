// IH1 factory preset library: curated committed chains, grouped by category. Each preset is structured data
// (FX type ids + plain parameter values by stable parameter id), turned into the normal chain-only state
// JSON that Session::fromJson validates and installs atomically. Factory presets are read-only.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace ih1 {

// Init (V3 Phase 7.1): pinned first in the factory list. It has an empty chain; loading it calls
// Session::initialise() (no committed FX, staged None, drafts at defaults) instead of a chain recall.
constexpr const char* kInitPresetId = "init";

struct PresetFx {
    std::string type;                                 // FX type id (registry)
    std::vector<std::pair<std::string, float>> params;  // parameter id -> plain value; others = defaults
};

struct FactoryPreset {
    std::string id;           // stable, never renamed (e.g. "atm.slow_horizon")
    std::string name;
    std::string category;
    std::string description;  // one line: what it sounds like
    int version = 1;
    std::vector<PresetFx> chain;  // processing order
    std::string tags;         // suggested sources, comma separated (vocals, drums, bass, synths, guitars, keys, loops, full mix)
};

// Category display order. The last one, "User", holds user presets (not part of the factory list).
const std::vector<std::string>& presetCategories();
const std::vector<FactoryPreset>& factoryPresets();  // category order, then curated order
int factoryPresetIndex(const std::string& id);       // -1 if unknown

// Chain-only state JSON (format ih1-state) with deterministic instance ids and visual seeds, plus a
// "preset" metadata object. Loads through Session::fromJson(text, false).
std::string presetToJson(const FactoryPreset& p);

// Every FX type and parameter id exists, values are finite and in range, the chain is non-empty and
// within kMaxChain. On failure `error` says what is wrong.
bool validatePreset(const FactoryPreset& p, std::string* error = nullptr);

}  // namespace ih1
