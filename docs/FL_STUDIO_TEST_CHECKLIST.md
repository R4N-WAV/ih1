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
