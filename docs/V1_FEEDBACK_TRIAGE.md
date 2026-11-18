# IH1 V1 feedback triage

**Baseline:** commit `0079915` on `claude/new-session-5fkj8f`, JUCE 9.0.5 (pinned `d60b7a7`), VST3 SDK bundled with JUCE, CMake 3.28, GCC 13.3, Ninja. The clean Linux Release build passed, the 68 doctest cases passed (4,413,110 assertions), and `ih1_hosttest` passed (0 failures). The baseline UI was captured under Xvfb with Mesa llvmpipe (OpenGL 4.5 core).

**Environment limits:** FL Studio, Windows and real GPUs are not available in this container. Anything that needs them is marked *host-unverified*.

## Architecture map (as found)

| Area | Where | Notes |
|---|---|---|
| Engine (audio thread) | `core/src/engine/Engine.cpp` | Program snapshots handed over through a mailbox, with crossfaded transitions and retirement through an SPSC queue. RT-safe. |
| Model (message thread) | `core/src/model/Session.cpp` | Drafts, staged processor, immutable `CommittedInstance`, and JSON state. `fromJson` validates fully before applying. |
| FX | `core/src/fx/*` | 38 types. Schema listed with `tools/fx_dump`. |
| Plug-in wrapper | `plugin/Source/PluginProcessor.*` | APVTS with a fixed parameter set: Staged Control 1–8 macros polled at 30 Hz. |
| Editor | `plugin/Source/PluginEditor.*` | One `OpenGLContext` attached to the whole editor, with component painting and continuous repaint. |
| Formation | `core/src/visual/Formation.cpp`, `plugin/Source/FormationView.*` | 2-D particle advection drawn as additive line segments into a decaying RGBA16F FBO, then bloom. The FBO is reallocated on every size change. |
| Dropdowns | `SettingsPanel` uses `juce::ComboBox`; the Presets button uses `juce::PopupMenu` | Both are JUCE desktop popup windows. |

## The nineteen points

