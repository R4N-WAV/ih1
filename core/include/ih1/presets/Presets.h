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

// V4 Phase 1.5: a preset macro (see Session::MacroDef). Targets address chain positions of this preset.
struct PresetMacroTarget { int slot = 0; std::string param; float depth = 0.0f; };
struct PresetMacro { std::string name; std::vector<PresetMacroTarget> targets; };

struct FactoryPreset {
    std::string id;           // stable, never renamed (e.g. "atm.slow_horizon")
    std::string name;
    std::string category;
    std::string description;  // one line: what it sounds like
    int version = 1;
    std::vector<PresetFx> chain;  // processing order
    std::string tags;         // suggested sources, comma separated (vocals, drums, bass, synths, guitars, keys, loops, full mix)
    // V4 metadata (searchable in the browser)
    std::string intensity;    // Subtle, Moderate, Strong, Extreme
    std::string styles;       // genre / style inspiration, comma separated (e.g. "IDM, glitch")
    std::vector<PresetMacro> macros;  // 0..4
};

// V4 helpers (PresetsV4.cpp): the V4 additions, the V3 -> V4 category remap, default intensity, derived macros.
void addV4Presets(std::vector<FactoryPreset>& v);
std::string v4CategoryFor(const std::string& v3Category);
std::string v4DefaultIntensity(const std::string& category, const std::string& id);
std::vector<PresetMacro> deriveMacros(const FactoryPreset& p);  // V4 rule table (kept for reference; no longer applied)
// V5 additions (PresetsV5.cpp): atmosphere, drum / drum-bus and tremolo / groove presets
void addV5Presets(std::vector<FactoryPreset>& v);

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
