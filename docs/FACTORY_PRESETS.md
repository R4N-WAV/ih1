# Factory preset index

There are 28 read-only factory presets, defined in `core/src/presets/Presets.cpp`. Each one is a committed chain in processing order. Every preset is checked by `tests/test_presets.cpp`:

- it validates against the FX registry: types, parameter ids, ranges and whole steps;
- it loads as one atomic chain with exact order and values;
- it gives deterministic instance ids, so it always draws the same formation;
- it plays 3 s of sine and noise finite, below the safety ceiling, and not silent.

The preset arrows walk this order and wrap. User presets (category *User*) come after the factory list.


## Atmospheres

| Preset | Chain | Sound |
|---|---|---|
| **Slow Horizon** (`atm.slow_horizon`) | eq → reverb → width | A vast, darkened cathedral that blooms slowly behind the source and opens wide. |
| **Glass Cathedral** (`atm.glass_cathedral`) | shimmer → chorus | Octave and fifth shimmer rising out of a long tail, gently chorused. |
| **Under Ice** (`atm.under_ice`) | filter → convolution → spectral | Muffled and submerged: low-passed, bloomed through a strange space, edges blurred. |
| **Afterglow** (`atm.afterglow`) | vibrato → tape_echo → reverb → tape_colour | Worn tape repeats with a slow wobble, fading into a soft plate. |

## Voices

| Preset | Chain | Sound |
|---|---|---|
| **Choir of One** (`voc.choir_of_one`) | harmony → doubler → reverb | Three in-key harmonies, doubled and placed in a modest hall. |
| **Wide Double** (`voc.wide_double`) | doubler → eq → compressor | A natural four-voice double, a lifted presence band, gently levelled. |
| **Octave Ghost** (`voc.octave_ghost`) | pitch_shift → formant → reverse → reverb | Octaves above and below, darker formants, with backwards echoes trailing behind. |
| **Tuned Glide** (`voc.tuned_glide`) | pitch_correct → doubler → delay | Firm pitch correction in A minor, doubled, with a quiet quarter-note echo. |

## Fragments

| Preset | Chain | Sound |
|---|---|---|
| **Glass Grains** (`frg.glass_grains`) | granular → reverb | Octave-up grains scattered forwards and backwards into a long hall. |
| **Backwards Letters** (`frg.backwards_letters`) | reverse → spectral → delay | Phrases played back in reverse, smeared, then bounced across the stereo field. |
| **Frozen Breath** (`frg.frozen_breath`) | spectral → granular → width | A smeared spectral haze with sparse grains drifting through it, very wide. |
| **Loop Ghost** (`frg.loop_ghost`) | looper → granular → reverb | Captures two bars, then lets them dissolve an octave down into a small space. |

## Motion

| Preset | Chain | Sound |
|---|---|---|
| **Tidal Chorus** (`mot.tidal_chorus`) | chorus → tremolo | A slow, deep four-voice chorus that drifts from side to side on the half note. |
| **Jet Wash** (`mot.jet_wash`) | flanger → reverb | A slow, resonant through-zero sweep in a tight room. |
| **Spiral Stairs** (`mot.spiral_stairs`) | phaser → freq_shift → width | Ten-stage phasing into a frequency-shifted feedback loop that endlessly climbs. |
| **Leslie Room** (`mot.leslie_room`) | rotary → tape_colour → reverb | Slow rotary speaker with a little drive, warmed on tape, in a small room. |

## Rhythmic

| Preset | Chain | Sound |
|---|---|---|
| **Dotted Rain** (`rhy.dotted_rain`) | filter → delay → reverb | Dotted-eighth ping-pong repeats, thinned below and softened above, on a plate. |
| **Stutter Bloom** (`rhy.stutter_bloom`) | stutter → delay → reverb | Bar-wise sixteenth stutters that echo and bloom into space. |
| **Gated Pulse** (`rhy.gated_pulse`) | gate → filter → delay | Eighth-note pumping into a tempo-synced filter sweep and a quarter-note echo. |
| **Tape Triplets** (`rhy.tape_triplets`) | tape_echo → saturation | Three tape heads on quarter-note triplets, saturated like an old echo unit. |

## Tone & Destruction

| Preset | Chain | Sound |
|---|---|---|
| **Broken Transistor** (`ton.broken_transistor`) | amp → bitcrush → compressor | A cranked small combo, partly crushed, slammed by a fast compressor. |
| **Warm Console** (`ton.warm_console`) | eq → transient → compressor → tape_colour → exciter | Subtle polish: gentle shelves, a softened attack, opto levelling and console drive. |
| **Crushed Telephone** (`ton.crushed_telephone`) | filter → bitcrush → saturation → width | A narrow, resonant band, bit-reduced and clipped hard, folded to mono. |
| **Fold Space** (`ton.fold_space`) | saturation → ring_mod → reverb | Wavefolding and ring modulation, cut short by a gated burst of space. |

## Signature

| Preset | Chain | Sound |
|---|---|---|
| **Supercluster** (`sig.supercluster`) | chorus → reverse → resonator → reverb → width | IH1's own: chorus, reversed grains and tuned resonance gathering in a long hall. |
| **Machine Choir** (`sig.machine_choir`) | vocoder → chorus → reverb | A 32-band vocoder (played from MIDI notes, or a fixed C3) chorused into a hall. |
| **Doppler Drift** (`sig.doppler_drift`) | doppler → delay → reverb | The source circles the listener while modulated echoes trail behind it. |
| **Vocal Polish** (`sig.vocal_polish`) | deesser → eq → compressor → reverb | A clean starting point: de-essed, lifted, levelled and placed on a short plate. |

## Notes

- The Looper in *Loop Ghost* starts empty on every load. Recorded audio is never stored in presets or project state, by design.
- *Machine Choir* follows MIDI notes routed to IH1. Without MIDI it sings a fixed C3.
- These presets use effects that add 2048 samples of latency (Harmony, Pitch Shift, Formant Shift, Spectral Freeze), which is reported to the host: *Choir of One*, *Octave Ghost*, *Under Ice*, *Backwards Letters* and *Frozen Breath*.
