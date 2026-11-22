// V5 factory presets (feedback round 5, Phase 4): atmosphere, drums / drum bus, and tremolo / groove. Original IH1
// recipes; genre names are inspiration only and no commercial preset was copied or imitated value-by-value.
// Macros: none are authored here. Each preset's macros come from its effect combination (Session::refreshMacros,
// ComboMacros.cpp) or it has none, so every macro knob that appears is the combination's own, tested gesture.
#include "ih1/presets/Presets.h"

namespace ih1 {
namespace {
using PV = std::vector<std::pair<std::string, float>>;
const char* AC = "Atmosphere / Cinematic / Drone";
const char* DR = "Drums / Drum Bus";
const char* TG = "Tremolo / Groove";
const char* MB = "Musical / Beautiful";
const char* EX = "Experimental / Hybrid";
}  // namespace

void addV5Presets(std::vector<FactoryPreset>& v) {
    auto add = [&](const char* id, const char* name, const char* cat, const char* tags, const char* intensity, const char* styles, const char* desc,
                   std::vector<PresetFx> chain) {
        FactoryPreset p{id, name, cat, desc, 5, std::move(chain), tags};
        p.intensity = intensity;
        p.styles = styles;
        v.push_back(std::move(p));
    };

    // ==================================================================================== Atmosphere (22)
    add("v5.soft_horizon", "Soft Horizon", AC, "pads, keys, vocals, guitars", "Moderate", "ambient, post-rock",
        "A warm, diffuse space with a long low end and soft highs that drifts very slowly.",
        {{"cloud_chamber", PV{{"size", 60}, {"decay", 5}, {"low_decay", 1.4f}, {"high_decay", 0.45f}, {"density", 80}, {"bloom", 35}, {"drift", 40}, {"predelay", 25}, {"mix", 35}}},
         {"width", PV{{"width", 115}, {"mono_below", 140}}}});
    add("v5.pale_chamber", "Pale Chamber", AC, "vocals, acoustic, keys", "Subtle", "folk, singer-songwriter",
        "An intimate room built from close early reflections and only a short, pale tail: depth without wash.",
        {{"reflections", PV{{"pattern", 0}, {"time", 35}, {"taps", 14}, {"spread", 60}, {"decay", 55}, {"tone", 6500}, {"diffusion", 45}, {"mix", 35}}},
         {"cloud_chamber", PV{{"size", 25}, {"decay", 0.9f}, {"high_decay", 0.5f}, {"density", 90}, {"mix", 18}}}});
    add("v5.velvet_drift", "Velvet Drift", AC, "pads, synths, guitars", "Moderate", "dark ambient, drone",
        "A dark, slowly moving late tail: highs fade early, the low mids hang and breathe.",
        {{"chorus", PV{{"voices", 2}, {"rate", 0.18f}, {"depth", 45}, {"mix", 35}}},
         {"cloud_chamber", PV{{"size", 80}, {"decay", 12}, {"low_decay", 1.3f}, {"high_decay", 0.2f}, {"density", 85}, {"bloom", 50}, {"drift", 65}, {"mix", 45}}}});
    add("v5.glass_bloom", "Glass Bloom", AC, "keys, vocals, plucks, pads", "Strong", "ambient, cinematic",
        "Bright pitch feedback blooms over a space whose high end is softened so the shimmer never turns harsh.",
        {{"shimmer", PV{{"decay", 9}, {"size", 70}, {"shift1", 12}, {"shift2", 7}, {"balance", 35}, {"regen", 70}, {"tone", 6000}, {"low_cut", 350}, {"mix", 40}}},
         {"cloud_chamber", PV{{"decay", 4}, {"high_decay", 0.35f}, {"bloom", 40}, {"mix", 25}}}});
    add("v5.frozen_orchard", "Frozen Orchard", AC, "pads, vocals, strings", "Strong", "drone, modern classical",
        "Spectral sustain that slowly smears in frequency, set into a wide, dark chamber.",
        {{"spectral_smear", PV{{"smear", 70}, {"time", 75}, {"scatter", 45}, {"mix", 70}}},
         {"cloud_chamber", PV{{"size", 75}, {"decay", 10}, {"high_decay", 0.3f}, {"drift", 50}, {"mix", 40}}}});
    add("v5.reverse_weather", "Reverse Weather", AC, "vocals, guitars, keys, drums", "Strong", "shoegaze, trip-hop",
        "Every phrase is preceded by its own reversed, inhaled reverb tail: weather rolling in backwards.",
        {{"reverse_bloom", PV{{"sync", 1}, {"division", 13}, {"tail", 4}, {"tone", 5000}, {"density", 70}, {"width", 110}, {"mix", 55}}},
         {"width", PV{{"width", 110}, {"mono_below", 150}}}});
    add("v5.deep_tide", "Deep Tide", AC, "bass, pads, drones", "Moderate", "dub techno, deep ambient",
        "A long, low-passed decay where the lows outlast everything: an undertow under the sound.",
        {{"eq", PV{{"hp", 30}, {"lp", 7000}}},
         {"cloud_chamber", PV{{"size", 85}, {"decay", 9}, {"low_decay", 1.8f}, {"high_decay", 0.15f}, {"density", 75}, {"bloom", 60}, {"low_cut", 30}, {"mix", 45}}}});
    add("v5.violet_relay", "Violet Relay", AC, "synths, vocals, guitars, keys", "Moderate", "ambient dub, downtempo",
        "Dotted tempo echoes hand each phrase over to a spacious, modulated tail.",
        {{"delay", PV{{"sync", 1}, {"division", 8}, {"stereo", 2}, {"feedback", 45}, {"mod_depth", 30}, {"high_cut", 5500}, {"mix", 30}}},
         {"cloud_chamber", PV{{"size", 65}, {"decay", 6}, {"high_decay", 0.4f}, {"drift", 45}, {"mix", 35}}}});
    add("v5.cloud_memory", "Cloud Memory", AC, "vocals, keys, field, guitars", "Strong", "ambient, lo-fi ambient",
        "Long, overlapping grains with a little reverse randomness, dissolved into a soft cloud.",
        {{"granular", PV{{"size_ms", 260}, {"density", 22}, {"position_ms", 400}, {"spray_ms", 900}, {"scatter", 3}, {"reverse", 25}, {"spread", 80}, {"diffusion", 60}, {"feedback", 25}, {"mix", 65}}},
         {"reverb", PV{{"mode", 1}, {"decay", 5}, {"mix", 35}}}});
    add("v5.small_room_huge_sky", "Small Room / Huge Sky", AC, "vocals, drums, guitars", "Strong", "cinematic, post-rock",
        "Tight, close reflections keep the source present while a very wide, long field opens behind it.",
        {{"reflections", PV{{"pattern", 1}, {"time", 22}, {"taps", 10}, {"spread", 50}, {"mix", 40}}},
         {"cloud_chamber", PV{{"size", 100}, {"decay", 14}, {"high_decay", 0.5f}, {"predelay", 80}, {"width", 150}, {"mix", 40}}}});
    add("v5.suspended_bells", "Suspended Bells", AC, "plucks, keys, drums, percussion", "Strong", "ambient, game audio",
        "Every sound strikes tuned bell modes that ring inside a controlled, dark ambience.",
        {{"modal", PV{{"material", 2}, {"note", 2}, {"octave", 3}, {"modes", 16}, {"decay", 3.5f}, {"bright", 55}, {"mix", 40}}},
         {"cloud_chamber", PV{{"size", 70}, {"decay", 6}, {"high_decay", 0.35f}, {"mix", 30}}}});
    add("v5.afterimage_choir", "Afterimage Choir", AC, "vocals, pads, strings", "Strong", "cinematic, choral ambient",
        "Octave and fifth layers of the source are diffused into a choir-like afterimage.",
        {{"pitch_shift", PV{{"shift_a", 12}, {"shift_b", 7}, {"level_b", -6}, {"width", 70}, {"mix", 35}}},
         {"shimmer", PV{{"decay", 8}, {"regen", 45}, {"tone", 5500}, {"mix", 40}}}});
    add("v5.dust_cathedral", "Dust Cathedral", AC, "pads, keys, drones, field", "Strong", "dark ambient, lo-fi",
        "A dark noise-and-dust texture follows the source into a long cathedral space.",
        {{"texture", PV{{"type", 0}, {"amount", -16}, {"density", 55}, {"band", 2500}, {"follow", 90}, {"mix", 100}}},
         {"reverb", PV{{"mode", 4}, {"decay", 9}, {"damping", 65}, {"high_cut", 5000}, {"mix", 45}}}});
    add("v5.slow_glass", "Slow Glass", AC, "pads, keys, strings", "Moderate", "drone, minimalism",
        "Spectral blur with a stable tonal core and a glassy high decay that lingers.",
        {{"spectral_smear", PV{{"smear", 45}, {"time", 55}, {"scatter", 20}, {"mix", 55}}},
         {"cloud_chamber", PV{{"decay", 5}, {"high_decay", 1.2f}, {"low_decay", 0.6f}, {"mix", 30}}}});
    add("v5.blue_hour", "Blue Hour", MB, "vocals, keys, guitars, full mix", "Subtle", "neo-soul, ambient pop",
        "Clean, immersive stereo movement: gentle reflections that drift, and a short soft tail.",
        {{"reflections", PV{{"pattern", 0}, {"time", 45}, {"taps", 16}, {"motion", 45}, {"diffusion", 50}, {"mix", 30}}},
         {"chorus", PV{{"voices", 1}, {"rate", 0.25f}, {"depth", 20}, {"mix", 20}}}});
    add("v5.rain_in_reverse", "Rain in Reverse", AC, "drums, percussion, plucks", "Strong", "IDM, ambient",
        "Short reversed fragments rain down before a diffuse tail.",
        {{"reverse", PV{{"mode", 2}, {"grain_ms", 90}, {"density", 30}, {"spread", 70}, {"mix", 55}}},
         {"cloud_chamber", PV{{"decay", 4}, {"density", 60}, {"mix", 30}}}});
    add("v5.distant_machinery", "Distant Machinery", EX, "drums, field, synths", "Strong", "industrial, dark ambient",
        "Metallic resonances through a dark tempo-echo network: machines heard from far away.",
        {{"modal", PV{{"material", 4}, {"note", 0}, {"octave", 1}, {"inharm", 45}, {"decay", 1.5f}, {"mix", 35}}},
         {"echo_matrix", PV{{"division", 7}, {"pattern", 5}, {"feedback", 40}, {"tone", 2500}, {"darken", 60}, {"mix", 35}}},
         {"reverb", PV{{"mode", 4}, {"decay", 6}, {"damping", 75}, {"mix", 25}}}});
    add("v5.soft_focus", "Haze Focus", MB, "vocals, keys, drums", "Subtle", "pop, ambient pop",
        "Every attack swells in softly and blooms into a light haze, while the space stays close.",
        {{"swell", PV{{"rise", 220}, {"curve", 40}, {"depth", 70}, {"wash", 30}, {"mix", 100}}},
         {"cloud_chamber", PV{{"size", 40}, {"decay", 2.5f}, {"mix", 20}}}});
    add("v5.infinite_slope", "Infinite Slope", AC, "pads, drones, guitars", "Extreme", "drone, cinematic",
        "A huge, almost endless space (still decaying) whose lows and highs fall away at different speeds.",
        {{"cloud_chamber", PV{{"size", 100}, {"decay", 35}, {"low_decay", 1.1f}, {"high_decay", 0.25f}, {"density", 95}, {"bloom", 70}, {"drift", 55}, {"mix", 50}}},
         {"eq", PV{{"hp", 45}, {"lp", 11000}}}});
    add("v5.empty_cinema", "Empty Cinema", AC, "vocals, dialogue, drums, foley", "Moderate", "cinematic, game audio",
        "A distinct early-reflection pattern of a large room with a long but controlled tail.",
        {{"reflections", PV{{"pattern", 2}, {"time", 140}, {"taps", 9}, {"spread", 80}, {"decay", 45}, {"tone", 4500}, {"mix", 35}}},
         {"reverb", PV{{"mode", 1}, {"decay", 4}, {"predelay", 60}, {"mix", 30}}}});
    add("v5.swell_choir", "Swell Choir", AC, "guitars, plucks, keys, vocals", "Strong", "post-rock, shoegaze",
        "Volume-pedal swells blossom into a shimmering pitch halo: plucks become bowed pads.",
        {{"swell", PV{{"rise", 450}, {"curve", 60}, {"depth", 85}, {"wash", 40}, {"mix", 100}}},
         {"shimmer", PV{{"decay", 10}, {"shift1", 12}, {"regen", 55}, {"mix", 45}}}});
    add("v5.inhale", "Inhale", AC, "vocals, drums, synths", "Strong", "trailer, future garage",
        "A short reversed tail sucks into every hit, then a quick dark room answers it.",
        {{"reverse_bloom", PV{{"sync", 1}, {"division", 10}, {"tail", 1.2f}, {"tone", 4000}, {"mix", 45}}},
         {"drum_space", PV{{"mode", 4}, {"decay", 0.9f}, {"mix", 25}}}});

    // ==================================================================================== Drums / Drum Bus (22)
    add("v5.pocket_machine", "Pocket Machine", DR, "drums, percussion, loops", "Subtle", "neo-soul, lo-fi hip-hop",
        "A low-depth swung sixteenth pulse on the highs only: the hats breathe in the pocket, the kick stays solid.",
        {{"pulse_shaper", PV{{"pattern", 12}, {"division", 4}, {"depth", 35}, {"attack", 2}, {"gate", 55}, {"release", 60}, {"swing", 55}, {"accent", 60}, {"band", 3}, {"mix", 100}}},
         {"tape_colour", PV{{"drive", 3}, {"hf_loss", 3}, {"mix", 100}}}});
    add("v5.snare_cathedral", "Snare Cathedral", DR, "drums, snare, percussion", "Strong", "80s, cinematic",
        "The attack of each hit excites a bright room while a long, controlled chamber rises behind it.",
        {{"drum_space", PV{{"mode", 1}, {"size", 70}, {"decay", 1.4f}, {"attack_send", 90}, {"body_send", 20}, {"mix", 35}}},
         {"cloud_chamber", PV{{"size", 75}, {"decay", 3.5f}, {"high_decay", 0.5f}, {"predelay", 20}, {"mix", 25}}}});
    add("v5.dust_break", "Dust Break", DR, "drums, breaks, loops", "Strong", "jungle, lo-fi, breakbeat",
        "A break re-sliced every bar, worn through a dusty sampler and a filtered echo.",
        {{"slice_shuffle", PV{{"length", 2}, {"slices", 2}, {"pattern", 1}, {"reverse", 15}, {"swing", 20}, {"mix", 100}}},
         {"degrade", PV{{"rate", 16000}, {"bits", 12}, {"bandwidth", 55}, {"noise", 12}, {"mix", 80}}},
         {"delay", PV{{"sync", 1}, {"division", 7}, {"feedback", 25}, {"high_cut", 3500}, {"low_cut", 400}, {"mix", 15}}}});
    add("v5.ghost_hats", "Ghost Hats", DR, "hats, percussion, loops", "Moderate", "UK garage, house",
        "Ghost-note step gating with micro timing and probability on the highs: the hats talk.",
        {{"pulse_shaper", PV{{"pattern", 12}, {"division", 4}, {"depth", 70}, {"attack", 1}, {"gate", 35}, {"release", 30}, {"accent", 80}, {"chance", 85}, {"humanize", 35}, {"stereo", 1}, {"band", 3}, {"mix", 100}}},
         {"width", PV{{"width", 125}, {"mono_below", 200}}}});
    add("v5.concrete_room", "Concrete Room", DR, "drums, drum bus, loops", "Moderate", "rock, hip-hop",
        "Harder attacks into a dense, tight concrete room.",
        {{"transient", PV{{"attack", 45}, {"sustain", -10}, {"mix", 100}}},
         {"drum_space", PV{{"mode", 0}, {"size", 35}, {"decay", 0.5f}, {"attack_send", 80}, {"body_send", 40}, {"tone", 20}, {"mix", 30}}}});
    add("v5.rubber_kit", "Rubber Kit", DR, "drums, drum bus, loops", "Strong", "hip-hop, boom bap",
        "Parallel crush with a bouncy medium release and a warm low end under clean hits.",
        {{"drum_crush", PV{{"crush", 65}, {"drive", 8}, {"character", 0}, {"release", 1}, {"tone", -20}, {"body", 4}, {"blend", 55}}},
         {"transient", PV{{"attack", 25}, {"sustain", 10}, {"mix", 100}}}});
    add("v5.half_life_break", "Half-Life Break", DR, "drums, breaks", "Strong", "trip-hop, downtempo",
        "Half-time reconstruction of the break with its attacks kept sharp.",
        {{"time_warp", PV{{"mode", 2}, {"division", 10}, {"keep_pitch", 0}, {"tone", 12000}, {"mix", 100}}},
         {"band_transients", PV{{"low_attack", 35}, {"mid_attack", 40}, {"mix", 100}}}});
    add("v5.metal_skin", "Metal Skin", DR, "drums, percussion, toms", "Strong", "industrial, IDM",
        "Every hit rings metallic plate modes: a drum kit made of sheet metal.",
        {{"modal", PV{{"material", 4}, {"note", 7}, {"octave", 2}, {"inharm", 45}, {"decay", 0.8f}, {"excite", 1}, {"mix", 45}}},
         {"band_transients", PV{{"mid_attack", 30}, {"high_sustain", -30}, {"mix", 100}}}});
    add("v5.broken_pocket", "Broken Pocket", DR, "drums, loops, percussion", "Strong", "broken beat, footwork",
        "A syncopated tempo gate with swing and per-bar probability: the groove keeps shifting.",
        {{"pulse_shaper", PV{{"pattern", 13}, {"division", 4}, {"depth", 90}, {"attack", 1.2f}, {"gate", 45}, {"release", 35}, {"swing", 40}, {"chance", 75}, {"mix", 100}}},
         {"drum_space", PV{{"mode", 0}, {"size", 25}, {"decay", 0.35f}, {"mix", 20}}}});
    add("v5.drum_orbit", "Drum Orbit", DR, "drums, percussion, loops", "Moderate", "techno, dub techno",
        "Hits ping-pong left and right in a pattern and trail into filtered echoes.",
        {{"pulse_shaper", PV{{"pattern", 1}, {"division", 4}, {"depth", 55}, {"stereo", 2}, {"release", 70}, {"mix", 100}}},
         {"delay", PV{{"sync", 1}, {"division", 8}, {"stereo", 2}, {"feedback", 40}, {"low_cut", 300}, {"high_cut", 4500}, {"mix", 25}}}});
    add("v5.hard_soft", "Hard / Soft", DR, "drums, drum bus", "Moderate", "pop, rock",
        "Sharper attacks with a compressed, fuller body blended underneath.",
        {{"band_transients", PV{{"low_attack", 30}, {"mid_attack", 45}, {"high_attack", 20}, {"mid_sustain", -15}, {"mix", 100}}},
         {"drum_crush", PV{{"crush", 50}, {"drive", 4}, {"release", 3}, {"blend", 40}}}});
    add("v5.salted_cymbals", "Salted Cymbals", DR, "cymbals, overheads, hats", "Strong", "experimental, ambient",
        "Cymbal washes are smeared into a spectral sustain with a fine salt of crackle.",
        {{"spectral_smear", PV{{"smear", 55}, {"time", 60}, {"high_cut", 16000}, {"mix", 45}}},
         {"texture", PV{{"type", 1}, {"amount", -20}, {"band", 7000}, {"follow", 95}, {"mix", 100}}}});
    add("v5.tape_room", "Tape Room", DR, "drums, loops, percussion", "Moderate", "lo-fi, indie",
        "A wobbling tape stage into a short, diffuse room.",
        {{"tape_colour", PV{{"drive", 6}, {"bump", 2}, {"hf_loss", 6}, {"mix", 100}}},
         {"vibrato", PV{{"depth", 8}, {"rate", 0.8f}, {"mix", 100}}},
         {"drum_space", PV{{"mode", 1}, {"size", 30}, {"decay", 0.45f}, {"mix", 25}}}});
    add("v5.kick_lantern", "Kick Lantern", DR, "kick, drums, bass", "Moderate", "techno, trap",
        "A longer, rounder low end and a sharper click on each kick.",
        {{"band_transients", PV{{"low_attack", 50}, {"low_sustain", 45}, {"high_attack", 25}, {"low_x", 140}, {"mix", 100}}},
         {"saturation", PV{{"style", 1}, {"drive", 6}, {"mix", 60}}}});
    add("v5.shattered_snare", "Shattered Snare", DR, "snare, drums, loops", "Extreme", "glitch, breakcore",
        "Micro-loops and pitched fragments burst out after the dry attack.",
        {{"glitch", PV{{"division", 2}, {"repeat", 45}, {"ratchet", 4}, {"pitch", 30}, {"reverse", 20}, {"gate", 10}, {"mix", 100}}},
         {"band_transients", PV{{"mid_attack", 40}, {"mix", 100}}}});
    add("v5.deep_dub_kit", "Deep Dub Kit", DR, "drums, percussion", "Strong", "dub, dub techno",
        "Filtered, saturated tape feedback and a dark room under the kit.",
        {{"tape_echo", PV{{"sync", 1}, {"division", 8}, {"heads", 3}, {"intensity", 75}, {"saturation", 45}, {"treble", -4}, {"mix", 30}}},
         {"drum_space", PV{{"mode", 4}, {"decay", 1.2f}, {"mix", 20}}}});
    add("v5.glass_hats", "Glass Hats", DR, "hats, percussion", "Moderate", "IDM, minimal",
        "Hats ring a high, glassy resonance and flicker across the stereo field.",
        {{"modal", PV{{"material", 3}, {"note", 4}, {"octave", 5}, {"decay", 0.35f}, {"bright", 80}, {"mix", 35}}},
         {"pulse_shaper", PV{{"pattern", 1}, {"depth", 40}, {"stereo", 3}, {"mix", 100}}}});
    add("v5.crushed_but_clear", "Crushed But Clear", DR, "drums, drum bus, loops", "Extreme", "big beat, hip-hop",
        "Heavy, fuzzy parallel crushing under drums whose attacks stay intact.",
        {{"drum_crush", PV{{"crush", 90}, {"drive", 18}, {"character", 2}, {"release", 0}, {"react", 70}, {"tone", -30}, {"body", 3}, {"blend", 45}, {"out", -2}}},
         {"band_transients", PV{{"mid_attack", 25}, {"high_attack", 20}, {"mix", 100}}}});
    add("v5.offbeat_machinery", "Offbeat Machinery", DR, "drums, percussion, loops", "Strong", "techno, electro",
        "An alternating off-beat gate with humanised timing and a metallic early reflection.",
        {{"pulse_shaper", PV{{"pattern", 2}, {"division", 4}, {"depth", 85}, {"attack", 0.8f}, {"gate", 40}, {"release", 25}, {"humanize", 30}, {"stereo", 1}, {"mix", 100}}},
         {"reflections", PV{{"pattern", 7}, {"time", 18}, {"taps", 8}, {"mix", 25}}}});
    add("v5.last_hit_in_the_hall", "Last Hit in the Hall", DR, "drums, percussion, vocals", "Strong", "cinematic, trailer",
        "The attacks lead a long hall tail that blooms upward in pitch.",
        {{"drum_space", PV{{"mode", 1}, {"size", 60}, {"decay", 1}, {"attack_send", 90}, {"mix", 25}}},
         {"shimmer", PV{{"decay", 6}, {"shift1", 12}, {"regen", 40}, {"mix", 25}}}});
    add("v5.gated_booth", "Gated Booth", DR, "drums, snare, toms", "Strong", "80s, synth-pop",
        "A bright, dense gated room that slams shut a fraction after every hit.",
        {{"drum_space", PV{{"mode", 2}, {"size", 45}, {"decay", 1.2f}, {"attack_send", 85}, {"body_send", 45}, {"gate", 220}, {"tone", 25}, {"mix", 40}}},
         {"transient", PV{{"attack", 35}, {"mix", 100}}}});
    add("v5.hit_melody_kit", "Melodic Kit", EX, "drums, percussion, loops", "Strong", "IDM, game audio",
        "Every hit plays the next note of a minor-pentatonic line, ringing beside the drums.",
        {{"hit_melody", PV{{"key", 9}, {"scale", 0}, {"pattern", 2}, {"range", 2}, {"decay", 0.5f}, {"bright", 55}, {"mix", 45}}},
         {"reflections", PV{{"pattern", 0}, {"time", 50}, {"mix", 25}}}});

    // ==================================================================================== Tremolo / Groove (17)
    add("v5.amp_shimmer", "Amp Shimmer", TG, "keys, guitars, electric piano", "Subtle", "soul, indie",
        "Gentle, warm amp-style tremolo on the mids, with a little tube colour.",
        {{"tremolo", PV{{"mode", 0}, {"sync", 0}, {"rate", 5.5f}, {"depth", 35}, {"shape", 0}}},
         {"saturation", PV{{"style", 0}, {"drive", 4}, {"mix", 50}}}});
    add("v5.slow_breath", "Slow Breath", TG, "pads, strings, drones", "Moderate", "ambient, drone",
        "A slow stereo pulse that breathes across a pad, left and right a quarter apart.",
        {{"tremolo", PV{{"mode", 0}, {"sync", 1}, {"division", 15}, {"depth", 55}, {"shape", 0}, {"stereo", 90}}},
         {"cloud_chamber", PV{{"decay", 4}, {"mix", 25}}}});
    add("v5.syncopated_chords", "Syncopated Chords", TG, "keys, synths, chords", "Strong", "house, deep house",
        "A tresillo volume pattern turns sustained chords into a syncopated part.",
        {{"pulse_shaper", PV{{"pattern", 4}, {"division", 4}, {"depth", 90}, {"attack", 3}, {"gate", 60}, {"release", 70}, {"accent", 40}, {"mix", 100}}},
         {"delay", PV{{"sync", 1}, {"division", 8}, {"feedback", 25}, {"high_cut", 5000}, {"mix", 15}}}});
    add("v5.chopped_voice", "Chopped Voice", TG, "vocals, dialogue", "Strong", "future garage, pop",
        "Tight sixteenth chops with accents turn a vocal line into a rhythm part.",
        {{"pulse_shaper", PV{{"pattern", 8}, {"division", 4}, {"depth", 100}, {"attack", 1}, {"gate", 50}, {"release", 20}, {"accent", 50}, {"mix", 100}}},
         {"reflections", PV{{"pattern", 3}, {"time", 70}, {"mix", 20}}}});
    add("v5.rapid_gate", "Rapid Gate", TG, "drums, loops, synths", "Extreme", "trance, hardstyle",
        "Machine-gun thirty-second gating, hard but click-free.",
        {{"pulse_shaper", PV{{"pattern", 14}, {"division", 2}, {"depth", 100}, {"attack", 0.4f}, {"gate", 45}, {"release", 4}, {"accent", 10}, {"mix", 100}}},
         {"saturation", PV{{"style", 3}, {"drive", 5}, {"mix", 50}}}});
    add("v5.swing_shaker", "Swing Shaker", TG, "percussion, hats, shakers", "Moderate", "afro house, reggaeton",
        "Heavy swing on the high band: a straight shaker loop starts to shuffle.",
        {{"pulse_shaper", PV{{"pattern", 1}, {"division", 4}, {"depth", 60}, {"gate", 45}, {"release", 40}, {"swing", 70}, {"accent", 65}, {"band", 3}, {"mix", 100}}},
         {"reflections", PV{{"pattern", 1}, {"time", 25}, {"taps", 8}, {"mix", 20}}}});
    add("v5.left_right_percussion", "Left / Right Percussion", TG, "percussion, drums", "Moderate", "latin, afrobeat",
        "Alternate percussion steps move left and right, the gaps filled by the other side.",
        {{"pulse_shaper", PV{{"pattern", 5}, {"division", 4}, {"depth", 80}, {"gate", 55}, {"release", 45}, {"stereo", 3}, {"mix", 100}}},
         {"drum_space", PV{{"mode", 3}, {"size", 30}, {"decay", 0.4f}, {"mix", 20}}}});
    add("v5.polyrhythm_drift", "Polyrhythm Drift", TG, "synths, pads, loops", "Strong", "IDM, minimal",
        "A five-step pattern drifts against the four-four bar, while a slow tremolo rolls underneath.",
        {{"pulse_shaper", PV{{"pattern", 1}, {"division", 4}, {"steps", 5}, {"depth", 75}, {"gate", 50}, {"release", 60}, {"mix", 100}}},
         {"tremolo", PV{{"sync", 1}, {"division", 13}, {"depth", 40}, {"shape", 1}}}});
    add("v5.hit_opener", "Hit Opener", TG, "drums, loops, plucks", "Moderate", "breakbeat, electro",
        "The pattern restarts on every hit (no host clock needed), so each attack opens a little rhythm.",
        {{"pulse_shaper", PV{{"pattern", 9}, {"division", 4}, {"depth", 75}, {"gate", 40}, {"release", 30}, {"reset", 1}, {"mix", 100}}},
         {"reflections", PV{{"pattern", 3}, {"time", 90}, {"taps", 6}, {"mix", 20}}}});
    add("v5.tremolo_into_space", "Tremolo into Space", TG, "keys, guitars, synths", "Moderate", "shoegaze, dream pop",
        "Tremolo before the reverb: the space itself is excited in pulses and keeps the motion in its tail.",
        {{"tremolo", PV{{"sync", 1}, {"division", 7}, {"depth", 80}, {"shape", 1}, {"stereo", 45}}},
         {"cloud_chamber", PV{{"decay", 5}, {"bloom", 30}, {"mix", 40}}}});
    add("v5.space_then_chop", "Space then Chop", TG, "vocals, pads, guitars", "Strong", "future garage, ambient",
        "A big wash first, then a sixteenth gate chops the whole tail into a rhythmic shimmer.",
        {{"cloud_chamber", PV{{"decay", 7}, {"size", 80}, {"mix", 55}}},
         {"pulse_shaper", PV{{"pattern", 1}, {"division", 4}, {"depth", 85}, {"gate", 55}, {"release", 25}, {"mix", 100}}}});
    add("v5.tremolo_echo", "Pulse Echo", TG, "synths, guitars, vocals", "Moderate", "dub, synthwave",
        "Square-wave pulses feed a ping-pong echo: the repeats keep the rhythm.",
        {{"tremolo", PV{{"sync", 1}, {"division", 4}, {"depth", 85}, {"shape", 2}}},
         {"delay", PV{{"sync", 1}, {"division", 8}, {"stereo", 2}, {"feedback", 45}, {"mix", 30}}}});
    add("v5.low_pump", "Low Pump", TG, "bass, full mix, synths", "Moderate", "house, French touch",
        "Only the low band pumps on the beat; the highs stay steady.",
        {{"pulse_shaper", PV{{"pattern", 0}, {"division", 7}, {"depth", 70}, {"attack", 40}, {"gate", 20}, {"release", 120}, {"accent", 0}, {"band", 1}, {"mix", 100}}},
         {"saturation", PV{{"style", 1}, {"drive", 5}, {"mix", 70}}}});
    add("v5.broken_machine_pulse", "Broken Machine Pulse", TG, "synths, drums, noise", "Extreme", "industrial, noise",
        "A seeded pattern rewritten every bar, crushed and folded: unstable but bounded machinery.",
        {{"pulse_shaper", PV{{"pattern", 15}, {"division", 2}, {"depth", 100}, {"attack", 0.5f}, {"gate", 40}, {"release", 10}, {"seed", 23}, {"mix", 100}}},
         {"fold_crush", PV{{"drive", 10}, {"fold", 30}, {"crush", 25}, {"mix", 60}}}});
    add("v5.wurly_wave", "Wurly Wave", TG, "electric piano, keys, guitars", "Subtle", "soul, lo-fi",
        "A slow vintage tremolo with a small pan movement and soft tape warmth.",
        {{"tremolo", PV{{"sync", 0}, {"rate", 4.2f}, {"depth", 30}, {"shape", 0}, {"stereo", 25}}},
         {"tape_colour", PV{{"drive", 3}, {"hf_loss", 4}, {"mix", 100}}}});
    add("v5.gallop_pad", "Gallop Pad", TG, "pads, synths, strings", "Strong", "synthwave, cinematic",
        "A galloping three-note pulse drives a sustained pad forward.",
        {{"pulse_shaper", PV{{"pattern", 6}, {"division", 4}, {"depth", 80}, {"attack", 2}, {"gate", 50}, {"release", 50}, {"accent", 55}, {"mix", 100}}},
         {"reverb", PV{{"mode", 2}, {"decay", 2.5f}, {"mix", 20}}}});
    add("v5.halftime_sway", "Half-time Sway", TG, "full mix, loops, keys", "Moderate", "trip-hop, hip-hop",
        "A slow half-time pulse sways the whole loop with long, rounded edges.",
        {{"pulse_shaper", PV{{"pattern", 10}, {"division", 7}, {"depth", 55}, {"attack", 30}, {"gate", 70}, {"release", 200}, {"mix", 100}}},
         {"tape_colour", PV{{"drive", 4}, {"hf_loss", 5}, {"mix", 100}}}});
}

}  // namespace ih1