| # | Observed V1 behaviour | Diagnosed cause | Affected code | Fix | Regression test | Status |
|---|---|---|---|---|---|---|
| 4.1 | DELETE is a filled, rounded button at the top-right. | Deliberate V1 design. | `FormationView::paint/layoutLane` | Plain red `DELETE` text at the upper-left corner of the field, shown only while a committed item is dragged. Hover changes brightness only. Drop opens the `Delete <name>?` confirmation. | Covered by the editor UI script and code review. | Done |
| 4.2 | A staged FX draws a halo plus the caption `staged: X` in the field. | `FrameInput::hasStaged` drives a kind-2 particle halo, and the overlay paints a caption. | `Formation.cpp`, `FormationView.cpp` | `hasStaged` and `stagedLevel` removed from the formation input entirely. | `test_formation`: the staged state has zero influence on the field (identical vertices). | Done |
| 4.3 | FX names float on the cores; the commit toast puts a name over the field. | `labels_` from `cores()`, plus `showToast(name + " joined…")`. | `FormationView.cpp`, `PluginEditor.cpp` | Labels and name toasts removed. Names appear only in the chain strip in the reserved band at the bottom edge, which is the reorder/delete affordance. | Screenshot review. | Done |
| 4.4 | Every chain reads as one purple/pink cloud. | Hues sit in a narrow 215–325° arc at fixed saturation 0.48 with additive overlap. Per-channel exponential tone mapping plus a white blend washes everything to pink-white, and a constant floor lifts the black. | `Formation.cpp` (`hueForType`, `toneMap`) | New `ih1/visual/Palette.h`: 8 roles × tiers, a stable assignment per type, a duplicate shade rank from instance ids, and spatially separated regions. The tone map preserves hue. | `test_formation` (palette determinism, separation); `tools/palette_preview`. | Done |
| 4.5 | Carousel names are white or grey. | Not implemented in V1. | `Carousel.cpp` | Names drawn in their palette colour, with a stronger value for the selected item. | Visual check of all 38. | Done |
| 4.6 | Dropdowns close without applying the clicked item. | Not proven in FL Studio (no host here). Two concrete defects were found in code. **(a)** JUCE desktop `PopupMenu` is dismissed by its 10 ms focus poll when no JUCE peer has focus, which happens in host-owned or bridged plug-in windows. Activation is polled from real-time mouse state 250 ms after creation (`juce_PopupMenu.cpp` `checkButtonState`), so menus can vanish on click. **(b)** A choice edit writes the Staged Control macro, and a stale host echo of that macro is then pulled back at 30 Hz and overwrites the draft, which reverts the visible selection. | `SettingsPanel`, `PluginEditor::showPresetMenu`, `Processor::pullHostParams` | A single IH1-owned in-editor menu (`Ih1Menu`) that is a child component, not a desktop window, used for every dropdown and list. It opens once on mouse-down, applies a stable id once, and cancels on Escape or an outside click without selecting. Host macro echoes are ignored for 400 ms after a UI edit of that macro. | `test_menu` (synthetic mouse and key events through `Ih1Menu`). | Done in code; host-unverified |
| 4.7 | The field is `#0a080d`, not black. | Tone-map floor offset; `Colours::background` is cleared under the GL. | `Formation.cpp toneMap`, kCompFS | Floor removed, field cleared to `#000000`, bloom thresholded, no gradients. | `test_formation`: pixel max outside strands = 0. | Done |
| 4.8 | FL Studio crashes on fast resize. | Not reproduced (no FL Studio or Windows here). Code risks found: the full-resolution RGBA16F accumulation FBO and two bloom FBOs are deleted and reallocated on **every** intermediate size (up to 2560×1600×scale, ≈260 MB of float16 at 2× scale). FBO completeness is never checked. There is no size cap. `setDefaultLookAndFeel(&lnf_)` globally points at an editor member, which is unsafe with multiple instances or open popups. | `FormationView.cpp` renderer, `PluginEditor.cpp` | Coalesced reallocation (only after the size has been stable for 150 ms; until then the existing targets are stretched). Render size capped to ≤ 2.4 MP. Completeness checked, with fallback to the software formation. Zero and NaN guards. Global LnF removed. Resize stress harness added to `ih1_hosttest`. | `ih1_hosttest` editor stress: 300 resizes + reopen. | Mitigated; host-unverified |
| 4.9 | The window opens at 1280×780. | Hard-coded. | `PluginEditor.cpp` | Opens at 1500×900, clamped to 92 % of the display's user area. Limits are 1120×700 to 3840×2400, and the last size is remembered per instance. | `ih1_hosttest` checks the default size. | Done |
| 4.10 | Motion is too kinetic and creature-like. | 2-D curl advection at high speed, a global pull toward the centre, and short particle lives that reseed every few seconds. | `Formation.cpp` | Rewritten as a true 3-D streamline field: stable seeds, a slowly evolving divergence-free 3-D field, a perspective camera with slow sub-degree parallax, depth fog, tapered connected strands, and a wisp layer through bloom. | `test_formation`: continuity bound on per-frame vertex motion; reference comparison images. | Done |
| 4.11 | `Choose an effect` with an empty bordered panel. | V1 design. | `Carousel.cpp`, `SettingsPanel.cpp`, `PluginEditor.cpp` | The carousel shows `None` and the settings panel is hidden when nothing is selected. | Editor UI script and screenshot. | Done |
| 4.12 | Focus box around the carousel, a border around the settings, many dividers. | `Carousel::paint` focus rect, `SettingsPanel::paint` border, editor divider, lane background. | Same files | Removed. A keyboard focus tick is shown only after keyboard focus. | Screenshot review. | Done |
| 4.13 | Right-click does nothing. | Not implemented. | `Carousel.cpp` | Right-click inside the carousel opens `Ih1Menu` at the cursor with the full catalogue grouped by family and coloured. It stages the choice and never commits. | `test_menu` (placement and selection). | Done |
| 4.14 | RESET keeps the recorded loop. | `Session::resetDraft` only resets values; the staged processor and its buffers survive. | `Session.cpp` | RESET also swaps in a freshly prepared staged processor, built off the audio thread and crossfaded in through the normal program handover. Buffers, position and state are cleared for every FX, including the Looper. Committed instances are untouched. | `test_engine`: record loop → RESET → silence; committed Looper unaffected. | Done |
| 4.15 | Single `Presets` button with save/load files. | V1 design. | `PluginEditor.cpp` | Preset box: `‹ name ›`. Clicking the name opens the browser (categories, list, search). Prev/next walk the global order and wrap. Loading is atomic. A modified marker shows when the chain differs. | `test_presets`. | Done |
| 4.16 | No factory presets. | Not implemented. | New `core/src/presets/` | 28 factory presets in 7 categories as structured data. Every one is validated against the registry and loaded in tests. | `test_presets`: every preset loads, ids and params are valid, audio is finite. | Done |
| 4.17 | No commit feedback except a toast. | Not implemented. | Renderer and formation | Transfer packet: the FX colour gathers at the left edge of the field, travels along a curve and disperses into the new region (0.65 s). It is UI-only; audio commits immediately. | `test_formation`: the transfer never changes chain state; rapid commits stay correct. | Done |
| 4.18 | Rounded, default-looking widgets. | LnF radii of 4–16 px, filled pills. | `Ih1LookAndFeel.*` and all components | Token set: radius 0–2 px, hairline 1 px, restrained accent, no pills. | Screenshot review. | Done |
| 4.19 | Cramped generic knob grid. | Generic 96×108 cells for every FX. | `SettingsPanel.*` | Hierarchical layout: primary knobs large, secondary row smaller, advanced controls behind `More`, and segmented selectors for short choices. Graphs computed from real parameters (delay taps, reverb decay, filter and EQ response, dynamics transfer, shaper curve, LFO, harmony voices) or real DSP state (Looper buffer and position). | Screenshot review of all 38 panels. | Done |

