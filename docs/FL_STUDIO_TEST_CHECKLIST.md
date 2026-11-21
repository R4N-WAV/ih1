# FL Studio acceptance checklist (V3, 0.3.0 test round 3; V2.1 items follow)

**FL Studio validation was not performed by the developer.** Install from the CI artifact
`IH1-windows-testbuild-zip` (`IH1_0.3.0_Windows_TestBuild_Round-3.zip`). Note FL build, Windows, GPU / driver,
audio device and buffer size. Keep monitor levels low for the safety items.

## V3: installer and build
- [ ] The ZIP opens and contains `IH1-Setup-0.3.0.exe` and `README.txt`.
- [ ] The wizard shows the IH1 formation panel, a dark header, version 0.3.0 and "test build, round 3".
- [ ] It installs to `C:\Program Files\Common Files\VST3\IH1.vst3`; existing files in `Documents\IH1\Presets` are untouched.
- [ ] After a rescan, the text under the IH1 wordmark reads `v0.3.0 <build>`. Uninstall leaves `Documents\IH1\Presets`.

## V3: presets always audition alone (Feedback 1, 2)
- [ ] Stage Delay (or Saturation / Grain Engine / Reverse / Pitch Shift), press the preset **next** arrow while playing:
  the wheel shows **None**, the settings panel hides, only the preset is heard, no click.
- [ ] Same via the preset browser, and via FL's own preset menu / project reload (host state).
- [ ] Click next / previous rapidly for ~10 s during playback: nothing staged is ever heard, no pops.
- [ ] Scroll back to the previously staged FX: its settings are still as you left them (until RESET).
- [ ] Right-click the wheel: **None** is the first row, ticked when nothing is staged; choosing it un-stages.
- [ ] **Init** (first preset): empty chain, None, settings at defaults, output gain 0 dB, no loop / freeze left over.

## V3: level safety (Feedback 8)
- [ ] With a hot input, scroll onto **Saturation** with Drive at maximum: no level jump on landing.
- [ ] Land on Transient Shaper, Delay, Doubler, Chorus, Pitch Correction: no click on landing.
- [ ] Grain Engine in Loop and Freeze with high pitch and feedback: no clicks when freeze engages / releases.
- [ ] Chain options (···) > Output Safety: Guard is ticked by default; the meter left of ··· shows peak and reduction.
- [ ] True Peak: FL's delay compensation shows the extra 72 samples (48 kHz); in Zero Latency mode it says Guard runs.
- [ ] Safety Off with a high-feedback preset (e.g. *Dub Machine*): a warning appears.

## V3: commit and formation (Feedback 3, 4)
- [ ] COMMIT: a small packet travels into the field; the new FX's colour region appears only when it arrives.
- [ ] Double-click COMMIT: exactly one packet / one instance. Close the editor mid-transfer and reopen: the FX is there.
- [ ] The formation reads as luminous field sheets in depth (no drawn lines, no fog), in the accepted palette, black
      around it, slow motion; it brightens with the music. Resize aggressively for a minute: no crash.
- [ ] Note the GPU and whether the editor stays smooth (the volume steps its quality down automatically).

## V3: sound
- [ ] Try 5-10 presets from each creative category on a vocal, a drum loop and a pad; note any that sound timid,
      broken or too loud. New modules: Degrade, Fold & Crush, Spectral Smear, Spectral Delay, Time Warp, Glitch Scatter.

---

# FL Studio acceptance checklist (V2.1)

**FL Studio validation was not performed in this pass.** No Windows machine or FL Studio was available. Everything
below still needs a person in FL Studio (VST3, 64-bit). Tick each item, and note the FL Studio build, Windows version,
GPU and driver, audio device and buffer size.

## Setup
- [ ] Install `IH1-Setup-<version>.exe` from the CI artifact.
- [ ] Rescan plug-ins, then load IH1 on a mixer insert.
- [ ] Use a playing loop, for example drums plus a pad or a vocal.
- [ ] Repeat the key items once with FL's "Make bridged" option.

## Audio and latency (Phase 16)
- [ ] Load *Essentials / Velvet Plate*. Open the chain options (··· at the bottom right of the field):
  - [ ] it shows Full Quality / Low Latency / Zero Latency;
  - [ ] the status line shows samples and ms.
