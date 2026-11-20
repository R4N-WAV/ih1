# IH1 V2.1: what changed, why, and the evidence

This pass implements `IH1_V2_1_MASTER_PROMPT.md` using `IH1_V2_1_DEEP_RESEARCH.md`, KADAL (`R4N-WAV/Kadal`), the
reference video and the existing code. Test evidence is in section 9, open items in `docs/KNOWN_ISSUES.md`.

## 0. Starting point

- The working branch `claude/sleepy-goodall-frh3kc` was at `0079915` (V1). The V1-feedback pass that the V2.1
  feedback describes (3-D filaments, 28 presets, in-editor menus, the "More" toggle) was on
  `claude/new-session-5fkj8f`. Its tip is a descendant of `0079915`, so the branch was fast-forwarded to it. Nothing
  was overwritten or discarded.
- `IH1_V2_RESEARCH_SUPPLEMENT.md` and `IH1_V1_FEEDBACK_MASTER_PROMPT.md` were not supplied in this session and are
  not in the repository. The V1-feedback documents in `docs/` were used instead.
- The uploaded reference video is byte-identical to `references/ih1_visual_reference.mp4` (same MD5).
- Baseline before any change: unit tests 78/78, `ih1_hosttest` 48/48, `ih1_uitest` passing.

## 1. Diagnosis

