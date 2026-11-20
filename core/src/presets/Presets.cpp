#include "ih1/presets/Presets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ih1/fx/FxProcessor.h"
#include "ih1/model/Json.h"
#include "ih1/model/Session.h"

namespace ih1 {

const std::vector<std::string>& presetCategories() {
    // V2.1 categories (feedback 7). The last one, "User", holds user presets.
    static const std::vector<std::string> c = {"Essentials", "Granular Motion", "Stretch & Smear", "Glitch & Rhythm", "Vocal Transformations",
                                               "Atmospheres", "Harmonic & Tonal", "Movement & Width", "Character & Dirt", "Experimental", "User"};
    return c;
}

namespace {
using P = std::vector<std::pair<std::string, float>>;

std::vector<FactoryPreset> build() {
    std::vector<FactoryPreset> v;
    auto add = [&](const char* id, const char* name, const char* cat, const char* desc, std::vector<PresetFx> chain) {
        v.push_back({id, name, cat, desc, 1, std::move(chain), ""});
    };
    // V2.1: category + source tags in one call
    auto addT = [&](const char* id, const char* name, const char* cat, const char* tags, const char* desc, std::vector<PresetFx> chain) {
        v.push_back({id, name, cat, desc, 1, std::move(chain), tags});
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
    // =================================================================== V2.1 library (original IH1 designs)
    // ------------------------------------------------------------------ Essentials
    addT("ess.clean_slap", "Clean Slap", "Essentials", "vocals, guitars, drums", "A single short slapback, thinned and darkened, with a touch of room. Mix-ready.",
         {{"delay", P{{"sync", 0}, {"time_ms", 95}, {"stereo", 0}, {"feedback", 8}, {"low_cut", 200}, {"high_cut", 6000}, {"mix", 20}}},
          {"reverb", P{{"mode", 0}, {"decay", 0.6f}, {"size", 30}, {"mix", 10}}}});
    addT("ess.quarter_throw", "Quarter Throw", "Essentials", "vocals, synths, keys", "Quarter-note ping-pong that ducks under the dry signal and softens with every repeat.",
         {{"delay", P{{"sync", 1}, {"division", 10}, {"stereo", 2}, {"feedback", 38}, {"low_cut", 250}, {"high_cut", 4500}, {"duck", 50}, {"mix", 24}}},
          {"reverb", P{{"mode", 2}, {"decay", 1.2f}, {"predelay", 30}, {"mix", 10}}}});
    addT("ess.plate_vocal", "Velvet Plate", "Essentials", "vocals, snare", "Levelled and lifted, then a bright short plate with pre-delay so the words stay in front.",
         {{"eq", P{{"hp", 90}, {"hs_freq", 9000}, {"hs_gain", 1.5f}}},
          {"compressor", P{{"mode", 2}, {"threshold", -22}, {"ratio", 3}}},
          {"reverb", P{{"mode", 2}, {"decay", 1.8f}, {"predelay", 45}, {"high_cut", 11000}, {"mix", 18}}}});
    addT("ess.mix_glue", "Mix Glue", "Essentials", "full mix, drums, loops", "Gentle 2:1 bus compression with slow attack and a light console drive. Subtle on purpose.",
         {{"compressor", P{{"mode", 0}, {"threshold", -16}, {"ratio", 2}, {"attack", 30}, {"release", 250}, {"sc_hp", 90}}},
          {"tape_colour", P{{"mode", 1}, {"drive", 3}, {"bump", 0.5f}, {"hf_loss", 0.5f}}}});

    // ------------------------------------------------------------------ Granular Motion
    addT("gm.particle_drift", "Particle Drift", "Granular Motion", "synths, keys, vocals, guitars", "Medium grains scattered half a second back and across the field, settling into a hall.",
         {{"grain_engine", P{{"size_ms", 70}, {"density", 35}, {"position_ms", 250}, {"spray_ms", 400}, {"spread", 85}, {"stereo", 1}, {"mix", 45}}},
          {"reverb", P{{"mode", 1}, {"decay", 3.5f}, {"mix", 25}}}});
    addT("gm.octave_dust", "Octave Dust", "Granular Motion", "keys, plucks, guitars", "Plucked grains jump between octaves and some run backwards: sparkling, never out of tune.",
         {{"grain_engine", P{{"size_ms", 45}, {"density", 50}, {"pitch_jit", 12}, {"scale", 1}, {"reverse", 30}, {"window", 3}, {"spray_ms", 300}, {"spread", 70}, {"mix", 40}}},
          {"chorus", P{{"rate", 0.3f}, {"depth", 35}, {"mix", 25}}}});
    addT("gm.minor_scatter", "Minor Scatter", "Granular Motion", "synths, pads, keys", "Gaussian grains snapped to a minor scale, echoed on dotted eighths across the stereo field.",
         {{"grain_engine", P{{"size_ms", 150}, {"density", 22}, {"pitch_jit", 7}, {"scale", 4}, {"window", 1}, {"spread", 90}, {"spray_ms", 500}, {"mix", 45}}},
          {"delay", P{{"sync", 1}, {"division", 8}, {"stereo", 2}, {"feedback", 35}, {"high_cut", 5000}, {"mix", 22}}}});
    addT("gm.swarm", "Swarm", "Granular Motion", "vocals, full mix, noise", "Dozens of tiny, slightly detuned grains with independent left and right streams: a buzzing, living cloud.",
         {{"grain_engine", P{{"size_ms", 22}, {"density", 160}, {"voices", 64}, {"spray_ms", 900}, {"pitch_jit", 0.3f}, {"stereo", 1}, {"spread", 100}, {"mix", 50}}},
          {"width", P{{"width", 140}, {"mono_below", 160}}}});
    addT("gm.cascade", "Cascade", "Granular Motion", "keys, plucks, vocals", "Grain feedback through a fifth-up delay: each pass is re-grained higher and darker until it fades.",
         {{"grain_engine", P{{"size_ms", 180}, {"density", 14}, {"pitch", 7}, {"feedback", 62}, {"delay_ms", 380}, {"fb_hp", 220}, {"fb_lp", 5200}, {"spray_ms", 120}, {"mix", 40}}},
          {"reverb", P{{"mode", 1}, {"decay", 4}, {"mix", 22}}}});

    // ------------------------------------------------------------------ Stretch & Smear
    addT("ss.half_time_haze", "Half-Time Haze", "Stretch & Smear", "vocals, synths, loops", "The recent past played back at half speed with the pitch kept, smoothly spliced, drifting into reverb.",
         {{"grain_engine", P{{"mode", 1}, {"speed", 0.5f}, {"size_ms", 120}, {"density", 30}, {"spray_ms", 20}, {"align", 1}, {"mix", 65}}},
          {"reverb", P{{"mode", 1}, {"decay", 4}, {"mix", 25}}}});
    addT("ss.quarter_drift", "Quarter Drift", "Stretch & Smear", "pads, vocals, keys", "Four times slower, long overlapping grains at a constant overlap, blurred at the edges.",
         {{"grain_engine", P{{"mode", 1}, {"speed", 0.25f}, {"size_ms", 260}, {"lock", 1}, {"overlap", 6}, {"spray_ms", 40}, {"window", 1}, {"mix", 75}}},
          {"spectral", P{{"blur", 30}, {"mix", 30}}}});
    addT("ss.glacier", "Glacier", "Stretch & Smear", "pads, strings, ambient", "An eighth-speed scan through the buffer: chords become slowly shifting glacial walls.",
         {{"grain_engine", P{{"mode", 1}, {"speed", 0.125f}, {"size_ms", 420}, {"density", 9}, {"window", 1}, {"spray_ms", 60}, {"mix", 85}}},
          {"reverb", P{{"mode", 4}, {"decay", 10}, {"damping", 60}, {"mix", 35}}}});
    addT("ss.tape_slow", "Tape Slowdown", "Stretch & Smear", "drums, loops, vocals", "Varispeed at half speed: the recent past an octave down, like a slowed tape, warmly saturated.",
         {{"grain_engine", P{{"mode", 1}, {"speed", 0.5f}, {"varispeed", 1}, {"size_ms", 90}, {"density", 30}, {"mix", 60}}},
          {"tape_colour", P{{"drive", 7}, {"hf_loss", 5}}}});
    addT("ss.smear_field", "Smear Field", "Stretch & Smear", "vocals, guitars, full mix", "Spectral blur plus wide-sprayed grains: transients dissolve into a soft, moving smear.",
         {{"spectral", P{{"blur", 70}, {"smear", 55}, {"mix", 45}}},
          {"grain_engine", P{{"size_ms", 200}, {"density", 18}, {"spray_ms", 1500}, {"spread", 80}, {"mix", 35}}}});

    // ------------------------------------------------------------------ Glitch & Rhythm
    addT("gr.slice_retrigger", "Slice Retrigger", "Glitch & Rhythm", "drums, loops, vocals", "The playhead jumps back to the same slice every eighth note: rhythmic repeats that follow the tempo.",
         {{"grain_engine", P{{"mode", 1}, {"speed", 1}, {"retrig", 1}, {"sync", 1}, {"retrig_div", 7}, {"position_ms", 120}, {"size_ms", 60}, {"density", 40}, {"window", 2}, {"spray_ms", 0}, {"mix", 70}}},
          {"eq", P{{"hp", 70}, {"b2_freq", 3000}, {"b2_gain", 1.5f}}}});
    addT("gr.backspin", "Backspin Slices", "Glitch & Rhythm", "drums, loops, vocals", "Every beat the last moment plays backwards from the top: reversed slices locked to the grid.",
         {{"grain_engine", P{{"mode", 1}, {"speed", -1}, {"retrig", 1}, {"sync", 1}, {"retrig_div", 10}, {"position_ms", 20}, {"size_ms", 70}, {"density", 35}, {"window", 2}, {"spray_ms", 0}, {"mix", 60}}},
          {"delay", P{{"sync", 1}, {"division", 7}, {"feedback", 20}, {"mix", 15}}}});
    addT("gr.buffer_roll", "Buffer Roll", "Glitch & Rhythm", "drums, loops, full mix", "Thirty-second-note rolls that step down in pitch and decay, roughened by a light crush.",
         {{"stutter", P{{"interval", 1}, {"gate", 2}, {"slice", 0}, {"pitch", -1}, {"decay", 20}, {"chance", 75}}},
          {"bitcrush", P{{"bits", 10}, {"rate", 22050}, {"mix", 40}}}});
    addT("gr.halftime_chop", "Halftime Chop", "Glitch & Rhythm", "drums, loops", "Half-speed slices restarted every half note, then pumped on eighths: a lurching, chopped groove.",
         {{"grain_engine", P{{"mode", 1}, {"speed", 0.5f}, {"retrig", 1}, {"sync", 1}, {"retrig_div", 13}, {"size_ms", 80}, {"density", 30}, {"window", 2}, {"mix", 80}}},
          {"gate", P{{"mode", 2}, {"division", 7}, {"curve", 50}, {"range", 18}}}});

    // ------------------------------------------------------------------ Vocal Transformations
    addT("vt.whisper_cloud", "Whisper Cloud", "Vocal Transformations", "vocals, spoken word", "Tiny bright grains around a slightly lifted formant: the voice breaks into breathy particles.",
         {{"formant", P{{"shift", 3}, {"mix", 60}}},
          {"grain_engine", P{{"size_ms", 28}, {"density", 90}, {"spray_ms", 140}, {"spread", 90}, {"window", 1}, {"mix", 40}}},
          {"reverb", P{{"mode", 2}, {"decay", 2.2f}, {"mix", 20}}}});
    addT("vt.octave_halo", "Octave Halo", "Vocal Transformations", "vocals", "Swelling grains an octave above and below the voice, behind an in-key harmony, like a distant choir.",
         {{"harmony", P{{"mode", 1}, {"voices", 1}, {"wet", -6}}},
          {"grain_engine", P{{"pitch_jit", 12}, {"scale", 1}, {"window", 4}, {"size_ms", 220}, {"density", 22}, {"voices", 48}, {"spread", 80}, {"mix", 35}}}});
    addT("vt.ring_robot", "Circuit Voice", "Vocal Transformations", "vocals, spoken word", "Hard-tuned, frequency-shifted against itself and lightly crushed: a metallic machine voice.",
         {{"pitch_correct", P{{"speed", 0}, {"amount", 100}}},
          {"freq_shift", P{{"shift", 30}, {"feedback", 25}, {"delay_ms", 40}, {"mix", 50}}},
          {"bitcrush", P{{"bits", 12}, {"rate", 16000}, {"mix", 35}}}});
    addT("vt.giant", "Giant", "Vocal Transformations", "vocals, spoken word", "An octave down with darker formants and a big room: a huge, slow-moving voice.",
         {{"pitch_shift", P{{"shift_a", -12}, {"mix", 100}}},
          {"formant", P{{"shift", -4}, {"mix", 100}}},
          {"reverb", P{{"mode", 0}, {"decay", 1.4f}, {"size", 80}, {"mix", 22}}}});

    // ------------------------------------------------------------------ Atmospheres
    addT("atm.slow_bloom", "Slow Bloom", "Atmospheres", "pads, vocals, guitars", "Third-speed swelling grains that bloom into an octave shimmer tail.",
         {{"grain_engine", P{{"mode", 1}, {"speed", 0.33f}, {"window", 4}, {"size_ms", 600}, {"density", 6}, {"mix", 55}}},
          {"shimmer", P{{"decay", 10}, {"regen", 45}, {"mix", 30}}}});
    addT("atm.deep_hollow", "Deep Hollow", "Atmospheres", "pads, drones, guitars", "A long, heavily damped cathedral fed by low, re-grained feedback: dark and enormous.",
         {{"grain_engine", P{{"pitch", -12}, {"feedback", 45}, {"fb_lp", 2500}, {"size_ms", 300}, {"density", 10}, {"mix", 30}}},
          {"reverb", P{{"mode", 4}, {"decay", 16}, {"damping", 80}, {"high_cut", 4000}, {"mix", 45}}}});
    addT("atm.veil", "Veil", "Atmospheres", "vocals, keys, guitars", "A translucent spectral veil with a faint shimmer, softened on a long plate.",
         {{"spectral", P{{"blur", 85}, {"smear", 40}, {"shimmer", 20}, {"mix", 45}}},
          {"reverb", P{{"mode", 2}, {"decay", 5}, {"mix", 30}}}});

    // ------------------------------------------------------------------ Harmonic & Tonal
    addT("ht.fifth_resonance", "Fifth Resonance", "Harmonic & Tonal", "drums, percussion, plucks", "Hits ring out as tuned fifths in D, with grains snapped to fifths shimmering above.",
         {{"resonator", P{{"key", 2}, {"chord", 2}, {"decay", 5}, {"bright", 60}, {"mix", 40}}},
          {"grain_engine", P{{"pitch_jit", 12}, {"scale", 2}, {"size_ms", 100}, {"density", 20}, {"mix", 25}}}});
    addT("ht.major_bloom", "Major Bloom", "Harmonic & Tonal", "vocals, keys, guitars", "Three in-key harmonies opening into a fifth-and-octave shimmer.",
         {{"harmony", P{{"mode", 1}, {"voices", 2}, {"detune", 6}, {"wet", -4}}},
          {"shimmer", P{{"shift1", 12}, {"shift2", 7}, {"decay", 8}, {"mix", 30}}}});
    addT("ht.minor_drone", "Minor Drone", "Harmonic & Tonal", "pads, guitars, drones", "Captures two seconds, loops it at half speed and lets a minor resonator sing over it.",
         {{"grain_engine", P{{"mode", 2}, {"loop_ms", 2000}, {"speed", 0.5f}, {"size_ms", 250}, {"density", 12}, {"window", 1}, {"gain", -4}, {"mix", 55}}},
          {"resonator", P{{"key", 9}, {"chord", 4}, {"octave", 1}, {"decay", 8}, {"mix", 18}}}});
    addT("ht.crystal", "Crystal", "Harmonic & Tonal", "keys, plucks, synths", "Octave-up pentatonic grains over a high pentatonic resonator: glassy and bell-like.",
         {{"grain_engine", P{{"pitch", 12}, {"pitch_jit", 12}, {"scale", 5}, {"window", 3}, {"size_ms", 80}, {"density", 28}, {"spread", 80}, {"mix", 35}}},
          {"resonator", P{{"chord", 6}, {"octave", 3}, {"decay", 3}, {"bright", 75}, {"mix", 25}}}});

    // ------------------------------------------------------------------ Movement & Width
    addT("mw.wide_grains", "Wide Grains", "Movement & Width", "synths, vocals, guitars", "Independent left and right grain streams add width without phasing; the bass stays centred.",
         {{"grain_engine", P{{"stereo", 1}, {"spread", 100}, {"size_ms", 60}, {"density", 40}, {"position_ms", 30}, {"spray_ms", 40}, {"mix", 30}}},
          {"width", P{{"width", 140}, {"mono_below", 150}}}});
    addT("mw.pan_haze", "Pan Haze", "Movement & Width", "pads, keys, guitars", "A slow half-note auto-pan through a four-voice chorus into a soft hall.",
         {{"tremolo", P{{"mode", 1}, {"sync", 1}, {"division", 13}, {"depth", 60}}},
          {"chorus", P{{"voices", 3}, {"rate", 0.35f}, {"depth", 45}, {"mix", 40}}},
          {"reverb", P{{"mode", 1}, {"decay", 3}, {"mix", 20}}}});
    addT("mw.phase_drift", "Phase Drift", "Movement & Width", "guitars, synths, drums", "Twelve-stage phasing so slow it feels like weather, with a faint flange on top.",
         {{"phaser", P{{"stages", 5}, {"rate", 0.05f}, {"feedback", 35}, {"range", 4}, {"stereo", 120}}},
          {"flanger", P{{"rate", 0.03f}, {"depth", 1.5f}, {"feedback", 20}, {"mix", 30}}}});

    // ------------------------------------------------------------------ Character & Dirt
    addT("cd.cassette", "Cassette", "Character & Dirt", "drums, keys, full mix, loops", "Worn tape: saturated, dulled, slowly warbling and slightly crushed.",
         {{"tape_colour", P{{"drive", 8}, {"hf_loss", 6}, {"bump", 2.5f}}},
          {"vibrato", P{{"depth", 8}, {"rate", 0.8f}}},
          {"bitcrush", P{{"bits", 12}, {"rate", 32000}, {"mix", 30}}}});
    addT("cd.blown_speaker", "Blown Speaker", "Character & Dirt", "guitars, bass, drums", "A cranked small amp into a folding stage, blended in parallel so the body survives.",
         {{"amp", P{{"gain", 30}, {"cab", 1}, {"out", -9}, {"mix", 80}}},
          {"saturation", P{{"style", 4}, {"drive", 14}, {"mix", 30}}}});
    addT("cd.lofi_sampler", "Lo-Fi Sampler", "Character & Dirt", "drums, loops, keys", "Twelve-bit sampler colour, rounded lows, and a fast FET squeeze.",
         {{"bitcrush", P{{"bits", 12}, {"rate", 22050}, {"prefilter", 1}}},
          {"eq", P{{"hp", 40}, {"ls_gain", 2}, {"lp", 12000}}},
          {"compressor", P{{"mode", 1}, {"threshold", -18}, {"ratio", 5}, {"attack", 3}}}});

    // ------------------------------------------------------------------ Experimental
    addT("ex.ring_cascade", "Ring Cascade", "Experimental", "synths, drums, noise", "Ring modulation feeding pitched grain feedback and a slowly climbing frequency shift.",
         {{"ring_mod", P{{"freq", 330}, {"mod_depth", 30}, {"mix", 40}}},
          {"grain_engine", P{{"pitch", 5}, {"feedback", 70}, {"delay_ms", 220}, {"fb_lp", 6000}, {"size_ms", 120}, {"density", 20}, {"mix", 40}}},
          {"freq_shift", P{{"shift", 4}, {"feedback", 30}, {"mix", 35}}}});
    addT("ex.inverted_world", "Inverted World", "Experimental", "vocals, guitars, keys", "A held loop scanned backwards at half speed like reversed tape, under a spectral shimmer.",
         {{"grain_engine", P{{"mode", 2}, {"loop_ms", 3000}, {"speed", -0.5f}, {"varispeed", 1}, {"size_ms", 160}, {"density", 20}, {"gain", -5}, {"mix", 60}}},
          {"spectral", P{{"blur", 50}, {"shimmer", 35}, {"mix", 35}}}});
    addT("ex.fractal_feedback", "Fractal Feedback", "Experimental", "synths, vocals, noise", "Grains re-grained through minor-scale feedback, each pass a little lower and darker.",
         {{"grain_engine", P{{"pitch", -5}, {"pitch_jit", 3}, {"scale", 4}, {"feedback", 85}, {"delay_ms", 250}, {"fb_hp", 150}, {"fb_lp", 3000}, {"size_ms", 140}, {"density", 18}, {"mix", 45}}},
          {"reverb", P{{"mode", 3}, {"decay", 2}, {"mix", 25}}}});

    // ------------------------------------------------------------------ legacy ids (kept for DAW projects), re-filed
    struct Refile { const char* id; const char* cat; const char* tags; };
    static const Refile kRefile[] = {
        {"atm.slow_horizon", "Atmospheres", "pads, vocals, guitars"},     {"atm.glass_cathedral", "Atmospheres", "pads, keys, vocals"},
        {"atm.under_ice", "Atmospheres", "full mix, pads, loops"},        {"atm.afterglow", "Atmospheres", "guitars, keys, vocals"},
        {"voc.choir_of_one", "Vocal Transformations", "vocals"},          {"voc.wide_double", "Vocal Transformations", "vocals, guitars"},
        {"voc.octave_ghost", "Vocal Transformations", "vocals"},          {"voc.tuned_glide", "Vocal Transformations", "vocals"},
        {"frg.glass_grains", "Granular Motion", "keys, plucks, vocals"},   {"frg.backwards_letters", "Stretch & Smear", "vocals, guitars"},
        {"frg.frozen_breath", "Stretch & Smear", "pads, vocals"},         {"frg.loop_ghost", "Granular Motion", "loops, drums, keys"},
        {"mot.tidal_chorus", "Movement & Width", "guitars, synths, keys"}, {"mot.jet_wash", "Movement & Width", "drums, guitars, full mix"},
        {"mot.spiral_stairs", "Experimental", "synths, guitars"},         {"mot.leslie_room", "Movement & Width", "keys, organ, guitars"},
        {"rhy.dotted_rain", "Essentials", "guitars, vocals, synths"},     {"rhy.stutter_bloom", "Glitch & Rhythm", "drums, loops"},
        {"rhy.gated_pulse", "Glitch & Rhythm", "synths, pads, loops"},    {"rhy.tape_triplets", "Glitch & Rhythm", "guitars, drums, vocals"},
        {"ton.broken_transistor", "Character & Dirt", "guitars, drums, bass"}, {"ton.warm_console", "Essentials", "full mix, drums, bass"},
        {"ton.crushed_telephone", "Character & Dirt", "vocals, drums"},   {"ton.fold_space", "Experimental", "synths, drums, bass"},
        {"sig.supercluster", "Harmonic & Tonal", "keys, pads, vocals"},   {"sig.machine_choir", "Vocal Transformations", "vocals, synths"},
        {"sig.doppler_drift", "Experimental", "vocals, synths, percussion"}, {"sig.vocal_polish", "Essentials", "vocals"},
    };
    for (auto& p : v)
        for (const auto& r : kRefile)
            if (p.id == r.id) { p.category = r.cat; p.tags = r.tags; }
    // presets are listed (and stepped by the arrows) in category order, curated order inside a category
    const auto& cats = presetCategories();
    auto rank = [&](const std::string& c) { return static_cast<int>(std::find(cats.begin(), cats.end(), c) - cats.begin()); };
    std::stable_sort(v.begin(), v.end(), [&](const FactoryPreset& a, const FactoryPreset& b) { return rank(a.category) < rank(b.category); });
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
    meta.set("tags", p.tags);
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
