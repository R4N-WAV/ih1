# IH1 V1 feedback: what changed and how it works

This document covers the repair and refinement pass on V1, which starts from commit `0079915`.
- Item-by-item status: `docs/V1_FEEDBACK_TRIAGE.md`.
- Evidence: `docs/QA_AND_REGRESSION_RESULTS.md`.
- Anything still open: `docs/KNOWN_ISSUES.md`.

## Architecture after the pass

The split is unchanged. The engine, model, FX, formation simulation, palette and presets are plain C++20 in `core/`. `plugin/` is a JUCE 9 wrapper.

New modules:

| Module | Purpose |
|---|---|
| `core/include/ih1/visual/Palette.h` | One FX colour-identity system, shared by carousel, menus, settings accents and formation. |
| `core/include/ih1/visual/Raster.h` | CPU rasteriser with the same look as the GL path. Used by the software fallback, the preview tool and tests. |
| `core/include/ih1/presets/Presets.h` | Factory preset library as structured data, plus validation. |
| `plugin/Source/Widgets.*` | `MenuOverlay` (the single popup surface), `Dropdown`, `Segmented`. |
| `plugin/Source/FxGraph.*` | Effect-specific graphs drawn from real parameter or DSP data. |
| `plugin/Source/PresetBrowser.*` | Preset box (`< name >`) and the in-editor browser. |
| `tools/uitest/UiTest.cpp` | `ih1_uitest`, which drives the real editor through its native peer. |
| `tools/fx_dump.cpp` | Prints the FX parameter schema. |

## P0: resize crash (4.8)

The crash could not be reproduced here: there is no FL Studio or Windows in this environment. The fixes target every resize and lifecycle risk found in the code:

- **Render targets are no longer reallocated on every intermediate size.**
  - V1 freed and reallocated three float16 FBOs (full-size accumulation plus two bloom buffers) on every frame of a live resize, up to about 260 MB at 2× scale.
  - Now the renderer tracks the requested size and reallocates only after it has been stable for 150 ms. Until then it keeps drawing into the existing targets and stretches them in the composite pass. `FormationRenderer::renderOpenGL` in `plugin/Source/FormationView.cpp`.
- **Bounded memory.** Internal resolution is capped at 2.4 megapixels and 4096 px per side, whatever size the host requests.
- **Validated GL.** Every new framebuffer is checked with `glCheckFramebufferStatus`, and `GL_OUT_OF_MEMORY` is checked after allocation and after each frame. Any failure frees the targets, flags the renderer, and the editor's timer detaches the context. It then switches to the CPU formation (`FormationView::setSoftwareFallback`). Audio is never involved.
- **Guards.** Zero, negative, non-finite and absurd sizes, and invalid rendering scales, never reach GL. Aspect ratio and projection maths are clamped in `Formation::update`.
- **Ownership.** All GL objects are created and destroyed only inside the `OpenGLRenderer` callbacks, where JUCE has the context current. The message thread only publishes plain values under a SpinLock: area, chain, commit events. The editor detaches the context in its destructor before any child component is destroyed, so `openGLContextClosing` frees everything with the context current.
- **No global LookAndFeel.** V1 called `LookAndFeel::setDefaultLookAndFeel(&lnf_)` on a per-editor member. With two IH1 editors open, closing one changed the global under the other. Async popups and alerts could also reference a destroyed LnF. Removed; the LnF is set on the editor's own component tree.
- **Fewer desktop windows.** All menus, the preset browser and dialogs are child components of the editor (see 4.6). No IH1 popup is a separate native window that a host could reparent, focus or destroy during a resize.
- **Adaptive workload.** If the formation simulation takes more than 12 ms per frame for 30 frames, it drops to the Low quality tier.
- **Regression harness.** `ih1_hosttest` section 9 opens the VST3's editor in a host window and resizes it through the real `IPlugView::onSize` path more than 300 times, including:
  - random and extreme sizes (400×300 to 4000×3000), near the minimum, very wide, and very tall;
  - often with no frame rendered in between;
  - while an audio thread processes continuously;
  - with the chain swapped mid-resize;
  - closing and reopening the editor immediately after a resize.

  It then checks that the editor settles at a requested size, that audio stayed finite, and that eight more quick open/close cycles succeed.

