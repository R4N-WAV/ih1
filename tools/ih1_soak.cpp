// ih1_soak: long-running stability soak of the IH1 engine (no host, no GUI).
//
// An audio thread processes continuously with irregular block sizes (a drum-like loop plus a sustained
// chord) while the message thread performs random workflow operations every few milliseconds: stage and
// commit (including the heavy FX: Grain Engine, Harmony, Convolution, Spectral), delete, reorder, Clear FX
// Chain, latency-mode switches, factory preset loads, RESET and staged-parameter automation. Session::service
// runs as the plug-in's timer would.
//
// Reported: blocks processed, worst block time vs. the real-time budget (at 48 kHz), non-finite samples
// (must be 0), output peak, resident memory at start / end / maximum (growth = leak signal), number of
// operations. Usage: ih1_soak [seconds=1800] [maxChain=12]. Exit code 1 on non-finite output or a
// sustained memory growth above 64 MB.
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

#include "ih1/engine/Engine.h"
#include "ih1/model/Session.h"
#include "ih1/presets/Presets.h"

using namespace ih1;
using Clock = std::chrono::steady_clock;

static double rssMb() {
    std::ifstream f("/proc/self/statm");
    long pages = 0, resident = 0;
    if (f >> pages >> resident) return static_cast<double>(resident) * 4096.0 / (1024.0 * 1024.0);
    return 0.0;
}

