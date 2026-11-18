// CPU benchmark: every FX alone, and long chains of duplicates, at a given sample rate / block size.
// Usage: ih1_bench [sampleRate=48000] [block=256] [seconds=5]
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/engine/Engine.h"
#include "ih1/fx/FxProcessor.h"
#include "ih1/model/Session.h"

using namespace ih1;

static double runFx(int type, double sr, int block, double seconds) {
    const auto& d = fxRegistry()[static_cast<size_t>(type)];
    auto fx = d.create();
    fx->prepare(sr, block);
    fx->setParams(d.defaults(), true);
    std::vector<float> L(static_cast<size_t>(block)), R(static_cast<size_t>(block));
    dsp::Rng rng(1);
    ProcessContext ctx;
    ctx.sampleRate = sr;
    const int blocks = static_cast<int>(seconds * sr / block);
    double busy = 0;
    for (int b = 0; b < blocks; ++b) {
        for (int i = 0; i < block; ++i) { L[static_cast<size_t>(i)] = 0.2f * rng.bipolar(); R[static_cast<size_t>(i)] = L[static_cast<size_t>(i)]; }
        float* ch[2] = {L.data(), R.data()};
        const auto t0 = std::chrono::steady_clock::now();
        fx->process(ch, block, ctx);
        busy += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }
    return busy / seconds * 100.0;  // % of one core in real time
}

static double runChain(int type, int count, double sr, int block, double seconds, double& worstBlockUs) {
    Engine engine;
    Session session(engine);
    session.prepare(sr, block);
    for (int i = 0; i < count; ++i) { session.select(type); session.commit(); }
    session.prepare(sr, block);
    std::vector<float> L(static_cast<size_t>(block)), R(static_cast<size_t>(block));
    dsp::Rng rng(2);
    ProcessContext ctx;
    ctx.sampleRate = sr;
    const int blocks = static_cast<int>(seconds * sr / block);
    double busy = 0;
    worstBlockUs = 0;
    for (int b = 0; b < blocks; ++b) {
        for (int i = 0; i < block; ++i) { L[static_cast<size_t>(i)] = 0.2f * rng.bipolar(); R[static_cast<size_t>(i)] = L[static_cast<size_t>(i)]; }
        const auto t0 = std::chrono::steady_clock::now();
        engine.process(L.data(), R.data(), block, ctx);
        const double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        busy += dt;
        worstBlockUs = std::max(worstBlockUs, dt * 1e6);
        session.service();
    }
    return busy / seconds * 100.0;
}

int main(int argc, char** argv) {
    const double sr = argc > 1 ? std::atof(argv[1]) : 48000.0;
    const int block = argc > 2 ? std::atoi(argv[2]) : 256;
    const double seconds = argc > 3 ? std::atof(argv[3]) : 5.0;
    std::printf("IH1 benchmark: %.0f Hz, block %d, %.1f s of audio per measurement\n\n", sr, block, seconds);
    std::printf("%-18s %10s %10s\n", "FX", "CPU %", "latency");
    const auto& reg = fxRegistry();
    for (size_t t = 0; t < reg.size(); ++t) {
        if (reg[t].experimental) continue;
        auto fx = reg[t].create();
        fx->prepare(sr, block);
        fx->setParams(reg[t].defaults(), true);
        std::printf("%-18s %9.2f%% %8d smp\n", reg[t].name.c_str(), runFx(static_cast<int>(t), sr, block, seconds), fx->latencySamples());
    }
    std::printf("\nDuplicate chains (engine incl. routing; block budget = %.0f us)\n", block / sr * 1e6);
    for (const char* id : {"delay", "reverb", "chorus", "saturation", "granular", "harmony"}) {
        for (int count : {8, 32}) {
            double worst = 0;
            const double cpu = runChain(fxTypeIndex(id), count, sr, block, seconds / 2, worst);
            std::printf("%-12s x%-3d %8.1f%%  worst block %8.0f us\n", id, count, cpu, worst);
        }
    }
    return 0;
}