## P1: dropdowns that do not select (4.6)

**Cause.** Not reproduced in FL Studio here. Two concrete defects were found in code:

1. JUCE's desktop `PopupMenu` (used by V1's `ComboBox` and the Presets menu) closes itself from a 10 ms timer whenever no JUCE window has focus (`doesAnyJuceCompHaveFocus`). It also triggers items by polling real-time mouse state 250 ms after opening (`juce_PopupMenu.cpp`, `checkButtonState`). Inside a host-owned or bridged plug-in window, focus belongs to the host, so a menu can close on the click without applying it.
2. A choice edit also writes the matching Staged Control macro. The processor polled the macros at 30 Hz, so a host echo or stale replay of that parameter overwrote the draft and reverted the visible choice.

**Fix.**
- `MenuOverlay` is now the only popup in IH1. It is a child component covering the editor and owns the whole interaction:
  - it opens once per gesture;
  - it applies an item only when press and release land on the same enabled row, or on a press-drag-release from the control, or on Enter;
  - an outside click, Escape, an editor resize or a destroyed owner cancel it;
  - every mouse and wheel event is consumed while it is open, so nothing clicks through;
  - the result callback runs exactly once, after the overlay has closed, through a `SafePointer`;
  - keyboard: Up/Down, PgUp/PgDn, Home/End, Enter, Escape, and type-to-jump;
  - it places itself below or above its anchor, or beside the pointer, flipping at the edges.
- Every choice parameter uses `Dropdown` or `Segmented` (for 2–4 short options) on top of it.
- **Macro echo.** `Processor::uiSetParam` records the time of each UI edit. `pullHostParams` absorbs host values for that macro for 400 ms instead of applying them.

## P1: visual fidelity

### Formation rebuilt as a 3-D filament field (4.10)

`core/src/visual/Formation.cpp` was rewritten.

- **Strands.** Each committed instance owns up to 160 seeded strands of four kinds:
  - hair: long and fine;
  - wisp: curlier;
  - spark: gathered at the shared core, which forms the pale intersection;
  - veil: six near-parallel copies of one traced curve, which read as a translucent smoky band.

  Strand identities, offsets and lengths come from the instance's stable visual seed. Nothing is reseeded per frame.
- **Tracing.** Each strand is a true 3-D streamline, traced both ways from its seed with midpoint (RK2) steps through a divergence-free flow field. The field is a sum of nine plane-wave curl terms with wavelengths of 1.3–3.5 units and phase drift of 0.05–0.12 rad/s. Two sign errors found during tuning were fixed:
  - the backward half now follows `-(guide + field)`;
  - a continuous forward clamp stops tails flipping where the field opposes the guide.
- **Skeleton.** It follows the reference: a vertical plume crossing a horizontal arm that recedes toward the camera. Signal order runs from the first FX at the arm's tip, through the core, to the last FX in the plume. Sheets are kept thin by a pull along their normal, so the 3-D depth reads.
- **Camera.** Perspective with depth fog: far strands dim and cool toward indigo. Parallax is slow, under 4°, over 75–92 s. The composition sits right of centre with black space on the left.
- **Motion.** Measured per frame at 60 fps, it stays under 0.4 % of the field height (`test_formation`). Reorders glide anchors with a 1.1 s time constant and never clear the field.
- **Audio** comes only from committed-slot telemetry. Per-instance level raises that instance's light; overall energy raises strand tension by up to 12 %.
- **Size.** The formation grows with chain length.
- **Cost.** About 3–5 ms per frame for six FX at Medium quality on this container's CPU. There are three quality tiers.

### Black void (4.7)

- The tone map is hue-preserving: it compresses the brightest channel and scales the others by the same factor. Only very dense overlaps ease toward lavender-white.
- The `#0a080d` floor offset is gone, and the field is cleared to `#000000`.
- Bloom adds haze only around actual strands.
- There are no stars or gradients.
- `test_formation` asserts an empty chain composites to exact zeros and that the top-left corner of a five-FX field stays at zero.

### Colour identities (4.4, 4.5)

