# IH1 V6 (feedback round 6): what changed, why, and the evidence

Version **0.6.0** (test build, round 6), following the established convention (round N = 0.N.0, installer
`TestRound=N`). This pass implements `IH1_V6_FEEDBACK_MASTER_PROMPT.md` with `IH1_V6_RESEARCH_SUPPLEMENT.md`.

Related documents:
- crash diagnosis: `docs/V6_CRASH_ANALYSIS.md`
- preset audit: `docs/V6_PRESET_AUDIT.md`
- generated preset index: `docs/FACTORY_PRESETS.md`
- open items: `docs/KNOWN_ISSUES.md` (V6 section)
- owner checklist: `docs/FL_STUDIO_TEST_CHECKLIST.md` (V6 section)

Status words used below:
- **fixed (verified):** reproduced, fixed, re-tested.
- **fixed (not host-verified):** verified on Linux, not yet in FL Studio.
- **not reproduced**, **unable to verify:** as stated.

## 0. Starting point (Phase 0)

| Item | Finding |
|---|---|
| Branch | `claude/sweet-albattani-g4tmmv` was at `0079915` (V1). The V5 work (0.5.0) was on `claude/bold-mccarthy-ydvunz` (`d9b9ebf`), a strict descendant, so the branch was fast-forwarded to it. Nothing was discarded. |
| Toolchain | CMake 3.28, GCC 13.3, Ninja, JUCE 9.0.5 (pinned), doctest 2.4.11, C++20. Windows release: MSVC 2022 x64 in CI. Installer: Inno Setup 6. |
| Product version | 0.5.0 in `CMakeLists.txt` / `installer/IH1.iss`. Now 0.6.0. |
| Baseline | Unit tests, VST3 host test and editor test passed on the V5 tip (Linux, RelWithDebInfo). |
| Installed build | The owner's crash came from CI run #40 (`52a8bb5`, the last Windows artefact of V5; run #41 was docs-only and failed on Windows). |
| Prior prompts | Only the V1 brief and master prompt are in `docs/source/`. Active requirements come from the V2.1-V5 implementation documents (ledger below). |
| KADAL | **Not searched.** Attaching `R4N-WAV/Kadal` was refused by the session's permission policy (as in V4 / V5). `docs/KADAL_RESEARCH_MAP.md` and the paths recorded in earlier rounds were reused. |
| FL Studio / Windows | Not available. See section 9. |

### Requirements ledger (inherited; status after V6)
| Requirement | Origin | V6 status | Guard |
|---|---|---|---|
| Committed chain = audio truth; staged plays after it; immutable snapshots | V1, V4 | kept | `test_engine`, `test_v5` |
| None is a first-class carousel entry | V3, V4 | kept. **The P0 crash was this entry leaking into the FX list loop**; None stays the list's first row | `ih1_uitest` (V6 block) |
| COMMIT keeps the browser position (stand-by copy) | V5 | kept | `test_v5`, `ih1_uitest` |
| Preset recall silences staged audition; Init; Clear FX Chain | V2.1-V3 | kept | `ih1_uitest`, `test_v3` |
| Truthful Full / Low / Zero latency | V2.1 | kept; Whisper reports 1024 / 512 samples and has no Zero variant (stated in the read-out) | `every FX: reported latency is exact`, `latency registry` |
| Output Safety (never relied on in audits) | V3 | kept | feedback / bank audits run with it off |
| Accepted palette (types appended) | V2.1-V5 | kept: the V6 types are appended after the hidden test effects | palette tests |
| FX tree look and behaviour | V4, V5 | untouched | `test_formation`, `test_v5_visual` |
| Click-free chopping, parameter glide | V4, V5 | kept; the new FX pass the click audit | `ih1_click_audit` |
| Combination-only macros | V5 | kept and extended to vocal chains | `V5 bank: every macro ... audible` |

## 1. P0: the FL Studio access violation

Full write-up: `docs/V6_CRASH_ANALYSIS.md`. **Status: fixed (not host-verified).**

- **Root cause:**
  - The FX right-click list (`Carousel::showCatalogueMenu`) iterated the carousel ring `order_`, whose first entry
    since V4 is None (`-1`), and read `fxRegistry()[-1]`, 280 bytes before the registry's heap block.
  - In the owner's FL Studio heap, those bytes formed the "pointer" `0x68746170`.
  - The macro / staged-control entries in FL's undo list are the parameter re-syncs of a preset recall that preceded
    the right-click. They were not the cause.