- [ ] Build a chain of EQ + Saturation + Compressor and switch to **Zero Latency** while playing:
  - [ ] the status reads `Zero Latency 0 samples`;
  - [ ] FL's plug-in delay readout shows 0;
  - [ ] there are no clicks, apart from tails restarting.
- [ ] Add Harmony with Zero requested:
  - [ ] the status names Harmony and its sample count;
  - [ ] FL compensates: the processed track stays in time with a dry duplicate. Check by nulling a phase-inverted copy at Mix 0 %.
- [ ] Switch modes while stopped and while playing. Note any playback interruption, which is FL's delay-compensation recompute.
- [ ] Sample rates 44.1, 48 and 96 kHz.
- [ ] Buffer sizes 64, 256 and 1024: no dropouts with a 6-FX chain including Grain Engine and Convolution.

## Clear FX Chain (Phase 15)
- [ ] Commit 5 FX, including two of the same type, then reorder them.
- [ ] Click ···, then *Clear FX Chain…*. Check each way out leaves the chain untouched:
  - [ ] Escape;
  - [ ] clicking outside the dialog;
  - [ ] Cancel.
- [ ] Clear Chain while playing:
  - [ ] the audio continues dry and click-free;
  - [ ] the formation empties, with no ghost strands;
  - [ ] the staged FX and its settings remain.
- [ ] Save the project, reopen it, and check the chain is still empty. Load a preset afterwards.

## Workflow
- [ ] Preset arrows: click fast 30+ times, alternating, including double-clicks. Each click moves exactly one preset, and the name matches the browser's order.
- [ ] Right-click the carousel:
  - [ ] category menu beside the cursor;
  - [ ] moving diagonally into a submenu keeps it open;
  - [ ] the submenu stays visible near the window's right and bottom edges.
- [ ] Delay, Tape Echo, Reverse, Vibrato, Tremolo, Filter / Wah, Doppler, Grain Engine:
  - [ ] with Sync on, the free time or rate knob is greyed out and ignores dragging and the wheel;
  - [ ] its tooltip explains why;
  - [ ] Division stays live;
  - [ ] turning Sync off restores the old value.
- [ ] Advanced controls are visible without any More button.
- [ ] MUTE (M) greys the carousel, settings, COMMIT and RESET, and the staged FX is silent. Unmute restores everything.
- [ ] SOLO (S) turns the formation grey while only the staged FX is heard. Colours return when SOLO is off.
- [ ] COMMIT 10 times quickly, including duplicates: a coloured packet flies into the formation every time.
- [ ] Automate Staged Control 1–8 from FL and check it plays back.

## Grain Engine
- [ ] *Stretch & Smear / Half-Time Haze* on a vocal: pitch kept, half speed.
- [ ] *Tape Slowdown* is an octave down.
- [ ] *Glitch & Rhythm / Slice Retrigger*, *Backspin Slices*: slices locked to FL's tempo, with tempo changes mid-song.
- [ ] *Harmonic & Tonal / Minor Drone* loaded during silence starts looping only after sound arrives (never loops silence).
- [ ] Freeze while playing holds the moment, and Freeze off returns to live.
- [ ] Feedback at 100 % never runs away, and decays when the input stops.

## Editor and GPU stability
- [ ] Resize the window fast and drastically for 60 s while playing, with a 6-FX chain.
- [ ] Close and reopen the editor 20 times quickly while playing.
- [ ] Hide and show the plug-in window, and switch FL projects.
- [ ] Use two IH1 instances with both editors open, then close one.
- [ ] Display scaling at 100 %, 125 %, 150 % and 200 %.
- [ ] Check the formation:
  - [ ] the diagonal composition, with no sideways T;
  - [ ] near-black background, with a faint violet response only around the formation;
  - [ ] it reacts to transients.

## Soak
- [ ] 30 minutes of playback with a demanding chain (Grain Engine, Harmony, Convolution, Reverb, Saturation, Delay) and the editor open. Note CPU, RAM in Task Manager, and any glitches.

Report anything unexpected with the steps, so it can be reproduced in `ih1_hosttest` or `ih1_uitest`.
