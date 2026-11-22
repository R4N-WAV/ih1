#include "ih1/fx/FxProcessor.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1 {

const char* familyName(FxFamily f) {
    switch (f) {
        case FxFamily::TimeSpace: return "Time & Space";
        case FxFamily::PitchVoice: return "Pitch & Voice";
        case FxFamily::Modulation: return "Modulation";
        case FxFamily::TextureSpectral: return "Texture & Spectral";
        case FxFamily::ToneDynamics: return "Tone & Dynamics";
    }
    return "";
}

const char* categoryName(FxCategory c) {
    switch (c) {
        case FxCategory::TimeSpace: return "Time & Space";
        case FxCategory::PitchVocal: return "Pitch & Vocal";
        case FxCategory::Modulation: return "Modulation";
        case FxCategory::GranularTime: return "Granular & Time";
        case FxCategory::Spectral: return "Spectral";
        case FxCategory::RhythmGlitch: return "Rhythm & Glitch";
        case FxCategory::Dynamics: return "Dynamics";
        case FxCategory::ToneCharacter: return "Tone & Character";
        case FxCategory::Experimental: return "Experimental";
        case FxCategory::Count: break;
    }
    return "";
}

const char* latencyModeName(LatencyMode m) {
    switch (m) {
        case LatencyMode::Low: return "Low Latency";
        case LatencyMode::Zero: return "Zero Latency";
        default: return "Full Quality";
    }
}

