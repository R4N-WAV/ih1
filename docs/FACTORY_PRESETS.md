# Factory preset index (V2.1)

66 read-only factory presets in 10 categories, defined as data in `core/src/presets/Presets.cpp` (28 legacy ids from V1.1 kept and re-filed so DAW projects that reference them still resolve, plus 38 new). Every preset is an original IH1 design: no names, descriptions, settings or assets from any commercial product.

Checked by `tests/test_presets.cpp`: validates against the registry; loads as one atomic chain with exact order and values; deterministic instance ids (same formation every time); no near-duplicates (full chain + settings signature); source tags present; and the **factory audit** renders every preset on three sources at -18 dBFS RMS (drum loop, sustained pad chord, vocal-like formant pulses) and checks level against the source (-24 .. +9 dB; measured range -15 .. +8 dB), peak (< +6 dBFS), mono compatibility (L/R correlation > -0.3) and bit-identical repeat renders. Loop / Freeze presets arm and latch only once real signal has arrived.

The arrows walk this order (category by category) and wrap; user presets (*User*) follow.

| Category | Presets |
|---|---|
| Essentials | 7 |
| Granular Motion | 7 |
| Stretch & Smear | 7 |
| Glitch & Rhythm | 7 |
| Vocal Transformations | 9 |
| Atmospheres | 7 |
| Harmonic & Tonal | 5 |
| Movement & Width | 6 |
| Character & Dirt | 5 |
| Experimental | 6 |

## Essentials

| Preset | Chain | Sources | Sound |
|---|---|---|---|
| **Dotted Rain** (`rhy.dotted_rain`) | filter → delay → reverb | guitars, vocals, synths | Dotted-eighth ping-pong repeats, thinned below and softened above, on a plate. |
| **Warm Console** (`ton.warm_console`) | eq → transient → compressor → tape_colour → exciter | full mix, drums, bass | Subtle polish: gentle shelves, a softened attack, opto levelling and console drive. |
| **Vocal Polish** (`sig.vocal_polish`) | deesser → eq → compressor → reverb | vocals | A clean starting point: de-essed, lifted, levelled and placed on a short plate. |
| **Clean Slap** (`ess.clean_slap`) | delay → reverb | vocals, guitars, drums | A single short slapback, thinned and darkened, with a touch of room. Mix-ready. |
| **Quarter Throw** (`ess.quarter_throw`) | delay → reverb | vocals, synths, keys | Quarter-note ping-pong that ducks under the dry signal and softens with every repeat. |
| **Velvet Plate** (`ess.plate_vocal`) | eq → compressor → reverb | vocals, snare | Levelled and lifted, then a bright short plate with pre-delay so the words stay in front. |
| **Mix Glue** (`ess.mix_glue`) | compressor → tape_colour | full mix, drums, loops | Gentle 2:1 bus compression with slow attack and a light console drive. Subtle on purpose. |

## Granular Motion

| Preset | Chain | Sources | Sound |
|---|---|---|---|
| **Glass Grains** (`frg.glass_grains`) | granular → reverb | keys, plucks, vocals | Octave-up grains scattered forwards and backwards into a long hall. |
| **Loop Ghost** (`frg.loop_ghost`) | looper → granular → reverb | loops, drums, keys | Captures two bars, then lets them dissolve an octave down into a small space. |
| **Particle Drift** (`gm.particle_drift`) | grain_engine → reverb | synths, keys, vocals, guitars | Medium grains scattered half a second back and across the field, settling into a hall. |
| **Octave Dust** (`gm.octave_dust`) | grain_engine → chorus | keys, plucks, guitars | Plucked grains jump between octaves and some run backwards: sparkling, never out of tune. |
| **Minor Scatter** (`gm.minor_scatter`) | grain_engine → delay | synths, pads, keys | Gaussian grains snapped to a minor scale, echoed on dotted eighths across the stereo field. |
| **Swarm** (`gm.swarm`) | grain_engine → width | vocals, full mix, noise | Dozens of tiny, slightly detuned grains with independent left and right streams: a buzzing, living cloud. |
| **Cascade** (`gm.cascade`) | grain_engine → reverb | keys, plucks, vocals | Grain feedback through a fifth-up delay: each pass is re-grained higher and darker until it fades. |

## Stretch & Smear

