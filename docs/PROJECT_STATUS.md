# IH1 — Project Status

**Updated:** 2026-11-18
**Phase:** 0–5 implemented; Phase 6 (QA/packaging) in progress. Not yet tested in a real DAW.

## Done
- **Phases 0–1:** audits, KADAL research map, visual analysis, decisions (all resolved; `docs/OPEN_DECISIONS.md`).
- **Core (`core/`, JUCE-free C++20):**
  - FX registry with stable IDs and per-type drafts
  - immutable committed chain with stable unique IDs
  - lock-free program mailbox and retire queue
  - seamless COMMIT hand-over
  - SOLO/MUTE router with latency-compensated dry and solo paths
  - versioned JSON state: preset vs DAW session
- **38 effects implemented** (the full catalogue): `docs/FX_CATALOGUE.md`.
- **Plug-in (`plugin/`, JUCE 9.0.5):**
  - VST3 and standalone builds
  - carousel, staged settings with tooltips, S/M, COMMIT/RESET
  - OpenGL formation
  - chain lane with drag reorder, DELETE target and confirmation
  - chain presets and DAW state
- **Formation:** a render-independent simulation shared by the plug-in and the offline preview tool. It is measured against the reference video (`docs/img/formation_compare_v1.jpg`).
- **Installer:** `installer/IH1.iss` (Inno Setup), built by CI as `IH1-Setup-<ver>.exe`.

## Verification (actual results)
| Check | Where | Result |
|---|---|---|
| Unit, contract and behaviour tests (68 cases, ~2.2 M assertions) | Linux (this VM) | pass |
| Threaded stress under ThreadSanitizer; core under ASan/UBSan | Linux | pass |
| `ih1_hosttest`: VST3 loaded via JUCE hosting; 44.1–192 kHz, odd blocks, automation storm, bypass latency, state null test, hostile state, lifecycle | Linux | pass |
| Standalone app under Xvfb with software OpenGL (screenshot `docs/img/ui_standalone_linux.jpg`) | Linux | runs |
| MSVC 2022 x64 build, unit tests, `ih1_hosttest` | GitHub Actions run #8 (`e5305c6`) | pass |
| pluginval v1.0.4, strictness 5, including the Editor tests | Windows CI (GPU-less runner; software-rendered formation fallback) and Linux CI, run #8 | pass: `SUCCESS`, exit code 0 on both. The separate Steinberg vst3 validator step was skipped because its path isn't configured |
| Setup wizard `IH1-Setup-0.1.0.exe` compiled (Inno Setup 6.7.1) | Windows CI, run #8 | pass: artifact `IH1-windows-setup`. Not yet installed on a real machine |
| Editor GL capability check and software fallback | Linux under Xvfb with GL 4.x, forced software rendering, and Mesa capped at GL 2.1 | pass |
| FL Studio 25 / Cubase 14 | — | **not tested** (no access from this environment) |

## Known issues / open items
- **No real-DAW testing.** Owner listening and DAW testing are pending (FL Studio 25, Cubase 14).
- **Inno Setup licence.** Inno Setup 6.7 reports "Non-commercial use only". Before a commercial release, buy an Inno Setup commercial licence or switch the wizard to NSIS (zlib) or WiX.
- **JUCE licence.** JUCE is on the Starter tier; move to Indie before revenue passes USD 20k.
- **Visual gaps.** The formation v1 still lacks the reference's sharpest "electric" hair-lines and edge iridescence.
- **CPU spikes in long spectral chains.** Spectral FX compute frames on the same block. Stagger hops per instance.
- **No macOS build yet.**

## Next steps
1. Owner installs `IH1-Setup` from the Actions artifacts and tries IH1 in FL Studio 25 and Cubase 14. Feedback goes into a test log.
2. Formation v2: sharper filament layer and edge dispersion; profile on Windows GPUs.
3. Stagger STFT hops across instances; reduce Harmony CPU.
