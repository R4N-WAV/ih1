// V4 factory bank additions (Phase 1): original IH1 recipes. Genre names are inspiration for the sound-design traits
// (rhythm, timbre, sampling, space), not copies of anyone's presets. Commercial factory presets (Portal, Infiltrator,
// Crystallizer, H3000, ...) were not copied, not referenced by name, and not imitated value-by-value.
//
// Also here: the V3 -> V4 category remap, intensity / style defaults for the V3 bank, and the topology-aware macro
// rules used for presets that do not define their own macros.
#include <algorithm>
#include <cmath>
#include <map>
#include <string>

#include "ih1/fx/FxProcessor.h"
#include "ih1/presets/Presets.h"

namespace ih1 {

namespace {
using PV = std::vector<std::pair<std::string, float>>;
using M = std::vector<PresetMacro>;

// V4 category names (presetCategories() order)
const char* UT = "Init / Utility";
const char* SC = "Signal Collapse";
const char* GR = "Granular / Reconstruction";
const char* RG = "Rhythm / Glitch / Stutter";
const char* PR = "Pitch / Reverse / Harmonic";
const char* SF = "Spectral / Frequency Space";
const char* DD = "Dirt / Degradation";
const char* MB = "Musical / Beautiful";
const char* AC = "Atmosphere / Cinematic / Drone";
const char* EX = "Experimental / Hybrid";
}  // namespace

void addV4Presets(std::vector<FactoryPreset>& v) {
    auto add = [&](const char* id, const char* name, const char* cat, const char* tags, const char* intensity, const char* styles, const char* desc,
                   std::vector<PresetFx> chain, M macros = {}) {
        FactoryPreset p{id, name, cat, desc, 4, std::move(chain), tags};
        p.intensity = intensity;
        p.styles = styles;
        p.macros = std::move(macros);
        v.push_back(std::move(p));
    };
    (void)UT;

    // ======================================================================================= Signal Collapse
    add("v4.signal_collapse", "Signal Collapse", SC, "vocals, drums, full mix, synths", "Extreme", "IDM, glitch, noise",
        "The spectrum is shattered into moving shards, crushed to a low sample rate and fed through a short, bounded feedback echo.",
        {{"spectral_shatter", PV{{"shards", 2}, {"division", 4}, {"reorder", 85}, {"freeze", 20}, {"gate", 25}, {"mix", 100}}},
         {"degrade", PV{{"rate", 5200}, {"bits", 8}, {"anti_alias", 0}, {"drive", 9}, {"mix", 100}}},
         {"echo_matrix", PV{{"division", 2}, {"pattern", 6}, {"feedback", 45}, {"drive", 50}, {"mix", 35}}}},
        {{"Damage", {{1, "rate", -3500.0f}, {1, "drive", 10.0f}}}, {"Chaos", {{0, "reorder", 15.0f}, {0, "gate", 40.0f}}}, {"Space", {{2, "mix", 40.0f}, {2, "feedback", 30.0f}}}});
    add("v4.bit_cathedral", "Bit Cathedral", SC, "pads, vocals, keys, guitars", "Extreme", "industrial, cinematic, noise",
        "A cathedral reverb whose tail is crushed to 9 bits and rung through bell modes: a ruined, glowing hall.",
        {{"modal", PV{{"material", 2}, {"note", 2}, {"octave", 2}, {"modes", 16}, {"decay", 4}, {"mix", 45}}},
         {"reverb", PV{{"mode", 4}, {"decay", 12}, {"mix", 70}}},
         {"degrade", PV{{"rate", 9000}, {"bits", 9}, {"anti_alias", 0}, {"wobble", 30}, {"mix", 100}}}});
    add("v4.frame_demolition", "Frame Demolition", SC, "drums, loops, full mix", "Extreme", "breakcore, deconstructed club",
        "Attacks are pulled off and rolled, the body is frozen between hits, and the bar is rebuilt in a scrambled order.",
        {{"transient_split", PV{{"sensitivity", 70}, {"repeats", 3}, {"spacing", 35}, {"body_mode", 2}, {"body_len", 180}, {"mix", 100}}},
         {"slice_shuffle", PV{{"length", 2}, {"slices", 3}, {"pattern", 1}, {"evolve", 1}, {"reverse", 35}, {"pitch", 4}, {"pitch_amt", 30}, {"mix", 100}}},
         {"fold_crush", PV{{"drive", 12}, {"fold", 45}, {"crush", 30}, {"mix", 70}}}});
    add("v4.corrupted_broadcast", "Corrupted Broadcast", SC, "vocals, field, full mix", "Strong", "industrial, experimental hip-hop",
        "A drifting ring modulator into a narrow radio band, static bursts and a frequency shift that never settles.",
        {{"ring_mod", PV{{"freq", 140}, {"mod_depth", 40}, {"mod_rate", 0.2f}, {"mix", 55}}},
         {"degrade", PV{{"rate", 8000}, {"bits", 10}, {"bandwidth", 80}, {"noise", 20}, {"mix", 100}}},
         {"texture", PV{{"type", 2}, {"amount", -8}, {"density", 55}, {"band", 2500}, {"mix", 100}}},
         {"freq_shift", PV{{"shift", 13}, {"mix", 60}}}});
    add("v4.dead_air", "Dead Air Engine", SC, "drums, vocals, loops", "Extreme", "glitch, noise, IDM",
        "Rhythmic dropouts, micro-loops and backwards fragments, clipped inside a bounded feedback loop.",
        {{"glitch", PV{{"division", 4}, {"repeat", 35}, {"ratchet", 6}, {"reverse", 25}, {"gate", 35}, {"lofi", 20}, {"seed", 23}, {"mix", 100}}},
         {"echo_matrix", PV{{"division", 4}, {"pattern", 3}, {"feedback", 55}, {"drive", 80}, {"tone", 5000}, {"mix", 40}}},
         {"saturation", PV{{"style", 3}, {"drive", 14}}}});
    add("v4.glass_shards", "Glass Shards", SC, "keys, guitars, drums, synths", "Extreme", "glitch, hyperpop",
        "Spectral shards gated on the sixteenth, struck through glass modes: splintered, ringing and metallic.",
        {{"spectral_shatter", PV{{"shards", 3}, {"division", 4}, {"reorder", 70}, {"gate", 35}, {"out", -6}, {"mix", 100}}},
         {"modal", PV{{"material", 3}, {"note", 7}, {"octave", 3}, {"excite", 1}, {"decay", 1.5f}, {"mix", 50}}},
         {"transient", PV{{"attack", 45}}}});
    add("v4.subatomic", "Subatomic", SC, "bass, drums, synths", "Strong", "leftfield bass, industrial",
        "Low frequencies are smeared late through dispersion, shifted an octave down and folded: a sub-heavy, unstable mass.",
        {{"dispersion", PV{{"stages", 4}, {"center", 150}, {"spread", 3}, {"curve", -80}, {"q", 2.5f}, {"mix", 100}}},
         {"pitch_shift", PV{{"shift_a", -12}, {"mix", 60}}},
         {"fold_crush", PV{{"drive", 10}, {"fold", 35}, {"crush", 0}, {"tone", 4000}, {"mix", 80}}}});
    add("v4.broken_clock", "Broken Clock", SC, "drums, loops, full mix", "Extreme", "breakcore, drill'n'bass",
        "A tape stop every bar, irregular retriggers and pitched slices: time itself keeps failing.",
        {{"time_warp", PV{{"mode", 1}, {"trigger", 2}, {"division", 7}, {"stop_ms", 350}, {"mix", 100}}},
         {"glitch", PV{{"division", 3}, {"repeat", 30}, {"ratchet", 4}, {"pitch", 25}, {"jump", 20}, {"seed", 61}, {"mix", 100}}},
         {"degrade", PV{{"rate", 16000}, {"bits", 11}, {"mix", 70}}}});

    // ======================================================================================= Granular / Reconstruction
    add("v4.pocket_archaeology", "Pocket Archaeology", GR, "drums, loops, keys, vocals", "Strong", "experimental hip-hop, abstract beats",
        "The last bar dug up and re-sequenced in strides, a few slices an octave down, warmed and given a small room.",
        {{"slice_shuffle", PV{{"length", 2}, {"slices", 2}, {"pattern", 2}, {"pitch", 1}, {"pitch_amt", 25}, {"reverse", 10}, {"mix", 100}}},
         {"tape_colour", PV{{"drive", 6}, {"hf_loss", 4}}},
         {"reverb", PV{{"mode", 0}, {"decay", 0.8f}, {"mix", 18}}}});
    add("v4.ghost_break", "Ghost Break", GR, "drums, loops", "Strong", "jungle, breakcore",
        "Breaks rebuilt from reversed slices and octave-down ghosts, with the attacks kept sharp.",
        {{"slice_shuffle", PV{{"length", 3}, {"slices", 2}, {"pattern", 6}, {"reverse", 40}, {"pitch", 1}, {"pitch_amt", 20}, {"fade", 1.5f}, {"mix", 100}}},
         {"transient", PV{{"attack", 40}, {"sustain", -20}, {"out", -9}}}});
    add("v4.borrowed_chorus", "Borrowed Chorus", GR, "vocals, keys, guitars", "Strong", "future garage, dream pop",
        "Overlapping grains pitched into fifths and octaves, spread wide: one voice becomes a borrowed choir.",
        {{"grain_engine", PV{{"size_ms", 180}, {"density", 30}, {"pitch_jit", 12}, {"scale", 2}, {"spread", 95}, {"stereo", 1}, {"mix", 70}}},
         {"chorus", PV{{"depth", 30}, {"mix", 30}}}, {"reverb", PV{{"mode", 1}, {"decay", 3}, {"mix", 25}}}});
    add("v4.velvet_fragments", "Velvet Fragments", GR, "vocals, keys, pads", "Moderate", "ambient, dream pop",
        "Soft, long grains with high overlap, quantised to a pentatonic scale: fragments that blur into a velvet chord.",
        {{"grain_engine", PV{{"size_ms", 320}, {"density", 18}, {"overlap", 6}, {"pitch_jit", 12}, {"scale", 5}, {"window", 1}, {"spray_ms", 600}, {"mix", 65}}},
         {"reverb", PV{{"mode", 1}, {"decay", 4}, {"mix", 30}}}});
    add("v4.half_remembered", "Half-Remembered", GR, "vocals, keys, guitars, loops", "Strong", "vaporwave, ambient",
        "A slow granular scan through the last seconds, with reversed fragments surfacing: memory more than playback.",
        {{"grain_engine", PV{{"mode", 1}, {"speed", 0.5f}, {"size_ms", 220}, {"density", 22}, {"reverse", 35}, {"spray_ms", 300}, {"mix", 100}}},
         {"tape_colour", PV{{"drive", 5}, {"hf_loss", 6}}}, {"vibrato", PV{{"depth", 12}, {"rate", 0.4f}}}});
    add("v4.sampler_jigsaw", "Sampler Jigsaw", GR, "drums, loops, vocals", "Strong", "footwork, juke",
        "Two-beat captures cut into 32 pieces and replayed as pairs with a swung, gated shuffle.",
        {{"slice_shuffle", PV{{"length", 1}, {"slices", 3}, {"pattern", 5}, {"gate", 70}, {"swing", 45}, {"mix", 100}}},
         {"compressor", PV{{"mode", 1}, {"threshold", -22}, {"ratio", 5}}}});
    add("v4.dust_cloud", "Dust Cloud", GR, "keys, pads, field, vocals", "Strong", "lowercase, ambient",
        "A sparse grain cloud with fine dust imprinted on it: the source turns into drifting particles.",
        {{"granular", PV{{"size_ms", 60}, {"density", 14}, {"spray_ms", 900}, {"spread", 90}, {"mix", 100}}},
         {"texture", PV{{"type", 0}, {"amount", -10}, {"density", 35}, {"band", 6000}, {"mix", 100}}},
         {"reverb", PV{{"mode", 3}, {"decay", 2}, {"mix", 30}}}});
    add("v4.time_mosaic", "Time Mosaic", GR, "vocals, drums, loops, full mix", "Extreme", "IDM, glitch",
        "Grains read from all over the last four seconds, re-triggered on the beat and scattered in pitch.",
        {{"grain_engine", PV{{"size_ms", 70}, {"density", 60}, {"position_ms", 2000}, {"spray_ms", 2000}, {"pitch_jit", 7}, {"retrig", 1}, {"sync", 1}, {"retrig_div", 7}, {"mix", 100}}},
         {"spectral_delay", PV{{"division", 6}, {"shape", 4}, {"feedback", 30}, {"mix", 30}}}});
    add("v4.attack_ghosts", "Attack Ghosts", GR, "drums, percussion, guitars", "Strong", "deconstructed club, IDM",
        "Every hit flams into a roll while its body plays backwards in chunks.",
        {{"transient_split", PV{{"sensitivity", 60}, {"repeats", 4}, {"spacing", 28}, {"rep_decay", 70}, {"body_mode", 1}, {"body_len", 300}, {"mix", 100}}},
         {"reverb", PV{{"mode", 2}, {"decay", 1.2f}, {"mix", 15}}}});
    add("v4.sustain_engine", "Sustain Engine", GR, "drums, guitars, keys", "Strong", "post-rock, ambient",
        "Attacks stay dry; the body after each hit freezes and holds until the next one: a drum kit that sustains like a pad.",
        {{"transient_split", PV{{"sensitivity", 55}, {"att_gain", 0}, {"body_mode", 2}, {"body_len", 250}, {"body_gain", 2}, {"mix", 100}}},
         {"reverb", PV{{"mode", 1}, {"decay", 5}, {"mix", 30}}}});
    add("v4.palindrome_tape", "Palindrome Tape", GR, "vocals, keys, loops", "Strong", "chopped and screwed, vaporwave",
        "The last bar plays there and back, slowed by varispeed into a wobbling, dragged loop.",
        {{"slice_shuffle", PV{{"length", 2}, {"slices", 1}, {"pattern", 4}, {"mix", 100}}},
         {"time_warp", PV{{"mode", 0}, {"speed", 0.82f}, {"mix", 100}}}, {"tape_colour", PV{{"drive", 6}, {"hf_loss", 5}}}});

    // ======================================================================================= Rhythm / Glitch / Stutter
    add("v4.false_downbeat", "False Downbeat", RG, "drums, loops, full mix", "Strong", "IDM, UK garage",
        "Retriggered fragments on a shifting step grid with a short, filtered echo pattern behind them.",
        {{"glitch", PV{{"division", 4}, {"repeat", 45}, {"ratchet", 3}, {"jump", 15}, {"seed", 4}, {"mix", 100}}},
         {"echo_matrix", PV{{"division", 4}, {"pattern", 2}, {"feedback", 20}, {"tone", 4500}, {"mix", 30}}}});
    add("v4.pocket_swing", "Pocket Swing", RG, "drums, vocals, keys", "Moderate", "UK garage, 2-step",
        "Swing-shuffled slices and swung off-beat echoes: the groove leans back.",
        {{"slice_shuffle", PV{{"length", 1}, {"slices", 2}, {"pattern", 0}, {"swing", 55}, {"gate", 85}, {"mix", 70}}},
         {"echo_matrix", PV{{"division", 4}, {"pattern", 2}, {"swing", 60}, {"feedback", 15}, {"mix", 25}}}});
    add("v4.juke_grid", "Juke Grid", RG, "drums, vocals, loops", "Strong", "footwork, juke",
        "Thirty-second ratchets and gated stabs, triplet echo taps in between.",
        {{"glitch", PV{{"division", 3}, {"repeat", 55}, {"ratchet", 8}, {"gate", 25}, {"seed", 37}, {"mix", 100}}},
         {"echo_matrix", PV{{"division", 3}, {"pattern", 4}, {"feedback", 10}, {"mix", 25}}}});
    add("v4.stop_start", "Stop / Start", RG, "drums, loops, full mix", "Strong", "experimental hip-hop, glitch",
        "Every bar the tape brakes and every slice afterwards stutters back to life.",
        {{"time_warp", PV{{"mode", 1}, {"trigger", 2}, {"division", 10}, {"stop_ms", 600}, {"mix", 100}}},
         {"stutter", PV{{"interval", 2}, {"chance", 50}, {"slice", 3}, {"mix", 100}}}});
    add("v4.reverse_roll", "Reverse Roll", RG, "drums, vocals, loops", "Strong", "breakcore, glitch",
        "Rolls of reversed slices whose pitch climbs step by step.",
        {{"stutter", PV{{"interval", 1}, {"chance", 70}, {"slice", 1}, {"pitch", 2}, {"reverse", 1}, {"mix", 100}}},
         {"reverb", PV{{"mode", 5}, {"decay", 0.6f}, {"mix", 20}}}});
    add("v4.mosaic_gate", "Mosaic Gate", RG, "pads, keys, full mix", "Strong", "deconstructed club, IDM",
        "A rhythmic gate in frequency space: bands drop out and return on the sixteenth.",
        {{"spectral_shatter", PV{{"shards", 1}, {"division", 4}, {"reorder", 0}, {"gate", 45}, {"smooth", 1}, {"mix", 100}}},
         {"gate", PV{{"mode", 2}, {"division", 7}, {"range", 24}, {"curve", 60}}}});
    add("v4.clave_machine", "Clave Machine", RG, "drums, percussion, synths", "Moderate", "dub techno, UK garage",
        "A triplet-grid echo rhythm with darkening taps and a driven, filtered loop.",
        {{"echo_matrix", PV{{"division", 6}, {"pattern", 4}, {"decay", 80}, {"darken", 50}, {"feedback", 40}, {"drive", 35}, {"mix", 40}}},
         {"filter", PV{{"type", 0}, {"cutoff", 5500}, {"resonance", 15}}}});
    add("v4.ratchet_vox", "Ratchet Vox", RG, "vocals", "Strong", "hyperpop, glitch pop",
        "Vocal ratchets with octave fragments and short gated repeats, tightened by a fast compressor.",
        {{"glitch", PV{{"division", 4}, {"repeat", 40}, {"ratchet", 5}, {"pitch", 30}, {"gate", 10}, {"seed", 88}, {"mix", 100}}},
         {"compressor", PV{{"mode", 1}, {"threshold", -24}, {"ratio", 6}}}, {"saturation", PV{{"drive", 8}}}});
    add("v4.irregular_heart", "Irregular Heart", RG, "drums, bass, loops", "Strong", "deconstructed club, footwork",
        "An evolving random-walk rebuild of every bar with ducking pumps: never quite the same rhythm twice, but repeatable.",
        {{"slice_shuffle", PV{{"length", 2}, {"slices", 2}, {"pattern", 6}, {"evolve", 1}, {"seed", 52}, {"gate", 80}, {"mix", 100}}},
         {"gate", PV{{"mode", 2}, {"division", 10}, {"range", 18}, {"curve", 40}}}});
    add("v4.backspin_cascade", "Backspin Cascade", RG, "vocals, drums, loops", "Strong", "chopped and screwed, experimental hip-hop",
        "Backspins every two bars into a cascade of reversed, pitched echoes.",
        {{"time_warp", PV{{"mode", 4}, {"trigger", 3}, {"division", 10}, {"stop_ms", 500}, {"mix", 100}}},
         {"reverse_echo", PV{{"interval", 7}, {"feedback", 35}, {"mix", 35}}}});

    // ======================================================================================= Pitch / Reverse / Harmonic
    add("v4.crystal_stairs", "Crystal Stairs", PR, "guitars, keys, vocals, synths", "Strong", "shoegaze, ambient",
        "Reversed echoes an octave up that keep climbing through the feedback: a shimmering staircase.",
        {{"reverse_echo", PV{{"interval", 12}, {"fine", 6}, {"feedback", 65}, {"tone", 8000}, {"mix", 50}}},
         {"reverb", PV{{"mode", 1}, {"decay", 4}, {"mix", 25}}}});
    add("v4.fifth_cascade", "Fifth Cascade", PR, "keys, guitars, pads", "Strong", "post-rock, cinematic",
        "Backwards grains in fifths, each pass another fifth up, softened by a long hall.",
        {{"reverse_echo", PV{{"interval", 7}, {"feedback", 55}, {"sync", 1}, {"division", 10}, {"mix", 45}}},
         {"reverb", PV{{"mode", 4}, {"decay", 7}, {"mix", 30}}}});
    add("v4.descent", "Descent", PR, "vocals, synths, drums", "Strong", "cinematic, industrial",
        "Reversed grains falling an octave each pass into a dark, filtered pit.",
        {{"reverse_echo", PV{{"interval", -12}, {"feedback", 60}, {"tone", 3000}, {"low_cut", 60}, {"mix", 50}}},
         {"saturation", PV{{"drive", 6}}}});
    add("v4.bell_choir", "Bell Choir", PR, "vocals, synths, keys", "Strong", "experimental, ambient",
        "Partials stretched apart into bell-like spacing and panned in alternation: a choir of struck metal.",
        {{"partial_recompose", PV{{"stretch", 45}, {"spread", 70}, {"residual", -20}, {"mix", 100}}},
         {"reverb", PV{{"mode", 2}, {"decay", 3}, {"mix", 25}}}});
    add("v4.hollow_harmonics", "Hollow Harmonics", PR, "synths, bass, guitars", "Moderate", "leftfield bass, IDM",
        "Even partials removed and the rest squeezed: a hollow, clarinet-like ghost of the source.",
        {{"partial_recompose", PV{{"stretch", -15}, {"odd_even", -90}, {"residual", -30}, {"mix", 100}}}, {"saturation", PV{{"drive", 6}}}});
    add("v4.metal_shift", "Metal Shift", PR, "drums, synths, vocals", "Strong", "industrial, psychedelic",
        "Every partial shifted 120 Hz up: harmony bends into inharmonic, ringing metal.",
        {{"partial_recompose", PV{{"shift", 120}, {"stretch", 10}, {"residual", -12}, {"mix", 100}}},
         {"modal", PV{{"material", 1}, {"note", 4}, {"octave", 3}, {"mix", 25}}}});
    add("v4.octave_shadow", "Octave Shadow", PR, "vocals, guitars, synths", "Moderate", "future garage, hyperpop",
        "An octave-down shadow on every other echo tap and an octave-up glint on the rest.",
        {{"echo_matrix", PV{{"division", 7}, {"pattern", 0}, {"pitch_mode", 4}, {"interval", 12}, {"decay", 70}, {"feedback", 20}, {"mix", 40}}},
         {"reverb", PV{{"mode", 2}, {"decay", 1.6f}, {"mix", 18}}}});
    add("v4.climbing_taps", "Climbing Taps", PR, "keys, guitars, vocals", "Strong", "IDM, glitch pop",
        "Eight echoes, each a minor third above the last, recirculating so the cycle keeps rising.",
        {{"echo_matrix", PV{{"division", 4}, {"pattern", 0}, {"pitch_mode", 1}, {"interval", 3}, {"pitch_loop", 1}, {"feedback", 35}, {"mix", 45}}},
         {"chorus", PV{{"depth", 20}, {"mix", 25}}}});
    add("v4.formant_morph", "Formant Glass", PR, "vocals", "Strong", "hyperpop, experimental",
        "A grafted, shifted spectral shape of the voice's own past moment, with formants pushed up: glassy, alien vocals.",
        {{"spectral_graft", PV{{"source", 1}, {"echo_ms", 300}, {"amount", 80}, {"formant", 5}, {"out", -4}, {"mix", 100}}},
         {"formant", PV{{"shift", 4}, {"mix", 70}}}, {"reverb", PV{{"mode", 2}, {"decay", 1.5f}, {"mix", 20}}}});

    // ======================================================================================= Spectral / Frequency Space
    add("v4.shard_field", "Shard Field", SF, "pads, field, keys", "Strong", "ambient, lowercase",
        "Slow-changing spectral shards, some frozen, all crossfaded: a frequency mosaic that breathes.",
        {{"spectral_shatter", PV{{"shards", 2}, {"division", 13}, {"reorder", 60}, {"freeze", 35}, {"smooth", 10}, {"mix", 100}}},
         {"reverb", PV{{"mode", 1}, {"decay", 5}, {"mix", 30}}}});
    add("v4.frequency_fold", "Frequency Fold", SF, "synths, drums, vocals", "Extreme", "IDM, deconstructed club",
        "Every shard moves on every sixteenth, with a third of them silenced: the register of the source is shuffled.",
        {{"spectral_shatter", PV{{"shards", 3}, {"division", 4}, {"reorder", 100}, {"gate", 30}, {"smooth", 2}, {"mix", 100}}},
         {"transient", PV{{"attack", 50}}}});
    add("v4.graft_memory", "Graft Memory", SF, "vocals, synths, pads", "Strong", "ambient, experimental",
        "The live sound wears the spectral shape of a held moment, refreshed every four bars.",
        {{"spectral_graft", PV{{"source", 0}, {"amount", 85}, {"refresh", 3}, {"resolution", 40}, {"mix", 100}}},
         {"reverb", PV{{"mode", 1}, {"decay", 3}, {"mix", 25}}}});
    add("v4.cross_talk", "Cross Talk", SF, "full mix, stereo loops, synths", "Moderate", "psychedelic, IDM",
        "Each channel takes the other's spectral shape: stereo images swap colour and shimmer.",
        {{"spectral_graft", PV{{"source", 2}, {"amount", 70}, {"resolution", 48}, {"mix", 100}}},
         {"chorus", PV{{"depth", 20}, {"mix", 25}}}});
    add("v4.echo_imprint", "Echo Imprint", SF, "vocals, drums, loops", "Strong", "dub techno, abstract beats",
        "Half a second ago's spectral shape is grafted onto now: rhythmic colour echoes without repeating audio.",
        {{"spectral_graft", PV{{"source", 1}, {"echo_ms", 500}, {"amount", 100}, {"smooth", 15}, {"mix", 100}}},
         {"spectral_delay", PV{{"division", 8}, {"shape", 0}, {"feedback", 30}, {"mix", 30}}}});
    add("v4.prism_delay", "Prism Delay", SF, "guitars, keys, vocals", "Moderate", "shoegaze, dream pop",
        "Highs arrive last in every echo, dispersed into a chirping prism.",
        {{"spectral_delay", PV{{"division", 7}, {"shape", 0}, {"spread", 90}, {"feedback", 45}, {"mix", 40}}},
         {"dispersion", PV{{"stages", 3}, {"center", 2000}, {"spread", 4}, {"curve", 60}, {"mix", 70}}}});
    add("v4.partial_drift", "Partial Drift", SF, "pads, keys, vocals", "Strong", "ambient, drone",
        "Partials sustain, stretch slightly and drift apart in the stereo field over a smeared bed.",
        {{"partial_recompose", PV{{"stretch", 12}, {"sustain", 70}, {"spread", 60}, {"residual", -24}, {"mix", 100}}},
         {"spectral_smear", PV{{"smear", 40}, {"time", 60}, {"scatter", 20}, {"mix", 50}}}});
    add("v4.laser_chirp", "Laser Chirp", SF, "drums, percussion, synths", "Strong", "IDM, industrial",
        "Sixty-four all-pass stages turn every attack into a falling laser chirp.",
        {{"dispersion", PV{{"stages", 5}, {"center", 900}, {"spread", 6}, {"curve", -70}, {"q", 3}, {"mix", 100}}},
         {"compressor", PV{{"mode", 1}, {"threshold", -20}, {"ratio", 4}}}});
    add("v4.spring_vortex", "Spring Vortex", SF, "guitars, drums, vocals", "Strong", "dub, psychedelic",
        "Dispersion with feedback and slow motion: a springy, swirling vortex around every transient.",
        {{"dispersion", PV{{"stages", 3}, {"center", 1200}, {"spread", 3}, {"q", 2}, {"feedback", 60}, {"mod_depth", 0.8f}, {"mod_rate", 0.15f}, {"mix", 80}}},
         {"echo_matrix", PV{{"division", 8}, {"pattern", 5}, {"feedback", 30}, {"mix", 25}}}});

    // ======================================================================================= Dirt / Degradation
    add("v4.tape_memory", "Tape Memory", DD, "keys, vocals, guitars, loops", "Moderate", "vaporwave, lo-fi hip-hop",
        "A slightly slow, wobbling tape with frequency loss, saturation and fine dust.",
        {{"time_warp", PV{{"mode", 0}, {"speed", 0.94f}, {"mix", 100}}}, {"tape_colour", PV{{"drive", 7}, {"hf_loss", 6}}},
         {"vibrato", PV{{"depth", 14}, {"rate", 0.6f}}}, {"texture", PV{{"type", 1}, {"amount", -16}, {"density", 25}, {"mix", 100}}}});
    add("v4.vinyl_ghost", "Vinyl Ghost", DD, "keys, vocals, full mix", "Moderate", "lo-fi, vaporwave, ambient",
        "Crackle that follows the music, a narrow band and a soft 12-bit sampler finish.",
        {{"texture", PV{{"type", 1}, {"amount", -8}, {"density", 45}, {"follow", 70}, {"mix", 100}}},
         {"degrade", PV{{"rate", 22000}, {"bits", 12}, {"bandwidth", 35}, {"mix", 100}}}});
    add("v4.friction_drive", "Friction Drive", DD, "guitars, bass, synths", "Strong", "industrial, noise",
        "A scraped, bowed grain carried by the sound, then folded: abrasive but locked to the playing.",
        {{"texture", PV{{"type", 3}, {"amount", 0}, {"band", 1800}, {"width", 40}, {"mix", 100}}},
         {"fold_crush", PV{{"drive", 14}, {"fold", 55}, {"crush", 15}, {"mix", 90}}}});
    add("v4.grit_imprint", "Grit Imprint", DD, "drums, vocals, synths", "Strong", "glitch, IDM",
        "The signal is chopped by granular noise and crushed: a gritty, buzzing surface.",
        {{"texture", PV{{"type", 4}, {"amount", -4}, {"density", 60}, {"mix", 100}}}, {"bitcrush", PV{{"bits", 9}, {"mix", 60}}}});
    add("v4.static_storm", "Static Storm", DD, "vocals, field, synths", "Extreme", "noise, power electronics",
        "Interference bursts on every attack, a collapsing bandwidth and aliasing everywhere.",
        {{"texture", PV{{"type", 2}, {"amount", 0}, {"density", 75}, {"transient", 80}, {"mix", 100}}},
         {"degrade", PV{{"rate", 4000}, {"bits", 7}, {"anti_alias", 0}, {"bandwidth", 60}, {"drive", 12}, {"mix", 100}}}});
    add("v4.cassette_choir", "Cassette Choir", DD, "vocals, keys", "Moderate", "lo-fi, dream pop",
        "Doubled into a small choir and printed to a worn cassette.",
        {{"doubler", PV{{"voices", 1}, {"detune", 18}, {"mix", 60}}}, {"degrade", PV{{"rate", 18000}, {"bits", 11}, {"bandwidth", 40}, {"wobble", 30}, {"noise", 15}}}});
    add("v4.dirty_room", "Dirty Room", DD, "drums, loops", "Strong", "experimental hip-hop, boom bap",
        "Drums squashed hard, crushed and put in a small, dusty room.",
        {{"compressor", PV{{"mode", 1}, {"threshold", -30}, {"ratio", 8}}}, {"bitcrush", PV{{"bits", 10}, {"rate", 26000}, {"mix", 70}}},
         {"reverb", PV{{"mode", 0}, {"decay", 0.7f}, {"mix", 22}}}, {"texture", PV{{"type", 0}, {"amount", -18}, {"mix", 100}}}});

    // ======================================================================================= Musical / Beautiful
    add("v4.silver_thread", "Silver Thread", MB, "vocals, keys, guitars", "Moderate", "dream pop, ambient",
        "A light, high grain shimmer an octave above, restrained stereo motion and a soft hall.",
        {{"grain_engine", PV{{"size_ms", 140}, {"density", 20}, {"pitch", 12}, {"spread", 60}, {"mix", 25}}},
         {"reverb", PV{{"mode", 1}, {"decay", 3.5f}, {"low_cut", 200}, {"mix", 25}}}});
    add("v4.soft_focus", "Soft Focus", MB, "vocals, keys, pads, full mix", "Subtle", "dream pop, ambient",
        "Gentle dispersion smear, light diffusion and a touch more width: the edges soften.",
        {{"dispersion", PV{{"stages", 2}, {"center", 3000}, {"spread", 3}, {"q", 0.8f}, {"mix", 35}}},
         {"width", PV{{"width", 120}}}, {"reverb", PV{{"mode", 3}, {"decay", 1.2f}, {"mix", 15}}}});
    add("v4.halo_chords", "Halo Chords", MB, "keys, guitars, pads", "Moderate", "post-rock, cinematic",
        "Pitched echo taps in fifths and octaves under a slow chorus and a restrained tail.",
        {{"echo_matrix", PV{{"division", 10}, {"pattern", 1}, {"pitch_mode", 3}, {"feedback", 25}, {"mix", 30}}},
         {"chorus", PV{{"depth", 25}, {"rate", 0.3f}, {"mix", 30}}}, {"reverb", PV{{"mode", 1}, {"decay", 4}, {"mix", 25}}}});
    add("v4.air_between_notes", "Air Between Notes", MB, "vocals, guitars, keys", "Subtle", "singer-songwriter, ambient",
        "Echoes only between the phrases (ducked), lightly diffused and darkened.",
        {{"delay", PV{{"sync", 1}, {"division", 10}, {"feedback", 35}, {"duck", 70}, {"diffusion", 35}, {"high_cut", 5000}, {"mix", 30}}},
         {"reverb", PV{{"mode", 3}, {"decay", 1.5f}, {"mix", 12}}}});
    add("v4.infinite_instrument", "Infinite Instrument", MB, "keys, guitars, vocals", "Strong", "ambient, drone",
        "Short notes ring on as held partials and tuned resonances: the instrument never quite stops.",
        {{"partial_recompose", PV{{"stretch", 0}, {"sustain", 80}, {"residual", -12}, {"mix", 60}}},
         {"modal", PV{{"material", 0}, {"note", 0}, {"octave", 2}, {"decay", 6}, {"mix", 25}}},
         {"reverb", PV{{"mode", 1}, {"decay", 5}, {"mix", 25}}}});
    add("v4.memory_bloom", "Memory Bloom", MB, "keys, vocals, pads", "Moderate", "ambient, dream pop",
        "A slow, pitch-kept stretch feeding a soft, filtered grain cloud.",
        {{"grain_engine", PV{{"mode", 1}, {"speed", 0.5f}, {"size_ms", 260}, {"density", 16}, {"mix", 45}}},
         {"granular", PV{{"size_ms", 200}, {"density", 10}, {"spray_ms", 800}, {"mix", 30}}}, {"eq", PV{{"lp", 9000}}}});
    add("v4.glass_room", "Glass Room", MB, "vocals, keys, guitars", "Subtle", "pop, indie",
        "A small convolved space, micro detune and a glassy resonance underneath.",
        {{"convolution", PV{{"ir", 1}, {"mix", 20}}}, {"doubler", PV{{"detune", 8}, {"mix", 30}}},
         {"modal", PV{{"material", 3}, {"note", 0}, {"octave", 4}, {"excite", 1}, {"decay", 1}, {"mix", 10}}}},
        // V5: authored macros (the combination's Ring was too faint with the glass layer at 10 %)
        {{"Glass", {{2, "mix", 30.0f}, {2, "decay", 2.0f}, {2, "bright", 25.0f}}}, {"Space", {{0, "mix", 25.0f}, {1, "mix", 25.0f}}}});
    add("v4.warm_echo_hall", "Warm Echo Hall", MB, "vocals, guitars, keys", "Moderate", "dub, soul",
        "Tape echoes into a warm hall: classic depth with character.",
        {{"tape_echo", PV{{"intensity", 45}, {"mix", 30}}}, {"reverb", PV{{"mode", 1}, {"decay", 2.8f}, {"mix", 22}}}});
    add("v4.bell_garden", "Bell Garden", MB, "keys, guitars, plucks", "Moderate", "ambient, cinematic",
        "Attacks strike soft bells tuned to the key, with a gentle shimmer above.",
        {{"modal", PV{{"material", 2}, {"note", 0}, {"octave", 3}, {"excite", 1}, {"decay", 3}, {"mix", 35}}},
         {"shimmer", PV{{"mix", 20}}}});
    add("v4.silk_doubles", "Silk Doubles", MB, "vocals, guitars", "Subtle", "pop, indie",
        "Wide doubles, a soft exciter lift and a short plate.",
        {{"doubler", PV{{"voices", 1}, {"detune", 10}, {"width", 100}, {"mix", 40}}}, {"exciter", PV{{"amount", 20}}},
         {"reverb", PV{{"mode", 2}, {"decay", 1.2f}, {"mix", 14}}}});
    add("v4.lullaby_cloud", "Lullaby Cloud", MB, "vocals, keys, music box", "Moderate", "ambient, dream pop",
        "A soft, major-scale grain cloud floating behind the source.",
        {{"granular", PV{{"size_ms", 220}, {"density", 12}, {"pitch", 12}, {"spray_ms", 700}, {"spread", 80}, {"mix", 30}}},
         {"reverb", PV{{"mode", 1}, {"decay", 4.5f}, {"mix", 28}}}});
    add("v4.velvet_motion", "Velvet Motion", MB, "pads, keys, synths, guitars", "Moderate", "dream pop, synthwave",
        "Slow phasing, a gentle tremolo and a wide chorus: lush, moving, never seasick.",
        {{"phaser", PV{{"rate", 0.15f}, {"mix", 40}}}, {"chorus", PV{{"depth", 30}, {"rate", 0.25f}, {"mix", 35}}},
         {"tremolo", PV{{"depth", 20}}}});

    // ======================================================================================= Atmosphere / Cinematic / Drone
    add("v4.monolith", "Monolith", AC, "pads, field, drums, vocals", "Extreme", "cinematic, drone",
        "A low, membrane-like resonance swallowed by a vast reverb and a frozen spectrum.",
        {{"modal", PV{{"material", 5}, {"note", 2}, {"octave", 1}, {"decay", 8}, {"mix", 50}}},
         {"spectral", PV{{"freeze", 1}, {"fade_ms", 2000}, {"blur", 60}, {"mix", 60}}}, {"reverb", PV{{"mode", 4}, {"decay", 18}, {"mix", 55}}}});
    add("v4.reverse_dawn", "Reverse Dawn", AC, "vocals, keys, guitars", "Strong", "cinematic, post-rock",
        "Reversed swells rising out of a long reverse reverb, lightly pitched up.",
        {{"reverse", PV{{"mode", 0}, {"sync", 1}, {"division", 13}, {"mix", 70}}}, {"reverse_echo", PV{{"interval", 5}, {"feedback", 30}, {"mix", 30}}},
         {"reverb", PV{{"mode", 6}, {"decay", 6}, {"mix", 40}}}});
    add("v4.tidal_body", "Tidal Body", AC, "field, pads, vocals", "Strong", "drone, ambient",
        "Spectral smear and a slow dispersion sweep: the sound becomes a tide of frequencies.",
        {{"spectral_smear", PV{{"smear", 70}, {"time", 80}, {"scatter", 40}, {"out", 9}, {"mix", 100}}},
         {"dispersion", PV{{"stages", 3}, {"center", 500}, {"spread", 5}, {"mod_depth", 1.2f}, {"mod_rate", 0.05f}, {"mix", 70}}}});
    add("v4.trailer_hit", "Trailer Hit", AC, "drums, percussion, full mix", "Extreme", "cinematic, trailer",
        "Hits become huge: an octave-down body, a struck plate and a dark, enormous tail.",
        {{"transient", PV{{"attack", 50}, {"sustain", 40}, {"out", -4}}}, {"pitch_shift", PV{{"shift_a", -12}, {"level_b", -40}, {"mix", 45}}},
         {"modal", PV{{"material", 4}, {"note", 0}, {"octave", 1}, {"excite", 1}, {"decay", 4}, {"mix", 35}}}, {"reverb", PV{{"mode", 4}, {"decay", 9}, {"mix", 40}}}});
    add("v4.ghost_station", "Ghost Station", AC, "vocals, field, full mix", "Strong", "dub ambient, hauntology",
        "A distant, radio-band voice with static, echoes and a cold hall.",
        {{"degrade", PV{{"rate", 11000}, {"bandwidth", 75}, {"noise", 20}}}, {"texture", PV{{"type", 2}, {"amount", -14}, {"density", 30}}},
         {"echo_matrix", PV{{"division", 8}, {"pattern", 5}, {"feedback", 45}, {"darken", 60}, {"mix", 35}}}, {"reverb", PV{{"mode", 1}, {"decay", 6}, {"mix", 35}}}});
    add("v4.slow_planet", "Slow Planet", AC, "pads, keys, drones", "Strong", "drone, ambient",
        "Half-speed grains, a long spectral delay and a held freeze: a slowly turning mass.",
        {{"grain_engine", PV{{"mode", 1}, {"speed", 0.3f}, {"size_ms", 400}, {"density", 10}, {"mix", 100}}},
         {"spectral_delay", PV{{"division", 13}, {"shape", 2}, {"feedback", 55}, {"mix", 40}}}, {"reverb", PV{{"mode", 4}, {"decay", 12}, {"mix", 40}}}});
    add("v4.resonant_sky", "Resonant Sky", AC, "pads, vocals, guitars", "Moderate", "ambient, cinematic",
        "Glass modes ring above a shimmer reverb: a bright, open sky.",
        {{"modal", PV{{"material", 3}, {"note", 7}, {"octave", 4}, {"decay", 5}, {"mix", 30}}}, {"shimmer", PV{{"mix", 40}}}});
    add("v4.underworld", "Underworld", AC, "vocals, synths, drums", "Extreme", "cinematic, industrial, dark ambient",
        "Falling reversed echoes, a downward dispersion and a dark, folding drive.",
        {{"reverse_echo", PV{{"interval", -7}, {"feedback", 60}, {"tone", 2500}, {"mix", 50}}},
         {"dispersion", PV{{"stages", 4}, {"center", 200}, {"curve", -90}, {"mix", 80}}}, {"fold_crush", PV{{"drive", 8}, {"fold", 25}, {"mix", 60}}}});

    // ======================================================================================= Experimental / Hybrid
    add("v4.modal_rebuild", "Modal Rebuild", EX, "drums, loops, percussion", "Extreme", "IDM, experimental",
        "Bars rebuilt from scrambled slices whose attacks strike a bar-metal resonator.",
        {{"slice_shuffle", PV{{"length", 2}, {"slices", 2}, {"pattern", 3}, {"mix", 100}}},
         {"modal", PV{{"material", 1}, {"note", 9}, {"octave", 3}, {"excite", 1}, {"decay", 2.0f}, {"mix", 70}}}});
    add("v4.graft_cascade", "Graft Cascade", EX, "vocals, synths", "Strong", "experimental, hyperpop",
        "A grafted spectral shape, then reversed pitch echoes climbing out of it.",
        {{"spectral_graft", PV{{"source", 0}, {"amount", 70}, {"refresh", 2}, {"mix", 100}}}, {"reverse_echo", PV{{"interval", 12}, {"feedback", 50}, {"mix", 40}}}});
    add("v4.split_world", "Split World", EX, "drums, guitars, full mix", "Extreme", "deconstructed club, experimental",
        "Attacks stay put while the body is smeared, shattered and pushed into a different register.",
        {{"transient_split", PV{{"body_mode", 3}, {"body_gain", 3}, {"mix", 100}}}, {"spectral_shatter", PV{{"shards", 1}, {"division", 7}, {"reorder", 70}, {"mix", 70}}}});
    add("v4.texture_reactor", "Texture Reactor", EX, "drums, vocals, field", "Strong", "noise, IDM",
        "Static and grit imprinted on the attacks, then folded and ring-modulated.",
        {{"texture", PV{{"type", 2}, {"amount", -6}, {"transient", 90}}}, {"ring_mod", PV{{"freq", 420}, {"mix", 35}}}, {"fold_crush", PV{{"drive", 10}, {"fold", 40}, {"mix", 70}}}});
    add("v4.echo_lattice", "Echo Lattice", EX, "keys, guitars, synths", "Strong", "IDM, dub techno",
        "A zigzag-pitched echo grid ringing through modal resonances and dispersion.",
        {{"echo_matrix", PV{{"division", 4}, {"pattern", 3}, {"pitch_mode", 2}, {"interval", 5}, {"feedback", 40}, {"mix", 45}}},
         {"modal", PV{{"material", 4}, {"note", 2}, {"octave", 3}, {"decay", 2}, {"mix", 25}}}, {"dispersion", PV{{"stages", 2}, {"mix", 50}}}});
    add("v4.partial_glitch", "Partial Glitch", EX, "synths, vocals, keys", "Extreme", "glitch, IDM",
        "Partials stretched and shifted, then glitched on the sixteenth: harmonic structure in pieces.",
        {{"partial_recompose", PV{{"stretch", 60}, {"shift", -60}, {"residual", -18}, {"mix", 100}}}, {"glitch", PV{{"division", 4}, {"repeat", 35}, {"reverse", 20}, {"seed", 15}, {"mix", 100}}}});
    add("v4.dust_choir", "Dust Choir", EX, "vocals, pads", "Strong", "lowercase, ambient",
        "A choir of bell partials made of dust and frozen breath.",
        {{"partial_recompose", PV{{"stretch", 30}, {"sustain", 60}, {"spread", 80}, {"residual", -40}, {"mix", 100}}},
         {"texture", PV{{"type", 0}, {"amount", -10}, {"density", 50}, {"band", 8000}}}, {"reverb", PV{{"mode", 4}, {"decay", 8}, {"mix", 40}}}});
}

// ---------------------------------------------------------------------------------------------- V3 bank, V4 metadata
std::string v4CategoryFor(const std::string& v3) {
    static const std::map<std::string, std::string> m = {
        {"Init / Utility", "Init / Utility"},
        {"Grime & Dirt", "Dirt / Degradation"},
        {"Stretch & Smear", "Granular / Reconstruction"},
        {"Pitch Mutation", "Pitch / Reverse / Harmonic"},
        {"Rhythmic Damage", "Rhythm / Glitch / Stutter"},
        {"Granular Worlds", "Granular / Reconstruction"},
        {"Spectral Objects", "Spectral / Frequency Space"},
        {"Feedback Machines", "Signal Collapse"},
        {"Vocal Transformations", "Pitch / Reverse / Harmonic"},
        {"Cinematic / Spatial", "Atmosphere / Cinematic / Drone"},
        {"Hybrid / Experimental", "Experimental / Hybrid"},
    };
    const auto it = m.find(v3);
    return it == m.end() ? v3 : it->second;
}

std::string v4DefaultIntensity(const std::string& category, const std::string& id) {
    static const char* extreme[] = {"g3.alias_grinder", "cd.blown_speaker", "gr.slice_retrigger", "vt.giant", "ss.smear_field"};
    for (const char* e : extreme) if (id == e) return "Extreme";
    if (category == "Init / Utility") return "Subtle";
    if (category == "Musical / Beautiful") return "Moderate";
    return "Strong";
}

// Topology-aware macro rules: (FX type, parameter, macro kind, plain-value depth). A preset without its own macros gets
// up to three kinds that its chain can actually express, each reaching every matching parameter in the chain.
namespace {
struct Rule { const char* type; const char* param; int kind; float depth; };
enum Kind { Space = 0, Damage, Motion, Chaos, Pitch, Blur, kKinds };
const char* kKindName[kKinds] = {"Space", "Damage", "Motion", "Chaos", "Pitch", "Blur"};
const Rule kRules[] = {
    {"reverb", "mix", Space, 30.0f}, {"reverb", "decay", Space, 4.0f}, {"delay", "feedback", Space, 25.0f}, {"delay", "mix", Space, 20.0f},
    {"tape_echo", "mix", Space, 25.0f}, {"shimmer", "mix", Space, 25.0f}, {"convolution", "mix", Space, 25.0f},
    {"echo_matrix", "feedback", Space, 25.0f}, {"echo_matrix", "mix", Space, 20.0f}, {"spectral_delay", "feedback", Space, 25.0f},
    {"reverse_echo", "feedback", Space, 20.0f}, {"modal", "decay", Space, 3.0f}, {"granular", "diffusion", Space, 40.0f},
    {"degrade", "rate", Damage, -4000.0f}, {"degrade", "drive", Damage, 8.0f}, {"fold_crush", "drive", Damage, 8.0f}, {"fold_crush", "crush", Damage, 40.0f},
    {"bitcrush", "bits", Damage, -4.0f}, {"saturation", "drive", Damage, 10.0f}, {"amp", "gain", Damage, 10.0f}, {"texture", "amount", Damage, 10.0f},
    {"tape_colour", "drive", Damage, 8.0f}, {"grain_engine", "fb_drive", Damage, 10.0f},
    {"chorus", "depth", Motion, 35.0f}, {"flanger", "depth", Motion, 35.0f}, {"vibrato", "depth", Motion, 25.0f}, {"filter", "depth", Motion, 40.0f},
    {"tremolo", "depth", Motion, 40.0f}, {"dispersion", "mod_depth", Motion, 0.8f}, {"grain_engine", "spread", Motion, 40.0f},
    {"spectral_smear", "scatter", Motion, 30.0f}, {"width", "width", Motion, 50.0f}, {"phaser", "rate", Motion, 0.6f},
    {"glitch", "repeat", Chaos, 25.0f}, {"glitch", "jump", Chaos, 25.0f}, {"glitch", "reverse", Chaos, 20.0f}, {"grain_engine", "pitch_jit", Chaos, 10.0f},
    {"grain_engine", "spray_ms", Chaos, 800.0f}, {"granular", "scatter", Chaos, 35.0f}, {"slice_shuffle", "reverse", Chaos, 40.0f},
    {"slice_shuffle", "pitch_amt", Chaos, 40.0f}, {"spectral_shatter", "reorder", Chaos, 35.0f}, {"spectral_shatter", "gate", Chaos, 25.0f},
    {"stutter", "chance", Chaos, 40.0f}, {"texture", "density", Chaos, 40.0f}, {"transient_split", "repeats", Chaos, 3.0f},
    {"partial_recompose", "stretch", Pitch, 40.0f}, {"freq_shift", "shift", Pitch, 150.0f}, {"modal", "inharm", Pitch, 35.0f},
    {"reverse_echo", "fine", Pitch, 25.0f}, {"echo_matrix", "interval", Pitch, 5.0f}, {"partial_recompose", "shift", Pitch, 80.0f},
    {"pitch_shift", "feedback", Space, 30.0f}, {"pitch_shift", "fine_a", Pitch, 25.0f}, {"ring_mod", "mod_depth", Motion, 40.0f},
    {"ring_mod", "mix", Damage, 25.0f}, {"harmony", "detune", Motion, 15.0f}, {"harmony", "vib_depth", Motion, 30.0f},
    {"vocoder", "shift", Pitch, 4.0f}, {"formant", "shift", Pitch, 4.0f}, {"time_warp", "scrub_depth", Chaos, 200.0f},
    {"time_warp", "tone", Blur, -4000.0f}, {"looper", "fade_ms", Blur, 40.0f},
    {"spectral_smear", "time", Blur, 35.0f}, {"spectral_smear", "smear", Blur, 30.0f}, {"spectral", "blur", Blur, 40.0f},
    {"spectral_graft", "amount", Blur, 30.0f}, {"partial_recompose", "sustain", Blur, 50.0f}, {"grain_engine", "size_ms", Blur, 150.0f},
};
// priority of macro kinds by category (the gestures that suit it first)
std::vector<int> priority(const std::string& cat) {
    if (cat == "Signal Collapse" || cat == "Dirt / Degradation") return {Damage, Chaos, Space, Motion, Blur, Pitch};
    if (cat == "Rhythm / Glitch / Stutter") return {Chaos, Space, Damage, Motion, Pitch, Blur};
    if (cat == "Granular / Reconstruction") return {Chaos, Blur, Space, Pitch, Motion, Damage};
    if (cat == "Pitch / Reverse / Harmonic") return {Pitch, Space, Chaos, Motion, Blur, Damage};
    if (cat == "Spectral / Frequency Space") return {Blur, Chaos, Space, Pitch, Motion, Damage};
    if (cat == "Atmosphere / Cinematic / Drone") return {Space, Blur, Motion, Damage, Chaos, Pitch};
    if (cat == "Musical / Beautiful") return {Space, Motion, Blur, Pitch, Chaos, Damage};
    if (cat == "Init / Utility") return {Space, Motion, Damage, Blur, Chaos, Pitch};
    return {Chaos, Damage, Space, Pitch, Blur, Motion};
}
}  // namespace

std::vector<PresetMacro> deriveMacros(const FactoryPreset& p) {
    std::vector<PresetMacro> out;
    for (int kind : priority(p.category)) {
        PresetMacro m;
        m.name = kKindName[kind];
        for (size_t s = 0; s < p.chain.size(); ++s) {
            const int t = fxTypeIndex(p.chain[s].type);
            if (t < 0) continue;
            const auto& d = fxRegistry()[static_cast<size_t>(t)];
            for (const auto& r : kRules) {
                if (r.kind != kind || p.chain[s].type != r.type) continue;
                const int k = d.paramIndex(r.param);
                if (k < 0 || d.params[static_cast<size_t>(k)].kind != ParamKind::Float) continue;
                m.targets.push_back({static_cast<int>(s), r.param, r.depth});
            }
        }
        if (!m.targets.empty()) out.push_back(std::move(m));
        if (out.size() >= 3) break;
    }
    return out;
}

}  // namespace ih1