`Palette.h` defines eight roles tuned to the reference: Indigo, Periwinkle, Violet, Lavender, Orchid, Rose, Ice and Pearl.

- Every FX type has a fixed role and tier. Families are spread across roles, so common pairings differ. Tests check ΔE > 12 for delay/reverb, reverb/shimmer, harmony/doubler, chorus/reverb, saturation/compressor and granular/reverb.
- Duplicates keep their hue but alternate deeper/lighter shades. The shade comes from the rank of the instance's id among its type, so reordering never recolours anything.
- The carousel, the right-click list, settings accents, the chain strip, the delete confirmation and the formation all use the same function.
- There are no greens, yellows, oranges or reds. `danger` red is used only for DELETE.

### Staged FX and labels removed from the field (4.2, 4.3)

- `FrameInput` no longer has any staged, SOLO or output-level fields, so the staged FX cannot reach the formation.
- Core labels, the `staged:` caption and the "<name> joined the formation" toast are gone.
- Names appear only in the chain strip. The strip sits in a reserved 46 px band below the artwork; the GL viewport excludes it. It is the reorder/delete affordance.

### Commit transfer (4.17)

- `Editor::doCommit` calls `uiCommit`. The audio chain changes immediately, through the existing commit hand-over. `uiCommit` returns the new instance id, and the formation then gets a transfer request.
- The packet:
  - is a twisting bundle of the FX's colour that tightens in flight;
  - travels 0.65 s along an arc from the left edge, level with the carousel, to the new region;
  - disperses as a short burst, after which the region's strands grow out over 1.4 s.
- Rapid commits each get their own packet.
- If the editor is closed during a commit, nothing is replayed: requests live only in the open renderer.

## P1/P2: workflow and UI

- **Startup (4.11).** The carousel reads `None`. The settings panel is hidden whenever nothing is staged, and again after COMMIT.
- **Right-click list (4.13).** It is available only inside the carousel and opens beside the pointer. It holds the full catalogue grouped by family, with coloured names, keyboard navigation and wheel scrolling. Choosing an entry stages it; it never commits.
- **DELETE (4.1).**
  - While a committed item is dragged, a plain red `DELETE` appears at the upper-left of the field, brighter and more widely spaced when hovered. It has no box.
  - Dropping on it opens `Delete <name>?` (the name in its colour) with Cancel and Delete.
  - Escape, Cancel, an outside click, or dropping anywhere else deletes nothing.
  - Deletion is by instance id, so duplicates are never confused.
  - A drag whose instance disappears, for example during a preset load, is cancelled.
- **Borders (4.12, 4.18).**
  - Removed: the carousel focus box, the settings border, the column divider, the lane background and the pills.
  - The design tokens are in `Ih1LookAndFeel.h`: radius 1.5 px, 1 px hairlines used sparingly, and accent colour only for interaction.
  - Keyboard focus shows a small accent tick or underline, only when focus arrived by keyboard.
  - COMMIT and RESET are precise outlined blocks; S, M and footer actions are text buttons.
- **Settings panel (4.19).**
  - Order: description, then S/M, then an optional graph, then selectors (two per row), then primary knobs, then advanced controls behind `More`.
  - Primary knobs size themselves to the space, from 52 to 80 px.
  - Values can be typed: double-click the value.
  - Double-click a knob to reset it.
  - Tooltips explain the audible effect.
  - Graphs, all from real data:

    | Effect | Graph | Data source |
    |---|---|---|
    | Delay / Tape Echo | Tap pattern and decay; ping-pong shown as L/R | Host tempo for synced times |
    | Reverb / Shimmer / Convolution / Resonator | RT60 decay with pre-delay, early reflections and damping slope | Parameters |
    | Filter / EQ | Magnitude response | Impulse response of the same `dsp::Svf` sections the DSP uses |
    | Compressor / Gate | Static transfer curve | Same knee formula as the DSP |
    | Saturation / Tape-Console / Amp / Bitcrusher / Exciter | Input/output curve | Measured by running a sine through a private instance of the real processor |
    | Modulation | LFO shape and stereo offset | Parameters |
    | Harmony / Pitch Shift | Voice interval, level and pan | Parameters |
    | Granular | Read window and grain sample | Parameters |
    | Looper | Live buffer peaks, record position, playhead and state | `FxDisplay`, lock-free atomics written by the audio thread |
