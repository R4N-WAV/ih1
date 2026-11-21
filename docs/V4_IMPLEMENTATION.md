# IH1 V4 (feedback round 4): what changed, why, and the evidence

Version **0.4.0** (test build, round 4). This pass implements `IH1_V4_FEEDBACK_MASTER_PROMPT.md` with
`IH1_V4_RESEARCH_SUPPLEMENT.md`, the original reference video, the UI sketch and the new still
`image(20261121-190430).png`. Open items: `docs/KNOWN_ISSUES.md` (V4 section). FL Studio checklist:
`docs/FL_STUDIO_TEST_CHECKLIST.md`.

## 0. Starting point and inputs

| Item | Finding |
|---|---|
| Branch | `claude/keen-edison-jiahki` was at `0079915` (V1). The V3 work (0.3.0) was on `claude/amazing-noether-7iiyjr`, a strict descendant, so the branch was fast-forwarded to `92a9ff4`. Nothing was discarded. |
| Build | CMake 3.28, GCC 13, Ninja, JUCE 9.0.5 (pinned), doctest 2.4.11, OpenGL 3.2 core (GLSL 1.50) with a software fallback, Inno Setup 6 in CI. |
| Baseline | V3 tip built in a separate worktree: unit tests **130 / 130**. |
| Prior prompts | `IH1_V3_FEEDBACK_MASTER_PROMPT.md`, `IH1_V2_*` and `IH1_V2_1_*` are not in the repository and were not supplied. Their active requirements were taken from `docs/V3_IMPLEMENTATION.md`, `docs/V2_1_IMPLEMENTATION.md`, `docs/KNOWN_ISSUES.md` and `docs/source/` (V1 briefs). |
| Visual references | Opened in this session. The uploaded video is byte-identical to `references/ih1_visual_reference.mp4` (md5 `b8e5df8f...`). The still and the video were compared against renders (section 5). |
| KADAL | **Not searched in this session.** Attaching `R4N-WAV/Kadal` was refused by the session's permission policy. KADAL material was reused only through the paths already recorded in `docs/V3_IMPLEMENTATION.md` section 10 and `docs/KADAL_RESEARCH_MAP.md` (listed in section 10 below). Nothing was added to KADAL. |
| FL Studio / Windows / GPU | Not available here. Everything was validated on Linux (Mesa llvmpipe). See section 9. |

### Requirements ledger (still-active inherited requirements, status after V4)

| Requirement | Source | Status | Where | Regression guard |
|---|---|---|---|---|
| Committed chain = audio truth; staged plays after it | V1 | kept | `Engine`, `Session` | `test_engine`, `test_v3` null test |
| Committed values immutable; reorder / delete only | V1 | kept; **bypass and macros never touch the values** | `CommittedInstance` (no setters) | `test_v4` bypass + macro tests |
| None is a real staged state; recall lands on None | V3 | kept; **None is now position 0 of the carousel ring** | `Carousel`, `Session::step` | `test_v4`, `ih1_uitest` V4 checks |
| Drafts cached until RESET; Init resets everything | V3 | kept; Init also clears macros | `Session::initialise` | `test_v3` Init |
| Clear FX Chain (discreet, confirmed) | V2.1 | kept; also clears macros | `Session::clearChain` | `ih1_uitest` |
| Truthful Full / Low / Zero latency | V2.1 | kept; the three STFT FX are honest Low (512) in Zero mode | `Registry::annotate` | `ih1_hosttest` (reported == measured) |
| Output Safety (Guard / True Peak) | V3 | kept | `OutputSafety` | `ih1_hosttest` |
| Accepted FX palette | V2.1/V3 | kept: the registry is append-only, so no tier moved | `Palette.cpp` | palette test |
| Commit transfer lifecycle | V3 | kept | `Formation::Life` | `test_formation`, `test_field_volume` |
| Branded installer + ZIP, correct version | V3 | 0.4.0, round 4, dark wizard | `installer/`, CI | CI ZIP check |

