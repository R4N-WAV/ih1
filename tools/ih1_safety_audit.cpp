// IH1 safety audit (V3 Phase 8.1): reproduces "sharp volume jump when landing on an FX" and transition clicks.
//
// For every production FX type and several cached drafts (defaults + seeded random drafts + a "hot" draft with
// every gain-like control at maximum), the tool runs the real Engine + Session:
//   1. stage the FX, write the draft, un-stage it (None) -- the draft stays cached, as in the product;
//   2. play 1 s of programme through the empty chain;
//   3. land on the FX again while audio runs (Session::select), exactly the user's carousel gesture;
//   4. measure, per 250 ms programme period, the loudest 5 ms RMS window: the overshoot is the loudest period in the
//      first 0.5 s after landing minus the loudest steady period (1.5..3 s). A rising tail (reverb) is negative.
//   5. measure the largest sample-to-sample step near the switch against the steady-state largest step (sine source).
// Programme: drum-like noise bursts + a sustained chord at about -14 dBFS RMS ("normal"), and +8 dB hotter ("hot").
//
// Usage: ih1_safety_audit [threshold_dB=2] [filter-substring]
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/engine/Engine.h"
#include "ih1/model/Session.h"

using namespace ih1;

namespace {
constexpr double kSr = 48000.0;
constexpr int kBlock = 256;

std::vector<float> programme(int n, float gain, uint32_t seed, bool sineOnly) {
    std::vector<float> x(static_cast<size_t>(n) * 2);
    dsp::Rng rng;
    rng.seed_(seed);
    const int period = static_cast<int>(0.25 * kSr);
    float env = 0.0f;
    for (int i = 0; i < n; ++i) {
        const double t = i / kSr;
        float chord = 0.25f * static_cast<float>(std::sin(2 * dsp::kPi * 220.0 * t) + 0.7 * std::sin(2 * dsp::kPi * 277.2 * t) + 0.5 * std::sin(2 * dsp::kPi * 329.6 * t));
        if (sineOnly) chord = 0.5f * static_cast<float>(std::sin(2 * dsp::kPi * 220.0 * t));
        if (!sineOnly && i % period == 0) env = 1.0f;
        env *= 0.9993f;
        const float burst = sineOnly ? 0.0f : env * 0.8f * rng.bipolar();
        x[static_cast<size_t>(i) * 2] = gain * (chord * 0.6f + burst);
        x[static_cast<size_t>(i) * 2 + 1] = gain * (chord * 0.6f - 0.3f * burst + 0.7f * burst);
    }
    return x;
}

struct Result {
    float overshootDb = 0, stepRatio = 0, peakFirst = 0, peakSteady = 0;
    bool nonFinite = false;
};

// Render: returns stereo-interleaved output; selects `type` at sample `atSample`.
Result run(int type, const ParamValues& draft, float gain, bool sineOnly) {
    Engine engine;
    Session session(engine);
    engine.setSafety(0, 0.0f);  // measure the FX themselves, not the Output Safety stage
    session.prepare(kSr, kBlock);
    const auto& d = fxRegistry()[static_cast<size_t>(type)];
    session.select(type);
    for (size_t i = 0; i < d.params.size(); ++i) session.setDraftParam(static_cast<int>(i), draft[i]);
    session.select(-1);
    session.service();
    const int total = static_cast<int>(4.0 * kSr);
    const int at = static_cast<int>(1.0 * kSr);
    auto in = programme(total, gain, 7, sineOnly);
    std::vector<float> outL(static_cast<size_t>(total)), outR(static_cast<size_t>(total));
    std::vector<float> L(kBlock), R(kBlock);
    ProcessContext ctx;
    ctx.sampleRate = kSr;
    Result res;
    for (int off = 0; off < total; off += kBlock) {
        if (at >= off && at < off + kBlock) session.select(type);
        session.service();
        const int n = std::min(kBlock, total - off);
        for (int i = 0; i < n; ++i) {
            L[static_cast<size_t>(i)] = in[static_cast<size_t>(off + i) * 2];
            R[static_cast<size_t>(i)] = in[static_cast<size_t>(off + i) * 2 + 1];
        }
        engine.process(L.data(), R.data(), n, ctx);
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(L[static_cast<size_t>(i)]) || !std::isfinite(R[static_cast<size_t>(i)])) res.nonFinite = true;
            outL[static_cast<size_t>(off + i)] = L[static_cast<size_t>(i)];
            outR[static_cast<size_t>(off + i)] = R[static_cast<size_t>(i)];
        }
    }
    const int period = static_cast<int>(0.25 * kSr), win = static_cast<int>(0.005 * kSr);
    auto loudest = [&](int a, int b) {
        float m = 0.0f;
        for (int s = a; s + win <= b; s += win / 2) {
            double e = 0;
            for (int i = s; i < s + win; ++i) e += 0.5 * (outL[static_cast<size_t>(i)] * outL[static_cast<size_t>(i)] + outR[static_cast<size_t>(i)] * outR[static_cast<size_t>(i)]);
            m = std::max(m, static_cast<float>(std::sqrt(e / win)));
        }
        return m;
    };
    auto peak = [&](int a, int b) {
        float m = 0.0f;
        for (int i = a; i < b; ++i) m = std::max({m, std::fabs(outL[static_cast<size_t>(i)]), std::fabs(outR[static_cast<size_t>(i)])});
        return m;
    };
    if (std::getenv("IH1_AUDIT_TRACE"))
        for (int p = -2; p < 12; ++p) std::printf("  period %3d loudest %.3f\n", p, static_cast<double>(loudest(at + p * period, at + (p + 1) * period)));
    float first = 0.0f, steady = 1e-9f;
    for (int p = 0; p < 2; ++p) first = std::max(first, loudest(at + p * period, at + (p + 1) * period));
    for (int p = 6; p < 12; ++p) steady = std::max(steady, loudest(at + p * period, at + (p + 1) * period));
    // what the ear compares a jump against: the louder of the dry programme just before landing and the FX's own
    // steady state (an FX that is simply quieter than the dry, or whose tail builds up, is not a jump)
    float before = 0.0f;
    for (int p = 0; p < 3; ++p) before = std::max(before, loudest(at - (p + 1) * period, at - p * period));
    steady = std::max(steady, before);
    res.overshootDb = 20.0f * std::log10(std::max(first, 1e-9f) / steady);
    res.peakFirst = peak(at, at + 2 * period);
    res.peakSteady = std::max(peak(at + 6 * period, at + 12 * period), peak(at - 3 * period, at));
    auto maxStep = [&](int a, int b) {
        float m = 0.0f;
        for (int i = std::max(1, a); i < b; ++i)
            m = std::max({m, std::fabs(outL[static_cast<size_t>(i)] - outL[static_cast<size_t>(i - 1)]), std::fabs(outR[static_cast<size_t>(i)] - outR[static_cast<size_t>(i - 1)])});
        return m;
    };
    const float sw = maxStep(at - 960, at + 4800), st = std::max(maxStep(at + 72000, at + 144000), maxStep(at - 40000, at - 1000));
    res.stepRatio = sw / std::max(st, 1e-9f);
    return res;
}

