# IH1 — Research & Engineering Brief

**Project:** IH1 (working title)  
**Repository:** `R4N-WAV/ih1`  
**Prepared:** 18 November 2026  
**Revision:** 2 — expanded effect catalogue and DSP research  
**Purpose:** Source-backed research and implementation guidance for a creative VST3 audio-effects instrument built around an immutable “commit” workflow and a living, audio-reactive force-field visual.

---

## 0. How to use this document

This brief separates three kinds of statements:

- **Documented fact** — supported by an interview, artist statement, manual, or technical specification.
- **Engineering interpretation** — a practical way to translate a documented technique into a modern plug-in.
- **Creative proposal** — an idea that may fit IH1, but is **not** claimed to be part of Imogen Heap’s actual setup or sound-making process.

The intention is not to make a preset or impersonation of one particular record. The intention is to understand the musical and technological methods Heap has publicly described—especially human-controlled harmony, vocal transformation, unusual source material, delay-based space, and gestural control—and translate those principles into an original, flexible instrument. Do not market any proposed DSP module as “used by Imogen Heap” unless a source in this brief directly supports that claim.

**Important project constraint:** The attached visual-reference video is the primary authority for the FX-field artwork. The written observations below describe what is visible; they do not replace the video. The user's Paint sketch is the primary authority for the left-side selector, settings panel, SOLO/MUTE, COMMIT/RESET, and interaction concepts.

## 1. Product concept in one page

IH1 is a creative serial FX processor with two interacting systems:

1. **A staged effect editor:** the user scrolls through an FX carousel, edits one currently highlighted effect, and hears that uncommitted effect after the already committed chain unless it is muted. Its draft settings survive carousel navigation.
2. **A committed FX formation:** pressing **COMMIT** copies the staged effect and all its current parameter values into the committed chain. A committed instance becomes immutable: it can be moved in the chain or deleted after confirmation, but cannot be edited. Duplicate FX types are allowed.

The committed chain is both the actual serial audio-processing order and the data source for the large visual formation. It should not be confused with an ordinary node-editor diagram. The visual should feel like the organic, luminous magnetic/filament formation in the supplied video.

The sound should be spacious, expressive, unusual, and performable—not a collection of generic utility effects. The effect library is intended to be **broad and expandable**, with as many genuinely useful, sonically distinct effects as can be implemented to a high standard. The sketch's visible **DELAY, REVERB, and REVERSE** labels are examples of the carousel and its workflow—not a limit on the effect count. The product should grow into a deep creative multi-FX instrument spanning time/space, pitch and voice, modulation, dynamics, tone, spectral processing, and granular transformation.

The strongest documented Heap-related foundations remain:

- four-voice, MIDI-guided vocal harmonisation with formant and pitch character controls;
- a meaningful dry/processed balance and independent voice level/pan;
- delays used as a source of space and rhythmic reflection;
- long, highly shaped reverb as a special sound-design tool;
- reverse processing, volume automation, field recordings, and transformed acoustic objects;
- expressive, gesture-like control of sound parameters.

These Heap-related principles inform the musical identity, but they do not restrict IH1 to vocal effects. Every effect must work as sensibly as possible on general mono/stereo audio. The catalogue in Section 4 is the expanded product scope and technical research map. It distinguishes established effect classes from IH1-specific creative extensions.

---

## 2. Imogen Heap: documented technology and production methods

### 2.1 “Hide and Seek”: the most concrete technical reference

**Documented facts**

- “Hide and Seek” originated during the *Speak for Yourself* sessions after Heap's computer failed. She used a borrowed **DigiTech Vocalist Workstation EX**, a microphone, a keyboard/MIDI control, and a small four-track MiniDisc recorder.
- The signature sound is frequently called a vocoder, but the documented mechanism is more specific: the Vocalist's “Vocoder” preset created up to four transposed copies of the input voice, selected/pitched from MIDI keyboard notes, and manipulated formants. It did not use the conventional two-signal synth-carrier-plus-vocal-modulator architecture normally meant by a vocoder.
- The unit was set to four-note polyphony. If more than four keys were played, it selected only four notes, sometimes producing unexpected inversions. Heap described this happy unpredictability as part of the creative result.
- Heap described the balance as roughly **50/50** between the natural lead voice and the processed harmony sound.
- The final recording retained meaningful breath detail and subtle environmental recordings. Heap also described spending considerable time on delays and creating “air” around the voice.

**Sources**