## 1. Diagnosis (reproduced before fixing)

| Feedback | Symptom | Root cause found | Fix | Proof |
|---|---|---|---|---|
| 4 (P0) | Clicks in chopping FX | Measured with the new registry-wide `tools/ih1_click_audit` (section 4). **Glitch Scatter**: leaving a Gate step crossfaded from a signal at full level that had been silent; the lo-fi stage switched per block and its reducer restarted cold. **Stutter**: pitched repeats `fmod`-wrapped the read inside a slice; a new trigger replaced an audible repeat mid-level; linear slice corners. **Time Warp**: a stale "old source = live" flag made a Backspin / Tape Stop release swap instantly; Keep Pitch switched hard at 1x; the scrub head read the empty ring before the first sample. **Reverse**: chunk length / pitch changes moved the read point mid-chunk; the host-grid snap could land inside the fade-in. **Gate / Tremolo**: instant gain edges (Pump drop, Square / Ramp shapes, 0.05 ms attack). **Grain Engine**: the grain-delay tap jumped with its control. | Per-effect fixes (section 4), a generic **parameter glide** and an engine **discrete-switch dip** | Static settings, 30 random cases per FX: **59 clicking cases -> 2** (V3 vs V4, identical detector) |
| 3 | None vanishes when scrolling | `None` was not in the carousel's item list: an overlay drawn over a skipped centre row; stepping from it jumped to a remembered position | `None` is logical position 0 of the same ring (wheel, arrows, keys, clicks, menu) | `test_v4` ring test; `ih1_uitest` wheel through None both ways, 12/12 rapid steps |
| 5 | FX tree: centres per FX, few strands, fog, dull, squares, black | The V3 volume lit a **dipole per FX** with a defined core (`kCoreGain`) and joined neighbours with link sheets. Squares: adaptive quality dropped the march to **a quarter resolution** with per-texel jitter, magnified 4x by bilinear filtering; the software fallback ran at a fifth. Background: rays outside the bound were exactly black by design. | New continuous-medium model (section 5); GL reconstruction fixes | Renders `docs/img/v4_*`; `test_field_volume` (V4) |
| 6 | Clicking a committed name should bypass | No per-instance enable state existed | `CommittedInstance::enabled` + per-slot engine ramp | `test_v4`, `ih1_uitest` |
| 1 | Preset bank too small | 67 presets | 148 presets, metadata, macros | `test_presets`, macro safety test |
| 2 | Ten new FX | 45 FX | 55 FX | `test_v4`, contract suite |
| 8 | FX relationships | No chain analysis or modulation | Per-stage analysis telemetry; preset macros as shared, coordinated modulation; explicit FX-to-FX links deferred (section 7) | `test_v4` |

## 2. Workflow and state

- **Per-instance bypass (Phase 6).** `CommittedInstance::enabled()` is a separate boolean; `Session::setEnabled /
  toggleEnabled` rebuild the program with the same processors, which the engine installs **without an output fade**
  (flag-only programs). Each slot ramps between its processed output and a **latency-matched copy of its input** over
  15 ms, so bypassing a spectral FX never shifts the chain timing. **Time & Space** effects (and Echo Matrix, Reverse
  Pitch Echo, Spectral Delay) stop receiving new input but let their tail ring; every other effect switches off with
  the crossfade and keeps running underneath, so re-enabling is seamless. A bypassed tail that decays below -100 dBFS
  for 1.5 s is parked (no CPU). The flag is saved (`"enabled"`) and restored; the strip draws a filled mark in the FX
  colour (enabled) or a hollow grey mark and grey text (bypassed), and the formation greys and dims that stretch of
  the field without breaking it. A click toggles; a drag still reorders / deletes. Tooltip explains both.
- **None (Phase 3).** One index model for wheel, arrows, keys, neighbour clicks and type-to-jump; the right-click
  `None` row uses the same `onSelect(-1)`. `Session::step` uses the same ring.
