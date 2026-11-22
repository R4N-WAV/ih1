# IH1 feedback audit

Tail 14 s after each source; growth = loudest 100 ms window from +1 s to the end vs the first second after the excitation.

## FX types, recursive controls at the legal maximum (Output Safety off)

| FX / draft | Raised controls | Worst source | ref dBFS | late dBFS | growth | slope dB/s | peak | faults | verdict |
|---|---|---|---|---|---|---|---|---|---|
| delay max | feedback | chord | -10.2 | -26.9 | -8.4 | -0.69 | -0.6 | 0 | LONG |
| delay rnd1 | feedback | chord | -12.3 | -43.4 | +0.8 | -99.00 | +1.0 | 0 | ok |
| delay rnd2 | feedback | chord | -12.4 | -66.5 | -11.3 | -99.00 | -0.9 | 0 | ok |
| delay rnd3 | feedback | noise | -12.0 | -240.0 | -17.6 | -99.00 | -0.9 | 0 | ok |
| delay rnd4 | feedback | noise | -14.6 | -240.0 | -17.3 | -99.00 | -0.9 | 0 | ok |
| delay rnd5 | feedback | noise | -16.3 | -44.2 | +0.1 | -99.00 | -0.9 | 0 | ok |
| delay rnd6 | feedback | chord | -13.3 | -45.3 | -2.3 | -99.00 | -0.9 | 0 | ok |
| tape_echo max | (none) | chord | -11.3 | -240.0 | -33.1 | -99.00 | -0.9 | 0 | ok |
| tape_echo rnd1 | (none) | chord | -6.4 | -98.6 | -0.4 | -99.00 | -0.2 | 0 | ok |
| tape_echo rnd2 | (none) | drums | -3.3 | -29.9 | +0.0 | -99.00 | +6.1 | 0 | ok |
| tape_echo rnd3 | (none) | noise | -19.9 | -50.1 | -9.8 | -99.00 | -0.9 | 0 | ok |
| tape_echo rnd4 | (none) | noise | -25.3 | -44.8 | +12.3 | -99.00 | -0.0 | 0 | ok |
| tape_echo rnd5 | (none) | noise | -22.6 | -22.2 | +1.1 | -0.01 | -0.9 | 0 | hold (intended) |
| tape_echo rnd6 | (none) | noise | -21.3 | -83.8 | +0.3 | -99.00 | -0.9 | 0 | ok |
| reverb max | decay,size | chord | -10.6 | -37.5 | -4.8 | -99.00 | -0.9 | 0 | ok |
| reverb rnd1 | decay,size | noise | -16.8 | -55.0 | -15.2 | -99.00 | -0.9 | 0 | ok |
| reverb rnd2 | decay,size | chord | -13.9 | -240.0 | -9.6 | -99.00 | -0.9 | 0 | ok |
| reverb rnd3 | decay,size | chord | -15.8 | -45.1 | -6.1 | -99.00 | -0.9 | 0 | ok |
| reverb rnd4 | decay,size | noise | -13.8 | -48.5 | -12.7 | -99.00 | -0.9 | 0 | ok |
| reverb rnd5 | decay,size | chord | -23.3 | -57.6 | -6.5 | -99.00 | -0.9 | 0 | ok |
| reverb rnd6 | decay,size | drums | -11.7 | -37.3 | -4.2 | -99.00 | -0.3 | 0 | ok |
| reverse max | feedback,density | chord | -8.4 | -107.8 | -12.1 | -99.00 | +1.8 | 0 | ok |
| reverse rnd1 | feedback,density | chord | -10.6 | -40.4 | -3.2 | -99.00 | +0.1 | 0 | ok |
| reverse rnd2 | feedback,density | drums | -11.3 | -25.6 | -1.8 | -1.14 | +1.7 | 0 | LONG |
| reverse rnd3 | feedback,density | chord | -13.0 | -21.2 | -1.6 | -0.67 | -0.3 | 0 | LONG |
| reverse rnd4 | feedback,density | chord | -9.5 | -90.8 | -2.9 | -99.00 | +2.1 | 0 | ok |
| reverse rnd5 | feedback,density | noise | -9.2 | -176.8 | -17.3 | -99.00 | +4.2 | 0 | ok |
| reverse rnd6 | feedback,density | chord | -12.4 | -58.3 | -3.4 | -99.00 | -0.9 | 0 | ok |
| shimmer max | decay,size,regen | chord | -9.4 | -59.9 | -12.4 | -99.00 | +0.5 | 0 | ok |
| shimmer rnd1 | decay,size,regen | chord | -11.4 | -63.9 | -7.2 | -99.00 | -0.9 | 0 | ok |
| shimmer rnd2 | decay,size,regen | chord | -11.6 | -60.1 | -8.0 | -99.00 | -0.9 | 0 | ok |
| shimmer rnd3 | decay,size,regen | chord | -12.8 | -76.1 | -10.9 | -99.00 | -0.9 | 0 | ok |
| shimmer rnd4 | decay,size,regen | chord | -13.4 | -68.1 | -12.5 | -99.00 | -0.9 | 0 | ok |
| shimmer rnd5 | decay,size,regen | chord | -13.2 | -64.2 | -13.7 | -99.00 | -0.9 | 0 | ok |
| shimmer rnd6 | decay,size,regen | chord | -5.1 | -36.0 | -2.7 | -99.00 | +4.0 | 0 | ok |
| convolution max | (none) | chord | -13.0 | -240.0 | -142.6 | -99.00 | -1.3 | 0 | ok |
| convolution rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.8 | 0 | ok |
| convolution rnd2 | (none) | chord | -11.3 | -240.0 | -26.9 | -99.00 | -0.9 | 0 | ok |
| convolution rnd3 | (none) | chord | -11.8 | -240.0 | -24.1 | -99.00 | -1.1 | 0 | ok |
| convolution rnd4 | (none) | chord | -16.1 | -240.0 | -25.9 | -99.00 | -6.0 | 0 | ok |
| convolution rnd5 | (none) | chord | -10.4 | -240.0 | -42.1 | -99.00 | -1.2 | 0 | ok |
| convolution rnd6 | (none) | chord | -11.4 | -240.0 | -113.3 | -99.00 | -1.3 | 0 | ok |
| doppler max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| doppler rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| doppler rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| doppler rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| doppler rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| doppler rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| doppler rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| harmony max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +1.7 | 0 | ok |
| harmony rnd1 | (none) | noise | -63.0 | -240.0 | -177.0 | -99.00 | -38.0 | 0 | ok |
| harmony rnd2 | (none) | noise | -41.3 | -240.0 | -198.7 | -99.00 | -28.7 | 0 | ok |
| harmony rnd3 | (none) | noise | -48.6 | -240.0 | -191.4 | -99.00 | -36.0 | 0 | ok |
| harmony rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -22.8 | 0 | ok |
| harmony rnd5 | (none) | noise | -69.5 | -240.0 | -170.5 | -99.00 | -56.8 | 0 | ok |
| harmony rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -9.2 | 0 | ok |
| pitch_shift max | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | -4.5 | 0 | ok |
| pitch_shift rnd1 | feedback | chord | -10.3 | -240.0 | -34.7 | -99.00 | +2.0 | 0 | ok |
| pitch_shift rnd2 | feedback | drums | -11.7 | -110.2 | -1.2 | -99.00 | +11.4 | 0 | ok |
| pitch_shift rnd3 | feedback | noise | -17.4 | -240.0 | -26.8 | -99.00 | -3.5 | 0 | ok |
| pitch_shift rnd4 | feedback | chord | -9.2 | -39.7 | -3.4 | -99.00 | +1.4 | 0 | ok |
| pitch_shift rnd5 | feedback | noise | -17.8 | -76.3 | -10.0 | -99.00 | -3.9 | 0 | ok |
| pitch_shift rnd6 | feedback | noise | -18.0 | -108.2 | -13.3 | -99.00 | -2.6 | 0 | ok |
| formant max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.4 | 0 | ok |
| formant rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -4.1 | 0 | ok |
| formant rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -7.0 | 0 | ok |
| formant rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +3.1 | 0 | ok |
| formant rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -6.0 | 0 | ok |
| formant rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.1 | 0 | ok |
| formant rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -5.8 | 0 | ok |
| doubler max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| doubler rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| doubler rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| doubler rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| doubler rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| doubler rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| doubler rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pitch_correct max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pitch_correct rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pitch_correct rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pitch_correct rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pitch_correct rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pitch_correct rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pitch_correct rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| vocoder max | (none) | chord | -7.1 | -240.0 | -178.6 | -99.00 | +1.6 | 0 | ok |
| vocoder rnd1 | (none) | noise | -16.1 | -240.0 | -34.8 | -99.00 | -0.9 | 0 | ok |
| vocoder rnd2 | (none) | noise | -17.3 | -175.7 | -17.8 | -99.00 | -0.9 | 0 | ok |
| vocoder rnd3 | (none) | noise | -19.6 | -240.0 | -45.5 | -99.00 | -0.9 | 0 | ok |
| vocoder rnd4 | (none) | noise | -20.3 | -193.4 | -19.5 | -99.00 | -0.9 | 0 | ok |
| vocoder rnd5 | (none) | chord | -18.5 | -223.3 | -25.9 | -99.00 | -0.9 | 0 | ok |
| vocoder rnd6 | (none) | noise | -17.5 | -119.0 | -12.9 | -99.00 | -0.9 | 0 | ok |
| chorus max | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| chorus rnd1 | feedback | noise | -19.5 | -240.0 | -117.3 | -99.00 | -0.9 | 0 | ok |
| chorus rnd2 | feedback | noise | -18.7 | -240.0 | -179.9 | -99.00 | -0.9 | 0 | ok |
| chorus rnd3 | feedback | noise | -18.3 | -240.0 | -71.0 | -99.00 | -0.9 | 0 | ok |
| chorus rnd4 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| chorus rnd5 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| chorus rnd6 | feedback | noise | -20.0 | -240.0 | -113.5 | -99.00 | -0.9 | 0 | ok |
| flanger max | feedback | chord | -13.1 | -240.0 | -199.3 | -99.00 | +1.0 | 0 | ok |
| flanger rnd1 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| flanger rnd2 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| flanger rnd3 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| flanger rnd4 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.3 | 0 | ok |
| flanger rnd5 | feedback | chord | -14.3 | -240.0 | -177.3 | -99.00 | +0.2 | 0 | ok |
| flanger rnd6 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.2 | 0 | ok |
| phaser max | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.9 | 0 | ok |
| phaser rnd1 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.9 | 0 | ok |
| phaser rnd2 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.9 | 0 | ok |
| phaser rnd3 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | +1.7 | 0 | ok |
| phaser rnd4 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | +1.3 | 0 | ok |
| phaser rnd5 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.3 | 0 | ok |
| phaser rnd6 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | +1.9 | 0 | ok |
| vibrato max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| vibrato rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| vibrato rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| vibrato rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| vibrato rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| vibrato rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| vibrato rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| tremolo max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| tremolo rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| tremolo rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| tremolo rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| tremolo rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.4 | 0 | ok |
| tremolo rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.1 | 0 | ok |
| tremolo rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| filter max | resonance |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| filter rnd1 | resonance |  | -240.0 | -240.0 | -200.0 | -99.00 | +4.0 | 0 | ok |
| filter rnd2 | resonance |  | -240.0 | -240.0 | -200.0 | -99.00 | +8.8 | 0 | ok |
| filter rnd3 | resonance |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| filter rnd4 | resonance |  | -240.0 | -240.0 | -200.0 | -99.00 | +9.6 | 0 | ok |
| filter rnd5 | resonance |  | -240.0 | -240.0 | -200.0 | -99.00 | +1.6 | 0 | ok |
| filter rnd6 | resonance |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.8 | 0 | ok |
| ring_mod max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| ring_mod rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| ring_mod rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| ring_mod rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| ring_mod rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| ring_mod rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| ring_mod rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| rotary max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| rotary rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| rotary rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| rotary rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| rotary rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| rotary rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| rotary rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| freq_shift max | feedback | chord | -8.3 | -143.3 | -15.1 | -99.00 | +0.8 | 0 | ok |
| freq_shift rnd1 | feedback | noise | -15.4 | -114.1 | -13.6 | -99.00 | -0.9 | 0 | ok |
| freq_shift rnd2 | feedback | chord | -14.4 | -73.0 | -8.2 | -99.00 | -0.9 | 0 | ok |
| freq_shift rnd3 | feedback | chord | -14.1 | -50.2 | -8.7 | -99.00 | -0.9 | 0 | ok |
| freq_shift rnd4 | feedback | chord | -14.9 | -70.5 | -7.6 | -99.00 | -0.9 | 0 | ok |
| freq_shift rnd5 | feedback | chord | -14.5 | -42.9 | -5.2 | -99.00 | -0.9 | 0 | ok |
| freq_shift rnd6 | feedback | chord | -14.3 | -126.5 | -8.1 | -99.00 | -0.9 | 0 | ok |
| grain_engine max | size_ms,density,feedback,size_jit,loop_ms,fb_hp,fb_lp,fb_rate,fb_drive | noise | -11.5 | -240.0 | -50.4 | -99.00 | -0.5 | 0 | ok |
| grain_engine rnd1 | size_ms,density,feedback,size_jit,loop_ms,fb_hp,fb_lp,fb_rate,fb_drive | chord | -13.9 | -240.0 | +0.5 | -99.00 | -0.9 | 0 | ok |
| grain_engine rnd2 | size_ms,density,feedback,size_jit,loop_ms,fb_hp,fb_lp,fb_rate,fb_drive | noise | -13.9 | -240.0 | -8.2 | -99.00 | -0.9 | 0 | ok |
| grain_engine rnd3 | size_ms,density,feedback,size_jit,loop_ms,fb_hp,fb_lp,fb_rate,fb_drive | chord | -15.7 | -80.5 | -13.4 | -99.00 | -0.9 | 0 | ok |
| grain_engine rnd4 | size_ms,density,feedback,size_jit,loop_ms,fb_hp,fb_lp,fb_rate,fb_drive | chord | -18.6 | -83.1 | -6.7 | -99.00 | -0.9 | 0 | ok |
| grain_engine rnd5 | size_ms,density,feedback,size_jit,loop_ms,fb_hp,fb_lp,fb_rate,fb_drive | noise | -6.4 | -240.0 | -0.6 | -99.00 | +8.0 | 0 | ok |
| grain_engine rnd6 | size_ms,density,feedback,size_jit,loop_ms,fb_hp,fb_lp,fb_rate,fb_drive | chord | -13.5 | -53.2 | +3.7 | -99.00 | +2.2 | 0 | ok |
| granular max | size_ms,density,feedback | chord | 1.3 | -193.7 | -8.8 | -99.00 | +11.1 | 0 | ok |
| granular rnd1 | size_ms,density,feedback | chord | -10.7 | -99.9 | -2.9 | -99.00 | -0.9 | 0 | ok |
| granular rnd2 | size_ms,density,feedback | chord | -10.5 | -169.8 | -4.5 | -99.00 | -0.4 | 0 | ok |
| granular rnd3 | size_ms,density,feedback | noise | -19.4 | -172.6 | -4.1 | -99.00 | -0.9 | 0 | ok |
| granular rnd4 | size_ms,density,feedback | chord | -13.5 | -122.3 | -8.5 | -99.00 | -0.9 | 0 | ok |
| granular rnd5 | size_ms,density,feedback | chord | -19.3 | -134.2 | -2.0 | -99.00 | -0.9 | 0 | ok |
| granular rnd6 | size_ms,density,feedback | chord | -9.6 | -161.0 | -4.4 | -99.00 | +0.9 | 0 | ok |
| spectral max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.3 | 0 | ok |
| spectral rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -2.3 | 0 | ok |
| spectral rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.1 | 0 | ok |
| spectral rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -2.0 | 0 | ok |
| spectral rnd4 | (none) | chord | -11.6 | -240.0 | -160.8 | -99.00 | -0.6 | 0 | ok |
| spectral rnd5 | (none) | chord | -16.5 | -23.3 | -6.7 | +0.00 | -3.6 | 0 | hold (intended) |
| spectral rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.2 | 0 | ok |
| stutter max | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| stutter rnd1 | decay | noise | -13.9 | -240.0 | -20.9 | -99.00 | -0.9 | 0 | ok |
| stutter rnd2 | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| stutter rnd3 | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| stutter rnd4 | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| stutter rnd5 | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| stutter rnd6 | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| looper max | (none) | noise | -13.9 | -13.9 | +0.0 | +0.00 | -0.9 | 0 | hold (intended) |
| looper rnd1 | (none) | noise | -13.9 | -240.0 | +0.0 | -99.00 | -0.9 | 0 | ok |
| looper rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| looper rnd3 | (none) | noise | -13.9 | -13.9 | -0.0 | -99.00 | -0.9 | 0 | ok |
| looper rnd4 | (none) | chord | -13.9 | -41.8 | -27.2 | -0.07 | -0.9 | 0 | hold (intended) |
| looper rnd5 | (none) | noise | -13.9 | -240.0 | -0.2 | -99.00 | +2.0 | 0 | ok |
| looper rnd6 | (none) | noise | -25.4 | -52.7 | -2.5 | -99.00 | -0.9 | 0 | ok |
| resonator max | decay | chord | -10.4 | -68.5 | -8.3 | -99.00 | -0.9 | 0 | ok |
| resonator rnd1 | decay | drums | -14.9 | -77.9 | -12.3 | -99.00 | -0.9 | 0 | ok |
| resonator rnd2 | decay | chord | -9.3 | -86.4 | -13.6 | -99.00 | -0.9 | 0 | ok |
| resonator rnd3 | decay | chord | -19.9 | -68.2 | -6.6 | -99.00 | -0.9 | 0 | ok |
| resonator rnd4 | decay | noise | -17.6 | -100.2 | -17.9 | -99.00 | -0.9 | 0 | ok |
| resonator rnd5 | decay | chord | 4.9 | -46.5 | -5.6 | -99.00 | +12.0 | 0 | ok |
| resonator rnd6 | decay | noise | -16.2 | -112.9 | -25.1 | -99.00 | -0.1 | 0 | ok |
| eq max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| eq rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| eq rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| eq rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| eq rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +7.2 | 0 | ok |
| eq rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| eq rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| saturation max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.8 | 0 | ok |
| saturation rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -5.3 | 0 | ok |
| saturation rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -5.0 | 0 | ok |
| saturation rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +5.1 | 0 | ok |
| saturation rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.8 | 0 | ok |
| saturation rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -6.8 | 0 | ok |
| saturation rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -6.2 | 0 | ok |
| width max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| width rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| width rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +8.7 | 0 | ok |
| width rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| width rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| width rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| width rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +6.4 | 0 | ok |
| compressor max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +4.8 | 0 | ok |
| compressor rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.7 | 0 | ok |
| compressor rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +12.0 | 0 | ok |
| compressor rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| compressor rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.6 | 0 | ok |
| compressor rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +12.0 | 0 | ok |
| compressor rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +12.0 | 0 | ok |
| gate max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| gate rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| gate rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| gate rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| gate rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| gate rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| gate rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| deesser max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| deesser rnd1 | (none) | chord | -42.4 | -240.0 | -197.6 | -99.00 | -0.9 | 0 | ok |
| deesser rnd2 | (none) | chord | -43.7 | -240.0 | -196.3 | -99.00 | -0.9 | 0 | ok |
| deesser rnd3 | (none) | chord | -42.1 | -240.0 | -197.9 | -99.00 | -0.9 | 0 | ok |
| deesser rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| deesser rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| deesser rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| transient max | sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | +12.0 | 0 | ok |
| transient rnd1 | sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | +9.3 | 0 | ok |
| transient rnd2 | sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | +11.7 | 0 | ok |
| transient rnd3 | sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| transient rnd4 | sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | +1.0 | 0 | ok |
| transient rnd5 | sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | +10.8 | 0 | ok |
| transient rnd6 | sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | +12.0 | 0 | ok |
| tape_colour max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -2.2 | 0 | ok |
| tape_colour rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.3 | 0 | ok |
| tape_colour rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +1.9 | 0 | ok |
| tape_colour rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.8 | 0 | ok |
| tape_colour rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.2 | 0 | ok |
| tape_colour rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -6.7 | 0 | ok |
| tape_colour rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -7.0 | 0 | ok |
| bitcrush max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| bitcrush rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.5 | 0 | ok |
| bitcrush rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.5 | 0 | ok |
| bitcrush rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.3 | 0 | ok |
| bitcrush rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.6 | 0 | ok |
| bitcrush rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.6 | 0 | ok |
| bitcrush rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.4 | 0 | ok |
| exciter max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.2 | 0 | ok |
| exciter rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.4 | 0 | ok |
| exciter rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.2 | 0 | ok |
| exciter rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.1 | 0 | ok |
| exciter rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.7 | 0 | ok |
| exciter rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.3 | 0 | ok |
| exciter rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.3 | 0 | ok |
| amp max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.9 | 0 | ok |
| amp rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -7.3 | 0 | ok |
| amp rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +7.0 | 0 | ok |
| amp rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -8.1 | 0 | ok |
| amp rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -13.4 | 0 | ok |
| amp rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.4 | 0 | ok |
| amp rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -5.0 | 0 | ok |
| spectral_smear max | (none) | noise | -25.2 | -174.1 | -12.1 | -99.00 | +6.4 | 0 | ok |
| spectral_smear rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -7.3 | 0 | ok |
| spectral_smear rnd2 | (none) | chord | -18.8 | -65.0 | -22.4 | -99.00 | -7.3 | 0 | ok |
| spectral_smear rnd3 | (none) | chord | -13.4 | -218.6 | -20.1 | -99.00 | -5.7 | 0 | ok |
| spectral_smear rnd4 | (none) | chord | -19.3 | -123.8 | -36.6 | -99.00 | -7.3 | 0 | ok |
| spectral_smear rnd5 | (none) | chord | -19.3 | -120.8 | -31.6 | -99.00 | -7.3 | 0 | ok |
| spectral_smear rnd6 | (none) | chord | -18.1 | -101.9 | -16.3 | -99.00 | -7.3 | 0 | ok |
| spectral_delay max | feedback | chord | -12.8 | -65.9 | -4.8 | -99.00 | -3.8 | 0 | ok |
| spectral_delay rnd1 | feedback | chord | -6.2 | -40.9 | -3.3 | -99.00 | +2.3 | 0 | ok |
| spectral_delay rnd2 | feedback | chord | -25.7 | -75.6 | -11.7 | -99.00 | -13.7 | 0 | ok |
| spectral_delay rnd3 | feedback | chord | -19.1 | -46.6 | -14.4 | -1.11 | -7.3 | 0 | LONG |
| spectral_delay rnd4 | feedback | noise | -19.1 | -240.0 | -29.2 | -99.00 | -7.1 | 0 | ok |
| spectral_delay rnd5 | feedback | chord | -10.8 | -156.9 | -19.3 | -99.00 | +1.5 | 0 | ok |
| spectral_delay rnd6 | feedback | chord | -17.9 | -47.3 | -9.3 | -99.00 | -7.3 | 0 | ok |
| time_warp max | (none) | drums | -240.0 | -240.0 | +170.2 | -99.00 | -0.9 | 0 | ok |
| time_warp rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| time_warp rnd2 | (none) | chord | -26.2 | -240.0 | +11.2 | -99.00 | -0.9 | 0 | ok |
| time_warp rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| time_warp rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| time_warp rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| time_warp rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| glitch max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| glitch rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| glitch rnd2 | (none) | noise | -21.2 | -240.0 | +2.5 | -99.00 | -0.9 | 0 | ok |
| glitch rnd3 | (none) | chord | -14.7 | -240.0 | -4.6 | -99.00 | -0.9 | 0 | ok |
| glitch rnd4 | (none) | chord | -15.4 | -240.0 | -4.0 | -99.00 | -0.9 | 0 | ok |
| glitch rnd5 | (none) | chord | -16.1 | -240.0 | -6.7 | -99.00 | -0.9 | 0 | ok |
| glitch rnd6 | (none) | chord | -16.3 | -240.0 | -5.2 | -99.00 | -0.9 | 0 | ok |
| degrade max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| degrade rnd1 | (none) | impulse | -37.7 | -68.8 | -31.0 | -99.00 | -0.9 | 0 | ok |
| degrade rnd2 | (none) | impulse | -37.7 | -67.7 | -29.9 | -99.00 | -0.9 | 0 | ok |
| degrade rnd3 | (none) | impulse | -37.7 | -75.3 | -37.5 | -99.00 | -0.9 | 0 | ok |
| degrade rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| degrade rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| degrade rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| fold_crush max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -4.0 | 0 | ok |
| fold_crush rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -4.8 | 0 | ok |
| fold_crush rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -7.2 | 0 | ok |
| fold_crush rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -4.7 | 0 | ok |
| fold_crush rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -6.7 | 0 | ok |
| fold_crush rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -6.4 | 0 | ok |
| fold_crush rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -5.8 | 0 | ok |
| slice_shuffle max | (none) | noise | -34.6 | -240.0 | +20.8 | -99.00 | -0.9 | 0 | ok |
| slice_shuffle rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| slice_shuffle rnd2 | (none) | noise | -19.9 | -240.0 | -1.0 | -99.00 | -0.9 | 0 | ok |
| slice_shuffle rnd3 | (none) | drums | -17.4 | -240.0 | +1.1 | -99.00 | -0.9 | 0 | ok |
| slice_shuffle rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| slice_shuffle rnd5 | (none) | noise | -19.9 | -240.0 | -5.4 | -99.00 | -0.9 | 0 | ok |
| slice_shuffle rnd6 | (none) | chord | -19.5 | -240.0 | -4.6 | -99.00 | -0.9 | 0 | ok |
| transient_split max | repeats,rep_decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.6 | 0 | ok |
| transient_split rnd1 | repeats,rep_decay | chord | -19.4 | -240.0 | -23.0 | -99.00 | -0.9 | 0 | ok |
| transient_split rnd2 | repeats,rep_decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| transient_split rnd3 | repeats,rep_decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| transient_split rnd4 | repeats,rep_decay | chord | -19.5 | -240.0 | -40.3 | -99.00 | -0.9 | 0 | ok |
| transient_split rnd5 | repeats,rep_decay | chord | -13.1 | -240.0 | -56.5 | -99.00 | +0.9 | 0 | ok |
| transient_split rnd6 | repeats,rep_decay | chord | -19.3 | -240.0 | -24.9 | -99.00 | -0.9 | 0 | ok |
| echo_matrix max | decay,feedback | noise | -13.7 | -35.1 | -9.4 | -1.18 | +5.3 | 0 | LONG |
| echo_matrix rnd1 | decay,feedback | noise | -16.9 | -77.1 | -6.2 | -99.00 | -0.5 | 0 | ok |
| echo_matrix rnd2 | decay,feedback | chord | -11.8 | -37.1 | -4.9 | -99.00 | -0.9 | 0 | ok |
| echo_matrix rnd3 | decay,feedback | chord | -9.0 | -39.2 | -4.2 | -99.00 | +0.3 | 0 | ok |
| echo_matrix rnd4 | decay,feedback | chord | -13.5 | -49.1 | +1.0 | -99.00 | -0.9 | 0 | ok |
| echo_matrix rnd5 | decay,feedback | chord | -8.6 | -26.8 | -3.3 | -1.28 | +1.7 | 0 | LONG |
| echo_matrix rnd6 | decay,feedback | chord | -10.4 | -47.9 | -6.2 | -99.00 | +0.7 | 0 | ok |
| reverse_echo max | feedback | chord | -9.4 | -240.0 | -8.2 | -99.00 | +2.7 | 0 | ok |
| reverse_echo rnd1 | feedback | noise | -17.9 | -37.1 | +2.4 | -99.00 | -0.9 | 0 | ok |
| reverse_echo rnd2 | feedback | chord | -15.3 | -35.1 | +1.8 | -99.00 | -0.9 | 0 | ok |
| reverse_echo rnd3 | feedback | noise | -17.1 | -208.9 | -9.1 | -99.00 | -0.9 | 0 | ok |
| reverse_echo rnd4 | feedback | chord | -12.7 | -51.8 | -0.8 | -99.00 | -0.9 | 0 | ok |
| reverse_echo rnd5 | feedback | noise | -13.9 | -64.2 | -1.7 | -99.00 | -0.0 | 0 | ok |
| reverse_echo rnd6 | feedback | noise | -13.9 | -240.0 | -12.7 | -99.00 | +2.5 | 0 | ok |
| dispersion max | feedback | chord | -13.0 | -240.0 | -45.7 | -99.00 | +2.5 | 0 | ok |
| dispersion rnd1 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | +5.7 | 0 | ok |
| dispersion rnd2 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| dispersion rnd3 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.1 | 0 | ok |
| dispersion rnd4 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | +3.9 | 0 | ok |
| dispersion rnd5 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| dispersion rnd6 | feedback |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| modal max | decay | drums | -14.9 | -63.5 | -19.6 | -99.00 | -0.9 | 0 | ok |
| modal rnd1 | decay | drums | -16.3 | -96.3 | -23.9 | -99.00 | -0.9 | 0 | ok |
| modal rnd2 | decay | drums | -16.7 | -82.8 | -16.0 | -99.00 | -0.9 | 0 | ok |
| modal rnd3 | decay | noise | -19.7 | -116.8 | -44.2 | -99.00 | -0.9 | 0 | ok |
| modal rnd4 | decay | drums | -17.2 | -103.1 | -27.1 | -99.00 | -0.9 | 0 | ok |
| modal rnd5 | decay | noise | -19.8 | -110.3 | -28.2 | -99.00 | -0.9 | 0 | ok |
| modal rnd6 | decay | noise | -26.7 | -91.4 | -12.1 | -99.00 | -0.9 | 0 | ok |
| texture max | density | noise | -12.6 | -240.0 | -30.6 | -99.00 | +5.2 | 0 | ok |
| texture rnd1 | density |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| texture rnd2 | density | chord | -13.5 | -240.0 | -40.3 | -99.00 | -0.9 | 0 | ok |
| texture rnd3 | density | noise | -13.9 | -240.0 | -58.8 | -99.00 | -0.9 | 0 | ok |
| texture rnd4 | density | noise | -13.7 | -240.0 | -31.6 | -99.00 | +4.2 | 0 | ok |
| texture rnd5 | density | noise | -13.9 | -240.0 | -29.7 | -99.00 | -0.7 | 0 | ok |
| texture rnd6 | density | noise | -13.6 | -240.0 | -16.5 | -99.00 | -0.5 | 0 | ok |
| spectral_shatter max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.1 | 0 | ok |
| spectral_shatter rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -6.5 | 0 | ok |
| spectral_shatter rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.3 | 0 | ok |
| spectral_shatter rnd3 | (none) | chord | -19.3 | -240.0 | -37.8 | -99.00 | -7.1 | 0 | ok |
| spectral_shatter rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -22.1 | 0 | ok |
| spectral_shatter rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -1.6 | 0 | ok |
| spectral_shatter rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -7.3 | 0 | ok |
| partial_recompose max | sustain | noise | -11.9 | -43.5 | -2.5 | -99.00 | +7.1 | 0 | ok |
| partial_recompose rnd1 | sustain | drums | -17.6 | -69.9 | -19.0 | -99.00 | -5.8 | 0 | ok |
| partial_recompose rnd2 | sustain | noise | -18.0 | -53.6 | -6.5 | -99.00 | -0.7 | 0 | ok |
| partial_recompose rnd3 | sustain | noise | -18.8 | -54.0 | -3.2 | -99.00 | -2.2 | 0 | ok |
| partial_recompose rnd4 | sustain | noise | -14.9 | -49.0 | -4.1 | -99.00 | +1.9 | 0 | ok |
| partial_recompose rnd5 | sustain | noise | -19.2 | -71.8 | -21.5 | -99.00 | -6.0 | 0 | ok |
| partial_recompose rnd6 | sustain | drums | -17.7 | -62.0 | -14.4 | -99.00 | -4.3 | 0 | ok |
| spectral_graft max | resolution |  | -240.0 | -240.0 | -200.0 | -99.00 | +9.2 | 0 | ok |
| spectral_graft rnd1 | resolution |  | -240.0 | -240.0 | -200.0 | -99.00 | -3.3 | 0 | ok |
| spectral_graft rnd2 | resolution |  | -240.0 | -240.0 | -200.0 | -99.00 | +6.0 | 0 | ok |
| spectral_graft rnd3 | resolution |  | -240.0 | -240.0 | -200.0 | -99.00 | -6.0 | 0 | ok |
| spectral_graft rnd4 | resolution |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.6 | 0 | ok |
| spectral_graft rnd5 | resolution |  | -240.0 | -240.0 | -200.0 | -99.00 | +5.6 | 0 | ok |
| spectral_graft rnd6 | resolution |  | -240.0 | -240.0 | -200.0 | -99.00 | -7.1 | 0 | ok |
| cloud_chamber max | size,decay,low_decay,high_decay,density | chord | -10.7 | -25.6 | -4.7 | -1.09 | -0.6 | 0 | LONG |
| cloud_chamber rnd1 | size,decay,low_decay,high_decay,density | chord | -28.6 | -39.7 | +1.3 | -1.39 | -0.9 | 0 | LONG |
| cloud_chamber rnd2 | size,decay,low_decay,high_decay,density | chord | -12.9 | -28.1 | -3.3 | -1.18 | -0.9 | 0 | LONG |
| cloud_chamber rnd3 | size,decay,low_decay,high_decay,density | chord | -13.5 | -33.3 | -7.5 | -1.02 | -0.9 | 0 | LONG |
| cloud_chamber rnd4 | size,decay,low_decay,high_decay,density | noise | -13.9 | -44.1 | -9.9 | -99.00 | -0.9 | 0 | ok |
| cloud_chamber rnd5 | size,decay,low_decay,high_decay,density |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| cloud_chamber rnd6 | size,decay,low_decay,high_decay,density | chord | -13.3 | -42.7 | -14.8 | -1.32 | -0.9 | 0 | LONG |
| reflections max | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| reflections rnd1 | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| reflections rnd2 | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| reflections rnd3 | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| reflections rnd4 | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| reflections rnd5 | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| reflections rnd6 | decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| reverse_bloom max | density | chord | -9.7 | -240.0 | -19.0 | -99.00 | -0.6 | 0 | ok |
| reverse_bloom rnd1 | density | chord | -10.5 | -240.0 | -8.3 | -99.00 | -0.9 | 0 | ok |
| reverse_bloom rnd2 | density | noise | -28.6 | -88.9 | +2.5 | -99.00 | +2.6 | 0 | ok |
| reverse_bloom rnd3 | density | chord | -21.4 | -240.0 | +1.4 | -99.00 | -0.9 | 0 | ok |
| reverse_bloom rnd4 | density | chord | -10.9 | -119.9 | -3.3 | -99.00 | -0.9 | 0 | ok |
| reverse_bloom rnd5 | density | chord | -16.3 | -129.7 | +5.6 | -99.00 | -0.1 | 0 | ok |
| reverse_bloom rnd6 | density | chord | -9.7 | -75.8 | -8.3 | -99.00 | -0.8 | 0 | ok |
| swell max | (none) | chord | -12.0 | -240.0 | -64.1 | -99.00 | -4.3 | 0 | ok |
| swell rnd1 | (none) | noise | -15.6 | -240.0 | -59.2 | -99.00 | -2.9 | 0 | ok |
| swell rnd2 | (none) | chord | -13.3 | -240.0 | -71.4 | -99.00 | -4.4 | 0 | ok |
| swell rnd3 | (none) | chord | -12.0 | -240.0 | -63.8 | -99.00 | -1.2 | 0 | ok |
| swell rnd4 | (none) | chord | -12.2 | -240.0 | -65.4 | -99.00 | -1.2 | 0 | ok |
| swell rnd5 | (none) | chord | -11.1 | -240.0 | -61.1 | -99.00 | -3.6 | 0 | ok |
| swell rnd6 | (none) | chord | -11.6 | -240.0 | -60.4 | -99.00 | -3.5 | 0 | ok |
| pulse_shaper max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pulse_shaper rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pulse_shaper rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pulse_shaper rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pulse_shaper rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pulse_shaper rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| pulse_shaper rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| drum_crush max | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.7 | 0 | ok |
| drum_crush rnd1 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.7 | 0 | ok |
| drum_crush rnd2 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| drum_crush rnd3 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +2.9 | 0 | ok |
| drum_crush rnd4 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +5.3 | 0 | ok |
| drum_crush rnd5 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| drum_crush rnd6 | (none) |  | -240.0 | -240.0 | -200.0 | -99.00 | +4.1 | 0 | ok |
| drum_space max | size,decay | chord | -16.4 | -205.4 | -37.8 | -99.00 | -0.9 | 0 | ok |
| drum_space rnd1 | size,decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| drum_space rnd2 | size,decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| drum_space rnd3 | size,decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| drum_space rnd4 | size,decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| drum_space rnd5 | size,decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| drum_space rnd6 | size,decay |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| band_transients max | low_sustain,mid_sustain,high_sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | +0.0 | 0 | ok |
| band_transients rnd1 | low_sustain,mid_sustain,high_sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| band_transients rnd2 | low_sustain,mid_sustain,high_sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.0 | 0 | ok |
| band_transients rnd3 | low_sustain,mid_sustain,high_sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | +4.8 | 0 | ok |
| band_transients rnd4 | low_sustain,mid_sustain,high_sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| band_transients rnd5 | low_sustain,mid_sustain,high_sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| band_transients rnd6 | low_sustain,mid_sustain,high_sustain |  | -240.0 | -240.0 | -200.0 | -99.00 | -0.9 | 0 | ok |
| hit_melody max | decay | drums | -16.9 | -240.0 | -33.4 | -99.00 | -0.9 | 0 | ok |
| hit_melody rnd1 | decay | drums | -16.1 | -240.0 | -46.7 | -99.00 | -0.9 | 0 | ok |
| hit_melody rnd2 | decay | drums | -23.6 | -240.0 | -32.0 | -99.00 | -0.9 | 0 | ok |
| hit_melody rnd3 | decay | drums | -15.7 | -240.0 | -35.9 | -99.00 | -0.9 | 0 | ok |
| hit_melody rnd4 | decay | drums | -16.8 | -240.0 | -43.2 | -99.00 | -0.9 | 0 | ok |
| hit_melody rnd5 | decay | noise | -18.0 | -240.0 | -32.1 | -99.00 | -0.9 | 0 | ok |
| hit_melody rnd6 | decay | noise | -29.3 | -240.0 | -44.3 | -99.00 | -0.9 | 0 | ok |

**448 cases: 0 GROW / FAULT, 0 unexplained HOLD.**
