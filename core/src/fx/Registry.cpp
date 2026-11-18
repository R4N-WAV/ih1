#include "ih1/fx/FxProcessor.h"
#include "ih1/fx/AllFx.h"

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

const std::vector<FxDescriptor>& fxRegistry() {
    static const std::vector<FxDescriptor> reg = [] {
        std::vector<FxDescriptor> r;
        fx::registerAll(r);
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
        // grouped by family, registry order within a family
        for (int fam = 0; fam <= static_cast<int>(FxFamily::ToneDynamics); ++fam)
            for (size_t i = 0; i < r.size(); ++i)
                if (!r[i].experimental && static_cast<int>(r[i].family) == fam) o.push_back(static_cast<int>(i));
        return o;
    }();
    return order;
}

}  // namespace ih1
