# IH1 V3 (feedback round 3): what changed, why, and the evidence

Version **0.3.0** (test build, round 3). This pass implements `IH1_V3_FEEDBACK_MASTER_PROMPT.md` with
`IH1_V3_RESEARCH_SUPPLEMENT.md`, KADAL (`R4N-WAV/Kadal`), the reference video and the V2.1 code. Open items are in
`docs/KNOWN_ISSUES.md` (V3 section); the FL Studio checklist is `docs/FL_STUDIO_TEST_CHECKLIST.md`.

## 0. Starting point and inputs

- The session branch `claude/amazing-noether-7iiyjr` was at `0079915` (V1). The V2.1 work (0.2.1) was on
  `claude/sleepy-goodall-frh3kc`, a strict descendant, so the branch was fast-forwarded to it. Nothing was discarded.
- `IH1_V2_1_DEEP_RESEARCH.md` and `IH1_V2_RESEARCH_SUPPLEMENT.md` are not in the repository and were not supplied in
  this session. Their requirements were taken from `docs/V2_1_IMPLEMENTATION.md`, `docs/KNOWN_ISSUES.md` and the
  V1 briefs in `docs/source/`.
- Baseline before any change: unit tests 101/101, `ih1_hosttest` 56/56, `ih1_uitest` passing.

## 1. Diagnosis (reproduced before fixing)

| Feedback | Symptom | Root cause found in code | Fix | Proof |
|---|---|---|---|---|
| 8 | Sharp level jump when landing on Saturation | A freshly staged Saturation starts at unity auto-gain with empty 300 ms RMS trackers, so the first ~0.3 s play the full drive gain. Measured with `tools/ih1_safety_audit`: **+5.4 dB** at defaults, **+23 dB** with a hot cached draft (clamped at the +12 dBFS engine ceiling). | Conservative 1/drive start, 40 ms warm-up with fast trackers, then fast-down / slow-up gain tracking. (`core/src/fx/Tone.cpp`) | Defaults now < 0.5 dB; worst draft (36 dB drive and +12 dB output) +3.3 dB. `test_v3`: hot cached draft < 2 dB. |
| 8 (found) | Same class: Transient Shaper | Detectors reset to -120 dB, so the first signal read as a ~100 dB "attack": **+14 dB** at defaults, peak hitting +12 dBFS. Also after any digital silence in normal use. | -60 dB detector floor, warm start, 20 ms warm-up, ±18 dB boost. (`Dynamics.cpp`) | `test_v3` < 2 dB; audit clean. |
| 8 | Clicks / pops when staging and loading | Every delay-line / latency FX starts with empty buffers, so its first delayed copy (or latency-delayed dry path) begins mid-waveform tens of ms after the engine's 10 ms crossfade. Measured step ratio on a sine: up to **22x** (Pitch Correction), 9-18x (Doubler), 3-5x (Delay, Chorus, Flanger, Convolution, Harmony, Spectral...). | The engine gives every freshly built processor (stage, RESET, preset load, latency rebuild; not COMMIT, whose processor keeps playing) a 12 ms raised-cosine **input** fade. (`Engine.cpp`) | All step flags gone (≤ 1.1x). `test_v3` fresh-input test on 7 FX. |
| 8 | Grain engine clicks | (a) Loop holds the write head but the spawn clamp assumed it moves, so a fast grain ran past the head into stale ring data; (b) when Freeze/Loop latches or un-freezes mid-grain, a grain could cross the held head or the overwrite front. `tools/ih1_grain_clicks`: 20 of 600 random settings clicked. | Hold-aware clamp; a per-grain seam guard predicts a boundary crossing 128 samples ahead and fades that grain out in time; spawn keeps clear of the overwrite front. (`GrainEngine.cpp`) | 2 of 600 remaining, both ≤ 2.1x the smooth bound (saturated pitched feedback, not seams). `test_v3` seam test. |
| 8 | No final protection | The only output protection was a +12 dBFS hard clamp. | **Output Safety** (section 3). | Unit tests + host test (+24 dB in/out held at the ceiling). |
| 1 | Preset + previously staged FX heard together | `Session::fromJson(..., includeSession=false)` (every preset load) explicitly rebuilt the staged processor: "the staged draft stays staged". | Every recall lands on **None** (section 2). | Null test for every FX type (section 7). |
| 2 | No None in the right-click list | The list was categories only. | `None` first row, same path as the wheel. | `ih1_uitest`. |
| 4 | Stable formation lit before the transfer finished | On a matched transfer request the instance got `presence = 1` immediately, and presence drives its node field and glow; an instance whose chain snapshot arrived first also started growing. | Explicit lifecycle (section 4). | `test_formation` V3 cases, `test_field_volume`. |
| 3 | 2-D lines and foggy lighting | The renderer drew `GL_LINES` strands, blurred them at quarter resolution (bloom gain 0.45) and added Gaussian glows. | Ray-marched field volume (section 5). | Stills in `docs/img/v3_*`, `test_field_volume`. |
| 5 | Presets preserve too much of the input | Measured, not assumed (section 6): the engine could do it; the V2.1 bank used it timidly. **13 of 66** creative presets were transformative (Granular Motion 0 of 7), e.g. granular at 55 % mix into a 30 % reverb. | New bank + missing modules. | 56 of 61 transformative. |
| 6, 9 | Downsampling / dirt only as one Bitcrusher | No shared rate-reduction component, no character inside feedback loops. | Shared `RateReducer`, new modules and character stages (section 6). | `test_v3_dsp`. |