- **Proof:**
  1. The shipped binary (artefact digest verified) was disassembled. Offset `0x27F8D` is the first read of a
     string-from-pointer constructor called from `showCatalogueMenu`'s loop: `idx * 0x118` = `sizeof(FxDescriptor)`,
     `+0xCA` = category, `+0x20` = name.
  2. AddressSanitizer on the unmodified V5 source: `heap-buffer-overflow` at `Carousel.cpp:186`, 78 bytes before the
     `FxDescriptor` allocation, on the editor test's first right-click.
- **Fix:**
  - The loop accepts only valid registry indices.
  - Second latent out-of-bounds read found in the audit: `Processor::uiSetParam` read the spec of an index past the
    staged effect's control count. It now validates the index first.
- **Regression:**
  - `ih1_uitest` V6 block: 25 rounds of preset recall, macro gestures, staged controls 3-8 and the right-click list,
    checking the list's exact contents.
  - New CI job `linux-asan` runs the whole editor test under ASan (`halt_on_error`).
  - Windows Release builds now keep a private PDB artefact; the installer stage refuses debug files.
- **Diagnostics used:**
  - CI artefact + `.pdata` + objdump on Linux.
  - AddressSanitizer (GCC) on Linux, on the real editor.
  - Not run: WinDbg, Application Verifier and Page Heap (need Windows); there was no dump to open.
  - The owner gets reversible LocalDumps steps (crash analysis section 9).

## 2. Code audit (memory, lifetime, threads)
| Area | Result |
|---|---|
| Registry indexing with a possibly negative or foreign index | the menu loop was the only unguarded site (fixed); all others checked |
| Staged-control index vs control count | `uiSetParam` hardened; `Session::setDraftParam`, host pull and engine apply were already bounded |
| Menu / dialog callbacks | all `MenuOverlay::open` callbacks capture `SafePointer`; confirm dialogs close through `callAsync` + `SafePointer`; menus close on editor close (tested) |
| Preset macros | APVTS attachments; engine targets resolved to (slot, parameter) when the program is built, index-checked on the audio thread |
| Processor reclamation | programs and processors are retired to and freed on the message thread (`Session::service`) |
| ASan: whole editor test (commit, delete, drag, menus, dropdowns, presets, resize, close-with-menu-open, V6 block) | 0 reports after the fix |

## 3. Menus and text overlap (Phase 4)

Found with editor snapshots at 1120x700 (minimum) and 1600x1000, plus 2x snapshot scale. Before / after:
`docs/img/v6_presets_before.png`, `docs/img/v6_presets_after.png`, `docs/img/v6_fx_menu_before.png`,
`docs/img/v6_fx_menu_after.png`.

| Defect (found on screen) | Cause | Fix |
|---|---|---|
| Preset browser: the *User* category row drawn over the description | 13 categories x fixed 26 px rows in a column of about 314 px, with no clip | category rows fit the height (26 down to 20 px), then the column scrolls; every region is clipped to itself |
| Category names cut ("Spectral / Frequency Sp...") | fixed 158 px column | width measured from the real names plus counts (up to 40 % of the panel) |
| Description squashed | three lines in a 34 px area | two description lines with an ellipsis, plus a separate metadata line (52 px) |
| Search results: category text could run under long names | name width fixed at -90 px | category column measured; names truncate before it |
| FX list: the ticked (current) effect truncated ("Reflection Sculp...") | width measured with the regular font, drawn bold | widths measured with the bold font; hint and chevron columns reserved |
| Carousel centre title clipped mid-letter ("REFLECTION SCULPTO") | no fitting | scaled to fit (down to 60 %), then an ellipsis |

User-friendliness:
- the FX list has a footer with the hovered effect's description (category rows preview their effects);
- category rows carry their family colour swatch;
- the preset browser shows counts per category and is larger on large editors.

Keyboard support, edge placement and the single source of truth for row geometry (layout, painting and hit-testing
share the column constants) are kept from V2.1-V5 and still pass their tests. **Windows display scaling was not
tested on Windows** (`docs/KNOWN_ISSUES.md`).

