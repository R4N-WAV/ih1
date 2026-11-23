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
    // V3 categories (Phase 7.2). The last one, "User", holds user presets.
    // V4 categories (Phase 1.4): sound-led names, Init pinned first. V3 categories are remapped onto these.
    // V5 (Phase 4): Drums / Drum Bus and Tremolo / Groove. V6 (Phase 3): Vocal / Voice.
    static const std::vector<std::string> c = {"Init / Utility", "Signal Collapse", "Granular / Reconstruction", "Rhythm / Glitch / Stutter",
                                               "Pitch / Reverse / Harmonic", "Spectral / Frequency Space", "Dirt / Degradation", "Musical / Beautiful",
                                               "Atmosphere / Cinematic / Drone", "Drums / Drum Bus", "Tremolo / Groove", "Vocal / Voice", "Experimental / Hybrid", "User"};
    return c;
}

namespace {
using P = std::vector<std::pair<std::string, float>>;

std::vector<FactoryPreset> build() {
    // V3 factory bank (Phase 7.2 / 7.3): Init + utility presets, then ten creative categories. The creative presets
    // are designed wet-forward: the dry signal is not kept "for safety"; each one has a deliberate signal path and is
    // audited for its transformation (tools/ih1_preset_audit, docs/V3_PRESET_AUDIT.md). Ids of V2.1 presets whose
    // concept continues are kept (projects store "f:<id>"), with revised, bolder values.
    std::vector<FactoryPreset> v;
    auto add = [&](const char* id, const char* name, const char* cat, const char* tags, const char* desc, std::vector<PresetFx> chain) {
        v.push_back({id, name, cat, desc, 2, std::move(chain), tags});
    };
    const char* U = "Init / Utility";
    const char* G = "Grime & Dirt";
    const char* S = "Stretch & Smear";
    const char* PM = "Pitch Mutation";
    const char* R = "Rhythmic Damage";
    const char* GW = "Granular Worlds";
    const char* SO = "Spectral Objects";
    const char* FM = "Feedback Machines";
    const char* VT = "Vocal Transformations";
    const char* CS = "Cinematic / Spatial";
    const char* HX = "Hybrid / Experimental";

    // ================================================================== Init / Utility (enhancement, not creative)
    v.push_back({kInitPresetId, "Init", U, "The clean starting state: no committed FX, nothing staged, every setting at its default.", 2, {}, "any source"});
    add("ess.mix_glue", "Mix Glue", U, "full mix, drums, bass", "Gentle bus compression into soft tape: glue and weight, nothing more.",
        {{"compressor", P{{"threshold", -18}, {"ratio", 2}, {"attack", 20}, {"release", 200}}}, {"tape_colour", P{{"drive", 4}}}});
    add("ess.plate_vocal", "Velvet Plate", U, "vocals", "A vocal chain: de-ess, presence, levelling and a short plate.",
        {{"deesser", P{{"threshold", -10}}}, {"eq", P{{"hp", 90}, {"b2_freq", 3200}, {"b2_gain", 2}}}, {"compressor", P{{"threshold", -22}, {"ratio", 3}}},
         {"reverb", P{{"mode", 2}, {"decay", 1.6f}, {"mix", 18}}}});
    add("ess.clean_slap", "Clean Slap", U, "vocals, guitars, drums", "A tight slapback and a small room: depth without blur.",
        {{"delay", P{{"sync", 0}, {"time_ms", 95}, {"feedback", 12}, {"mix", 22}}}, {"reverb", P{{"mode", 0}, {"decay", 0.7f}, {"mix", 14}}}});
    add("ess.quarter_throw", "Quarter Throw", U, "vocals, keys, synths", "Tempo echoes on the quarter note, filtered to sit behind the source.",
        {{"delay", P{{"sync", 1}, {"division", 10}, {"stereo", 2}, {"feedback", 30}, {"high_cut", 4500}, {"mix", 25}}}, {"reverb", P{{"mode", 1}, {"decay", 2.0f}, {"mix", 12}}}});
    add("ton.warm_console", "Warm Console", U, "full mix, drums, bass, keys", "Console warmth, a firmer attack and a lifted top.",
        {{"transient", P{{"attack", 12}}}, {"tape_colour", P{{"mode", 1}, {"drive", 5}, {"out", -2}}}, {"exciter", P{{"amount", 15}}}});

    // ================================================================== Grime & Dirt
    add("cd.lofi_sampler", "Sampler Ghost", G, "drums, keys, loops, vocals", "A vintage 12-bit sampler at 26 kHz: rounded highs, held-sample grain, a little hiss and drift.",
        {{"degrade", P{{"rate", 26000}, {"bits", 12}, {"anti_alias", 1}, {"reconstruct", 1}, {"bandwidth", 15}, {"drive", 3}, {"wobble", 8}, {"noise", 12}}},
         {"tape_colour", P{{"drive", 5}, {"hf_loss", 3}}}});
    add("g3.alias_grinder", "Alias Grinder", G, "drums, synths, bass, full mix", "Raw 3.2 kHz sample-and-hold with no anti-alias filter: highs fold back as metallic, inharmonic grit.",
        {{"degrade", P{{"rate", 3200}, {"bits", 7}, {"anti_alias", 0}, {"reconstruct", 0}, {"bandwidth", 0}, {"drive", 9}, {"jitter", 25}, {"noise", 0}, {"mix", 100}}},
         {"eq", P{{"hp", 60}, {"lp", 14000}}}});
    add("cd.cassette", "Bin Cassette", G, "vocals, keys, guitars, loops", "A chewed tape found in a skip: narrow band, warped speed, crushed dynamics.",
        {{"degrade", P{{"rate", 16000}, {"bits", 10}, {"bandwidth", 55}, {"wobble", 45}, {"noise", 30}, {"drive", 8}}},
         {"vibrato", P{{"depth", 28}, {"rate", 0.7f}}}, {"compressor", P{{"mode", 2}, {"threshold", -30}, {"ratio", 6}, {"auto_makeup", 0}, {"makeup", 4}}}});
    add("ton.fold_space", "Folded Steel", G, "synths, bass, drums, guitars", "Hard wavefolding that bites harder the louder you play, darkened afterwards: a metallic, ringing distortion.",
        {{"fold_crush", P{{"drive", 20}, {"fold", 90}, {"bias", 25}, {"dynamics", 60}, {"crush", 0}, {"tone", 7000}}}, {"filter", P{{"type", 0}, {"cutoff", 6000}, {"resonance", 20}}}});
    add("g3.splinter_hits", "Splinter Hits", G, "drums, percussion, loops", "Only the transients are crushed into splinters; the body stays thick and folded.",
        {{"fold_crush", P{{"drive", 14}, {"fold", 40}, {"crush", 90}, {"target", 0}, {"tone", 15000}}}, {"compressor", P{{"mode", 1}, {"threshold", -24}, {"ratio", 8}, {"attack", 1}}}});
    add("cd.blown_speaker", "Blown Radio", G, "vocals, guitars, drums", "A transistor radio pushed into a torn speaker: boxy band, fizz and rattle.",
        {{"degrade", P{{"rate", 11000}, {"bits", 9}, {"bandwidth", 85}, {"drive", 16}, {"noise", 15}}}, {"amp", P{{"gain", 26}, {"cab", 1}, {"mid", 6}, {"out", -14}}},
         {"saturation", P{{"style", 3}, {"drive", 12}}}});
    add("g3.bitrot_echo", "Bitrot Echo", G, "vocals, synths, guitars, drums", "Every repeat comes back at a lower sample rate and bit depth, rotting a little more each time.",
        {{"delay", P{{"sync", 1}, {"division", 8}, {"stereo", 2}, {"feedback", 72}, {"high_cut", 9000}, {"mix", 55}, {"lofi_rate", 5000}, {"lofi_bits", 8}}},
         {"bitcrush", P{{"bits", 10}, {"rate", 22050}, {"mix", 60}}}});

    // ================================================================== Stretch & Smear
    add("ss.glacier", "Glacier", S, "pads, vocals, keys, field", "Frozen into a slowly melting spectrum: seconds of spectral memory, smeared across pitch.",
        {{"spectral_smear", P{{"smear", 85}, {"time", 92}, {"scatter", 65}, {"out", 4}, {"mix", 100}}}, {"reverb", P{{"mode", 4}, {"decay", 10}, {"mix", 40}}}});
    add("ss.half_time_haze", "Molasses", S, "drums, vocals, loops, keys", "Half-time playback with the pitch kept, then smeared: everything drags and blurs.",
        {{"time_warp", P{{"mode", 2}, {"division", 13}, {"keep_pitch", 1}, {"mix", 100}}}, {"spectral_smear", P{{"smear", 35}, {"time", 55}, {"scatter", 20}, {"mix", 100}}}});
    add("ss.tape_slow", "Taffy Pull", S, "vocals, guitars, keys, pads", "A granular quarter-speed stretch that pulls phrases out like taffy, pitch intact.",
        {{"grain_engine", P{{"mode", 1}, {"speed", 0.25f}, {"size_ms", 160}, {"density", 30}, {"spray_ms", 40}, {"align", 1}, {"gain", -3}, {"mix", 100}}},
         {"spectral_smear", P{{"smear", 25}, {"time", 30}, {"scatter", 10}, {"mix", 70}}}});
    add("ss.smear_field", "Smear Field", S, "field, pads, vocals, full mix", "No pitch, no attack: a dense cloud of the source's colours with the phase scattered away.",
        {{"spectral_smear", P{{"smear", 95}, {"bias", -40}, {"time", 40}, {"scatter", 90}, {"out", 11}, {"mix", 100}}}, {"width", P{{"width", 140}}}});
    add("ss.quarter_drift", "Varispeed Sink", S, "vocals, drums, guitars, loops", "Tape playback at 0.6x: everything sinks a fifth and a half and slows, with wow.",
        {{"time_warp", P{{"mode", 0}, {"speed", 0.6f}, {"mix", 100}}}, {"tape_colour", P{{"drive", 8}, {"hf_loss", 5}}}, {"vibrato", P{{"depth", 18}, {"rate", 0.5f}}}});
    add("s3.frozen_bloom", "Frozen Bloom", S, "pads, keys, vocals, guitars", "A spectral hold blooms out of the source and hangs, with grains glittering through it.",
        {{"spectral", P{{"freeze", 1}, {"fade_ms", 1500}, {"blur", 70}, {"smear", 30}, {"shimmer", 40}, {"mix", 100}}},
         {"grain_engine", P{{"mode", 0}, {"density", 14}, {"pitch_jit", 12}, {"scale", 1}, {"spray_ms", 900}, {"mix", 45}}}});

    // ================================================================== Pitch Mutation
    add("gm.octave_dust", "Octave Fracture", PM, "vocals, keys, guitars, synths", "Every grain lands on a different octave, two below to two above: the melody shatters across the register.",
        {{"grain_engine", P{{"mode", 0}, {"size_ms", 90}, {"density", 45}, {"pitch_jit", 24}, {"scale", 1}, {"spray_ms", 200}, {"spread", 90}, {"mix", 100}}},
         {"reverb", P{{"mode", 1}, {"decay", 3.5f}, {"mix", 25}}}});
    add("vt.giant", "Sub Mutant", PM, "vocals, drums, synths, bass", "Down two octaves with darker formants, folded into a growling mass.",
        {{"pitch_shift", P{{"shift_a", -24}, {"shift_b", -12}, {"level_b", -6}, {"preserve", 0}, {"mix", 100}}}, {"fold_crush", P{{"drive", 10}, {"fold", 30}, {"crush", 0}, {"tone", 5000}}}});
    add("p3.reverse_fifths", "Reverse Fifths", PM, "keys, guitars, pads, vocals", "Backwards grains stacked in fifths and octaves: an inside-out chord of the source.",
        {{"grain_engine", P{{"mode", 0}, {"size_ms", 240}, {"density", 22}, {"pitch_jit", 19}, {"scale", 2}, {"reverse", 85}, {"spray_ms", 500}, {"mix", 100}}},
         {"reverb", P{{"mode", 6}, {"decay", 3}, {"mix", 30}}}});
    add("gm.minor_scatter", "Minor Swarm", PM, "keys, synths, guitars, vocals", "A swarm of short grains snapped to a minor scale over three octaves.",
        {{"grain_engine", P{{"size_ms", 45}, {"density", 90}, {"pitch_jit", 14}, {"scale", 4}, {"spray_ms", 300}, {"stereo", 1}, {"mix", 100}}},
         {"chorus", P{{"depth", 30}, {"mix", 30}}}});
    add("p3.ring_mutant", "Ring Mutant", PM, "vocals, drums, synths", "A drifting ring modulator into an octave-up shift: pitch becomes inharmonic metal.",
        {{"ring_mod", P{{"freq", 310}, {"mod_depth", 60}, {"mod_rate", 0.3f}, {"mix", 85}}}, {"pitch_shift", P{{"shift_a", 12}, {"shift_b", 7}, {"level_b", -8}, {"mix", 100}}}});
    add("p3.glide_bend", "Tape Bend", PM, "vocals, guitars, keys, loops", "The playhead bends up and down behind the input: a seasick, pitch-swinging scrub.",
        {{"time_warp", P{{"mode", 3}, {"scrub_rate", 0.4f}, {"scrub_depth", 260}, {"mix", 100}}}, {"tape_colour", P{{"drive", 6}}}});

    // ================================================================== Rhythmic Damage
    add("gr.slice_retrigger", "Scatter Grid", R, "drums, loops, vocals, full mix", "Sixteenth steps become micro-loops, reversed pieces and jumps back in time: a different break every bar, the same every playback.",
        {{"glitch", P{{"division", 4}, {"repeat", 40}, {"ratchet", 4}, {"reverse", 20}, {"jump", 25}, {"pitch", 10}, {"gate", 10}, {"lofi", 30}, {"mix", 100}}},
         {"compressor", P{{"mode", 1}, {"threshold", -20}, {"ratio", 4}}}});
    add("r3.tape_stop_bars", "Brake Every Two", R, "drums, loops, full mix, bass", "The tape winds down to a stop at the end of every second bar and snaps back on the one.",
        {{"time_warp", P{{"mode", 1}, {"trigger", 3}, {"division", 13}, {"stop_ms", 900}, {"curve", 40}, {"mix", 100}}}, {"saturation", P{{"style", 1}, {"drive", 6}}}});
    add("gr.backspin", "Backspin Bars", R, "drums, vocals, loops", "A fast reverse spin-out at the end of every bar.",
        {{"time_warp", P{{"mode", 4}, {"trigger", 2}, {"division", 10}, {"stop_ms", 400}, {"mix", 100}}}, {"filter", P{{"type", 2}, {"cutoff", 80}}}});
    add("gr.buffer_roll", "Ratchet Rolls", R, "drums, loops, percussion", "Eighth-note steps that roll into eight-fold ratchets, crushed on the glitched steps.",
        {{"glitch", P{{"division", 7}, {"repeat", 70}, {"ratchet", 8}, {"reverse", 5}, {"jump", 5}, {"pitch", 5}, {"gate", 5}, {"lofi", 60}, {"seed", 21}, {"mix", 100}}},
         {"delay", P{{"sync", 1}, {"division", 4}, {"feedback", 20}, {"mix", 15}}}});
    add("gr.halftime_chop", "Halftime Chop", R, "drums, loops, vocals", "Quarter-note slices played at half speed (an octave down), gated on the eighths.",
        {{"time_warp", P{{"mode", 2}, {"division", 10}, {"mix", 100}}}, {"gate", P{{"mode", 2}, {"division", 7}, {"curve", 70}}}});
    add("rhy.stutter_bloom", "Stutter Bloom", R, "vocals, drums, synths", "Bar-wise stutters that step down in pitch and bloom into a reversed space.",
        {{"stutter", P{{"interval", 2}, {"gate", 4}, {"slice", 2}, {"chance", 80}, {"pitch", -2}, {"decay", 20}}},
         {"glitch", P{{"division", 4}, {"repeat", 15}, {"reverse", 25}, {"jump", 10}, {"gate", 15}, {"mix", 100}}}, {"reverb", P{{"mode", 6}, {"decay", 2.5f}, {"mix", 30}}}});

    // ================================================================== Granular Worlds
    add("gm.particle_drift", "Particle Drift", GW, "vocals, keys, pads, guitars", "Long grains drifting seconds behind the source, reversed and spread wide: the phrase dissolves into motion.",
        {{"grain_engine", P{{"mode", 0}, {"size_ms", 420}, {"density", 12}, {"position_ms", 1500}, {"spray_ms", 1500}, {"reverse", 45}, {"pitch_jit", 3}, {"spread", 100}, {"stereo", 1}, {"mix", 100}}},
         {"reverb", P{{"mode", 1}, {"decay", 4}, {"mix", 30}}}});
    add("gm.swarm", "Insect Choir", GW, "vocals, field, synths, drums", "Thousands of 15 ms grains, pitch-scattered and panned: the source becomes a buzzing, living swarm.",
        {{"grain_engine", P{{"size_ms", 15}, {"density", 200}, {"voices", 64}, {"pitch_jit", 9}, {"spray_ms", 800}, {"spread", 100}, {"stereo", 1}, {"window", 1}, {"gain", -6}, {"mix", 100}}},
         {"filter", P{{"type", 2}, {"cutoff", 300}}}});
    add("frg.frozen_breath", "Frozen Breath", GW, "vocals, pads, keys", "The moment is frozen; a slow playhead scans it endlessly while grains bloom over it.",
        {{"grain_engine", P{{"mode", 1}, {"freeze", 1}, {"speed", 0.08f}, {"size_ms", 300}, {"density", 20}, {"spray_ms", 200}, {"pitch_jit", 2}, {"mix", 100}}},
         {"spectral_smear", P{{"smear", 30}, {"time", 40}, {"scatter", 20}, {"mix", 60}}}});
    add("gm.cascade", "Grain Cascade", GW, "keys, guitars, vocals, synths", "Grains fed back into themselves through a darkening, lo-fi loop: an evolving, regenerating cascade.",
        {{"grain_engine", P{{"size_ms", 180}, {"density", 25}, {"spray_ms", 400}, {"pitch", 7}, {"feedback", 70}, {"delay_ms", 380}, {"fb_lp", 5000}, {"fb_rate", 14000}, {"fb_drive", 6}, {"mix", 100}}},
         {"compressor", P{{"threshold", -24}, {"ratio", 3}}}});
    add("frg.loop_ghost", "Loop Ghost", GW, "vocals, drums, loops, keys", "Holds a second of audio and cycles it backwards at half speed: a ghost of the phrase.",
        {{"grain_engine", P{{"mode", 2}, {"loop_ms", 1000}, {"speed", -0.5f}, {"size_ms", 140}, {"density", 30}, {"spray_ms", 20}, {"mix", 100}}},
         {"reverb", P{{"mode", 3}, {"decay", 2}, {"mix", 35}}}});
    add("gm.wide_grains", "Stereo Rain", GW, "drums, percussion, field, keys", "Independent left and right grain streams of short plucked grains: rain across the stereo field.",
        {{"grain_engine", P{{"size_ms", 35}, {"density", 70}, {"window", 3}, {"stereo", 1}, {"spread", 100}, {"spray_ms", 1200}, {"reverse", 30}, {"mix", 100}}},
         {"delay", P{{"sync", 1}, {"division", 6}, {"stereo", 2}, {"feedback", 35}, {"mix", 25}}}});

    // ================================================================== Spectral Objects
    add("so.rising_echo", "Rising Echoes", SO, "drums, vocals, keys, synths", "Each frequency band echoes later than the one below: hits smear upwards into sweeping spectral repeats.",
        {{"spectral_delay", P{{"sync", 1}, {"division", 10}, {"shape", 0}, {"spread", 100}, {"feedback", 55}, {"damping", 30}, {"mix", 100}}},
         {"reverb", P{{"mode", 2}, {"decay", 2}, {"mix", 20}}}});
    add("so.scatter_prism", "Prism Scatter", SO, "vocals, drums, field, synths", "Bands return in a fixed scattered order with per-band feedback: the source refracts into spectral shards.",
        {{"spectral_delay", P{{"sync", 0}, {"time_ms", 900}, {"shape", 4}, {"spread", 100}, {"feedback", 65}, {"offset", 70}, {"mix", 100}}},
         {"reverb", P{{"mode", 2}, {"decay", 1.8f}, {"mix", 25}}}});
    add("so.partial_gate", "Partial Gate", SO, "vocals, keys, pads, guitars", "Only the strongest partials survive the spectral gate; the rest is silence. Sparse, glassy and strange.",
        {{"spectral_smear", P{{"smear", 10}, {"time", 25}, {"scatter", 0}, {"gate", -48}, {"out", 6}, {"mix", 100}}}, {"reverb", P{{"mode", 2}, {"decay", 3}, {"mix", 30}}}});
    add("so.disintegrate", "Disintegrate", SO, "vocals, pads, keys, field", "Partials drop out and return at random: the sound tears and sparkles apart.",
        {{"spectral_smear", P{{"smear", 20}, {"time", 20}, {"scatter", 30}, {"disintegrate", 90}, {"out", 10}, {"mix", 100}}}, {"spectral_delay", P{{"sync", 1}, {"division", 7}, {"shape", 3}, {"feedback", 30}, {"mix", 45}}}});
    add("ht.fifth_resonance", "Fifth Resonance", SO, "drums, percussion, vocals, field", "Everything excites a ringing bank of fifths: drums become tuned metal.",
        {{"resonator", P{{"key", 9}, {"octave", 2}, {"chord", 2}, {"decay", 4}, {"bright", 60}, {"mix", 100}}}, {"spectral_smear", P{{"smear", 15}, {"time", 30}, {"mix", 50}}}});
    add("ht.crystal", "Glass Chord", SO, "keys, vocals, guitars, pads", "A held spectrum of the source, shimmering in a pentatonic resonator.",
        {{"spectral", P{{"freeze", 1}, {"fade_ms", 600}, {"blur", 50}, {"shimmer", 60}, {"mix", 100}}}, {"resonator", P{{"chord", 6}, {"decay", 6}, {"bright", 70}, {"mix", 70}}}});

    // ================================================================== Feedback Machines
    add("ex.fractal_feedback", "Fractal Feedback", FM, "vocals, synths, keys, field", "Grains regenerate through a pitched, saturated feedback path: a self-similar, ever-growing structure.",
        {{"grain_engine", P{{"size_ms", 120}, {"density", 35}, {"pitch", 12}, {"pitch_jit", 7}, {"scale", 2}, {"feedback", 85}, {"delay_ms", 250}, {"fb_hp", 200}, {"fb_lp", 7000}, {"fb_drive", 9}, {"mix", 100}}},
         {"compressor", P{{"threshold", -26}, {"ratio", 4}}}});
    add("mot.spiral_stairs", "Endless Stair", FM, "synths, pads, vocals, guitars", "A frequency-shifted feedback loop that climbs forever through ten-stage phasing.",
        {{"freq_shift", P{{"shift", 14}, {"feedback", 82}, {"delay_ms", 160}, {"mix", 100}}}, {"phaser", P{{"stages", 4}, {"rate", 0.12f}, {"feedback", 60}, {"range", 4}}}});
    add("f3.dub_siren", "Dub Machine", FM, "vocals, drums, synths", "Saturated tape echoes at the edge of runaway, darkened and degraded on every pass.",
        {{"tape_echo", P{{"sync", 1}, {"division", 8}, {"heads", 3}, {"intensity", 100}, {"saturation", 70}, {"age", 70}, {"mix", 55}}},
         {"degrade", P{{"rate", 18000}, {"bits", 11}, {"bandwidth", 35}, {"noise", 10}, {"out", -7}, {"mix", 70}}}});
    add("f3.reverse_loop", "Mirror Loop", FM, "vocals, guitars, keys", "Reversed chunks fed back to reverse again, pitched down each time.",
        {{"reverse", P{{"mode", 1}, {"sync", 1}, {"division", 10}, {"feedback", 75}, {"pitch", -5}, {"mix", 100}}}, {"reverb", P{{"mode", 1}, {"decay", 3}, {"mix", 25}}}});
    add("ex.ring_cascade", "Metal Cascade", FM, "drums, synths, vocals", "Ring modulation into a resonant flanger feedback loop: bells, sirens, machinery.",
        {{"ring_mod", P{{"freq", 440}, {"wave", 1}, {"mod_depth", 40}, {"mix", 90}}}, {"flanger", P{{"feedback", 90}, {"rate", 0.08f}, {"depth", 5}}},
         {"delay", P{{"sync", 1}, {"division", 6}, {"feedback", 60}, {"mix", 35}, {"lofi_rate", 9000}}}});
    add("f3.spectral_regen", "Spectral Regen", FM, "pads, vocals, field, keys", "Spectral echoes at high per-band feedback, smeared on the way back: a self-feeding spectral tide.",
        {{"spectral_delay", P{{"sync", 0}, {"time_ms", 650}, {"shape", 2}, {"spread", 70}, {"feedback", 85}, {"damping", 50}, {"mix", 100}}},
         {"spectral_smear", P{{"smear", 40}, {"time", 50}, {"scatter", 40}, {"mix", 70}}}});

    // ================================================================== Vocal Transformations
    add("vt.whisper_cloud", "Whisper Cloud", VT, "vocals", "The voice is reduced to breath: phase-scattered, smeared and scattered into short grains.",
        {{"spectral_smear", P{{"smear", 55}, {"time", 30}, {"scatter", 100}, {"mix", 100}}}, {"grain_engine", P{{"size_ms", 60}, {"density", 60}, {"spray_ms", 600}, {"spread", 100}, {"mix", 100}}}});
    add("sig.machine_choir", "Machine Choir", VT, "vocals, synths", "The voice drives a 24-band vocoder chord, chorused into a hall.",
        {{"vocoder", P{{"bands", 3}, {"carrier", 0}, {"note", 45}, {"use_midi", 0}, {"mix", 100}}}, {"chorus", P{{"voices", 3}, {"depth", 50}, {"mix", 50}}},
         {"reverb", P{{"mode", 1}, {"decay", 3}, {"mix", 25}}}});
    add("vt.octave_halo", "Synthetic Double", VT, "vocals", "An octave-up double with shifted formants and a robotic in-key correction.",
        {{"pitch_correct", P{{"key", 9}, {"scale", 1}, {"speed", 0}, {"amount", 100}}}, {"harmony", P{{"mode", 0}, {"voices", 1}, {"int1", 12}, {"int2", -12}, {"lvl1", -2}, {"lvl2", -6}, {"dry", -60}}},
         {"formant", P{{"shift", 5}, {"mix", 100}}}});
    add("vt.ring_robot", "Circuit Voice", VT, "vocals", "A broken robot: ring modulated, crushed to 9 bits and squeezed through a narrow band.",
        {{"ring_mod", P{{"freq", 95}, {"wave", 2}, {"mix", 70}}}, {"degrade", P{{"rate", 9000}, {"bits", 9}, {"bandwidth", 60}, {"drive", 10}, {"noise", 5}}}});
    add("voc.octave_ghost", "Formant Ghost", VT, "vocals", "Down an octave with the formants pushed up: a different, uncanny speaker, trailed by reversed echoes.",
        {{"pitch_shift", P{{"shift_a", -12}, {"preserve", 0}, {"mix", 100}}}, {"formant", P{{"shift", 6}, {"mix", 100}}}, {"reverse", P{{"mode", 1}, {"mix", 40}}}});
    add("v3.vowel_chorus", "Vowel Swarm", VT, "vocals", "A choir of grains in a major scale, each on a different vowel colour of the same voice.",
        {{"harmony", P{{"mode", 1}, {"voices", 3}, {"detune", 20}, {"dry", -12}}}, {"grain_engine", P{{"size_ms", 110}, {"density", 40}, {"pitch_jit", 7}, {"scale", 3}, {"spray_ms", 300}, {"mix", 100}}}});

    // ================================================================== Cinematic / Spatial
    add("atm.glass_cathedral", "Glass Cathedral", CS, "vocals, keys, pads, guitars", "Octave and twelfth shimmer rising out of a twelve-second tail; the source becomes the space.",
        {{"shimmer", P{{"decay", 14}, {"size", 85}, {"shift1", 12}, {"shift2", 19}, {"regen", 70}, {"routing", 0}, {"mix", 85}}}, {"width", P{{"width", 100}, {"mono_below", 250}, {"out", -8}}}});
    add("atm.slow_bloom", "Reverse Bloom", CS, "vocals, keys, guitars, drums", "Every phrase swells in backwards, out of a reversed cathedral.",
        {{"reverse", P{{"mode", 0}, {"sync", 1}, {"division", 13}, {"mix", 100}}}, {"reverb", P{{"mode", 6}, {"decay", 6}, {"size", 90}, {"mix", 60}}}});
    add("atm.deep_hollow", "Deep Hollow", CS, "drums, vocals, field, bass", "Pitched down an octave into a dark, immense cavern: impacts become distant thunder.",
        {{"pitch_shift", P{{"shift_a", -12}, {"mix", 100}}}, {"reverb", P{{"mode", 4}, {"decay", 18}, {"size", 100}, {"damping", 75}, {"mix", 70}}}, {"eq", P{{"lp", 4000}, {"ls_gain", 3}}}});
    add("sig.doppler_drift", "Fly-By", CS, "vocals, synths, percussion, field", "The source circles the listener at speed, Doppler-bent and wide, in an open space.",
        {{"doppler", P{{"path", 2}, {"speed", 60}, {"distance", 3}, {"period", 3}}}, {"reverb", P{{"mode", 1}, {"decay", 4}, {"mix", 35}}}});
    add("atm.under_ice", "Under Ice", CS, "vocals, drums, pads, field", "Submerged: low-passed, smeared and bloomed in a ghostly convolution space.",
        {{"filter", P{{"type", 1}, {"cutoff", 900}, {"resonance", 25}}}, {"spectral_smear", P{{"smear", 50}, {"time", 60}, {"scatter", 30}, {"mix", 100}}},
         {"convolution", P{{"ir", 5}, {"mix", 60}}}});
    add("c3.horizon_freeze", "Horizon Freeze", CS, "pads, keys, vocals, field", "The reverb tail is frozen into an endless horizon behind a scanning grain cloud.",
        {{"grain_engine", P{{"mode", 1}, {"speed", 0.15f}, {"size_ms", 500}, {"density", 10}, {"spray_ms", 300}, {"mix", 100}}},
         {"reverb", P{{"mode", 4}, {"decay", 30}, {"freeze", 1}, {"mix", 55}}}});

    // ================================================================== Hybrid / Experimental
    add("ex.inverted_world", "Inverted World", HX, "vocals, drums, keys, full mix", "Reversed, octave-shifted grains through spectral delay and a crushed, folded output.",
        {{"grain_engine", P{{"size_ms", 200}, {"density", 30}, {"reverse", 100}, {"pitch", -12}, {"pitch_jit", 12}, {"scale", 1}, {"mix", 100}}},
         {"spectral_delay", P{{"sync", 1}, {"division", 7}, {"shape", 1}, {"feedback", 40}, {"mix", 60}}}, {"fold_crush", P{{"drive", 8}, {"fold", 40}, {"crush", 40}, {"mix", 60}}}});
    add("h3.machine_dream", "Machine Dream", HX, "drums, loops, synths, vocals", "Glitched steps degraded to 6 kHz, then frozen and smeared between the hits.",
        {{"glitch", P{{"division", 4}, {"repeat", 35}, {"reverse", 25}, {"jump", 30}, {"pitch", 15}, {"lofi", 70}, {"seed", 42}, {"mix", 100}}},
         {"spectral_smear", P{{"smear", 40}, {"time", 70}, {"scatter", 30}, {"mix", 60}}}});
    add("h3.broken_tape_choir", "Broken Tape Choir", HX, "vocals, keys, guitars", "Harmonised into a choir, then stopped and started like a dying tape machine every bar.",
        {{"harmony", P{{"mode", 1}, {"voices", 2}, {"detune", 15}, {"dry", -6}}}, {"time_warp", P{{"mode", 1}, {"trigger", 2}, {"division", 10}, {"stop_ms", 500}, {"mix", 100}}},
         {"degrade", P{{"rate", 20000}, {"bits", 12}, {"bandwidth", 40}, {"wobble", 40}, {"noise", 12}}}});
    add("h3.particle_engine", "Particle Accelerator", HX, "drums, synths, field", "Tiny grains at 200 per second, ring modulated and spectrally delayed into a rising, crackling stream.",
        {{"grain_engine", P{{"size_ms", 8}, {"density", 200}, {"voices", 64}, {"pitch_jit", 24}, {"spray_ms", 2000}, {"stereo", 1}, {"mix", 100}}},
         {"ring_mod", P{{"freq", 1200}, {"mod_depth", 80}, {"mix", 50}}}, {"spectral_delay", P{{"sync", 1}, {"division", 4}, {"shape", 0}, {"feedback", 50}, {"mix", 60}}}});
    add("h3.rubber_room", "Rubber Room", HX, "vocals, drums, loops", "Scrubbed back and forth, folded, then flung around a spectral delay.",
        {{"time_warp", P{{"mode", 3}, {"scrub_rate", 2.2f}, {"scrub_depth", 90}, {"mix", 100}}}, {"fold_crush", P{{"drive", 12}, {"fold", 70}, {"crush", 20}}},
         {"spectral_delay", P{{"sync", 1}, {"division", 7}, {"shape", 2}, {"feedback", 35}, {"mix", 40}}}});
    add("h3.tidal_memory", "Tidal Memory", HX, "pads, field, vocals, keys", "A slow tide of memory: the last eight seconds drift back in, pitched in octaves, degraded and blurred.",
        {{"grain_engine", P{{"size_ms", 800}, {"density", 4}, {"position_ms", 6000}, {"spray_ms", 2000}, {"pitch_jit", 12}, {"scale", 1}, {"window", 4}, {"mix", 100}}},
         {"degrade", P{{"rate", 15000}, {"bits", 10}, {"bandwidth", 25}, {"wobble", 30}, {"noise", 8}}}, {"reverb", P{{"mode", 4}, {"decay", 9}, {"mix", 40}}}});

    // V4: remap the V3 bank onto the V4 categories and give every preset an intensity; then the V4 and V5 additions
    for (auto& p : v) {
        p.category = v4CategoryFor(p.category);
        if (p.intensity.empty()) p.intensity = v4DefaultIntensity(p.category, p.id);
    }
    addV4Presets(v);
    addV5Presets(v);
    addV6Presets(v);
    // V5 Phase 8: no generic, topology-derived macros any more. A preset keeps only the macros its author wrote;
        // every other preset gets the macros of its effect COMBINATION at load time (Session::refreshMacros), or none.
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
    if (p.chain.empty() && p.id != kInitPresetId) return fail("empty chain");
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
    if (p.macros.size() > 4) return fail("more than 4 macros");
    for (const auto& m : p.macros) {
        if (m.name.empty() || m.targets.empty()) return fail("macro without name or targets");
        for (const auto& t : m.targets) {
            if (t.slot < 0 || t.slot >= static_cast<int>(p.chain.size())) return fail("macro " + m.name + ": slot out of range");
            const auto& d = fxRegistry()[static_cast<size_t>(fxTypeIndex(p.chain[static_cast<size_t>(t.slot)].type))];
            const int k = d.paramIndex(t.param);
            if (k < 0) return fail("macro " + m.name + ": unknown parameter " + t.param);
            if (d.params[static_cast<size_t>(k)].kind != ParamKind::Float) return fail("macro " + m.name + ": " + t.param + " is not continuous");
            if (!std::isfinite(t.depth) || t.depth == 0.0f) return fail("macro " + m.name + ": zero depth");
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
    if (!p.intensity.empty()) meta.set("intensity", p.intensity);
    if (!p.styles.empty()) meta.set("styles", p.styles);
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
    if (!p.macros.empty()) {
        json::Value ms = json::Value::array();
        for (const auto& m : p.macros) {
            json::Value mo = json::Value::object();
            mo.set("name", m.name);
            json::Value ts = json::Value::array();
            for (const auto& t : m.targets) {
                json::Value to = json::Value::object();
                to.set("slot", t.slot);
                to.set("param", t.param);
                to.set("depth", static_cast<double>(t.depth));
                ts.push(std::move(to));
            }
            mo.set("targets", std::move(ts));
            ms.push(std::move(mo));
        }
        root.set("macros", std::move(ms));
    }
    return root.dump(1);
}

}  // namespace ih1