| Feedback | Symptom | Root cause (code) | Fix | Test that proves it |
|---|---|---|---|---|
| 10 | Preset arrows work about 1 click in 5 | `PresetBar` used 10 x 14 px `ShapeButton`s inside 22 px columns. The rest of each column fell through to `PresetBar::mouseDown`, which ignored it. `Button` fires on mouse-up, so a press that drifted out of the tiny box was lost. Every step also rescanned and parsed all user preset files. | Full-height 30 px arrow zones that act on mouse-down. Each press, including the second press of a double-click, steps once. The preset list is cached (2 s, invalidated on save, rename and delete). `PresetBrowser.cpp`, `PluginProcessor.cpp` | `ih1_uitest`: 120 rapid real-peer clicks at random points across both zones, logging expected vs actual index: 120 exact single steps |
| 3 | Commit transition rarely appears | `doCommit` queued the transfer at once, but the renderer's chain snapshot was only refreshed by a 30 Hz timer. The GL thread (about 60 fps) usually consumed the transfer before the new id was in its chain. `Formation::layout` then hit `continue` and dropped it. | The formation keeps a transfer request (up to 2 s) until its instance arrives. `FormationView::requestTransfer` also publishes the chain first. The audio commit never waits on the animation. | `test_formation`: a transfer that arrives a frame before its instance still runs. A transfer for an id that never arrives expires. |
| 9.1 | Persistent sideways T | The V1.1 layout hard-coded the reference's skeleton: "a vertical plume crossing a horizontal arm" (`Formation::layout`). | A new layout: nodes lie along an understated bottom-left to top-right S path, with seeded lateral and depth offsets. | `test_formation`: node x/y correlate with chain order (> 0.85 / < -0.75). The light's principal axis lies between 20° and 70°. |
| 9.2 to 9.4 | Stiff rainbow "hair", lighting reads as blur | Strands were streamlines of one global curl field with fixed lengths, mostly parallel. Bloom gain was 1.6. | A dipole force field (section 4). Sharp additive cores with 2 px near strands. Bloom 0.45. Depth fog. | Screenshots `docs/img/ui_v2_1_editor_gl.jpg`. Motion test: p99 < 0.4 % of the height per frame, no pops. |
| 9.5 | Too vibrant, childlike rainbow | 8 roles with saturation 0.42 to 0.66, spread over teal, pink and blue per type | 9 family colours fitted to the measured envelope of the reference (section 4.3) | `test_formation`: families ΔE > 15, hue range, saturation ≤ 0.5 |
| 9.7 | SOLO monochrome and MUTE greying missing or unreliable | `FrameInput` had no SOLO input. MUTE only toggled the M button. | Monochrome follows the engine's own solo ramp (telemetry). MUTE greys and disables the carousel, every control, COMMIT and RESET. Only S and M stay live. | `test_formation` (monochrome and colour return), `ih1_uitest` (mute greying) |
| 2 | Free time knob active while Sync is on | No dependency concept existed | Data-driven `ParamSpec::enabledBy / enabledWhen / disabledReason`, annotated centrally for all 8 sync FX. Disabled JUCE components get no mouse, wheel or key input. The value is kept. | `test_v21` (every sync FX, dormant value has no audible effect and survives toggling), `ih1_uitest` (drag and wheel ignored, tooltip, re-enabled) |
| 4 | Long flat right-click list | Single-level `MenuOverlay` | Two-level menus with hover intent and an aim triangle, click, keyboard and edge flipping. Categories come from the registry (`FxCategory`). | `ih1_uitest`: categories hold the whole catalogue, diagonal move keeps the submenu, rest switches it, Left / Escape, edge placement |
| 8 | Advanced controls hidden behind More | `showAdvanced_` toggle | Removed. The `Advanced` label sits above an always-visible group. | `ih1_uitest`: every Delay control visible without a toggle |
| 15 | No safe clear | "Clear chain" lived in the preset browser, with no exact warning | A discreet chain utility (···) at the right end of the chain strip, with an editor-modal dialog using the exact wording. It uses the existing lock-free program hand-over. The staged FX is kept. | `test_v21` (duplicates, staged kept, empty state, preset after clear), threaded stress with clears, `ih1_uitest` (Cancel, Escape, outside click, confirm, rapid open/cancel) |
| 16 | Wanted honest zero latency | Spectral FX (2048), Convolution (256) and oversampled shapers (23) always added latency | Latency modes with a central capability registry (section 3) | `test_v21` and `ih1_hosttest`: reported == impulse / cross-correlation measured |
| 6 | Not Portal-class | Granular Cloud only: no scan or stretch, no retrigger, fixed normalisation | New Grain Engine (section 5) | `test_v21` (level, pitch under stretch, varispeed, freeze, scale, stress, automation) |
| 7 | Weak presets | 28 presets, some single-purpose | 66 presets in 10 categories with source tags, audited on three sources | `test_presets` factory audit |
| Found in audit | V1 Delete confirmation was never red | The look-and-feel ignored a quiet button's text colour | An explicit `danger` style, used only on the final destructive action | Screenshot `docs/img/ui_v2_1_clear_dialog.jpg` |
| Found in audit | A preset in Loop or Freeze would loop silence | Loop and Freeze latched at load time, when the buffer was empty | They arm and latch once enough real signal has been captured | `test_v21` freeze test, factory audit (no silent preset) |
| Found in audit | My first Zero-latency shaper note ("slightly more aliasing") was wrong | Measured: about 24 dB more aliasing, and Fold at -5 dBc | Added the Low variant (min-phase IIR, same aliasing as Full). The Zero note now states the measured cost. | Section 3.2 measurements, `test_v21` |

## 2. Workflow and UI (Phases 2, 4, 5, 8, 10, 15)

- **Spacing (Phase 5).** A 4 px scale in `Ih1LookAndFeel.h`, used as a hierarchy rather than a uniform margin:
  - outer inset 32 px (was 22), top and bottom 24 px;
  - header to carousel 16 px, carousel to settings 24 px, settings to the COMMIT row 24 px;
  - selector column gap 24 px.

  The column widens with the window (440 to 560 px), so the settings area is not squeezed. No boxes or dividers were added.
- **Chain utility (Phase 15).**
  - Three faint dots at the right end of the chain strip. Right-clicking the strip opens the same menu.
  - The menu offers the latency modes and *Clear FX Chain…*, which is disabled when the chain is empty.
  - The confirmation covers the whole editor:
    - **Clear the FX chain?**
    - "This will remove every committed FX from the tree and audio chain. This action cannot be undone unless a saved preset or undo state is available."
    - Actions: **Cancel** and **Clear Chain**. Only Clear Chain is red; focus starts on Cancel.
  - The DSP side was already correct and is reused (`Session::clearChain` → new `Program` → mailbox → swap at a block boundary with a 5 ms fade → old program back through the SPSC retire queue → processors freed on the message thread). The audio thread never allocates, frees or locks.
  - The formation and chain strip update from the same session revision. The preset browser no longer carries its own clear.
