# IH1 feedback audit

Tail 15 s after each source; growth = loudest 100 ms window from +1 s to the end vs the first second after the excitation.

## Factory presets (Output Safety off)

| Preset / pass | Chain | Worst source | ref dBFS | late dBFS | growth | slope dB/s | peak | faults | verdict |
|---|---|---|---|---|---|---|---|---|---|
| ess.mix_glue | compressor > tape_colour |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.7 | 0 | ok |
| ess.plate_vocal | deesser > eq > compressor > reverb | chord | -12.1 | -240.0 | -48.5 | -99.00 | +4.4 | 0 | ok |
| ess.clean_slap | delay > reverb | chord | -13.7 | -240.0 | -99.1 | -99.00 | -1.3 | 0 | ok |
| ess.clean_slap @macros100 | delay > reverb | chord | -5.7 | -164.8 | -18.4 | -99.00 | +3.1 | 0 | ok |
| ess.clean_slap @sweep | delay > reverb | chord | -10.7 | -166.5 | -14.9 | -99.00 | -0.6 | 0 | ok |
| ess.quarter_throw | delay > reverb | chord | -12.3 | -240.0 | -32.7 | -99.00 | +1.0 | 0 | ok |
| ess.quarter_throw @macros100 | delay > reverb | chord | -7.8 | -122.3 | -13.1 | -99.00 | +2.8 | 0 | ok |
| ess.quarter_throw @sweep | delay > reverb | chord | -10.8 | -122.7 | -11.4 | -99.00 | +1.6 | 0 | ok |
| ton.warm_console | transient > tape_colour > exciter |  | -240.0 | -240.0 | -200.0 | -99.00 | +1.7 | 0 | ok |
| ex.fractal_feedback | grain_engine > compressor | chord | -12.7 | -240.0 | -9.7 | -99.00 | +5.6 | 0 | ok |
| mot.spiral_stairs | freq_shift > phaser | chord | -10.5 | -240.0 | -23.7 | -99.00 | +0.7 | 0 | ok |
| f3.dub_siren | tape_echo > degrade | noise | -23.3 | -42.7 | -5.1 | -1.18 | -8.3 | 0 | LONG |
| f3.dub_siren @macros100 | tape_echo > degrade | noise | -23.6 | -43.7 | -6.2 | -1.16 | -8.5 | 0 | LONG |
| f3.dub_siren @sweep | tape_echo > degrade | noise | -23.5 | -43.5 | -6.1 | -1.17 | -8.5 | 0 | LONG |
| f3.reverse_loop | reverse > reverb | chord | -9.7 | -64.5 | -4.4 | -99.00 | +0.9 | 0 | ok |
| ex.ring_cascade | ring_mod > flanger > delay | chord | -12.3 | -240.0 | -33.0 | -99.00 | -1.2 | 0 | ok |
| f3.spectral_regen | spectral_delay > spectral_smear | noise | -22.5 | -81.8 | -2.4 | -99.00 | -4.2 | 0 | ok |
| v4.signal_collapse | spectral_shatter > degrade > echo_matrix | noise | -17.6 | -212.5 | -20.9 | -99.00 | -1.7 | 0 | ok |
| v4.signal_collapse @macros100 | spectral_shatter > degrade > echo_matrix | chord | -10.9 | -102.9 | -11.6 | -99.00 | -1.0 | 0 | ok |
| v4.signal_collapse @sweep | spectral_shatter > degrade > echo_matrix | noise | -17.7 | -103.7 | -10.3 | -99.00 | -0.3 | 0 | ok |
| v4.bit_cathedral | modal > reverb > degrade | chord | -8.8 | -240.0 | -6.1 | -99.00 | -1.8 | 0 | ok |
| v4.bit_cathedral @macros100 | modal > reverb > degrade | noise | -31.6 | -240.0 | -2.5 | -99.00 | -1.5 | 0 | ok |
| v4.bit_cathedral @sweep | modal > reverb > degrade | noise | -29.9 | -240.0 | -2.7 | -99.00 | -2.2 | 0 | ok |
| v4.frame_demolition | transient_split > slice_shuffle > fold_crush | noise | -21.2 | -240.0 | +4.4 | -99.00 | -7.6 | 0 | ok |
| v4.corrupted_broadcast | ring_mod > degrade > texture > freq_shift | drums | -30.3 | -240.0 | -15.2 | -99.00 | -8.3 | 0 | ok |
| v4.dead_air | glitch > echo_matrix > saturation | noise | -16.9 | -103.1 | +0.7 | -99.00 | -2.6 | 0 | ok |
| v4.dead_air @macros100 | glitch > echo_matrix > saturation | noise | -18.5 | -55.8 | -0.9 | -99.00 | -0.0 | 0 | ok |
| v4.dead_air @sweep | glitch > echo_matrix > saturation | chord | -9.8 | -55.0 | -2.3 | -99.00 | -0.4 | 0 | ok |
| v4.glass_shards | spectral_shatter > modal > transient | chord | -10.9 | -240.0 | -74.2 | -99.00 | +8.3 | 0 | ok |
| v4.subatomic | dispersion > pitch_shift > fold_crush | chord | -9.0 | -240.0 | -100.8 | -99.00 | -2.5 | 0 | ok |
| v4.broken_clock | time_warp > glitch > degrade |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.9 | 0 | ok |
| ss.glacier | spectral_smear > reverb | noise | -35.5 | -64.8 | -1.2 | -99.00 | -7.5 | 0 | ok |
| ss.half_time_haze | time_warp > spectral_smear | noise | -240.0 | -240.0 | +136.1 | -99.00 | -5.8 | 0 | ok |
| ss.tape_slow | grain_engine > spectral_smear | noise | -240.0 | -240.0 | +159.9 | -99.00 | +4.3 | 0 | ok |
| ss.smear_field | spectral_smear > width | chord | -27.9 | -81.0 | -4.1 | -99.00 | -12.3 | 0 | ok |
| ss.quarter_drift | time_warp > tape_colour > vibrato | chord | -240.0 | -240.0 | +167.0 | -99.00 | -5.9 | 0 | ok |
| s3.frozen_bloom | spectral > grain_engine | drums | -24.5 | -24.6 | +0.2 | +0.01 | -5.6 | 0 | hold (intended) |
| gm.particle_drift | grain_engine > reverb | noise | -240.0 | -240.0 | +164.8 | -99.00 | +2.8 | 0 | ok |
| gm.particle_drift @macros100 | grain_engine > reverb | noise | -240.0 | -240.0 | +163.9 | -99.00 | +3.1 | 0 | ok |
| gm.particle_drift @sweep | grain_engine > reverb | noise | -240.0 | -240.0 | +162.6 | -99.00 | +2.5 | 0 | ok |
| gm.swarm | grain_engine > filter | chord | -12.2 | -240.0 | -10.1 | -99.00 | +1.0 | 0 | ok |
| frg.frozen_breath | grain_engine > spectral_smear | chord | -16.1 | -240.0 | -0.1 | -99.00 | -5.1 | 0 | ok |
| gm.cascade | grain_engine > compressor | chord | -13.5 | -240.0 | -5.1 | -99.00 | +2.6 | 0 | ok |
| frg.loop_ghost | grain_engine > reverb | drums | -7.2 | -6.1 | +2.8 | -0.01 | +3.6 | 0 | hold (intended) |
| frg.loop_ghost @macros100 | grain_engine > reverb | chord | -11.6 | -8.5 | +5.6 | +0.06 | +2.1 | 0 | GROW |
| frg.loop_ghost @sweep | grain_engine > reverb | chord | -6.4 | -11.8 | -0.4 | +0.05 | +3.0 | 0 | hold (intended) |
| gm.wide_grains | grain_engine > delay | noise | -19.1 | -240.0 | -1.2 | -99.00 | +0.0 | 0 | ok |
| v4.pocket_archaeology | slice_shuffle > tape_colour > reverb | noise | -26.8 | -240.0 | +12.9 | -99.00 | -1.5 | 0 | ok |
| v4.ghost_break | slice_shuffle > transient | chord | -240.0 | -240.0 | +176.3 | -99.00 | +12.0 | 0 | ok |
| v4.borrowed_chorus | grain_engine > chorus > reverb | chord | -9.1 | -240.0 | -26.9 | -99.00 | +0.2 | 0 | ok |
| v4.borrowed_chorus @macros100 | grain_engine > chorus > reverb | noise | -14.6 | -240.0 | -22.1 | -99.00 | +2.0 | 0 | ok |
| v4.borrowed_chorus @sweep | grain_engine > chorus > reverb | chord | -9.1 | -240.0 | -22.6 | -99.00 | +1.3 | 0 | ok |
| v4.velvet_fragments | grain_engine > reverb | chord | -10.6 | -240.0 | -12.6 | -99.00 | -0.1 | 0 | ok |
| v4.velvet_fragments @macros100 | grain_engine > reverb | chord | -10.5 | -240.0 | -4.6 | -99.00 | +1.3 | 0 | ok |
| v4.velvet_fragments @sweep | grain_engine > reverb | chord | -11.2 | -240.0 | -5.6 | -99.00 | +1.1 | 0 | ok |
| v4.half_remembered | grain_engine > tape_colour > vibrato | noise | -240.0 | -240.0 | +165.0 | -99.00 | -3.4 | 0 | ok |
| v4.sampler_jigsaw | slice_shuffle > compressor | noise | -15.8 | -240.0 | -5.7 | -99.00 | -0.8 | 0 | ok |
| v4.dust_cloud | granular > texture > reverb | chord | -14.9 | -240.0 | -8.1 | -99.00 | -0.3 | 0 | ok |
| v4.dust_cloud @macros100 | granular > texture > reverb | noise | -23.4 | -240.0 | -1.4 | -99.00 | -1.6 | 0 | ok |
| v4.dust_cloud @sweep | granular > texture > reverb | noise | -22.1 | -240.0 | -2.5 | -99.00 | -1.3 | 0 | ok |
| v4.time_mosaic | grain_engine > spectral_delay | noise | -240.0 | -240.0 | +158.2 | -99.00 | -4.5 | 0 | ok |
| v4.attack_ghosts | transient_split > reverb | chord | -13.5 | -240.0 | -39.2 | -99.00 | -1.3 | 0 | ok |
| v4.sustain_engine | transient_split > reverb | chord | -11.4 | -120.6 | +1.1 | -99.00 | +0.5 | 0 | ok |
| v4.palindrome_tape | slice_shuffle > time_warp > tape_colour | drums | -240.0 | -240.0 | +169.3 | -99.00 | -2.3 | 0 | ok |
| gr.slice_retrigger | glitch > compressor | drums | -13.7 | -240.0 | -14.7 | -99.00 | +1.2 | 0 | ok |
| r3.tape_stop_bars | time_warp > saturation |  | -240.0 | -240.0 | -200.0 | -99.00 | +3.5 | 0 | ok |
| gr.backspin | time_warp > filter |  | -240.0 | -240.0 | -200.0 | -99.00 | +7.3 | 0 | ok |
| gr.buffer_roll | glitch > delay | noise | -15.6 | -240.0 | -94.0 | -99.00 | -1.3 | 0 | ok |
| gr.halftime_chop | time_warp > gate |  | -240.0 | -240.0 | -200.0 | -99.00 | -5.2 | 0 | ok |
| rhy.stutter_bloom | stutter > glitch > reverb | noise | -13.9 | -230.9 | -8.8 | -99.00 | -2.2 | 0 | ok |
| rhy.stutter_bloom @macros100 | stutter > glitch > reverb | noise | -17.2 | -240.0 | -7.9 | -99.00 | -3.1 | 0 | ok |
| rhy.stutter_bloom @sweep | stutter > glitch > reverb | chord | -13.0 | -240.0 | -7.2 | -99.00 | -2.2 | 0 | ok |
| v4.false_downbeat | glitch > echo_matrix | chord | -11.5 | -189.8 | -5.8 | -99.00 | +0.4 | 0 | ok |
| v4.pocket_swing | slice_shuffle > echo_matrix | noise | -24.2 | -211.9 | +6.9 | -99.00 | -2.1 | 0 | ok |
| v4.juke_grid | glitch > echo_matrix | chord | -11.9 | -240.0 | -4.5 | -99.00 | -1.0 | 0 | ok |
| v4.stop_start | time_warp > stutter |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.4 | 0 | ok |
| v4.reverse_roll | stutter > reverb |  | -240.0 | -240.0 | -200.0 | -99.00 | -2.0 | 0 | ok |
| v4.mosaic_gate | spectral_shatter > gate |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.4 | 0 | ok |
| v4.clave_machine | echo_matrix > filter | chord | -12.9 | -116.7 | -11.6 | -99.00 | +2.2 | 0 | ok |
| v4.ratchet_vox | glitch > compressor > saturation | chord | -16.3 | -240.0 | -0.1 | -99.00 | +3.4 | 0 | ok |
| v4.irregular_heart | slice_shuffle > gate | chord | -14.7 | -240.0 | +0.1 | -99.00 | -2.0 | 0 | ok |
| v4.backspin_cascade | time_warp > reverse_echo | chord | -12.1 | -240.0 | -28.5 | -99.00 | +0.8 | 0 | ok |
| gm.octave_dust | grain_engine > reverb | chord | -12.6 | -240.0 | -21.5 | -99.00 | +0.8 | 0 | ok |
| gm.octave_dust @macros100 | grain_engine > reverb | noise | -16.9 | -240.0 | -4.7 | -99.00 | -0.1 | 0 | ok |
| gm.octave_dust @sweep | grain_engine > reverb | noise | -17.7 | -240.0 | -4.8 | -99.00 | +0.3 | 0 | ok |
| vt.giant | pitch_shift > fold_crush |  | -240.0 | -240.0 | -200.0 | -99.00 | -7.3 | 0 | ok |
| p3.reverse_fifths | grain_engine > reverb | chord | -12.8 | -240.0 | -6.6 | -99.00 | -1.4 | 0 | ok |
| p3.reverse_fifths @macros100 | grain_engine > reverb | chord | -13.2 | -240.0 | -1.8 | -99.00 | -1.9 | 0 | ok |
| p3.reverse_fifths @sweep | grain_engine > reverb | noise | -19.7 | -240.0 | +0.9 | -99.00 | -2.3 | 0 | ok |
| gm.minor_scatter | grain_engine > chorus |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.7 | 0 | ok |
| p3.ring_mutant | ring_mod > pitch_shift |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.7 | 0 | ok |
| p3.glide_bend | time_warp > tape_colour |  | -240.0 | -240.0 | -200.0 | -99.00 | -2.7 | 0 | ok |
| vt.whisper_cloud | spectral_smear > grain_engine | noise | -28.6 | -240.0 | -4.9 | -99.00 | -10.7 | 0 | ok |
| sig.machine_choir | vocoder > chorus > reverb | noise | -20.5 | -240.0 | -30.3 | -99.00 | -1.5 | 0 | ok |
| sig.machine_choir @macros100 | vocoder > chorus > reverb | noise | -21.2 | -240.0 | -25.5 | -99.00 | +0.3 | 0 | ok |
| sig.machine_choir @sweep | vocoder > chorus > reverb | noise | -20.7 | -240.0 | -26.1 | -99.00 | +0.1 | 0 | ok |
| vt.octave_halo | pitch_correct > harmony > formant |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.1 | 0 | ok |
| vt.ring_robot | ring_mod > degrade |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.1 | 0 | ok |
| voc.octave_ghost | pitch_shift > formant > reverse | noise | -16.2 | -240.0 | -23.6 | -99.00 | +2.2 | 0 | ok |
| v3.vowel_chorus | harmony > grain_engine |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.0 | 0 | ok |
| v4.crystal_stairs | reverse_echo > reverb | chord | -10.5 | -194.8 | -14.4 | -99.00 | +1.1 | 0 | ok |
| v4.crystal_stairs @macros100 | reverse_echo > reverb | chord | -11.1 | -101.8 | -6.0 | -99.00 | -0.2 | 0 | ok |
| v4.crystal_stairs @sweep | reverse_echo > reverb | chord | -10.6 | -101.9 | -6.9 | -99.00 | +0.3 | 0 | ok |
| v4.fifth_cascade | reverse_echo > reverb | chord | -8.4 | -114.2 | -10.9 | -99.00 | +0.3 | 0 | ok |
| v4.fifth_cascade @macros100 | reverse_echo > reverb | chord | -9.1 | -70.7 | -3.9 | -99.00 | +0.2 | 0 | ok |
| v4.fifth_cascade @sweep | reverse_echo > reverb | chord | -8.0 | -70.7 | -4.8 | -99.00 | +0.9 | 0 | ok |
| v4.descent | reverse_echo > saturation | noise | -11.9 | -240.0 | -30.8 | -99.00 | +0.3 | 0 | ok |
| v4.bell_choir | partial_recompose > reverb | chord | -15.5 | -225.5 | -32.0 | -99.00 | -4.1 | 0 | ok |
| v4.hollow_harmonics | partial_recompose > saturation |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.7 | 0 | ok |
| v4.metal_shift | partial_recompose > modal | noise | -21.1 | -240.0 | -44.4 | -99.00 | -4.0 | 0 | ok |
| v4.octave_shadow | echo_matrix > reverb | chord | -8.0 | -105.8 | -15.1 | -99.00 | +1.3 | 0 | ok |
| v4.octave_shadow @macros100 | echo_matrix > reverb | chord | -3.5 | -50.2 | -6.2 | -99.00 | +7.3 | 0 | ok |
| v4.octave_shadow @sweep | echo_matrix > reverb | chord | -4.1 | -50.2 | -5.5 | -99.00 | +6.5 | 0 | ok |
| v4.climbing_taps | echo_matrix > chorus | chord | -10.8 | -200.8 | -14.4 | -99.00 | +0.8 | 0 | ok |
| v4.formant_morph | spectral_graft > formant > reverb | noise | -7.4 | -240.0 | -51.9 | -99.00 | +11.0 | 0 | ok |
| so.rising_echo | spectral_delay > reverb | noise | -14.0 | -182.8 | -13.3 | -99.00 | -1.3 | 0 | ok |
| so.rising_echo @macros100 | spectral_delay > reverb | noise | -13.8 | -110.3 | -7.9 | -99.00 | -1.3 | 0 | ok |
| so.rising_echo @sweep | spectral_delay > reverb | noise | -13.9 | -110.3 | -7.9 | -99.00 | -1.3 | 0 | ok |
| so.scatter_prism | spectral_delay > reverb | chord | -14.3 | -89.6 | -6.8 | -99.00 | -2.5 | 0 | ok |
| so.scatter_prism @macros100 | spectral_delay > reverb | chord | -12.2 | -52.4 | -3.0 | -99.00 | -2.1 | 0 | ok |
| so.scatter_prism @sweep | spectral_delay > reverb | chord | -12.4 | -52.5 | -3.1 | -99.00 | -2.1 | 0 | ok |
| so.partial_gate | spectral_smear > reverb | chord | -6.9 | -209.5 | -23.9 | -99.00 | +9.7 | 0 | ok |
| so.partial_gate @macros100 | spectral_smear > reverb | noise | -58.3 | -125.0 | -2.5 | -99.00 | -1.3 | 0 | ok |
| so.partial_gate @sweep | spectral_smear > reverb | noise | -52.5 | -119.9 | -3.2 | -99.00 | +3.1 | 0 | ok |
| so.disintegrate | spectral_smear > spectral_delay | noise | -33.1 | -240.0 | -36.3 | -99.00 | -9.0 | 0 | ok |
| ht.fifth_resonance | resonator > spectral_smear | chord | -8.0 | -195.3 | -16.0 | -99.00 | -0.3 | 0 | ok |
| ht.crystal | spectral > resonator | chord | -25.4 | -25.6 | +0.8 | -0.00 | -9.5 | 0 | hold (intended) |
| v4.shard_field | spectral_shatter > reverb | chord | -17.0 | -165.1 | -17.0 | -99.00 | -4.0 | 0 | ok |
| v4.frequency_fold | spectral_shatter > transient |  | -240.0 | -240.0 | -200.0 | -99.00 | +12.0 | 0 | ok |
| v4.graft_memory | spectral_graft > reverb | chord | -12.1 | -240.0 | -31.6 | -99.00 | +7.3 | 0 | ok |
| v4.cross_talk | spectral_graft > chorus |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.6 | 0 | ok |
| v4.echo_imprint | spectral_graft > spectral_delay | noise | -34.7 | -240.0 | -41.8 | -99.00 | +6.4 | 0 | ok |
| v4.prism_delay | spectral_delay > dispersion | noise | -19.1 | -240.0 | -40.8 | -99.00 | -7.2 | 0 | ok |
| v4.partial_drift | partial_recompose > spectral_smear | noise | -17.3 | -60.3 | -1.3 | -99.00 | +1.0 | 0 | ok |
| v4.laser_chirp | dispersion > compressor |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.6 | 0 | ok |
| v4.spring_vortex | dispersion > echo_matrix | drums | -13.3 | -81.0 | -14.8 | -99.00 | -0.7 | 0 | ok |
| cd.lofi_sampler | degrade > tape_colour | impulse | -37.8 | -86.2 | -48.2 | -99.00 | -1.8 | 0 | ok |
| g3.alias_grinder | degrade > eq |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.6 | 0 | ok |
| cd.cassette | degrade > vibrato > compressor | impulse | -37.7 | -55.3 | -17.6 | -99.00 | -4.1 | 0 | ok |
| ton.fold_space | fold_crush > filter |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.9 | 0 | ok |
| g3.splinter_hits | fold_crush > compressor |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.4 | 0 | ok |
| cd.blown_speaker | degrade > amp > saturation |  | -240.0 | -240.0 | -200.0 | -99.00 | -4.5 | 0 | ok |
| g3.bitrot_echo | delay > bitcrush | chord | -11.4 | -240.0 | -12.4 | -99.00 | -1.7 | 0 | ok |
| g3.bitrot_echo @macros100 | delay > bitcrush | chord | -12.7 | -49.2 | -7.7 | -99.00 | -1.7 | 0 | ok |
| g3.bitrot_echo @sweep | delay > bitcrush | chord | -12.0 | -48.9 | -8.3 | -99.00 | -1.6 | 0 | ok |
| v4.tape_memory | time_warp > tape_colour > vibrato > texture | drums | -240.0 | -240.0 | +169.3 | -99.00 | -0.1 | 0 | ok |
| v4.vinyl_ghost | texture > degrade | noise | -15.8 | -240.0 | -18.4 | -99.00 | +2.3 | 0 | ok |
| v4.friction_drive | texture > fold_crush |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.0 | 0 | ok |
| v4.grit_imprint | texture > bitcrush |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.1 | 0 | ok |
| v4.static_storm | texture > degrade | noise | -13.3 | -240.0 | -15.8 | -99.00 | -1.6 | 0 | ok |
| v4.cassette_choir | doubler > degrade |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.3 | 0 | ok |
| v4.dirty_room | compressor > bitcrush > reverb > texture | noise | -18.6 | -240.0 | -37.8 | -99.00 | +1.4 | 0 | ok |
| v4.silver_thread | grain_engine > reverb | chord | -10.8 | -240.0 | -28.5 | -99.00 | -0.4 | 0 | ok |
| v4.silver_thread @macros100 | grain_engine > reverb | chord | -10.6 | -240.0 | -16.0 | -99.00 | -1.2 | 0 | ok |
| v4.silver_thread @sweep | grain_engine > reverb | chord | -10.7 | -240.0 | -17.3 | -99.00 | -0.7 | 0 | ok |
| v4.soft_focus | dispersion > width > reverb | chord | -16.4 | -240.0 | -59.9 | -99.00 | -3.1 | 0 | ok |
| v4.soft_focus @macros100 | dispersion > width > reverb | chord | -15.4 | -240.0 | -58.2 | -99.00 | -3.7 | 0 | ok |
| v4.soft_focus @sweep | dispersion > width > reverb | chord | -14.8 | -240.0 | -59.6 | -99.00 | -3.3 | 0 | ok |
| v4.halo_chords | echo_matrix > chorus > reverb | drums | -14.1 | -67.3 | -6.0 | -99.00 | -2.5 | 0 | ok |
| v4.halo_chords @macros100 | echo_matrix > chorus > reverb | drums | -14.3 | -43.1 | -2.2 | -99.00 | +0.1 | 0 | ok |
| v4.halo_chords @sweep | echo_matrix > chorus > reverb | drums | -14.1 | -43.2 | -2.4 | -99.00 | -2.5 | 0 | ok |
| v4.air_between_notes | delay > reverb | chord | -10.7 | -240.0 | -27.5 | -99.00 | +0.2 | 0 | ok |
| v4.air_between_notes @macros100 | delay > reverb | chord | -1.1 | -111.6 | -10.3 | -99.00 | +5.6 | 0 | ok |
| v4.air_between_notes @sweep | delay > reverb | chord | -6.0 | -111.9 | -6.4 | -99.00 | +2.1 | 0 | ok |
| v4.infinite_instrument | partial_recompose > modal > reverb | chord | -21.9 | -64.8 | -2.7 | -99.00 | -0.3 | 0 | ok |
| v4.infinite_instrument @macros100 | partial_recompose > modal > reverb | chord | -23.9 | -65.9 | -1.5 | -99.00 | -3.2 | 0 | ok |
| v4.infinite_instrument @sweep | partial_recompose > modal > reverb | chord | -22.6 | -65.9 | -2.8 | -99.00 | -1.9 | 0 | ok |
| v4.memory_bloom | grain_engine > granular > eq | noise | -18.0 | -240.0 | -0.6 | -99.00 | -0.8 | 0 | ok |
| v4.glass_room | convolution > doubler > modal | chord | -16.2 | -240.0 | -52.9 | -99.00 | -5.3 | 0 | ok |
| v4.glass_room @macros100 | convolution > doubler > modal | chord | -19.8 | -240.0 | -45.2 | -99.00 | -11.8 | 0 | ok |
| v4.glass_room @sweep | convolution > doubler > modal | chord | -17.3 | -240.0 | -47.7 | -99.00 | -7.3 | 0 | ok |
| v4.warm_echo_hall | tape_echo > reverb | chord | -11.2 | -240.0 | -26.7 | -99.00 | -1.3 | 0 | ok |
| v4.warm_echo_hall @macros100 | tape_echo > reverb | chord | -4.4 | -97.4 | -10.4 | -99.00 | +4.7 | 0 | ok |
| v4.warm_echo_hall @sweep | tape_echo > reverb | chord | -6.1 | -97.7 | -9.8 | -99.00 | +2.5 | 0 | ok |
| v4.bell_garden | modal > shimmer | chord | -16.4 | -159.3 | -30.2 | -99.00 | -4.0 | 0 | ok |
| v4.bell_garden @macros100 | modal > shimmer | drums | -17.5 | -93.9 | -14.6 | -99.00 | -7.6 | 0 | ok |
| v4.bell_garden @sweep | modal > shimmer | drums | -16.3 | -91.8 | -14.7 | -99.00 | -6.1 | 0 | ok |
| v4.silk_doubles | doubler > exciter > reverb | chord | -14.7 | -240.0 | -62.4 | -99.00 | -5.7 | 0 | ok |
| v4.lullaby_cloud | granular > reverb | chord | -11.5 | -240.0 | -16.6 | -99.00 | -0.6 | 0 | ok |
| v4.lullaby_cloud @macros100 | granular > reverb | chord | -11.3 | -240.0 | -11.1 | -99.00 | -1.3 | 0 | ok |
| v4.lullaby_cloud @sweep | granular > reverb | chord | -10.9 | -240.0 | -11.1 | -99.00 | -0.9 | 0 | ok |
| v4.velvet_motion | phaser > chorus > tremolo |  | -240.0 | -240.0 | -200.0 | -99.00 | -2.2 | 0 | ok |
| v5.blue_hour | reflections > chorus |  | -240.0 | -240.0 | -200.0 | -99.00 | -4.6 | 0 | ok |
| v5.blue_hour @macros100 | reflections > chorus |  | -240.0 | -240.0 | -200.0 | -99.00 | -9.8 | 0 | ok |
| v5.blue_hour @sweep | reflections > chorus |  | -240.0 | -240.0 | -200.0 | -99.00 | -7.1 | 0 | ok |
| v5.soft_focus | swell > cloud_chamber | chord | -11.7 | -240.0 | -31.3 | -99.00 | -3.6 | 0 | ok |
| atm.glass_cathedral | shimmer > width | noise | -28.1 | -115.3 | -12.8 | -99.00 | -11.3 | 0 | ok |
| atm.slow_bloom | reverse > reverb | noise | -22.8 | -113.3 | -0.2 | -99.00 | -3.6 | 0 | ok |
| atm.deep_hollow | pitch_shift > reverb > eq | chord | -4.2 | -45.9 | -4.8 | -99.00 | +6.8 | 0 | ok |
| atm.deep_hollow @macros100 | pitch_shift > reverb > eq | chord | -5.5 | -41.1 | -4.4 | -99.00 | +3.9 | 0 | ok |
| atm.deep_hollow @sweep | pitch_shift > reverb > eq | chord | -5.7 | -39.9 | -5.1 | -99.00 | +5.2 | 0 | ok |
| sig.doppler_drift | doppler > reverb | chord | -14.4 | -203.1 | -24.4 | -99.00 | -5.1 | 0 | ok |
| atm.under_ice | filter > spectral_smear > convolution | noise | -36.9 | -106.1 | -3.0 | -99.00 | -8.6 | 0 | ok |
| atm.under_ice @macros100 | filter > spectral_smear > convolution | noise | -46.4 | -73.5 | -0.7 | -99.00 | -20.1 | 0 | ok |
| atm.under_ice @sweep | filter > spectral_smear > convolution | noise | -43.0 | -70.1 | -0.8 | -99.00 | -17.1 | 0 | ok |
| c3.horizon_freeze | grain_engine > reverb | chord | -240.0 | -240.0 | +167.8 | -99.00 | -1.6 | 0 | ok |
| c3.horizon_freeze @macros100 | grain_engine > reverb | chord | -240.0 | -240.0 | +157.7 | -99.00 | -13.5 | 0 | ok |
| c3.horizon_freeze @sweep | grain_engine > reverb | chord | -240.0 | -240.0 | +158.6 | -99.00 | -12.3 | 0 | ok |
| v4.monolith | modal > spectral > reverb | drums | -15.3 | -27.8 | -9.5 | -0.05 | -4.9 | 0 | hold (intended) |
| v4.monolith @macros100 | modal > spectral > reverb | noise | -29.7 | -38.2 | -0.7 | -0.51 | -9.2 | 0 | LONG |
| v4.monolith @sweep | modal > spectral > reverb | chord | -16.8 | -36.8 | -2.8 | -0.50 | -6.7 | 0 | LONG |
| v4.reverse_dawn | reverse > reverse_echo > reverb | chord | -11.0 | -99.0 | -1.5 | -99.00 | -1.2 | 0 | ok |
| v4.reverse_dawn @macros100 | reverse > reverse_echo > reverb | chord | -13.8 | -62.1 | +6.0 | -99.00 | +1.5 | 0 | ok |
| v4.reverse_dawn @sweep | reverse > reverse_echo > reverb | chord | -12.3 | -62.4 | +3.4 | -99.00 | +0.6 | 0 | ok |
| v4.tidal_body | spectral_smear > dispersion | noise | -37.0 | -79.1 | -3.2 | -99.00 | -9.5 | 0 | ok |
| v4.trailer_hit | transient > pitch_shift > modal > reverb | noise | -18.7 | -113.4 | -18.4 | -99.00 | +12.0 | 0 | ok |
| v4.trailer_hit @macros100 | transient > pitch_shift > modal > reverb | noise | -22.5 | -84.5 | -8.9 | -99.00 | +9.8 | 0 | ok |
| v4.trailer_hit @sweep | transient > pitch_shift > modal > reverb | noise | -19.5 | -85.6 | -13.0 | -99.00 | +12.0 | 0 | ok |
| v4.ghost_station | degrade > texture > echo_matrix > reverb | drums | -22.8 | -46.7 | -12.1 | -0.76 | -4.9 | 0 | LONG |
| v4.ghost_station @macros100 | degrade > texture > echo_matrix > reverb | impulse | -37.8 | -46.5 | -8.6 | +0.10 | -4.2 | 0 | hold (intended) |
| v4.ghost_station @sweep | degrade > texture > echo_matrix > reverb | impulse | -37.8 | -46.5 | -8.6 | +0.09 | -3.8 | 0 | hold (intended) |
| v4.slow_planet | grain_engine > spectral_delay > reverb | chord | -240.0 | -223.4 | +167.7 | -99.00 | -2.3 | 0 | ok |
| v4.slow_planet @macros100 | grain_engine > spectral_delay > reverb | chord | -193.9 | -135.0 | +169.4 | -99.00 | -1.3 | 0 | ok |
| v4.slow_planet @sweep | grain_engine > spectral_delay > reverb | chord | -192.1 | -145.7 | +167.9 | -99.00 | -2.8 | 0 | ok |
| v4.resonant_sky | modal > shimmer | chord | -14.1 | -152.7 | -25.7 | -99.00 | -3.7 | 0 | ok |
| v4.resonant_sky @macros100 | modal > shimmer | noise | -23.3 | -118.3 | -15.8 | -99.00 | -9.3 | 0 | ok |
| v4.resonant_sky @sweep | modal > shimmer | noise | -18.1 | -117.3 | -19.4 | -99.00 | -5.4 | 0 | ok |
| v4.underworld | reverse_echo > dispersion > fold_crush | chord | -12.9 | -240.0 | -29.2 | -99.00 | -4.5 | 0 | ok |
| v5.soft_horizon | cloud_chamber > width | chord | -12.3 | -128.2 | -14.6 | -99.00 | -1.2 | 0 | ok |
| v5.pale_chamber | reflections > cloud_chamber | chord | -15.9 | -240.0 | -62.8 | -99.00 | -3.8 | 0 | ok |
| v5.pale_chamber @macros100 | reflections > cloud_chamber | chord | -18.9 | -240.0 | -42.4 | -99.00 | -7.8 | 0 | ok |
| v5.pale_chamber @sweep | reflections > cloud_chamber | chord | -17.2 | -240.0 | -45.3 | -99.00 | -6.2 | 0 | ok |
| v5.velvet_drift | chorus > cloud_chamber | chord | -13.1 | -66.9 | -8.2 | -99.00 | -2.2 | 0 | ok |
| v5.velvet_drift @macros100 | chorus > cloud_chamber | chord | -17.5 | -64.5 | -4.4 | -99.00 | -8.0 | 0 | ok |
| v5.velvet_drift @sweep | chorus > cloud_chamber | chord | -15.2 | -65.1 | -7.5 | -99.00 | -3.9 | 0 | ok |
| v5.glass_bloom | shimmer > cloud_chamber | chord | -12.0 | -125.4 | -18.2 | -99.00 | -0.9 | 0 | ok |
| v5.glass_bloom @macros100 | shimmer > cloud_chamber | chord | -11.6 | -75.8 | -7.1 | -99.00 | -1.3 | 0 | ok |
| v5.glass_bloom @sweep | shimmer > cloud_chamber | chord | -11.3 | -76.4 | -7.5 | -99.00 | -1.5 | 0 | ok |
| v5.frozen_orchard | spectral_smear > cloud_chamber | chord | -18.5 | -71.5 | -9.8 | -99.00 | -7.2 | 0 | ok |
| v5.frozen_orchard @macros100 | spectral_smear > cloud_chamber | chord | -21.7 | -63.9 | -6.2 | -99.00 | -14.2 | 0 | ok |
| v5.frozen_orchard @sweep | spectral_smear > cloud_chamber | chord | -20.6 | -63.4 | -6.8 | -99.00 | -12.1 | 0 | ok |
| v5.reverse_weather | reverse_bloom > width | chord | -11.5 | -181.2 | -3.1 | -99.00 | -1.6 | 0 | ok |
| v5.deep_tide | eq > cloud_chamber | chord | -13.2 | -70.8 | -7.7 | -99.00 | -2.0 | 0 | ok |
| v5.violet_relay | delay > cloud_chamber | chord | -11.9 | -122.6 | -13.8 | -99.00 | -1.1 | 0 | ok |
| v5.violet_relay @macros100 | delay > cloud_chamber | chord | -8.1 | -63.6 | -5.0 | -99.00 | +0.6 | 0 | ok |
| v5.violet_relay @sweep | delay > cloud_chamber | chord | -8.0 | -63.5 | -5.0 | -99.00 | +0.5 | 0 | ok |
| v5.cloud_memory | granular > reverb | chord | -13.0 | -240.0 | -5.4 | -99.00 | -2.5 | 0 | ok |
| v5.cloud_memory @macros100 | granular > reverb | noise | -19.2 | -240.0 | -5.2 | -99.00 | -2.7 | 0 | ok |
| v5.cloud_memory @sweep | granular > reverb | chord | -13.2 | -240.0 | -6.0 | -99.00 | -1.3 | 0 | ok |
| v5.small_room_huge_sky | reflections > cloud_chamber | chord | -19.1 | -72.1 | -6.0 | -99.00 | -1.5 | 0 | ok |
| v5.small_room_huge_sky @macros100 | reflections > cloud_chamber | noise | -24.2 | -83.2 | -5.3 | -99.00 | -9.4 | 0 | ok |
| v5.small_room_huge_sky @sweep | reflections > cloud_chamber | chord | -14.6 | -60.9 | -7.3 | -99.00 | -5.9 | 0 | ok |
| v5.suspended_bells | modal > cloud_chamber | chord | -11.5 | -128.9 | -21.9 | -99.00 | -3.1 | 0 | ok |
| v5.suspended_bells @macros100 | modal > cloud_chamber | drums | -21.0 | -65.6 | -8.3 | -99.00 | -3.5 | 0 | ok |
| v5.suspended_bells @sweep | modal > cloud_chamber | chord | -11.4 | -73.6 | -11.1 | -99.00 | -2.9 | 0 | ok |
| v5.afterimage_choir | pitch_shift > shimmer | chord | -14.3 | -139.4 | -23.3 | -99.00 | -3.0 | 0 | ok |
| v5.afterimage_choir @macros100 | pitch_shift > shimmer | noise | -20.6 | -117.8 | -18.1 | -99.00 | -6.7 | 0 | ok |
| v5.afterimage_choir @sweep | pitch_shift > shimmer | noise | -17.8 | -116.3 | -20.5 | -99.00 | -3.1 | 0 | ok |
| v5.dust_cathedral | texture > reverb | chord | -9.1 | -91.3 | -9.1 | -99.00 | +2.1 | 0 | ok |
| v5.slow_glass | spectral_smear > cloud_chamber | chord | -16.2 | -112.0 | -15.7 | -99.00 | -2.3 | 0 | ok |
| v5.slow_glass @macros100 | spectral_smear > cloud_chamber | chord | -19.4 | -68.0 | -10.6 | -99.00 | -9.3 | 0 | ok |
| v5.slow_glass @sweep | spectral_smear > cloud_chamber | chord | -17.5 | -65.3 | -11.8 | -99.00 | -6.6 | 0 | ok |
| v5.rain_in_reverse | reverse > cloud_chamber | chord | -11.2 | -178.5 | -25.1 | -99.00 | -0.5 | 0 | ok |
| v5.infinite_slope | cloud_chamber > eq | chord | -12.4 | -40.1 | -7.7 | -99.00 | -2.0 | 0 | ok |
| v5.empty_cinema | reflections > reverb | chord | -15.8 | -205.8 | -24.7 | -99.00 | -4.9 | 0 | ok |
| v5.empty_cinema @macros100 | reflections > reverb | chord | -21.6 | -204.0 | -16.1 | -99.00 | -9.8 | 0 | ok |
| v5.empty_cinema @sweep | reflections > reverb | chord | -10.7 | -197.8 | -23.6 | -99.00 | -1.0 | 0 | ok |
| v5.swell_choir | swell > shimmer | chord | -7.7 | -117.1 | -23.6 | -99.00 | +0.8 | 0 | ok |
| v5.inhale | reverse_bloom > drum_space | chord | -13.8 | -240.0 | -30.8 | -99.00 | -3.6 | 0 | ok |
| v5.inhale @macros100 | reverse_bloom > drum_space | chord | -19.3 | -189.1 | -10.8 | -99.00 | -8.2 | 0 | ok |
| v5.inhale @sweep | reverse_bloom > drum_space | chord | -15.0 | -189.6 | -15.5 | -99.00 | -5.7 | 0 | ok |
| v5.pocket_machine | pulse_shaper > tape_colour |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.7 | 0 | ok |
| v5.snare_cathedral | drum_space > cloud_chamber | chord | -16.8 | -205.4 | -21.9 | -99.00 | -5.0 | 0 | ok |
| v5.snare_cathedral @macros100 | drum_space > cloud_chamber | chord | -18.9 | -86.5 | -5.9 | -99.00 | -9.7 | 0 | ok |
| v5.snare_cathedral @sweep | drum_space > cloud_chamber | chord | -16.9 | -85.4 | -6.0 | -99.00 | -6.7 | 0 | ok |
| v5.dust_break | slice_shuffle > degrade > delay | noise | -77.7 | -77.6 | +57.4 | -99.00 | -4.8 | 0 | ok |
| v5.dust_break @macros100 | slice_shuffle > degrade > delay | noise | -75.2 | -75.4 | +54.3 | -99.00 | -2.9 | 0 | ok |
| v5.dust_break @sweep | slice_shuffle > degrade > delay | noise | -75.5 | -75.3 | +54.7 | -99.00 | -3.0 | 0 | ok |
| v5.ghost_hats | pulse_shaper > width |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.1 | 0 | ok |
| v5.concrete_room | transient > drum_space | chord | -7.3 | -240.0 | -152.7 | -99.00 | +12.0 | 0 | ok |
| v5.rubber_kit | drum_crush > transient |  | -240.0 | -240.0 | -200.0 | -99.00 | +10.9 | 0 | ok |
| v5.rubber_kit @macros100 | drum_crush > transient |  | -240.0 | -240.0 | -200.0 | -99.00 | +12.0 | 0 | ok |
| v5.rubber_kit @sweep | drum_crush > transient |  | -240.0 | -240.0 | -200.0 | -99.00 | +12.0 | 0 | ok |
| v5.half_life_break | time_warp > band_transients |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.0 | 0 | ok |
| v5.metal_skin | modal > band_transients | chord | -11.5 | -240.0 | -119.1 | -99.00 | -0.2 | 0 | ok |
| v5.broken_pocket | pulse_shaper > drum_space |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.1 | 0 | ok |
| v5.broken_pocket @macros100 | pulse_shaper > drum_space |  | -240.0 | -240.0 | -200.0 | -99.00 | -7.3 | 0 | ok |
| v5.broken_pocket @sweep | pulse_shaper > drum_space |  | -240.0 | -240.0 | -200.0 | -99.00 | -7.3 | 0 | ok |
| v5.drum_orbit | pulse_shaper > delay | chord | -15.3 | -240.0 | -35.1 | -99.00 | -3.5 | 0 | ok |
| v5.drum_orbit @macros100 | pulse_shaper > delay | noise | -17.0 | -138.0 | -25.6 | -99.00 | -5.5 | 0 | ok |
| v5.drum_orbit @sweep | pulse_shaper > delay | chord | -15.4 | -130.5 | -23.6 | -99.00 | -4.5 | 0 | ok |
| v5.hard_soft | band_transients > drum_crush |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.5 | 0 | ok |
| v5.hard_soft @macros100 | band_transients > drum_crush |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.8 | 0 | ok |
| v5.hard_soft @sweep | band_transients > drum_crush |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.3 | 0 | ok |
| v5.salted_cymbals | spectral_smear > texture | chord | -16.3 | -95.2 | -18.1 | -99.00 | -2.2 | 0 | ok |
| v5.tape_room | tape_colour > vibrato > drum_space | chord | -15.8 | -240.0 | -167.3 | -99.00 | -4.3 | 0 | ok |
| v5.kick_lantern | band_transients > saturation |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.5 | 0 | ok |
| v5.shattered_snare | glitch > band_transients | drums | -8.8 | -240.0 | -23.9 | -99.00 | -0.0 | 0 | ok |
| v5.deep_dub_kit | tape_echo > drum_space | chord | -15.1 | -69.3 | -9.7 | -99.00 | -3.1 | 0 | ok |
| v5.deep_dub_kit @macros100 | tape_echo > drum_space | chord | -18.0 | -39.7 | -5.2 | -0.87 | -6.4 | 0 | LONG |
| v5.deep_dub_kit @sweep | tape_echo > drum_space | chord | -15.7 | -39.8 | -7.5 | -0.87 | -4.4 | 0 | LONG |
| v5.glass_hats | modal > pulse_shaper |  | -240.0 | -240.0 | -200.0 | -99.00 | -6.4 | 0 | ok |
| v5.crushed_but_clear | drum_crush > band_transients |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.0 | 0 | ok |
| v5.crushed_but_clear @macros100 | drum_crush > band_transients |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.0 | 0 | ok |
| v5.crushed_but_clear @sweep | drum_crush > band_transients |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.0 | 0 | ok |
| v5.offbeat_machinery | pulse_shaper > reflections |  | -240.0 | -240.0 | -200.0 | -99.00 | -8.7 | 0 | ok |
| v5.offbeat_machinery @macros100 | pulse_shaper > reflections | noise | -240.0 | -240.0 | -60.0 | -99.00 | -12.0 | 0 | ok |
| v5.offbeat_machinery @sweep | pulse_shaper > reflections | drums | -40.9 | -240.0 | -199.1 | -99.00 | -7.5 | 0 | ok |
| v5.last_hit_in_the_hall | drum_space > shimmer | chord | -14.8 | -172.9 | -27.9 | -99.00 | -3.4 | 0 | ok |
| v5.last_hit_in_the_hall @macros100 | drum_space > shimmer | chord | -16.6 | -115.4 | -16.6 | -99.00 | -7.0 | 0 | ok |
| v5.last_hit_in_the_hall @sweep | drum_space > shimmer | chord | -15.0 | -113.8 | -19.0 | -99.00 | -4.8 | 0 | ok |
| v5.gated_booth | drum_space > transient |  | -240.0 | -240.0 | -200.0 | -99.00 | +7.9 | 0 | ok |
| v5.amp_shimmer | tremolo > saturation |  | -240.0 | -240.0 | -200.0 | -99.00 | -2.0 | 0 | ok |
| v5.slow_breath | tremolo > cloud_chamber | chord | -14.0 | -182.9 | -25.4 | -99.00 | -1.5 | 0 | ok |
| v5.syncopated_chords | pulse_shaper > delay | chord | -13.8 | -240.0 | -45.0 | -99.00 | -2.4 | 0 | ok |
| v5.syncopated_chords @macros100 | pulse_shaper > delay | chord | -13.8 | -194.1 | -22.7 | -99.00 | -3.7 | 0 | ok |
| v5.syncopated_chords @sweep | pulse_shaper > delay | chord | -13.7 | -194.2 | -22.8 | -99.00 | -3.7 | 0 | ok |
| v5.chopped_voice | pulse_shaper > reflections |  | -240.0 | -240.0 | -200.0 | -99.00 | -5.8 | 0 | ok |
| v5.rapid_gate | pulse_shaper > saturation |  | -240.0 | -240.0 | -200.0 | -99.00 | -2.9 | 0 | ok |
| v5.swing_shaker | pulse_shaper > reflections |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.4 | 0 | ok |
| v5.swing_shaker @macros100 | pulse_shaper > reflections |  | -240.0 | -240.0 | -200.0 | -99.00 | -6.7 | 0 | ok |
| v5.swing_shaker @sweep | pulse_shaper > reflections |  | -240.0 | -240.0 | -200.0 | -99.00 | -5.0 | 0 | ok |
| v5.left_right_percussion | pulse_shaper > drum_space | chord | -18.6 | -240.0 | -184.5 | -99.00 | -4.4 | 0 | ok |
| v5.left_right_percussion @macros100 | pulse_shaper > drum_space | chord | -23.1 | -240.0 | -175.7 | -99.00 | -8.9 | 0 | ok |
| v5.left_right_percussion @sweep | pulse_shaper > drum_space | chord | -20.7 | -240.0 | -177.7 | -99.00 | -6.1 | 0 | ok |
| v5.polyrhythm_drift | pulse_shaper > tremolo |  | -240.0 | -240.0 | -200.0 | -99.00 | -2.0 | 0 | ok |
| v5.hit_opener | pulse_shaper > reflections |  | -240.0 | -240.0 | -200.0 | -99.00 | -4.0 | 0 | ok |
| v5.hit_opener @macros100 | pulse_shaper > reflections |  | -240.0 | -240.0 | -200.0 | -99.00 | -9.0 | 0 | ok |
| v5.hit_opener @sweep | pulse_shaper > reflections |  | -240.0 | -240.0 | -200.0 | -99.00 | -4.7 | 0 | ok |
| v5.tremolo_into_space | tremolo > cloud_chamber | chord | -14.6 | -148.9 | -19.4 | -99.00 | -2.9 | 0 | ok |
| v5.space_then_chop | cloud_chamber > pulse_shaper | chord | -13.1 | -103.5 | -8.6 | -99.00 | -3.8 | 0 | ok |
| v5.space_then_chop @macros100 | cloud_chamber > pulse_shaper | chord | -18.3 | -106.5 | -1.8 | -99.00 | -10.2 | 0 | ok |
| v5.space_then_chop @sweep | cloud_chamber > pulse_shaper | chord | -15.9 | -105.5 | -3.6 | -99.00 | -6.5 | 0 | ok |
| v5.tremolo_echo | tremolo > delay | chord | -15.1 | -240.0 | -25.6 | -99.00 | -1.2 | 0 | ok |
| v5.tremolo_echo @macros100 | tremolo > delay | chord | -12.9 | -107.0 | -14.1 | -99.00 | -1.2 | 0 | ok |
| v5.tremolo_echo @sweep | tremolo > delay | chord | -13.9 | -105.3 | -12.5 | -99.00 | -1.3 | 0 | ok |
| v5.low_pump | pulse_shaper > saturation |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.0 | 0 | ok |
| v5.broken_machine_pulse | pulse_shaper > fold_crush |  | -240.0 | -240.0 | -200.0 | -99.00 | -7.4 | 0 | ok |
| v5.wurly_wave | tremolo > tape_colour |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.5 | 0 | ok |
| v5.gallop_pad | pulse_shaper > reverb | chord | -13.7 | -240.0 | -34.5 | -99.00 | -2.7 | 0 | ok |
| v5.gallop_pad @macros100 | pulse_shaper > reverb | chord | -12.7 | -240.0 | -32.6 | -99.00 | -2.7 | 0 | ok |
| v5.gallop_pad @sweep | pulse_shaper > reverb | chord | -12.6 | -240.0 | -31.3 | -99.00 | -3.2 | 0 | ok |
| v5.halftime_sway | pulse_shaper > tape_colour |  | -240.0 | -240.0 | -200.0 | -99.00 | -4.6 | 0 | ok |
| ex.inverted_world | grain_engine > spectral_delay > fold_crush | chord | -17.1 | -240.0 | -21.6 | -99.00 | -9.6 | 0 | ok |
| h3.machine_dream | glitch > spectral_smear | chord | -18.6 | -81.0 | -7.2 | -99.00 | -9.6 | 0 | ok |
| h3.broken_tape_choir | harmony > time_warp > degrade | impulse | -37.8 | -75.9 | -38.0 | -99.00 | +0.5 | 0 | ok |
| h3.particle_engine | grain_engine > ring_mod > spectral_delay | noise | -25.9 | -240.0 | -0.2 | -99.00 | -6.2 | 0 | ok |
| h3.rubber_room | time_warp > fold_crush > spectral_delay | chord | -16.3 | -240.0 | -51.7 | -99.00 | -5.5 | 0 | ok |
| h3.tidal_memory | grain_engine > degrade > reverb | chord | -240.0 | -240.0 | +167.4 | -99.00 | -2.9 | 0 | ok |
| h3.tidal_memory @macros100 | grain_engine > degrade > reverb | chord | -240.0 | -240.0 | +168.6 | -99.00 | -0.9 | 0 | ok |
| h3.tidal_memory @sweep | grain_engine > degrade > reverb | chord | -240.0 | -240.0 | +168.6 | -99.00 | -2.0 | 0 | ok |
| v4.modal_rebuild | slice_shuffle > modal | noise | -20.3 | -240.0 | +1.1 | -99.00 | -5.8 | 0 | ok |
| v4.graft_cascade | spectral_graft > reverse_echo | chord | -11.6 | -240.0 | -20.6 | -99.00 | +7.6 | 0 | ok |
| v4.split_world | transient_split > spectral_shatter | noise | -13.6 | -240.0 | -62.0 | -99.00 | -0.0 | 0 | ok |
| v4.texture_reactor | texture > ring_mod > fold_crush | noise | -18.1 | -240.0 | -24.3 | -99.00 | -2.7 | 0 | ok |
| v4.echo_lattice | echo_matrix > modal > dispersion | chord | -10.9 | -145.6 | -14.6 | -99.00 | -1.2 | 0 | ok |
| v4.partial_glitch | partial_recompose > glitch |  | -240.0 | -240.0 | -200.0 | -99.00 | -5.4 | 0 | ok |
| v4.dust_choir | partial_recompose > texture > reverb | noise | -13.5 | -50.4 | -3.7 | -99.00 | +5.1 | 0 | ok |
| v5.distant_machinery | modal > echo_matrix > reverb | chord | -10.9 | -83.8 | -13.0 | -99.00 | +0.8 | 0 | ok |
| v5.distant_machinery @macros100 | modal > echo_matrix > reverb | chord | -12.3 | -82.0 | -9.0 | -99.00 | -1.3 | 0 | ok |
| v5.distant_machinery @sweep | modal > echo_matrix > reverb | chord | -11.9 | -81.7 | -9.5 | -99.00 | +0.3 | 0 | ok |
| v5.hit_melody_kit | hit_melody > reflections | drums | -18.0 | -240.0 | -190.2 | -99.00 | -7.4 | 0 | ok |
| v5.hit_melody_kit @macros100 | hit_melody > reflections | drums | -23.5 | -240.0 | -184.0 | -99.00 | -13.2 | 0 | ok |
| v5.hit_melody_kit @sweep | hit_melody > reflections | drums | -20.9 | -240.0 | -186.6 | -99.00 | -9.0 | 0 | ok |

**354 cases: 1 GROW / FAULT, 0 unexplained HOLD.**
