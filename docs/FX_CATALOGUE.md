# FX Catalogue: Status Matrix

Status: **done** = implemented and passing the registry-wide contracts plus a behaviour test. **All 38 catalogue types are implemented.** Listening review by the owner is still pending for every one of them; compilation and tests are not a substitute for it.

Every implemented effect passes these contracts (`tests/test_fx_generic.cpp`):
- descriptor and tooltip completeness
- silence in → silence out
- bounded, finite output at default, minimum, maximum and random settings with hot input
- exact latency (mix = 0 returns the input delayed by the reported latency)
- block-size independence (below −40 dB difference) and determinism
- reset clearing state
- re-prepare at 44.1, 96 and 192 kHz

CPU is per instance at 48 kHz with 256-sample blocks, from `ih1_bench` on the CI/VM.

| # | Effect (id) | Batch | Status | Algorithm (KADAL basis) | Behaviour tests | Latency | CPU |
|---|---|---|---|---|---|---|---|
| 1 | Delay (`delay`) | A | done | Length-type delay, crossfaded time changes, multi-tap, ping-pong, filtered and saturated feedback, ducking (KNW-SA0JJFX4HE) | echo on the synced time; time change keeps pitch | 0 | 0.7% |
| 2 | Tape Echo (`tape_echo`) | A | done | Speed-type tape loop (tape equation), 3 heads, motor slew, wow/flutter, ADAA saturation in the loop (KNW-SA0JJFX4HE, KNW-DKQ16F12WF) | heads at 1x/2x/3x; time change bends pitch | 0 | 0.7% |
| 3 | Reverb (`reverb`) | A | done | Room/Hall/Ambience/Cathedral: 8-line Householder FDN with Jot absorbent filters. Plate: Dattorro tank. Gated and Reverse articulations; freeze (KNW-ZWRYBGRD0Y, KNW-5QZBASA3A6) | measured RT60 within 10% for Hall, Plate and Room; freeze sustains; gate closes | 0 | 1.4% |
| 4 | Reverse (`reverse`) | A | done | Reverse Buffer (2 Hann readers), Reverse Delay (tempo-aligned chunks with feedback), Reverse Grains | rising swell plays falling; continuity | 0 | 0.7% |
| 5 | Harmony (`harmony`) | A | done | 4 voices from one STFT: Laroche–Dolson peak shifting, true-envelope formants, YIN; Intervals / Key & Scale / MIDI with voice allocation, sustain and pitch bend (SRC-3Q84R4ZMZG, SRC-VWKQGV9FYC, KNW-Q1PTF3K9KM) | +7 st, scale third, MIDI note/release/sustain/all-notes-off | 2048 | 3.9% |
| 6 | Pitch Shift (`pitch_shift`) | A | done | Same engine, 2 voices, shift-feedback delay | +12 st doubles f0 | 2048 | 6.0% |
| 7 | Formant Shift (`formant`) | A | done | True-envelope warp, pitch preserved | pitch unchanged, centroid moves | 2048 | 4.8% |
| 8 | Doubler (`doubler`) | A | done | 2/4 voices: fixed micro-detune (TD shifter), distinct delays, filtered-noise drift | decorrelated and mono-safe | 0 | 0.5% |
| 9 | Chorus (`chorus`) | A | done | 1–4 modulated delays, phase-locked stereo LFOs, optional BBD-like colour (KNW-Y84ZDH4MWH, KNW-5AB5W7MA65) | FM sidebands present | 0 | 0.7% |
| 10 | Filter / Wah (`filter`) | A | done | Trapezoidal SVF 12/24 dB, LFO/envelope modulation, ADAA drive (KNW-R8PP5DNBY0) | −40 dB above cutoff; envelope opens | 0 | 0.2% |
| 11 | EQ / Dynamic EQ (`eq`) | A | done | SVF shelves/bells/cuts; dynamic bell 2 with band-limited detector | bell +6 dB measured; dynamic action only when loud | 0 | 1.3% |
| 12 | Saturation (`saturation`) | A | done | 6 ADAA curves at 2x oversampling, tilt pre/de-emphasis, bias, auto gain (KNW-MCXD9ZQP83, KNW-W26C85ANW0) | alias −51 dBc at +24 dB hard clip; level held | 23 | 2.0% |
| 13 | Granular Cloud (`granular`) | A | done | Rolling buffer, 96-grain pool, seeded scheduler, freeze, feedback, diffusion (KNW-01J68SK1RC) | freeze sustains | 0 | 0.8% |
| 14 | Spectral Freeze (`spectral`) | A | done | STFT magnitude freeze with phase propagation, time blur, frequency smear, shimmer | freeze sustains | 2048 | 2.2% |
| 15 | Stutter (`stutter`) | A | done | Host-grid capture and retrigger, seeded chance, pitch steps, reverse, decay | replays the captured slice | 0 | 0.1% |
| 16 | Stereo Width (`width`) | A | done | M/S width, LR4 mono-maker, decorrelated spread added to the side only (KNW-1VXWHJAGBV, KNW-61XX879RSM) | 0% is mono; mono sum exact | 0 | 0.1% |
| 17 | Flanger | B | done | Short modulated delay with polarity feedback (KNW-Y84ZDH4MWH); no through-zero | notch depth 19 dB on noise | 0 | 0.6% |
| 18 | Phaser | B | done | 2-12 first-order all-pass stages with resonance feedback | notch depth 18 dB on noise | 0 | 1.0% |
| 19 | Vibrato | B | done | 100% wet swept delay with depth calibrated in cents | 50 ct set gives 50.0 ct measured | 0 | 0.3% |
| 20 | Tremolo / Auto-Pan | B | done | LFO gain or constant-power pan, locked to the host grid | 25+ dB pulsing; panning power steady within 1 dB | 0 | 0.1% |
| 21 | Shimmer | B | done | Own 8-line FDN plus dual-tap pitch shifters routed to the tank, the input or both; capped regeneration | octave-up tail 13 dB over the source tone | 0 | 1.8% |
| 22 | Vocoder | B | done | 8-32 band SVF analysis/synthesis, polyBLEP saw/pulse/noise carrier, MIDI or fixed note, unvoiced noise | carrier harmonics present; silent when the modulator stops | 0 | 0.9% |
| 23 | Pitch Correction | B | done | YIN, scale snapping with hysteresis, retune speed, period-synchronous cycle repeat/drop (KNW-Q798GYV78J) | 448 Hz corrected to 439.5 Hz | 0 | 2.5% |
| 24 | Compressor / Limiter | B | done | Feed-forward soft knee in dB; Clean / FET (fast, program-dependent) / Opto (two-stage) / Limit (KNW-X0BEXVDR9Z) | static curve within 6% | 0 | 0.4% |
| 25 | Gate / Expander / Pump | B | done | Hysteresis, hold, key filters; tempo-synced pump (KNW-HJ3JZGNYZY) | -35 dB below threshold, unity above | 0 | 0.1% |
| 26 | De-Esser | B | done | Band detector, split-band or wideband, level-relative threshold, listen | -6 dB at 7 kHz, unity at 400 Hz | 0 | 0.2% |
| 27 | Transient Shaper | B | done | Fast/slow dB followers, attack and sustain gains (KNW-NFJ86FFD2E) | attack +22 dB vs tail +5 dB | 0 | 0.2% |
| 28 | Bitcrusher | B | done | Quantiser with TPDF dither option, sample-and-hold clock with jitter, anti-alias pre-filter | 3 bits gives at most 9 levels | 0 | 0.1% |
| 29 | Ring Modulator | B | done | Sine/tri/polyBLEP-square carrier with drift LFO | sidebands 40 dB over carrier and input | 0 | 0.2% |
| 30 | Tape / Console Colour | B | done | Calibrated ADAA saturation at 2x, head bump, HF loss, console asymmetry, auto gain | level held within 1.5 dB at +18 dB drive | 23 | 1.5% |
| 31 | Rotary Speaker | B | done | LR4 crossover, horn/drum Doppler delays, AM, inertia, two mics | AM over 3 dB at Fast | 0 | 1.1% |
| 32 | Frequency Shifter | C | done | Hilbert pair (90 deg +-0.7 deg, 50 Hz-20 kHz) and SSB mixing; climbing feedback | +100 Hz shift, opposite sideband -49 dB | 0 | 0.7% |
| 33 | Doppler | C | done | Pass-by/circle trajectory to propagation delay, distance gain and pan | 1116 Hz approaching / 884 Hz receding at 40 m/s | 0 | 0.2% |
| 34 | Looper | C | done | Buffer hold: capture on first sound / bar line / every 4 loops; overdub, speed, reverse; parameters only in state (C3) | replays the captured phrase (correlation over 0.95) | 0 | 0.1% |
| 35 | Resonator / Modal | C | done | Tuned Karplus-Strong comb bank in chords (KNW-1JA0N4393Q) | lowest partial at the tuned 220 Hz | 0 | 0.5% |
| 36 | Convolution / IR | C | done | Uniform partitioned overlap-save (P=256); 6 built-in generated IRs precomputed; crossfaded IR switching | decaying tail; click-free switching | 256 | 8.4% |
| 37 | Exciter | C | done | High-pass band, ADAA shaper at 2x, generated harmonics added level-matched | +30 dB at the 2nd harmonic | 23 | 1.5% |
| 38 | Amp / Cabinet (`amp`) | C | done | Two ADAA preamp stages at 2x, tone stack, power sag, 3 cabinet models | 3rd harmonic present; cabinet rolls off 9 kHz by more than 15 dB | 23 | 5.1% |