- **Preset macros (Phase 1.5).** A macro is a modulation source owned by the recalled chain: each target adds
  `depth x value` to one continuous parameter of one instance at runtime, clamped to its range, smoothed (40 ms per
  block plus the parameter glide). The committed values never change and macro 0 is the chain exactly as stored, so
  invariant 4 holds. Four host parameters `pmacro_1..4` (appended, version hint 3); every recall returns them to 0.
  The editor shows only the macros the recalled preset defines, captioned with their names, beside IN / MIX / OUT.
  Targets follow instance ids through reorder; deleting an instance drops its targets; Clear / Init clear them.
- **State.** The document stays `version 1` (older builds still open V4 projects: all FX enabled, no macros) and gains
  `"schema": 4`, per-instance `"enabled"` and chain `"macros"`. V3 documents load unchanged.

## 3. Ten new processors (Phase 2)

Inventory comparison (45 existing FX, `tools/fx_dump`): candidates already present in equivalent form were **not**
re-added: Spectral Delay (V3), Tape Splice / Varispeed = Time Warp (V3), Wavefold = Fold & Crush (V3), Sideband /
Frequency Shift = Frequency Shifter + Ring Modulator, Spectral Freeze = Spectral Freeze (V1) / Spectral Smear hold,
Spectral Gate = Spectral Smear's gate. Micro-Loop Scrambler overlaps Glitch Scatter / Stutter in vocabulary only; the
Slice Shuffler's state model (whole-bar capture + deterministic permutation) is different and is documented as such.

| Effect (id) | Category | DSP | Distinct from | Latency | CPU class |
|---|---|---|---|---|---|
| Slice Shuffler (`slice_shuffle`) | Rhythm & Glitch | captures the previous 1 beat - 2 bars; replays its 4-32 slices in a permutation (reverse order, seeded shuffle, strides, palindrome, pairs, random walk, stutter), per-slice reverse / sampler pitch / gate / swing; 2-voice crossfaded playback, latched per-voice edge fades | Glitch Scatter (per-step probabilistic actions on the live stream) | 0 | 1 |
| Attack / Body (`transient_split`) | Experimental | fast/slow envelope transient mask splits the signal into two layers that sum back **exactly**; attack delay + repeats (flams, rolls); body Reversed (per-voice latched chunks) / Held (frozen grains between hits) / Smeared (diffuser) | Transient Shaper (gain only) | 0 | 1 |
| Echo Matrix (`echo_matrix`) | Time & Space | 8-tap step grid (8 patterns), per-tap level / pan / darkening / pitch (rotating two-head shifter), swing, cycle feedback through filter + saturator, "pitch in loop" | Delay (fixed tap patterns, no per-tap pitch) | 0 | 2 |
| Reverse Pitch Echo (`reverse_echo`) | Pitch & Vocal | overlapping Hann grains read backwards (or forwards / alternating) at a pitch ratio inside a filtered, soft-limited feedback loop: each pass is shifted again (cascades) | Reverse (no pitch in the loop), Pitch Shift (not reversed) | 0 | 1 |
| Dispersion (`dispersion`) | Modulation | up to 64 TPT second-order all-passes spread over a band (curve packs them low or high), slow motion, bounded feedback; pure phase at 100 % | Phaser (few stages, dry mix notches) | 0 | 2 |
| Modal Resonator (`modal`) | Spectral | up to 24 two-pole modes with material ratio sets (harmonic, bar, bell, glass, plate, membrane), inharmonicity stretch, damping by mode, transient / input excitation, a +9 dB resonance guard | Resonator (tuned comb strings in chords) | 0 | 2 |
| Texture Imprint (`texture`) | Tone & Character | source-following dust / crackle / static bursts / friction (input-modulated band noise) / grit (S&H noise AM), seeded, presence-gated (silence stays silent) | none | 0 | 1 |
| Spectral Shatter (`spectral_shatter`) | Spectral | STFT shards (8-32 log bands) moved to other bands by integer-bin translation with the exact N/4-hop phase rotation; per-shard freeze / gate; map changes crossfaded per frame; per-frame step timing (block-size independent) | Spectral Smear / Delay (no frequency remapping) | 1024 (Low 512) | 3 |
| Partial Recomposer (`partial_recompose`) | Pitch & Vocal | peak / region tracking (shared `SpectralAnalysis`), each partial moved by its own ratio (stretch around the lowest strong partial, Hz shift), identity phase locking, odd / even balance, alternate-partial panning, partial sustain, residual kept in place | Pitch Shift / Harmony (one ratio for all) | 2048 (Low 512) | 3 |
| Spectral Graft (`spectral_graft`) | Spectral | one-input cross-synthesis, honestly named: the live spectral envelope is replaced (Amount) by a held moment (waits for a settled window), an echo of N ms ago, or the opposite channel; shape shift, smoothing, level keeping | Vocoder (filter bank + synth carrier) | 1024 (Low 512) | 3 |

