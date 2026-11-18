# FX Catalogue: Status Matrix

Status: **done** = implemented and passing the registry-wide contracts plus behaviour tests. Listening review by the owner is still pending for all of them. **planned** = not yet implemented, never shown in the carousel.

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
| 17 | Flanger | B | planned | Short modulated delay with polarity feedback (KNW-Y84ZDH4MWH) | | | |
| 18 | Phaser | B | planned | 2–12 SVF all-pass stages with feedback | | | |
| 19 | Vibrato | B | planned | 100% wet modulated delay, depth in cents | | | |
| 20 | Tremolo / Auto-Pan | B | planned | LFO/envelope gain, constant-power pan, shared clock | | | |
| 21 | Shimmer | B | planned | Reverb tank plus pitch shifter in input/regeneration paths | | | |
| 22 | Vocoder | B | planned | SVF filter-bank analysis/synthesis with internal carrier | | | |
| 23 | Pitch Correction | B | planned | YIN + note hysteresis + cycle-repeat shifter (KNW-Q798GYV78J) | | | |
| 24 | Compressor / Limiter | B | planned | Feed-forward soft-knee core; FET/opto behaviours; true-peak limiter (KNW-X0BEXVDR9Z) | | | |
| 25 | Gate / Expander / Ducker | B | planned | Shared detector, hysteresis and hold (KNW-HJ3JZGNYZY) | | | |
| 26 | De-Esser | B | planned | Band detector to split-band or wideband gain | | | |
| 27 | Transient Shaper | B | planned | Fast/slow dB followers (KNW-NFJ86FFD2E) | | | |
| 28 | Bitcrusher | B | planned | Quantiser plus sample-and-hold with jitter and dither | | | |
| 29 | Ring Modulator | B | planned | Band-limited carrier oscillators | | | |
| 30 | Tape / Console Colour | B | planned | Calibrated level, head bump, hysteresis/triode (KNW-DKQ16F12WF, KNW-MQRRKX0DSE) | | | |
| 31 | Rotary Speaker | B | planned | Crossover, horn/drum Doppler delays, AM and inertia | | | |
| 32 | Frequency Shifter | C | planned | Hilbert all-pass pair and quadrature oscillator | | | |
| 33 | Doppler | C | planned | Trajectory-driven modulated delay | | | |
| 34 | Looper | C | planned | Long-buffer record/overdub state machine (parameters only in state; decision C3) | | | |
| 35 | Resonator / Modal | C | planned | Tuned comb/EKS bank (KNW-1JA0N4393Q) | | | |
| 36 | Convolution / IR | C | planned | Uniform partitioned FFT; off-thread IR loading | | | |
| 37 | Exciter | C | planned | High-pass band, ADAA shaper, level-matched blend | | | |
| — | Amp / Cabinet | C | planned | Triode stages, tone stack, cabinet impulse response | | | |
