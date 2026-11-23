# V6 factory preset audit (Phase 3, prompt 5.1)

**Tool:** `tools/ih1_bank_audit.cpp` (`ih1_bank_audit [--md out.md] [id-filter]`). Every factory preset is rendered through
the real Engine + Session (Output Safety **off**: the preset is measured, not the output stage) on the first source
its tags name and on a vocal phrase (synthetic, deterministic sources from `core/src/presets/Audition.cpp`).

**What this is, and is not.** A measurement screen with stated rules, applied to all 245 presets (the 209 of V5, the
37 new vocal presets, minus Init). It is **not a listening test on real recordings**. Every verdict other than
Keep was opened by hand and acted on as described below. The owner's listening pass in FL Studio remains open
(`docs/FL_STUDIO_TEST_CHECKLIST.md`, V6 item 6).

## Measurements and rules

| Column | Meaning |
|---|---|
| Gain dB | output RMS vs input RMS, after the first second |
| Peak | sample peak, linear, Output Safety off (sources peak around -3 dBFS) |
| Class | audition class: how much the INPUT is transformed (Subtle / Moderate / Transformative) |
| Added dB | residual after the best-aligned input, 10 log10(1 - r^2): how much NEW content the preset adds (doubles, choirs and echoes keep the input, so Class alone undersells them) |
| Macros | macro knobs the recalled chain shows |
| Closest earlier | the earlier preset of the same category whose CHANGE (output minus latency-aligned input, per channel) is most alike, aligned by cross-correlation; residual in dB (lower = more alike) |