## 4. Vocal FX (Phase 2)

### Inventory: vocal-capable effects that already existed (not duplicated)
| Effect | Algorithm | Latency (48 kHz, Full) | Voice role |
|---|---|---|---|
| Harmony | phase-vocoder peak shifting (Laroche-Dolson), YIN tracking, intervals / key & scale / MIDI, up to 4 voices, formant preservation | 2048 (STFT) | harmony, choir |
| Pitch Shift | the same engine, 2 voices, natural formants, feedback | 2048 | octaves, halos |
| Formant | true-envelope formant shift | 2048 | size / gender |
| Pitch Correction | tracked correction, speed / tolerance | 23 (tracking) | tuning, hard-tune robot |
| Vocoder | 8-32 band channel vocoder, internal carrier or MIDI, consonant pass | 0 | robot, talking synth |
| Doubler | 2 / 4 detuned, delayed, drifting copies | 0 | doubles |
| De-Esser | split-band / wideband sibilance control | 0 | polish |
| Others in vocal use | Granular, Grain Engine, Stutter, Reverse, Shimmer, Spectral Freeze, Reverse Bloom, Cloud Chamber ... | as reported | textures, spaces |

Gaps found: no way to impose vowels on a sound (talkbox), no breath / whisper resynthesis, and no adlib throw that
ducks and drops in pitch. A fixed-note robot already exists (Vocoder with a fixed note, Pitch Correction at speed 0),
so none was added.

### New (all appended to the registry; 67 effects in the carousel)
| Effect | Source assumptions | Algorithm (why) | Latency | CPU (48 k / 256) | Limits |
|---|---|---|---|---|---|
| **Vowel Morph** | any source; best on harmonically rich material | 5 parallel band-pass resonators morphing through published adult formant data (A-E-I-O-U, log-frequency / dB interpolation); motion by hand, LFO, loudness or a synced vowel sequence; formant shift, resonance, drive, stereo spread; adaptive level match (300 ms, capped) | **0** | 0.79 % | a resonator bank, not an LPC vocal-tract model: vowels are stylised; level match settles over ~300 ms |
| **Whisper** | monophonic or polyphonic voice | STFT phase randomisation (removes periodicity) + spectral-envelope smoothing (removes the harmonic comb); consonant / onset frames keep their phase; per-frame energy kept; time-domain level match to the latency-aligned input | **1024** Full, **512** Low; no Zero variant (analysis window) | 2.48 % | the output is noise: crest factor up to about 14 dB |
| **Vocal Throw** | lead / adlib vocals | ducked, band-limited echo; pitch per repeat by a two-tap Hann delay-line shifter in the loop (bounded: feedback <= 90 % x unity band-pass x unity shifter, soft ceiling); ping-pong; Last Word mode catches and throws only the end of each phrase; large time jumps crossfade two read heads | **0** (echoes are the effect) | 0.76 % | the in-loop shifter is grainy by design (not transparent) |

Evidence (`tests/test_v6.cpp` + registry-wide contracts):
- Vowel Morph: A vs I resonances (> 15 dB band ratio difference), the sequence steps on the tempo grid, level within
  3 dB of the input.
- Whisper: spectral flatness > 20x the input's (harmonic comb gone), level within 0.4 dB, vowel tilt kept; Whisper
  0 % = the input.
- Vocal Throw: nothing before the first repeat, an octave drop on the second repeat, decaying; ducking > 10 dB; Last
  Word throws nothing during the phrase and the end after it.
- All 10 registry-wide contract tests pass with the new types (5.56 M assertions): silence -> silence, bounded at
  extreme and random settings, exact latency, block-size independence, reset / re-prepare, honest latency modes,
  tempo-sync greying.