- **Latency status.** A quiet line next to the dots: e.g. `Zero Latency 0 samples`, or `Zero latency unavailable: Harmony needs 512 samples`. Its tooltip lists every contributing effect and the trade-offs. Both come from the registry, never from hand-written UI lists.

## 3. Latency (Phase 16)

### 3.1 Design

- **`LatencyMode { Quality, Low, Zero }`** is requested per session and saved in the DAW state (`session.latencyMode`).
- **Resolution per FX.** Each FX type resolves the request through `FxDescriptor::resolveLatency` and its `LatencyCaps`, which are annotated centrally in `core/src/fx/Registry.cpp`. If Zero is unavailable for a type, it runs its Low variant and the status names it.
- **Switching modes.** `Session::setLatencyMode` rebuilds every processor variant on the message thread, with the same ids, frozen values and seeds. It swaps them in through the existing hand-over. Tails restart; the output crossfades over 5 ms.
- **Host reporting.** `Processor::uiSetLatencyMode` calls `setLatencySamples` on the message thread. JUCE then issues the VST3 `restartComponent(kLatencyChanged)`. Latency never changes from parameter automation.
- **What "zero" means here.** Zero means nothing is buffered, nothing looks ahead, and nothing uses a linear-phase or analysis window. Causal IIR filters still have the frequency-dependent phase every analogue or minimum-phase EQ has. That is part of an effect's sound, as the research's latency table assumes for minimum-phase EQ and filters.
- **Not latency.** Echoes, tails and grains read from the past are the effect's sound (research Part II §8).

### 3.2 Capability table

Generated from the real processors:

| FX | Category | Full Quality (48 / 96 kHz) | Low (48 / 96) | Zero (48 / 96) | Zero available | What changes |
|---|---|---|---|---|---|---|
| Convolution | Time & Space | 256 / 256 | 0 / 0 | 0 / 0 | yes | Hybrid partitioning: the first 256 taps are convolved in the time domain. Same response (error < -60 dB vs. Full shifted by 256), more CPU. |
| Harmony | Pitch & Vocal | 2048 / 4096 | 512 / 1024 | 512 / 1024 | **no**: runs Low and is named in the status | 512-point STFT: about 11 ms instead of 43 ms. Coarser low-frequency resolution. |
| Pitch Shift | Pitch & Vocal | 2048 / 4096 | 512 / 1024 | 512 / 1024 | **no** | Same as Harmony. A time-domain shifter was rejected for Zero: its read head sits on average half a window behind the input, which is a real delay. |
| Formant Shift | Pitch & Vocal | 2048 / 4096 | 512 / 1024 | 512 / 1024 | **no** | Same as Harmony |
| Spectral Freeze | Spectral | 2048 / 4096 | 512 / 1024 | 512 / 1024 | **no** | Same as Harmony |
| Saturation, Tape / Console, Exciter, Amp / Cabinet | Tone & Character | 23 / 23 | 5 / 5 | 0 / 0 | yes | See below |

The other 30 production effects report 0 samples in every mode.

**Oversampled shapers.**
- Low uses 2x minimum-phase elliptic IIR oversampling (order 10, 0.05 dB ripple, -90 dB from the base Nyquist). Its broadband delay is 5 samples (measured by cross-correlation and reported). Its aliasing is the same as Full Quality.
- Zero uses no oversampling, with antiderivative anti-aliasing only.

Measured on Saturation at +24 dB drive with a 4999 Hz tone at 48 kHz; values are the largest alias component:

| Style | Full (2x FIR + ADAA) | Low (2x min-phase IIR + ADAA) | Zero (1x ADAA) |
|---|---|---|---|
| Tube | -53.9 dBc | -54.0 dBc | -30.0 dBc |
| Tape | -47.2 dBc | -47.2 dBc | -27.4 dBc |
| Transistor | -55.8 dBc | -55.9 dBc | -29.6 dBc |
| Clip | -51.4 dBc | -51.4 dBc | -27.9 dBc |
| Fold | -94.3 dBc | -94.3 dBc | **-5.3 dBc** |
| Smooth | -54.1 dBc | -54.1 dBc | -31.1 dBc |