Rules, in order:
- **Remove:** not finite, near silent (< -30 dB), or no audible effect at all.
- **Improve:** level outside -15 .. +8 dB, or peak >= 2.0 (+6 dBFS, the factory test's criterion), or labelled Strong /
  Extreme while measuring Subtle and adding less than -12 dB.
- **Merge:** changes the sound like an earlier preset of the same category and source (residual < -12 dB on the vocal
  phrase **and** on the shared primary source, both channels).
- **Keep:** otherwise.

### Two likeness measures were tried and rejected (recorded so they are not reused)
1. A long-term spectral fingerprint (32 bands, mean level and motion). Every preset that keeps the dry voice shares
   the voice's spectrum, so an echo throw "matched" a polished lead within 0.6 dB. It produced 36 false "Merge"
   verdicts.
2. The residual of whole outputs. Two subtle presets both stay near the dry input, so they "matched" each other
   (*Hard / Soft* vs *Ghost Hats*). Comparing on the vocal phrase alone also misjudged drum presets: hat and kick
   processors barely act on a voice. The final measure compares what each preset changes, on its own source too, per
   channel (a stereo-only movement such as *Wurly Wave*'s pan is not cancelled by a mono sum).

## Result

**Before fixes:** 12 Improve, 1 Merge, 0 Remove (final measure). **After:** **245 Keep, 0 Improve, 0 Merge, 0 Remove.**

### Improve: what was wrong and what changed (identity kept, gain staging corrected)
| Preset | Found | Change | After |
|---|---|---|---|
| `v4.glass_shards` Glass Shards | peak 2.63 on drums | Spectral Shatter Out -6 dB, Transient Attack 60 -> 45 | -7.9 dB, peak 0.80 |
| `v4.ghost_break` Ghost Break | +10.2 dB, peak 4.00 | Transient Out -9 dB | +1.4 dB, peak 1.69 |
| `v4.formant_morph` Formant Glass | peak 2.10 | Spectral Graft Out -4 dB | -3.1 dB, peak 1.33 |
| `so.disintegrate` Disintegrate | -19.4 dB (partials dropped, level never restored) | Spectral Smear Out +10 dB | -9.4 dB |
| `v4.tidal_body` Tidal Body | -15.4 dB | Spectral Smear Out +9 dB | -6.4 dB |
| `v4.trailer_hit` Trailer Hit | +6.3 dB, peak 2.68 | Transient Attack 70 -> 50, Out -4 dB | -0.1 dB, peak 0.88 |
| `v5.snare_cathedral` Snare Cathedral | labelled Strong, added only -13 dB | Drum Space Mix 35 -> 50, Cloud Chamber Mix 25 -> 38 | added -8 dB |
| `v5.concrete_room` Concrete Room | +9.0 dB, peak 4.00 | Transient Out -8 dB | +1.0 dB, peak 1.62 |
| `v5.rubber_kit` Rubber Kit | +8.7 dB, peak 2.58 | Drum Crush Out -7 dB | +1.1 dB, peak 1.02 |
| `v5.broken_pocket` Broken Pocket | -19.4 dB (gate closed most of the time) | Depth 90 -> 80, Gate 45 -> 70 % | -13.8 dB, added -9 dB |
| `v5.offbeat_machinery` Offbeat Machinery | -17.5 dB | Depth 85 -> 70, Gate 40 -> 50 % | -12.7 dB |
| `v4.modal_rebuild` Modal Rebuild | labelled Extreme, added only -16 dB | Modal Mix 45 -> 70, Decay 1.2 -> 2 s | added -7 dB |

(The V5 factory test passed these presets because it measures with different sources and allows +9 dB; the stricter
V6 screen found them.)

### Merge
- `ss.smear_field` *Smear Field* vs `ss.glacier` *Glacier* (residual -12.5 dB: both erase the source into spectral
  smear). Reworked rather than removed, because the two have different intentions. Glacier is a slow melt in a
  cathedral; Smear Field is now a faster (Time 70 -> 40 %), low-weighted (Bias +30 -> -40 %) wide cloud at a normal
  level (Out +6 -> +11 dB: -9.9 -> -2.6 dB). After: residual -5.8 dB against Glacier.

### Remove
None. No preset was silent, non-finite or without an audible effect.

### Vocal / Voice (new, 37)
All Keep. The vocal bank also has its own gate in `tests/test_v6.cpp`:
- metadata;
- peak and level on a vocal phrase and on a sustained vocal;
- honest intensity labels by added content;
- every new processor used.

That gate found and fixed:
- Whisper's onset over-compensation;
- choir and megaphone gain staging;
- a stutter too weak for its label;
- two labels relaxed to match the measurement (*Last Word Echo* Subtle, *Rising Throw* Moderate).

Macros: 23 of 37 vocal presets get combination macros (Voice / Mouth / Throw / Halo / Bloom ...). All of them, with
the rest of the bank (202 macros), pass the "each macro audibly changes the sound on its own" test.

## Full table (final)

| Preset | Name | Category | Intensity | Source | Gain dB | Peak | Class | Added dB | Macros | Closest earlier (residual dB) | Verdict | Reason |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `ess.mix_glue` | Mix Glue | Init / Utility | Subtle | vocal phrase | +2.4 | 0.85 | Subtle | -10 | 0 |  | Keep |  |
| `ess.plate_vocal` | Velvet Plate | Init / Utility | Subtle | vocal phrase | +2.5 | 0.97 | Subtle | -8 | 0 | `ess.mix_glue` (-1.5) | Keep |  |
| `ess.clean_slap` | Clean Slap | Init / Utility | Subtle | vocal phrase | +0.8 | 0.86 | Subtle | -10 | 3 | `ess.mix_glue` (-2.1) | Keep |  |
| `ess.quarter_throw` | Quarter Throw | Init / Utility | Subtle | vocal phrase | +0.6 | 0.85 | Subtle | -12 | 3 | `ess.clean_slap` (-0.0) | Keep |  |
| `ton.warm_console` | Warm Console | Init / Utility | Subtle | vocal phrase | -0.7 | 0.63 | Subtle | -9 | 0 | `ess.mix_glue` (-0.5) | Keep |  |
| `ex.fractal_feedback` | Fractal Feedback | Signal Collapse | Strong | vocal phrase | +1.6 | 1.02 | Transformative | -0 | 0 |  | Keep |  |
| `mot.spiral_stairs` | Endless Stair | Signal Collapse | Strong | vocal phrase | +2.8 | 1.56 | Transformative | -0 | 0 | `ex.fractal_feedback` (-0.9) | Keep |  |
| `f3.dub_siren` | Dub Machine | Signal Collapse | Strong | vocal phrase | +1.1 | 0.47 | Transformative | -1 | 1 | `ex.fractal_feedback` (-0.3) | Keep |  |
| `f3.reverse_loop` | Mirror Loop | Signal Collapse | Strong | vocal phrase | +1.5 | 1.01 | Transformative | -0 | 0 | `ex.fractal_feedback` (-1.1) | Keep |  |
| `ex.ring_cascade` | Metal Cascade | Signal Collapse | Strong | vocal phrase | +0.1 | 1.14 | Transformative | -0 | 0 | `f3.reverse_loop` (-1.4) | Keep |  |
| `f3.spectral_regen` | Spectral Regen | Signal Collapse | Strong | vocal phrase | -3.3 | 0.65 | Transformative | -0 | 0 | `ex.ring_cascade` (-2.3) | Keep |  |
| `v4.signal_collapse` | Signal Collapse | Signal Collapse | Extreme | vocal phrase | +0.6 | 0.73 | Transformative | -0 | 3 | `f3.spectral_regen` (-1.3) | Keep |  |
| `v4.bit_cathedral` | Bit Cathedral | Signal Collapse | Extreme | vocal phrase | -0.9 | 0.54 | Transformative | -1 | 2 | `f3.spectral_regen` (-2.1) | Keep |  |
| `v4.frame_demolition` | Frame Demolition | Signal Collapse | Extreme | vocal phrase | -0.8 | 0.44 | Transformative | -0 | 0 | `f3.spectral_regen` (-1.4) | Keep |  |
| `v4.corrupted_broadcast` | Corrupted Broadcast | Signal Collapse | Strong | vocal phrase | -4.9 | 0.56 | Transformative | -0 | 0 | `f3.spectral_regen` (-3.3) | Keep |  |
| `v4.dead_air` | Dead Air Engine | Signal Collapse | Extreme | vocal phrase | +2.5 | 0.41 | Transformative | -0 | 2 | `f3.spectral_regen` (-1.1) | Keep |  |
| `v4.glass_shards` | Glass Shards | Signal Collapse | Extreme | drum loop | -7.9 | 0.80 | Transformative | -1 | 0 |  | Keep |  |
| `v4.subatomic` | Subatomic | Signal Collapse | Strong | drum loop | -2.2 | 0.47 | Transformative | -0 | 0 | `v4.glass_shards` (-2.9) | Keep |  |
| `v4.broken_clock` | Broken Clock | Signal Collapse | Extreme | vocal phrase | -1.1 | 0.61 | Transformative | -0 | 0 | `f3.spectral_regen` (-2.0) | Keep |  |
| `ss.glacier` | Glacier | Granular / Reconstruction | Strong | vocal phrase | -10.7 | 0.82 | Transformative | -1 | 0 |  | Keep |  |
| `ss.half_time_haze` | Molasses | Granular / Reconstruction | Strong | vocal phrase | -9.9 | 1.45 | Transformative | -0 | 0 | `ss.glacier` (-8.0) | Keep |  |
| `ss.tape_slow` | Taffy Pull | Granular / Reconstruction | Strong | vocal phrase | -6.9 | 1.75 | Transformative | -0 | 0 | `ss.glacier` (-6.5) | Keep |  |
| `ss.smear_field` | Smear Field | Granular / Reconstruction | Extreme | vocal phrase | -2.6 | 0.80 | Transformative | -0 | 0 | `ss.glacier` (-5.8) | Keep |  |
| `ss.quarter_drift` | Varispeed Sink | Granular / Reconstruction | Strong | vocal phrase | -0.9 | 0.58 | Transformative | -0 | 0 | `ss.glacier` (-2.2) | Keep |  |
| `s3.frozen_bloom` | Frozen Bloom | Granular / Reconstruction | Strong | vocal phrase | -12.5 | 0.41 | Transformative | -0 | 0 | `ss.glacier` (-9.2) | Keep |  |
| `gm.particle_drift` | Particle Drift | Granular / Reconstruction | Strong | vocal phrase | +5.5 | 1.25 | Transformative | -0 | 1 | `s3.frozen_bloom` (-1.4) | Keep |  |
| `gm.swarm` | Insect Choir | Granular / Reconstruction | Strong | vocal phrase | -1.5 | 0.62 | Transformative | -0 | 0 | `s3.frozen_bloom` (-3.4) | Keep |  |
| `frg.frozen_breath` | Frozen Breath | Granular / Reconstruction | Strong | vocal phrase | -6.5 | 1.24 | Transformative | -0 | 0 | `s3.frozen_bloom` (-8.2) | Keep |  |
| `gm.cascade` | Grain Cascade | Granular / Reconstruction | Strong | vocal phrase | +2.1 | 1.25 | Transformative | -0 | 0 | `s3.frozen_bloom` (-2.1) | Keep |  |
| `frg.loop_ghost` | Loop Ghost | Granular / Reconstruction | Strong | vocal phrase | +2.6 | 1.00 | Transformative | -0 | 2 | `s3.frozen_bloom` (-1.9) | Keep |  |
| `gm.wide_grains` | Stereo Rain | Granular / Reconstruction | Strong | drum loop | -1.3 | 0.98 | Transformative | -0 | 0 |  | Keep |  |
| `v4.pocket_archaeology` | Pocket Archaeology | Granular / Reconstruction | Strong | vocal phrase | +0.1 | 0.67 | Subtle | -8 | 0 | `s3.frozen_bloom` (-1.0) | Keep |  |
| `v4.ghost_break` | Ghost Break | Granular / Reconstruction | Strong | drum loop | +1.4 | 1.69 | Subtle | -5 | 0 | `gm.wide_grains` (-0.8) | Keep |  |
| `v4.borrowed_chorus` | Borrowed Chorus | Granular / Reconstruction | Strong | vocal phrase | +1.2 | 0.83 | Transformative | -1 | 2 | `s3.frozen_bloom` (-1.2) | Keep |  |
| `v4.velvet_fragments` | Velvet Fragments | Granular / Reconstruction | Moderate | vocal phrase | +2.4 | 1.04 | Moderate | -2 | 2 | `s3.frozen_bloom` (-0.3) | Keep |  |
| `v4.half_remembered` | Half-Remembered | Granular / Reconstruction | Strong | vocal phrase | -0.1 | 0.77 | Transformative | -0 | 0 | `s3.frozen_bloom` (-2.1) | Keep |  |
| `v4.sampler_jigsaw` | Sampler Jigsaw | Granular / Reconstruction | Strong | vocal phrase | -1.7 | 1.07 | Transformative | -1 | 0 | `s3.frozen_bloom` (-2.3) | Keep |  |
| `v4.dust_cloud` | Dust Cloud | Granular / Reconstruction | Strong | vocal phrase | -3.7 | 0.90 | Transformative | -0 | 2 | `s3.frozen_bloom` (-4.9) | Keep |  |
| `v4.time_mosaic` | Time Mosaic | Granular / Reconstruction | Extreme | vocal phrase | -1.2 | 0.79 | Transformative | -0 | 0 | `s3.frozen_bloom` (-3.0) | Keep |  |
| `v4.attack_ghosts` | Attack Ghosts | Granular / Reconstruction | Strong | drum loop | +0.7 | 0.84 | Transformative | -1 | 0 | `gm.wide_grains` (-0.5) | Keep |  |
| `v4.sustain_engine` | Sustain Engine | Granular / Reconstruction | Strong | drum loop | +6.4 | 1.12 | Transformative | -1 | 0 | `v4.attack_ghosts` (-0.0) | Keep |  |
| `v4.palindrome_tape` | Palindrome Tape | Granular / Reconstruction | Strong | vocal phrase | -0.8 | 0.60 | Transformative | -0 | 0 | `s3.frozen_bloom` (-2.3) | Keep |  |
| `gr.slice_retrigger` | Scatter Grid | Rhythm / Glitch / Stutter | Extreme | vocal phrase | -1.3 | 1.25 | Transformative | -0 | 0 |  | Keep |  |
| `r3.tape_stop_bars` | Brake Every Two | Rhythm / Glitch / Stutter | Strong | vocal phrase | -0.5 | 0.47 | Moderate | -2 | 0 | `gr.slice_retrigger` (-0.3) | Keep |  |
| `gr.backspin` | Backspin Bars | Rhythm / Glitch / Stutter | Strong | vocal phrase | -0.4 | 0.70 | Transformative | -1 | 0 | `gr.slice_retrigger` (-1.0) | Keep |  |
| `gr.buffer_roll` | Ratchet Rolls | Rhythm / Glitch / Stutter | Strong | drum loop | +3.1 | 0.88 | Transformative | -0 | 0 |  | Keep |  |
| `gr.halftime_chop` | Halftime Chop | Rhythm / Glitch / Stutter | Strong | vocal phrase | -2.4 | 0.66 | Transformative | -0 | 0 | `gr.slice_retrigger` (-1.7) | Keep |  |
| `rhy.stutter_bloom` | Stutter Bloom | Rhythm / Glitch / Stutter | Strong | vocal phrase | -2.4 | 0.66 | Transformative | -0 | 2 | `gr.halftime_chop` (-1.7) | Keep |  |
| `v4.false_downbeat` | False Downbeat | Rhythm / Glitch / Stutter | Strong | vocal phrase | +0.4 | 0.84 | Transformative | -0 | 0 | `gr.slice_retrigger` (-1.4) | Keep |  |
| `v4.pocket_swing` | Pocket Swing | Rhythm / Glitch / Stutter | Moderate | vocal phrase | -1.1 | 0.77 | Moderate | -2 | 0 | `gr.halftime_chop` (-0.9) | Keep |  |
| `v4.juke_grid` | Juke Grid | Rhythm / Glitch / Stutter | Strong | vocal phrase | -0.1 | 0.86 | Transformative | -0 | 0 | `gr.halftime_chop` (-1.5) | Keep |  |
| `v4.stop_start` | Stop / Start | Rhythm / Glitch / Stutter | Strong | vocal phrase | -0.5 | 0.69 | Transformative | -0 | 0 | `gr.halftime_chop` (-1.4) | Keep |  |
| `v4.reverse_roll` | Reverse Roll | Rhythm / Glitch / Stutter | Strong | vocal phrase | -0.5 | 0.71 | Moderate | -1 | 0 | `rhy.stutter_bloom` (-0.8) | Keep |  |
| `v4.mosaic_gate` | Mosaic Gate | Rhythm / Glitch / Stutter | Strong | vocal phrase | -5.0 | 0.74 | Transformative | -2 | 0 | `gr.halftime_chop` (-2.6) | Keep |  |
| `v4.clave_machine` | Clave Machine | Rhythm / Glitch / Stutter | Moderate | drum loop | +3.4 | 0.93 | Moderate | -3 | 0 | `gr.buffer_roll` (-0.7) | Keep |  |
| `v4.ratchet_vox` | Ratchet Vox | Rhythm / Glitch / Stutter | Strong | vocal phrase | -2.2 | 0.51 | Transformative | -0 | 0 | `v4.mosaic_gate` (-2.0) | Keep |  |
| `v4.irregular_heart` | Irregular Heart | Rhythm / Glitch / Stutter | Strong | drum loop | -7.0 | 0.71 | Subtle | -9 | 0 | `gr.buffer_roll` (-1.5) | Keep |  |
| `v4.backspin_cascade` | Backspin Cascade | Rhythm / Glitch / Stutter | Strong | vocal phrase | +1.2 | 1.00 | Subtle | -4 | 0 | `r3.tape_stop_bars` (-0.4) | Keep |  |
| `gm.octave_dust` | Octave Fracture | Pitch / Reverse / Harmonic | Strong | vocal phrase | +0.1 | 0.95 | Transformative | -0 | 2 |  | Keep |  |
| `vt.giant` | Sub Mutant | Pitch / Reverse / Harmonic | Extreme | vocal phrase | -1.9 | 0.31 | Transformative | -0 | 0 | `gm.octave_dust` (-1.3) | Keep |  |
| `p3.reverse_fifths` | Reverse Fifths | Pitch / Reverse / Harmonic | Strong | vocal phrase | +0.2 | 0.98 | Transformative | -0 | 2 | `gm.octave_dust` (-1.4) | Keep |  |
| `gm.minor_scatter` | Minor Swarm | Pitch / Reverse / Harmonic | Strong | vocal phrase | +0.3 | 0.81 | Transformative | -0 | 0 | `gm.octave_dust` (-1.4) | Keep |  |
| `p3.ring_mutant` | Ring Mutant | Pitch / Reverse / Harmonic | Strong | vocal phrase | -7.2 | 0.58 | Transformative | -0 | 0 | `gm.octave_dust` (-2.5) | Keep |  |
| `p3.glide_bend` | Tape Bend | Pitch / Reverse / Harmonic | Strong | vocal phrase | -0.9 | 0.60 | Transformative | -0 | 0 | `p3.ring_mutant` (-2.0) | Keep |  |
| `vt.whisper_cloud` | Whisper Cloud | Pitch / Reverse / Harmonic | Strong | vocal phrase | -12.2 | 0.21 | Transformative | -0 | 0 | `p3.ring_mutant` (-7.0) | Keep |  |
| `sig.machine_choir` | Machine Choir | Pitch / Reverse / Harmonic | Strong | vocal phrase | -0.6 | 0.86 | Transformative | -0 | 2 | `vt.whisper_cloud` (-3.1) | Keep |  |
| `vt.octave_halo` | Synthetic Double | Pitch / Reverse / Harmonic | Strong | vocal phrase | -2.6 | 0.64 | Transformative | -0 | 0 | `vt.whisper_cloud` (-4.2) | Keep |  |
| `vt.ring_robot` | Circuit Voice | Pitch / Reverse / Harmonic | Strong | vocal phrase | -1.3 | 0.56 | Transformative | -0 | 0 | `vt.whisper_cloud` (-2.0) | Keep |  |
| `voc.octave_ghost` | Formant Ghost | Pitch / Reverse / Harmonic | Strong | vocal phrase | +0.6 | 1.16 | Transformative | -0 | 0 | `vt.whisper_cloud` (-2.5) | Keep |  |
| `v3.vowel_chorus` | Vowel Swarm | Pitch / Reverse / Harmonic | Strong | vocal phrase | +1.1 | 1.20 | Transformative | -0 | 0 | `vt.whisper_cloud` (-2.6) | Keep |  |
| `v4.crystal_stairs` | Crystal Stairs | Pitch / Reverse / Harmonic | Strong | vocal phrase | +3.2 | 1.35 | Subtle | -4 | 2 | `vt.whisper_cloud` (-0.0) | Keep |  |
| `v4.fifth_cascade` | Fifth Cascade | Pitch / Reverse / Harmonic | Strong | guitar chord | +2.7 | 1.03 | Moderate | -4 | 2 |  | Keep |  |
| `v4.descent` | Descent | Pitch / Reverse / Harmonic | Strong | vocal phrase | +2.4 | 0.74 | Moderate | -4 | 0 | `vt.whisper_cloud` (-0.0) | Keep |  |
| `v4.bell_choir` | Bell Choir | Pitch / Reverse / Harmonic | Strong | vocal phrase | -4.5 | 0.35 | Transformative | -0 | 0 | `vt.whisper_cloud` (-5.3) | Keep |  |
| `v4.hollow_harmonics` | Hollow Harmonics | Pitch / Reverse / Harmonic | Moderate | bass phrase | -0.4 | 0.53 | Transformative | -1 | 0 |  | Keep |  |
| `v4.metal_shift` | Metal Shift | Pitch / Reverse / Harmonic | Strong | vocal phrase | -4.7 | 0.46 | Transformative | -0 | 0 | `vt.whisper_cloud` (-5.0) | Keep |  |
| `v4.octave_shadow` | Octave Shadow | Pitch / Reverse / Harmonic | Moderate | vocal phrase | +2.9 | 1.07 | Moderate | -3 | 2 | `vt.octave_halo` (-0.0) | Keep |  |
| `v4.climbing_taps` | Climbing Taps | Pitch / Reverse / Harmonic | Strong | vocal phrase | +1.3 | 0.90 | Moderate | -3 | 0 | `vt.whisper_cloud` (-0.3) | Keep |  |
| `v4.formant_morph` | Formant Glass | Pitch / Reverse / Harmonic | Strong | vocal phrase | -3.1 | 1.33 | Moderate | -3 | 1 | `vt.whisper_cloud` (-2.8) | Keep |  |
| `so.rising_echo` | Rising Echoes | Spectral / Frequency Space | Strong | vocal phrase | +1.1 | 0.86 | Transformative | -0 | 1 |  | Keep |  |
| `so.scatter_prism` | Prism Scatter | Spectral / Frequency Space | Strong | vocal phrase | +1.3 | 0.82 | Transformative | -0 | 1 | `so.rising_echo` (-2.2) | Keep |  |
| `so.partial_gate` | Partial Gate | Spectral / Frequency Space | Strong | vocal phrase | -0.3 | 1.80 | Transformative | -1 | 2 | `so.scatter_prism` (-0.4) | Keep |  |
| `so.disintegrate` | Disintegrate | Spectral / Frequency Space | Strong | vocal phrase | -9.4 | 0.90 | Transformative | -1 | 0 | `so.rising_echo` (-3.4) | Keep |  |
| `ht.fifth_resonance` | Fifth Resonance | Spectral / Frequency Space | Strong | vocal phrase | +1.6 | 0.74 | Transformative | -0 | 0 | `so.disintegrate` (-1.7) | Keep |  |
| `ht.crystal` | Glass Chord | Spectral / Frequency Space | Strong | vocal phrase | -1.4 | 0.41 | Transformative | -0 | 0 | `so.disintegrate` (-3.8) | Keep |  |
| `v4.shard_field` | Shard Field | Spectral / Frequency Space | Strong | piano / keys | -1.2 | 0.55 | Transformative | -1 | 0 |  | Keep |  |
| `v4.frequency_fold` | Frequency Fold | Spectral / Frequency Space | Extreme | vocal phrase | +0.1 | 1.36 | Transformative | -0 | 0 | `so.disintegrate` (-3.1) | Keep |  |
| `v4.graft_memory` | Graft Memory | Spectral / Frequency Space | Strong | vocal phrase | +0.2 | 1.92 | Subtle | -5 | 0 | `so.disintegrate` (-0.2) | Keep |  |
| `v4.cross_talk` | Cross Talk | Spectral / Frequency Space | Moderate | vocal phrase | -4.5 | 1.13 | Subtle | -8 | 0 | `so.disintegrate` (-6.3) | Keep |  |
| `v4.echo_imprint` | Echo Imprint | Spectral / Frequency Space | Strong | vocal phrase | -2.3 | 1.98 | Moderate | -2 | 0 | `v4.cross_talk` (-2.8) | Keep |  |
| `v4.prism_delay` | Prism Delay | Spectral / Frequency Space | Moderate | vocal phrase | -5.0 | 0.56 | Transformative | -1 | 0 | `v4.cross_talk` (-4.5) | Keep |  |
| `v4.partial_drift` | Partial Drift | Spectral / Frequency Space | Strong | vocal phrase | +3.0 | 1.15 | Transformative | -0 | 0 | `so.disintegrate` (-1.7) | Keep |  |
| `v4.laser_chirp` | Laser Chirp | Spectral / Frequency Space | Strong | drum loop | +0.6 | 1.07 | Transformative | -0 | 0 |  | Keep |  |
| `v4.spring_vortex` | Spring Vortex | Spectral / Frequency Space | Strong | vocal phrase | -0.2 | 0.98 | Transformative | -0 | 0 | `v4.cross_talk` (-2.0) | Keep |  |
| `cd.lofi_sampler` | Sampler Ghost | Dirt / Degradation | Strong | vocal phrase | -0.2 | 0.62 | Moderate | -6 | 0 |  | Keep |  |
| `g3.alias_grinder` | Alias Grinder | Dirt / Degradation | Extreme | vocal phrase | +2.5 | 0.92 | Moderate | -2 | 0 | `cd.lofi_sampler` (-1.0) | Keep |  |
| `cd.cassette` | Bin Cassette | Dirt / Degradation | Strong | vocal phrase | -7.9 | 0.36 | Transformative | -0 | 0 | `cd.lofi_sampler` (-1.5) | Keep |  |
| `ton.fold_space` | Folded Steel | Dirt / Degradation | Strong | drum loop | +4.3 | 1.95 | Transformative | -0 | 0 |  | Keep |  |
| `g3.splinter_hits` | Splinter Hits | Dirt / Degradation | Strong | drum loop | -2.4 | 1.07 | Moderate | -1 | 0 | `ton.fold_space` (-0.4) | Keep |  |
| `cd.blown_speaker` | Blown Radio | Dirt / Degradation | Extreme | vocal phrase | -0.6 | 0.22 | Transformative | -1 | 0 | `cd.cassette` (-0.7) | Keep |  |
| `g3.bitrot_echo` | Bitrot Echo | Dirt / Degradation | Strong | vocal phrase | +1.4 | 1.02 | Moderate | -4 | 2 | `cd.cassette` (-0.1) | Keep |  |
| `v4.tape_memory` | Tape Memory | Dirt / Degradation | Moderate | vocal phrase | -0.6 | 0.82 | Transformative | -0 | 0 | `cd.cassette` (-2.0) | Keep |  |
| `v4.vinyl_ghost` | Vinyl Ghost | Dirt / Degradation | Moderate | vocal phrase | +2.3 | 0.86 | Moderate | -4 | 0 | `cd.lofi_sampler` (-2.5) | Keep |  |
| `v4.friction_drive` | Friction Drive | Dirt / Degradation | Strong | bass phrase | +3.3 | 0.50 | Moderate | -2 | 0 |  | Keep |  |
| `v4.grit_imprint` | Grit Imprint | Dirt / Degradation | Strong | vocal phrase | -0.7 | 0.84 | Subtle | -6 | 0 | `cd.lofi_sampler` (-1.1) | Keep |  |
| `v4.static_storm` | Static Storm | Dirt / Degradation | Extreme | vocal phrase | +4.9 | 0.64 | Transformative | -1 | 0 | `cd.cassette` (-0.8) | Keep |  |
| `v4.cassette_choir` | Cassette Choir | Dirt / Degradation | Moderate | vocal phrase | -0.4 | 0.64 | Transformative | -1 | 0 | `cd.cassette` (-1.4) | Keep |  |
| `v4.dirty_room` | Dirty Room | Dirt / Degradation | Strong | drum loop | -5.2 | 1.15 | Subtle | -5 | 0 | `ton.fold_space` (-1.0) | Keep |  |
| `v4.silver_thread` | Silver Thread | Musical / Beautiful | Moderate | vocal phrase | +1.5 | 0.91 | Subtle | -6 | 2 |  | Keep |  |
| `v4.soft_focus` | Soft Focus | Musical / Beautiful | Subtle | vocal phrase | -2.4 | 0.58 | Subtle | -6 | 2 | `v4.silver_thread` (-0.0) | Keep |  |
| `v4.halo_chords` | Halo Chords | Musical / Beautiful | Moderate | guitar chord | -0.7 | 0.76 | Moderate | -5 | 3 |  | Keep |  |
| `v4.air_between_notes` | Air Between Notes | Musical / Beautiful | Subtle | vocal phrase | +0.5 | 0.76 | Subtle | -12 | 3 | `v4.soft_focus` (-0.2) | Keep |  |
| `v4.infinite_instrument` | Infinite Instrument | Musical / Beautiful | Strong | vocal phrase | +2.0 | 0.82 | Transformative | -0 | 2 | `v4.soft_focus` (-0.7) | Keep |  |
| `v4.memory_bloom` | Memory Bloom | Musical / Beautiful | Moderate | vocal phrase | +2.0 | 1.49 | Moderate | -2 | 0 | `v4.soft_focus` (-0.0) | Keep |  |
| `v4.glass_room` | Glass Room | Musical / Beautiful | Subtle | vocal phrase | -2.9 | 0.54 | Subtle | -9 | 2 | `v4.soft_focus` (-1.6) | Keep |  |
| `v4.warm_echo_hall` | Warm Echo Hall | Musical / Beautiful | Moderate | vocal phrase | +1.6 | 0.89 | Subtle | -6 | 3 | `v4.silver_thread` (-0.3) | Keep |  |
| `v4.bell_garden` | Bell Garden | Musical / Beautiful | Moderate | guitar chord | -2.8 | 0.58 | Subtle | -10 | 2 | `v4.halo_chords` (-0.8) | Keep |  |
| `v4.silk_doubles` | Silk Doubles | Musical / Beautiful | Subtle | vocal phrase | -2.2 | 0.62 | Subtle | -5 | 1 | `v4.glass_room` (-1.6) | Keep |  |
| `v4.lullaby_cloud` | Lullaby Cloud | Musical / Beautiful | Moderate | vocal phrase | +1.6 | 0.94 | Subtle | -7 | 2 | `v4.silver_thread` (-1.0) | Keep |  |
| `v4.velvet_motion` | Velvet Motion | Musical / Beautiful | Moderate | guitar chord | -4.5 | 0.43 | Subtle | -6 | 0 | `v4.bell_garden` (-2.6) | Keep |  |
| `v5.blue_hour` | Blue Hour | Musical / Beautiful | Subtle | vocal phrase | -4.6 | 0.45 | Subtle | -11 | 2 | `v4.glass_room` (-3.6) | Keep |  |
| `v5.soft_focus` | Haze Focus | Musical / Beautiful | Subtle | vocal phrase | +0.5 | 0.75 | Subtle | -15 | 0 | `v5.blue_hour` (-0.1) | Keep |  |
| `atm.glass_cathedral` | Glass Cathedral | Atmosphere / Cinematic / Drone | Strong | vocal phrase | -8.3 | 0.23 | Transformative | -1 | 0 |  | Keep |  |
| `atm.slow_bloom` | Reverse Bloom | Atmosphere / Cinematic / Drone | Strong | vocal phrase | -3.7 | 0.64 | Transformative | -0 | 0 | `atm.glass_cathedral` (-5.3) | Keep |  |
| `atm.deep_hollow` | Deep Hollow | Atmosphere / Cinematic / Drone | Strong | vocal phrase | +1.7 | 0.69 | Transformative | -0 | 1 | `atm.glass_cathedral` (-2.2) | Keep |  |
| `sig.doppler_drift` | Fly-By | Atmosphere / Cinematic / Drone | Strong | vocal phrase | -13.1 | 0.70 | Transformative | -0 | 0 | `atm.glass_cathedral` (-8.6) | Keep |  |
| `atm.under_ice` | Under Ice | Atmosphere / Cinematic / Drone | Strong | vocal phrase | -4.8 | 0.41 | Transformative | -0 | 2 | `sig.doppler_drift` (-5.6) | Keep |  |
| `c3.horizon_freeze` | Horizon Freeze | Atmosphere / Cinematic / Drone | Strong | vocal phrase | -0.2 | 0.55 | Transformative | -0 | 2 | `sig.doppler_drift` (-5.0) | Keep |  |
| `v4.monolith` | Monolith | Atmosphere / Cinematic / Drone | Extreme | vocal phrase | -1.4 | 0.53 | Moderate | -2 | 2 | `sig.doppler_drift` (-2.7) | Keep |  |
| `v4.reverse_dawn` | Reverse Dawn | Atmosphere / Cinematic / Drone | Strong | vocal phrase | +1.5 | 1.02 | Moderate | -2 | 2 | `atm.slow_bloom` (-2.6) | Keep |  |
| `v4.tidal_body` | Tidal Body | Atmosphere / Cinematic / Drone | Strong | vocal phrase | -6.4 | 0.70 | Transformative | -0 | 0 | `sig.doppler_drift` (-5.2) | Keep |  |
| `v4.trailer_hit` | Trailer Hit | Atmosphere / Cinematic / Drone | Extreme | vocal phrase | -0.1 | 0.88 | Moderate | -2 | 3 | `v4.monolith` (-1.7) | Keep |  |
| `v4.ghost_station` | Ghost Station | Atmosphere / Cinematic / Drone | Strong | vocal phrase | +4.0 | 1.00 | Transformative | -1 | 2 | `sig.doppler_drift` (-0.5) | Keep |  |
| `v4.slow_planet` | Slow Planet | Atmosphere / Cinematic / Drone | Strong | piano / keys | -1.6 | 0.71 | Transformative | -0 | 3 |  | Keep |  |
| `v4.resonant_sky` | Resonant Sky | Atmosphere / Cinematic / Drone | Moderate | vocal phrase | -1.2 | 0.58 | Subtle | -6 | 2 | `sig.doppler_drift` (-1.5) | Keep |  |
| `v4.underworld` | Underworld | Atmosphere / Cinematic / Drone | Extreme | vocal phrase | +0.4 | 0.49 | Transformative | -0 | 0 | `sig.doppler_drift` (-2.2) | Keep |  |
| `v5.soft_horizon` | Soft Horizon | Atmosphere / Cinematic / Drone | Moderate | vocal phrase | +0.5 | 0.73 | Subtle | -13 | 0 | `atm.slow_bloom` (-0.0) | Keep |  |
| `v5.pale_chamber` | Pale Chamber | Atmosphere / Cinematic / Drone | Subtle | vocal phrase | -3.4 | 0.50 | Subtle | -11 | 2 | `sig.doppler_drift` (-5.8) | Keep |  |
| `v5.velvet_drift` | Velvet Drift | Atmosphere / Cinematic / Drone | Moderate | guitar chord | -1.5 | 0.55 | Moderate | -5 | 2 |  | Keep |  |
| `v5.glass_bloom` | Glass Bloom | Atmosphere / Cinematic / Drone | Strong | vocal phrase | +1.8 | 0.82 | Subtle | -7 | 3 | `atm.under_ice` (-0.0) | Keep |  |
| `v5.frozen_orchard` | Frozen Orchard | Atmosphere / Cinematic / Drone | Strong | vocal phrase | -7.0 | 0.73 | Subtle | -7 | 2 | `sig.doppler_drift` (-7.8) | Keep |  |
| `v5.reverse_weather` | Reverse Weather | Atmosphere / Cinematic / Drone | Strong | vocal phrase | +1.7 | 0.99 | Subtle | -6 | 0 | `sig.doppler_drift` (-0.1) | Keep |  |
| `v5.deep_tide` | Deep Tide | Atmosphere / Cinematic / Drone | Moderate | bass phrase | +1.1 | 0.66 | Moderate | -3 | 0 |  | Keep |  |
| `v5.violet_relay` | Violet Relay | Atmosphere / Cinematic / Drone | Moderate | vocal phrase | +1.2 | 0.93 | Subtle | -9 | 3 | `v5.soft_horizon` (-0.0) | Keep |  |
| `v5.cloud_memory` | Cloud Memory | Atmosphere / Cinematic / Drone | Strong | vocal phrase | +0.9 | 0.78 | Moderate | -3 | 1 | `sig.doppler_drift` (-0.7) | Keep |  |
| `v5.small_room_huge_sky` | Small Room / Huge Sky | Atmosphere / Cinematic / Drone | Strong | vocal phrase | -2.1 | 0.54 | Subtle | -6 | 2 | `sig.doppler_drift` (-2.9) | Keep |  |
| `v5.suspended_bells` | Suspended Bells | Atmosphere / Cinematic / Drone | Strong | drum loop | -2.4 | 0.56 | Moderate | -6 | 2 |  | Keep |  |
| `v5.afterimage_choir` | Afterimage Choir | Atmosphere / Cinematic / Drone | Strong | vocal phrase | -1.4 | 0.66 | Subtle | -5 | 2 | `v4.resonant_sky` (-4.0) | Keep |  |
| `v5.dust_cathedral` | Dust Cathedral | Atmosphere / Cinematic / Drone | Strong | piano / keys | +2.9 | 1.09 | Moderate | -4 | 0 | `v4.slow_planet` (-0.0) | Keep |  |
| `v5.slow_glass` | Slow Glass | Atmosphere / Cinematic / Drone | Moderate | piano / keys | -3.5 | 0.51 | Subtle | -10 | 2 | `v4.slow_planet` (-2.3) | Keep |  |
| `v5.rain_in_reverse` | Rain in Reverse | Atmosphere / Cinematic / Drone | Strong | drum loop | +2.9 | 1.22 | Moderate | -3 | 0 | `v5.suspended_bells` (-0.0) | Keep |  |
| `v5.infinite_slope` | Infinite Slope | Atmosphere / Cinematic / Drone | Extreme | guitar chord | +1.9 | 0.91 | Moderate | -4 | 0 | `v5.velvet_drift` (-0.0) | Keep |  |
| `v5.empty_cinema` | Empty Cinema | Atmosphere / Cinematic / Drone | Moderate | vocal phrase | -2.1 | 0.58 | Subtle | -7 | 2 | `sig.doppler_drift` (-2.5) | Keep |  |
| `v5.swell_choir` | Swell Choir | Atmosphere / Cinematic / Drone | Strong | vocal phrase | +2.9 | 0.94 | Subtle | -5 | 0 | `v4.resonant_sky` (-1.9) | Keep |  |
| `v5.inhale` | Inhale | Atmosphere / Cinematic / Drone | Strong | vocal phrase | -1.9 | 0.58 | Subtle | -12 | 2 | `sig.doppler_drift` (-3.0) | Keep |  |
| `v5.pocket_machine` | Pocket Machine | Drums / Drum Bus | Subtle | drum loop | +0.2 | 0.71 | Subtle | -13 | 0 |  | Keep |  |
| `v5.snare_cathedral` | Snare Cathedral | Drums / Drum Bus | Strong | drum loop | -4.6 | 0.63 | Subtle | -8 | 2 | `v5.pocket_machine` (-1.1) | Keep |  |
| `v5.dust_break` | Dust Break | Drums / Drum Bus | Strong | drum loop | -0.3 | 0.69 | Moderate | -4 | 2 | `v5.pocket_machine` (-2.0) | Keep |  |
| `v5.ghost_hats` | Ghost Hats | Drums / Drum Bus | Moderate | drum loop | -0.1 | 0.74 | Subtle | -18 | 0 | `v5.pocket_machine` (-0.2) | Keep |  |
| `v5.concrete_room` | Concrete Room | Drums / Drum Bus | Moderate | drum loop | +1.0 | 1.62 | Subtle | -5 | 0 | `v5.pocket_machine` (-0.9) | Keep |  |
| `v5.rubber_kit` | Rubber Kit | Drums / Drum Bus | Strong | drum loop | +1.1 | 1.02 | Subtle | -10 | 3 | `v5.concrete_room` (-0.8) | Keep |  |
| `v5.half_life_break` | Half-Life Break | Drums / Drum Bus | Strong | drum loop | -0.7 | 1.00 | Transformative | -0 | 0 | `v5.snare_cathedral` (-1.6) | Keep |  |
| `v5.metal_skin` | Metal Skin | Drums / Drum Bus | Strong | drum loop | -2.1 | 0.75 | Moderate | -8 | 0 | `v5.snare_cathedral` (-1.3) | Keep |  |
| `v5.broken_pocket` | Broken Pocket | Drums / Drum Bus | Strong | drum loop | -13.8 | 0.31 | Subtle | -9 | 2 | `v5.snare_cathedral` (-5.3) | Keep |  |
| `v5.drum_orbit` | Drum Orbit | Drums / Drum Bus | Moderate | drum loop | -1.6 | 0.80 | Subtle | -12 | 3 | `v5.dust_break` (-0.5) | Keep |  |
| `v5.hard_soft` | Hard / Soft | Drums / Drum Bus | Moderate | drum loop | +3.7 | 1.02 | Subtle | -12 | 3 | `v5.broken_pocket` (-4.3) | Keep |  |
| `v5.salted_cymbals` | Salted Cymbals | Drums / Drum Bus | Strong | vocal phrase | -3.6 | 0.66 | Subtle | -12 | 0 |  | Keep |  |
| `v5.tape_room` | Tape Room | Drums / Drum Bus | Moderate | drum loop | -2.1 | 0.58 | Moderate | -1 | 0 | `v5.broken_pocket` (-2.6) | Keep |  |
| `v5.kick_lantern` | Kick Lantern | Drums / Drum Bus | Moderate | drum loop | -0.2 | 0.62 | Subtle | -12 | 0 | `v5.dust_break` (-1.2) | Keep |  |
| `v5.shattered_snare` | Shattered Snare | Drums / Drum Bus | Extreme | drum loop | +0.2 | 0.99 | Transformative | -0 | 0 | `v5.hard_soft` (-1.7) | Keep |  |
| `v5.deep_dub_kit` | Deep Dub Kit | Drums / Drum Bus | Strong | drum loop | +0.6 | 0.71 | Moderate | -4 | 2 | `v5.drum_orbit` (-0.3) | Keep |  |
| `v5.glass_hats` | Glass Hats | Drums / Drum Bus | Moderate | drum loop | -5.3 | 0.50 | Subtle | -15 | 0 | `v5.broken_pocket` (-7.4) | Keep |  |
| `v5.crushed_but_clear` | Crushed But Clear | Drums / Drum Bus | Extreme | drum loop | +3.5 | 0.95 | Subtle | -9 | 3 | `v5.glass_hats` (-2.0) | Keep |  |
| `v5.offbeat_machinery` | Offbeat Machinery | Drums / Drum Bus | Strong | drum loop | -12.7 | 0.24 | Subtle | -12 | 2 | `v5.broken_pocket` (-8.6) | Keep |  |
| `v5.last_hit_in_the_hall` | Last Hit in the Hall | Drums / Drum Bus | Strong | vocal phrase | -1.7 | 0.56 | Subtle | -10 | 2 | `v5.salted_cymbals` (-2.1) | Keep |  |
| `v5.gated_booth` | Gated Booth | Drums / Drum Bus | Strong | drum loop | +2.9 | 1.81 | Subtle | -7 | 0 | `v5.concrete_room` (-1.2) | Keep |  |
| `v5.amp_shimmer` | Amp Shimmer | Tremolo / Groove | Subtle | guitar chord | -1.6 | 0.64 | Subtle | -15 | 0 |  | Keep |  |
| `v5.slow_breath` | Slow Breath | Tremolo / Groove | Moderate | synth pad | -3.5 | 0.33 | Subtle | -14 | 0 |  | Keep |  |
| `v5.syncopated_chords` | Syncopated Chords | Tremolo / Groove | Strong | piano / keys | -2.8 | 0.88 | Moderate | -5 | 3 |  | Keep |  |
| `v5.chopped_voice` | Chopped Voice | Tremolo / Groove | Strong | vocal phrase | -8.2 | 0.55 | Moderate | -2 | 0 |  | Keep |  |
| `v5.rapid_gate` | Rapid Gate | Tremolo / Groove | Extreme | drum loop | -1.8 | 0.66 | Transformative | -5 | 0 |  | Keep |  |
| `v5.swing_shaker` | Swing Shaker | Tremolo / Groove | Moderate | drum loop | -2.1 | 0.68 | Subtle | -14 | 2 | `v5.rapid_gate` (-1.0) | Keep |  |
| `v5.left_right_percussion` | Left / Right Percussion | Tremolo / Groove | Moderate | drum loop | -5.8 | 0.63 | Subtle | -14 | 2 | `v5.swing_shaker` (-3.6) | Keep |  |
| `v5.polyrhythm_drift` | Polyrhythm Drift | Tremolo / Groove | Strong | drum loop | -5.0 | 0.68 | Subtle | -11 | 0 | `v5.left_right_percussion` (-5.7) | Keep |  |
| `v5.hit_opener` | Hit Opener | Tremolo / Groove | Moderate | drum loop | -2.7 | 0.57 | Subtle | -11 | 2 | `v5.polyrhythm_drift` (-4.5) | Keep |  |
| `v5.tremolo_into_space` | Tremolo into Space | Tremolo / Groove | Moderate | guitar chord | -2.4 | 0.71 | Subtle | -9 | 0 | `v5.amp_shimmer` (-1.6) | Keep |  |
| `v5.space_then_chop` | Space then Chop | Tremolo / Groove | Strong | vocal phrase | -3.5 | 0.61 | Subtle | -6 | 2 | `v5.chopped_voice` (-4.4) | Keep |  |
| `v5.tremolo_echo` | Pulse Echo | Tremolo / Groove | Moderate | vocal phrase | -2.7 | 0.91 | Moderate | -4 | 2 | `v5.chopped_voice` (-3.5) | Keep |  |
| `v5.low_pump` | Low Pump | Tremolo / Groove | Moderate | vocal phrase | -0.2 | 0.50 | Subtle | -14 | 0 | `v5.chopped_voice` (-0.7) | Keep |  |
| `v5.broken_machine_pulse` | Broken Machine Pulse | Tremolo / Groove | Extreme | drum loop | -3.8 | 0.54 | Transformative | -4 | 0 | `v5.polyrhythm_drift` (-3.9) | Keep |  |
| `v5.wurly_wave` | Wurly Wave | Tremolo / Groove | Subtle | guitar chord | -1.3 | 0.63 | Subtle | -17 | 0 | `v5.amp_shimmer` (-1.3) | Keep |  |
| `v5.gallop_pad` | Gallop Pad | Tremolo / Groove | Strong | synth pad | -2.5 | 0.43 | Subtle | -8 | 2 | `v5.slow_breath` (-1.4) | Keep |  |
| `v5.halftime_sway` | Half-time Sway | Tremolo / Groove | Moderate | vocal phrase | -5.4 | 0.51 | Subtle | -7 | 0 | `v5.chopped_voice` (-3.9) | Keep |  |
| `v6.lead_polish` | Lead Polish | Vocal / Voice | Subtle | vocal phrase | +0.5 | 0.85 | Subtle | -9 | 2 |  | Keep |  |
| `v6.bright_pop_lead` | Bright Pop Lead | Vocal / Voice | Moderate | vocal phrase | -1.9 | 0.97 | Transformative | -0 | 1 | `v6.lead_polish` (-0.4) | Keep |  |
| `v6.warm_tape_lead` | Warm Tape Lead | Vocal / Voice | Moderate | vocal phrase | +1.3 | 0.80 | Subtle | -7 | 3 | `v6.bright_pop_lead` (-0.5) | Keep |  |
| `v6.close_confession` | Close Confession | Vocal / Voice | Subtle | vocal phrase | -2.9 | 0.46 | Subtle | -11 | 0 | `v6.bright_pop_lead` (-2.1) | Keep |  |
| `v6.slapback_booth` | Slapback Booth | Vocal / Voice | Moderate | vocal phrase | -0.5 | 0.93 | Subtle | -9 | 2 | `v6.bright_pop_lead` (-0.9) | Keep |  |
| `v6.breath_double` | Breath Double | Vocal / Voice | Moderate | vocal phrase | -6.0 | 0.49 | Subtle | -7 | 2 | `v6.close_confession` (-4.1) | Keep |  |
| `v6.wide_double` | Wide Double | Vocal / Voice | Moderate | vocal phrase | -2.2 | 0.63 | Moderate | -4 | 0 | `v6.breath_double` (-2.8) | Keep |  |
| `v6.micro_stack` | Micro Stack | Vocal / Voice | Moderate | vocal phrase | -5.3 | 0.43 | Subtle | -6 | 0 | `v6.breath_double` (-6.7) | Keep |  |
| `v6.octave_wall` | Octave Wall | Vocal / Voice | Strong | vocal phrase | -1.5 | 0.66 | Subtle | -7 | 0 | `v6.breath_double` (-2.2) | Keep |  |
| `v6.thirds_choir` | Thirds Choir | Vocal / Voice | Moderate | vocal phrase | +0.3 | 0.90 | Subtle | -4 | 2 | `v6.breath_double` (-0.5) | Keep |  |
| `v6.cathedral_choir` | Cathedral Choir | Vocal / Voice | Strong | vocal phrase | +2.0 | 0.96 | Subtle | -5 | 2 | `v6.micro_stack` (-0.0) | Keep |  |
| `v6.fifth_angels` | Fifth Angels | Vocal / Voice | Strong | vocal phrase | +0.2 | 0.76 | Subtle | -5 | 1 | `v6.breath_double` (-0.5) | Keep |  |
| `v6.midi_choir` | MIDI Choir | Vocal / Voice | Strong | vocal phrase | +0.6 | 0.73 | Subtle | -10 | 2 | `v6.thirds_choir` (-0.8) | Keep |  |
| `v6.ether_veil` | Ether Veil | Vocal / Voice | Strong | vocal phrase | -0.6 | 0.80 | Subtle | -5 | 4 | `v6.fifth_angels` (-2.3) | Keep |  |
| `v6.dream_halo` | Dream Halo | Vocal / Voice | Strong | vocal phrase | -0.7 | 0.68 | Subtle | -6 | 3 | `v6.breath_double` (-1.0) | Keep |  |
| `v6.distant_choir` | Distant Choir | Vocal / Voice | Strong | vocal phrase | -1.0 | 0.60 | Transformative | -1 | 2 | `v6.breath_double` (-1.8) | Keep |  |
| `v6.reverse_vox_swell` | Reverse Vox Swell | Vocal / Voice | Strong | vocal phrase | +0.1 | 0.73 | Subtle | -7 | 1 | `v6.breath_double` (-0.4) | Keep |  |
| `v6.falling_throw` | Falling Throw | Vocal / Voice | Moderate | vocal phrase | +0.4 | 0.78 | Subtle | -14 | 2 | `v6.warm_tape_lead` (-1.2) | Keep |  |
| `v6.last_word_echo` | Last Word Echo | Vocal / Voice | Subtle | vocal phrase | -0.8 | 0.63 | Subtle | -19 | 2 | `v6.breath_double` (-2.3) | Keep |  |
| `v6.rising_throw` | Rising Throw | Vocal / Voice | Moderate | vocal phrase | +1.0 | 0.85 | Subtle | -9 | 2 | `v6.ether_veil` (-2.0) | Keep |  |
| `v6.vowel_cloud` | Vowel Cloud | Vocal / Voice | Strong | vocal phrase | +0.3 | 0.79 | Subtle | -5 | 2 | `v6.breath_double` (-0.5) | Keep |  |
| `v6.held_exhale` | Held Exhale | Vocal / Voice | Strong | vocal phrase | -0.4 | 1.15 | Moderate | -3 | 2 | `v6.breath_double` (-2.0) | Keep |  |
| `v6.syllable_stutter` | Syllable Stutter | Vocal / Voice | Strong | vocal phrase | -1.1 | 0.54 | Moderate | -4 | 0 | `v6.breath_double` (-1.4) | Keep |  |
| `v6.vox_chop_glitch` | Vox Chop Glitch | Vocal / Voice | Extreme | vocal phrase | -0.8 | 0.69 | Transformative | -0 | 0 | `v6.breath_double` (-2.9) | Keep |  |
| `v6.reverse_chop` | Reverse Chop | Vocal / Voice | Strong | vocal phrase | +0.4 | 0.88 | Moderate | -3 | 0 | `v6.breath_double` (-1.2) | Keep |  |
| `v6.talkbox_funk` | Talkbox Funk | Vocal / Voice | Strong | vocal phrase | -1.7 | 0.44 | Transformative | -1 | 0 | `v6.breath_double` (-2.2) | Keep |  |
| `v6.vowel_sequencer` | Vowel Sequencer | Vocal / Voice | Strong | vocal phrase | +0.6 | 0.85 | Transformative | -0 | 0 | `v6.talkbox_funk` (-1.3) | Keep |  |
| `v6.giant_mouth` | Giant Mouth | Vocal / Voice | Strong | vocal phrase | -4.9 | 0.46 | Transformative | -1 | 2 | `v6.breath_double` (-3.5) | Keep |  |
| `v6.tiny_mouth` | Tiny Mouth | Vocal / Voice | Moderate | vocal phrase | -1.0 | 0.93 | Transformative | -0 | 0 | `v6.breath_double` (-2.9) | Keep |  |
| `v6.robot_monotone` | Robot Monotone | Vocal / Voice | Strong | vocal phrase | +0.9 | 1.19 | Transformative | -0 | 1 | `v6.breath_double` (-1.9) | Keep |  |
| `v6.stepped_robot` | Stepped Robot | Vocal / Voice | Strong | vocal phrase | -1.6 | 0.53 | Transformative | -0 | 1 | `v6.breath_double` (-3.1) | Keep |  |
| `v6.compu_voice` | Compu-Voice | Vocal / Voice | Extreme | vocal phrase | -4.9 | 0.47 | Moderate | -2 | 0 | `v6.breath_double` (-3.4) | Keep |  |
| `v6.megaphone_riot` | Megaphone Riot | Vocal / Voice | Strong | vocal phrase | +2.3 | 0.33 | Transformative | -0 | 0 | `v6.breath_double` (-1.1) | Keep |  |
| `v6.industrial_scream` | Industrial Scream | Vocal / Voice | Extreme | vocal phrase | -2.9 | 0.36 | Transformative | -0 | 3 | `v6.breath_double` (-2.2) | Keep |  |
| `v6.ghost_whisper` | Ghost Whisper | Vocal / Voice | Strong | vocal phrase | -1.3 | 1.54 | Transformative | -0 | 3 | `v6.breath_double` (-11.0) | Keep |  |
| `v6.radio_voice` | Radio Voice | Vocal / Voice | Moderate | vocal phrase | -0.2 | 0.35 | Transformative | -1 | 0 | `v6.breath_double` (-2.3) | Keep |  |
| `v6.adlib_spiral` | Adlib Spiral | Vocal / Voice | Strong | vocal phrase | -3.1 | 0.52 | Subtle | -7 | 0 | `v6.breath_double` (-4.4) | Keep |  |
| `ex.inverted_world` | Inverted World | Experimental / Hybrid | Strong | vocal phrase | -4.1 | 0.31 | Transformative | -0 | 0 |  | Keep |  |
| `h3.machine_dream` | Machine Dream | Experimental / Hybrid | Strong | vocal phrase | -6.6 | 0.55 | Transformative | -0 | 0 | `ex.inverted_world` (-3.3) | Keep |  |
| `h3.broken_tape_choir` | Broken Tape Choir | Experimental / Hybrid | Strong | vocal phrase | +2.0 | 0.76 | Transformative | -0 | 0 | `h3.machine_dream` (-1.2) | Keep |  |
| `h3.particle_engine` | Particle Accelerator | Experimental / Hybrid | Strong | drum loop | -2.4 | 0.70 | Transformative | -0 | 0 |  | Keep |  |
| `h3.rubber_room` | Rubber Room | Experimental / Hybrid | Strong | vocal phrase | -4.4 | 0.29 | Transformative | -0 | 0 | `ex.inverted_world` (-3.3) | Keep |  |
| `h3.tidal_memory` | Tidal Memory | Experimental / Hybrid | Strong | vocal phrase | -1.7 | 0.74 | Transformative | -0 | 1 | `h3.machine_dream` (-2.6) | Keep |  |
| `v4.modal_rebuild` | Modal Rebuild | Experimental / Hybrid | Extreme | drum loop | -9.3 | 0.30 | Moderate | -7 | 0 | `h3.particle_engine` (-3.7) | Keep |  |
| `v4.graft_cascade` | Graft Cascade | Experimental / Hybrid | Strong | vocal phrase | +1.4 | 1.67 | Subtle | -4 | 0 | `h3.machine_dream` (-0.0) | Keep |  |
| `v4.split_world` | Split World | Experimental / Hybrid | Extreme | vocal phrase | -0.5 | 0.64 | Transformative | -0 | 0 | `h3.machine_dream` (-1.8) | Keep |  |
| `v4.texture_reactor` | Texture Reactor | Experimental / Hybrid | Strong | vocal phrase | -1.0 | 0.66 | Moderate | -2 | 0 | `ex.inverted_world` (-0.9) | Keep |  |
| `v4.echo_lattice` | Echo Lattice | Experimental / Hybrid | Strong | guitar chord | +0.4 | 0.70 | Transformative | -1 | 0 |  | Keep |  |
| `v4.partial_glitch` | Partial Glitch | Experimental / Hybrid | Extreme | vocal phrase | -3.8 | 0.38 | Transformative | -0 | 0 | `h3.machine_dream` (-3.1) | Keep |  |
| `v4.dust_choir` | Dust Choir | Experimental / Hybrid | Strong | vocal phrase | +7.3 | 1.37 | Transformative | -0 | 0 | `h3.machine_dream` (-0.7) | Keep |  |
| `v5.distant_machinery` | Distant Machinery | Experimental / Hybrid | Strong | drum loop | +0.6 | 0.78 | Transformative | -3 | 2 | `v4.modal_rebuild` (-0.4) | Keep |  |
| `v5.hit_melody_kit` | Melodic Kit | Experimental / Hybrid | Strong | drum loop | -6.6 | 0.46 | Moderate | -7 | 1 | `v4.modal_rebuild` (-7.1) | Keep |  |

245 presets: 245 Keep, 0 Improve, 0 Merge, 0 Remove