- [MusicRadar — “How a crashed PC led to the creation of Hide and Seek” (25 July 2025)](https://www.musicradar.com/artists/imogen-heap-hide-and-seek) — detailed account of the Vocalist Workstation EX, four MIDI-selected vocal layers, formant manipulation, and the difference between the preset and a conventional vocoder.
- [iZotope — “Get That Sound with VocalSynth: Harmony Effects”](https://www.izotope.com/community/blog/get-that-sound-with-vocalsynth-harmony-effects) — reproduces Heap's first-person explanation of keyboard-selected harmonies, four-note polyphony, and approximately 50/50 dry/harmony balance.
- [Aurgasm — 2005 interview with Imogen Heap](https://aurgasm.us/2005/10/aurgasm-interview-imogen-heap/) — first-person description of the original MIDI-controlled harmonizer session.
- [Journal on the Art of Record Production — “Imogen Heap as Musical Cyborg”](https://www.arpjournal.com/asarpwp/imogen-heap-as-musical-cyborg-renegotiations-of-power-gender-and-sound/) — secondary academic discussion of the relationship between voice, keyboard, harmonizer, and creative agency.

**Engineering interpretation for IH1**

A convincing artist-informed Harmony module should not be limited to a generic “+3rd / +5th” pitch-shifter. It should allow the user to actively compose the harmony arrangement:

- a dry lead path plus up to four independently controllable harmony voices;
- MIDI-note mode for explicit note selection;
- chord/key/scale modes for musically constrained harmony;
- configurable voice count or voice-priority behaviour when more notes arrive than voices can play;
- per-voice level and stereo position;
- separate pitch interval/target, formant character, detune, vibrato, and note-entry glide/scoop controls;
- a simple dry-versus-harmony blend control, defaulting to a useful middle position rather than forcing a fully wet result.

The four-voice arrangement and expressive note selection are more important than blindly cloning any legacy hardware control panel.

### 2.2 What the Vocalist Workstation EX actually offers

The owner’s manual is valuable because it documents the control vocabulary of the hardware that inspired the *Hide and Seek* sound.

**Five harmony types in the manual**

1. **Chordal:** the player supplies the current chord; harmony voices follow chord tones and inversions.
2. **Scalic:** the player supplies key and scale; harmonies move within that scale.
3. **Vocoder:** the keyboard/MIDI notes specify harmony pitches directly.
4. **Chromatic:** fixed chromatic intervals; useful for unison thickening and non-scale-specific layers.
5. **Pitch Correct:** pitch is controlled using pitch bend, keyboard, or MIDI note input.

Other documented controls include:

- **Gender/formant character:** shifts overtones/formant character up or down independently of the harmony interval.
- **Detune:** per-voice detune, randomness, and continuous pitch randomisation. The manual describes a ±100-cent range for the main amount and small random deviations.
- **Vibrato:** per-voice depth, rate, waveform, delayed onset, and randomness.
- **Scoop:** a quick pitch approach from below a note, with occurrence, amount, rate, and optional MIDI-velocity triggering.
- **Reverb:** room type, decay time, brightness/high-frequency decay, and HF rolloff.
- **Mixing:** independent lead/harmony balance, individual voice levels, individual pans, and separate lead/harmony reverb amounts.
- **MIDI control:** chord/key/style selection, separate voice channels in four-channel mode, and continuous control of volume, detune, vibrato depth/rate, and harmony hold.

The manual describes five room choices—Studio, Chamber, Club, Hall, and Arena—with documented decay ranges from short studio ambience to very long arena tails. This is useful evidence for a flexible range of spaces, though IH1 need not copy the hardware's preset names or limits.

**Sources**

- [DigiTech Vocalist Workstation EX manual (rehosted searchable manual)](https://device.report/m/7bc0be4fd8fd6603bda953cf934829cd9faa6421458037301bb3304750172c53) — see “Understanding the Five Harmony Types,” “Gender,” “Detune,” “Vibrato,” “Scoop,” “Reverb,” “Mix,” and MIDI sections.
- [DigiTech official product-manual index](https://digitech.com/product-manuals/) — confirms the manufacturer still lists the Vocalist Workstation EX manual.
- [MusicRadar 2025 article](https://www.musicradar.com/artists/imogen-heap-hide-and-seek) — connects the EX unit specifically to “Hide and Seek.”

**Important distinction:** The detailed manual documents features available on the Vocalist Workstation EX platform. It does not mean every feature was used on “Hide and Seek,” nor that each one is a signature Heap technique. Treat them as a historically grounded design vocabulary.

### 2.3 *Ellipse*: gear and recording/processing workflow

A detailed *Sound On Sound* feature on *Ellipse* documents a different, more studio-oriented set of tools and practices.

**Documented equipment and software**

- **Neumann TLM 103** vocal microphone.
- **Avalon VT-737** tube channel strip in the vocal recording chain.
- **sE Electronics Reflexion Filter** around the vocal microphone.
- **TC-Helicon VoiceWorks Plus** vocal processor in the studio rack.
- **Electrix Repeater** delay/looper in the portable vocal rack.
- **Pro Tools** as the central recording/mixing environment, with a Digidesign D-Control surface.
- **Waves SuperTap** as a main delay tool; Waves TrueVerb used for sound design.
- **Waves Renaissance Compressor**, **Focusrite D2 EQ**, **Focusrite D3 compressor**, and **Waves S1** stereo widening in the mixing workflow.

**Documented production methods**

- Heap preferred delay-based space in many contexts, saying she did not generally like the blanket-like wash of reverb and preferred the space created by delay.
- This is not an absolute rejection of reverb: she also described a jaw harp being sent through a very long TrueVerb reverb, volume-ridden, processed, reversed, and sent through TrueVerb again. The point is that space is actively shaped to the source and arrangement.
- Many sounds began with unusual acoustic sources or everyday objects rather than a finished synthesizer preset. Examples in the article include a whirly plastic tube pitched down for an opening pad and plastic ceiling light panels tapped to create timpani-like rhythm.
- A jaw harp was processed through several stages until it became a low, growling pulse and a rhythmic element.
- She frequently captured sounds in different rooms or spaces to obtain a natural sense of distance and room character.
- Vocals were built early, with harmonies and many vocal rides completed before other arrangement elements. This ensured that the voice remained intelligible and emotionally present.
- Some vocal doubling was used, but tightly edited backing-vocal layers were an important way to create thickness.
- To prevent harsh “s” and “t” consonants from triggering distracting echoes, the article describes a technique of creating a copy of the vocal with those consonants manually removed, then using that edited track to feed the vocal delay.
- She preferred manual volume editing to a conventional de-esser for the specific vocal workflow described.
- Despite dense sessions, she deliberately kept the arrangement open by limiting how many elements were prominent simultaneously.

**Primary source**

- [Sound On Sound — Simon Sherbourne, “Imogen Heap: Recording Ellipse” (December 2009)](https://www.soundonsound.com/people/imogen-heap-recording-ellipse). The search-indexed article text was available during preparation, although the site blocked direct page opening. Treat Sound On Sound as the primary source for the details above.

**Engineering interpretation for IH1**

- Prioritise **delay as an expressive space-building effect**, not only as a tempo echo.
- Reverb should offer a wide and beautiful range, but the default aesthetic should avoid simply burying the input in dense noise.
- Add optional tonal shaping inside delay/reverb feedback paths so the user can create depth while retaining vocal clarity.
- A future “Consonant-Safe Send” or “Transient-Sensitive Send” mode could attenuate sibilant/high-transient content in the delay input. This is an engineering translation of the documented manual-editing workflow, not a feature Heap is documented as using in a plug-in.
- Give users ways to turn a small, dry, recognisable sound into a large transformed texture through gain automation, reverse, pitch, delay, and reverb combinations.

### 2.4 Mi.Mu Gloves and gestural music control

**Documented facts**

- Heap and a team of specialists developed the wireless **Mi.Mu Gloves** as a gestural performance system over several years.
- Heap has described the goal as reaching into the software while continuing to sing or play, rather than being tied to faders and buttons.
- She maps glove movements into Ableton Live using MIDI note and CC messages. A left-to-right arm movement can pan a sound; pointing toward a location can control reverb length.
- The official Glover software is based on mappings from continuous movements and discrete events to musical outputs, including MIDI and OSC.
- The glove system uses finger flex sensors and wrist inertial measurement data. Glover turns these into posture, position, movement, and acceleration inputs. Inputs can be normalised and mapped to output ranges; thresholds can turn continuous movement into events; reset-on-release can return a parameter to a chosen value.

**Sources**

- [Spotify for Artists — “Imogen Heap on Technology for Artists” (12 August 2019)](https://artists.spotify.com/fi/blog/imogen-heap-on-technology-for-artists) — first-person account of the gloves and why they were developed.
- [The MIDI Association — “Imogen Heap Exclusive Interview”](https://midi.org/imogen-heap-exclusive-interview) — Heap explicitly describes mapping gloves into Ableton via MIDI, panning with arm movement, and reverb-length control with a pointing gesture.
- [Mi.Mu official documentation — Mapping](https://www.mimugloves.com/documentation/mapping/) — describes continuous movement, discrete events, MIDI, OSC, thresholds, and reset-on-release.
- [Mi.Mu official documentation — Glove Device Inputs](https://www.mimugloves.com/documentation/glove-device-inputs/) — describes flex sensors, IMU-based movement data, postures, and buttons.
- [Mi.Mu official documentation — Introduction to Glover](https://production.mimugloves.com/documentation/intro-to-glover/) — describes Glover as gesture-to-MIDI/OSC mapping software.
- [What Hi-Fi? — “I want studio tricks in my hands on stage” (2015)](https://www.whathifi.com/news/promoted-i-want-studio-tricks-in-my-hands-stage-imogen-heap) — describes the expressive/performative connection between movement, effects, and sound.

**Engineering interpretation for IH1**

Make the effect controls feel performable. At minimum, consider:

- DAW automation for stable/global parameters;
- MIDI learn or assignable MIDI CC mapping for a carefully selected set of continuous controls;
- optional MIDI note input for harmony;
- smooth parameter ramps so fast gesture changes do not click;
- optional latch/hold and reset-on-release behaviours for macro controls.

Do not claim that a VST3 plug-in can directly receive Mi.Mu wireless sensor data automatically. The practical first step is standard host-routed MIDI/automation. OSC or direct glove support would be a separate integration project and should not be added without approval.

---

## 3. Sound-design principles to translate into IH1

These are the most useful recurring ideas across the sources, expressed as product principles rather than as an attempt to imitate a particular record.

### 3.1 Human source, transformed identity

Keep the source recognisable enough to remain emotionally connected, while allowing the processed layers to become unfamiliar. A dry/processed mix, separate voice levels, and controllable formant/pitch character are essential to this principle.

### 3.2 Harmony as an instrument, not a correction utility

Do not treat pitch processing solely as automatic tuning. A user should be able to play or sequence harmony notes, create wide voicings, choose inversions, vary the number of active voices, and introduce deliberate instability or movement.

### 3.3 Space through reflections, motion, and contrast

Delay can create a sense of space without filling every gap. Reverb can become a large textural transformation when intentionally pushed. Different sources should be able to have different amounts of delay and reverb rather than one undifferentiated wet bus.

### 3.4 Start from unusual sources

A good effect should work on vocals, keys, guitar, percussion, field recordings, and found sounds. Do not hard-code the plug-in as “vocal only.” The interface may lead with vocal-friendly examples, but the DSP should accept ordinary mono and stereo audio.

### 3.5 Movement should be musical

Continuous parameter changes should sound like a performance, not like someone violently moving a software slider. Use smoothing, musical ranges, sensible defaults, and mapped macros that can be ridden in real time.

### 3.6 Detail and negative space

A dense sound can still be clear. Avoid automatically stacking too many delays, wide harmonies, and long reverbs at once. Provide per-voice/per-effect levels, filtering, ducking or dynamic-send options, and accessible gain staging.

---

## 4. Expanded IH1 FX catalogue and reference implementations

**Scope correction:** the sketch is a workflow/UI sketch. Its visible `DELAY`, `REVERB`, and `REVERSE` labels are not an exhaustive effect list. IH1 is intended to contain a large, useful catalogue. Claude must not constrain the architecture or the carousel to three effects, and must not interpret “start with high quality” as “ship only three forever.” Build the architecture for a large, expandable collection and implement it in planned, testable batches.

The catalogue below is a curated target of **37 meaningful effect types**. Some share DSP primitives, but each should remain a separately selectable FX where its musical behaviour is distinct. Sub-modes that are merely variations of the same engine (for example Hall/Plate/Spring within Reverb) can be selected inside that FX's settings rather than bloating the carousel with duplicates. Multiple committed instances of any type remain allowed.

Priority meanings:

- **A — identity/core:** strongly aligned with the product and/or foundational for a versatile creative FX instrument. These should be developed early.
- **B — important breadth:** expected catalogue coverage after the core engine is stable.
- **C — advanced/experimental:** include when implementation quality, CPU budget, and user testing justify it. These remain legitimate planned effects, not discarded ideas.

This is a catalogue and architecture target, not a claim that all 37 must be coded in one initial sprint. Do not silently remove items to save effort. Record implementation status, dependencies, DSP quality, and reasons for deferral in the project roadmap.

### 4.1 Effect catalogue

| # | FX carousel item | Priority | Distinct sonic purpose / useful modes | Notable references and what to study |
|---:|---|:---:|---|---|
| 1 | **Delay** | A | Clean digital echo, slapback, stereo/ping-pong, multi-tap, rhythmic patterns, feedback filtering, modulation, ducking | Soundtoys EchoBoy's many echo styles and 16-tap Rhythm mode; Lexicon 480L Twin Delays; Eventide H3000 delays. Study tap timing/level/pan, feedback tone and musical time changes. |
| 2 | **Tape Echo** | A | Dedicated tape-machine behaviour: multiple playback heads, tape bandwidth, saturation, wow/flutter, splice/dropout, spring-reverb option, self-oscillation | Roland RE-201 Space Echo / UAD emulation; Strymon Deco. Study how tape speed changes both echo spacing and pitch, and how feedback becomes an instrument. |
| 3 | **Reverb** | A | Room, ambience, chamber, hall, plate, spring, large/abstract spaces; early/late balance, decay contour, damping, diffusion, modulation, freeze | Lexicon 480L (Shape/Spread, Random, Ambience); EMT-style plate; spring units; Valhalla-style algorithmic spaces. Study density, modal behaviour, early reflections and decay contour—not merely the RT60 knob. |
| 4 | **Reverse** | A | Reverse-buffer playback, reverse delay, reverse grains, rhythmic capture/retrigger, crossfaded backwards phrases | Eventide H3000 Reverse Shift / Soundtoys Crystallizer; reverse examples in Lexicon 480L Effects bank. Keep this distinct from reverse reverb and explain exact capture/retrigger semantics. |
| 5 | **Harmony** | A | Multi-voice musical harmonizer, MIDI note mode, key/scale/chord mode, per-voice interval, level, pan, detune, vibrato, glide and formant controls | DigiTech Vocalist Workstation EX as the documented “Hide and Seek” reference; Eventide H3000 pitch algorithms. Four voices are a historical starting reference, not a hard architecture ceiling. |
| 6 | **Pitch Shift / Octaver** | A | Independent pitch transposition, octave/sub-octave, interval stacks, fine detune, pitch-shift delay, pitch feedback | Eventide H910/H3000, Soundtoys Little AlterBoy, classic octave processors. Study transition artefacts, latency, phase coherence, transients and formant handling. |
| 7 | **Formant Shift** | A | Move spectral-envelope/formant character independently of fundamental pitch; subtle vocal reshaping through extreme character changes | Little AlterBoy; Eventide H3000-era pitch/formant processing; modern spectral/formant shifters. Make clear that formant is not the same as pitch. |
| 8 | **Pitch Correction / Hard Tune** | B | Monophonic pitch detection and correction toward a key/scale or selected note; smooth or deliberately stepped/robotic response | Auto-Tune-style hard correction and Melodyne-style pitch editing are different workflows. IH1 is a real-time insert: prioritise stable pitch tracking, note transitions and correction-speed control. |
| 9 | **Vocoder** | B | True carrier/modulator effect: vocal or audio modulator envelope controls a harmonically rich or noise carrier through filter bands | Ableton Vocoder and classic rack vocoders. It is architecturally distinct from the DigiTech Vocalist EX “Vocoder” preset used for “Hide and Seek”; do not conflate them. |
| 10 | **Doubler / MicroPitch** | A | ADT-style thickening, detuned left/right voices, short delays, stereo spread, subtle or exaggerated doubling | Eventide H3000 MicroPitch; Eventide TriceraChorus; Abbey Road ADT concepts. Avoid obvious comb filtering and check mono compatibility. |
| 11 | **Chorus** | A | One or multiple short modulated delay voices create width, movement and pitch variation | Eventide TriceraChorus/BBD-style chorus; Lexicon 480L random-delay chorus; classic CE-1/Dimension-style approaches. Study independent LFO phases and multiple voices. |
| 12 | **Flanger** | B | Very short modulated delay plus feedback creates moving comb-filter notches and the characteristic jet/sweep sound | Eventide Instant Flanger; tape flanging; Strymon Deco flange region. Control through-zero options only if latency/phase routing can be handled correctly. |
| 13 | **Phaser** | B | Cascaded all-pass stages create moving notches; feedback/resonance changes emphasis | Eventide Instant Phaser; Soundtoys PhaseMistress. Study stage count, odd/even stage character, resonance and modulation. |
| 14 | **Vibrato** | B | Pitch-only modulation using a continuously varying delay; no dry/chorused anchor at full wet | Classic tape/analog vibrato and modulation section of DAFX. Keep distinct from chorus, which blends delayed voices with the source. |
| 15 | **Tremolo / Auto-Pan** | B | Amplitude modulation, rhythmic gating/pulsing, stereo level movement; LFO and envelope-triggered modes | Ableton modulation devices, Xfer LFO Tool. Study tempo-locked LFO shapes, swing, phase offsets and constant-power panning. |
| 16 | **Rotary Speaker** | B | Frequency-dependent rotating horn/drum simulation with amplitude modulation, Doppler pitch shift, crossover, stereo motion and acceleration | Leslie cabinet / rotary-speaker simulations; DAFX rotary-speaker chapter. It must sound more physical than a simple autopan. |
| 17 | **Filter / Wah** | A | Static and moving low/high/band-pass, resonant sweeps, envelope filter, wah and formant-like filter shapes | Moog ladder-inspired filters, wah pedals, Soundtoys FilterFreak. Study filter topology, resonance, drive, coefficient stability and modulation. |
| 18 | **EQ / Dynamic EQ** | A | Parametric bell/shelf/notch/high/low-pass shaping; optional band-triggered dynamic attenuation/boost | FabFilter Pro-Q family. Study bell/shelf filter design, Q/bandwidth, phase modes, per-band detector and attack/release; keep static EQ intuitive. |
| 19 | **Saturation / Distortion** | A | Gentle harmonic enhancement through overdrive, fuzz, clipping, asymmetric waveshaping, multiband distortion and feedback coloration | Soundtoys Decapitator (five analog-inspired styles), FabFilter Saturn 2 (multiband, many styles, oversampling), iZotope Trash (multiband/convolution). |
| 20 | **Tape / Console Colour** | B | Dedicated low-to-medium saturation, frequency response, crosstalk/soft compression, head bump or transformer-like tone; separate from destructive distortion | Ampex-style tape models, console/transformer emulations, Strymon Deco saturation. Prefer a distinct gentle-colour effect rather than burying every subtle model in the distortion menu. |
| 21 | **Bitcrusher / Sample-Rate Reducer** | B | Quantisation-depth reduction, sample-and-hold/downsampling, aliasing, digital clipping and optional jitter | Classic tracker/game-console degradation and modern Redux/Decimort-style processors. Preserve separate bit-depth and sample-rate controls; allow intentionally dirty aliasing as well as filtered modes. |
| 22 | **Compressor / Limiter** | B | Peak/RMS or hybrid level control, transparent levelling, punch, aggressive pumping, limiting and parallel blend | 1176 FET (very fast attack and program-dependent release; “all buttons” colour), LA-2A optical levelling. Study detector ballistics and gain-reduction behaviour, not only threshold/ratio. |
| 23 | **Gate / Expander / Ducker** | B | Noise reduction, transient gating, downward expansion, envelope ducking, tempo-shaped pumping | Classic studio gates and modern sidechain duckers. Study detector hysteresis, hold, attack/release, range, lookahead and external/internal key options. |
| 24 | **De-Esser** | B | Reduce sibilance through wideband or split-band attenuation driven by a high-frequency detector | FabFilter Pro-DS; classic vocal de-essing. Study band-limited detection, split/full-band modes, frequency range, threshold/range and oversampling under fast gain changes. |
| 25 | **Transient Shaper** | B | Independently reshape attack and sustain envelopes without requiring a conventional compressor ratio/threshold workflow | SPL Transient Designer-style processors. Use attack/sustain envelope extraction, frequency-dependent options only if the simpler broadband model is reliable. |
| 26 | **Granular Cloud** | A | Continuous rolling-buffer grain clouds; grain size, density, position, pitch, spray, direction, envelopes, freeze and diffusion | Mutable Instruments Clouds; Output Portal; Soundtoys Crystallizer. Study grain scheduling/windowing and the way overlapping grains create a texture rather than a conventional echo. |
| 27 | **Spectral Freeze / Blur** | A | Capture/hold spectral energy, smear bins over time/frequency, spectral delay, evolving frozen pads and dissolving textures | Spectral processors and phase-vocoder research. This is an original IH1 creative expansion, not a documented Heap technique. |
| 28 | **Stutter / Glitch / Beat Repeat** | A | Tempo-aware capture and retrigger, slice repeat, chance, gate, offset, reverse, pitch and rhythmic probability | Ableton Beat Repeat; Eventide H910/H3000 glitch/stutter lineage. Study deterministic clocking, buffer boundaries, quantised capture and anti-click crossfades. |
| 29 | **Looper / Buffer Hold** | C | Capture a longer phrase, repeat/overdub/hold, reverse playback, feedback and controlled buffer manipulation | Live looping and delay-buffer instruments. More stateful than Stutter: distinguish phrase recording/overdub from short retriggered slices. |
| 30 | **Ring Modulator** | B | Multiply signal by oscillator/carrier; create sum/difference sidebands and clangorous inharmonic timbres | Classic analog ring modulators and H3000-style modulation effects. Include sine/triangle/noise or external carrier only if input routing is approved. |
| 31 | **Frequency Shifter** | C | Shift every spectral component by a fixed number of Hz (not semitones), using quadrature/analytic-signal processing | Eventide-style frequency shifting. Unlike pitch shifting, intervals are not preserved; small shifts create moving/inharmonic beating. |
| 32 | **Resonator / Modal Body** | C | Excite tuned resonant modes, strings, tubes, plates or abstract resonant bodies; pitch/key tracking and damping | Physical-model/resonator effects and the H3000 string-modeler lineage. Use tuned delay/comb or modal-resonator banks; manage feedback stability. |
| 33 | **Convolution / Impulse Space** | C | Convolve with room, plate, spring, cabinet or creative IRs; allow user-loaded IR only with background loading and safe state management | Convolution reverb; iZotope Trash Convolve module. Use partitioned FFT convolution for long IRs and measure latency/CPU. |
| 34 | **Shimmer** | B | Reverb with pitch-shifted energy fed into or around the reverb tank; octave/interval voices and controllable regeneration | Strymon BigSky Shimmer; Eventide-style pitch/reverb combinations. It is more than placing a pitch shifter after a normal reverb: feedback topology shapes the result. |
| 35 | **Reverse Reverb / Gated / Nonlinear Space** | B | Reverse swell, gated abrupt tail, nonlinear/plate-like envelope; distinct reverb articulation presets/algorithms | AMS RMX16 NonLin/Reverse-style reverbs; Lexicon 480L Effects and Blackhole reverse gravity. Consider these selectable Reverb algorithms or a separate FX only if the UI needs direct access. |
| 36 | **Stereo Imager / Width** | B | Mid/side width, frequency-dependent width, balance, Haas/micro-delay, mono-compatible decorrelation | Waves S1-style width tools; M/S processing. Include correlation/mono checks; do not treat delay offsets as harmless widening. |
| 37 | **Exciter / Harmonic Enhancer** | C | Add controlled upper harmonics or psychoacoustic presence without full distortion | DAFX nonlinear/exciter chapter; high-frequency harmonic excitation. Keep level-matched and avoid harsh aliasing or excessive high-frequency noise. |

**Important distinctions to preserve**

- **Harmony ≠ vocoder.** A MIDI-directed pitch harmonizer creates transposed copies/voices. A conventional vocoder analyses the modulator's band envelopes and imposes them on a separate carrier.
- **Pitch shift ≠ frequency shift.** Pitch shift scales frequencies by musical intervals; frequency shift adds/subtracts a fixed Hz amount and destroys harmonic interval relationships.
- **Chorus ≠ flanger ≠ phaser ≠ vibrato.** They share modulation primitives but have different signal topologies and characteristic outputs.
- **Tape echo ≠ clean delay.** Tape echo models the time-varying playback mechanism, frequency response, saturation, and feedback behaviour—not just filtered repeats.
- **Reverse ≠ reverse reverb.** One plays captured material backwards; the other shapes or reverses a reverberant envelope.
- **Granular ≠ stutter ≠ looper.** Granular is many overlapping micro-events; stutter repeats short captured blocks on a grid; a looper captures a longer phrase and can overdub.
- **Dynamic EQ ≠ multiband compression.** A dynamic EQ changes selected filter bands; a multiband compressor splits the complete signal into crossover bands and applies dynamics to each band.

### 4.2 Famous implementations: what IH1 should learn from them

These are engineering and listening references, not instructions to copy their source code, branding, presets or proprietary algorithms.

1. **Eventide H910 / H3000:** the important innovation is the combination of pitch conversion, short delay and feedback, including pitch steps and expressive keyboard control. The H3000 expanded that into a programmable multi-effect system containing pitch shifting, stereo delays, reverbs, stutter, vocoder, string modelling, phasing and sampling. This is proof that a single effect environment can contain a broad, coherent palette rather than a handful of unrelated modules. [H910 history](https://www.eventideaudio.com/rackmount/h910-harmonizer/) · [H3000 overview](https://www.eventideaudio.com/rackmount/h3000/)
2. **Lexicon 480L:** study its separate Reverb, Random, Ambience, Effects and Twin Delay algorithms. Shape and Spread control the time envelope of the reverb build-up; random time-varying delay elements reduce static metallic modes; Ambience blends reflections into the direct sound; its Effects bank covers reverse, modulation, doubling, tremolo and chorus. This demonstrates why one shared algorithm with a few generic knobs is not enough for high-quality space effects. [Universal Audio's 480L manual](https://help.uaudio.com/hc/en-us/articles/33194625601044-Lexicon-480L-Digital-Reverb-and-Effects-Manual)
3. **Roland RE-201 Space Echo:** three playback heads make distinct rhythmic patterns; the playback motor's changing speed creates pitch movement; tape saturation compresses and colours the signal; tape friction/slippage creates subtle pitch fluctuations; its spring tank provides a separate spatial layer. Model these causal behaviours instead of adding random noise as a fake “analog” control. [Roland RE-201 manual](https://static.roland.com/assets/media/pdf/RE-20_OM.pdf) · [UAD manual](https://help.uaudio.com/hc/en-us/articles/33538271334164-Roland-RE-201-Space-Echo-Manual)
4. **Soundtoys EchoBoy:** its 30 echo styles and 16-tap rhythm editor show how one delay engine can support many usable musical patterns. Its style editor exposes saturation, tape wobble and diffusion—useful reminders that the feedback network and coloration are part of delay's identity. [EchoBoy](https://www.soundtoys.com/product/EchoBoy/)
5. **Eventide Crystallizer:** combines granular slicing, pitch shift and reverse echo. This is a useful model for an FX whose output becomes a new musical texture rather than merely repeating the source. [Crystallizer](https://www.soundtoys.com/product/crystallizer/)
6. **Eventide Blackhole and Strymon BigSky Shimmer:** these show two distinct approaches to huge/abstract reverb and pitch-shifted reverb feedback. Blackhole's Gravity spans forward and reverse-like response; BigSky's Shimmer uses two pitch voices and offers input-only, tank-regeneration or combined routing. [Blackhole guide](https://s3.amazonaws.com/com.eventide.downloads/Product%20Manuals/Blackhole%20User%20Guide.pdf) · [BigSky manual](https://www.strymon.net/manuals/BigSky_UserManual_RevD.pdf)
7. **Eventide TriceraChorus and Strymon Deco:** study multi-voice chorus with phase-offset LFOs and MicroPitch detuning, plus the transition of a tape double-tracker from flange through chorus to slap/echo. The key is to implement the voice structure and phase relationships—not just a generic LFO on one delay. [TriceraChorus](https://www.eventideaudio.com/plug-ins/TriceraChorus/) · [Deco manual](https://www.strymon.net/manuals/Deco_v2_UserManual_RevC.pdf)
8. **Soundtoys Decapitator / FabFilter Saturn 2 / iZotope Trash:** these cover three complementary approaches to distortion: a small number of carefully modelled analog circuit colours; a highly flexible multiband waveshaping environment with modulation and oversampling; and broad experimental distortion plus convolution/IR processing. IH1 should have a musically useful core drive stage and should only add multiband complexity when crossovers/latency/CPU are justified. [Decapitator](https://www.soundtoys.com/product/Decapitator/) · [Saturn 2](https://www.fabfilter.com/products/saturn-2-multiband-distortion-saturation-plug-in) · [Trash](https://www.izotope.com/products/trash)
9. **1176 and LA-2A compressors:** the 1176 demonstrates extremely fast FET-style attack and program-dependent release; the LA-2A demonstrates smoother optical, program-dependent levelling. These are different dynamic behaviours, not simply different GUI skins. [1176 manual](https://help.uaudio.com/hc/en-us/articles/34530260482324-1176-Classic-FET-Compressor-Manual) · [LA-2A manual](https://help.uaudio.com/hc/en-us/articles/19378009641748-LA-2A-Tube-Compressor-Manual)
10. **Mutable Instruments Clouds:** a strong model for a continuous live-input granular texture processor. It continuously records an input buffer and creates overlapping grains with adjustable source position, size, pitch, density and timing distribution; its diffuser/allpass/reverb stage can blur the result further. [Clouds manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/clouds/manual/)
11. **Ableton Beat Repeat and Vocoder:** Beat Repeat is a clear reference for tempo-synchronised capture/retrigger, interval, offset, chance and gate. Ableton's Vocoder documentation clearly describes a real carrier/modulator filterbank and the distinction from formant processing. [Live 12 effects reference](https://www.ableton.com/en/manual/live-audio-effect-reference/)
12. **FabFilter Pro-Q / Pro-DS / Pro-MB:** study accessible dynamic EQ controls, band-limited triggering, sibilance detection, and the trade-off between fast gain changes, oversampling and CPU. [Dynamic EQ](https://www.fabfilter.com/help/pro-q/using/dynamic-eq) · [Pro-DS oversampling](https://www.fabfilter.com/help/pro-ds/using/oversampling)

### 4.3 Catalogue implementation rules

- Keep one shared DSP primitive where it is technically appropriate, but do not flatten distinct effects into one generic “modulation” or “space” item. A shared fractional-delay component may serve Chorus, Flanger, Vibrato, Tape Echo and Delay; each still needs its own topology, parameter model and listening tests.
- Make effect identity visible and consistent across carousel, instance label, preset data, telemetry and tree palette mapping. Use stable string IDs (for example `tape_echo`, not a display name that can be localised).
- Give each effect a compact first-level control set and optional advanced controls. Tooltips explain the musical impact in plain language.
- Effects with internal modes should expose modes deliberately. For example, Reverb can offer Hall/Room/Plate/Spring/Ambience/Nonlinear/Shimmer if those algorithms are genuinely implemented; don't show unavailable or cosmetic modes.
- Every effect needs a unity/bypass path, wet/dry or equivalent blend where appropriate, input/output gain staging, safe defaults, sample-rate/block-size support, mono/stereo behaviour, state serialisation, CPU/memory budget and repeatable tests.
- For any effect with feedback, prove bounded behaviour at extremes, including DC control, denormal prevention, and a deliberate policy for self-oscillation/freeze.
- Do not make the carousel unusable as the library grows. Preserve the requested carousel and investigate unobtrusive category grouping/search/filtering only as a scalable navigation aid; do not replace the central concept without the user's approval.
- A type may share algorithms with another type, but it must have a clear sonic purpose and useful independent controls. Avoid adding near-duplicates merely to inflate the count.

---

## 5. DSP design notes

### 5.1 Harmony / pitch-shift engine

**Goal:** A high-quality, low-artifact multi-voice harmonizer capable of subtle doubling and overtly artificial, formant-shifted harmony stacks.

**Proposed signal model**

- One dry input/lead path.
- Up to four independent harmony voices.
- Each harmony voice receives a target pitch or interval and has its own gain, pan, formant character, detune, vibrato, onset/scoop, and wet contribution.
- Sum the processed voices with the dry path using a predictable mix law and headroom management.
- Accept MIDI note-on/note-off events for direct harmony control. Consider key/scale/chord modes as separate ways to derive target notes.

**Pitch-shifting approaches to evaluate**

1. **Phase-vocoder/STFT approach:** analyse overlapping FFT frames, move spectral components to target frequency positions, maintain phase continuity, and reconstruct with overlap-add. Peak/vertical phase locking generally improves coherence compared with independently evolving every bin.
2. **Time-domain approaches:** PSOLA/WSOLA-style methods can be attractive for monophonic vocal material and transient preservation, but require careful pitch-period handling and robust behaviour on non-vocal sources.
3. **Hybrid path:** choose or crossfade algorithms based on source/quality mode only if listening tests justify the complexity.

**Critical quality issues**

- A naive phase vocoder can smear consonants/transients and move formants with the shifted pitch, making voices sound unnaturally small/large or metallic.
- Include a formant-preservation option based on spectral-envelope estimation, or a separately controlled formant stage. Do not claim full formant independence unless it is actually measured and demonstrated.
- Include transient/unvoiced detection or another strategy to retain crisp consonants and attacks.
- Measure latency and report it correctly to the host. Provide a lower-latency/quality compromise only if necessary; never conceal latency.
- Handle note allocation deterministically when more than four MIDI notes arrive. Candidate policies: newest-note priority, lowest/highest-note priority, oldest-note priority, or round-robin. Expose the choice only if it is musically useful; the default should be predictable.
- Handle note-off, sustain pedal if supported, all-notes-off, pitch bend, and MIDI channel filtering without stuck notes.
- Do not hard-code only a four-note maximum if the architecture can support a safe future expansion, but four voices are the historically grounded reference point.

**Useful per-voice controls**

- Target: MIDI note, semitone interval, or scale-derived interval.
- Level and pan.
- Formant / vocal character.
- Fine detune in cents, optionally small note-to-note randomisation.
- Vibrato depth, rate, waveform, onset delay, and amount of variation.
- Scoop/glide amount and time; use musically sensible defaults and keep the effect subtle unless pushed.
- Optional per-voice filtering and reverb/delay send.

**Sources**

- [CLEESE paper (PLOS ONE, 2019)](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0205943) — describes the STFT phase vocoder, phase locking, formant shifts, and transient smearing.
- [DigiTech EX manual](https://device.report/m/7bc0be4fd8fd6603bda953cf934829cd9faa6421458037301bb3304750172c53) — historical feature vocabulary and control behaviour.
- [MusicRadar 2025](https://www.musicradar.com/artists/imogen-heap-hide-and-seek) — verified connection between Heap's four-voice MIDI harmonizer and formant-shifted vocal layers.

### 5.2 Delay engine

**Goal:** Delay as a spatial and rhythmic instrument, capable of both clean repeats and evolving, filtered textures.

**Useful controls**

- Time: milliseconds or tempo divisions, with optional independent left/right times.
- Tap count and per-tap level/pan/time offset.
- Feedback amount and feedback tone.
- Low-pass/high-pass filtering in the feedback loop.
- Modulation depth/rate for gentle movement.
- Diffusion or smear in the feedback path as an optional creative control.
- Wet/dry balance, output level, stereo width.
- Ducking or transient-sensitive send as optional clarity tools.
- Freeze/hold only with an explicit, stable feedback design and overload protection.

**DSP details**

- Use a preallocated circular delay buffer; do not allocate or resize inside the audio callback.
- Fractional delay interpolation is needed for smooth delay-time changes and modulation. JUCE's DelayLine supports linear, 3rd-order Lagrange, and Thiran options; its documentation notes that smoothing delay-time changes can avoid artifacts.
- Lagrange interpolation is a good candidate for a modulated delay when more frequency-response accuracy is needed than linear interpolation provides; benchmark it at realistic voice counts and sample rates.
- Smooth parameter changes. A sudden time jump can create a pitch glide/click; choose whether the user wants glide, crossfade, or a controlled tape-like transition.
- Keep feedback stable; apply filtering in the feedback path and guard against runaway DC/high-frequency accumulation.
- Test mono input, stereo input, mono fold-down, and extreme feedback values.

**Reference**

- [JUCE DelayLine documentation](https://docs.juce.com/master/classjuce_1_1dsp_1_1DelayLine.html) and [source documentation](https://github.com/juce-framework/JUCE/blob/master/modules/juce_dsp/processors/juce_DelayLine.h).

### 5.3 Reverb engine

**Goal:** Move from believable acoustic space to very long, luminous, evolving tails without turning every signal into a blanket of dense noise.

**Useful controls**

- Pre-delay.
- Early-reflection level/shape and late-field level.
- Decay/RT60.
- Low-frequency and high-frequency decay/damping.
- HF rolloff / tone.
- Diffusion/density.
- Stereo width and modulation.
- Wet/dry and output level.
- Freeze/hold as an optional advanced control.
- Optional ducking or dynamic return control.

**DSP approaches**

- A simple Freeverb-style comb/allpass design is a useful baseline for a prototype, but do not stop there if IH1 is intended to be a premium, distinctive processor.
- Evaluate a **Feedback Delay Network (FDN)** for the late reverberation field. FDNs use delay lines coupled through a feedback matrix; a suitably energy-preserving/unitary feedback matrix supports stable, controllable decay behaviour and dense late reverberation.
- Use a distinct early-reflection stage or convolution/reflection pattern to preserve the sense of an identifiable room before the late field blooms.
- Avoid audible metallic modes, ringing, unstable feedback, and a static “same reverb on every source” impression.
- Provide a safe parameter smoothing/transition strategy. JUCE's basic `Reverb::setParameters()` documentation explicitly warns that changing settings concurrently with processing may create artifacts; do not update internal DSP state unsafely from the UI thread.

**Source**

- [Jot & Chaigne, “Digital Delay Networks for Designing Artificial Reverberators” (AES Convention, 1991)](https://aes.org/publications/elibrary/elibrary-page/?id=5663) — foundational work on delay-network reverberator decay control and frequency-response compensation.
- [DAFx 2026 tutorial — Feedback Delay Networks](https://dafx26.mit.edu/tutorials/) — describes FDNs as a computationally efficient artificial-reverb framework and discusses stability, modal density, coloration, and optimization.
- [JUCE Reverb parameters](https://docs.juce.com/master/structjuce_1_1Reverb_1_1Parameters.html) — baseline concepts: room size, damping, wet/dry, width, freeze mode.

### 5.4 Reverse engine

**Goal:** Reverse material in a way that can be musical, rhythmic, or atmospheric, not merely a destructive global playback reversal.

**Proposed implementation options**

- A short rolling buffer with reverse-read windows.
- Tempo-aware capture windows (e.g., beat, bar, dotted/eighth divisions) and free-time capture.
- Windowing/crossfades at the edges to prevent clicks.
- Overlap-add or overlapping reverse grains to keep the stream continuous.
- Optional feedback/echo into the reverse buffer, with strict stability and latency rules.
- Wet/dry, buffer length, trigger/retrigger mode, pitch, stereo offset, and filter controls.

**Clarification required:** “Reverse” can mean reverse delay, reverse reverb, reverse-buffer playback, or a granular reverse texture. The user's carousel label alone does not specify which. Ask which flavour is wanted before building a large feature set. A first prototype can implement one clearly defined reverse mode and expand later.

### 5.5 Granular / spectral effects (experimental proposals)

These effects are plausible extensions for IH1's ethereal, transforming identity, but the research above does **not** establish them as Heap's specific tools. They must be labelled as original IH1 creative directions.

**Granular concept:** split the input into many short, windowed grains; control grain duration, density, read position, pitch, direction, and overlap. Randomness should be bounded and shaped, with controls for jitter and stereo decorrelation. Use window functions and overlap-add to avoid clicks and amplitude holes. Keep grain memory and processing costs bounded.

**Spectral freeze/blur concept:** capture an STFT representation, hold or smear spectral content, and use carefully managed phase continuity/re-synthesis. This can create a sustained spectral cloud but needs serious testing for transients, phase incoherence, and metallic artifacts.

Do not add these modules to V1 merely because they look impressive. Use KADAL to find any existing research, prototypes, benchmarks, and prior algorithms before starting a fresh implementation.

---

### 5.6 Modulated time effects: Chorus, Flanger, Vibrato and MicroPitch

These should be separate carousel effects even though they share delay-line primitives.

**Common DSP building block**

- A modulated delay reads a circular buffer at a time-varying fractional sample position. The delay trajectory is often sinusoidal, but triangle, random/smoothed, multi-phase or user-drawn trajectories can be useful.
- Use fractional-delay interpolation rather than integer-only read positions. Smooth rate/depth/offset changes and avoid discontinuities when switching modes.
- Keep modulation phase state continuous when parameters change; resetting the LFO at random times produces audible jumps and makes the effect feel cheap.

**Chorus**

- Blend the dry source with one or more delayed voices whose delay times are modulated by low-frequency oscillators. Multiple voices should have independent base delay, depth, rate, phase and pan.
- A three-voice design can give left/centre/right voices different LFO phase relationships. Eventide TriceraChorus describes the left/right modulation as offset by ±120 degrees relative to centre, producing a much richer field than duplicating one identical voice.
- Controls: voices, delay/base offset, depth, rate, stereo phase/spread, voice level, tone, wet/dry and optional MicroPitch offset.
- Test comb filtering and mono compatibility at low rate/depth and when the dry path is mixed strongly.

**Flanger**

- Use a much shorter modulated delay than chorus and mix it with the dry signal. The summation creates moving comb-filter notches. Feedback increases resonance and can turn the sweep into a pronounced jet-like sound.
- Include positive/negative feedback only if the topology is stable and tested. A through-zero flanger requires a dry-path timing strategy and often intentional latency; do not claim zero-latency through-zero behaviour unless measured.
- Controls: manual delay/centre, depth, rate, feedback, polarity, stereo phase, envelope/LFO mode, wet/dry.

**Phaser**

- Use a cascade of all-pass stages whose phase response moves with control frequency. Mix the filtered signal with the dry input to create cancellation notches; feedback changes resonance/peak strength.
- Stage count, odd/even topology, feedback and the relationship between resonant peaks/notches strongly affect character. The PhaseMistress manual documents stage and resonance structures that go beyond a one-control “swoosh.”
- Controls: stage count, sweep centre/range, rate, resonance/feedback, stereo phase, LFO/envelope modes and mix.

**Vibrato**

- Use a modulated fractional delay with essentially 100% wet pitch movement, not a stable dry anchor plus moving copy. Make pitch excursion understandable in cents and keep a fine-depth mode for subtle movement.

**Doubler / MicroPitch / ADT**

- Create two or more parallel delayed voices with small, slightly different pitch offsets and time offsets. Avoid identical mirrored voices, which create static comb filtering rather than convincing double tracking.
- Use slow independent drift or bounded random modulation rather than an uncontrolled random-number generator. Offer mono-safe/mono-check modes and a width control.

**References:** [DAFx modulation effects](https://www.dafx.de/DAFX_Book_Page_2nd_edition/chapter2.html) · [DAFx modulators and demodulators](https://www.dafx.de/DAFX_Book_Page/chapter4.html) · [Eventide TriceraChorus](https://www.eventideaudio.com/plug-ins/TriceraChorus/) · [Soundtoys PhaseMistress manual](https://store.soundtoys.com/wp-content/uploads/PhaseMistress-Manual.pdf) · [Strymon Deco manual](https://www.strymon.net/manuals/Deco_v2_UserManual_RevC.pdf)

### 5.7 Tremolo, Auto-Pan, Rotary and Doppler

**Tremolo / Auto-Pan**

- Tremolo multiplies the signal by a time-varying gain curve. Offer sine, triangle, square, ramp, sample-and-hold (smoothed), and custom curve shapes; include tempo sync, swing, phase, depth and offset.
- Auto-pan uses related time-varying gain coefficients for left/right channels. Use a constant-power pan law to avoid a perceived loudness dip at the centre, and test mono fold-down.
- An envelope mode can make movement respond to input level; use an envelope follower with adjustable attack/release, not raw sample amplitude.
- A sequencer/pattern mode can provide rhythmic chopping. It should share a clock/sync core with Stutter rather than creating a second inconsistent tempo engine.

**Rotary speaker**

- A convincing rotary/Leslie effect requires frequency splitting into low-frequency drum and high-frequency horn paths; each has separate modulation rate/depth, amplitude modulation, phase and stereo placement.
- Include inertia/acceleration so a speed switch transitions over time instead of instantly jumping rate. Add Doppler-like frequency modulation as the rotating horn/drum moves relative to the listener.
- Keep crossover/filter slopes stable and ensure the horn/drum paths recombine without excessive phase cancellation.

**Doppler**

- Model a moving source by continuously changing propagation delay, not by applying pitch shift with a static ratio. The derivative of the delay trajectory controls the instantaneous frequency shift; gain/pan can optionally follow distance/trajectory.
- This should be a separate advanced spatial effect, not a synonym for Auto-Pan.

**References:** [DAFx modulators and demodulators](https://www.dafx.de/DAFX_Book_Page/chapter4.html) includes tremolo, rotary-speaker simulation, ring modulation, frequency modulation and envelope-following applications. [Ableton Live 12 effects reference](https://www.ableton.com/en/manual/live-audio-effect-reference/) provides practical modulation-effect controls. [Xfer LFO Tool](https://xferrecords.com/products/lfo-tool/) is a reference for tempo-synchronised editable control curves.

### 5.8 Pitch shifting, harmonizer, formant shifter, pitch correction and vocoder

The existing Section 5.1 Harmony engine remains the foundation. Extend it as follows:

**Pitch shift / octave**

- Use an algorithm designed for the intended material and latency budget. A phase vocoder gives flexible polyphonic frequency-domain shifting but can smear transients; PSOLA/WSOLA-like time-domain methods can preserve monophonic vocal attacks better but are less general. Test multiple approaches using vocal, piano, guitar and percussive material.
- Include transient handling, stereo phase consistency, formant-preserving vs formant-following options, and a quality/latency mode only if the trade-off is real and measured.
- Pitch interpolation and crossfades must be smooth during glides and interval changes.

**Formant shifter**

- Estimate or model the spectral envelope separately from the fine harmonic structure (for example using cepstral/envelope methods). Apply a controlled spectral-envelope warp while preserving or separately controlling the fundamental/harmonic locations.
- Do not expose a “gender” label as the only control. Use clear terms such as Formant, Character or Vocal Size, with an accessible tooltip; the control is a spectral effect and does not determine a person's identity.

**Pitch correction / hard tune**

- Detect fundamental frequency from voiced segments, reject unvoiced/noisy frames, track notes with temporal continuity, and map the estimate to a selected scale/allowed-note set. Avoid rapid note flicker near semitone boundaries through hysteresis and stable note tracking.
- Correction speed determines how quickly the estimated pitch moves to the target. Smooth settings can be transparent; very fast settings create the intentionally stepped robotic effect.
- Preserve formants when appropriate and allow deliberate scale/key selection. A real-time plugin cannot duplicate Melodyne's full offline note-editing workflow simply by adding a Retune knob.

**True vocoder**

- The modulator (e.g. speech) passes through a bank of band-pass filters. Each band's envelope is detected/smoothed and controls the gain of a corresponding carrier-filter band. Sum the modulated carrier bands.
- Provide internal oscillator/noise/saw/pulse carriers and, only if the plug-in architecture and host routing support it, an external sidechain/carrier input. A bright, harmonically rich carrier improves articulation; unvoiced consonants often need a noise path or transient enhancement.
- Expose band count/spacing, attack/release, sibilance/unvoiced handling, carrier tone, formant shift and mix. Use matched filterbank design and gain compensation to limit spectral holes and excessive peaks.
- Never describe a classic vocoder as the actual mechanism used for “Hide and Seek”: the source research attributes the sound to MIDI-directed pitch harmonies with formant manipulation.

**References:** [DigiTech EX manual](https://device.report/m/7bc0be4fd8fd6603bda953cf934829cd9faa6421458037301bb3304750172c53) · [Eventide H910](https://www.eventideaudio.com/rackmount/h910-harmonizer/) · [Soundtoys Little AlterBoy](https://www.soundtoys.com/product/little-alterboy/) · [Ableton Vocoder explanation](https://www.easymanua.ls/ableton/live/manual?p=338) · [CLEESE phase-vocoder paper](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0205943)

### 5.9 Filter, wah, EQ and dynamic EQ

**Filter / wah**

- Support low-pass, high-pass, band-pass, notch, peak and all-pass where musically useful. Resonance/Q must remain stable across the available frequency range and sample rates.
- State-variable filters are a useful general-purpose starting point because several responses can be obtained from a shared topology. Ladder-style filters are useful when the nonlinear resonance/drive behaviour is part of the identity; do not label a simple biquad as “Moog-style analogue modelling.”
- Smooth cutoff/resonance and modulate coefficients at audio/control rate safely. Include LFO, envelope follower and manual/automation modes.
- A wah is a resonant band-pass/peak response swept through frequency. An envelope filter uses detected input level to control cutoff. These should be modes of Filter/Wah only if the controls and signal behaviour remain clear.

**Static EQ**

- Use stable biquad or higher-order IIR filters for efficient minimum-phase parametric bands; optionally evaluate FIR/linear-phase for mastering-style use only when latency and pre-ringing are acceptable.
- Provide frequency, gain and Q/bandwidth; shelves and high/low-pass slope controls; band solo/audition and output trim. Avoid parameter layouts that require a musician to understand filter coefficients.

**Dynamic EQ**

- A selected band has a detector path (often band-limited to that band's frequency region) and a gain-control path. Threshold, range, attack, release and knee define how the band's gain changes with the signal.
- Negative dynamic range attenuates only when the band becomes too loud; positive range can expand/emphasise. This is more surgical than splitting the entire signal into crossover bands.
- A spectrally selective de-essing mode can be built on the same detector core, but expose it as a dedicated De-Esser FX if users should have a simple sibilance-specific entry.

**References:** [DAFx filters and EQ](https://www.dafx.de/DAFX_Book_Page/chapter2.html) · [FabFilter Pro-Q dynamic EQ](https://www.fabfilter.com/help/pro-q/using/dynamic-eq) · [Moog ladder model paper](https://raw.githubusercontent.com/ddiakopoulos/MoogLadders/master/research/DAngeloValimaki.pdf)

### 5.10 Saturation, distortion, tape colour, bitcrushing and amp/cabinet

**Saturation / distortion**

- Begin with a properly gain-staged waveshaping core and several genuinely distinct curves/topologies: symmetric soft clipping, asymmetric clipping, tanh-like smooth saturation, foldback, hard clip, diode/transfer approximations and a small set of measured/modelled analog behaviours.
- Add pre/post tone shaping because the spectrum entering a nonlinearity strongly affects the resulting harmonics. Include input drive, bias/asymmetry, tone, output compensation, wet/dry and oversampling/quality mode.
- Multiband drive is valuable but requires phase-aware crossover filters, independent band gain and correct recombination. Do not simply distort separately split bands and ignore crossover summation.
- Nonlinear processing creates harmonics above Nyquist that fold back as aliasing. Use oversampling plus appropriate anti-alias filtering; evaluate ADAA or other antialiasing strategies for waveshapers. Higher factors cost CPU and add filter latency. Make the quality mode measurable and report latency.
- A subtle tape/console-colour effect should model frequency response, soft saturation and possibly dynamic compression/hysteresis characteristics; do not reduce it to random noise or an EQ curve.

**Bitcrusher / sample-rate reducer**

- Bit-depth reduction quantises amplitude; sample-rate reduction holds or resamples fewer points. These are different controls and sound different.
- Include optional pre/post filtering, jitter, clipping, dither/no-dither and wet/dry. Dither is useful for transparent low-level quantisation; intentionally lo-fi settings may disable it to preserve rough correlated distortion.
- Let aliasing be an intentional creative component, but provide filtered/oversampled modes for less destructive settings.

**Amp and cabinet**

- An amp mode can combine input gain, nonlinear preamp stages, tone stack, power-stage compression/soft clipping and output stage. Cabinet/speaker response can be represented with IR convolution or compact speaker models.
- If the amp/cab effect is not central to IH1's ethereal identity, make it a later catalogue item rather than diverting resources from pitch/time/spectral effects.

**References:** [DAFx nonlinear processing](https://www.dafx.de/DAFX_Book_Page/chapter5.html) · [Decapitator](https://www.soundtoys.com/product/Decapitator/) · [Saturn 2](https://www.fabfilter.com/help/saturn) · [iZotope Trash](https://www.izotope.com/products/trash) · [Moog ladder circuit model](https://raw.githubusercontent.com/ddiakopoulos/MoogLadders/master/research/DAngeloValimaki.pdf)

### 5.11 Dynamics: Compressor, Limiter, Gate, Expander, Ducker, De-Esser and Transient Shaper

**Shared detector core**

- Create a reusable detector supporting peak, RMS/averaged power, or blended detection; rectification/smoothing; attack, release, hold, hysteresis and optional lookahead.
- Keep detector and gain-smoothing state separate from the audio path. Make detector time constants sample-rate aware and avoid denormals.
- Support internal sidechain/key filtering and optional external sidechain only after VST3 bus configuration is designed and tested.

**Compressor / Limiter**

- Implement threshold, ratio, knee, attack, release, makeup/output gain, mix, detector mode and gain-reduction metering. Use log-domain/dB gain computation and safe control smoothing.
- Decide between feed-forward and feedback topologies based on the intended behaviour. Program-dependent release can sound more organic than a single fixed release time; a separate optical-style mode can have slower and signal-dependent behaviour.
- An 1176-inspired mode should capture fast FET-like response and deliberate aggression; an LA-2A-inspired mode should feel slower/smoother and optically program-dependent. Do not ship these labels unless the response characteristics are intentionally modelled and tested.
- Limiter needs oversampled/lookahead peak control if claiming true-peak protection. A simple hard clipper is not a transparent true-peak limiter.

**Gate / Expander / Ducker**

- A gate attenuates below a threshold; an expander reduces level progressively; a ducker reduces a target signal when the detector/key signal rises. Each needs different curve semantics.
- Add range/floor, attack, hold, release, hysteresis and sidechain filter. Hysteresis prevents chatter around threshold; hold prevents rapidly opening/closing on short gaps.
- Tempo-shaped ducking can be offered as an envelope/LFO mode, but should not be conflated with signal-triggered compression.

**De-Esser**

- Detect energy in a configurable sibilance band, compare against threshold, then attenuate either that band (split-band) or the whole signal (wideband). Split-band is more transparent when tuned carefully; wideband can sound more natural on some vocals.
- Fast gain movement can alias or distort; oversampling is useful in demanding settings and adds CPU/latency. Provide listen-to-detection/audition and reduction metering.

**Transient Shaper**

- Extract an envelope with fast attack and slower sustain smoothing; derive attack and sustain components and apply separate gain scalars. Provide soft limits to prevent extreme settings from creating clicks or overshoot.
- Start broadband. Multiband transients are a separate complexity/quality step.

**References:** [DAFx nonlinear/dynamics chapter](https://www.dafx.de/DAFX_Book_Page/chapter5.html) · [UAD 1176 manual](https://help.uaudio.com/hc/en-us/articles/34530260482324-1176-Classic-FET-Compressor-Manual) · [UAD LA-2A manual](https://help.uaudio.com/hc/en-us/articles/19378009641748-LA-2A-Tube-Compressor-Manual) · [FabFilter Pro-DS oversampling](https://www.fabfilter.com/help/pro-ds/using/oversampling) · [FabFilter Pro-MB oversampling](https://www.fabfilter.com/help/pro-mb/using/oversampling)

### 5.12 Granular Cloud and granular reverse

- Continuously write input into a bounded rolling buffer. Grain voices read from positions in that buffer with independent start offsets, lengths, pitch/rate, direction and amplitude envelopes.
- Core controls: grain size, density/overlap, position, position jitter/spray, pitch, pitch scatter, reverse probability, stereo spread, envelope shape, freeze and wet/dry.
- Use Hann or other suitable windows and overlap-add/crossfade so grain boundaries do not click or create amplitude holes. Ensure gain normalisation is stable as density changes.
- Randomness should be bounded and optionally seeded. An FX committed into the tree must reproduce its own settings; if its random texture is important to the saved sound, save its seed/state policy too. Do not allow nondeterministic randomness to make presets unpredictably change every load unless explicitly desired.
- Bound memory and active grain count. No dynamic allocation per grain in the audio callback. Define behaviour when the host changes sample rate or block size.
- Freeze stops writing to the rolling buffer but continues grain playback. Distinguish this from a looper, which records a longer musical phrase and may overdub.
- A diffusion/allpass stage can turn distinct grains into a soft cloud. Mutable Instruments Clouds is a useful public reference: it highlights the relationship between grain size, density, position, pitch, envelope and buffer freeze.

**References:** [Mutable Instruments Clouds manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/clouds/manual/) · [Clouds original design notes](https://pichenettes.github.io/mutable-instruments-documentation/modules/clouds/original_blurb/) · [Soundtoys Crystallizer](https://www.soundtoys.com/product/crystallizer/)

### 5.13 Spectral Freeze, Blur and spectral delay

- Use STFT analysis/synthesis with carefully selected window size, hop size and overlap. Larger windows improve frequency resolution but smear transients and increase latency; smaller windows improve time resolution but blur low-frequency detail.
- Freeze can hold a magnitude spectrum while phase continues, is propagated, or is re-synthesised. These choices sound different; test for metallic ringing and phase discontinuity rather than assuming one method is universally best.
- Blur can smooth magnitude bins across frequency and/or time, diffuse phase, or mix neighbouring frames. Expose separate time-smear and frequency-smear concepts only if users can hear and understand their difference.
- Spectral delay can delay selected frequency bins/bands by different times, creating frequency-dependent echoes. This is computationally expensive; start with a limited number of bands or a low-resolution spectral representation.
- Preserve stereo coherence. Independent random phase per channel can destroy the centre image; consider linked analysis with controlled decorrelation.
- Preallocate FFT plans, windows, buffers and frame queues. Report the latency caused by the analysis window/hop, and test plugin bypass/transport transitions.

This family is an original IH1 creative expansion, not attributed to Heap. It belongs in the catalogue because it supports evolving, ethereal material and can interact with Reverse, Reverb, Granular and Delay in distinctive ways.

**Reference foundations:** [CLEESE phase-vocoder study](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0205943) discusses phase-vocoder behaviour and transient/formant issues; [DAFx filters and convolution](https://www.dafx.de/DAFX_Book_Page/chapter2.html) provides the underlying time/frequency filtering concepts.

### 5.14 Stutter, glitch, beat repeat and looper

**Stutter / Beat Repeat**

- Capture a short region in a circular buffer. Trigger repeats according to host tempo/transport or free-running time. Provide interval, offset, capture length, repeat count/gate, chance, pitch, reverse, decay and filter/feedback.
- Quantised capture must use the host's musical clock and account for tempo changes, transport start/stop and variable block sizes. Avoid assuming a fixed BPM or audio block maps to a beat.
- Crossfade the capture boundary and repeated window. Use a window suited to the slice length; too-long fades smear transients, too-short fades click.
- Keep a deterministic mode for repeatable user presets. Random chance/slice selection should optionally use a saved seed.

**Looper / Buffer Hold**

- Treat as a separate long-buffer record/play/overdub state machine. Define record start, loop length, overdub feedback, erase/replace, reverse, speed and stop/resume semantics.
- Looper controls are stateful and must be integrated carefully with the commit workflow: committed settings are immutable, but the incoming audio capture state is runtime state. Decide whether preset saving includes captured audio (large/unwieldy) or only looper parameters; default recommendation is not to embed recorded audio unless explicitly requested.

**Reference:** [Ableton Live 12 Beat Repeat](https://www.ableton.com/en/manual/live-audio-effect-reference/) documents tempo interval, offset, chance and gate. [Eventide H910 history](https://www.eventideaudio.com/timeline/) documents the early digital pitch/time splice artefact that became an intentional musical glitch.

### 5.15 Ring modulation and frequency shifting

- Ring modulation multiplies the input by a carrier oscillator. For a sinusoidal carrier, spectral components produce sum and difference sidebands; there is no preserved dry fundamental unless the dry signal is mixed back in. Include carrier frequency, waveform, mix and optional modulation.
- A frequency shifter moves every frequency by a fixed number of hertz rather than multiplying pitch by a ratio. A single-sideband implementation typically uses quadrature paths and a Hilbert transform/analytic signal; phase error and imperfect quadrature create unwanted sideband leakage.
- Use a stable quadrature oscillator / Hilbert filter design across the audio band; evaluate bandwidth and low-frequency behaviour. Do not implement frequency shifting by simply using a pitch shifter with tiny semitone changes.

**Reference:** [DAFx Modulators and Demodulators](https://www.dafx.de/DAFX_Book_Page/chapter4.html) covers ring, amplitude, frequency and phase modulation and their audio-effect applications.

### 5.16 Stereo imaging and spatial movement

- Provide a simple width/balance stage and optional mid/side processing. M/S conversion is a matrix transform, not a free stereo-widening trick; large side boosts can create mono cancellation or unstable low frequencies.
- Haas/micro-delay widening can produce strong width but also comb filtering on mono fold-down. Provide mono audition/correlation feedback or a safer decorrelation mode.
- Keep low-frequency stereo width controlled when the user chooses a broad setting. Offer frequency-dependent width only after crossovers and phase response are tested.
- Panning should use a constant-power law or another documented law; do not create loudness dips when a source moves through centre.
- Link stereo pitch-shift/granular/spectral channels when appropriate to retain a stable centre image; expose random stereo spread as a deliberate creative feature.

**Reference:** [DAFx Spatial Effects](https://www.dafx.de/DAFX_Book_Page/chapter6.html) covers pan, precedence, distance, Doppler, trajectories, reverberation, and apparent source width.

### 5.17 Convolution / impulse-response FX

- Convolution applies an impulse response to an input. For small IRs, direct convolution may be viable; for long IRs, partitioned FFT convolution reduces CPU cost by transforming blocks/IR partitions and multiplying in the frequency domain.
- Uniform partitioning is simpler; non-uniform partitioning can process an early short partition with low latency and later long partitions more efficiently. Choose based on latency/CPU target and test in a plugin host.
- Load/decode/resample IRs off the audio thread. Build/precompute the convolution engine before handing it to the real-time processor; never read files or allocate FFT plans in the callback.
- Presets may reference a bundled IR ID/path but must handle missing files safely. Check redistribution licences for every supplied IR. User-loaded IR paths may not travel with a project to another machine; provide missing-IR warnings and a clear policy.
- Creative IRs can act as resonant filters or unusual object transforms, as shown by iZotope Trash's convolution module. Convolution reverb is not automatically “more realistic”; its output depends on the measured/constructed IR, stereo capture and truncation.

**Reference:** [Epic Games' Convolution Reverb technical explanation](https://dev.epicgames.com/documentation/unreal-engine/convolution-reverb-in-unreal-engine) explains time-domain convolution, why long IRs are expensive, and why FFT-domain multiplication is practical in real time. [iZotope Trash](https://www.izotope.com/products/trash) is a creative product reference.

### 5.18 Resonator / modal effects

- A resonator emphasises or generates energy around tuned modes. Options include comb/allpass feedback structures, parallel band-pass resonators, tuned delay lines or modal synthesis with controlled decay per mode.
- Parameters: tuning/root/key tracking, mode count/spacing, inharmonicity, damping/decay, excitation position/brightness, feedback, stereo spread and wet/dry.
- If the effect is designed to self-oscillate, include explicit headroom/feedback limiting and a stable parameter range. Tuning should remain accurate across sample rates.
- Distinguish a static resonant filter from a resonator that rings after the input transient; the latter has an energy/storage model and can create pitched tails.

**Reference:** Eventide H3000's published effect list includes a string modeler; [DAFx filters](https://www.dafx.de/DAFX_Book_Page/chapter2.html) covers resonant filters and delay structures.

### 5.19 Shimmer and special reverb topologies

- Shimmer generally pitch-shifts the signal entering or circulating within a reverb network. The routing is the defining feature: input-only pitch-shift, regenerated-tank pitch-shift, or both can produce distinct results.
- Use pitch-shift voices with controllable interval, level and filtering. Low-frequency damping before/after pitch shift may prevent muddy octave-down accumulation.
- Reverse reverb can be built by reversing an impulse response or by time-reversing/reshaping a captured reverb envelope; make the exact behaviour explicit.
- Gated/nonlinear reverb uses a deliberately shaped decay envelope (for example abrupt truncation or non-natural build/decay) rather than a standard exponential decay. It should be selectable as an algorithm or clearly separated FX type if direct carousel access improves usability.
- Avoid stacking pitch shift in an uncontrolled feedback loop. Cap gain and test worst-case intervals/tank feedback.

**References:** [Strymon BigSky Shimmer manual](https://www.strymon.net/manuals/BigSky_UserManual_RevD.pdf) documents two pitch voices and input/tank regeneration modes; [Eventide Blackhole guide](https://s3.amazonaws.com/com.eventide.downloads/Product%20Manuals/Blackhole%20User%20Guide.pdf) describes Gravity's forward and inverse/reverse-like range; [Lexicon 480L manual](https://help.uaudio.com/hc/en-us/articles/33194625601044-Lexicon-480L-Digital-Reverb-and-Effects-Manual) covers multiple reverb and effects algorithms.

### 5.20 Shared modulation, envelope and clock infrastructure

A large FX catalogue should not duplicate control logic in every module.

- Build reusable LFO shapes, tempo-synchronised phase accumulators, envelope followers, transient detectors and random-but-repeatable modulation sources.
- A modulation source may target a control within its committed FX snapshot only through the FX's own committed modulation settings. The user must not be able to edit the committed FX after commit; the modulation continues autonomously using the committed values.
- Provide a shared host transport/tempo service and handle tempo changes, host discontinuities, loop boundaries, offline rendering and transport stopped states.
- Smooth modulation depth/rate/phase changes. Random modulation should use bounded distributions and saved seeds when repeatable state is required.
- Consider a compact per-FX modulation drawer rather than a separate “Modulation” carousel item unless a true standalone modulation processor is justified.
- The modulation system must never change the immutable parameter snapshot; it evolves the sound according to its saved modulation rules.

**Reference:** [Xfer LFO Tool](https://xferrecords.com/products/lfo-tool/) demonstrates tempo-synchronised custom curves and MIDI/automation-based modulation; [Eventide H3000 Factory](https://www.eventideaudio.com/plug-ins/h3000-factory/) demonstrates combining pitch shifters, delays, filters, VCAs and modulation sources as a modular multi-FX environment.

### 5.21 Effect-by-effect acceptance template

Before an FX is considered implemented, the developer must complete a short technical card:

1. **Sonic contract:** one sentence saying what the effect does that neighbouring FX do not.
2. **Reference listening:** at least two credible reference examples/products and notes on what is being studied.
3. **Algorithm:** signal-flow diagram, DSP method, key equations/parameters and expected latency.
4. **Parameter design:** essential musician-facing controls, default values, ranges and plain-language tooltips.
5. **Real-time safety:** allocation, memory, feedback, thread, sample-rate, block-size and host-tempo behaviour.
6. **Tests:** deterministic unit tests, impulse/step/sine tests where relevant, stereo/mono tests, extreme-parameter tests, and listening tests on multiple sources.
7. **Performance:** CPU and memory at supported sample rates, realistic block sizes and duplicate chain lengths.
8. **State:** full committed parameter snapshot, versioned serialization, deterministic random seed policy and preset round-trip test.
9. **Quality status:** prototype / technically functional / listening-approved / release-ready. Compilation alone is not a quality sign-off.

No FX should be added merely to make the list look longer. Equally, do not omit an established useful effect because it was not visible in the UI sketch.

---

## 6. Audio-chain and commit-system model

The current best interpretation of the sketch is a **linear serial FX chain rendered as a spatial/filament formation**. The term “tree” describes the visual metaphor; no parallel routing or branch mixer is explicitly specified. Treat parallel branches as unconfirmed.

### 6.1 Core state model

Use two separate concepts:

**A. Staged/draft FX**

- Exactly one carousel FX type is selected at a time, or none.
- The selected FX has a draft parameter state.
- When the user scrolls to another FX, preserve the previous type's draft values in a per-FX-type draft store.
- When the user returns to a type, restore its previous uncommitted values.
- The staged FX processes audio after the currently committed chain by default.
- **MUTE FX:** bypass/mute only the staged FX; the committed chain remains audible.
- **SOLO FX:** temporarily bypass the committed chain and audition only the staged FX (with a short fade/ramp so audio and tails do not click). The committed chain's visual regions fade toward monochrome and reduced brightness as it is taken out of the audible path.
- SOLO and MUTE should be mutually exclusive or have an explicitly defined priority; a confusing combination must not be possible.
- **RESET:** reset only the current staged FX's draft values to its documented defaults. Do not erase committed instances.

**B. Committed FX instances**

Each committed instance should have:

- a stable unique instance ID;
- FX type;
- a complete, independent snapshot of its parameter values;
- a stable visual identity/seed and palette assignment;
- its position in the ordered serial chain;
- schema/version information for future preset migration.

On COMMIT, deep-copy the current staged settings into a new committed instance and append it to the end of the serial chain. After that, the committed instance is immutable. No parameter changes, automation, preset edits, or hidden UI route should mutate the committed snapshot. Deletion is the only destructive operation; reordering is allowed and changes processing order.

**After COMMIT:** return the carousel to a neutral/unselected state. Do not silently highlight or activate another FX type. The committed chain continues processing; the user must scroll/select a new draft FX to stage another effect.

**Duplicate FX:** allowed. Two or more instances of the same FX type are separate, independent committed snapshots.

### 6.2 Processing semantics

Proposed normal path:

`Input → committed FX instance 1 → committed FX instance 2 → ... → last committed FX → staged FX (when selected and not muted) → output`

Proposed SOLO path:

`Input → staged FX only → output`

Proposed MUTE path:

`Input → committed FX chain only → output`

Transitions between these paths should be smoothed. Do not hard-switch state if it causes clicks. Decide whether existing delay/reverb tails are allowed to ring out during transitions; the sketch says the committed branch should die away when staged FX is soloed, so a short controlled tail fade is the initial interpretation.

### 6.3 Persistence: an important distinction

The user said presets should save “the tree” / the complete chain, not individual carousel drafts. The most consistent interpretation is:

- **User preset save/load:** save and restore the committed FX chain as one ordered structure, including every committed instance and its exact settings. Do not include uncommitted carousel drafts in the user preset.
- **DAW project/plugin-state recall:** likely should restore the committed chain and may also need to restore the current staged draft/UI state so reopening a project does not lose active work. This distinction was not explicitly settled. Ask the user before deciding.

Use versioned serialisation (e.g., XML/JSON/binary state with schema version and validation). Never rely on visual layout coordinates as the authoritative chain order; serialise the actual ordered chain.

### 6.4 Reordering and deletion interaction

- Each committed instance should be draggable to a new position in the formation/chain.
- Show a clear drag preview and insertion position. Do not change audio order until the drop is accepted.
- While dragging an instance, reveal a small **DELETE** drop target in a consistent corner, as requested in the sketch description.
- Dropping over DELETE opens a confirmation prompt naming the actual instance/type: “Delete [FX name]?” with explicit **Cancel** and **Delete** options.
- Dropping anywhere else reorders; dropping back in its original position changes nothing.
- Cancel, Escape, or dropping outside all valid targets must not delete anything.
- Deletion removes only that specific instance; duplicates remain untouched.
- Consider a post-delete undo action, but do not replace the user's required confirmation flow.

### 6.5 Carousel and tooltips

- Support both mouse-wheel scrolling and up/down arrow controls adjacent to the FX name.
- Use a smooth carousel transition: selected effect is clearest/largest; adjacent names are partially visible and fade/scale in a restrained way.
- Keep the settings panel tied to the current staged FX type.
- Draft values are per FX type, not per carousel position.
- Hovering a control label/term for a short delay should show a concise plain-language explanation. Tooltips should explain what changing the control *sounds like*, not only provide textbook definitions.
- Add keyboard focus/activation and sensible accessible labels; do not make the carousel mouse-only.
- Avoid ambiguous labels like “size” where the user needs “Decay Time,” “Feedback,” or “Formant.”

---

## 7. Visual formation: primary reference is the supplied video

### 7.1 What is visible in the reference video

The supplied 30.65-second, 720×1280 portrait video shows a very dark, almost black field with a living, asymmetric formation of fine light filaments. The brightest energy gathers in a shifting central/right region, while long, wispy strands stretch outward and across the darker surrounding space. There are translucent smoky layers, hair-thin lines, luminous intersections, and flowing wisps. The dominant visible colours are:

- black / near-black background;
- deep indigo and blue-violet shadow tones;
- violet, lavender, and magenta filaments;
- icy blue/cyan strands and faint teal accents;
- very bright pale lavender and white highlights.

The formation changes gradually and organically. It does not look like a grid, a conventional node graph, a starfield, a standard “space wallpaper” nebula, or a static radial equaliser.

### 7.2 Hard visual requirements derived from the user's description and video

- The right/large canvas is the formation field; the left control strip remains useful and readable.
- **No committed FX = visually empty field.** It may have an extremely subtle atmospheric black/near-black floor, but do not show an active network before any effect is committed.
- One committed FX produces a small/contained formation.
- More committed FX produce a larger and more complex formation; fewer FX produce a smaller one.
- The colours of the formation's regions correspond to the committed FX instances/types. Use only the hues present in the supplied video. Do not invent a rainbow palette or unrelated bright colours.
- The visual's brightness/intensity responds to the passing audio signal. Silence should not make it flash; real signal should animate it naturally.
- SOLO should make the committed formation fade/die back, desaturating toward monochrome as its audio branch is suppressed. Restore its colour/energy smoothly when the committed chain becomes audible again.
- A staged FX that has not been committed should be represented subtly as the current live audition/effect, but must not be mistaken for a committed formation component.
- The formation must feel fluid, asymmetrical, layered, and continuous. **Do not use fixed grids, evenly spaced dots, rigid branching lines, or a standard node-editor layout.**

### 7.3 Suggested procedural rendering model

This is an engineering proposal for recreating the reference's visual language; it is not a claim about how the original video was produced.

- Represent every committed instance with a stable seed, type colour, weight, and a contribution to a shared force/flow field.
- Build the formation from multi-scale curved filaments and translucent trails. Possible tools include curl-noise flow, particle advection, attractor/repulsor fields, slowly evolving vector fields, and accumulated trails.
- Use multiple scales: a few bright fine strands, many dimmer diffuse strands, and broad translucent wisps. Use additive/screen-like blending carefully to preserve blacks.
- Avoid random repositioning every frame. Particle paths should have temporal continuity and stable identity when the UI repaints.
- Increasing committed FX count should affect the formation's spatial extent, density, and complexity. It should not simply increase every particle's brightness to maximum.
- The effect type should determine a restrained colour region drawn from the video palette. Duplicate instances may use related shades or different spatial sub-regions while remaining within the same hue family.
- Reordering should change the DSP chain order. It may also subtly rearrange the visual relationship, but should not wipe and regenerate the entire field.
- Use audio analysis to drive a smoothed energy value. RMS/peak and optionally a few coarse frequency bands can influence filament brightness, width, movement rate, or activity. Smooth the signal with attack/release behaviour to avoid flicker.
- Visual rendering runs on the UI/render thread, not in the audio callback. The audio thread publishes a small amount of meter/analysis data through atomics or a lock-free transfer structure.
- Offer a quality/performance setting if the renderer becomes expensive. Animation must never compromise audio stability.

### 7.4 Palette guidance

Treat these as approximate visual anchors sampled/observed from the supplied video, not exact colour-management values. Claude should inspect the original video frame-by-frame and tune the palette against the real reference before finalising it.

| Role | Approximate colour direction |
|---|---|
| Background | `#020207` / near-black |
| Deep field | indigo / blue-black |
| Dim filaments | muted blue-violet and smoky purple |
| Main filaments | lavender, violet, restrained magenta |
| Secondary filaments | icy blue / cyan with only faint teal |
| Highlight cores | pale lavender-white / near-white |

Do not use a bright neon rainbow, yellow/orange as a dominant colour, green as a dominant colour, stars, planets, or generic galaxy clip-art. The user's video is the authority if these notes differ from what a frame reveals.

---

## 8. VST3 and implementation architecture

### 8.1 Framework / SDK choice

The repo is expected to be a new C++ plug-in project, but Claude must inspect the repository and KADAL before selecting a framework.

- The official **Steinberg VST 3.8 SDK is MIT-licensed**, with commercial use permitted subject to retaining license notices. See [Steinberg's VST 3 licensing page](https://steinbergmedia.github.io/vst3_dev_portal/pages/VST%2B3%2BLicensing/VST3%2BLicense.html) and [official SDK repository](https://github.com/steinbergmedia/vst3sdk).
- **JUCE 9** currently uses AGPLv3 or a commercial JUCE licence. See [JUCE's current license file](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md). Do not assume that using JUCE is automatically free for a closed-source commercial release. Check the intended distribution model and exact licence before choosing it.
- If KADAL already contains a stable JUCE/VST3 architecture or reusable modules, inspect it first. Do not start a competing framework or duplicate an existing engine without a reason.
- If no useful existing base exists, compare direct VST3 SDK vs JUCE for UI, state management, testing, licensing, and long-term maintenance. Present the trade-off to the user rather than silently selecting based only on convenience.

### 8.2 Host compatibility and VST3 MIDI

- First target should likely be **Windows x64 VST3**, with project structure that does not unnecessarily prevent future macOS support. Confirm before committing to a platform-specific design.
- The user works with FL Studio 25 and Cubase 14; test in both if the environment is available. A standalone VST3 validator/test host should be part of the build pipeline.
- VST3 MIDI is event-based. Steinberg documents MIDI Note On/Off as VST3 events; MIDI is not simply a VST2-style built-in stream. Implement the appropriate event input bus/host support for a MIDI-controlled Harmony module.
- Keep VST3 parameter IDs stable across releases. Do not create/remove/reassign the exported parameter list dynamically every time a carousel FX type or committed instance changes.
- Since committed FX are immutable and can be duplicated dynamically, their individual settings are best represented in the plug-in's versioned internal state, not as a dynamic host-automation parameter list. A stable set of global/staging parameters may be exported for host automation, but this requires deliberate design.

**Sources**

- [VST 3 API documentation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/API%2BDocumentation/Index.html)
- [VST 3 MIDI documentation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/About%2BMIDI/Index.html)
- [VST 3 Parameters and Automation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Parameters%2BAutomation/Index.html)
- [VST 3 communication FAQ](https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Communication.html)

### 8.3 Real-time audio safety

Non-negotiable rules:

- No heap allocation, file I/O, network access, logging, blocking locks, UI calls, or expensive object construction in the audio processing callback.
- Allocate and prepare delay lines, FFT buffers, filters, grain buffers, and per-instance processor state before real-time processing begins.
- Handle variable audio block sizes; do not assume every callback uses the expected block length.
- Smooth or ramp continuous parameters and crossfade disruptive topology/state changes.
- Use thread-safe, non-blocking communication between the UI and audio engine. UI actions such as commit, reorder, delete, and preset load must be passed to the audio engine as prepared state changes at safe boundaries.
- Do not mutate a processor's live DSP object from the message/UI thread while the audio thread is processing it.
- Be explicit about sample-rate changes, mono/stereo layouts, denormal protection, tail length, bypass, and latency reporting.
- Keep CPU and memory bounded when users add many duplicate FX instances. Define a maximum safe chain length or a graceful warning after measuring actual cost; do not pretend the chain is unlimited if it is not.

Useful references:

- [JUCE AudioProcessor documentation](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html) — `prepareToPlay`, `processBlock`, variable block sizes, and processing lifecycle.
- [JUCE APVTS state tutorial](https://juce.com/tutorials/tutorial_audio_processor_value_tree_state/) — stable parameter management and state serialisation. Note that `copyState()` is thread-safe but not real-time-safe; do not call it in the audio callback.
- [VST 3 data exchange](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Data%2BExchange/Index.html) — host-mediated transfer of processor analysis data to the editor.

### 8.4 Suggested module boundaries

Use modular boundaries similar to the following, adapting to the framework found in KADAL:

```text
PluginProcessor
  ├── AudioEngine
  │    ├── CommittedFxChain (ordered instances)
  │    ├── StagedFxProcessor (one current draft FX)
  │    ├── SoloMuteRouter
  │    ├── InputOutputGain / safety / metering
  │    └── HostEventRouter (MIDI notes, CC, tempo, transport)
  ├── StateManager
  │    ├── CommittedChainSerialiser
  │    ├── DraftSettingsStore
  │    └── PresetManager
  ├── Editor
  │    ├── FxCarousel
  │    ├── FxSettingsPanel
  │    ├── SoloMuteCommitResetControls
  │    ├── FormationRenderer
  │    ├── DragReorderDeleteOverlay
  │    └── DelayedPlainLanguageTooltips
  └── AnalysisBridge (lock-free/atomic UI telemetry)
```

A committed instance should own a pre-built processor state or refer to an immutable parameter snapshot plus a processor instance. Commit, reorder, and delete must not cause unprepared allocation or processor construction inside the real-time callback. Use a safe hand-off mechanism (preparation on a non-audio thread, atomic pointer swap, lock-free queue, or equivalent) and retire old processors safely away from the audio thread.

### 8.5 Preset and state design

Recommended data shape (conceptual, not mandatory JSON):

```json
{
  "schemaVersion": 1,
  "chain": [
    {
      "instanceId": "stable-unique-id",
      "type": "delay",
      "order": 0,
      "parameters": { "...": "complete parameter snapshot" },
      "visual": { "paletteSlot": "...", "seed": 12345 }
    }
  ]
}
```

Implementation notes:

- Validate FX type IDs, parameter ranges, missing fields, duplicate IDs, and schema version during load.
- Do not load malformed data into the audio thread.
- Preserve duplicate FX instances as separate array elements with unique IDs.
- Reordering changes array/chain order; never sort by FX type.
- Provide backward-compatible migration hooks even if the first release only has schema version 1.
- User presets save the committed chain only, pending confirmation of whether DAW project state should additionally save the active draft.

---

## 9. Test and quality checklist

### Audio correctness

- Silence in → silence out (except intentionally self-running feedback tails, if supported).
- Unity/neutral settings do not create unexpected gain or tonal change.
- Mono/stereo routing works and folds to mono gracefully.
- No clicks at commit, reset, solo/mute, reorder, delete, preset load, or bypass transitions.
- Reverb/delay tails behave intentionally at mode changes.
- Feedback remains bounded under pathological parameter combinations.
- No runaway DC, denormals, NaNs, infinities, or unbounded memory growth.
- Host transport tempo changes update tempo-synced FX correctly.
- Sample-rate changes re-prepare all DSP state correctly.
- Block sizes vary without crash or glitch.
- MIDI note release/all-notes-off does not leave stuck harmony voices.

### UX correctness

- Scroll wheel and arrow buttons change the carousel predictably.
- Draft values survive switching FX types and returning.
- RESET resets only the current draft FX.
- COMMIT snapshots the staged FX exactly once and clears the selected state afterward.
- Committed nodes cannot be edited through any UI or automation loophole.
- Duplicate FX types work independently.
- Reorder changes the actual audio order and visual relationship.
- Delete requires drop-on-target and explicit confirmation.
- Cancel never deletes.
- Preset save/load restores the committed chain in the correct order with exact per-instance values.
- Tooltips appear after a short hover delay and explain terms plainly.

### Visual correctness

- Field is empty with no committed nodes.
- Formation extent/complexity grows with number and type of committed instances.
- Effect colours remain inside the reference video's palette.
- Brightness follows the audio signal with smooth attack/release; silence remains calm.
- Soloing the staged effect fades the committed formation toward monochrome as specified.
- No fixed-grid appearance, no generic particle wallpaper, no unrelated stars/planets, and no random frame-to-frame popping.
- Animation remains stable when the DAW is stopped, playing, and under high CPU load.

### Test hosts and build

- Build Release and Debug configurations.
- Validate VST3 metadata, bus layout, state recall, and host lifecycle.
- Test FL Studio 25 and Cubase 14 if installed/available; record exact host versions and what was verified.
- Never claim host compatibility based only on a successful compile.

---

## 10. Decisions not settled by the research

These are genuine product decisions, not facts that can be inferred from interviews or technical documents. Claude should present them to the user before locking the architecture:

1. **Catalogue navigation:** the user has specified a broad FX library. The exact number of modules will grow through the roadmap; do not reopen the question of whether the plugin is limited to Delay / Reverb / Reverse. If a 30+ item carousel requires category/search navigation, propose the least disruptive option while preserving the carousel.
2. **Harmony input:** should the first harmony implementation receive MIDI notes from the DAW, derive notes from a selected key/scale, or offer both? Does the user want the four-voice “Hide and Seek”-style arrangement as a first-release module?
3. **Reverse meaning:** reverse delay, reverse-buffer playback, reverse reverb, or one chosen mode for V1?
4. **Preset versus DAW state:** should user presets contain only committed chain data while a DAW project additionally remembers the uncommitted staged FX/draft values?
5. **Post-commit draft behaviour:** when an FX is committed, should its staged draft settings remain available for making another duplicate, or should that FX's draft return to defaults?
6. **Chain topology:** is the FX “tree” a visual formation representing a simple serial chain (the most direct reading of the drag-reorder requirement), or are actual parallel branches/splits expected?
7. **Committed node interaction:** should clicking a committed formation part only show read-only details, or should it have no click action at all? It must never enable editing.
8. **Target formats:** Windows x64 VST3 first is the practical assumption; should future macOS VST3 support be a requirement from the start?
9. **Host automation:** should controls for the staged FX/global macros be automatable in the DAW? Committed instances are immutable, so exposing their individual parameters as host automation would conflict with the commit rule.

If the user does not answer immediately, do not invent answers to these points. Continue only with work that does not lock them in (repo audit, KADAL research, framework comparison, DSP prototypes that are explicitly disposable, UI skeleton, and test infrastructure).

---

## 11. Source index

### Artist / gear / techniques

1. [MusicRadar — Imogen Heap and “Hide and Seek” (2025)](https://www.musicradar.com/artists/imogen-heap-hide-and-seek)
2. [iZotope — Heap's harmony explanation and VocalSynth notes](https://www.izotope.com/community/blog/get-that-sound-with-vocalsynth-harmony-effects)
3. [Sound On Sound — “Imogen Heap: Recording Ellipse” (2009)](https://www.soundonsound.com/people/imogen-heap-recording-ellipse)
4. [Spotify for Artists — “Imogen Heap on Technology for Artists” (2019)](https://artists.spotify.com/fi/blog/imogen-heap-on-technology-for-artists)
5. [The MIDI Association — Imogen Heap interview](https://midi.org/imogen-heap-exclusive-interview)
6. [What Hi-Fi? — Mi.Mu gloves / expressive stage control (2015)](https://www.whathifi.com/news/promoted-i-want-studio-tricks-in-my-hands-stage-imogen-heap)
7. [Mi.Mu — Mapping documentation](https://www.mimugloves.com/documentation/mapping/)
8. [Mi.Mu — Glove device input documentation](https://www.mimugloves.com/documentation/glove-device-inputs/)
9. [Mi.Mu — Glover introduction](https://production.mimugloves.com/documentation/intro-to-glover/)
10. [DigiTech Vocalist Workstation EX manual](https://device.report/m/7bc0be4fd8fd6603bda953cf934829cd9faa6421458037301bb3304750172c53)
11. [Aurgasm — 2005 Heap interview](https://aurgasm.us/2005/10/aurgasm-interview-imogen-heap/)
12. [Journal on the Art of Record Production — Heap as musical cyborg](https://www.arpjournal.com/asarpwp/imogen-heap-as-musical-cyborg-renegotiations-of-power-gender-and-sound/)

### DSP / plugin implementation

13. [CLEESE paper — phase vocoder, phase locking, formants, transients (PLOS ONE, 2019)](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0205943)
14. [AES — Jot & Chaigne, artificial reverberators and digital delay networks (1991)](https://aes.org/publications/elibrary/elibrary-page/?id=5663)
15. [DAFx 2026 — Feedback Delay Networks tutorial](https://dafx26.mit.edu/tutorials/)
16. [JUCE DelayLine class](https://docs.juce.com/master/classjuce_1_1dsp_1_1DelayLine.html)
17. [JUCE Chorus class](https://docs.juce.com/master/classjuce_1_1dsp_1_1Chorus.html)
18. [JUCE Reverb parameters](https://docs.juce.com/master/structjuce_1_1Reverb_1_1Parameters.html)
19. [JUCE AudioProcessor](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html)
20. [JUCE APVTS state tutorial](https://juce.com/tutorials/tutorial_audio_processor_value_tree_state/)
21. [Steinberg VST3 SDK repository](https://github.com/steinbergmedia/vst3sdk)
22. [Steinberg VST3 licensing](https://steinbergmedia.github.io/vst3_dev_portal/pages/VST%2B3%2BLicensing/VST3%2BLicense.html)
23. [Steinberg VST3 MIDI documentation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/About%2BMIDI/Index.html)
24. [Steinberg VST3 Parameters and Automation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Parameters%2BAutomation/Index.html)
25. [Steinberg VST3 Communication FAQ](https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Communication.html)
26. [JUCE current licence file (JUCE 9)](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md)

### Expanded FX catalogue / DSP research

27. [DAFx — Filters, EQ, convolution, time-varying filters and phaser (Chapter 2)](https://www.dafx.de/DAFX_Book_Page/chapter2.html)
28. [DAFx — Delay structures, fractional delays, chorus, flanger, vibrato and echo (Chapter 3)](https://www.dafx.de/DAFX_Book_Page/chapter3.html)
29. [DAFx — Modulators/demodulators, ring modulation, stereo phaser, rotary speaker and envelope following (Chapter 4)](https://www.dafx.de/DAFX_Book_Page/chapter4.html)
30. [DAFx — Nonlinear processing, dynamics, compression, gate, de-esser, distortion, tape saturation and exciters (Chapter 5)](https://www.dafx.de/DAFX_Book_Page/chapter5.html)
31. [DAFx — Spatial effects, Doppler, distance, trajectories, reverberation and stereo width (Chapter 6)](https://dafx.de/DAFX_Book_Page/chapter6.html)
32. [Universal Audio — Lexicon 480L digital reverb/effects manual](https://help.uaudio.com/hc/en-us/articles/33194625601044-Lexicon-480L-Digital-Reverb-and-Effects-Manual)
33. [Universal Audio — Roland RE-201 Space Echo manual](https://help.uaudio.com/hc/en-us/articles/33538271334164-Roland-RE-201-Space-Echo-Manual)
34. [Roland — RE-201 tape-head, speed, saturation and spring-reverb explanation](https://static.roland.com/assets/media/pdf/RE-20_OM.pdf)
35. [Soundtoys — EchoBoy](https://www.soundtoys.com/product/EchoBoy/)
36. [Soundtoys — Crystallizer](https://www.soundtoys.com/product/crystallizer/)
37. [Eventide — H910 Harmonizer history and design](https://www.eventideaudio.com/rackmount/h910-harmonizer/)
38. [Eventide — H3000 multi-effect system and effect list](https://www.eventideaudio.com/rackmount/h3000/)
39. [Eventide — H3000 Factory modular effects blocks](https://www.eventideaudio.com/plug-ins/h3000-factory/)
40. [Eventide — Blackhole user guide](https://s3.amazonaws.com/com.eventide.downloads/Product%20Manuals/Blackhole%20User%20Guide.pdf)
41. [Strymon — BigSky Shimmer user manual](https://www.strymon.net/manuals/BigSky_UserManual_RevD.pdf)
42. [Eventide — TriceraChorus](https://www.eventideaudio.com/plug-ins/TriceraChorus/)
43. [Strymon — Deco v2 user manual](https://www.strymon.net/manuals/Deco_v2_UserManual_RevC.pdf)
44. [Soundtoys — Decapitator](https://www.soundtoys.com/product/Decapitator/)
45. [FabFilter — Saturn 2 help and distortion styles/oversampling](https://www.fabfilter.com/help/saturn)
46. [iZotope — Trash multiband distortion and convolution module](https://www.izotope.com/products/trash)
47. [Universal Audio — 1176 FET compressor manual](https://help.uaudio.com/hc/en-us/articles/34530260482324-1176-Classic-FET-Compressor-Manual)
48. [Universal Audio — LA-2A optical compressor manual](https://help.uaudio.com/hc/en-us/articles/19378009641748-LA-2A-Tube-Compressor-Manual)
49. [FabFilter — Pro-Q dynamic EQ](https://www.fabfilter.com/help/pro-q/using/dynamic-eq)
50. [FabFilter — Pro-DS oversampling and de-essing](https://www.fabfilter.com/help/pro-ds/using/oversampling)
51. [FabFilter — Pro-MB oversampling and dynamic processing](https://www.fabfilter.com/help/pro-mb/using/oversampling)
52. [Mutable Instruments — Clouds granular processor manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/clouds/manual/)
53. [Mutable Instruments — Clouds granular design notes](https://pichenettes.github.io/mutable-instruments-documentation/modules/clouds/original_blurb/)
54. [Ableton — Live 12 audio effects reference, including Beat Repeat](https://www.ableton.com/en/manual/live-audio-effect-reference/)
55. [Ableton — Vocoder architecture in Live manual](https://www.easymanua.ls/ableton/live/manual?p=338)
56. [Xfer Records — LFO Tool](https://xferrecords.com/products/lfo-tool/)
57. [Epic Games — Convolution Reverb technical explanation](https://dev.epicgames.com/documentation/unreal-engine/convolution-reverb-in-unreal-engine)
58. [D'Angelo & Välimäki — An Improved Virtual Analog Model of the Moog Ladder Filter (2013)](https://raw.githubusercontent.com/ddiakopoulos/MoogLadders/master/research/DAngeloValimaki.pdf)
59. [Zynaptiq — MORPH real-time spectral morphing and formant shifting](https://www.zynaptiq.com/morph/)
---

## 12. Final direction

The most useful IH1 should be a **musical instrument disguised as an FX processor**: the user auditions and shapes an effect, commits it, and gradually constructs an immutable chain whose sound and appearance both become more complex. The Heap research points to expressive pitch/harmony, delay-led space, transformed acoustic material, and gesture-like control. The supplied video—not generic “space” design—must govern the visual result.

The first build should prove the commit model, extensible effect registry, immutable snapshots, state persistence and organic visual formation, with a deliberately broad first implementation batch—not a three-effect ceiling. Delay, Reverb and Reverse must be included because the sketch names them; Harmony, Pitch/Formant, modulation, distortion, dynamics, granular and spectral effects are part of the wider IH1 identity and should be prioritised through the catalogue roadmap. Expand in coherent batches, but do not stop at the sketch's examples. Every FX must earn its place through a distinct sound, a robust DSP implementation and repeatable tests.
