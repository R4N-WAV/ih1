// Offline CPU render of the formation (the same simulation and compositing as the plug-in's software path),
// for comparing against the reference video and for screenshots.
//
// Usage: formation_preview <outDir> <chain> [seconds=6] [width=1280] [height=800] [fps=30] [every=0.5]
//   <chain> is a comma-separated list of FX type ids in processing order, e.g. "delay,reverb,harmony",
//   or a number N for the first N of a built-in mixed list, or "none". Duplicates get duplicate shades.
//   Add "+commit" to the chain spec to commit the last instance at t = 1 s (transfer animation).
// Writes PPM frames to outDir every `every` seconds. Env: IH1_EXPOSURE, IH1_QUALITY (0/1/2), IH1_STATS.
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "ih1/visual/Formation.h"
#include "ih1/visual/Raster.h"

using namespace ih1::visual;

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: %s outDir chain [seconds] [w] [h] [fps] [every]\n", argv[0]); return 1; }
    const std::string out = argv[1];
    std::string spec = argv[2];
    const float seconds = argc > 3 ? static_cast<float>(std::atof(argv[3])) : 6.0f;
    const int W = argc > 4 ? std::atoi(argv[4]) : 1280, H = argc > 5 ? std::atoi(argv[5]) : 800;
    const int fps = argc > 6 ? std::atoi(argv[6]) : 30;
    const float every = argc > 7 ? static_cast<float>(std::atof(argv[7])) : 0.5f;
    const float exposure = std::getenv("IH1_EXPOSURE") ? static_cast<float>(std::atof(std::getenv("IH1_EXPOSURE"))) : 1.6f;
    const int q = std::getenv("IH1_QUALITY") ? std::atoi(std::getenv("IH1_QUALITY")) : 1;

    bool commitLast = false;
    if (const auto p = spec.find("+commit"); p != std::string::npos) { commitLast = true; spec.erase(p); }
    std::vector<std::string> types;
    static const char* mixed[] = {"delay", "reverb", "harmony", "chorus", "granular", "saturation", "tape_echo", "spectral", "filter", "eq", "reverse", "pitch_shift"};
    if (spec != "none" && !spec.empty()) {
        if (std::isdigit(static_cast<unsigned char>(spec[0]))) {
            for (int i = 0; i < std::atoi(spec.c_str()); ++i) types.push_back(mixed[i % 12]);
        } else {
            std::stringstream ss(spec);
            std::string t;
            while (std::getline(ss, t, ',')) if (!t.empty()) types.push_back(t);
        }
    }

    Formation form(static_cast<Quality>(std::clamp(q, 0, 2)));
    Canvas canvas(W, H);
    FrameInput in;
    in.aspect = static_cast<float>(W) / static_cast<float>(H);
    in.dt = 1.0f / static_cast<float>(fps);
    std::map<std::string, int> rank;
    std::vector<InstanceInput> all;
    for (size_t i = 0; i < types.size(); ++i) {
        const int shade = rank[types[i]]++;
        all.push_back({static_cast<uint64_t>(1000 + i), fxColour(types[i], shade), static_cast<uint32_t>(7919u * (i + 3)), 0.0f});
    }
    in.chain = all;
    if (commitLast && !in.chain.empty()) in.chain.pop_back();

    const int frames = static_cast<int>(seconds * static_cast<float>(fps));
    const int saveEvery = std::max(1, static_cast<int>(std::lround(every * static_cast<float>(fps))));
    int saved = 0;
    double simMs = 0;
    std::vector<uint8_t> rgb;
    for (int fr = 0; fr < frames; ++fr) {
        const float t = static_cast<float>(fr) / static_cast<float>(fps);
        in.transfers.clear();
        if (commitLast && fr == fps && in.chain.size() < all.size()) {
            in.chain.push_back(all.back());
            in.transfers.push_back({all.back().id, 0.30f});
        }
        // synthetic music: phrase swells
        const float lv = 0.04f + 0.03f * std::sin(t * 1.3f) + 0.02f * std::sin(t * 4.1f);
        for (size_t i = 0; i < in.chain.size(); ++i) in.chain[i].level = lv * (0.8f + 0.4f * std::sin(t * 0.7f + static_cast<float>(i)));
        const auto t0 = std::chrono::steady_clock::now();
        form.update(in);
        simMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        canvas.decay(form.decay(in.dt));
        canvas.draw(form.segments(), static_cast<float>(H));
        if (fr % saveEvery == saveEvery - 1) {
            canvas.composite(rgb, exposure);
            char name[64];
            std::snprintf(name, sizeof name, "/f%03d.ppm", saved++);
            if (FILE* f = std::fopen((out + name).c_str(), "wb")) {
                std::fprintf(f, "P6\n%d %d\n255\n", W, H);
                std::fwrite(rgb.data(), 1, rgb.size(), f);
                std::fclose(f);
            }
        }
    }
    std::printf("instances %d, strands %d, segments %zu, sim %.2f ms/frame\n", form.instanceCount(), form.strandCount(), form.segments().size(), simMs / std::max(1, frames));
    return 0;
}