namespace {
// ---- one central annotation table: catalogue category, tempo-sync dependencies, latency capabilities ----
struct Annot {
    const char* id;
    FxCategory cat;
    const char* syncFreeParam;  // free-running time/rate parameter disabled while "sync" is on
};
constexpr Annot kAnnot[] = {
    {"delay", FxCategory::TimeSpace, "time_ms"},
    {"tape_echo", FxCategory::TimeSpace, "time_ms"},
    {"reverb", FxCategory::TimeSpace, nullptr},
    {"shimmer", FxCategory::TimeSpace, nullptr},
    {"convolution", FxCategory::TimeSpace, nullptr},
    {"doppler", FxCategory::TimeSpace, "period"},
    {"harmony", FxCategory::PitchVocal, nullptr},
    {"pitch_shift", FxCategory::PitchVocal, nullptr},
    {"formant", FxCategory::PitchVocal, nullptr},
    {"doubler", FxCategory::PitchVocal, nullptr},
    {"pitch_correct", FxCategory::PitchVocal, nullptr},
    {"vocoder", FxCategory::PitchVocal, nullptr},
    {"chorus", FxCategory::Modulation, nullptr},
    {"flanger", FxCategory::Modulation, nullptr},
    {"phaser", FxCategory::Modulation, nullptr},
    {"vibrato", FxCategory::Modulation, "rate"},
    {"tremolo", FxCategory::Modulation, "rate"},
    {"filter", FxCategory::Modulation, "rate"},
    {"rotary", FxCategory::Modulation, nullptr},
    {"grain_engine", FxCategory::GranularTime, "retrig_ms"},
    {"granular", FxCategory::GranularTime, nullptr},
    {"reverse", FxCategory::GranularTime, "length_ms"},
    {"looper", FxCategory::GranularTime, nullptr},
    {"spectral", FxCategory::Spectral, nullptr},
    {"freq_shift", FxCategory::Spectral, nullptr},
    {"resonator", FxCategory::Spectral, nullptr},
    {"stutter", FxCategory::RhythmGlitch, nullptr},
    {"gate", FxCategory::RhythmGlitch, nullptr},
    {"bitcrush", FxCategory::RhythmGlitch, nullptr},
    {"compressor", FxCategory::Dynamics, nullptr},
    {"deesser", FxCategory::Dynamics, nullptr},
    {"transient", FxCategory::Dynamics, nullptr},
    {"eq", FxCategory::ToneCharacter, nullptr},
    {"saturation", FxCategory::ToneCharacter, nullptr},
    {"tape_colour", FxCategory::ToneCharacter, nullptr},
    {"exciter", FxCategory::ToneCharacter, nullptr},
    {"amp", FxCategory::ToneCharacter, nullptr},
    {"width", FxCategory::ToneCharacter, nullptr},
    {"ring_mod", FxCategory::Experimental, nullptr},
    // V3
    {"spectral_smear", FxCategory::Spectral, nullptr},
    {"spectral_delay", FxCategory::Spectral, "time_ms"},
    {"time_warp", FxCategory::GranularTime, nullptr},
    {"glitch", FxCategory::RhythmGlitch, nullptr},
    {"degrade", FxCategory::ToneCharacter, nullptr},
    {"fold_crush", FxCategory::ToneCharacter, nullptr},
    // V4
    {"slice_shuffle", FxCategory::RhythmGlitch, nullptr},
    {"transient_split", FxCategory::Experimental, nullptr},
    {"echo_matrix", FxCategory::TimeSpace, nullptr},
    {"reverse_echo", FxCategory::PitchVocal, "length_ms"},
    {"dispersion", FxCategory::Modulation, nullptr},
    {"modal", FxCategory::Spectral, nullptr},
    {"texture", FxCategory::ToneCharacter, nullptr},
    {"spectral_shatter", FxCategory::Spectral, nullptr},
    {"partial_recompose", FxCategory::PitchVocal, nullptr},
    {"spectral_graft", FxCategory::Spectral, nullptr},
    // V5
    {"cloud_chamber", FxCategory::TimeSpace, nullptr},
    {"reflections", FxCategory::TimeSpace, nullptr},
    {"reverse_bloom", FxCategory::TimeSpace, "length_ms"},
    {"swell", FxCategory::Dynamics, nullptr},
    {"pulse_shaper", FxCategory::RhythmGlitch, nullptr},
    {"drum_crush", FxCategory::Dynamics, nullptr},
    {"drum_space", FxCategory::TimeSpace, nullptr},
    {"band_transients", FxCategory::Dynamics, nullptr},
    {"hit_melody", FxCategory::Experimental, nullptr},
};

void annotate(FxDescriptor& d) {
    for (const auto& a : kAnnot) {
        if (d.typeId != a.id) continue;
        d.category = a.cat;
        if (a.syncFreeParam && d.paramIndex("sync") >= 0) {
            const int k = d.paramIndex(a.syncFreeParam);
            if (k >= 0) {
                auto& p = d.params[static_cast<size_t>(k)];
                p.enabledBy = "sync";
                p.enabledWhen = 0.0f;
                p.disabledReason = "Inactive while Sync is on: the Division sets the time. Turn Sync off to use this control (its value is kept).";
            }
        }
    }
    auto& L = d.latency;
    const std::string& t = d.typeId;
    if (t == "swell") {
        L.lookahead = true;
        L.cpuClass = 1;
        L.lowNote = "No look-ahead: 0 samples instead of 3 ms, and the first ~1 ms of each attack is heard before the swell closes.";
        L.zeroNote = L.lowNote;
        return;
    }
    if (t == "cloud_chamber" || t == "drum_space" || t == "reflections") L.cpuClass = 2;
    if (t == "spectral_shatter" || t == "spectral_graft" || t == "partial_recompose") {
        L.zero = false;
        L.low = true;
        L.analysisWindow = true;
        L.cpuClass = 3;
        L.lowNote = "Shorter analysis window (512 points at 48 kHz): less latency, coarser low-frequency resolution.";
    } else if (t == "dispersion" || t == "echo_matrix" || t == "modal") {
        L.cpuClass = 2;
    } else if (t == "slice_shuffle" || t == "transient_split" || t == "reverse_echo" || t == "texture") {
        L.cpuClass = 1;
    } else if (t == "spectral_smear" || t == "spectral_delay") {
        L.zero = false;
        L.low = true;
        L.analysisWindow = true;
        L.cpuClass = 3;
        L.lowNote = "Shorter analysis window (512 points at 48 kHz): less latency, coarser low-frequency resolution.";
    } else if (t == "fold_crush") {
        L.oversampling = true;
        L.cpuClass = 2;
        L.lowNote = "2x minimum-phase IIR oversampling: 5 samples instead of 23, same anti-aliasing.";
        L.zeroNote = "No oversampling: 0 samples, but the folder aliases much more (part of its character at high Fold).";
    } else if (t == "time_warp" || t == "glitch" || t == "degrade") {
        L.cpuClass = 1;
    } else if (t == "harmony" || t == "formant" || t == "spectral" || t == "pitch_shift") {
        L.zero = false;
        L.low = true;
        L.analysisWindow = true;
        L.cpuClass = 3;
        L.lowNote = "Shorter analysis window (512 points at 48 kHz): about 11 ms instead of 43 ms, with coarser low-frequency resolution (bass notes can sound grainier).";
    } else if (t == "convolution") {
        L.cpuClass = 2;
        L.zeroNote = "The first partition is convolved directly in the time domain (same sound, more CPU).";
        L.lowNote = L.zeroNote;
    } else if (t == "saturation" || t == "tape_colour" || t == "exciter" || t == "amp") {
        // measured (docs/V2_1_IMPLEMENTATION.md, Saturation +24 dB drive, 5 kHz tone, 48 kHz)
        L.oversampling = true;
        L.cpuClass = 2;
        L.lowNote = "2x minimum-phase IIR oversampling: 5 samples instead of 23, same anti-aliasing as Full Quality (measured).";
        L.zeroNote = "No oversampling, antiderivative anti-aliasing only: 0 samples, but about 24 dB more aliasing at high drive, and the Fold style aliases heavily. Use Low Latency (5 samples) when that matters.";
    } else if (t == "test_latency") {
        L.zero = L.low = false;
    } else if (t == "grain_engine" || t == "granular" || t == "reverb" || t == "shimmer" || t == "vocoder" || t == "pitch_correct") {
        L.cpuClass = 2;
    }
}
}  // namespace

