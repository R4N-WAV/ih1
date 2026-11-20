# Known issues and unverified items

## V2.1 (this pass)

### Not verified in the target host
- **FL Studio was not tested.** No Windows or FL Studio here. Use `docs/FL_STUDIO_TEST_CHECKLIST.md`.
  - The latency-mode host notification (`setLatencySamples` → VST3 `restartComponent(kLatencyChanged)`) is verified only through JUCE's VST3 host in `ih1_hosttest`, where reported == impulse-measured in every mode.
  - Whether FL interrupts playback while recomputing delay compensation on a mode switch is unknown.
- **Windows and real GPUs.** pluginval passed at strictness 8 and 10, and the GL formation runs, only on Linux with Mesa llvmpipe. The Windows CI still runs pluginval at 5; it was not raised without a Windows result.

### Latency
- **No strict zero for four FX.** Harmony, Pitch Shift, Formant Shift and Spectral Freeze have no zero-latency variant. In Zero mode they run Low (512 samples at 48 kHz) and the status names them. A rotating-head time-domain shifter was rejected for Zero because its read head sits half a window behind on average, which is a real delay.
- **Zero-mode shapers alias more.** Saturation, Tape / Console, Exciter and Amp run at 1x with ADAA only in Zero mode: about 24 dB more aliasing at +24 dB drive, and the Fold style is very aliased (-5 dBc). The UI says so. Low (5 samples) keeps Full-Quality anti-aliasing.
- **Mode switches reset tails.** Switching modes rebuilds every processor variant, so tails restart (5 ms output crossfade). Latency never changes from parameter automation.
- **Minimum-phase dispersion.** "Zero" means no buffering, lookahead, analysis window or linear-phase filter. Causal filters (EQ, cabinet, tape shelves) keep their normal minimum-phase dispersion, like analogue gear. That is part of the sound and is not reported as latency.

### Formation
- **Occasional glides.** The fastest visible motion is a smoothed field-line branch switch on faint fringe strands, at most about 2 % of the field height per frame and decaying over about 0.5 s. The 99th percentile is 0.25 % per frame (`test_formation`).
- **Smoke texture.** The reference's smokier, sheet-like veils are approximated by wisps plus bloom. There is no volumetric veil layer.
- **Software fallback.** It renders at half resolution, so it looks softer than the GL path.
- **CPU cost.** The simulation is about 6–8 ms per frame for 6 FX and about 13 ms for 12 FX at Medium on this container's CPU. It drops to Low automatically above 12 ms. Real GPUs are not the bottleneck; the CPU tracing is.

### DSP
- **Grain Engine Stretch** is granular overlap-add with WSOLA-style alignment. It suits texture, pads and voice at 0.25–1x. Transients smear by design. There is no phase-vocoder or transient-aware hybrid TSM mode.
- **Grain Engine memory.** Each instance preallocates a 12 s stereo capture ring, rounded up to a power of two: about 8 MB at 44.1 / 48 kHz and about 32 MB at 176.4 / 192 kHz. It is bounded and never grows, but 32 instances at 192 kHz would use about 1 GB.
- **No modulation system or macros.** The architecture has no modulation matrix; Grain Engine randomness is per grain and deterministic. XY or macro control (Portal-style) is a future step.
- **Spectral FX CPU bursts.** STFT frames still compute in the block where a hop completes. With 1-sample host blocks that block can exceed its tiny budget (the soak tool reports it). At realistic blocks (≥ 128 samples) the worst block measured 4.7 ms of CPU time against a 5.33 ms budget with 12 heavy FX. Staggering hops is still open.
- **No blind listening test.** A level-matched blind comparison against commercial products was not done; it needs the owner.

### Presets
- **Silence on load.** Loop and Freeze presets wait for real signal before latching, so a preset loaded during silence is silent until sound arrives. This is intended.

---

## V1 feedback pass (earlier)

This list covers what remained open after the V1 feedback pass.