## 2. Workflow and state (Phases 1, 2, 7.1, 10)

- **None is a first-class state**: `Session::kNone` (never a registry index, so it cannot alias the first FX) and
  `hasStaged()`. With None the audio program contains no staged processor at all; it is not a hidden-highlight bypass.
- **Every recall lands on None**: factory and user presets, preset arrows, the browser, Init, and host state
  (`setStateInformation`). The old staged processor leaves with the old program through the normal hand-over (5 ms
  fade out, install, fade in at a block boundary). Drafts are a separate cache: a preset never touches them; a
  project state restores the drafts it saved. Returning to an FX finds its cached values until RESET.
- **Host side**: on recall SOLO / MUTE are set off, the eight *Staged Control* parameters re-sync to "nothing
  staged", and the host gets `updateHostDisplay(programChanged)`. Parameter ids are unchanged; new ids are appended.
- **Right-click list**: `None` first, ticked when nothing is staged, divided from the categories by a faint hairline,
  dispatched through the same `onSelect(-1)` as the wheel's None. Nested categories unchanged.
- **Init** (`f:init`, pinned first): `Session::initialise()` clears the chain, sets None, resets every draft, turns
  SOLO / MUTE off and retires every processor (no loop, freeze, feedback or modulation memory survives); input /
  output gain, mix and Output Safety return to defaults. The latency mode is kept (a host-latency preference).
- **Conflict documented**: V2.1 restored the staged selection with a DAW project. Feedback 1 asks for host recall to
  land on None as well, so a reopened project now shows None (its drafts are restored). Chosen because FL Studio's
  own plug-in presets arrive through the same host-state call.

## 3. Output Safety (Phase 8.5)

`core/include/ih1/dsp/OutputSafety.h`, the last stage after every FX and the output gain.

| Mode | Latency | What it does | What it does not do |
|---|---|---|---|
| Off | 0 | Nothing. Internal guards (finite checks, bounded feedback, de-click transitions, +12 dBFS last-resort clamp) stay on. | - |
| **Guard** (default, -0.5 dBFS) | 0 | Instant-attack, smooth-release sample-peak limiter: no sample leaves above the ceiling. Fully transparent below (difference < -60 dB after release). | It cannot see inter-sample peaks; heavy overs are effectively clipped on their first sample. It is not called true-peak protection anywhere. |
| True Peak | 1.5 ms (72 samples at 48 kHz) + 4 detector | 4x oversampled (BS.1770-style) peak detector, min-hold + box-average gain over the lookahead (KADAL KNW-S6PPRRX2EC, CLM-5HTBR5YBFQ), 0.5 dB margin for the detector's residual under-read. Latency is reported to the host. | In **Zero Latency** mode it is unavailable (no lookahead); Guard runs instead and the UI says so. |

