# V6 P0: FL Studio access violation in IH1.vst3 — diagnosis and fix

**Status: root cause identified with strong evidence and fixed. Not yet re-tested in FL Studio** (no Windows / FL Studio
access from the build environment). The owner should run the checklist at the end.

## 1. The report

FL Studio 25.2.3.5171 (64-bit), Windows 10 22H2 (19045.7846):

    Access violation at address 00007FFF73FD7F8D in module 'IH1.vst3' (offset 27F8D). Read of address 0000000068746170

Stack: nine IH1 frames under `USER32.dll` (a window message, i.e. the UI / message thread, not the audio thread).
Undo list: `staged control 3..8`, then `preset macro 1..3`, then `Knob tweaks`.

## 2. Which binary

The last successful Windows CI build of the V5 branch is run #40 (`52a8bb5`). Run #41 (`d9b9ebf`, docs only) failed
on Windows and produced no Windows artefact, so `52a8bb5` is the code the owner installed. The artefact
`IH1-windows-vst3` (id 14770884622, sha256 of the zip
`088527e7cc7b65073d0d560d1151e19dadc0d2b68b5389a987b7eb29a0924ab2`, matching GitHub's recorded digest) was
downloaded and disassembled. No PDB is produced by the release build, so functions were identified from the PE
`.pdata` unwind table, data layout and string references.

## 3. Resolving the frames (RVA = offset in the report)

| Frame | RVA | Function (from `.pdata`) |
|---|---|---|
| fault | `0x27F8D` | `0x27F70..0x27F96`: a string-from-`(const char*, length)` constructor; the faulting instruction is its first read, `cmp byte ptr [rdx], 0` |
| 1 | `0x604C34` | `0x604830..0x6051D5`: **`Carousel::showCatalogueMenu`** (the FX right-click list) |
| 2..8 | `0x407AAC` ... `0xEBAA1` | JUCE mouse dispatch (`mouseDown` from the window procedure) |

Why `0x604830` is `showCatalogueMenu`: it first walks the parent chain with `__RTDynamicCast` (= `MenuOverlay::find`),
tests the overlay's `open_` flag, builds a row from the literal `"None"`, then loops over a `std::vector<int>` member
of `this`, computes `registry_data + idx * 0x118` (`0x118` = `sizeof(FxDescriptor)` in MSVC), compares the byte at
`+0xCA` (`category`) with the loop's category counter, and on a match builds a string from the MSVC `std::string` at
`+0x20` (`FxDescriptor::name`: pointer-or-inline buffer at `+0x20`, size at `+0x30`, capacity at `+0x38`). This is
exactly the source loop:

```cpp
for (int idx : order_) {
    const auto& d = reg[static_cast<size_t>(idx)];
    if (static_cast<int>(d.category) != cat) continue;
    ...
    it.text = d.name;
```

## 4. Root cause

Since V4 Phase 3, `Carousel::order_` begins with the None entry, `kNoneEntry = -1`, so None is part of the wheel's
ring. The right-click list adds its own "None" row and then iterates `order_` **without skipping -1**.
`reg[-1]` addresses the 280 bytes *before* the registry's heap block. Whatever happens to lie there decides the outcome:

- usually the byte read as `category` matches no category, so the entry is skipped and nothing visible happens
  (this is why every earlier test passed);
- if it matches, the bytes read as a `std::string` are used: with a "long string" capacity the 8 bytes at `+0x20`
  are dereferenced as a text pointer. In the owner's FL Studio process they were `70 61 74 68 00 00 00 00`
  (`"path"`, the start of some other inline string); reading address `0x68746170` faulted.

The fault therefore depends on heap layout (host, preset history, what was allocated before the registry), which
matches "it crashed once in FL Studio, never in CI".

The Undo list is consistent with this. A preset recall re-syncs the eight staged-control host parameters and returns
the preset macros to 0 (`syncMacrosFromDraft()` then the macro reset in `applyRecallHostState()`). FL Studio
records those host notifications as knob tweaks. The crash itself happened on the next right-click on the FX
carousel. The macro and staged-control code was audited and is not involved.

## 5. Proof on Linux (AddressSanitizer)

`ih1_uitest` (drives the real editor) built with `-fsanitize=address` from the **unmodified V5 source**:

    ERROR: AddressSanitizer: heap-buffer-overflow ... READ of size 1
        #0 ih1::plugin::Carousel::showCatalogueMenu(juce::Point<int>) plugin/Source/Carousel.cpp:186
        #1 ih1::plugin::Carousel::mouseDown(juce::MouseEvent const&) plugin/Source/Carousel.cpp:212
        ...
        #11 main tools/uitest/UiTest.cpp:124        (the first right-click on the carousel)
    0x52c0000001b2 is located 78 bytes before 31744-byte region ... allocated by std::allocator<ih1::FxDescriptor>

With the fix the full UI suite runs under ASan with **0 reports**.

## 6. Fix

- `plugin/Source/Carousel.cpp`: the menu loop accepts only valid registry indices. None stays the dedicated first
  row, and selection behaviour is unchanged.
- `plugin/Source/PluginProcessor.cpp` (`uiSetParam`): a second latent out-of-bounds read found in the audit. An index
  below 8 but at or above the staged effect's control count read past the effect's `ParamSpec` vector. The editor
  never sends one, but the index is now validated first.

Nothing was disabled. Macros, staged controls, host automation and undo behave exactly as before.

## 7. Regression guards

- `ih1_uitest`, new V6 block: 25 rounds of preset recall (with macros), preset-macro gestures across the full range,
  staged controls 3-8 (including indices past the effect's control count), selection switching between Reverb and
  Doppler, then the real right-click list, checking its exact contents every time.
- CI job `linux-asan`: builds `ih1_uitest` with AddressSanitizer and fails on any report (`halt_on_error=1`).

## 8. Other paths audited (no defect found)

| Area | Finding |
|---|---|
| Preset macros | APVTS slider attachments, so no raw pointers. The engine resolves targets to (slot, parameter) when it builds the program, with index bounds checks; reading the registry on the audio thread is lock-free, and the registry never changes after start-up |
| Staged controls 1-8 | host values are applied for `k < params.size()` only; `Session::setDraftParam` is bounds-checked |
| Menu / dialog callbacks | every `MenuOverlay::open` callback captures a `SafePointer`; confirm dialogs close through `callAsync` with a `SafePointer` |
| Registry indexing elsewhere | every other `fxRegistry()[i]` with a possibly negative `i` is guarded (`fxColourForIndex`, carousel paint, type-to-search, `Engine` staged discrete check) |
| Processor lifetime | retired programs and processors are freed on the message thread (`Session::service`), never on the audio thread |

## 9. If IH1 ever crashes again: capturing a dump (owner, Windows, reversible)

1. Run `regedit` as administrator. Create the key
   `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\FL64.exe`.
2. In it, add the `DumpFolder` (REG_EXPAND_SZ) value `%LOCALAPPDATA%\CrashDumps`, `DumpType` (DWORD) = `2` (full dump)
   and `DumpCount` (DWORD) = `3`.
3. Reproduce. The `.dmp` file appears in `%LOCALAPPDATA%\CrashDumps`. It contains process memory, including your
   project, so share it privately.
4. To undo, delete the `FL64.exe` key.

Also send the crash log from `Documents\Image-Line\FL Studio\Support\Logs\Crash` and the IH1 build id shown in the
plug-in. CI uploads symbol files (`IH1-windows-pdb`, private artefact), so an offset can be resolved to a source line.