- **Window (4.9).**
  - First run is 1500×900, or 92 % of the display's usable area if that is smaller.
  - Limits are 1120×700 to 3840×2400.
  - The last size is stored in the project state.
- **Wheel (5.1).** Accumulated: high-resolution wheels and trackpads give one step per notch, at most every 70 ms. It only affects the carousel and is inert while a list is open.

## RESET clears the staged Looper (4.14)

`Session::resetDraft` restores defaults and also swaps in a freshly prepared staged processor of the same type. That processor is built on the message thread, crossfaded in by the engine's existing staged transition, and the old one is retired and freed on the message thread.

This clears the Looper's buffer, position, state and display, and also any delay or reverb memory. It is click-free, never allocates or frees on the audio thread, and never touches committed instances.

## Presets (4.15, 4.16)

**Library.** `core/src/presets/Presets.cpp` holds 28 factory presets in seven categories, listed in `docs/FACTORY_PRESETS.md`. Each preset is structured data: FX ids plus parameter values by stable id.
- `validatePreset` checks every id and range.
- `presetToJson` produces normal chain-only state with deterministic instance ids and seeds, so a preset always draws the same formation.

**Loading.** Through `Session::fromJson`, which validates everything first and then swaps the whole chain in one program hand-over. On failure nothing changes and the preset name is not updated.

**Preset box.** `< name >` in the header.
- The arrows walk the global order: factory categories in order, then user presets. They wrap at both ends. From "No preset", next goes to the first and previous to the last.
- A small rose square marks a chain modified since loading.
- The current preset reference is saved in the DAW project.

**Browser.**
- Layout: a category column, the list of presets, a search field across names, categories and descriptions, and a one-line description of the hovered preset.
- A preset loads on click; the browser stays open for auditioning. Double-click loads and closes.
- User presets:
  - stored as `Documents/IH1/Presets/*.ih1chain`;
  - written through a temporary file;
  - Save asks before overwriting, and Save As asks if the name exists;
  - Rename and Delete (with confirmation) are available;
  - factory presets are read-only.
- "Clear chain", with confirmation, replaces V1's Presets-menu item.

## Bugs found by the audit and fixed

| Area | Defect | Fix |
|---|---|---|
| DSP | **Vocoder output level scaled with ~bands².** The SVF band-pass peaks at Q in both stages. At 16, 24 and 32 bands, a -10 dBFS input hit the +12 dBFS safety limiter. | Unity-peak bands and √N scaling. The output is now within ±2.2 dB of the input for every band count. |
| Plug-in | Global default LookAndFeel pointing at a per-editor object. | See P0. |
| Plug-in | Staged Control macro echo reverting UI edits. | See 4.6. |
| UI | Reorder and delete resolved a drag by an index captured at mouse-down. | Resolved by id at drop; cancelled if the id vanished. |
| UI | Software fallback wrote pixels one by one with `setPixelColour` and ignored the new look. | Shared `Canvas` and direct line writes. |

## Files touched

- **core:**
  - `visual/Formation.*` (rewritten)
  - `visual/Palette.*` (new)
  - `visual/Raster.*` (new)
  - `presets/Presets.*` (new)
  - `model/Session.*` (RESET swap, `stagedProcessor`)
  - `fx/FxProcessor.h` (`FxDisplay`)
  - `fx/BatchC.cpp` (Looper display)
  - `fx/VoiceB.cpp` (Vocoder level)
- **plugin:**
  - every source file
  - `Widgets.*`, `FxGraph.*` and `PresetBrowser.*` (new)
  - `CMakeLists.txt` (new sources, `ih1_uitest`)
- **tests:**
  - `test_formation.cpp` (rewritten)
  - `test_presets.cpp` (new)
  - `test_engine.cpp` (Looper RESET)
- **tools:**
  - `formation_preview.cpp` (chain specs, transfer, timing)
  - `fx_dump.cpp`, `uitest/UiTest.cpp` and `hosttest` section 9 (new)
