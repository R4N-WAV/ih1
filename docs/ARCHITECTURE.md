# IH1 Architecture

```
ih1/
├── core/                 ih1_core: JUCE-free C++20 static library (all DSP, model, state, formation sim)
│   ├── include/ih1/      public headers
│   │   ├── dsp/          shared primitives (delay, filters, ADAA, STFT, LFO, envelope, oversampler, RNG…)
│   │   ├── fx/           FX modules (one header/source per effect type)
│   │   ├── model/        params, descriptors, registry, drafts, chain model, state (JSON)
│   │   ├── engine/       Engine (audio side), chain runtime, router, handoff queues, telemetry
│   │   └── visual/       formation simulation (render-API-free)
│   └── src/
├── plugin/               JUCE 9 wrapper: AudioProcessor, editor, OpenGL formation renderer
├── tests/                doctest unit/integration tests for ih1_core
├── tools/                offline renderers (WAV test renders, formation preview frames)
├── installer/            Inno Setup script (Windows setup wizard)
└── .github/workflows/    Linux (tests + VST3 + pluginval) and Windows (VST3 + pluginval + installer)
```

## 1. Model (message thread)

- **ParamSpec** — stable string id, display name, unit, range, skew, default, kind (float / choice / toggle), choice labels, plain-language tooltip.
- **FxDescriptor** — stable `typeId` (e.g. `tape_echo`), display name, family, tooltip, `schemaVersion`, ParamSpec list (≤ `kMaxParams` = 32), factory → `FxProcessor`, `status` (`release` / `experimental`; experimental types are hidden from the production carousel).
- **FxRegistry** — static ordered list of descriptors. Carousel order = registry order (grouped by family).
- **ParamValues** — fixed `std::array<float, kMaxParams>` plain values. No allocation, trivially copyable.
- **DraftStore** — one `ParamValues` per registered type (per-type drafts survive carousel navigation; RESET restores defaults for the current type only).
- **CommittedInstance** — `{ uint64 id, typeIndex, ParamValues values (const), uint32 visualSeed }`. Immutable after construction; the class exposes no setters.
- **ChainModel** — ordered `std::vector<CommittedInstance>`, max `kMaxChain` = 32. Operations: `commit(type, values)`, `move(from, to)`, `remove(id)`. Every mutation produces a new prepared **ChainRuntime** for the audio thread.

## 2. Audio engine (audio thread)

```
in ─┬─► [committed 1] → … → [committed N] ─► c ─┐
    └─► latency-compensation delay (L_chain) ─► x'┤ soloMix a: s_in = (1−a)·c + a·x'
                                                  ▼
                                   staged FX (live draft params)
                                                  ▼
                       muteMix m: out = (1−m)·staged(s_in) + m·s_in
                                                  ▼
                            global mix / output gain / safety (NaN→0, soft ceiling)
```

- **SOLO** ramps `a` → 1 over 250 ms (committed output fades out; the chain keeps running so tails continue under the fade and it can fade back in naturally). **MUTE** ramps `m` → 1 over 30 ms. SOLO and MUTE are **mutually exclusive**: engaging one releases the other. No staged FX selected = staged stage is identity.
- **Staged FX** reads atomically published draft values each block; every FX smooths its own parameters.
- **Committed FX** received their values once at preparation and never get new values.
- **Latency** = Σ committed latencies + staged latency; reported to the host from the message thread when it changes.

## 3. Real-time-safe handoff (KADAL `KNW-H0EPVHBYCE`)

- **Ownership:** every `FxProcessor` is created, `prepare()`d and finally destroyed on the message thread, which owns them by instance id. A `ChainRuntime` is just a fixed array of raw processor pointers in processing order, plus the summed latency.
- **Publish:** the message thread builds a new `ChainRuntime` and posts it through a single-slot lock-free mailbox. At block start the audio thread sees it, ramps the chain output down (5 ms), swaps, and ramps back up. Processors keep their internal state, so **delay/reverb tails survive reorder, commit and delete** (only the deleted instance stops).
- **Retire:** the audio thread returns the old runtime through an SPSC retire queue. Only after a runtime comes back does the message thread delete processors that no newer runtime references. The audio thread never frees anything.
- **COMMIT hand-over:** the warmed-up staged processor *becomes* the new committed instance (its values are frozen at that moment), appended to the chain. Before: `staged(chain(x))`; after: `chain'(x)` with the same processor last. The sound is continuous with no re-initialisation. The carousel then has no selection (staged = identity).
- **Staged type change:** a new staged processor is prepared on the message thread and swapped with the same fade-out/swap/fade-in on the staged stage (10 ms).
- **Sample-rate / block-size change:** `prepareToPlay` re-prepares every owned processor while the audio thread is stopped (host contract), then republishes.

## 3b. Latency modes (V2.1)

- `LatencyMode { Quality, Low, Zero }` lives in the Session and is saved in DAW state (`session.latencyMode`). It is not part of presets.
- Each `FxDescriptor` carries `LatencyCaps`: zero / low availability, analysis window, oversampling, CPU class and user-facing notes. These are annotated in one table in `core/src/fx/Registry.cpp`, together with the catalogue `FxCategory` and the tempo-sync dependencies.
- `FxDescriptor::resolveLatency(requested)` picks the variant a type really builds. `Session::makeProcessor` calls `setLatencyMode(resolved)` before `prepare()`.
- `Session::setLatencyMode` rebuilds every processor (chain and staged) on the message thread, with the same ids, values and seeds, and posts one program. Old processors are retired through the normal queue.
- The processor reports `Session::latencySamples()` to the host via `setLatencySamples` on the message thread. `Session::latencyContributors()` names the effects that still add latency (the UI status and tooltip).

## 4. State (versioned JSON, written/parsed by `ih1_core`)

```json
{ "format": "ih1-state", "version": 1,
  "chain": [ { "id": "…", "type": "delay", "fxVersion": 1, "seed": 123, "params": { "time_ms": 375.0 } } ],
  "session": { "drafts": { "delay": { … } }, "selected": "reverb", "solo": false, "mute": false, "latencyMode": 0 } }
```
- **User preset** = same document without `session`.
- Parameters are stored **by string id**: unknown ids are ignored, missing ids take defaults, values are clamped to range, unknown FX types are skipped with a warning, and duplicate instance ids are re-issued. Per-type `fxVersion` lets a module migrate old parameter sets.

## 5. Host parameters (fixed, stable IDs)

`bypass`, `solo`, `mute`, `in_gain`, `out_gain`, `mix`, `staged_1` … `staged_8`. The staged slots are normalised 0–1 and mapped onto the current staged FX's first 8 parameters. The UI writes them through gesture-bracketed changes. When the staged type changes, the slots are re-synced to the new type's draft values.

## 6. Telemetry to UI

- **What the audio thread writes.** Per-block RMS and peak (in and out), coarse low / mid / high band RMS, output L/R balance (V2.1), the SOLO / MUTE ramp positions, and the RMS after each committed slot. All go into `std::atomic<float>` values sized `kMaxChain`.
- **How the formation reads it.** `AudioAnalyser` (`plugin/Source/FormationView.h`) reads these atomics only, at frame rate. It derives smoothed energy, onsets, bands and balance. The formation applies its own attack / release smoothing.
- **SOLO.** The formation's monochrome amount is the engine's solo ramp, so the picture always matches what is audible.
- **Effect read-outs.** Effects with live state (Looper, Grain Engine) expose `FxProcessor::readDisplay` through lock-free atomics, for the settings graph.