| Preset | Chain | Sources | Sound |
|---|---|---|---|
| **Backwards Letters** (`frg.backwards_letters`) | reverse → spectral → delay | vocals, guitars | Phrases played back in reverse, smeared, then bounced across the stereo field. |
| **Frozen Breath** (`frg.frozen_breath`) | spectral → granular → width | pads, vocals | A smeared spectral haze with sparse grains drifting through it, very wide. |
| **Half-Time Haze** (`ss.half_time_haze`) | grain_engine → reverb | vocals, synths, loops | The recent past played back at half speed with the pitch kept, smoothly spliced, drifting into reverb. |
| **Quarter Drift** (`ss.quarter_drift`) | grain_engine → spectral | pads, vocals, keys | Four times slower, long overlapping grains at a constant overlap, blurred at the edges. |
| **Glacier** (`ss.glacier`) | grain_engine → reverb | pads, strings, ambient | An eighth-speed scan through the buffer: chords become slowly shifting glacial walls. |
| **Tape Slowdown** (`ss.tape_slow`) | grain_engine → tape_colour | drums, loops, vocals | Varispeed at half speed: the recent past an octave down, like a slowed tape, warmly saturated. |
| **Smear Field** (`ss.smear_field`) | spectral → grain_engine | vocals, guitars, full mix | Spectral blur plus wide-sprayed grains: transients dissolve into a soft, moving smear. |

## Glitch & Rhythm

| Preset | Chain | Sources | Sound |
|---|---|---|---|
| **Stutter Bloom** (`rhy.stutter_bloom`) | stutter → delay → reverb | drums, loops | Bar-wise sixteenth stutters that echo and bloom into space. |
| **Gated Pulse** (`rhy.gated_pulse`) | gate → filter → delay | synths, pads, loops | Eighth-note pumping into a tempo-synced filter sweep and a quarter-note echo. |
| **Tape Triplets** (`rhy.tape_triplets`) | tape_echo → saturation | guitars, drums, vocals | Three tape heads on quarter-note triplets, saturated like an old echo unit. |
| **Slice Retrigger** (`gr.slice_retrigger`) | grain_engine → eq | drums, loops, vocals | The playhead jumps back to the same slice every eighth note: rhythmic repeats that follow the tempo. |
| **Backspin Slices** (`gr.backspin`) | grain_engine → delay | drums, loops, vocals | Every beat the last moment plays backwards from the top: reversed slices locked to the grid. |
| **Buffer Roll** (`gr.buffer_roll`) | stutter → bitcrush | drums, loops, full mix | Thirty-second-note rolls that step down in pitch and decay, roughened by a light crush. |
| **Halftime Chop** (`gr.halftime_chop`) | grain_engine → gate | drums, loops | Half-speed slices restarted every half note, then pumped on eighths: a lurching, chopped groove. |

## Vocal Transformations

| Preset | Chain | Sources | Sound |
|---|---|---|---|
| **Choir of One** (`voc.choir_of_one`) | harmony → doubler → reverb | vocals | Three in-key harmonies, doubled and placed in a modest hall. |
| **Wide Double** (`voc.wide_double`) | doubler → eq → compressor | vocals, guitars | A natural four-voice double, a lifted presence band, gently levelled. |
| **Octave Ghost** (`voc.octave_ghost`) | pitch_shift → formant → reverse → reverb | vocals | Octaves above and below, darker formants, with backwards echoes trailing behind. |
| **Tuned Glide** (`voc.tuned_glide`) | pitch_correct → doubler → delay | vocals | Firm pitch correction in A minor, doubled, with a quiet quarter-note echo. |
| **Machine Choir** (`sig.machine_choir`) | vocoder → chorus → reverb | vocals, synths | A 32-band vocoder (played from MIDI notes, or a fixed C3) chorused into a hall. |
| **Whisper Cloud** (`vt.whisper_cloud`) | formant → grain_engine → reverb | vocals, spoken word | Tiny bright grains around a slightly lifted formant: the voice breaks into breathy particles. |
| **Octave Halo** (`vt.octave_halo`) | harmony → grain_engine | vocals | Swelling grains an octave above and below the voice, behind an in-key harmony, like a distant choir. |
| **Circuit Voice** (`vt.ring_robot`) | pitch_correct → freq_shift → bitcrush | vocals, spoken word | Hard-tuned, frequency-shifted against itself and lightly crushed: a metallic machine voice. |
| **Giant** (`vt.giant`) | pitch_shift → formant → reverb | vocals, spoken word | An octave down with darker formants and a big room: a huge, slow-moving voice. |

## Atmospheres

| Preset | Chain | Sources | Sound |
|---|---|---|---|
| **Slow Horizon** (`atm.slow_horizon`) | eq → reverb → width | pads, vocals, guitars | A vast, darkened cathedral that blooms slowly behind the source and opens wide. |
| **Glass Cathedral** (`atm.glass_cathedral`) | shimmer → chorus | pads, keys, vocals | Octave and fifth shimmer rising out of a long tail, gently chorused. |
| **Under Ice** (`atm.under_ice`) | filter → convolution → spectral | full mix, pads, loops | Muffled and submerged: low-passed, bloomed through a strange space, edges blurred. |
| **Afterglow** (`atm.afterglow`) | vibrato → tape_echo → reverb → tape_colour | guitars, keys, vocals | Worn tape repeats with a slow wobble, fading into a soft plate. |
| **Slow Bloom** (`atm.slow_bloom`) | grain_engine → shimmer | pads, vocals, guitars | Third-speed swelling grains that bloom into an octave shimmer tail. |
| **Deep Hollow** (`atm.deep_hollow`) | grain_engine → reverb | pads, drones, guitars | A long, heavily damped cathedral fed by low, re-grained feedback: dark and enormous. |
| **Veil** (`atm.veil`) | spectral → reverb | vocals, keys, guitars | A translucent spectral veil with a faint shimmer, softened on a long plate. |