- `ih1_feedback_audit`: 0 growth / 0 unexplained hold for all three.
- `ih1_click_audit` (60 cases x 4 s, continuous controls moving): **0 / 0 / 0**. The audit found three real defects on
  the way, all fixed:
  - Vocal Throw: a band-pass normalisation stepped per glide sub-block;
  - Vocal Throw: delay-time jumps swept one read head through history at up to 8x speed (now a two-head crossfade,
    decided on the parameter's final value);
  - Vowel Morph: resonator gains stepped every 16 samples (now ramped).

## 5. Presets (Phase 3)
- **Audit of the whole bank** (`docs/V6_PRESET_AUDIT.md`), 245 presets: 12 Improve (level / peak / label), 1 Merge,
  0 Remove. All were acted on. Final: **245 Keep**. The method's two rejected likeness measures are documented there.
- **New Vocal / Voice bank**: 37 presets covering all twelve areas of prompt 5.2. Each states whether it keeps the
  words ("Clear") or deliberately destroys them. Metadata: id, description, intended sources, intensity, style, chain,
  macros, latency, loop risk and level, all listed in `docs/FACTORY_PRESETS.md` (now generated by
  `tools/ih1_preset_index.cpp`).
- Gate (`V6 vocal bank` test):
  - every preset is finite, below +6 dBFS (the factory criterion) and at a sane level on two vocal sources;
  - intensity labels are honest by measured added content;
  - every new processor is used.
- Measured on the vocal phrase: 13 transformative, 6 moderate, the rest additive (doubles, throws, polish). Levels
  -6 .. +2 dB.

## 6. Macro verification
- Vocal chains now get combination macros:
  - *Voice in Space*: Voice = the voice processor's wet + the space's wet; Mouth = formant / vowel / character + space
    size.
  - *Voice Throw*: Voice; Throw = Vocal Throw feedback + pitch per repeat.
  - Vocal Throw joins every echo combination (Atmosphere, Pulse into Echo, Dub Wear).
- 23 of 37 vocal presets show macros. The other 14 have no eligible pair, or every candidate control is already at
  its end (the V5 "no placebo knob" rule).
- Every macro of every preset (202 in total) audibly changes the sound on its own, and stays safe at 0 / 100 % / swept
  (existing V4 / V5 tests, now over the larger bank).
- Exact mappings: `docs/FACTORY_PRESETS.md` ("Macro targets" under each category).

## 7. Audio quality summary
| Check | Result |
|---|---|
| NaN / Inf, bounded output, silence -> silence (every FX, extreme and random settings) | pass |
| Reported latency == measured (every FX, every mode) | pass |
| Feedback (new FX, max and 6 random settings, impulse / noise / chord / drums, 20 s tails) | 0 growth |
| Clicks, new FX, moving controls | 0 / 180 cases |
| Factory audit (every preset on drums, pad and voice: level, clipping, mono-safety, determinism) | pass |
| Vocal bank gate | pass |

## 8. Build, tests, host
| Check | Where | Result |
|---|---|---|
| Unit / behaviour / audit tests | Linux | **183 / 183 cases, 30,195,820 assertions pass** (8 min 56 s) |
| VST3 host-lifecycle test (`ih1_hosttest`: rates, odd blocks, automation storm, latency, state null test, hostile state, lifecycle) | Linux | pass |
| Editor tests (`ih1_uitest`, incl. the V6 crash block) | Linux, release build | **6 / 6 idle runs pass.** The pre-existing intermittent checks were also fixed: the unmodified V5 test misses a fixed 40-60 ms window in 2 of 6 idle runs (measured). Those checks now poll for the expected state, and **3 / 3 runs under concurrent CPU load pass** |
| Editor tests under AddressSanitizer | Linux | **0 ASan reports** (whole suite, twice: after the P0 fix and on the final code; the ASan build's slowness trips the same timing window once) |
| pluginval v1.0.4 | CI (Linux strictness 8, Windows) | see the CI run for this push. Not run locally: running the downloaded validator binary was refused by the session's permission policy |
| Steinberg vst3 validator | - | not run (the CI step is still not configured with its path) |
| **FL Studio 25 on Windows** | - | **not performed** (no access). Owner checklist: `docs/FL_STUDIO_TEST_CHECKLIST.md`, V6 |

## 9. Release
- Version **0.6.0**, test round 6.
- The setup wizard `IH1-Setup-0.6.0.exe` and the test-build ZIP are built by the existing Windows CI job from this
  branch (artefacts `IH1-windows-setup`, `IH1-windows-testbuild-zip`).
- The release binary is the normal Release configuration (no sanitizer). Its PDB is a separate private artefact
  (`IH1-windows-pdb`), refused by the installer stage.
