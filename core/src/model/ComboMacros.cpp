// V5 Phase 8: combination-specific macros ("secret menu"). The macro knobs near IN / MIX / OUT appear only when the
// committed chain contains a combination of effects for which a coordinated gesture is meaningful. Detection uses
// stable type ids (never display names). Each combination defines its own named macros; each macro maps one
// aspect onto the relevant parameters of the instances that fill the combination's roles, with tuned depths.
//
// Rules enforced here:
//   - a macro moves at least two parameters, and a combination's macro set spans at least two instances (otherwise
//     it would just be one effect's knob);
//   - no (instance, parameter) pair is driven by two macros (no duplicate controls);
//   - at most four macros, highest-priority combination first;
//   - bypassed instances do not count;
//   - stable bounds: Session::rebuild clamps each target to the parameter's macro-safe maximum (macroSafeMax), so a
//     macro can never push a loop into a self-oscillating range.
#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "ih1/model/Session.h"

namespace ih1 {
namespace {

struct Map { const char* type; const char* aspect; const char* param; float depth; };
// aspect -> parameter mappings (plain-unit depths added at macro 100 %)
const Map kMap[] = {
    // space
    {"reverb", "wet", "mix", 30.0f}, {"reverb", "decay", "decay", 4.0f}, {"reverb", "drift", "modulation", 40.0f}, {"reverb", "size", "size", 30.0f},
    {"cloud_chamber", "wet", "mix", 30.0f}, {"cloud_chamber", "decay", "decay", 6.0f}, {"cloud_chamber", "drift", "drift", 45.0f}, {"cloud_chamber", "size", "bloom", 45.0f},
    {"shimmer", "wet", "mix", 25.0f}, {"shimmer", "decay", "decay", 5.0f}, {"shimmer", "halo", "regen", 30.0f}, {"shimmer", "drift", "modulation", 40.0f},
    {"convolution", "wet", "mix", 30.0f}, {"convolution", "decay", "length", -40.0f},
    {"reverse_bloom", "wet", "mix", 30.0f}, {"reverse_bloom", "decay", "tail", 3.0f},
    {"drum_space", "wet", "mix", 30.0f}, {"drum_space", "decay", "decay", 0.8f}, {"drum_space", "size", "size", 35.0f},
    {"reflections", "wet", "mix", 30.0f}, {"reflections", "size", "time", 120.0f}, {"reflections", "drift", "motion", 50.0f},
    // echo
    {"delay", "wet", "mix", 20.0f}, {"delay", "decay", "feedback", 30.0f}, {"delay", "drift", "mod_depth", 40.0f}, {"delay", "echo", "feedback", 30.0f}, {"delay", "wear", "drive", 40.0f},
    {"tape_echo", "wet", "mix", 20.0f}, {"tape_echo", "decay", "intensity", 30.0f}, {"tape_echo", "drift", "wow", 40.0f}, {"tape_echo", "echo", "intensity", 30.0f}, {"tape_echo", "wear", "saturation", 40.0f},
    {"echo_matrix", "wet", "mix", 20.0f}, {"echo_matrix", "decay", "feedback", 30.0f}, {"echo_matrix", "echo", "feedback", 30.0f}, {"echo_matrix", "wear", "drive", 40.0f},
    {"reverse_echo", "wet", "mix", 20.0f}, {"reverse_echo", "decay", "feedback", 25.0f}, {"reverse_echo", "echo", "feedback", 25.0f},
    {"spectral_delay", "wet", "mix", 20.0f}, {"spectral_delay", "decay", "feedback", 25.0f}, {"spectral_delay", "echo", "feedback", 25.0f},
    // modulation / pulse
    {"chorus", "drift", "depth", 35.0f}, {"flanger", "drift", "depth", 35.0f}, {"vibrato", "drift", "depth", 25.0f}, {"dispersion", "drift", "mod_depth", 0.8f},
    {"tremolo", "pulse", "depth", 40.0f}, {"pulse_shaper", "pulse", "depth", 35.0f}, {"pulse_shaper", "swing", "swing", 45.0f}, {"pulse_shaper", "groove", "chance", -35.0f},
    {"gate", "pulse", "range", 30.0f},
    // drums
    {"transient", "impact", "attack", 50.0f}, {"transient", "body", "sustain", 45.0f},
    {"band_transients", "impact", "low_attack", 45.0f}, {"band_transients", "impact", "mid_attack", 45.0f}, {"band_transients", "body", "low_sustain", 40.0f},
    {"transient_split", "impact", "att_gain", 6.0f}, {"transient_split", "body", "body_gain", 6.0f},
    {"drum_crush", "impact", "blend", 30.0f}, {"drum_crush", "dirt", "drive", 12.0f}, {"drum_crush", "dirt", "react", 40.0f}, {"drum_crush", "body", "body", 4.0f},
    {"compressor", "impact", "ratio", 6.0f}, {"compressor", "body", "makeup", 4.0f},
    {"saturation", "dirt", "drive", 12.0f}, {"saturation", "wear", "drive", 10.0f},
    {"fold_crush", "dirt", "drive", 8.0f}, {"fold_crush", "wear", "crush", 40.0f},
    {"tape_colour", "dirt", "drive", 8.0f}, {"tape_colour", "wear", "drive", 8.0f},
    {"degrade", "dirt", "drive", 8.0f}, {"degrade", "wear", "rate", -5000.0f},
    {"bitcrush", "dirt", "drive", 10.0f}, {"bitcrush", "wear", "bits", -5.0f},
    {"amp", "dirt", "gain", 10.0f},
    {"drum_space", "room", "mix", 30.0f}, {"reflections", "room", "mix", 30.0f}, {"reverb", "room", "mix", 25.0f}, {"cloud_chamber", "room", "mix", 25.0f},
    // grain / chop / spectral / pitch / resonance
    {"granular", "scatter", "scatter", 40.0f}, {"granular", "scatter", "spray_ms", 600.0f}, {"granular", "cloud", "density", 25.0f}, {"granular", "blur", "spray_ms", 500.0f},
    {"grain_engine", "scatter", "spray_ms", 800.0f}, {"grain_engine", "scatter", "spread", 40.0f}, {"grain_engine", "cloud", "density", 25.0f}, {"grain_engine", "blur", "size_jit", 40.0f},
    {"glitch", "groove", "repeat", 30.0f}, {"glitch", "groove", "jump", 25.0f}, {"stutter", "groove", "chance", 35.0f}, {"slice_shuffle", "groove", "reverse", 40.0f},
    {"spectral_smear", "blur", "smear", 35.0f}, {"spectral_smear", "blur", "time", 35.0f},
    {"pitch_shift", "halo", "mix", 30.0f}, {"pitch_shift", "decay", "feedback", 25.0f}, {"harmony", "halo", "wet", 6.0f}, {"reverse_echo", "halo", "mix", 25.0f},
    {"modal", "ring", "decay", 4.0f}, {"modal", "ring", "bright", 30.0f}, {"resonator", "ring", "decay", 3.0f}, {"hit_melody", "ring", "decay", 1.2f}, {"hit_melody", "strike", "mix", 30.0f}, {"modal", "strike", "mix", 30.0f},
};

using Role = std::vector<const char*>;
struct MacroSpec { const char* name; std::vector<const char*> aspects; };
struct Combo { const char* name; Role a, b; std::vector<MacroSpec> macros; };

const Role kSpace = {"reverb", "cloud_chamber", "shimmer", "convolution", "reverse_bloom", "drum_space", "reflections"};
const Role kEcho = {"delay", "tape_echo", "echo_matrix", "reverse_echo", "spectral_delay"};
const Role kGrain = {"granular", "grain_engine"};
const Role kPulse = {"tremolo", "pulse_shaper", "gate"};
const Role kTrans = {"transient", "band_transients", "transient_split"};
const Role kCrush = {"drum_crush", "compressor", "saturation", "fold_crush", "tape_colour", "degrade", "bitcrush", "amp"};
const Role kMod = {"chorus", "flanger", "vibrato", "dispersion"};
const Role kPitch = {"pitch_shift", "harmony", "reverse_echo"};
const Role kChop = {"glitch", "stutter", "slice_shuffle", "pulse_shaper"};
const Role kDrumSpace = {"drum_space", "reflections", "reverb", "cloud_chamber"};
const Role kWear = {"degrade", "bitcrush", "tape_colour", "saturation", "fold_crush"};
const Role kRes = {"modal", "resonator", "hit_melody"};
const Role kBlur = {"spectral_smear", "granular", "grain_engine"};

const std::vector<Combo>& combos() {
    static const std::vector<Combo> c = {
        {"Drum Bus", kTrans, kCrush, {{"Impact", {"impact"}}, {"Body", {"body"}}, {"Dirt", {"dirt"}}}},
        {"Pulse into Echo", kPulse, kEcho, {{"Pulse", {"pulse", "wet"}}, {"Swing", {"swing", "groove"}}, {"Echo Motion", {"echo", "drift"}}}},
        {"Groove Room", kChop, kDrumSpace, {{"Groove", {"groove", "pulse"}}, {"Room", {"room", "size"}}}},
        {"Granular Space", kGrain, kSpace, {{"Scatter", {"scatter"}}, {"Cloud", {"cloud", "wet"}}, {"Drift", {"drift"}}}},
        {"Dub Wear", kWear, kEcho, {{"Wear", {"wear"}}, {"Dub", {"echo", "wet"}}}},
        {"Pitch Halo", kPitch, kSpace, {{"Halo", {"halo", "wet"}}, {"Decay", {"decay"}}}},
        {"Resonant Space", kRes, kSpace, {{"Ring", {"ring", "decay"}}, {"Strike", {"strike", "wet"}}}},
        {"Spectral Blur", kBlur, kSpace, {{"Blur", {"blur"}}, {"Space", {"wet", "decay"}}}},
        {"Atmosphere", kSpace, kEcho, {{"Bloom", {"wet"}}, {"Decay", {"decay"}}, {"Drift", {"drift"}}}},
        {"Moving Space", kMod, kSpace, {{"Drift", {"drift"}}, {"Bloom", {"wet", "size"}}}},
        {"Deep Space", kSpace, kSpace, {{"Bloom", {"wet", "size"}}, {"Decay", {"decay"}}, {"Drift", {"drift"}}}},
    };
    return c;
}

bool inRole(const Role& r, const std::string& t) {
    for (const char* x : r) if (t == x) return true;
    return false;
}

}  // namespace

std::vector<Session::MacroDef> comboMacros(const std::vector<CommittedInstance>& chain, std::string* comboName) {
    std::vector<Session::MacroDef> out;
    std::set<std::pair<uint64_t, std::string>> used;
    std::string names;
    for (const auto& c : combos()) {
        if (out.size() >= static_cast<size_t>(kNumPresetMacros)) break;
        // two different enabled instances must fill the two roles
        std::vector<const CommittedInstance*> members;
        bool haveA = false, haveB = false;
        for (const auto& ia : chain) {
            if (!ia.enabled() || !inRole(c.a, ia.descriptor().typeId)) continue;
            for (const auto& ib : chain) {
                if (&ib == &ia || !ib.enabled() || !inRole(c.b, ib.descriptor().typeId)) continue;
                haveA = haveB = true;
            }
        }
        if (!haveA || !haveB) continue;
        for (const auto& i : chain)
            if (i.enabled() && (inRole(c.a, i.descriptor().typeId) || inRole(c.b, i.descriptor().typeId))) members.push_back(&i);
        std::vector<Session::MacroDef> mine;
        std::set<uint64_t> inst;
        std::set<std::pair<uint64_t, std::string>> mineUsed;
        for (const auto& ms : c.macros) {
            if (out.size() + mine.size() >= static_cast<size_t>(kNumPresetMacros)) break;
            bool dupName = false;
            for (const auto& m : out) dupName = dupName || m.name == ms.name;
            if (dupName) continue;
            Session::MacroDef md;
            md.name = ms.name;
            std::set<uint64_t> mi;
            for (const auto* i : members) {
                const auto& d = i->descriptor();
                for (const auto& m : kMap) {
                    if (d.typeId != m.type) continue;
                    if (std::find_if(ms.aspects.begin(), ms.aspects.end(), [&](const char* a) { return std::strcmp(a, m.aspect) == 0; }) == ms.aspects.end()) continue;
                    const int k = d.paramIndex(m.param);
                    if (k < 0 || d.params[static_cast<size_t>(k)].kind != ParamKind::Float) continue;
                    // a control that is inactive for this instance's committed settings (mode-dependent) would be a
                    // placebo target: skip it (V5 bank gate found Grain Engine Scatter doing nothing in Loop mode)
                    if (!d.paramEnabled(k, i->values())) continue;
                    // headroom: a control already at the end of its (macro-safe) range cannot move: skip it too
                    {
                        const auto& ps = d.params[static_cast<size_t>(k)];
                        const float base = i->values()[static_cast<size_t>(k)];
                        const float hi = macroSafeMax(d.typeId, m.param, ps.maxValue);
                        const float eff = std::clamp(base + m.depth, ps.minValue, std::max(ps.minValue, hi)) - base;
                        if (std::fabs(eff) < 0.25f * std::fabs(m.depth)) continue;
                    }
                    if (used.count({i->id(), m.param}) || mineUsed.count({i->id(), m.param})) continue;
                    bool dup = false;
                    for (const auto& t : md.targets) dup = dup || (t.id == i->id() && t.param == m.param);
                    if (dup) continue;
                    md.targets.push_back({i->id(), m.param, m.depth});
                    mi.insert(i->id());
                }
            }
            if (md.targets.size() < 2 || md.targets.size() > 16) continue;
            for (const auto& t : md.targets) mineUsed.insert({t.id, t.param});
            inst.insert(mi.begin(), mi.end());
            mine.push_back(std::move(md));
        }
        if (inst.size() < 2) continue;  // the set must really combine the effects
        for (auto& m : mine) out.push_back(std::move(m));
        used.insert(mineUsed.begin(), mineUsed.end());
        if (!mine.empty()) names += (names.empty() ? "" : " + ") + std::string(c.name);
    }
    if (comboName) *comboName = names;
    return out;
}

// Upper bound a macro may push a parameter to (inclusive). Keeps every loop in its decaying range.
float macroSafeMax(const std::string& type, const std::string& param, float hi) {
    if (type == "tape_echo" && param == "intensity") return std::min(hi, 100.0f);  // above 100 % the tape sings
    if (param == "feedback") return std::min(hi, 85.0f);  // V5 audit: 95 % held Echo Matrix / Spectral Delay loops
    if (type == "shimmer" && param == "regen") return std::min(hi, 90.0f);
    if (type == "granular" && param == "density") return std::min(hi, 40.0f);
    return hi;
}

}  // namespace ih1