Each has a behaviour test in `tests/test_v4.cpp`, passes the registry contract suite (silence, finiteness,
block-size independence, determinism, reset, sample-rate change) and appears in at least two factory presets
(most in four or more).

## 4. Click-free chopping (Phase 4)

**Harness** `tools/ih1_click_audit [cases] [seconds] [ratioDb] [types...]`. Smooth inputs (110 / 440 Hz, -6 dBFS,
stereo phase offset, a DC offset in a third of the cases, the engine's 12 ms fresh-input ramp), random settings per
case, random tempo, a reset mid-run, and (moving mode) continuous parameters re-randomised every 0.35-0.9 s. The
output is high-passed at 6 kHz in 1 ms windows. A click must be (1) > 26 dB above the local +-60 ms median, (2) > -50
dB relative to the local signal power (calibrated: a hard cut measures -20..-30 dB, a 0.5 ms raised-cosine fade
-57..-63 dB, a 1 ms fade -71 dB), (3) short (<= 4 ms) and (4) isolated (the level falls back within 20 ms). Long or
sustained elevations are counted as texture (a lo-fi step, a scrub), not clicks. `IH1_CLICK_VERBOSE`, `_ONLY`,
`_SET`, `_DUMP` isolate mechanisms.

**Results** (30 cases x 4 s per FX). V3 = the unmodified 0.3.0 tip built in a worktree, same detector.

RESULTS_TABLE_PLACEHOLDER

Bitcrush / Degrade flags are the deliberate staircase of very low bit depths on a slow sine (the effect's sound,
not a splice); Grain Engine's remaining flags have its deliberate Feedback Rate sample-and-hold active (V3 note).

**Generic layers added.**
1. `FxBase` **parameter glide**: a non-snapped change of a continuous parameter glides over 30 ms while the effect
   runs in 32-sample sub-blocks (MIDI-driven effects excepted); discrete parameters switch.
2. Engine **discrete-switch dip**: a discrete change of the staged effect (mode, pattern, sync...) is applied under a
   6 ms raised-cosine dip of the staged wet signal into its own latency-aligned input (`test_v4`).
3. Equal-power crossfades on a raised-cosine time base (zero slope at both ends) in Glitch Scatter and Time Warp.
4. Read-head jumps requested during a crossfade are deferred (`XfadeReader::pending`) instead of dropping a source.
5. Latched parameters at window zero for every reversed / sliced voice (Reverse, Stutter pitch step, Attack / Body
   chunks, Slice Shuffler edge fades).

Intentional destruction kept: Glitch Gate steps still cut in ~3 ms, slices keep a 0.5 ms minimum edge, Stutter rolls
and ratchets are unchanged; nothing was solved by lengthening fades globally or lowering density / range.

## 5. The FX tree (Phase 5)

`core/src/visual/FieldVolume.cpp` (CPU reference) and the GLSL generated from it.

- **One continuous medium** around a spine through the committed chain (input lower-left, output upper-right),
  gently S-bent and drifting. The envelope is the **maximum** over spine segments, so the joints between effects
  never brighten: there is no centre, orb, halo or burst per effect.
- **Material**: two families of thin crossing filaments (ridge-sheet intersections; one flow-aligned, one isotropic,
  so they tangle), folded translucent ribbons, broad veils that also absorb (depth, shadow, weight), and long thin
  threads reaching out into space. Everything lives in flow-warped space, so the outline is organic.
- **Light / colour**: emission + absorption, depth cooling to indigo; FX colours blended along the spine coordinate
  (Gaussian in s, not radial) with chroma lifted where energetic (`kChroma` 1.45, palette mapping unchanged);
  white-lilac only in the hottest filaments; faint cyan / pink iridescence at filament edges, scaled by the colour's
  own chroma (none in SOLO grey); one hot mass toward the output, like the reference.
- **Background**: near-black plum / navy (`backgroundColour`, linear ~0.016 / 0.011 / 0.026, slightly lifted toward the
  field) plus a faint energy-reactive scatter halo. No stars, dots, nebula or decoration (tested: the empty void is
  smooth, max < 1.6 x min).
- **Audio**: energy = slow emission with a floor (a quiet field stays alive); transients = pulses travelling the spine;
  low = veil and outline motion; mid = ribbons; high = filaments; onset density = fine turbulence; balance = lean.
- **Bypassed FX**: its stretch greys and dims to 30 %; the medium stays continuous.
- **Squares**: the GL march never drops below a third of the resolution (steps are reduced first), white-noise step
  jitter and golden-ratio sub-pixel jitter per frame, accumulation weight 0.22, **bicubic (Catmull-Rom) reconstruction**
  of the accumulated volume, and **dither** in the final composite. The software fallback renders at a quarter (was a
  fifth) with per-frame jitter, history and high-quality upscaling. A `detail` control softens the finest filaments
  when few steps are taken (no sparkle).
- **Cost**: the material (six noise fields) is evaluated only where the medium or its threads can be visible; CPU
  reference 6.6x faster than the first V4 draft. GPU cost is unmeasured on real GPUs (section 9).
- Evidence: `docs/img/v4_formation_6fx.jpg`, `docs/img/v4_formation_bypass_compare.jpg` (6 FX, then the same chain with
  the third FX bypassed), `docs/img/ui_v4_editor.jpg` (software path snapshot of the real editor).
  The GL shader compiles and renders on Mesa (`ih1_uitest`: "the OpenGL formation renders ... no software fallback").

## 6. Factory bank (Phase 1)

| | V3 | V4 |
|---|---|---|
| Presets | 67 | **148** (81 new) |
| Categories | 11 | 10 sound-led: Init / Utility, Signal Collapse, Granular / Reconstruction, Rhythm / Glitch / Stutter, Pitch / Reverse / Harmonic, Spectral / Frequency Space, Dirt / Degradation, Musical / Beautiful, Atmosphere / Cinematic / Drone, Experimental / Hybrid |
| Metadata | sources | sources + intensity (Subtle / Moderate / Strong / Extreme) + style inspiration; all searchable in the browser and shown under the description |
| Macros | none | every preset except Init: 2-4 named macros, explicit for flagship presets, otherwise derived from the chain's topology (Space, Damage, Motion, Chaos, Pitch, Blur rules with per-parameter depths) |
| Transformative (level-matched, `audition`) | 56 / 61 | 89 / 130 creative (68 %), Musical / Beautiful (12) excluded by design; 6 subtle utility |

V3 preset ids are unchanged (projects keep resolving them); their categories were remapped. A test renders every
preset with **all macros at maximum** (finite, peak < +12 dBFS, bounded RMS). Known gap: several rhythm rebuilds
measure as non-transformative on the synthetic, near-periodic audition loops (a measurement limit, not a listening
verdict). Genre names are inspiration only; no commercial preset was copied or imitated value-by-value.

## 7. DSP audit and chain interaction (Phase 7)

- Every chopping / buffer / gating effect was audited by measurement (section 4) and fixed where the audit found a
  mechanism. Every effect gained the parameter glide. Delay's modulation depth and duck amount are now smoothed per
  sample. Two new FX were made mono-safe after the bank's mono-correlation audit caught anti-phase output (Dispersion
  stereo offset narrowed and default 0; Attack / Body smear identical on both channels).
