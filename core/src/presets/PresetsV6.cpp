// V6 factory presets (feedback round 6, Phase 3): a vocal bank. Original IH1 recipes; product names in the research
// supplement were workflow references only, and no commercial preset was copied or imitated value-by-value.
// Coverage (V6 prompt 5.2): polished lead, intimate / close, wide doubles, harmony / choir, ethereal, reverse and
// shimmer throws, granular / time-stretched, chops and stutters, formant / vowel, vocoder / robot, harsh / industrial,
// spoken word / whisper / adlibs. Intelligibility is stated in each description: "clear" presets keep the words,
// "destructive" ones deliberately do not.
// Macros: none are authored. A chain that pairs a voice processor with a space or an echo gets the combination's
// own macros (ComboMacros.cpp: Voice in Space, Voice Throw, plus the older pairs), others have none.
#include "ih1/presets/Presets.h"

namespace ih1 {
namespace {
using PV = std::vector<std::pair<std::string, float>>;
const char* VO = "Vocal / Voice";
}  // namespace

void addV6Presets(std::vector<FactoryPreset>& v) {
    auto add = [&](const char* id, const char* name, const char* tags, const char* intensity, const char* styles, const char* desc,
                   std::vector<PresetFx> chain) {
        FactoryPreset p{id, name, VO, desc, 6, std::move(chain), tags};
        p.intensity = intensity;
        p.styles = styles;
        v.push_back(std::move(p));
    };

    // ---------------------------------------------------------------------------------------- polished lead
    add("v6.lead_polish", "Lead Polish", "lead vocals", "Subtle", "pop, R&B, singer-songwriter",
        "Clear: tamed sibilance, steady level, a little presence and air, set in a short studio wall and a pale tail.",
        {{"deesser", PV{{"freq", 6500}, {"threshold", -18}, {"range", 8}}},
         {"compressor", PV{{"mode", 2}, {"threshold", -24}, {"ratio", 3}, {"attack", 15}, {"release", 180}}},
         {"eq", PV{{"hp", 90}, {"b2_freq", 3200}, {"b2_gain", 2.5f}, {"hs_freq", 10000}, {"hs_gain", 3}}},
         {"reflections", PV{{"pattern", 1}, {"time", 25}, {"taps", 10}, {"mix", 15}}},
         {"cloud_chamber", PV{{"size", 30}, {"decay", 1.2f}, {"high_decay", 0.5f}, {"mix", 12}}}});
    add("v6.bright_pop_lead", "Bright Pop Lead", "lead vocals", "Moderate", "pop, dance-pop",
        "Clear: gently tuned, punchy and bright, with a quiet double and a plate behind it.",
        {{"pitch_correct", PV{{"speed", 25}, {"amount", 70}}},
         {"compressor", PV{{"mode", 1}, {"threshold", -22}, {"ratio", 4}, {"attack", 5}, {"release", 120}}},
         {"exciter", PV{{"freq", 4500}, {"drive", 6}, {"amount", 25}}},
         {"doubler", PV{{"detune", 6}, {"delay_ms", 12}, {"mix", 20}}},
         {"reverb", PV{{"mode", 2}, {"decay", 1.6f}, {"predelay", 40}, {"mix", 18}}}});
    add("v6.warm_tape_lead", "Warm Tape Lead", "lead vocals, spoken", "Moderate", "soul, indie, lo-fi pop",
        "Clear: a warm tape front end, slow levelling and a soft dotted tape echo under the line.",
        {{"tape_colour", PV{{"drive", 8}, {"out", -3}}},
         {"compressor", PV{{"mode", 2}, {"threshold", -20}, {"ratio", 2.5f}}},
         {"tape_echo", PV{{"sync", 1}, {"division", 8}, {"intensity", 25}, {"mix", 15}}},
         {"reverb", PV{{"mode", 2}, {"decay", 2}, {"mix", 15}}}});

    // ---------------------------------------------------------------------------------------- intimate / close
    add("v6.close_confession", "Close Confession", "lead vocals, spoken, whisper", "Subtle", "folk, singer-songwriter, ASMR",
        "Clear: right at your ear, warm low mids, softened top, and only a tiny room around it.",
        {{"deesser", PV{{"freq", 7000}, {"threshold", -20}, {"range", 6}}},
         {"eq", PV{{"hp", 80}, {"ls_freq", 200}, {"ls_gain", 2}, {"hs_gain", -2}}},
         {"reflections", PV{{"pattern", 0}, {"time", 12}, {"taps", 8}, {"decay", 40}, {"mix", 22}}}});
    add("v6.slapback_booth", "Slapback Booth", "lead vocals, adlibs", "Moderate", "rockabilly, indie rock, garage",
        "Clear: an upfront compressed voice with a dark single slap and a cluster of hard reflections.",
        {{"compressor", PV{{"mode", 1}, {"threshold", -18}, {"ratio", 5}}},
         {"delay", PV{{"sync", 0}, {"time_ms", 95}, {"stereo", 0}, {"feedback", 8}, {"low_cut", 250}, {"high_cut", 4500}, {"mix", 22}}},
         {"reflections", PV{{"pattern", 3}, {"time", 40}, {"mix", 15}}}});
    add("v6.breath_double", "Breath Double", "lead vocals, backing", "Moderate", "alt-pop, bedroom pop",
        "Clear: the voice gains a whispered twin that breathes every word with it, a little to the sides.",
        {{"whisper", PV{{"whisper", 100}, {"smooth", 80}, {"consonants", 60}, {"air", 4}, {"mix", 35}}},
         {"doubler", PV{{"detune", 4}, {"delay_ms", 14}, {"mix", 25}}},
         {"reflections", PV{{"time", 20}, {"mix", 15}}}});

    // ---------------------------------------------------------------------------------------- wide doubles
    add("v6.wide_double", "Wide Double", "lead vocals, backing, choir", "Moderate", "pop, rock",
        "Clear: four detuned, slightly late copies spread wide; the lows stay centred and mono-safe.",
        {{"doubler", PV{{"voices", 1}, {"detune", 11}, {"delay_ms", 20}, {"drift", 50}, {"width", 100}, {"low_cut", 180}, {"mix", 45}}},
         {"width", PV{{"width", 120}, {"mono_below", 150}}}});
    add("v6.micro_stack", "Micro Stack", "backing, choir, lead vocals", "Moderate", "pop, R&B",
        "Clear: a thick stack from slow three-voice chorus and a tight double, like several takes sung together.",
        {{"chorus", PV{{"voices", 2}, {"rate", 0.25f}, {"depth", 25}, {"delay_ms", 14}, {"spread", 160}, {"mix", 40}}},
         {"doubler", PV{{"detune", 7}, {"delay_ms", 9}, {"mix", 35}}},
         {"width", PV{{"width", 125}, {"mono_below", 160}}}});
    add("v6.octave_wall", "Octave Wall", "lead vocals, hooks, adlibs", "Strong", "EDM, hyperpop, stadium",
        "Clear: octaves above and below and a fifth, detuned and spread into a wall behind the lead.",
        {{"harmony", PV{{"mode", 0}, {"voices", 2}, {"int1", 12}, {"int2", -12}, {"int3", 7}, {"lvl1", -6}, {"lvl2", -8}, {"lvl3", -10},
                        {"pan1", -70}, {"pan2", 70}, {"pan3", 0}, {"detune", 10}, {"humanize", 40}, {"wet", -4}}},
         {"doubler", PV{{"mix", 30}}},
         {"width", PV{{"width", 130}, {"mono_below", 160}}}});

    // ---------------------------------------------------------------------------------------- harmony / choir
    add("v6.thirds_choir", "Thirds Choir", "lead vocals, backing", "Moderate", "gospel, pop, folk",
        "Clear: a third above and a fourth below in C major (set Key / Scale to your song), panned, in a hall.",
        {{"harmony", PV{{"mode", 1}, {"key", 0}, {"scale", 0}, {"voices", 1}, {"int1", 2}, {"int2", -3}, {"lvl1", -3}, {"lvl2", -5},
                        {"pan1", -50}, {"pan2", 50}, {"humanize", 35}, {"glide", 60}, {"dry", -2}, {"wet", -3}}},
         {"reverb", PV{{"mode", 1}, {"decay", 3}, {"predelay", 30}, {"mix", 25}}}});
    add("v6.cathedral_choir", "Cathedral Choir", "lead vocals, sustained notes, choir", "Strong", "choral, cinematic",
        "A four-voice choir (octave, fifth, fourth below, octave below) with vibrato, bloomed into a vast chamber.",
        {{"harmony", PV{{"mode", 0}, {"voices", 3}, {"int1", 12}, {"int2", 7}, {"int3", -5}, {"int4", -12}, {"lvl1", -6}, {"lvl2", -8},
                        {"lvl3", -8}, {"lvl4", -10}, {"detune", 14}, {"vib_depth", 25}, {"humanize", 50}, {"character", -2}}},
         {"cloud_chamber", PV{{"size", 85}, {"decay", 9}, {"bloom", 50}, {"mix", 40}}}});
    add("v6.fifth_angels", "Fifth Angels", "sustained vocals, hooks", "Strong", "ambient pop, worship, cinematic",
        "A bright fifth-and-octave halo with lifted formants, feeding a shimmering pitch-feedback space.",
        {{"harmony", PV{{"mode", 0}, {"voices", 1}, {"int1", 7}, {"int2", 12}, {"lvl1", -4}, {"lvl2", -8}, {"character", 3}, {"dry", -2}, {"wet", -3}}},
         {"shimmer", PV{{"decay", 8}, {"shift1", 12}, {"shift2", 7}, {"regen", 45}, {"mix", 35}}}});
    add("v6.midi_choir", "MIDI Choir", "lead vocals (MIDI chords)", "Strong", "electronic, choral pop",
        "Play chords on a MIDI keyboard: up to four voices sing them from your voice, gliding, in a warm hall.",
        {{"harmony", PV{{"mode", 2}, {"voices", 3}, {"glide", 80}, {"humanize", 40}}},
         {"reverb", PV{{"mode", 1}, {"decay", 2.8f}, {"mix", 25}}}});

    // ---------------------------------------------------------------------------------------- ethereal / atmospheric
    add("v6.ether_veil", "Ether Veil", "sustained vocals, backing, spoken", "Strong", "ambient, dream pop",
        "Breath and shimmer surround the voice: an airy veil that keeps rising behind each phrase.",
        {{"whisper", PV{{"mix", 30}, {"air", 6}}},
         {"shimmer", PV{{"decay", 10}, {"regen", 50}, {"mix", 35}}},
         {"cloud_chamber", PV{{"size", 80}, {"decay", 8}, {"mix", 30}}}});
    add("v6.dream_halo", "Dream Halo", "lead vocals, hooks", "Strong", "dream pop, shoegaze",
        "Octave halos with natural formants, a reversed swell before every phrase and a soft cloud after.",
        {{"pitch_shift", PV{{"shift_a", 12}, {"shift_b", -12}, {"level_b", -12}, {"preserve", 1}, {"width", 80}, {"mix", 25}}},
         {"reverse_bloom", PV{{"sync", 1}, {"division", 13}, {"tail", 4}, {"mix", 40}}},
         {"cloud_chamber", PV{{"decay", 6}, {"mix", 25}}}});
    add("v6.distant_choir", "Distant Choir", "lead vocals, choir, backing", "Strong", "cinematic, post-rock",
        "A choir heard from the back of a stone hall: band-limited, far away, with a long late tail.",
        {{"harmony", PV{{"mode", 0}, {"voices", 2}, {"int1", 12}, {"int2", -12}, {"int3", 7}, {"humanize", 45}, {"dry", -5}, {"wet", -6}}},
         {"eq", PV{{"hp", 300}, {"lp", 5000}}},
         {"reverb", PV{{"mode", 4}, {"decay", 7}, {"predelay", 60}, {"mix", 50}}}});

    // ---------------------------------------------------------------------------------------- reverse / shimmer throws
    add("v6.reverse_vox_swell", "Reverse Vox Swell", "lead vocals, adlibs, spoken", "Strong", "trip-hop, pop, cinematic",
        "Every line is preceded by its own reversed, inhaled tail; the dry voice stays clear in front.",
        {{"reverse_bloom", PV{{"sync", 1}, {"division", 10}, {"tail", 3}, {"tone", 7000}, {"mix", 50}}},
         {"reflections", PV{{"time", 30}, {"mix", 15}}}});
    add("v6.falling_throw", "Falling Throw", "adlibs, hooks, lead vocals", "Moderate", "hip-hop, trap, pop",
        "Clear: quarter-note throws that step down in pitch, duck under the voice and bloom in the gaps.",
        {{"vocal_throw", PV{{"mode", 0}, {"sync", 1}, {"division", 10}, {"feedback", 55}, {"drop", -3}, {"tone", 1800}, {"focus", 40}, {"duck", 70}, {"mix", 50}}},
         {"reverb", PV{{"mode", 2}, {"decay", 2.5f}, {"mix", 15}}}});
    add("v6.last_word_echo", "Last Word Echo", "lead vocals, rap, spoken", "Subtle", "hip-hop, R&B, pop",
        "Clear: only the end of each phrase is caught and thrown into dotted echoes; the lines themselves stay dry.",
        {{"vocal_throw", PV{{"mode", 1}, {"sync", 1}, {"division", 8}, {"feedback", 50}, {"tone", 2200}, {"focus", 45}, {"duck", 80}, {"catch_ms", 400}, {"mix", 55}}},
         {"cloud_chamber", PV{{"decay", 4}, {"mix", 20}}}});
    add("v6.rising_throw", "Rising Throw", "adlibs, hooks", "Moderate", "EDM, hyperpop",
        "Echoes that climb an octave each repeat and dissolve into shimmer: a lift into the drop.",
        {{"vocal_throw", PV{{"sync", 1}, {"division", 7}, {"feedback", 45}, {"drop", 12}, {"tone", 3000}, {"focus", 25}, {"duck", 50}, {"mix", 45}}},
         {"shimmer", PV{{"decay", 6}, {"mix", 30}}}});

    // ---------------------------------------------------------------------------------------- granular / time-stretched
    add("v6.vowel_cloud", "Vowel Cloud", "sustained vocals, choir, spoken", "Strong", "ambient, experimental",
        "Destructive: the voice becomes a slow, wide cloud of overlapping vowel grains; words dissolve.",
        {{"granular", PV{{"size_ms", 220}, {"density", 20}, {"spray_ms", 600}, {"spread", 90}, {"jitter", 60}, {"diffusion", 60}, {"mix", 60}}},
         {"cloud_chamber", PV{{"size", 70}, {"decay", 7}, {"mix", 35}}}});
    add("v6.held_exhale", "Held Exhale", "sustained vocals, spoken, whisper", "Strong", "drone, cinematic",
        "Destructive: blurred, smeared spectral sustain turned to breath: a held exhale under the voice.",
        {{"spectral", PV{{"blur", 80}, {"smear", 50}, {"fade_ms", 1200}, {"mix", 60}}},
         {"whisper", PV{{"mix", 50}, {"air", 5}}},
         {"reverb", PV{{"mode", 1}, {"decay", 5}, {"mix", 30}}}});

    // ---------------------------------------------------------------------------------------- chops / stutters
    add("v6.syllable_stutter", "Syllable Stutter", "lead vocals, rap, vocal chops", "Strong", "EDM, future bass, pop",
        "Sixteenth-note syllable repeats fill the second half of every beat; the start of each beat plays through.",
        {{"stutter", PV{{"interval", 0}, {"offset", 4}, {"gate", 3}, {"slice", 2}, {"chance", 100}, {"mix", 100}}},
         {"reflections", PV{{"time", 20}, {"mix", 15}}}});
    add("v6.vox_chop_glitch", "Vox Chop Glitch", "vocal chops, adlibs, rap", "Extreme", "glitch-hop, IDM, hyperpop",
        "Destructive: ratchets, reversed shards and pitch jumps cut the phrase up, with throws catching the debris.",
        {{"glitch", PV{{"division", 4}, {"repeat", 50}, {"ratchet", 3}, {"reverse", 30}, {"jump", 30}, {"range", 4}, {"pitch", 20}, {"gate", 15}, {"lofi", 10}, {"seed", 21}}},
         {"vocal_throw", PV{{"sync", 1}, {"division", 7}, {"feedback", 35}, {"mix", 35}}}});
    add("v6.reverse_chop", "Reverse Chop", "vocal chops, hooks", "Strong", "future bass, house",
        "Eighth-note chunks of the voice play backwards, trailed by a short sixteenth echo.",
        {{"reverse", PV{{"mode", 0}, {"sync", 1}, {"division", 7}, {"fade", 6}, {"mix", 70}}},
         {"delay", PV{{"sync", 1}, {"division", 4}, {"feedback", 25}, {"mix", 20}}}});

    // ---------------------------------------------------------------------------------------- formant / vowel
    add("v6.talkbox_funk", "Talkbox Funk", "lead vocals, synths, guitars", "Strong", "funk, boogie, G-funk",
        "The louder you sing or play, the wider the mouth opens: a gritty talkbox wah with a little chorus.",
        {{"vowel_morph", PV{{"motion", 2}, {"depth", 70}, {"vowel", 3}, {"resonance", 65}, {"drive", 35}}},
         {"compressor", PV{{"mode", 1}, {"threshold", -20}, {"ratio", 4}}},
         {"chorus", PV{{"voices", 1}, {"depth", 20}, {"mix", 25}}}});
    add("v6.vowel_sequencer", "Vowel Sequencer", "sustained vocals, synths, pads", "Strong", "electro, synth-pop",
        "A tempo-synced I-A-U-E vowel pattern speaks through the sound in sixteenths, echoed on dotted eighths.",
        {{"vowel_morph", PV{{"motion", 3}, {"sync", 1}, {"division", 4}, {"pattern", 2}, {"depth", 100}, {"resonance", 70}, {"drive", 20}}},
         {"delay", PV{{"sync", 1}, {"division", 8}, {"feedback", 30}, {"mix", 20}}}});
    add("v6.giant_mouth", "Giant Mouth", "lead vocals, spoken", "Strong", "dark pop, horror, trap",
        "A huge, dark speaker: formants dropped and a slowly chewing O-U vowel, in a small dead room.",
        {{"formant", PV{{"shift", -6}, {"detail", 50}}},
         {"vowel_morph", PV{{"motion", 1}, {"rate", 0.2f}, {"depth", 40}, {"vowel", 3.5f}, {"formant", -5}, {"resonance", 55}, {"mix", 60}}},
         {"reverb", PV{{"mode", 0}, {"decay", 1.2f}, {"mix", 15}}}});
    add("v6.tiny_mouth", "Tiny Mouth", "lead vocals, adlibs", "Moderate", "hyperpop, novelty, pop",
        "Clear: the same notes from a much smaller throat, lightly tuned and doubled.",
        {{"formant", PV{{"shift", 7}}},
         {"pitch_correct", PV{{"amount", 60}}},
         {"doubler", PV{{"mix", 25}}}});

    // ---------------------------------------------------------------------------------------- vocoder / robot
    add("v6.robot_monotone", "Robot Monotone", "spoken, rap, lead vocals", "Strong", "electro, sci-fi",
        "Every word on one fixed note: a 24-band saw vocoder with consonants kept, crushed and boxed in metal.",
        {{"vocoder", PV{{"bands", 3}, {"carrier", 0}, {"note", 48}, {"use_midi", 0}, {"unvoiced", 50}, {"mix", 100}}},
         {"bitcrush", PV{{"bits", 10}, {"rate", 16000}, {"mix", 40}}},
         {"reflections", PV{{"pattern", 7}, {"time", 15}, {"mix", 20}}}});
    add("v6.stepped_robot", "Stepped Robot", "lead vocals, hooks", "Strong", "electro-pop, trap",
        "Instant, hard pitch steps onto a minor-pentatonic scale in C (set the Key), with a faint vowel colour.",
        {{"pitch_correct", PV{{"scale", 6}, {"speed", 0}, {"tolerance", 0}, {"amount", 100}}},
         {"vowel_morph", PV{{"motion", 0}, {"vowel", 1}, {"resonance", 40}, {"mix", 35}}},
         {"delay", PV{{"sync", 1}, {"division", 7}, {"feedback", 25}, {"mix", 15}}}});
    add("v6.compu_voice", "Compu-Voice", "spoken, rap, vocal chops", "Extreme", "chiptune, glitch, electro",
        "Destructive: a 6-bit, 8 kHz voice, ring-modulated and forced through a fast vowel pattern.",
        {{"bitcrush", PV{{"bits", 6}, {"rate", 8000}, {"jitter", 10}, {"mix", 100}}},
         {"ring_mod", PV{{"freq", 110}, {"mix", 35}}},
         {"vowel_morph", PV{{"motion", 3}, {"pattern", 4}, {"sync", 1}, {"division", 7}, {"depth", 60}, {"mix", 50}}}});

    // ---------------------------------------------------------------------------------------- harsh / industrial
    add("v6.megaphone_riot", "Megaphone Riot", "lead vocals, shouts, spoken", "Strong", "punk, industrial, rap",
        "A driven, band-passed megaphone voice that shouts back from a narrow, ducked throw.",
        {{"filter", PV{{"type", 4}, {"cutoff", 1800}, {"resonance", 40}, {"drive", 50}}},
         {"saturation", PV{{"style", 3}, {"drive", 20}, {"out", -9}}},
         {"vocal_throw", PV{{"sync", 1}, {"division", 10}, {"feedback", 40}, {"tone", 1500}, {"focus", 70}, {"mix", 35}}}});
    add("v6.industrial_scream", "Industrial Scream", "shouts, screams, spoken", "Extreme", "industrial, metal, noise",
        "Destructive: folded distortion, a sub-octave growl, broken digital grit and a gated room.",
        {{"saturation", PV{{"style", 4}, {"drive", 24}}},
         {"pitch_shift", PV{{"shift_a", -12}, {"preserve", 0}, {"character", -4}, {"mix", 45}}},
         {"degrade", PV{{"rate", 8000}, {"bits", 8}, {"noise", 20}, {"mix", 60}}},
         {"reverb", PV{{"mode", 5}, {"decay", 1}, {"mix", 25}}}});

    // ---------------------------------------------------------------------------------------- spoken / whisper / adlib
    add("v6.ghost_whisper", "Ghost Whisper", "spoken, whisper, lead vocals", "Strong", "horror, ambient, trip-hop",
        "The voice is replaced by its own whisper, preceded by a reversed breath and left hanging in a cold space.",
        {{"whisper", PV{{"mix", 100}, {"smooth", 90}, {"air", 7}, {"low_cut", 250}, {"out", -2}}},
         {"reverse_bloom", PV{{"sync", 1}, {"division", 13}, {"mix", 40}}},
         {"cloud_chamber", PV{{"decay", 6}, {"mix", 35}}}});
    add("v6.radio_voice", "Radio Voice", "spoken, lead vocals, samples", "Moderate", "lo-fi, hip-hop, podcast intro",
        "Clear: an old radio speaker, narrow and pushed, with a little sample-rate grit and a tiny box.",
        {{"eq", PV{{"hp", 400}, {"lp", 3500}, {"b1_freq", 1500}, {"b1_gain", 6}}},
         {"saturation", PV{{"style", 2}, {"drive", 12}}},
         {"degrade", PV{{"rate", 11000}, {"bits", 12}, {"noise", 10}, {"mix", 50}}},
         {"reflections", PV{{"pattern", 7}, {"time", 8}, {"mix", 12}}}});
    add("v6.adlib_spiral", "Adlib Spiral", "adlibs, hooks, rap", "Strong", "trap, hip-hop, R&B",
        "Dotted-eighth throws that fall a fourth each repeat and spin out wide through a chorus.",
        {{"vocal_throw", PV{{"sync", 1}, {"division", 8}, {"feedback", 65}, {"drop", -5}, {"tone", 2600}, {"focus", 60}, {"duck", 50}, {"mix", 55}}},
         {"chorus", PV{{"depth", 40}, {"mix", 40}}},
         {"width", PV{{"width", 140}, {"mono_below", 160}}}});
}

}  // namespace ih1
