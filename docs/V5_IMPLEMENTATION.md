# IH1 V5 (feedback round 5): what changed, why, and the evidence

Version **0.5.0** (test build, round 5), following the project's established convention (round N = 0.N.0, installer
`TestRound=N`). This pass implements `IH1_V5_FEEDBACK_MASTER_PROMPT.md` with `IH1_V5_RESEARCH_SUPPLEMENT.md`.
Open items: `docs/KNOWN_ISSUES.md` (V5 section). FL Studio checklist: `docs/FL_STUDIO_TEST_CHECKLIST.md` (V5 section).

## 0. Starting point and inputs

| Item | Finding |
|---|---|
| Branch | `claude/bold-mccarthy-ydvunz` was at `0079915` (V1). The V4 work (0.4.0) was on `claude/keen-edison-jiahki`, a strict descendant, so the branch was fast-forwarded to `8591041`. Nothing was discarded. |
| Build | CMake 3.28, GCC 13.3, Ninja, JUCE 9.0.5 (pinned), doctest 2.4.11, OpenGL 3.2 core with a software fallback. The X11 / ALSA / GL dev packages from CI were installed for the plug-in build. |
| Baseline | V4 tip: unit tests **148 / 148** (3 m 42 s). Baseline feedback audit and click audit recorded before any DSP change (sections 1 and 4). |
| Prior prompts | Only the V1 brief and master prompt are in `docs/source/`; the V2-V4 prompts were not supplied. Active requirements were taken from `docs/V4_IMPLEMENTATION.md`, `docs/V3_IMPLEMENTATION.md`, `docs/V2_1_IMPLEMENTATION.md` and `docs/KNOWN_ISSUES.md` (ledger below). |
| KADAL | **Not searched.** Attaching `R4N-WAV/Kadal` was refused by the session's permission policy (as in V4). The KADAL paths recorded in `docs/V4_IMPLEMENTATION.md` section 10 and `docs/KADAL_RESEARCH_MAP.md` were reused; nothing was added to KADAL. |
| FL Studio / Windows / GPU | Not available. Everything was validated on Linux (Mesa llvmpipe). Section 9. |

### Requirements ledger (inherited, status after V5)

| Requirement | Origin | Status | Regression guard |
|---|---|---|---|
| Committed chain = audio truth; staged plays after it | V1 | kept | `test_engine`, `test_v5` |
| Committed values immutable; reorder / bypass / delete only | V1, V4 | kept; the committed snapshot and the new staged copy share no parameters or processor | `test_v5` commit tests |
| None is a carousel entry; recalls land on None | V3, V4 | kept (**superseded for COMMIT only**: V5 keeps the browser on the committed effect) | `test_v4`, `test_v5`, `ih1_uitest` |
| Drafts cached until RESET; Init resets everything | V3 | kept; the committed type's draft returns to its defaults at COMMIT (V5 requirement) | `test_v5` |
| Clear FX Chain (confirmed), Init | V2.1, V3 | kept; both clear macros | `ih1_uitest` |
| Truthful Full / Low / Zero latency | V2.1 | kept; the muted stand-by copy adds no latency; Swell reports its 3 ms look-ahead and has a 0-sample variant | `test_v5`, `ih1_hosttest` (reported == measured) |
| Output Safety | V3 | kept, and never relied on: the feedback audit runs with it **off** | `ih1_feedback_audit` |
| Accepted palette | V2.1-V4 | kept: V5 types are appended, so no accepted colour tier moved; V5 types get a readability floor | palette tests |
| Continuous field, no per-FX centres, input bottom-left to output top-right | V4 | kept | `test_formation`, `test_v5_visual` |
| Click-free chopping, parameter glide, discrete-switch dip | V4 | kept and extended (section 4) | `ih1_click_audit` |
| Two-thirds of the creative bank transformative | V4 | kept for the V4 bank; V5 presets are a deliberate mix of mix-useful and radical (V5 4.2 / 4.6) | `test_presets` |
| Every preset has macros | V4 | **superseded** by V5 Phase 8 (macros only for meaningful combinations) | `test_v4`, `test_v5` |

## 1. Feedback runaway (Phase 5, P0)

