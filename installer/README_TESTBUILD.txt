IH1 {VERSION} - Windows test build, feedback round 4
====================================================

What this is
  IH1 is a creative sound-design FX instrument (VST3, 64-bit) by R4N-WAV.
  Version {VERSION}, build {COMMIT}. This is a TEST BUILD for the round-4 feedback pass, not a release.

Contents of this ZIP
  IH1-Setup-{VERSION}.exe   the setup wizard
  README.txt                this file

Install
  1. Close FL Studio (and any other DAW that has IH1 loaded).
  2. Run IH1-Setup-{VERSION}.exe and follow the wizard (administrator rights are needed for the VST3 folder).
     The plug-in is installed to:  C:\Program Files\Common Files\VST3\IH1.vst3
     The standalone app is optional.
  3. FL Studio: Options > Manage plugins > Find more plugins. If an older IH1 still loads, tick
     "Rescan previously verified plugins" and rescan. Add IH1 to a mixer insert.
  4. Check the small text under the IH1 wordmark: it should read v{VERSION} {COMMIT}.

Your data
  Your own presets live in Documents\IH1\Presets. Installing, updating or uninstalling never changes or
  deletes them. Factory presets are built into the plug-in.

Supported
  Windows 10 / 11, 64-bit VST3 hosts. Primary target: FL Studio. OpenGL 3.2 or newer for the full formation
  (IH1 falls back to a software formation automatically; audio is never affected).

Test status of this build (see docs/V4_IMPLEMENTATION.md in the repository for the evidence)
  * Automated: unit / behaviour / factory-audit tests, VST3 host-lifecycle tests and editor tests run on
    Linux; the Windows CI builds, runs the unit and host tests and pluginval, and builds this installer.
  * NOT tested in FL Studio by the developer (no Windows / FL Studio in the build environment). Please use
    docs/FL_STUDIO_TEST_CHECKLIST.md.

What is new in 0.4.0 (round 4)
  * 148 factory presets in ten categories (searchable by sound, source and style), each with up to four
    performance macros (Preset Macro 1-4, automatable; committed settings never change).
  * Ten new effects: Slice Shuffler, Attack / Body, Echo Matrix, Reverse Pitch Echo, Dispersion, Modal
    Resonator, Texture Imprint, Spectral Shatter, Partial Recomposer, Spectral Graft (55 effects in total).
  * Click / pop fixes in the chopping, gating and buffer effects; parameter changes glide.
  * None is a normal carousel entry; click a committed effect's name to bypass / enable it.
  * The formation is one continuous field (no per-effect centres), more vibrant, on a plum void.

Safety
  IH1 has an Output Safety stage (chain menu "...": Off / Guard / True Peak, ceiling). It is on (Guard,
  -0.5 dBFS) by default. No plug-in can guarantee the safety of speakers or hearing: keep your interface and
  monitor levels sensible, especially when trying extreme or high-feedback presets.

Known issues
  See docs/KNOWN_ISSUES.md (V4 section) in the repository.
