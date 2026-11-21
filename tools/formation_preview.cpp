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
#include "ih1/visual/FieldVolume.h"
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
    const float exposure = std::getenv("IH1_EXPOSURE") ? static_cast<float>(std::atof(std::getenv("IH1_EXPOSURE"))) : 2.2f;
    const float bloom = std::getenv("IH1_BLOOM") ? static_cast<float>(std::atof(std::getenv("IH1_BLOOM"))) : 0.45f;
    const float mono = std::getenv("IH1_MONO") ? static_cast<float>(std::atof(std::getenv("IH1_MONO"))) : 0.0f;
    const int q = std::getenv("IH1_QUALITY") ? std::atoi(std::getenv("IH1_QUALITY")) : 1;
    // V3: the field volume is the default; IH1_STRANDS=1 renders the V2.1 strands for comparison
    const bool volume = !std::getenv("IH1_STRANDS");
    const int steps = std::getenv("IH1_STEPS") ? std::atoi(std::getenv("IH1_STEPS")) : 64;

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
    form.setTraceStrands(!volume);
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
        in.audio.energy = 0.25f + 0.2f * std::sin(t * 1.3f);
        in.audio.low = 0.3f + 0.2f * std::sin(t * 0.5f);
        in.audio.high = 0.2f + 0.15f * std::sin(t * 2.3f);
        in.audio.transient = std::fmod(t, 1.0f) < 1.0f / static_cast<float>(fps) ? 0.8f : 0.0f;  // a beat per second
        in.monochrome = mono;
        const auto t0 = std::chrono::steady_clock::now();
        form.update(in);
        simMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (!volume) {
            canvas.decay(form.decay(in.dt));
            canvas.draw(form.segments(), static_cast<float>(H), std::sqrt(std::clamp(860.0f / static_cast<float>(H), 0.36f, 2.6f)));
        }
        if (fr % saveEvery == saveEvery - 1) {
            if (volume) {
                // V3: the field volume (the same function the GL shader runs), hue-preserving tone map, no bloom
                std::vector<float> lin;
                renderVolume(form.scene(), W, H, lin, steps);
                rgb.resize(static_cast<size_t>(W) * H * 3);
                for (size_t k = 0; k < static_cast<size_t>(W) * H; ++k) {
                    const Rgb o = toneMap({lin[k * 3], lin[k * 3 + 1], lin[k * 3 + 2]}, exposure);
                    rgb[k * 3] = static_cast<uint8_t>(std::clamp(o.r * 255.0f + 0.5f, 0.0f, 255.0f));
                    rgb[k * 3 + 1] = static_cast<uint8_t>(std::clamp(o.g * 255.0f + 0.5f, 0.0f, 255.0f));
                    rgb[k * 3 + 2] = static_cast<uint8_t>(std::clamp(o.b * 255.0f + 0.5f, 0.0f, 255.0f));
                }
            } else {
                canvas.composite(rgb, exposure, bloom, &form.glows(), static_cast<float>(H));
            }
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
