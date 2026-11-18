# Decisions

All Phase 1 decisions were resolved by the product owner on 2026-11-18. Items marked *(default)* were accepted as recommended.

| # | Decision | Resolution |
|---|---|---|
| A1 | Framework / licence | Owner delegated. **JUCE 9.0.5** (pinned commit `d60b7a78`) wraps a **JUCE-free C++20 engine `ih1_core`** (the owner's Isai pattern, KADAL `DOS-17T0VBY0CF`). Used under the **JUCE 9 EULA Starter tier** (free up to USD 20k revenue/funding, no splash screen in JUCE 9). **Move to Indie before exceeding that threshold**, or relicense as AGPLv3. The engine never includes JUCE headers, so the wrapper can be replaced by a raw VST3 SDK (MIT) wrapper later. |
| A2 | Platforms | **Windows x64 VST3 first**, plus a **setup wizard (installer)**. Code stays portable; Linux builds/tests in CI; macOS later. |
| B1 | DAW state vs presets | User presets = committed chain only. DAW project state = committed chain **+ per-type drafts + carousel selection + SOLO/MUTE**. |
| B2 | Draft after COMMIT | *(default)* The committed type keeps its draft values; the carousel selection clears. |
| B3 | Topology | Linear serial chain. "Tree/supercluster" is visual only. |
| B4 | Click on committed instance | *(default)* Hover = read-only tooltip; click/drag = reorder only. |
| C1 | Harmony input | **Both**: MIDI-note mode (VST3 event input) and key/scale/interval mode. |
| C2 | Reverse | *(default)* Reverse Delay, Reverse Buffer, Reverse Grains. Reverse reverb is a Reverb mode. |
| C3 | Looper audio | *(default)* Parameters only; captured audio is runtime-only. |
| C4 | Carousel at 37 items | *(default)* Sketch carousel + family dividers + type-to-jump. |
| D1 | Host automation | **Fixed set**: Bypass, SOLO, MUTE, Input Gain, Output Gain, Global Mix, Staged Control 1–8 (mapped to the staged FX's first 8 controls). Committed instances are never exposed. |