ParamValues hotDraft(const FxDescriptor& d) {
    ParamValues v = d.defaults();
    for (size_t i = 0; i < d.params.size(); ++i) {
        const auto& s = d.params[i];
        std::string id = s.id;
        const bool gainLike = id.find("drive") != std::string::npos || id.find("gain") != std::string::npos || id == "out" ||
                              id.find("feedback") != std::string::npos || id.find("resonance") != std::string::npos ||
                              id == "mix" || id.find("amount") != std::string::npos || id.find("level") != std::string::npos;
        if (gainLike && id != "auto_gain") v[i] = s.maxValue;
    }
    return d.clamp(v);
}

ParamValues randomDraft(const FxDescriptor& d, uint32_t seed) {
    dsp::Rng rng;
    rng.seed_(seed);
    ParamValues v{};
    for (size_t i = 0; i < d.params.size(); ++i) v[i] = d.params[i].fromNormalised(rng.uniform());
    return d.clamp(v);
}
}  // namespace

int main(int argc, char** argv) {
    const float thr = argc > 1 ? static_cast<float>(std::atof(argv[1])) : 2.0f;
    const char* filter = argc > 2 ? argv[2] : nullptr;
    int flagged = 0, runs = 0;
    std::printf("%-18s %-8s %-7s %9s %9s %9s %8s\n", "fx", "draft", "input", "over dB", "pk first", "pk stdy", "step x");
    for (int t : fxCarouselOrder()) {
        const auto& d = fxRegistry()[static_cast<size_t>(t)];
        if (filter && d.typeId.find(filter) == std::string::npos) continue;
        struct Case { const char* name; ParamValues v; };
        std::vector<Case> cases = {{"default", d.defaults()}, {"hot", hotDraft(d)}};
        for (uint32_t k = 1; k <= 3; ++k) cases.push_back({k == 1 ? "rand1" : (k == 2 ? "rand2" : "rand3"), randomDraft(d, 1000u * k + static_cast<uint32_t>(t))});
        for (const auto& c : cases)
            for (int level = 0; level < 2; ++level) {
                const float g = level ? 0.5f : 0.2f;
                const Result r = run(t, c.v, g, false);
                const Result s = run(t, c.v, g, true);
                ++runs;
                const bool bad = r.overshootDb > thr || r.nonFinite || s.nonFinite || s.stepRatio > 3.0f;
                if (bad || !filter) {
                    if (bad || std::strcmp(c.name, "default") == 0)
                        std::printf("%-18s %-8s %-7s %9.2f %9.3f %9.3f %8.2f%s\n", d.typeId.c_str(), c.name, level ? "hot" : "normal",
                                    r.overshootDb, r.peakFirst, r.peakSteady, s.stepRatio, bad ? "  <-- FLAG" : "");
                }
                flagged += bad ? 1 : 0;
            }
    }
    std::printf("\n%d of %d cases flagged (overshoot > %.1f dB, step ratio > 3, or non-finite)\n", flagged, runs, static_cast<double>(thr));
    return flagged ? 1 : 0;
}
