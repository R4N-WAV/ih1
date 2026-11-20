# QA and regression results

## V2.1 pass

Every result below was run and observed in this pass, on the same Linux container as the V1 pass:
4 vCPU, GCC 13.3, JUCE 9.0.5, Xvfb and Mesa 25.2.8 llvmpipe. FL Studio, Windows, macOS, GPUs and DPI scaling
were **not available**. The FL Studio acceptance list is `docs/FL_STUDIO_TEST_CHECKLIST.md`.

| Suite | Baseline (before V2.1) | V2.1 final | Notes |
|---|---|---|---|
| `ih1_tests` | 78 / 78 | **@UNIT@** | 23 new cases (below) |
| `ih1_hosttest` (real VST3 through JUCE's VST3 host) | 48 / 48 | **@HOST@** | New: latency modes through the VST3 state path. Host-reported == impulse-measured in every mode; the zero-capable chain's processed impulse arrives at +1. |
| `ih1_uitest` (real editor, native-peer events, OpenGL attached) | pass (44) | **@UI@** | New: nested menu, 120-click preset-arrow stress, Clear FX Chain paths, latency menu, Sync-disabled controls, Advanced visible, MUTE greying, GL formation active |
| pluginval v1.0.4 | not run locally | **SUCCESS at strictness 8 and 10** | Linux, Xvfb (`--validate IH1.vst3`) |
| ASan + UBSan (`ih1_tests`, leak detection on) | — | **@ASAN@** | No memory error, no UB, no leak |
| TSan (threaded stress with clear / latency switches, clear-chain, session-latency) | — | **3 / 3, 0 warnings** | |
| TSan soak (`ih1_soak` 180 s, audio + message threads) | — | **@TSANSOAK@** | |
| 30-minute soak (`ih1_soak 1800 12`) | — | **@SOAK@** | |
| CPU per block (thread CPU time, 80,000 × 256-sample blocks, random ops, chains up to 12 heavy FX) | — | median 0.12 ms, p99 1.3 ms, p99.9 2.0 ms, max 4.7 ms (budget 5.33 ms) | No operation causes a pathological spike. Wall-clock outliers in the soak come from CPU contention with parallel builds (see Notes). |

### New unit cases (`tests/test_v21.cpp` and additions)

- **Grain Engine (6):**
  - level within ±6 dB across 15–800 ms × 4–120 /s;
  - Stretch keeps 440 Hz, and Varispeed halves it;
  - Freeze sustains;
  - Octave quantisation;
  - maximum settings in all modes with 100 % feedback stay bounded and decay;
  - random automation every 64 samples stays finite.
- **Latency (6):**
  - every production FX resolves Zero / Low honestly;
  - oversampled FX: reported == measured broadband delay in all three variants;
  - Zero: impulse sample-aligned through the shapers;
  - Zero convolution == Full advanced by 256 (error < -60 dB);
  - session: contributors and state, with old states loading as Full Quality;
  - engine chain impulse aligned.
- **Tempo sync (2):** every Sync FX has exactly one data-driven dependency and Division stays live; the dormant time has no audible effect and is kept.
- **Clear chain (1):** duplicates, staged kept, telemetry empty, state empty, preset after clear.
- **Formation (5):** bottom-left → top-right with no T; a transfer before its instance is not lost; SOLO monochrome and colour return; audio light bounded and transient pulses; glow only with FX.
- **Presets (2):** at least 50 with no near-duplicates; the factory audit (198 renders: level, peak, mono, determinism).
- **DSP (2):** polyphase upsampler == direct form; delay diffusion (off = bit-identical, on = more than 4x denser, bounded).
- **Threaded stress:** now includes Clear FX Chain and latency-mode switches.

### Measurements

- **Latency and aliasing:** `docs/V2_1_IMPLEMENTATION.md` §3. On Saturation at +24 dB drive with a 5 kHz tone, the largest alias component is:

  | Style | Full | Low | Zero |
  |---|---|---|---|
  | Tube | -53.9 dBc | -54.0 dBc | -30.0 dBc |
  | Fold | -94.3 dBc | -94.3 dBc | -5.3 dBc |
- **Polyphase upsampler:** identical to the V2.0 direct form within 3.6e-7, and 2.22x faster (479 → 216 ms for 20,000 × 512-sample blocks).
- **Factory audit:** every preset on a drum loop, a pad chord and vocal-like formant pulses at -18 dBFS RMS.
  - Level relative to the source: -15 to +8 dB.
  - All peaks below +6 dBFS.
  - L/R correlation > -0.3.
  - Repeat renders bit-identical.
- **Formation motion (60 fps):**
  - 99th percentile 0.25 % of the field height per frame;
  - max 2 % (a smoothed branch-switch glide, not a pop);
  - simulation 6–8 ms per frame for 6 FX and about 13 ms for 12 FX (Medium, this CPU).

### Defects found by the V2.1 audit and fixed

These are in addition to the feedback items; the full diagnosis is in `docs/V2_1_IMPLEMENTATION.md` §1.

- **Destructive buttons never turned red.** A V1 bug in the look-and-feel; now red on the final destructive action only.
- **The confirmation dialog only covered the field.** A click in the left column neither cancelled nor was blocked. It is now editor-modal.
- **Left arrow did nothing on a hover-opened submenu.** Fixed.
- **Loop / Freeze presets would loop silence.** Loading during silence captured the empty buffer. They now arm and latch once real signal arrives.
- **Six presets failed the new audit** (a single-FX chain, level, a transient shaper reacting to grain onsets). Fixed.
- **My own first Zero-mode shaper note understated the aliasing.** It was measured, the Low variant was added, and the note now states the numbers.

### Notes

- **The soak drives the engine as fast as possible** (dozens of times real time) on a 4-core container. Builds, pluginval and the TSan runs shared those cores, so its wall-clock "worst block" includes pre-emption. The CPU-time profile above is the meaningful real-time figure.
- **Spectral FX compute an STFT frame inside the block where a hop completes.** A 1-sample host block can therefore exceed its 21 µs budget. This is listed in `docs/KNOWN_ISSUES.md`.

---

# QA and regression results: V1 feedback pass

Every result below was run and observed in this pass. Anything not run is listed under *Not tested*.

## Environment

| Item | Value |
|---|---|
| OS | Linux 6.18 (Ubuntu 24.04 container), 4 vCPU Intel Xeon @ 2.3 GHz, 16 GB |
| Toolchain | GCC 13.3, CMake 3.28, Ninja, Release with LTO |
| Framework | JUCE 9.0.5 (pinned `d60b7a7`), with the VST3 SDK bundled in JUCE |
| Display / GL | Xvfb 1920×1200×24, Mesa 25.2.8 llvmpipe (OpenGL 4.5 core, software rasteriser) |
| Hosts | `ih1_hosttest` (JUCE VST3 hosting, loads the built `IH1.vst3`), the JUCE standalone build, and `ih1_uitest` (editor driven through its native peer) |
| **Not available** | FL Studio, Cubase, Windows, macOS, GPU drivers, DPI scaling, pluginval (not downloaded in this pass; CI runs it) |

## Build

- `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` followed by `cmake --build build` builds all targets: `IH1.vst3`, the standalone app, `ih1_tests`, `ih1_hosttest`, `ih1_uitest`, `ih1_bench`, `formation_preview` and `fx_dump`.
- No new compiler warnings come from IH1 sources. The one remaining warning, `-Wfloat-equal` in `core/include/ih1/dsp/Basics.h`, predates this pass.
- The baseline V1 build also needed the ALSA/X11 dev packages listed in CI. Once installed, it built and its 68 tests passed.

## Automated suites (final build)

| Suite | Result | What it covers |
|---|---|---|
| `ih1_tests` (doctest) | **78 / 78 cases, 4,696,301 assertions: PASS** | The 68 V1 cases, plus 10 new cases (below). |
| `ih1_hosttest` | **48 / 48 checks: PASS** | VST3 load, parameters, 44.1–192 kHz × 32–1024 blocks, chain via state, automation storms (blocks 512/17/1/300), silence after tails, bypass equals delayed input, state round trip and null test, malformed state, create/destroy cycles. New section 9 covers the editor under host resizing. |
| `ih1_uitest` (software formation) | **44 / 44: PASS** | See the interaction matrix below. |
| `ih1_uitest` (OpenGL attached) | **44 / 44: PASS** | The same checks with the GL renderer running. |
| `ih1_bench` | Completed | Per-FX CPU (48 kHz / 256): Vocoder 0.61 %, Harmony 3.02 % (unchanged). Duplicate-chain worst blocks are within budget except 32 × Harmony (91.7 %, a known V1 limit). |

The 10 new unit cases are:
- Looper RESET (staged and committed);
- formation size, determinism, continuity, reorder, transfer, black void, tone map and hostile input;
- palette world and separation;
- 4 preset-library cases.

### Regression tests that fail on V1

- **Looper RESET.** Checked by stashing the fix. V1 still plays the loop after RESET (silence check fails).
- **Dropdown macro echo.** Covered by the 400 ms ownership window. No hosted echo source exists here to replay the exact FL Studio behaviour.

## P0 resize stress (4.8): `ih1_hosttest` section 9

The editor of the loaded VST3 opens in a native host window. JUCE's VST3 host wrapper resizes it through `IPlugView::onSize`.

**Setup:**
- a 5-FX chain is loaded;
- an audio thread calls `processBlock` every ~4 ms the whole time.

**Sequence:**
- 320 resizes, cycling through:
  - random sizes (300–4300 × 200–3200, including sizes outside the limits);
  - alternating 400×300 and 4000×3000;
  - near the minimum;
  - very wide (2400–2700 × 700);
  - very tall (1120 × 2000–2300);
- 0–25 ms between resizes, so often no frame is rendered in between;
- the chain is replaced via state every 40 resizes;
- every 64 resizes, the editor is closed immediately after a resize and reopened.

**Result, final build:**
- PASS in 16.8 s, with 5 close/reopen cycles;
- 3,667 audio blocks processed, all finite;
- the editor then settles at a requested 1700×1000;
- 8 further quick open/close cycles succeeded;
- first-run size measured at **1500 × 900**.

This was also run on the previous build: PASS, 3,444 blocks.

**This is not an FL Studio test.** See `docs/KNOWN_ISSUES.md`.

## Interaction matrix: `ih1_uitest`

Mouse, wheel and key events are delivered through `ComponentPeer::handleMouseEvent`, `handleMouseWheel` and `handleKeyPress`, the same entry points the OS uses. After each action the **model** is checked, not only what is drawn.

| Area | Checks (all PASS) |
|---|---|
| Startup (4.9, 4.11) | Opens at 1500×900. Nothing staged (`None`). Settings panel hidden. |
| Right-click list (4.13) | Opens on right-click in the carousel, beside the pointer (within 10 px). Holds the full catalogue (43 rows with family headings). Opening stages nothing. Escape cancels. An outside click on COMMIT while an effect is staged closes the list and does **not** commit. Choosing Reverb stages it and does not commit. Near the window edge, the list flips and stays inside. Down + Enter stages the next entry. |
| Dropdowns (4.6) | First, middle, last, the current item again, and first: each applied exactly. **35 / 35** exact selections in total, including 30 rapid random ones; items off-screen are reached by wheel-scrolling inside the list. Escape and outside clicks never change the value. No instant dismissal: the list is still open after 600 ms. Press, drag onto an item, release selects it. The segmented selector applies the clicked option. |
| Wheel (5.1) | Eight 0.015 deltas accumulate into one step. No change while a list is open. Wheel over the settings panel never changes the staged FX. |
| Commit / delete / reorder (2.2, 4.1, 5.2) | COMMIT adds one instance, the carousel returns to `None` and the panel hides. Three commits including a duplicate. Dropping on DELETE asks first and deletes nothing yet. Escape cancels. Dropping elsewhere never deletes. Confirm deletes **exactly** the dragged duplicate, by id. Dragging along the strip reorders the audio chain. |
| Presets (4.15) | All 28 factory presets listed. Next walks the whole list in order and wraps to the start; previous wraps to the end. A fresh load is not marked modified; committing on top marks it modified. A malformed file (version 99) is rejected and the chain is untouched. |
| Lifecycle | Closing the editor with a list open, then reopening, is safe. |

## Live checks in the standalone app (Xvfb, OpenGL)

Driven with `xdotool`. Screenshots are in `docs/img/`.

| Check | Result |
|---|---|
| Startup: `None`, no settings box, black field, preset box `No preset` | Pass (`ui_v1_1_start.jpg`). |
| Delay staged: icy identity colour, tap graph, segmented and dropdown selectors | Pass. |
| Division dropdown: opens once above its anchor (no room below), current value marked; clicking `1/4` sets the model, label and graph (500 ms at 120 BPM) | Pass (`ui_v1_1_dropdown.jpg`). |
| Right-click list beside the pointer, coloured and grouped; choosing Reverb stages it | Pass (`ui_v1_1_rightclick_list.jpg`). |
| COMMIT: the packet travels from the left edge, disperses, the region grows, the carousel returns to `None` | Pass (`ih1_v1_1_formation.mp4`). |
| Staging Harmony while a 5-FX preset plays: the formation does not change; after COMMIT the formation adds a lavender region without resetting | Pass (same video). |
| All 28 factory presets stepped with `›`: each draws a distinct multicoloured formation within the palette on black | Pass (`factory_presets_grid.jpg`). |
| Preset browser: categories, list, description, footer actions | Pass (`ui_v1_1_preset_browser.jpg`). |
| Drag to DELETE: plain red text in the upper-left, no box | Pass (`ui_v1_1_drag_delete.jpg`). |
| Settings panels for EQ, Compressor, Saturation, Filter, Chorus, Harmony, Granular and Looper, each with a graph | Pass (`ui_v1_1_settings_panels.jpg`). |
| `IH1_NO_OPENGL=1` software formation | Pass. |
| GL render-target failure → automatic fallback | Code path present. Not exercised: GL cannot be made to fail on demand here. |

**Defects found and fixed during these checks:**
- The editor painted the field opaque over the GL frame, so no formation was visible.
- The first-run size was overwritten by `setResizeLimits` → `resized()`.
- The carousel focus tick appeared after a mouse selection.

## Visual comparison with the reference (4.10)

- `docs/img/formation_compare_v1_1.jpg` puts reference frames at about 3 s and 17 s next to IH1's *Supercluster* preset (5 FX) in GL.
- The reference contact sheet and frame analysis are in `docs/img/reference_contact_sheet.jpg` and `docs/VISUAL_REFERENCE_ANALYSIS.md`.
- **Matches:**
  - a black void with generous negative space;
  - an asymmetric right-of-centre plume crossing a receding arm;
  - fine luminous strands and smoky veils;
  - a pale concentration at the crossing;
  - slow parallax;
  - colour regions per FX within blue, violet, lavender, rose and ice.
- **Remaining differences:**
  - the reference's core is denser and whiter;
  - its iridescent edge fringes were left out on purpose (see `KNOWN_ISSUES.md`).
- **Motion, measured:**
  - per-frame strand motion at 60 fps stays below 0.4 % of the field height;
  - reorder moves glide, under 5 % per frame, without reseeding;
  - one transfer animation lasts 0.65 s plus 0.45 s of dispersal.

## Audio and performance

- **Sample rates and blocks:** 44.1, 48, 88.2, 96 and 192 kHz × 32, 64, 256 and 1024 samples. Finite and bounded (hosttest). Odd blocks of 1, 17 and 300 samples are also covered under automation.
- **Factory presets:** each of the 28 played 3 s at 48 kHz / 256: finite, peak < 4.0 (the safety ceiling), and not silent.
- **Vocoder level fix:** the output/input RMS ratio at 8, 12, 16, 24 and 32 bands is now -4.4 … +2.2 dB. In V1 it was clipped at the +12 dBFS ceiling for 16 bands and above.
- **RESET** on a playing Looper: the click metric is below the engine test's limit (0.025 per-sample step).
- **Formation simulation (CPU, Medium quality):**
  - 5 FX: about 5 ms per frame;
  - 6 FX: about 4–5 ms per frame;
  - 1 FX: under 1 ms.

  It drops to Low quality automatically if a frame takes more than 12 ms.
- **Mono layout:** the processor's mono fold-down path is unchanged from V1. It was not re-tested separately in this pass.

## Not tested / uncertain

- FL Studio and Cubase: no host here. This includes the actual resize crash, menus inside FL's plug-in window, bridged mode, project save and reopen, and multiple instances in a DAW.
- Windows and macOS builds in this pass. CI is configured to build and test on Windows; it was not run from here.
- DPI scaling (125 / 150 / 200 %) and multi-monitor setups.
- Real GPU drivers: frame rate, GPU load, context loss and recreation.
- pluginval, in this pass: CI runs it at strictness 5.
- Long-session memory growth: no multi-hour soak. There is no per-frame allocation in the renderer after warm-up; `verts_` and the segment buffers are reserved.
- User presets across an application or DAW restart: the file-based save and load path is exercised, but not across a real host restart.