int main(int argc, char** argv) {
    const double seconds = argc > 1 ? std::atof(argv[1]) : 1800.0;
    const int maxChain = argc > 2 ? std::atoi(argv[2]) : 12;
    constexpr double kSr = 48000.0;
    constexpr int kMaxBlock = 1024;
    Engine engine;
    Session session(engine);
    session.prepare(kSr, kMaxBlock);

    std::atomic<bool> stop{false};
    std::atomic<long long> blocks{0}, samples{0}, nonFinite{0};
    std::atomic<double> worstRatio{0.0}, worstRatio128{0.0}, worstMs{0.0}, peak{0.0};
    std::thread audio([&] {
        std::vector<float> L(kMaxBlock), R(kMaxBlock);
        std::mt19937 rng(1);
        long long n0 = 0;
        uint32_t nz = 99;
        while (!stop.load()) {
            const int n = 1 + static_cast<int>(rng() % kMaxBlock);
            for (int i = 0; i < n; ++i) {
                const double t = static_cast<double>(n0 + i) / kSr;
                const double beat = std::fmod(t, 0.5);
                nz = nz * 1664525u + 1013904223u;
                const float noise = static_cast<float>(nz >> 9) / 8388608.0f - 1.0f;
                float v = 0.5f * static_cast<float>(std::exp(-beat * 18.0) * std::sin(2 * 3.14159265 * 60.0 * beat));
                v += 0.05f * noise * static_cast<float>(std::exp(-std::fmod(t, 0.125) * 80.0));
                v += 0.08f * static_cast<float>(std::sin(2 * 3.14159265 * 220.0 * t) + std::sin(2 * 3.14159265 * 330.0 * t));
                L[static_cast<size_t>(i)] = v;
                R[static_cast<size_t>(i)] = 0.9f * v;
            }
            ProcessContext ctx;
            ctx.sampleRate = kSr;
            ctx.bpm = 120.0;
            ctx.hostPlaying = true;
            ctx.ppqValid = true;
            ctx.ppqPosition = static_cast<double>(n0) / kSr * 2.0;
            const auto t0 = Clock::now();
            engine.process(L.data(), R.data(), n, ctx);
            const double used = std::chrono::duration<double>(Clock::now() - t0).count();
            const double budget = n / kSr;
            if (used / budget > worstRatio.load()) worstRatio.store(used / budget);
            if (n >= 128 && used / budget > worstRatio128.load()) worstRatio128.store(used / budget);  // host-like block sizes
            if (used * 1000.0 > worstMs.load()) worstMs.store(used * 1000.0);
            double pk = peak.load();
            for (int i = 0; i < n; ++i) {
                if (!std::isfinite(L[static_cast<size_t>(i)]) || !std::isfinite(R[static_cast<size_t>(i)])) nonFinite.fetch_add(1);
                pk = std::max(pk, static_cast<double>(std::fabs(L[static_cast<size_t>(i)])));
            }
            peak.store(pk);
            n0 += n;
            blocks.fetch_add(1);
            samples.fetch_add(n);
        }
    });

    const char* heavy[] = {"grain_engine", "harmony", "convolution", "spectral", "reverb", "shimmer", "granular", "pitch_shift",
                           "tape_echo", "delay", "chorus", "saturation", "amp", "vocoder", "formant", "stutter", "looper", "eq", "compressor"};
    std::mt19937 rng(7);
    const auto start = Clock::now();
    const double rss0 = rssMb();
    double rssMax = rss0;
    long long ops = 0;
    double nextReport = 60.0;
    auto elapsed = [&] { return std::chrono::duration<double>(Clock::now() - start).count(); };
    std::printf("soak: %.0f s, max chain %d, start RSS %.1f MB\n", seconds, maxChain, rss0);
    std::fflush(stdout);
    while (elapsed() < seconds) {
        const int r = static_cast<int>(rng() % 100);
        if (r < 30) {
            session.select(fxTypeIndex(heavy[rng() % (sizeof heavy / sizeof heavy[0])]));
            if (rng() % 2 && static_cast<int>(session.chain().size()) < maxChain) session.commit();
        } else if (r < 50) {
            if (session.selected() >= 0) {
                const auto& d = fxRegistry()[static_cast<size_t>(session.selected())];
                const int k = static_cast<int>(rng() % d.params.size());
                session.setDraftParam(k, d.params[static_cast<size_t>(k)].fromNormalised(static_cast<float>(rng() % 1000) / 1000.0f));
            }
        } else if (r < 60) {
            if (!session.chain().empty()) session.remove(session.chain()[rng() % session.chain().size()].id());
        } else if (r < 70) {
            const auto n = session.chain().size();
            if (n > 1) session.move(static_cast<int>(rng() % n), static_cast<int>(rng() % n));
        } else if (r < 73) {
            session.clearChain();
        } else if (r < 75) {
            session.setLatencyMode(static_cast<LatencyMode>(rng() % 3));
        } else if (r < 80) {
            const auto& all = factoryPresets();
            session.fromJson(presetToJson(all[rng() % all.size()]), false);
        } else if (r < 83) {
            session.resetDraft();
        } else if (r < 86) {
            session.setSolo(rng() % 2);
        } else if (r < 89) {
            session.setMute(rng() % 2);
        }
        ++ops;
        session.service();
        std::this_thread::sleep_for(std::chrono::milliseconds(2 + static_cast<int>(rng() % 20)));
        rssMax = std::max(rssMax, rssMb());
        if (elapsed() >= nextReport) {
            std::printf("t=%5.0f s  blocks %lld  audio %.0f s  worst %.2fx budget (blocks>=128: %.2fx, %.2f ms)  RSS %.1f MB  ops %lld  chain %zu  non-finite %lld\n",
                        elapsed(), blocks.load(), static_cast<double>(samples.load()) / kSr, worstRatio.load(), worstRatio128.load(), worstMs.load(), rssMb(), ops,
                        session.chain().size(), nonFinite.load());
            std::fflush(stdout);
            nextReport += 60.0;
        }
    }
    stop = true;
    audio.join();
    // leak check: drop everything the session owns, free retired programs, and let glibc return free pages,
    // so the comparison measures memory still held (a leak), not allocator caching of freed 8 MB grain buffers
    session.clearChain();
    session.select(-1);
    for (int i = 0; i < 10; ++i) session.service();
    {
        std::vector<float> L(256), R(256);
        ProcessContext ctx;
        for (int i = 0; i < 20; ++i) { engine.process(L.data(), R.data(), 256, ctx); session.service(); }
    }
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
    const double rss1 = rssMb();
    std::printf("\nSOAK SUMMARY: %.0f s wall, %lld blocks, %.0f s of audio (%.1fx real time),\n"
                "  worst block %.2fx its real-time budget (any size, incl. 1-sample blocks), %.2fx for blocks >= 128, worst %.2f ms,\n"
                "  output peak %.3f, non-finite samples %lld, %lld operations, RSS start %.1f / end (chain cleared, trimmed) %.1f / max %.1f MB\n",
                elapsed(), blocks.load(), static_cast<double>(samples.load()) / kSr, static_cast<double>(samples.load()) / kSr / elapsed(),
                worstRatio.load(), worstRatio128.load(), worstMs.load(), peak.load(), nonFinite.load(), ops, rss0, rss1, rssMax);
    const bool ok = nonFinite.load() == 0 && (rss1 - rss0) < 64.0;
    std::printf("%s\n", ok ? "SOAK PASSED" : "SOAK FAILED");
    return ok ? 0 : 1;
}
