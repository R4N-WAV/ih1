// IH1 grain click audit (V3 Phase 8.4). Renders a 110 Hz sine (-6 dBFS) through the Grain Engine FX for many
// seeded random settings and counts discontinuities: samples whose second difference exceeds what a smooth grain
// could produce (sine curvature at the highest pitch the settings allow + the window's own curvature), by 8x.
// Long runs (16 s) wrap the 12 s capture ring. Usage: ih1_grain_clicks [cases=400] [seconds=6]
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/fx/FxProcessor.h"

using namespace ih1;

int main(int argc, char** argv) {
    const int cases = argc > 1 ? std::atoi(argv[1]) : 400;
    const double secs = argc > 2 ? std::atof(argv[2]) : 6.0;
    const double sr = 48000.0;
    const int t = fxTypeIndex("grain_engine");
    const auto& d = fxRegistry()[static_cast<size_t>(t)];
    auto idx = [&](const char* id) { return d.paramIndex(id); };
    int bad = 0;
    double worst = 0;
    std::vector<int> hist(8, 0);
    for (int k = 0; k < cases; ++k) {
        dsp::Rng rng;
        rng.seed_(9000u + static_cast<uint32_t>(k));
        ParamValues v = d.defaults();
        for (size_t i = 0; i < d.params.size(); ++i) v[i] = d.params[i].fromNormalised(rng.uniform());
        v[static_cast<size_t>(idx("mix"))] = 100.0f;
        v[static_cast<size_t>(idx("gain"))] = 0.0f;
        if (std::getenv("IH1_NO_FB_CHAR")) { v[static_cast<size_t>(idx("fb_rate"))] = 48000.0f; v[static_cast<size_t>(idx("fb_drive"))] = 0.0f; }
        v = d.clamp(v);
        auto fx = d.create();
        fx->prepare(sr, 256);
        fx->setParams(v, true);
        const int total = static_cast<int>((k % 10 == 9 ? 16.0 : secs) * sr);
        std::vector<float> L(256), R(256);
        ProcessContext ctx;
        ctx.sampleRate = sr;
        float y1 = 0, y2 = 0;
        int clicks = 0, firstAt = -1;
        double maxD2 = 0;
        const float pitchMax = std::fabs(v[static_cast<size_t>(idx("pitch"))]) + v[static_cast<size_t>(idx("pitch_jit"))];
        const bool vs = v[static_cast<size_t>(idx("varispeed"))] > 0.5f;
        const double ratio = std::exp2(std::min(36.0f, pitchMax) / 12.0) * (vs ? 2.0 : 1.0);
        const double sizeS = std::max(0.004, v[static_cast<size_t>(idx("size_ms"))] * 0.001 * 0.25);
        // allowed curvature: sine at the highest pitch, times a generous factor for overlap of several grains, plus
        // the window (pluck/swell have a 3 % raised-cosine edge)
        const double A = 0.5 * 4.0;
        const double sineC = A * std::pow(2 * dsp::kPi * 110.0 * ratio / sr, 2.0);
        const double edge = std::max(8.0, 0.03 * sizeS * sr);
        const double winC = A * 0.5 * std::pow(dsp::kPi / edge, 2.0);
        const double allowed = 8.0 * (sineC + winC) + 1e-4;
        for (int off = 0; off < total; off += 256) {
            for (int i = 0; i < 256; ++i) {
                L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = 0.5f * static_cast<float>(std::sin(2 * dsp::kPi * 110.0 * (off + i) / sr));
            }
            float* ch[2] = {L.data(), R.data()};
            fx->process(ch, 256, ctx);
            for (int i = 0; i < 256; ++i) {
                const float y = L[static_cast<size_t>(i)];
                const double d2 = std::fabs(y - 2.0f * y1 + y2);
                if (off + i > 4) {
                    maxD2 = std::max(maxD2, d2);
                    if (d2 > allowed) { if (firstAt < 0) firstAt = off + i; ++clicks; }
                }
                y2 = y1;
                y1 = y;
            }
        }
        worst = std::max(worst, maxD2 / allowed);
        if (clicks > 0) {
            ++bad;
            const char* modes[] = {"Cloud", "Stretch", "Loop"};
            std::printf("case %3d %4.0fs clicks %5d first %.3fs  ratio %.1f | mode %s fr %d size %.0f dens %.1f pos %.0f pitch %.1f+-%.1f speed %.2f fb %.0f rev %.0f win %d retrig %d vari %d spray %.0f align %d delay %.0f\n",
                        k, total / sr, clicks, firstAt / sr, maxD2 / allowed, modes[std::clamp(static_cast<int>(v[static_cast<size_t>(idx("mode"))]), 0, 2)],
                        static_cast<int>(v[static_cast<size_t>(idx("freeze"))]), v[static_cast<size_t>(idx("size_ms"))], v[static_cast<size_t>(idx("density"))],
                        v[static_cast<size_t>(idx("position_ms"))], v[static_cast<size_t>(idx("pitch"))], v[static_cast<size_t>(idx("pitch_jit"))],
                        v[static_cast<size_t>(idx("speed"))], v[static_cast<size_t>(idx("feedback"))], v[static_cast<size_t>(idx("reverse"))],
                        static_cast<int>(v[static_cast<size_t>(idx("window"))]), static_cast<int>(v[static_cast<size_t>(idx("retrig"))]), vs ? 1 : 0,
                        v[static_cast<size_t>(idx("spray_ms"))], static_cast<int>(v[static_cast<size_t>(idx("align"))]), v[static_cast<size_t>(idx("delay_ms"))]);
        }
    }
    std::printf("\n%d of %d cases with discontinuities (worst = %.1fx the smooth bound)\n", bad, cases, worst);
    return bad ? 1 : 0;
}