**Tool.** `tools/ih1_feedback_audit` drives the real Engine + Session with Output Safety **off**. It covers every FX type with its recursive controls at the legal maximum plus 6 seeded random drafts at 85-100 %, and every factory preset with macros at 0 %, at 100 % and swept. Each case gets an impulse, a noise burst, a chord and a drum loop, each followed by 14-15 s of silence. Buffer-based effects get 30 s, so that replays of recorded history end inside the window. Verdicts:
- **FAULT:** a non-finite value or an engine guard reset.
- **GROW:** still louder than the excited level at the end, or still rising, or above +12 dBFS.
- **HOLD:** not decaying (T60 > 150 s); acceptable only for an intended freeze or loop.
- **LONG:** decaying slowly.

**Baseline (V4 DSP): 20 GROW / FAULT and 13 unexplained HOLD cases in 385 FX cases.** Every one was traced to a mechanism:

| Processor (case) | Why it grew / held | Fix | Regression test |
|---|---|---|---|
| Shimmer (rnd3-5: +12 dBFS after the input) | the pitch-shifted tank output was *added* on top of the lossless Householder recirculation: that mode recirculated at K + 0.9 regen > 1, bounded only by a tanh | the shift now *replaces* part of the mode inside the recirculation: (1 - a) s_u + a shift(s_u); shifter and filters <= 1, so every mode keeps gain <= K < 1 | `V5 runaway regression: shimmer ...` |
| Granular (max, rnd1-3, 6: +12 dBFS) | overlapping grains normalised by 1/sqrt(overlap) add coherently to ~0.65 sqrt(overlap) > 1; 0.9 feedback | `GrainCloud` exposes its exact weight bound (sum of envelope x pan gains x norm, peak-held 80 ms); feedback is divided by it | `... granular ...` |
| Reverse, Grains (rnd6) | same grain-cloud sum | same weight bound | `... reverse ...` |
| Reverse, Buffer (rnd1: +12 dBFS) | both half-overlapped voices re-read every sample and recirculate coherently: measured loop ~1.43 x Feedback (grows from 75 %, stable at 70 %) | Buffer recirculation x 0.62; Delay mode capped at 0.9; 120 s T60 ceiling per chunk | `... reverse ...` |
| Frequency Shifter (max: +9 dB) | I cos + Q sin has unity gain only where the Hilbert pair is in quadrature; near DC / the top octave it approaches sqrt 2 | feedback band-limited to the accurate band (HP 40 Hz, LP 13 kHz, Q 0.6) | `... frequency shifter ...` |
| Chorus (max, rnd1-6: hold at full level / growth) | voices summed with 1 / sqrt(V) add coherently at low frequencies: loop sqrt(V) x 0.7 > 1 from three voices | the loop takes acc / V (coherent-normalised) | `... chorus ...` |
| Delay (max, rnd1, 3, 5) | Feedback 100 % = unity loop, and Drive's makeup (1 + drive) is gain inside the loop | small-signal loop gain fb x (1 + drive) capped at 0.985 and at a 120 s T60 for the delay time | `... delay ... tape echo ...` |
| Tape Echo (rnd4, 5) | heads add coherently: per-head Intensity / active x 1.4 = 1.4 x Intensity; record makeup (1 + 0.25 sat) in the loop | summed loop = Intensity x makeup, capped 0.98 up to 100 %; the documented > 100 % self-oscillation kept (bounded) | `... tape echo ...` |
| Pitch Shift (rnd4) | two voices with pan gains up to 1.41 each recirculate together | feedback normalised by the voices' summed gain | `... pitch shift ...` |
| Grain Engine (max, rnd3, 4: hold) | coherence-adaptive norm keeps output power ~ input, and the drive makeup is in the loop | weight-bounded feedback (as GrainCloud); drive without makeup | `... grain engine ...` |
| Bitcrush (rnd2: -48 dBFS forever) | TPDF dither turned digital silence into +-1 LSB noise | no dither on digital silence | `V5 regression: bitcrusher dither ...` |

Macros: `macroSafeMax` keeps any macro target out of self-oscillating ranges (Tape Echo Intensity <= 100 %, any Feedback <= 85 %, Shimmer <= 90 %, Granular density <= 40).