The Zero note in the UI states this cost and points to Low (5 samples) for Fold.

**Verification.**
- `test_v21`: reported == measured in all three variants. In Zero mode the measurement may differ by one sample, from ADAA's half-sample average.
- `test_v21`: Zero convolution equals Full convolution advanced by 256 (error < -60 dB).
- `test_v21`: impulses through an engine chain in Zero mode arrive at +0 to +2.
- `ih1_hosttest`, through the real VST3:

  | Chain | Full Quality | Low | Zero |
  |---|---|---|---|
  | Saturation + Convolution + Harmony | 2327 | 512 | 512 (reported honestly) |
  | EQ + Saturation + Compressor + Tape / Console | 46 | 0 | 0 |

  In every case the host-reported value equals the impulse offset. The zero-capable chain's processed impulse arrives at +1.

  The host run predates the switch of Low from 1x to the IIR oversampler. The unit tests cover the IIR variant; the host suite was rerun after it (section 9).

## 4. Formation (Phases 1, 3, 9)

### 4.1 Field

- Each committed instance is a **soft magnetic dipole**: B = s(3(m·r)r/r² − m)/r³, softened by the node radius. Its axis is seeded at 35° to 85° to the signal path, so arcs cross the path instead of lying along it. The axis precesses by ±9° over minutes.
- Nodes lie along an **understated S path** from bottom-left (input) to top-right (output). Lateral (±0.42), depth (±0.70) and along-path offsets are seeded per instance. The path scales with chain length, so a single FX is compact.
- The total field adds a weak input→output flow and divergence-free curl noise (6 plane waves, 1.1 to 3.1 unit wavelengths, 0.04 to 0.10 rad/s drift).
- **Filaments** are integrated with midpoint (RK2) steps from stable seeds:
  - arcs: dipole field lines leaving near a pole and bending around the energy centre, with soft normalisation near null points;
  - transmission bundles: node i → i+1, sometimes i+2 (cross-links like a web or neuron). Each strand aims at its own point on the target's shell, so bundles never converge into one junction. Colours are graded between the two family colours.
  - wisps: faint, lit by the high band;
  - faint input strands into the first node and output strands from the last.
- **Stable identity.** Each strand traces a fixed number of points; a finished strand parks at its end. World points are eased towards the newly traced position (time constant 0.5 s), so a field-line branch switch becomes a glide, not a pop. Reorders glide the anchors with a 1.1 s time constant; nothing is reseeded.

### 4.2 Light

- Sharp additive cores. Near, bright strands are 2 px.
- Depth fog: far strands dim and cool towards indigo. Strands fade near the camera's near plane.
- A small highlight near node cores.
- Bloom gain 0.45 (was 1.6), so it is a halo, not the structure. Persistence is 0.3 per frame (was 0.5), keeping filaments crisp.
- **Field response.** A faint deep-violet Gaussian around each node and link midpoint. It is normalised by chain length, evaluated in a shader at bloom resolution in the GL path and at bloom resolution on the CPU. No FX means no glow: the void stays exactly black (tested). Nothing else decorates the background.

### 4.3 Palette

Measured from 61 frames of the reference (2 fps, 360 px), counting lit pixels (max channel > 0.12):
- hue 195° to 345°, with 89 % between 225° and 315°;
- saturation mostly 0.16 to 0.36;
- the brightest cores are near-white lavender (h 280, s 0.16);
- dim filaments are indigo (h 227 to 259);
- small rose (h 332) and icy-blue (h 195 to 225) accents;
- about 26 % of pixels are essentially black.

Nine family colours (one per `FxCategory`), with every pair at ΔE ≥ 15.6:

| Family | Colour | Hue | Saturation |
|---|---|---|---|
| Time & Space | indigo | 236 | 0.44 |
| Pitch & Vocal | pale lavender | 281 | 0.17 |
| Modulation | violet | 262 | 0.46 |
| Granular & Time | ice blue | 208 | 0.36 |
| Spectral | mauve | 296 | 0.34 |
| Rhythm & Glitch | rose orchid | 322 | 0.40 |
| Dynamics | slate | 248 | 0.20 |
| Tone & Character | dusty rose | 340 | 0.26 |
| Experimental | pearl | 222 | 0.14 |

Types inside a family vary slightly (±6° hue, small saturation and value steps). Duplicates use gentle deeper and lighter shades, ranked by instance id, so a reorder never recolours anything.

### 4.4 Audio and states

- **Audio.** `AudioAnalyser` reads only telemetry atomics:
  - smoothed RMS and fast peak, with an onset detector (refractory 120 ms);
  - low, mid and high band energies;
  - output L/R balance, newly added to `Telemetry`.

  The formation applies attack and release smoothing and bounded mappings:
  - energy raises emission and field strength;
  - an onset sends a pulse along the first node's transmission bundle, which cascades to the next node at 72 % strength;
  - per-node level jumps trigger local pulses;
  - the low band adds a slow heave and more curl;
  - the high band lights the wisps;
  - balance leans the curl field.

  Tested: loud versus quiet light ratio is 1.3 to 4.0; a transient rises and settles back within 10 %.
- **SOLO and MUTE.** SOLO turns the formation monochrome (tested with saturation < 0.12, and colour returns). MUTE greys the column.

### 4.5 Commit transition

- A packet of the FX's own dipole loops (the same field language and colour as the nodes) travels on a curved path from the workflow column into its node over 0.95 s, with a faint trail.
- On arrival a pulse fires, and the node's filaments grow out over 1.2 s.
- It runs every time: rapid, duplicate and first commits included, and it is never lost to ordering.

## 5. Grain Engine (Phase 6)

`core/include/ih1/dsp/GrainEngine.h`, `core/src/fx/GrainEngineFx.cpp`. Zero latency: every grain reads past audio.

- **Buffer and voices.**
  - 12 s stereo capture ring.
  - Up to 64 pooled voices; when they are busy, the new grain waits, so nothing is stolen and nothing clicks.
  - 4-point Hermite reads.
  - 5 windows (Hann, Gaussian, Tukey, Pluck, Swell), each exactly 0 at both ends.
- **Modes.**
  - Cloud: offset behind live, plus spray.
  - Stretch: a playhead at -2 to 2x; pitch is independent of speed (granular TSM), or follows it with **Varispeed**.
  - Loop: holds the last *Loop Length* and cycles it at the chosen speed (negative plays backwards).
  - Freeze works in any mode.
  - Loop and Freeze arm until real signal has been captured.
  - Live Stretch slower than 1x wraps back to the Offset at the end of the buffer.
  - Faster than 1x it follows the input, because a live buffer has no future.
- **Performance controls.**
  - Retrigger, free or tempo-synced, for slice, stutter and backspin.
  - Ratio Lock: density = overlap / size.
  - Size and pitch jitter; scale quantisation to Octaves, Fifths, Major, Minor, Pentatonic or Chromatic.
  - Reverse probability; spread; Linked or Independent L/R streams.
