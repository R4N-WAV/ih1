# Open Decisions (Phase 1 approval gate)

Each item states a recommended default. Items marked **blocking** change architecture and need an answer before Phase 2 code locks in; the rest can proceed on the default and stay reversible.

Already settled (not re-asked): broad catalogue (37 target types, batched A→B→C); serial chain semantics for COMMIT/SOLO/MUTE/RESET; committed instances immutable (reorder/delete only); user presets save the committed chain; video governs the visuals.

## A. Framework & platform

**A1. Framework / licence — blocking.**
- **Your Isai pattern** (KADAL `DOS-17T0VBY0CF`): a plain C++17 DSP engine without JUCE, tested standalone, wrapped by JUCE (APVTS state, editor, VST3 export). It already passed pluginval strictness 5.
- **JUCE 9 licence** (KADAL `SRC-Q5ZA32G2B3`): Starter is free up to USD 20k revenue/funding; Indie (USD 40/month or 800 perpetual) up to 300k; Pro above that; or AGPLv3 if open source. Closed-source therefore does *not* automatically mean paying.
- **Alternative:** Steinberg VST3 SDK 3.8 directly (MIT) + VSTGUI/custom OpenGL. No licence questions, but much more UI and host plumbing work.
- *Recommendation:* reuse the Isai pattern: a JUCE-free `ih1_core` engine (all DSP, chain, state model, tests) plus a thin JUCE 9 wrapper and editor (OpenGL for the formation). Because the core is JUCE-free, the wrapper can be swapped for a raw VST3 SDK one later.
- *Question:* OK to use JUCE 9, and under which licence (Starter/Indie/Pro, or AGPL open source)?

**A2. Platforms.** *Recommendation:* Windows x64 VST3 first, keeping the code portable (CMake, no Win32-only APIs in the core) so macOS VST3/AU can be added later without a rewrite. Linux CI builds the DSP core and tests.

## B. Workflow semantics

**B1. DAW project state vs user presets — blocking (state schema).** *Recommendation:* user presets = committed chain only. DAW project state = committed chain **plus** the per-type draft store, selected carousel item, and SOLO/MUTE state, so reopening a project loses nothing.

**B2. Draft after COMMIT.** *Recommendation:* keep the committed type's draft values (so committing again makes an intentional duplicate). RESET is still there to go back to defaults. The carousel highlight still clears, as specified.

**B3. Chain topology — blocking.** *Recommendation:* a linear serial chain; "tree/supercluster" describes the visual only. Parallel branches would need a graph engine and a different drag/drop model.

**B4. Clicking a committed instance.** *Recommendation:* hover shows a read-only tooltip (FX name, position, key values); click does nothing except start a drag.

## C. FX specifics

**C1. Harmony input — blocking (adds a MIDI event bus).** *Recommendation:* both. MIDI-note mode (four voices, deterministic voice stealing, default "newest note wins") **plus** key/scale/interval mode, which works without any MIDI routing. FL Studio needs MIDI routed to an effect manually; Cubase does it natively.

**C2. Reverse scope.** *Recommendation:* the Reverse FX ships three real modes: **Reverse Delay** (timed reversed echoes with feedback), **Reverse Buffer** (continuous windowed reversed playback of fixed-length chunks) and **Reverse Grains** (overlapping reversed grains). Reverse reverb lives in Reverb as an algorithm.

**C3. Looper audio in state.** *Recommendation:* save parameters only; captured audio is runtime-only.

**C4. Carousel navigation at 37 items.** *Recommendation:* keep the carousel exactly as sketched, and add family dividers in the list plus an optional type-to-jump (start typing a name). No separate browser.

## D. Host automation

**D1. Automatable parameters — blocking (stable parameter IDs).** *Recommendation:* expose a fixed set: SOLO, MUTE, input gain, output gain, global mix, and **8 generic "staged macro" slots** that map to the current staged FX's top controls. Committed instances are never exposed. Alternative: no host automation at all apart from gain/mix.