- Mode changes fade out / switch / fade in (no latency jump mid-signal). Host parameters `safety_mode`,
  `safety_ceiling` (appended). The chain menu (···) holds the modes and ceilings; the strip shows a compact output
  meter with gain reduction; a toast warns when safety is Off while the chain uses high feedback or drive.
- Measured: Guard holds -1 dBFS on a +12 dB sine burst; True Peak keeps an fs/4 sine at 45° (all samples 3 dB below
  its true peak) driven 6 dB over to within +0.25 dBTP of the ceiling; reported latency == impulse offset.
- **No plug-in can guarantee speaker or hearing safety**; the UI tooltip and README say so.

## 4. Commit lifecycle (Phase 3)

`Formation::Life`: **Probation** (a new instance just appeared; up to 0.25 s invisible while a COMMIT transfer may
still arrive) -> **Transferring** (only the travelling packet exists; the instance has zero field strength, no
strands, no glow, no incoming bundles) -> **Integrated** (on the packet's arrival event: its light fades in over
0.35 s and an arrival pulse fires). Without a transfer (preset load) probation ends in a plain fade-in.

One packet per id (repeated / double-click requests are ignored), a request for an already integrated instance never
replays, a new editor (reopen, GL reset) starts Integrated, and audio never waits for any of it. Tests: first,
repeated, duplicate, six quick commits, preset load mid-transfer, reopen mid-transfer.

## 5. Formation rendering (Phase 3A)

`core/include/ih1/visual/FieldVolume.h`. The formation is integrated along camera rays through an emission /
absorption volume. Nothing is a line primitive and nothing is a blur.

- Each committed instance is a dipole. Its light lives on the surfaces its field lines lie on: for a dipole
  r = L sin²θ, so L = r / sin²θ is constant along a field line. Thin luminous sheets at log-spaced L, displaced by
  3-D noise (crumpled, creased), broken by an irregular azimuthal pattern and slow 3-D patches.
- Creased membranes: thin ridges where a warped noise field crosses its midpoint, only inside the strong field.
- A compact flux-density body (|B| ~ 1/r³, strongest at the poles) and a defined core per node.
- Links: twisted flux sheets between consecutive nodes, colour-graded; onset pulses travel along them.
- The whole field is domain-warped by a smooth, slow, deterministic flow: coherent motion, no swirl or twitch.
- Emission doubles as density (front structure occludes what is behind); far structure dims and cools to indigo.
  Rays march only inside the formation's bounding sphere: the void is exactly black (tested).
- Palette, family colours and duplicate shades are unchanged (the registry is append-only, so no tier shifts).
- One model, two implementations: the CPU reference `renderVolume()` (software fallback, `formation_preview`,
  tests) and GLSL generated by `fieldFragmentShader()` from the same constants.
- GL path: half-resolution march, temporal accumulation (weight 0.35, jitter rotates per frame), bloom 0.10 (was
  0.45), Gaussian glow layer removed. **Adaptive cost**: the volume pass is timed every 20th frame and steps between
  64 samples at half resolution and 16 at a quarter. Software fallback: the same volume on a background thread.
- Evidence: `docs/img/v3_formation_strands_vs_volume.jpg` (V2.1 left, V3 right), `v3_formation_volume_6fx.jpg`,
  `v3_commit_transfer_sequence.jpg` (packet at 1.2 s, arrival 2.0 s, the new region only after arrival; 12 FX),
  `v3_formation_solo_mono.jpg`. The GL shader compiles and renders on Mesa llvmpipe (uitest asserts no fallback).
- Honest comparison: the volume removes the drawn-line and fog look and reads as a field occupying depth, but it
  is softer than V2.1's sharp "electric" hair-lines, and it does not yet reach the reference's density of folded
  veils. GPU cost has not been measured on real Windows GPUs (section 9).

