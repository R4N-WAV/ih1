// IH1 preset audition (V3 Phase 4.6): a repeatable source set and level-matched measurements of how much a preset
// transforms its input. Used by tools/ih1_preset_audit (table + WAV renders) and by the factory tests.
//
// Sources are synthesised (deterministic, no audio files in the repository), each 4 s at -18 dBFS RMS:
//   0 vocal phrase   1 sustained vocal   2 drum loop   3 single snare hits   4 bass phrase
//   5 guitar chord   6 piano / keys      7 synth pad   8 broadband field recording
// They are stand-ins with the right spectral / temporal character, not recordings; listening tests on real material
// remain the owner's job.
//
// Transformation is judged on the LEVEL-MATCHED output (a preset never counts as "extreme" for being louder):
//   correlation  : largest normalised cross-correlation between input and output over 0 .. latency + 50 ms
//   spectralDist : mean |dB| difference of 32-band log spectrograms (input vs output, aligned by the latency)
//   Transformative : correlation < 0.5 or spectralDist > 6 dB;  Moderate : < 0.8 or > 3 dB;  otherwise Subtle.
#pragma once

#include <string>
#include <vector>

namespace ih1::audition {

constexpr int kNumSources = 9;
constexpr double kRate = 48000.0;
const char* sourceName(int s);
// mono source, `seconds` long, -18 dBFS RMS
std::vector<float> source(int s, double seconds = 4.0);
// sources a preset's tag list says it suits ("vocals", "drums", "bass", "guitars", "keys", "synths", "pads",
// "full mix", "loops", "field", "any source"); never empty
std::vector<int> sourcesForTags(const std::string& tags);

enum class Class { Subtle = 0, Moderate = 1, Transformative = 2 };
const char* className(Class c);

struct Result {
    double gainDb = 0.0;        // output RMS vs input RMS (after the first second)
    double peak = 0.0;          // sample peak of the output (linear)
    double correlation = 1.0;   // see above
    double spectralDist = 0.0;  // dB
    int latency = 0;
    bool finite = true;
    Class cls = Class::Subtle;
    std::vector<float> outL, outR;  // the rendered output (for WAV renders)
};

// Render a preset (chain-only state JSON) on source s through the real Engine + Session, with a playing host
// timeline at 120 BPM, and measure it.
Result run(const std::string& presetJson, int s, double seconds = 4.0);

// 16-bit PCM WAV writer (offline tool use only)
bool writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, double sr = kRate);

}  // namespace ih1::audition