**After V5:**
- **FX types:** 448 cases, 0 GROW / FAULT, 0 unexplained HOLD (11 long tails that decay as configured). Table: `docs/V5_FEEDBACK_AUDIT_FX.md`.
- **Presets:** 354 cases (every preset, plus macros at 100 % and swept for all 73 presets with macros), 0 FAULT, 0 unexplained HOLD.
  - 7 intended holds: Looper, Freeze and Grain Engine Loop modes, and Degrade's documented noise floor.
  - One residual: `frg.loop_ghost` @ 100 % macro. It is an intended Loop-mode loop; its Cloud macro raises density and wet, so the held loop settles about 5.6 dB above its first second. It is flat (+0.06 dB/s) with a +2.1 dBFS peak before Output Safety, so it is bounded, not a runaway.
  - Table: `docs/V5_FEEDBACK_AUDIT_PRESETS.md`.
- The preset pass at first exercised only authored macros. Covering combination macros found two more cases. `v4.ghost_station` turned out to be Degrade's constant noise floor lifted by the macros. `v4.slow_planet` was a 0.3x Stretch replay that decays to -135 dBFS within 60 s. The macro feedback ceiling was lowered to 85 % anyway.

## 2. Clicks and pops (Phase 6, P0)

V4's calibrated detector (`tools/ih1_click_audit`: high-passed 1 ms windows, > 26 dB over the local median, > -50 dB of the local signal, short and isolated) was used unchanged apart from one **end guard** (V5): windows in the last 25 ms are not judged, because their isolation test cannot be evaluated (a Glitch gate re-opening at the very end of a render was counted). Every remaining flag was opened in the sample domain before deciding.

| Effect | Mechanism found | Fix |
|---|---|---|
| Glitch Scatter (static 2 -> **0**) | the 9-bit / 6 kHz lo-fi stage engaged during a Gate step's 3 ms fade to silence: an isolated crunch (case 1); case 8 was the end-of-render artefact | no lo-fi on Gate steps; fragments requested during a splice wait for it (keeps the outgoing source) |
| Time Warp (20-window periodic case fixed) | entering "rest" (triggered Half-Time / Tape Stop released) switched from the playhead (minLag = 6 samples behind) straight to the direct input: a 6-sample jump at every segment, visible as repeated samples in the dump | rest is entered with an equal-power crossfade from the playhead; keep-pitch grains are anchored so they never reach the write head |
| Reflection Sculptor (new; moving 19 -> **0**) | tap-count / variation changes moved read heads during the 30 ms glide; the outgoing layout lost its tone filters; predelay / diffusion stepped every 32 samples | layout crossfades (40 ms) queued while one runs, with their own tone memory; levels / pans glide per sample; predelay / diffusion smoothed |
| Hit Melody (new; static 1 -> **0**) | retuning a still-ringing two-pole voice; a strike wider than the note's partials | three voices, a ringing voice fades 3 ms before it is retuned; strike band-limited to 2x the top partial |

Final, 22 chopping / gating / new effects x 30 cases x 4 s:

| | Static (normal use) | Moving (all continuous controls jump every 0.35-0.9 s) |
|---|---|---|
| New V5 FX (9) | **0** | Drum Space 1, Drum Crush 5, Cloud Chamber 1, others 0 |
| Stutter, Glitch, Looper, Reverse, Gate, Tremolo, Slice Shuffler, Attack / Body, Echo Matrix | **0** | Reverse 1, Slice Shuffler 2, Attack / Body 4, Echo Matrix 7 |
| Time Warp / Reverse Pitch Echo | 1 / 1 (inspected: smooth onsets, no discontinuity) | 1 / 1 |
| Granular / Grain Engine | 1 / 2 (V4 residuals) | 6 / 7 |

Chopping stays sharp. No fade was lengthened globally. Gates keep their 3 ms edges, and Pulse Shaper's attack goes down to 0.2 ms as a raised cosine.

## 3. Commit keeps the browser position (Phase 7)