- **Smooth Splice.** A WSOLA-style search for the next grain's start: normalised cross-correlation against the newest grain's continuation, coarse every 4 samples then refined ±3. It is bounded at 64 points × 136 offsets per spawn.
- **Feedback.** Grain delay up to 2 s. The feedback resamples into the capture buffer through HP and LP filters, a DC blocker and tanh. Feedback is limited to 0.95 and decays when the input stops (tested).
- **Normalisation.** The output is scaled by 1/√max(1, E·K):
  - E is the smoothed sum of window²;
  - K is the measured coherence (power of the sum / sum of the grains' powers).

  This gives power normalisation for uncorrelated grains and amplitude normalisation for coherent ones. Sparse grains are never boosted. A soft ceiling (+6 dBFS knee) guards the rest.
- **Display.** A live settings graph:
  - the capture waveform;
  - each sounding grain drawn as its real window at its real read position;
  - the playhead, with held state, mode and speed.
- **Tests.**
  - Level stays within ±6 dB across size and density (15 to 800 ms, 4 to 120 per second).
  - Stretch at 0.5x keeps 440 Hz (energy at 440 Hz > 8 × at 220 Hz and > 4 × at 400 Hz).
  - Varispeed halves the pitch.
  - Freeze sustains after the input stops.
  - Octave quantisation keeps energy on octaves (> 10 × off-scale).
  - Maximum settings in all modes with 100 % feedback stay finite and bounded, then decay.
  - Random automation every 64 samples stays finite.

**Time-scale modification, honestly.** Stretch is granular overlap-add with waveform-similarity alignment (KADAL SRC-5Q7S4HYRH5, KNW-Y48Y80179M):
- good for texture, pads and voice at 0.25 to 1x;
- transients smear, by design, as a granular smear;
- no phase vocoder and no transient-aware hybrid TSM were added; the existing STFT engine stays for pitch work.

**Not added.** File import (not needed: the live buffer provides the "recording" feel), per-grain filters, a modulation matrix and macros (the architecture has no modulation system yet).

## 6. DSP quality (Phase 11)

- **Delay Diffusion (new).** Four incommensurate all-passes inside the feedback loop, after the filters, as in research Part I §3.1 type 5.
  - It is appended as an id-keyed parameter, so old projects load it at 0 and sound identical (tested bit for bit).
  - At 100 % it gives more than 4x the tail echo density (tested).
  - It stays bounded at 100 % feedback with full drive (tested).
- **Polyphase 2x upsampler.** Equal to the previous direct form within 3.6e-7 and **2.2x faster**. This speeds up Saturation, Tape / Console, Exciter and Amp in Full Quality.
- **Min-phase IIR oversampling** (Low): see section 3.2.
- **Research answers**, in brief:
  - *Why a premium delay can feel richer:* the topology (filters, saturation and diffusion inside the loop), read interpolation, smoothed modulation and gain staging.
    - IH1's Delay already had cubic reads, crossfaded time changes, filters and drive in the loop, a soft-clip bound and ducking. Diffusion was the missing dimension.
    - Much of a "premium" impression is level and workflow, so every comparison here is level-matched or level-reported.
  - *Grain transformations:* capture, scheduling, windows, coherent normalisation and feedback resampling (section 5).
  - *Aliasing:* measured, not assumed (section 3.2). Smooth shapers with ADAA are already decent at 1x. Fold needs oversampling.
- **Comparative listening protocol.**
  - The factory audit renders every preset on a drum loop, a sustained pad and vocal-like formant pulses, all at -18 dBFS RMS, and reports level, peak, L/R correlation and determinism (`tests/test_presets.cpp`).
  - A level-matched blind listening comparison against commercial products was **not** done; it needs the owner's ears and licences.

## 7. Factory presets (Phase 7)

`docs/FACTORY_PRESETS.md`, generated from the data:
- 66 presets in 10 categories, from 5 to 9 per category;
- the 28 legacy ids are kept and re-filed;
- 38 are new, 21 of them built on the Grain Engine;
- each preset is a real chain of at least two FX, with description, source tags and deterministic ids and seeds;
- no near-duplicates (full settings signature).

Audit results across 198 renders:
- level relative to the source: -15 to +8 dB;
- peaks < +6 dBFS;
- L/R correlation > -0.3;
- bit-identical repeats.

The audit caught and fixed six real problems before release:
- a single-FX preset;
- Minor Drone at +11 dB;
- Slice Retrigger hitting the +12 dB ceiling, through a transient shaper reacting to grain onsets;
- Blown Speaker and Inverted World too hot;
- unfair source levels in the test itself.

## 8. State, migration and compatibility

| Change | Version | Compatibility |
|---|---|---|
| State document | Still `ih1-state` version 1 | All additions are optional fields; old states load unchanged |
| `session.latencyMode` | New field | Missing → Full Quality, the V1 behaviour |
| Delay `diffusion` | Appended parameter | Missing → 0 |
| Grain Engine | New type | Older builds skip it with their existing "unknown FX" warning |
| `preset.tags` | New preset metadata | Ignored by older loaders |
| Factory preset ids | Unchanged | Projects still resolve their preset reference |

## 9. Evidence: what was run

All runs were on Linux in this container: 4 cores, Mesa llvmpipe OpenGL under Xvfb, no GPU.

| Check | Result |
|---|---|
| Unit, contract, behaviour and audit tests (`ih1_tests`) | **101 / 101 pass** |
| ASan + UBSan build of `ih1_tests` (leak detection on) | see `docs/QA_AND_REGRESSION_RESULTS.md` (V2.1 section) |
| TSan | see `docs/QA_AND_REGRESSION_RESULTS.md` (V2.1 section) |
| `ih1_uitest`: real editor, events through the native peer | **72 / 72**, plus the GL formation stays active (no fallback) |
| `ih1_hosttest`: the built VST3 hosted through JUCE's VST3 host | **56 / 56**, incl. 6 latency-mode checks with impulse == reported and 320+ resize / reopen cycles |
| pluginval v1.0.4 | **SUCCESS at strictness 8 and 10** (Linux, Xvfb) |
| 30-minute soak (`ih1_soak`): audio thread at full speed, random commits / deletes / reorders / clears / latency switches / preset loads / RESET / SOLO / MUTE | see `docs/QA_AND_REGRESSION_RESULTS.md` |
| CPU per block (thread CPU time, 80 000 blocks of 256 samples, chains up to 12 heavy FX) | median 0.12 ms, p99.9 2.0 ms, max 4.7 ms (budget 5.33 ms) |
| FL Studio | **Not tested** (no Windows or FL Studio here). Checklist: `docs/FL_STUDIO_TEST_CHECKLIST.md` |

## 10. KADAL sources used

| ID | Path | Used for |
|---|---|---|
| KNW-01J68SK1RC | `03_DSP_KNOWLEDGE/12_granular-processing/granular-synthesis-and-granulation.md` | Grain pool, windows, scheduling, the running-envelope normalisation, and its validation list (flat sum, pitch under stretch, bounded pool) |
| CLM-EA2FBZCRGN | (same record) | The pitch slip of fixed-hop granular stretching, and why to align. Motivated Smooth Splice and the 440 Hz stretch test. |
| SRC-5Q7S4HYRH5 | Driedger and Müller 2016, TSM review | OLA vs WSOLA vs phase vocoder: why Stretch is WSOLA-aligned granular OLA, and its limits |
| KNW-Y48Y80179M | `13_pitch-and-time-processing/voice-pitch-shifting-and-formant-preservation.md` | PSOLA / WSOLA alignment, the HPSS hybrid (not implemented; documented) |
| KNW-JC0N8AGZ90, CLM-VXYQGWYKBP | `03_nonlinear-processing/oversampling-filter-choice-for-nonlinear-processing.md` | ADAA vs oversampling per shaper; linear-phase resampler latency must be reported. Led to measuring Zero and adding the IIR Low variant. |
| KNW-H0EPVHBYCE, CLM-SS08AKETDP, CLM-JZKPY3EGC2 | `19_real-time-optimization/real-time-audio-thread-rules.md` | No allocation or locks on the audio thread; build objects off-thread and swap pointers through lock-free queues. The existing hand-over already follows this; latency-mode and clear reuse it. |
| KNW-ZWRYBGRD0Y | `10_reverb-and-spatial-processing/feedback-delay-networks.md` | Incommensurate delay lengths, for the Delay diffusion all-passes |
| SRC-3Q84R4ZMZG | Laroche and Dolson phase vocoder | The existing STFT pitch engine; why its window is the latency floor |
| IMP-WT4XJFQT9E | `05_IMPLEMENTATION_LIBRARY/cpp-and-juce/juce-apvts-parameters-and-state-pattern.md` | Append-only parameter ids, versioned state, pluginval |

Searched with nothing new found: `pluginval` (only the APVTS note), and "zero latency" beyond the real-time rules.
