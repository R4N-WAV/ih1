# Known issues and unverified items

This list covers what remains open after the V1 feedback pass. Nothing on it is hidden elsewhere.

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
