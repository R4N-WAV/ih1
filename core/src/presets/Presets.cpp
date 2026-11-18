#include "ih1/presets/Presets.h"

#include <cmath>
#include <cstdio>

#include "ih1/fx/FxProcessor.h"
#include "ih1/model/Json.h"
#include "ih1/model/Session.h"

namespace ih1 {

const std::vector<std::string>& presetCategories() {
    static const std::vector<std::string> c = {"Atmospheres", "Voices", "Fragments", "Motion", "Rhythmic", "Tone & Destruction", "Signature", "User"};
    return c;
}

namespace {
using P = std::vector<std::pair<std::string, float>>;

std::vector<FactoryPreset> build() {
    std::vector<FactoryPreset> v;
    auto add = [&](const char* id, const char* name, const char* cat, const char* desc, std::vector<PresetFx> chain) {
        v.push_back({id, name, cat, desc, 1, std::move(chain)});
    };

    // ------------------------------------------------------------------ Atmospheres
    add("atm.slow_horizon", "Slow Horizon", "Atmospheres", "A vast, darkened cathedral that blooms slowly behind the source and opens wide.",
        {{"eq", P{{"hp", 120}, {"hs_freq", 7000}, {"hs_gain", -3}}},
         {"reverb", P{{"mode", 4}, {"decay", 9}, {"size", 85}, {"predelay", 60}, {"damping", 55}, {"mix", 38}}},
         {"width", P{{"width", 140}, {"mono_below", 150}}}});
    add("atm.glass_cathedral", "Glass Cathedral", "Atmospheres", "Octave and fifth shimmer rising out of a long tail, gently chorused.",
        {{"shimmer", P{{"decay", 12}, {"size", 75}, {"shift1", 12}, {"shift2", 19}, {"regen", 45}, {"tone", 6500}, {"mix", 35}}},
         {"chorus", P{{"rate", 0.2f}, {"depth", 30}, {"mix", 30}}}});
    add("atm.under_ice", "Under Ice", "Atmospheres", "Muffled and submerged: low-passed, bloomed through a strange space, edges blurred.",
        {{"filter", P{{"type", 0}, {"cutoff", 2400}, {"resonance", 15}}},
         {"convolution", P{{"ir", 4}, {"length", 90}, {"mix", 45}}},
         {"spectral", P{{"blur", 60}, {"smear", 30}, {"mix", 30}}}});
    add("atm.afterglow", "Afterglow", "Atmospheres", "Worn tape repeats with a slow wobble, fading into a soft plate.",
        {{"vibrato", P{{"depth", 12}, {"rate", 0.6f}}},
         {"tape_echo", P{{"time_ms", 420}, {"intensity", 55}, {"wow", 30}, {"age", 50}, {"mix", 30}}},
         {"reverb", P{{"mode", 2}, {"decay", 4}, {"mix", 30}}},
         {"tape_colour", P{{"drive", 4}}}});

    // ------------------------------------------------------------------ Voices
    add("voc.choir_of_one", "Choir of One", "Voices", "Three in-key harmonies, doubled and placed in a modest hall.",
        {{"harmony", P{{"mode", 1}, {"voices", 2}, {"detune", 8}, {"wet", -3}}},
         {"doubler", P{{"mix", 40}}},
         {"reverb", P{{"mode", 1}, {"decay", 2.8f}, {"mix", 22}}}});
    add("voc.wide_double", "Wide Double", "Voices", "A natural four-voice double, a lifted presence band, gently levelled.",
        {{"doubler", P{{"detune", 12}, {"delay_ms", 22}, {"width", 100}, {"voices", 1}, {"mix", 55}}},
         {"eq", P{{"b2_freq", 3500}, {"b2_gain", 2}}},
         {"compressor", P{{"threshold", -22}, {"ratio", 3}}}});
    add("voc.octave_ghost", "Octave Ghost", "Voices", "Octaves above and below, darker formants, with backwards echoes trailing behind.",
        {{"pitch_shift", P{{"shift_a", 12}, {"shift_b", -12}, {"level_b", -10}, {"mix", 35}}},
         {"formant", P{{"shift", -3}, {"mix", 60}}},
         {"reverse", P{{"mode", 1}, {"mix", 25}}},
         {"reverb", P{{"mode", 3}, {"mix", 25}}}});
    add("voc.tuned_glide", "Tuned Glide", "Voices", "Firm pitch correction in A minor, doubled, with a quiet quarter-note echo.",
        {{"pitch_correct", P{{"key", 9}, {"scale", 1}, {"speed", 20}}},
         {"doubler", P{{"mix", 35}}},
         {"delay", P{{"sync", 1}, {"division", 10}, {"feedback", 25}, {"mix", 18}}}});

    // ------------------------------------------------------------------ Fragments
    add("frg.glass_grains", "Glass Grains", "Fragments", "Octave-up grains scattered forwards and backwards into a long hall.",
        {{"granular", P{{"size_ms", 60}, {"density", 30}, {"spray_ms", 600}, {"pitch", 12}, {"reverse", 30}, {"mix", 55}}},
         {"reverb", P{{"mode", 1}, {"decay", 5}, {"mix", 30}}}});
    add("frg.backwards_letters", "Backwards Letters", "Fragments", "Phrases played back in reverse, smeared, then bounced across the stereo field.",
        {{"reverse", P{{"mode", 0}, {"sync", 1}, {"division", 11}, {"mix", 60}}},
         {"spectral", P{{"blur", 30}, {"mix", 25}}},
         {"delay", P{{"stereo", 2}, {"feedback", 30}, {"mix", 20}}}});
    add("frg.frozen_breath", "Frozen Breath", "Fragments", "A smeared spectral haze with sparse grains drifting through it, very wide.",
        {{"spectral", P{{"blur", 70}, {"smear", 50}, {"shimmer", 30}, {"mix", 55}}},
         {"granular", P{{"density", 12}, {"spray_ms", 900}, {"mix", 30}}},
         {"width", P{{"width", 150}}}});
    add("frg.loop_ghost", "Loop Ghost", "Fragments", "Captures two bars, then lets them dissolve an octave down into a small space.",
        {{"looper", P{{"length", 3}, {"overdub", 20}, {"mix", 50}}},
         {"granular", P{{"density", 8}, {"pitch", -12}, {"mix", 35}}},
         {"reverb", P{{"mode", 3}, {"mix", 25}}}});

    // ------------------------------------------------------------------ Motion
    add("mot.tidal_chorus", "Tidal Chorus", "Motion", "A slow, deep four-voice chorus that drifts from side to side on the half note.",
        {{"chorus", P{{"voices", 3}, {"rate", 0.25f}, {"depth", 60}, {"vintage", 40}, {"mix", 50}}},
         {"tremolo", P{{"mode", 1}, {"sync", 1}, {"division", 13}, {"depth", 50}}}});
    add("mot.jet_wash", "Jet Wash", "Motion", "A slow, resonant through-zero sweep in a tight room.",
        {{"flanger", P{{"rate", 0.08f}, {"feedback", 70}, {"depth", 4}}},
         {"reverb", P{{"mode", 0}, {"decay", 1.2f}, {"mix", 18}}}});
    add("mot.spiral_stairs", "Spiral Stairs", "Motion", "Ten-stage phasing into a frequency-shifted feedback loop that endlessly climbs.",
        {{"phaser", P{{"stages", 4}, {"rate", 0.15f}, {"feedback", 60}, {"range", 4}}},
         {"freq_shift", P{{"shift", 3}, {"feedback", 40}, {"delay_ms", 200}, {"mix", 40}}},
         {"width", P{{"width", 130}}}});
    add("mot.leslie_room", "Leslie Room", "Motion", "Slow rotary speaker with a little drive, warmed on tape, in a small room.",
        {{"rotary", P{{"speed", 0}, {"drive", 30}}},
         {"tape_colour", P{{"drive", 6}}},
         {"reverb", P{{"mode", 0}, {"decay", 0.9f}, {"mix", 15}}}});

    // ------------------------------------------------------------------ Rhythmic
    add("rhy.dotted_rain", "Dotted Rain", "Rhythmic", "Dotted-eighth ping-pong repeats, thinned below and softened above, on a plate.",
        {{"filter", P{{"type", 2}, {"cutoff", 300}, {"resonance", 10}}},
         {"delay", P{{"sync", 1}, {"division", 8}, {"stereo", 2}, {"feedback", 45}, {"high_cut", 5000}, {"mix", 35}}},
         {"reverb", P{{"mode", 2}, {"decay", 2.2f}, {"mix", 15}}}});
    add("rhy.stutter_bloom", "Stutter Bloom", "Rhythmic", "Bar-wise sixteenth stutters that echo and bloom into space.",
        {{"stutter", P{{"interval", 2}, {"gate", 3}, {"slice", 2}, {"chance", 60}}},
         {"delay", P{{"sync", 1}, {"division", 4}, {"feedback", 30}, {"mix", 25}}},
         {"reverb", P{{"decay", 2.5f}, {"mix", 20}}}});
    add("rhy.gated_pulse", "Gated Pulse", "Rhythmic", "Eighth-note pumping into a tempo-synced filter sweep and a quarter-note echo.",
        {{"gate", P{{"mode", 2}, {"division", 7}, {"curve", 60}}},
         {"filter", P{{"type", 1}, {"cutoff", 900}, {"resonance", 35}, {"mod", 1}, {"sync", 1}, {"division", 10}, {"depth", 2}}},
         {"delay", P{{"sync", 1}, {"division", 10}, {"feedback", 25}, {"mix", 20}}}});
    add("rhy.tape_triplets", "Tape Triplets", "Rhythmic", "Three tape heads on quarter-note triplets, saturated like an old echo unit.",
        {{"tape_echo", P{{"sync", 1}, {"division", 9}, {"heads", 6}, {"intensity", 50}, {"mix", 35}}},
         {"saturation", P{{"style", 1}, {"drive", 6}}}});

    // ------------------------------------------------------------------ Tone & Destruction
    add("ton.broken_transistor", "Broken Transistor", "Tone & Destruction", "A cranked small combo, partly crushed, slammed by a fast compressor.",
        {{"amp", P{{"gain", 32}, {"cab", 1}, {"presence", 4}}},
         {"bitcrush", P{{"bits", 10}, {"rate", 18000}, {"mix", 60}}},
         {"compressor", P{{"mode", 1}, {"threshold", -18}, {"ratio", 6}}}});
    add("ton.warm_console", "Warm Console", "Tone & Destruction", "Subtle polish: gentle shelves, a softened attack, opto levelling and console drive.",
        {{"eq", P{{"hp", 30}, {"ls_gain", 2}, {"hs_gain", 1.5f}}},
         {"transient", P{{"attack", -20}, {"sustain", 10}}},
         {"compressor", P{{"mode", 2}, {"threshold", -18}, {"ratio", 3}, {"attack", 20}}},
         {"tape_colour", P{{"mode", 1}, {"drive", 8}}},
         {"exciter", P{{"amount", 20}}}});
    add("ton.crushed_telephone", "Crushed Telephone", "Tone & Destruction", "A narrow, resonant band, bit-reduced and clipped hard, folded to mono.",
        {{"filter", P{{"type", 4}, {"cutoff", 1500}, {"resonance", 40}}},
         {"bitcrush", P{{"bits", 6}, {"rate", 6000}, {"jitter", 20}}},
         {"saturation", P{{"style", 3}, {"drive", 18}}},
         {"width", P{{"width", 0}}}});
    add("ton.fold_space", "Fold Space", "Tone & Destruction", "Wavefolding and ring modulation, cut short by a gated burst of space.",
        {{"saturation", P{{"style", 4}, {"drive", 20}, {"mix", 70}}},
         {"ring_mod", P{{"freq", 180}, {"mix", 35}}},
         {"reverb", P{{"mode", 5}, {"decay", 0.8f}, {"mix", 30}}}});

    // ------------------------------------------------------------------ Signature
    add("sig.supercluster", "Supercluster", "Signature", "IH1's own: chorus, reversed grains and tuned resonance gathering in a long hall.",
        {{"chorus", P{{"mix", 30}}},
         {"reverse", P{{"mode", 2}, {"mix", 30}}},
         {"resonator", P{{"key", 9}, {"chord", 6}, {"decay", 4}, {"mix", 30}}},
         {"reverb", P{{"mode", 1}, {"decay", 6}, {"mix", 30}}},
         {"width", P{{"width", 130}}}});
    add("sig.machine_choir", "Machine Choir", "Signature", "A 32-band vocoder (played from MIDI notes, or a fixed C3) chorused into a hall.",
        {{"vocoder", P{{"bands", 4}, {"carrier", 3}, {"use_midi", 1}, {"note", 48}, {"mix", 90}}},
         {"chorus", P{{"mix", 35}}},
         {"reverb", P{{"mode", 1}, {"decay", 3}, {"mix", 25}}}});
    add("sig.doppler_drift", "Doppler Drift", "Signature", "The source circles the listener while modulated echoes trail behind it.",
        {{"doppler", P{{"path", 2}, {"speed", 15}, {"period", 8}, {"mix", 60}}},
         {"delay", P{{"sync", 0}, {"time_ms", 520}, {"feedback", 35}, {"mod_depth", 40}, {"mix", 25}}},
         {"reverb", P{{"mix", 20}}}});
    add("sig.vocal_polish", "Vocal Polish", "Signature", "A clean starting point: de-essed, lifted, levelled and placed on a short plate.",
        {{"deesser", P{{"threshold", -10}}},
         {"eq", P{{"hp", 80}, {"b2_freq", 4000}, {"b2_gain", 2}}},
         {"compressor", P{{"threshold", -20}, {"ratio", 3}}},
         {"reverb", P{{"mode", 2}, {"decay", 1.6f}, {"mix", 15}}}});
    return v;
}

uint64_t fnv(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (char c : s) h = (h ^ static_cast<uint8_t>(c)) * 1099511628211ull;
    return h;
}
}  // namespace

const std::vector<FactoryPreset>& factoryPresets() {
    static const std::vector<FactoryPreset> v = build();
    return v;
}

int factoryPresetIndex(const std::string& id) {
    const auto& v = factoryPresets();
    for (size_t i = 0; i < v.size(); ++i) if (v[i].id == id) return static_cast<int>(i);
    return -1;
}

bool validatePreset(const FactoryPreset& p, std::string* error) {
    auto fail = [&](const std::string& m) { if (error) *error = p.id + ": " + m; return false; };
    if (p.id.empty() || p.name.empty() || p.category.empty()) return fail("missing metadata");
    if (p.chain.empty()) return fail("empty chain");
    if (static_cast<int>(p.chain.size()) > kMaxChain) return fail("chain too long");
    for (const auto& fx : p.chain) {
        const int t = fxTypeIndex(fx.type);
        if (t < 0) return fail("unknown FX type '" + fx.type + "'");
        const auto& d = fxRegistry()[static_cast<size_t>(t)];
        for (const auto& [pid, val] : fx.params) {
            const int i = d.paramIndex(pid);
            if (i < 0) return fail(fx.type + ": unknown parameter '" + pid + "'");
            const auto& spec = d.params[static_cast<size_t>(i)];
            if (!std::isfinite(val) || val < spec.minValue || val > spec.maxValue) return fail(fx.type + "." + pid + " out of range");
            if (spec.kind != ParamKind::Float && std::fabs(val - std::round(val)) > 1e-6f) return fail(fx.type + "." + pid + " must be a whole step");
        }
    }
    return true;
}

std::string presetToJson(const FactoryPreset& p) {
    json::Value root = json::Value::object();
    root.set("format", "ih1-state");
    root.set("version", 1);
    json::Value meta = json::Value::object();
    meta.set("id", p.id);
    meta.set("name", p.name);
    meta.set("category", p.category);
    meta.set("description", p.description);
    meta.set("presetVersion", p.version);
    meta.set("factory", true);
    root.set("preset", std::move(meta));
    json::Value chain = json::Value::array();
    const uint64_t base = fnv(p.id);
    for (size_t i = 0; i < p.chain.size(); ++i) {
        const auto& fx = p.chain[i];
        json::Value e = json::Value::object();
        // deterministic instance id and visual seed: the same preset always draws the same formation
        uint64_t id = base ^ (0x9E3779B97F4A7C15ull * (i + 1));
        if (id == 0) id = 1;
        e.set("id", idToHex(id));
        e.set("type", fx.type);
        e.set("seed", static_cast<double>(static_cast<uint32_t>((id >> 17) ^ (id << 3)) | 1u));
        json::Value prm = json::Value::object();
        for (const auto& [pid, val] : fx.params) prm.set(pid, json::Value(static_cast<double>(val)));
        e.set("params", std::move(prm));
        chain.push(std::move(e));
    }
    root.set("chain", std::move(chain));
    return root.dump(1);
}

}  // namespace ih1
