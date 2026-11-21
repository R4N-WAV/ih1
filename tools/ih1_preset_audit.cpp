// IH1 preset audit (V3 Phase 4.6 / 7.3): renders every factory preset on the sources its tags name (plus a reference
// source), measures level, peak, correlation with the dry input and spectral distance (level-matched), classifies
// the transformation, and prints a Markdown table. Optional WAV renders of selected presets.
//
// Usage: ih1_preset_audit [--md out.md] [--wav dir id1,id2@source,...]
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "ih1/fx/FxProcessor.h"
#include "ih1/presets/Audition.h"
#include "ih1/presets/Presets.h"

using namespace ih1;

static std::string fileSafe(std::string s) {
    for (char& c : s) if (c == '/' || c == ' ' || c == '\\') c = '_';
    std::string o;
    for (char c : s) if (!(c == '_' && !o.empty() && o.back() == '_')) o += c;
    return o;
}

int main(int argc, char** argv) {
    std::string md, wavDir, wavIds;
    for (int i = 1; i + 1 < argc; ++i) {
        if (!std::strcmp(argv[i], "--md")) md = argv[++i];
        else if (!std::strcmp(argv[i], "--wav") && i + 2 < argc) { wavDir = argv[++i]; wavIds = argv[++i]; }
    }
    std::FILE* out = md.empty() ? stdout : std::fopen(md.c_str(), "w");
    if (!out) return 2;
    std::fprintf(out, "| Preset | Category | Chain | Intent | Sources (class, corr, spec dB, gain dB, peak) | Result |\n|---|---|---|---|---|---|\n");
    std::map<std::string, int> transformative, creative;
    int failures = 0;
    if (!wavDir.empty()) {
        for (int s = 0; s < audition::kNumSources; ++s) {
            const auto x = audition::source(s);
            audition::writeWav(wavDir + "/source__" + fileSafe(audition::sourceName(s)) + ".wav", x, x);
        }
    }
    for (const auto& p : factoryPresets()) {
        const bool utility = p.category == "Init / Utility";
        std::string chain;
        for (const auto& fx : p.chain) chain += (chain.empty() ? "" : " > ") + fxRegistry()[static_cast<size_t>(fxTypeIndex(fx.type))].name;
        if (chain.empty()) chain = "(empty)";
        const auto srcs = audition::sourcesForTags(p.tags);
        std::string cells;
        int nT = 0, nRun = 0;
        bool ok = true;
        const std::string js = presetToJson(p);
        for (int s : srcs) {
            const auto r = audition::run(js, s);
            ++nRun;
            nT += r.cls == audition::Class::Transformative;
            const bool sane = r.finite && r.gainDb < 9.0 && r.gainDb > -24.0 && r.peak < 2.0;
            ok = ok && sane;
            char buf[200];
            std::snprintf(buf, sizeof buf, "%s%s: %s, %.2f, %.1f, %+.1f, %.2f", cells.empty() ? "" : "<br>", audition::sourceName(s),
                          audition::className(r.cls), r.correlation, r.spectralDist, r.gainDb, r.peak);
            cells += buf;
            // render list entries: "id" (first tagged source) or "id@source"
            if (!wavDir.empty()) {
                const std::string want = p.id + "@" + std::to_string(s);
                const bool byId = ("," + wavIds + ",").find("," + p.id + ",") != std::string::npos && s == srcs.front();
                const bool bySrc = ("," + wavIds + ",").find("," + want + ",") != std::string::npos;
                if (byId || bySrc) audition::writeWav(wavDir + "/" + p.id + "__" + fileSafe(audition::sourceName(s)) + ".wav", r.outL, r.outR);
            }
        }
        const bool transf = nT * 2 >= nRun && nT >= std::min(2, nRun);  // transformative on at least half (>= 2) of its sources
        if (!utility) {
            ++creative[p.category];
            transformative[p.category] += transf;
        }
        const char* intent = utility ? "utility / enhancement" : (transf ? "transformative" : "character");
        if (!ok) ++failures;
        std::fprintf(out, "| %s | %s | %s | %s | %s | %s |\n", p.name.c_str(), p.category.c_str(), chain.c_str(), intent, cells.c_str(), ok ? "pass" : "**FAIL**");
        std::fflush(out);
    }
    int c = 0, t = 0;
    std::fprintf(out, "\n| Category | Creative presets | Transformative |\n|---|---|---|\n");
    for (const auto& [k, v] : creative) { std::fprintf(out, "| %s | %d | %d |\n", k.c_str(), v, transformative[k]); c += v; t += transformative[k]; }
    std::fprintf(out, "\n**%d of %d creative presets are transformative (%.0f %%); %d level/peak failures.**\n", t, c, c ? 100.0 * t / c : 0.0, failures);
    if (out != stdout) std::fclose(out);
    std::printf("%d of %d creative presets transformative; %d failures\n", t, c, failures);
    return failures ? 1 : 0;
}