## 6. Sound design (Phases 4, 5, 6, 7)

### 6.1 Capability audit (engine vs presets)

| Question | Answer from the code |
|---|---|
| Real buffer capture with independent read position? | Yes: Grain Engine (12 s stereo ring, up to 64 voices, Hermite reads). |
| Size / density / count / overlap / pitch / reverse / pan / offset / feedback independently? | Yes, plus spray, scale quantisation, retrigger, ratio lock, grain delay, L/R streams. |
| Stretch really changes duration? | Yes: Stretch mode is granular overlap-add with WSOLA alignment; Varispeed is separate. |
| 100 % wet? | Yes (Mix 100, Flat law). |
| Spectral smear beyond reverb? | Partly: Spectral Freeze had blur / smear / shimmer. No spectral delay, gate, dropouts or phase scatter. |
| Tape stop / half-time / scrub? | No. |
| Character in feedback loops? | No. Rate reduction only as the standalone Bitcrusher. |
| Presets using it? | Mostly not: dry dominant, 20-55 % mixes, one-domain chains (13 of 66 transformative). |

Conclusion: the weakness was mainly preset design; the engine needed specific additions, not a rewrite.

### 6.2 New DSP

- **`dsp::RateReducer`** (shared): sample-and-hold rate reduction with raw aliasing or an anti-alias pre-filter
  (Off / Soft / Strong), a reconstruction filter (Off / Soft / Strong), separate bit-depth quantiser (optional TPDF
  dither), clock jitter, linked or independent stereo clocks; exact bypass when off. The terms are kept apart:
  decimation with anti-aliasing vs. deliberate aliasing vs. bit depth vs. oversampling (never done here).
- New modules (appended, so accepted colours are unchanged):
  - **Degrade** (Tone & Character): drive -> bandwidth collapse -> RateReducer -> clock wobble and noise floor.
  - **Fold & Crush**: dynamic wavefolding (drive follows the envelope) with a transient- or sustain-selective
    crusher; oversampled with the same latency variants as Saturation.
  - **Spectral Smear**: frequency smear with up/down bias, time smear (seconds of spectral memory), stereo-coherent
    phase scatter, spectral gate with hysteresis, Disintegrate (partials drop out and return), Hold.
  - **Spectral Delay**: 24 log-spaced bands with their own delay (Rising, Falling, V, Comb, Scatter), per-band
    bounded feedback, high damping, stereo offset, tilt.
  - **Time Warp**: Varispeed, Tape Stop, Half-Time, Scrub, Backspin; manual or tempo-grid triggered; **Keep Pitch**
    (granular read) for Varispeed / Half-Time. At rest it passes the input exactly (no hidden latency).
  - **Glitch Scatter**: per-step micro-loops with ratchets, reversed fragments, scan jumps, octave fragments, gates,
    lo-fi on glitched steps; seeded per step (the same bar does the same thing every playback). Idle = exact input.
- Character stages: Delay **Repeats Rate / Repeats Bits** (inside the loop: every pass degrades further); Grain
  Engine **Feedback Rate / Feedback Drive**. Appended ids, defaults are off.
- Grain Engine Pitch Jitter range 0-12 -> 0-36 st (several-octave spread on the knob; not a host-automated control).
- `kMaxParams` 32 -> 48 (state is stored by id; the registry truncates rather than overflowing).
- Not adopted: Signalsmith Stretch or a phase-vocoder / transient-hybrid TSM. Granular TSM, varispeed and the
  STFT pitch engine cover the modes; a library dependency was not justified this round.
- Not done: preset macros. A macro that moves committed parameters conflicts with invariant 4 (committed FX are
  immutable); a design that respects it (e.g. preset-level performance controls outside the committed values) is
  left for a decision by the owner.

### 6.3 Factory bank

`docs/FACTORY_PRESETS.md`, measured in `docs/V3_PRESET_AUDIT_TABLE.md`. Init + 5 utility, 61 creative in ten
categories (6-7 each). Legacy ids are kept where a concept continues (bolder values), so project references resolve.