## Not verified in the target host

- **FL Studio: the resize crash (4.8) is not reproduced and not confirmed fixed.**
  - No FL Studio, Windows or GPU driver is available in this environment.
  - Every resize and GL lifecycle risk found in the code has been fixed (see `docs/V1_FEEDBACK_IMPLEMENTATION.md`, section P0).
  - A host-side stress harness now passes: `ih1_hosttest` section 9 runs 320 `IPlugView::onSize` resizes plus close/reopen cycles while audio runs, on Linux with Mesa OpenGL 4.5.
  - Still needed: in FL Studio (VST3, 64-bit, ideally both bridged and not bridged), resize the plug-in window fast and dramatically for a minute, with audio playing and a 5-FX chain loaded. Then close and reopen the editor straight away. Note the FL Studio build, the Windows version and the GPU.
- **FL Studio: dropdown selection (4.6) is not verified there.**
  - Every dropdown is now an in-editor surface, and the ownership problem found in JUCE's desktop popup no longer applies.
  - `ih1_uitest` passes 35 of 35 exact selections plus the cancel paths, with real peer events on Linux.
  - Still needed: the same check by hand in FL Studio.
- **Display scaling at 125 %, 150 % and 200 % has not been tested.** Xvfb has no DPI scaling. Logical sizes, limits and GL viewport maths use JUCE's rendering scale, and the internal render target is capped in physical pixels. A Windows check at each scale is needed.
- **No macOS build.** This is unchanged from V1.

## Visual

- **Fidelity to the reference video is close, not exact.** The formation now shares the reference's vocabulary:
  - a black void with a right-of-centre plume crossing a receding arm;
  - fine hair strands, smoky veils and a pale core;
  - slow parallax and per-FX colour regions.

  Differences remain. The reference's coloured "chromatic aberration" edge fringes (small cyan and orange glints) were left out on purpose, because they would read as extra colours unrelated to any FX. Its densest core is also brighter than IH1's at moderate chain lengths. Side-by-side comparisons are in `docs/img/`.
- **The formation simulation runs on the CPU, on the GL render thread.** It takes about 3–5 ms per frame for six FX at Medium quality on the test container. Only the strand polylines are uploaded each frame (a few hundred kB), not a persistent GPU particle system. The simulation drops to Low quality automatically if it takes more than 12 ms per frame. There is no user-facing quality setting yet.
- **Software OpenGL (llvmpipe) is slow.** With no GPU, rendering the whole editor through GL can use 2+ cores. Setting `IH1_NO_OPENGL=1` forces the lighter software formation. Real GPU drivers do not have this cost.
- **Single-FX formations are small by design.** "More FX = bigger formation" (UI sketch), so one FX draws a compact plume.

## Workflow

- **No undo** for delete or reorder (QoL 5.2 lists it as optional). The delete confirmation remains the safeguard.
- **Loop recordings are not saved.** A preset or project that contains a Looper loads with an empty loop. This is intentional: recorded audio is never stored in presets or project state.
- **Keyboard limits in the preset browser.** Typing goes to the search field. Arrow-key navigation of the list works only when the search field does not have focus. Mouse navigation is complete.
- **Chain strip overflow.** When a long chain overflows the chain strip, the extra items show as "+n" and cannot be dragged until the window is widened.

## Audio

- **Filter / Wah resonance gain (not changed).** In band-pass and Wah modes, the Filter boosts the centre frequency by Q × 1.5 or Q × 2.5. With high Resonance on narrow-band material this is loud: the peak can reach about +25 dB, and the existing output safety ceiling holds it. This is V1 behaviour and was left alone because it may be intended. The response graph shows it truthfully.
- **Mono and stereo only.** There is no sidechain input and no surround, the same as V1.
- **Latency of spectral effects.** Harmony, Pitch Shift, Formant Shift and Spectral Freeze add 2048 samples each. This is reported to the host and compensated on the SOLO and dry paths, the same as V1.