## V4 additions (0.4.0)

Details, distinctness and measurements: `docs/V4_IMPLEMENTATION.md` section 3. Behaviour tests: `tests/test_v4.cpp`.

| # | Effect (id) | Batch | Status | Algorithm | Behaviour test | Latency (48 kHz, Full) |
|---|---|---|---|---|---|---|
| 46 | Slice Shuffler (`slice_shuffle`) | V4 | done | previous-bar capture, deterministic slice permutation, per-slice reverse / pitch / gate / swing, 2-voice crossfades | replays the previous beat in reverse slice order, deterministic | 0 |
| 47 | Attack / Body (`transient_split`) | V4 | done | exact transient / body split; attack delay + repeats; body reversed / held / smeared | neutral = input (-60 dB error); attacks removable | 0 |
| 48 | Echo Matrix (`echo_matrix`) | V4 | done | 8-tap step grid, per-tap level / pan / darkening / rotating-head pitch, filtered + driven cycle feedback | taps on the grid; climbing pitch raises the centroid | 0 |
| 49 | Reverse Pitch Echo (`reverse_echo`) | V4 | done | overlapping reversed grains at a pitch ratio inside bounded feedback | rising ramp returns falling; +12 st lands near 600 Hz | 0 |
| 50 | Dispersion (`dispersion`) | V4 | done | up to 64 TPT 2nd-order all-passes, curve, motion, bounded feedback | level within 1 dB; a pulse is spread in time | 0 |
| 51 | Modal Resonator (`modal`) | V4 | done | 24 two-pole modes with material ratio sets, damping, inharmonicity, resonance guard | rings at the material's 2nd-mode ratio | 0 |
| 52 | Texture Imprint (`texture`) | V4 | done | source-following dust / crackle / static / friction / grit, seeded, presence-gated | deterministic per seed; follows the source | 0 |
| 53 | Spectral Shatter (`spectral_shatter`) | V4 | done | STFT shard translation with N/4-hop phase rotation, freeze / gate, crossfaded maps | a tone is relocated between bands | 1024 (Low 512) |
| 54 | Partial Recomposer (`partial_recompose`) | V4 | done | peak / region partials moved by individual ratios, identity phase locking, sustain, residual | stretch raises the centroid > 25 % | 2048 (Low 512) |
| 55 | Spectral Graft (`spectral_graft`) | V4 | done | one-input envelope transfer from a held moment / echo / opposite channel | a dark snapshot darkens a bright input | 1024 (Low 512) |