- Not changed (no measured, audible benefit found in this pass): reverb, filter and pitch-shifter algorithms. The SVF
  shelf mixing coefficients still step slightly during a fast glide (Tape Echo Bass); documented.
- **Stage analysis** (audio thread, lock-free telemetry): per committed slot RMS, sample peak, onset strength and the
  bypass ramp; shown in the strip tooltip (peak / RMS / crest).
- **Cross-FX modulation**: preset macros are the shipped, explicit, saved form ("a shared macro -> coordinated
  parameters across selected FX"). Envelope-follower links between stages (FX A envelope -> FX B parameter) are
  **deferred**: the analysis telemetry exists, but a link editor that respects commit-time immutability needs a
  product decision. No decorative routing lines were added.
- Serial order is never changed by any logic; feedback paths remain internal and bounded.

## 8. Latency, CPU

LATENCY_PLACEHOLDER

## 9. Evidence: what was run (Linux container, 4 cores, Mesa llvmpipe, no GPU, no Windows)

VERIFY_PLACEHOLDER

## 10. KADAL sources reused (paths from earlier rounds; not re-searched)

| ID | Path | Used for |
|---|---|---|
| KNW-01J68SK1RC | `03_DSP_KNOWLEDGE/12_granular-processing/granular-synthesis-and-granulation.md` | grain / slice windows, bounded voices |
| KNW-M8Q0G3M7NM | `03_DSP_KNOWLEDGE/16_sampling-and-playback/bandlimited-interpolation-resampling-and-sample-playback.md` | Hermite reads (Slice Shuffler, Echo Matrix, Reverse Pitch Echo) |
| SRC-3Q84R4ZMZG | (Laroche-Dolson phase vocoder, via `Spectral.h`) | Partial Recomposer peak / region model |
| KNW-R8PP5DNBY0, IMP-W4CTPKMXBZ | (TPT SVF, via `Svf.h`) | Dispersion all-pass cascade (modulation-safe) |
| KNW-H0EPVHBYCE, KNW-PY324W089P | `03_DSP_KNOWLEDGE/19_real-time-optimization/...` | no allocation / locks on the audio thread |
| SRC-5Q7S4HYRH5 | Driedger & Mueller TSM review | honest naming of stretch vs varispeed |

Missing in KADAL according to V3's search (not re-checked): deliberate sample-rate reduction, wavefolding, spectral
delay, volumetric field rendering. Recommended KADAL additions from this round: the calibrated click metric
(section 4), the N/4-hop integer-bin translation rule (Spectral Shatter), and the "max, not sum, over spine segments"
continuity rule (section 5).

## 11. Version and installer

0.3.0 -> **0.4.0** (features, state-compatible). CMake project version, plug-in metadata, installer `AppVersion`,
wizard texts, `INSTALL_NOTES.txt`, the ZIP README and CI are consistent. CI builds `IH1-Setup-0.4.0.exe` and
`IH1_0.4.0_Windows_Installer_Round-4.zip` (installer + README, entry count verified). The wizard uses Inno Setup's
dark appearance (`WizardStyle=modern dark`, guarded for compilers older than 6.6) and panel / header art rendered from
the V4 formation (`installer/branding`). User presets in `Documents\IH1\Presets` are never touched by install,
upgrade or uninstall. Not run here: the installer itself (no Windows).