- `Session::commit()` makes the warmed staged processor the immutable committed instance, as before. It then stays on the same type, resets that type's draft to its defaults, and prepares a **fresh** processor muted ("stand-by"). Nothing is shared between the two: separate processor, separate frozen values.
- The engine installs the new staged copy **already silent** in the same sample as the hand-over (`Program::stagedStartsMuted`). While a COMMIT program waits in the mailbox, the outgoing staged FX stays audible even if the host MUTE flag rises first. Result: no double processing and no dip (`test_v5`: the step at COMMIT is no larger than the sine's own slope, for zero-latency and latency FX).
- A stand-by copy adds **no latency** (its output is not delayed), so the host latency does not change at COMMIT. Auditioning restores the real latency through the normal program transition, and the host is notified.
- Staged parameter publishing is keyed by the target processor, so the defaults meant for the new copy can never reach the processor being committed.
- **Leaving stand-by:**
  - Audition: MUTE off, touching a control (a UI gesture; host automation does not count) or selecting another effect.
  - RESET keeps stand-by.
  - Recall / Init end it.
- **UI:** the carousel and settings stay usable. The panel shows "Committed. This fresh copy is muted (M)...", MUTE is lit, and controls are dimmed but editable. COMMIT is disabled until the copy is auditioned, so a stray double press can't commit a silent copy.
- **Evidence:**
  - `test_v5`: 4 commit cases, including duplicates, rapid commits, latency FX and recall.
  - `ih1_uitest`: 5 checks through the real editor.

## 4. New processors (Phases 2 / 3)

Inventory first (64 types after V5). Not re-added because they already existed:
- Shimmer.
- Spectral freeze / blur / hold (Spectral Freeze, Spectral Smear).
- Granular atmosphere (Granular Cloud, Grain Engine Cloud).
- Convolution.
- Modal / resonant drum body (Modal Resonator with membrane material and transient excitation).
- Break slicing (Slice Shuffler, Glitch, Stutter).

| Effect (id) | Role | DSP (what makes it distinct) | Latency | CPU (48 k / 256) |
|---|---|---|---|---|
| Cloud Chamber (`cloud_chamber`) | atmosphere | 16-line FDN, normalised Hadamard matrix; per-line **three-band absorption** (separate low / mid / high T60), normalised so max \|H\| <= max band gain; sparse -> dense input; Bloom all-passes; Drift on 4 lines; Freeze = exact identity, input muted, drift off | 0 | 0.93 % |
| Reflection Sculptor (`reflections`) | atmosphere | 24-tap early-reflection designer: 8 patterns, tap count, spread, fall-off, distance darkening, motion, diffusion; pure FIR; layout changes crossfaded | 0 | 1.55 % |
| Reverse Bloom (`reverse_bloom`) | atmosphere | input -> bounded Householder reverb -> its tail played backwards one chunk later by two Hann voices: a live reverse-reverb swell (the delay is the effect, not latency) | 0 | 0.37 % |
| Swell (`swell`) | atmosphere / hybrid | onset-triggered volume-pedal bloom (curve, depth, retrigger) into an all-pass wash; 3 ms look-ahead in Full Quality | 144 (Low / Zero: 0) | 0.19 % |
| Pulse Shaper (`pulse_shaper`) | tremolo / groove | 16-step amplitude patterns (16 rhythms) on the host grid / free clock: swing, accents / ghosts, per-bar seeded probability, humanise, attack / gate / release, stereo linked / offset / ping-pong / mirror, band focus (only lows / mids / highs move; gain 1 = exact input), reset on hit | 0 | 0.11 % |
| Drum Crush (`drum_crush`) | drums | parallel: instant-attack compressor whose gain reduction also drives an ADAA saturator, tilt tone, body shelf, DC blocker, blend over the untouched dry | 0 | 0.77 % |
| Drum Space (`drum_space`) | drums | exact transient / body split (sums to the input) exciting a bright / dark path into early reflections + 8-line room; 5 rooms incl. Gated | 0 | 0.76 % |
| Band Transients (`band_transients`) | drums | attack / sustain per band (LR4 splits, mid = exact remainder: neutral nulls), linked detection, peak safety | 0 | 1.49 % |
| Hit Melody (`hit_melody`) | hybrid | every detected hit strikes the next note of a key / scale pattern on three all-pole partial resonators | 0 | 1.09 % |

Each new type passes the registry contract suite and has a behaviour test of the property that makes it distinct (`test_v5`), for example:
- Cloud Chamber's lows outlast its highs by more than 15 dB, and its freeze holds within 3 dB.
- Reflection Sculptor is silent after its span (no loop).
- Pulse Shaper follows the host grid at more than 30 dB depth, and Band = Lows leaves an 8 kHz tone flat.
- Band Transients nulls at neutral settings.
- Hit Melody's pitch climbs with each hit.

The feedback audit finds no GROW / FAULT / HOLD in any of them.

## 5. Combination macros (Phase 8)

`core/src/model/ComboMacros.cpp` holds 11 role-pair combinations. Detection uses stable type IDs of **enabled** instances; a bypassed member removes the combination:
- **Drum Bus:** Impact, Body, Dirt.
- **Pulse into Echo:** Pulse, Swing, Echo Motion.
- **Groove Room:** Groove, Room.
- **Granular Space:** Scatter, Cloud.
- **Dub Wear:** Wear, Dub.
- **Pitch Halo:** Halo, Decay.
- **Resonant Space:** Ring, Strike.
- **Spectral Blur:** Blur, Space.
- **Atmosphere:** Bloom, Decay, Drift.
- **Moving Space:** Drift, Bloom.
- **Deep Space:** Bloom, Drift.

Rules:
- Each macro moves at least two parameters, and the set spans at least two effects.
- No parameter is driven by two macros.
- Parameters that are inactive for the committed settings (mode-dependent), or already at the end of their range, are skipped.
- `macroSafeMax` applies.
- Authored preset macros are kept. Everything else is regenerated after every chain change, and no combination means no knobs.
- Host macro values return to 0 when the set changes.
- Tooltips list the real mapping and the combination.

The V4 generic topology-derived macros are gone.

**Tests (all pass):**
- Each combination's macros, **one at a time** at 25 / 50 / 75 / 100 %: audible (at 100 %, the difference from 0 is above -26 dB), monotonic until the difference saturates, bounded and finite.
- Ineligible chains have no macros; bypass removes a combination; save / load restores the macros.
- Bank gate: all **152 macros of the 73 presets** that have them are audible on their own.

The gate found two placebo macros, which were then fixed:
- *Particle Drift* / Scatter: its targets were already at their maximum, so the headroom rule now skips such targets.
- *Glass Room* / Ring: too faint on a 10 % modal layer, so the preset now has authored macros.

## 6. Factory bank (Phases 4 / 10)

| | V4 | V5 |
|---|---|---|
| Presets | 148 | **209** (61 new) |
| New atmosphere | - | 22 (21 in Atmosphere / Cinematic / Drone, plus *Blue Hour* / *Haze Focus* in Musical) |
| New drums / drum bus | - | 22 (new category *Drums / Drum Bus*; 26 drum-tagged incl. hybrids) |
| New tremolo / volume shaping | - | 23 presets use Pulse Shaper or Tremolo audibly (17 in the new *Tremolo / Groove* category) |
| Presets with macros | 147 (mostly generic) | 73 (authored or combination; all audible) |

All new presets combine at least two effects and carry a description, sources, intensity and style. They load deterministically, pass the factory audit on drums, pad and voice (level -23 .. +8 dB relative to the source; no clipping, mono-safe) and the near-duplicate screen (same chain with every parameter within 5 % = duplicate). Genre names are inspiration only.

## 7. FX tree (Phase 1)

- **Edges and cropping.**
  - Cause: the field was framed for a fixed height, so at narrower or wider aspect ratios and with longer chains the dense medium ran into the panel edge and was cut by it (visible in the V4 renders).
  - Fix: smoothed **auto-framing**. A camera zoom of 0.45–1 keeps every node's projected envelope inside a 7 % margin and eases over 1.5 s, so resizes, commits and reorders reframe gradually. Only the faint halo and threads may cross the edge.
  - The zoom is shared by the GLSL path, the CPU reference and `Formation::project` (hover and labels).
  - `test_v5_visual`: aspect ratios 0.9–2.2 with 1–24 effects all stay inside.
- **Jitter.**
  - Cause: the simulation is already frame-rate independent (exponential followers, 1.1 s glides, a 76–90 s camera sway). The vibration came from the renderer: the ray-march step and sub-pixel jitter are redrawn every frame, and 22 % of every frame was fresh noise.
  - Fix: the accumulation weight is adaptive. 9 % when calm, 30 % on travelling onset pulses (flashes stay immediate), normalised to the real frame interval.
  - Measured on the CPU reference: frame-to-frame noise **-20.8 dB -> -28.9 dB** relative to the image.
- **Tone.** `visual::Mood`: the void lifts toward its existing plum, the deep indigo depth tint is desaturated, and exposure is 2.2 -> 2.4 (+0.8 dB). Three variants were compared (`docs/img/v5_mood_variants.png`: A = V4, B, C). **B** was chosen: more luminous and less threatening with the palette unchanged, while C washes the black toward milky purple.
- **Commit continuity.** No regeneration and no seed change. A commit adds one node; existing nodes glide to their new slots while the camera eases back. `test_v5_visual`, across three rapid commits:
  - existing nodes move less than 1.2 % of the panel height per frame;
  - the zoom changes less than 0.006 per frame;
  - identities never change.
- **Audio telemetry** is unchanged: lock-free atomics, smoothed RMS / bands / onsets, and an instant input-to-output pulse.

## 8. DSP audit of the library (Phase 9)

| Area | Evidence | Change |
|---|---|---|
| Every recursive path (all 64 types, all presets) | `ih1_feedback_audit` (section 1) | 10 processors fixed |
| Chopping / gating / buffer effects | `ih1_click_audit` (section 2) | 4 processors fixed |
| Every type: silence, finiteness, block-size independence, determinism, reset, sample-rate change | registry contract suite (`test_fx_generic`) | passes for all 64 |
| Latency reporting | `ih1_hosttest`: reported == impulse-measured in every mode | Swell variant added |
| Unchanged and why | reverb, filters, pitch shifters, oversampling: no measured defect in this pass | - |

CPU of the processors changed in section 1 is within noise of V4: Delay 0.80 %, Tape Echo 0.65 %, Shimmer 1.32 %, Chorus 1.15 %, Granular 1.14 %, Grain Engine 0.92 %.

## 9. Evidence: what was run (Linux container, 4 cores, Mesa llvmpipe, no GPU, no Windows)

| Check | Result |
|---|---|
| `ih1_tests` | **177 / 177**, 28.8 M assertions (V4 baseline 148) |
| `ih1_uitest` (real editor, Mesa OpenGL, `IH1_REQUIRE_GL`) | **pass**, 96 checks incl. 5 V5 commit checks; the GL formation (new zoom / mood uniforms) compiles and renders |
| `ih1_hosttest` (built VST3 through JUCE's host) | **56 / 56** |
| `ih1_feedback_audit` (presets + FX) | 448 FX cases + 354 preset cases: 0 FAULT, 0 unexplained HOLD; 1 bounded Loop-mode level rise (section 1) |
| `ih1_click_audit` | section 2 |
| pluginval, sanitizers, soak | **not re-run** in this pass (CI runs pluginval on Linux and Windows) |
| FL Studio, Windows, real GPUs, installer | **not tested** (not available) |

## 10. Files

- **New:**
  - `core/src/fx/V5Atmos.cpp`, `core/src/fx/V5Drums.cpp`
  - `core/src/model/ComboMacros.cpp`, `core/src/presets/PresetsV5.cpp`
  - `tools/ih1_feedback_audit.cpp`
  - `tests/test_v5.cpp`, `tests/test_v5_visual.cpp`
- **Changed:**
  - Engine / Session (commit stand-by, targeted parameter publishing, macros)
  - Shimmer, Grains, Granular, Reverse, Freq Shifter, Chorus, Delay, Tape Echo, Pitch Shift, Grain Engine, Bitcrusher, Glitch Scatter, Time Warp
  - Formation / FieldVolume (zoom, mood)
  - FormationView (adaptive accumulation)
  - SettingsPanel / PluginEditor / PluginProcessor (stand-by UI, macro tooltips and reset)
  - presets, the click audit, the uitest
  - version and installer metadata.