Audition (`core/presets/Audition`, `tools/ih1_preset_audit`): nine synthesised sources (vocal phrase, sustained vocal,
drum loop, snare hits, bass phrase, guitar chord, keys, pad, field recording; -18 dBFS RMS, peaks ≤ -1 dBFS). Every
preset renders on the sources its tags name; output is **level-matched** before judging (louder never counts as
bolder). Transformative = correlation with the dry input < 0.5 or timbre distance > 6 dB on active frames.

| | V2.1 bank | V3 bank |
|---|---|---|
| Creative presets transformative | 13 of 66 (20 %) | **56 of 61 (92 %)** |
| Level / peak / mono-safety failures | 1 | 0 |

The five that are not are deliberate character presets (e.g. *Sampler Ghost*, the subtle vintage-sampler texture
the prompt asks for next to *Alias Grinder*). A test enforces ≥ 2/3. Renders: `docs/audio_v3/` (5 dry sources, 12
processed presets on suitable sources). The sources are stand-ins, not recordings: listening on real material is the
owner's call.

## 7. Evidence: what was run (Linux container, 4 cores, Mesa llvmpipe, no GPU, no Windows)

| Check | Result |
|---|---|
| Unit, behaviour, audit tests (`ih1_tests`) | **130 / 130** (V2.1: 101) |
| Preset recall null test, every one of 44 FX staged with a modified draft | output == preset chain alone (error < 1e-5) |
| `ih1_safety_audit` (450 landing cases: defaults, hot, random drafts, two input levels) | 11 flagged, all explained in KNOWN_ISSUES (was 123 flags, 71 after the metric fix) |
| `ih1_grain_clicks` (600 random settings, 16 s runs included) | 2 marginal (≤ 2.1x); 5 when the intentional feedback sample-and-hold is on |
| `ih1_hosttest` (built VST3 via JUCE's VST3 host) | **56 / 56**: latency reported == measured in every mode, +24 dB held at the safety ceiling, resize / reopen stress |
| `ih1_uitest` (real editor, events through the peer), GL and software modes | pass, incl. V3: None row, rapid None, preset arrows / browser / Init land on None with SOLO off, drafts cached |
| ASan + UBSan (+ LeakSanitizer) build of `ih1_tests` | **all pass, 8.79 M assertions, 0 reports** |
| GitHub Actions (Linux + Windows: build, tests, host tests, UI tests, pluginval, installer) | passed for every V3 commit up to the preset bank. The renderer commit failed 2 Windows editor checks on a GPU-less 2-vCPU runner: the software volume's worker threads starved the UI thread. Fixed (single low-priority thread); reproduced and passing locally pinned to 2 cores. |
| FL Studio | **Not tested** (no Windows / FL Studio here) |

## 8. State, compatibility, versions

| Change | Compatibility |
|---|---|
| State document `ih1-state` version 1 | unchanged; still writes `selected` / `solo` / `mute` for older builds, ignores them on load (None) |
| New host parameters `safety_mode`, `safety_ceiling` | appended, version hint 2; older projects load the defaults (Guard, -0.5 dBFS) |
| Delay `lofi_rate` / `lofi_bits`, Grain `fb_rate` / `fb_drive` | appended, default off: old projects sound identical |
| Grain `pitch_jit` range 0-36 | stored values stay valid |
| Six new FX types | older builds skip them with their "unknown FX" warning |
| Factory preset ids | legacy ids kept where the concept continues; removed ids only lose their name in the preset box |
| Version | 0.2.1 -> **0.3.0** (features, compatible). "Round 3" is a label in the installer and ZIP name only. |

## 9. Installer (Phase 12)

Inno Setup kept. Panel / header images and the icon are rendered from the formation itself
(`installer/branding`, made with `tools/formation_preview` + ImageMagick); near-black header banner; welcome and
finish pages state the version, test round, VST3 path, that `Documents\IH1\Presets` is never touched, and the FL
Studio rescan steps. CI builds `IH1-Setup-0.3.0.exe` and `IH1_0.3.0_Windows_TestBuild_Round-3.zip` (installer +
README), verifies the archive opens with exactly those two entries, and uploads it as `IH1-windows-testbuild-zip`.
Not verified here: running the installer / uninstaller on Windows and FL Studio discovering the plug-in.

## 10. KADAL sources

| ID | Path | Used for |
|---|---|---|
| KNW-S6PPRRX2EC (CLM-YT31GZ87CQ, CLM-DGG9BKGM5K, CLM-5HTBR5YBFQ) | `03_DSP_KNOWLEDGE/09_dynamics-and-compression/true-peak-measurement-and-ceilings-for-clippers-and-limiters.md` | True Peak mode: 4x detector, min-hold + box lookahead, margin for under-read, honest latency |
| KNW-01J68SK1RC | `03_DSP_KNOWLEDGE/12_granular-processing/granular-synthesis-and-granulation.md` | Grain seam guard, bounded pool, normalisation (reused from V2.1) |
| KNW-M8Q0G3M7NM | `03_DSP_KNOWLEDGE/16_sampling-and-playback/bandlimited-interpolation-resampling-and-sample-playback.md` | Hermite reads in Time Warp / Glitch; sample-and-hold vs band-limited resampling distinction |
| KNW-JC0N8AGZ90, KNW-MCXD9ZQP83, KNW-F7DN55HDGE | `03_DSP_KNOWLEDGE/03_nonlinear-processing/...` | Fold & Crush oversampling variants (as Saturation) |
| MTH-86XWEEZ4C3 | `06_BENCHMARKS_AND_TESTING/measurement-methods/aliasing-measurement-nonlinear-processors.md` | RateReducer alias test (10 kHz through a 12 kHz hold) |
| KNW-H0EPVHBYCE, KNW-PY324W089P | `03_DSP_KNOWLEDGE/19_real-time-optimization/...` | No allocation / locks on the audio thread; denormals in new modules |
| SRC-5Q7S4HYRH5 (Driedger & Müller TSM review) | `02_SOURCE_LIBRARY/...` | Why Time Warp offers varispeed and a granular Keep Pitch rather than claiming PV-quality stretch |

**Missing in KADAL** (searched: "downsampl", "sample-and-hold", "bitcrush", "decimat", "wavefold", "spectral delay"):
no dedicated note on deliberate sample-rate reduction / bit crushing, wavefolding, spectral delay or volumetric
field rendering. Nothing was added to KADAL in this session; the research supplement's Part II 7.2 and this
document are the reference until a KADAL note is written.

## 11. Files

- Engine / safety: `core/src/engine/Engine.cpp`, `core/include/ih1/engine/Engine.h`, `core/*/dsp/OutputSafety.*`
- State: `core/*/model/Session.*`, `plugin/Source/PluginProcessor.*`
- DSP: `core/include/ih1/dsp/RateReducer.h`, `core/src/fx/V3Fx.cpp`, `Tone.cpp`, `Dynamics.cpp`, `Delay.cpp`,
  `GrainEngine.*`, `GrainEngineFx.cpp`, `Registry.cpp`, `RegisterAll.cpp`, `model/Param.h`
- Presets: `core/src/presets/Presets.cpp`, `core/*/presets/Audition.*`
- Visual: `core/*/visual/FieldVolume.*`, `core/*/visual/Formation.*`, `plugin/Source/FormationView.*`
- UI: `plugin/Source/Carousel.*`, `Widgets.*`
- Tools: `tools/ih1_safety_audit.cpp`, `ih1_grain_clicks.cpp`, `ih1_preset_audit.cpp`, `formation_preview.cpp`
- Tests: `tests/test_v3.cpp`, `test_v3_dsp.cpp`, `test_field_volume.cpp`, V3 cases in `test_formation.cpp`,
  `test_presets.cpp`, `tools/uitest/UiTest.cpp`
- Installer / CI: `installer/IH1.iss`, `installer/branding/*`, `installer/README_TESTBUILD.txt`, `.github/workflows/build.yml`