## Harmonic & Tonal

| Preset | Chain | Sources | Sound |
|---|---|---|---|
| **Supercluster** (`sig.supercluster`) | chorus → reverse → resonator → reverb → width | keys, pads, vocals | IH1's own: chorus, reversed grains and tuned resonance gathering in a long hall. |
| **Fifth Resonance** (`ht.fifth_resonance`) | resonator → grain_engine | drums, percussion, plucks | Hits ring out as tuned fifths in D, with grains snapped to fifths shimmering above. |
| **Major Bloom** (`ht.major_bloom`) | harmony → shimmer | vocals, keys, guitars | Three in-key harmonies opening into a fifth-and-octave shimmer. |
| **Minor Drone** (`ht.minor_drone`) | grain_engine → resonator | pads, guitars, drones | Captures two seconds, loops it at half speed and lets a minor resonator sing over it. |
| **Crystal** (`ht.crystal`) | grain_engine → resonator | keys, plucks, synths | Octave-up pentatonic grains over a high pentatonic resonator: glassy and bell-like. |

## Movement & Width

| Preset | Chain | Sources | Sound |
|---|---|---|---|
| **Tidal Chorus** (`mot.tidal_chorus`) | chorus → tremolo | guitars, synths, keys | A slow, deep four-voice chorus that drifts from side to side on the half note. |
| **Jet Wash** (`mot.jet_wash`) | flanger → reverb | drums, guitars, full mix | A slow, resonant through-zero sweep in a tight room. |
| **Leslie Room** (`mot.leslie_room`) | rotary → tape_colour → reverb | keys, organ, guitars | Slow rotary speaker with a little drive, warmed on tape, in a small room. |
| **Wide Grains** (`mw.wide_grains`) | grain_engine → width | synths, vocals, guitars | Independent left and right grain streams add width without phasing; the bass stays centred. |
| **Pan Haze** (`mw.pan_haze`) | tremolo → chorus → reverb | pads, keys, guitars | A slow half-note auto-pan through a four-voice chorus into a soft hall. |
| **Phase Drift** (`mw.phase_drift`) | phaser → flanger | guitars, synths, drums | Twelve-stage phasing so slow it feels like weather, with a faint flange on top. |

## Character & Dirt

| Preset | Chain | Sources | Sound |
|---|---|---|---|
| **Broken Transistor** (`ton.broken_transistor`) | amp → bitcrush → compressor | guitars, drums, bass | A cranked small combo, partly crushed, slammed by a fast compressor. |
| **Crushed Telephone** (`ton.crushed_telephone`) | filter → bitcrush → saturation → width | vocals, drums | A narrow, resonant band, bit-reduced and clipped hard, folded to mono. |
| **Cassette** (`cd.cassette`) | tape_colour → vibrato → bitcrush | drums, keys, full mix, loops | Worn tape: saturated, dulled, slowly warbling and slightly crushed. |
| **Blown Speaker** (`cd.blown_speaker`) | amp → saturation | guitars, bass, drums | A cranked small amp into a folding stage, blended in parallel so the body survives. |
| **Lo-Fi Sampler** (`cd.lofi_sampler`) | bitcrush → eq → compressor | drums, loops, keys | Twelve-bit sampler colour, rounded lows, and a fast FET squeeze. |

## Experimental

| Preset | Chain | Sources | Sound |
|---|---|---|---|
| **Spiral Stairs** (`mot.spiral_stairs`) | phaser → freq_shift → width | synths, guitars | Ten-stage phasing into a frequency-shifted feedback loop that endlessly climbs. |
| **Fold Space** (`ton.fold_space`) | saturation → ring_mod → reverb | synths, drums, bass | Wavefolding and ring modulation, cut short by a gated burst of space. |
| **Doppler Drift** (`sig.doppler_drift`) | doppler → delay → reverb | vocals, synths, percussion | The source circles the listener while modulated echoes trail behind it. |
| **Ring Cascade** (`ex.ring_cascade`) | ring_mod → grain_engine → freq_shift | synths, drums, noise | Ring modulation feeding pitched grain feedback and a slowly climbing frequency shift. |
| **Inverted World** (`ex.inverted_world`) | grain_engine → spectral | vocals, guitars, keys | A held loop scanned backwards at half speed like reversed tape, under a spectral shimmer. |
| **Fractal Feedback** (`ex.fractal_feedback`) | grain_engine → reverb | synths, vocals, noise | Grains re-grained through minor-scale feedback, each pass a little lower and darker. |