const std::vector<FxDescriptor>& fxRegistry() {
    static const std::vector<FxDescriptor> reg = [] {
        std::vector<FxDescriptor> r;
        fx::registerAll(r);
        for (auto& d : r) {
            annotate(d);
            // never index past ParamValues: a descriptor over the cap is truncated (and fails the contract test)
            if (d.params.size() > static_cast<size_t>(kMaxParams)) d.params.resize(static_cast<size_t>(kMaxParams));
            // V4: tell every processor which parameters are discrete (they switch; continuous ones glide)
            uint64_t mask = 0;
            for (size_t i = 0; i < d.params.size() && i < 64; ++i)
                if (d.params[i].kind != ParamKind::Float) mask |= 1ull << i;
            auto make = d.create;
            d.create = [make, mask]() {
                auto p = make();
                if (auto* b = dynamic_cast<fx::FxBase*>(p.get())) b->setDiscreteMask(mask);
                return p;
            };
        }
        return r;
    }();
    return reg;
}

int fxTypeIndex(const std::string& typeId) {
    const auto& r = fxRegistry();
    for (size_t i = 0; i < r.size(); ++i)
        if (r[i].typeId == typeId) return static_cast<int>(i);
    return -1;
}

const std::vector<int>& fxCarouselOrder() {
    static const std::vector<int> order = [] {
        std::vector<int> o;
        const auto& r = fxRegistry();
        // grouped by catalogue category (the same grouping as the nested menu), registry order within one
        for (int cat = 0; cat < kNumCategories; ++cat)
            for (size_t i = 0; i < r.size(); ++i)
                if (!r[i].experimental && static_cast<int>(r[i].category) == cat) o.push_back(static_cast<int>(i));
        return o;
    }();
    return order;
}

}  // namespace ih1
