# IH1 — Project Status

**Phase:** 0 complete → Phase 1 decisions presented, awaiting answers.
**Updated:** 2026-11-18

## Done
- Repo audit: `R4N-WAV/ih1`, branch `claude/new-session-l50e2f`. Empty repository (no prior commits or files), so nothing existing to preserve.
- Reference assets placed: `references/ih1_visual_reference.mp4`, `references/ih1_ui_sketch.png`. Source brief and master prompt archived in `docs/source/`.
- Read the full research brief (all 12 sections) and the master prompt.
- Video analysed end-to-end with measured palette, luminance, asymmetry and motion: `docs/VISUAL_REFERENCE_ANALYSIS.md`.
- KADAL audited (snapshot `a053848`; 770 indexed records; about 55 queries): `docs/KADAL_RESEARCH_MAP.md`. Coverage: strong for 19 catalogue families, partial for 11, gap for 8, plus no visual research.
- Phase 1 decisions written up with recommended defaults: `docs/OPEN_DECISIONS.md`.

## Key findings
- KADAL supports a **single-STFT, multi-voice Harmony engine** (Laroche–Dolson peak shifting + true-envelope formants) and gives clear Delay vs Tape Echo topologies, FDN + Dattorro reverbs, validated ADAA/SVF C++ patterns, and RT rules that match the commit handoff design.
- The owner's Isai plugin sets the house architecture: a JUCE-free C++17 engine with a permanent parameter-ID table, wrapped in JUCE.

## Next (safe work that doesn't depend on open decisions)
1. CMake skeleton + framework-agnostic `ih1_core` library + unit-test harness on Linux.
2. Core model: FX descriptor registry, stable type IDs, parameter schemas, per-type draft store, immutable committed instances, versioned state serialiser.
3. Real-time-safe chain handoff (prepare off-thread → SPSC/atomic swap → deferred retirement), proven with a gain/filter test effect + commit/reorder/delete/solo/mute tests.
4. Shared primitives with KADAL-derived tests: FractionalDelay, SVF, ADAA shapers, smoothers, denormal guard.
5. Standalone formation-renderer prototype, compared against the reference video.

## Catalogue status matrix
All 37 catalogue types: **planned** (none implemented). Batch A starts after decisions A1/C1.