The *Status* column was updated at the end of the pass. Evidence is in `docs/QA_AND_REGRESSION_RESULTS.md`.

## Additional defects found in the audit

| Severity | Defect | Fix |
|---|---|---|
| High | `juce::LookAndFeel::setDefaultLookAndFeel(&lnf_)` makes a global point at a per-editor object. With two editors open, closing one resets the global under the other, and async popups and alerts can reference a destroyed LnF. | Removed. LnF is set per component tree. |
| High | Staged Control macro echo can overwrite UI edits (see 4.6). | 400 ms UI-ownership window per macro. |
| Medium | GL renderer reallocates three float FBOs on every frame of a live resize, and never checks completeness. | Coalesced, capped and checked (4.8). |
| Medium | The software fallback renders at half size on the message thread at 30 Hz, using `setPixelColour` per pixel. | Uses a direct pixel pointer, with the size capped. |
| Low | The formation timer repaints the overlay at 30 Hz even with no change and no drag. | Repaints only while animating or dragging. |
| Low | The chain lane `insertionFor` / `uiMove` use pill indices captured at mouse-down. A preset load during a drag could move the wrong item. | Drag is resolved by instance id at drop and cancelled if the id has vanished. |
| Low | Tooltip for committed items listed internal hints. | Kept (read-only details), with no name over the artwork. |

## Order of work

1. P0: GL resize and lifecycle hardening, global LnF removal, resize stress harness.
2. P1: menu system (all dropdowns, right-click list) and macro echo fix; RESET swap.
3. P1: palette, formation rewrite, black field, staged and label removal, commit transfer.
4. P2: carousel and settings redesign, DELETE text, design tokens, window size.
5. P1/P2: preset model, browser and box; factory library.
6. QA, screenshots and docs.
