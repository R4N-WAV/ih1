// IH1 click / pop audit for every effect that cuts, slices, gates, retriggers, freezes or moves a read head
// (V4 Phase 4.4). Generic: it uses the registry only, so it runs unchanged on any build.
//
// Method. Inputs are smooth and band-limited (a sine at 110 / 440 Hz, -6 dBFS, with a stereo phase offset, optionally
// riding on a DC offset), so any isolated burst of energy above ~6 kHz in the output is a discontinuity created by
// the effect, not by the input. The output is high-passed (4th-order at 6 kHz); its energy is measured in 1 ms
// windows and each window is compared with the median of its +-60 ms neighbourhood. A click is a window more than
// `ratio` dB above that local median and above an absolute floor (-62 dBFS). Steady intentional grit (bit crushing,
// aliasing, folding) raises the median with it and is not counted; a splice, a hard gate edge or a read-head jump is.
// Each case: random settings (seeded), random tempo, a parameter re-randomisation (smoothed, as a host would send
// it) every 0.35-0.9 s for half the parameters, and a reset() in the middle (transport restart / state reload).
//
// Usage: ih1_click_audit [cases=60] [seconds=4] [ratioDb=26] [type ...]
// Prints per-effect results; exit code 1 if any case clicks.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/fx/FxProcessor.h"

using namespace ih1;

namespace {
// parameters that legitimately inject broadband noise or are meta controls: kept at their defaults / neutral
bool skipParam(const std::string& type, const std::string& id) {
    if (id == "mix" || id == "out" || id == "output" || id == "gain") return true;
    if (id == "noise" || id == "hiss" || id == "crackle" || id == "dust") return true;
    if (type == "texture" ) return true;  // noise imprinter: its whole purpose is added texture (audited separately)
    return false;
}

struct Hp4 {  // two cascaded RBJ high-pass biquads
    float b0, b1, b2, a1, a2;
    float z[2][2][2]{};  // [stage][channel][state]
    void design(float fc, float sr) {
        const float w = 2.0f * dsp::kPi * fc / sr, c = std::cos(w), s = std::sin(w), q = 0.7071f, al = s / (2 * q);
        const float a0 = 1 + al;
        b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = (1 + c) / 2 / a0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    float run(int st, int ch, float x) {
        float* zz = z[st][ch];
        const float y = b0 * x + zz[0];
        zz[0] = b1 * x - a1 * y + zz[1];
        zz[1] = b2 * x - a2 * y;
        return y;
    }
};

const char* kDefaultTypes[] = {"stutter", "glitch", "time_warp", "looper", "reverse", "granular", "grain_engine", "gate", "tremolo",
                               "spectral", "spectral_smear", "spectral_delay", "delay", "tape_echo", "bitcrush", "degrade", "fold_crush",
                               // V4 additions (skipped automatically when absent)
                               "slice_shuffle", "transient_split", "echo_matrix", "reverse_echo", "spectral_shatter", "spectral_graft",
                               "partial_recompose", "modal", "dispersion", "texture"};
}  // namespace

int main(int argc, char** argv) {
    const int cases = argc > 1 ? std::atoi(argv[1]) : 60;
    const double secs = argc > 2 ? std::atof(argv[2]) : 4.0;
    const float ratioDb = argc > 3 ? static_cast<float>(std::atof(argv[3])) : 26.0f;
    std::vector<std::string> types;
    for (int a = 4; a < argc; ++a) types.push_back(argv[a]);
    if (types.empty()) for (const char* t : kDefaultTypes) types.push_back(t);

    const double sr = 48000.0;
    const int block = 256;
    const int N = static_cast<int>(secs * sr);
    const int win = 48;            // 1 ms
    const int ctx = 60;            // +-60 windows = +-60 ms
    const float floorE = std::pow(10.0f, -62.0f / 10.0f);
    int totalBad = 0;
    std::printf("%-18s %6s %6s %8s  %s\n", "effect", "cases", "clicks", "worst dB", "first failing case");
    for (const auto& type : types) {
        const int t = fxTypeIndex(type);
        if (t < 0) continue;
        const auto& d = fxRegistry()[static_cast<size_t>(t)];
        int bad = 0;
        float worst = -99.0f;
        std::string firstBad;
        for (int k = 0; k < cases; ++k) {
            dsp::Rng rng(0xC11Cu + static_cast<uint32_t>(k) * 7919u + static_cast<uint32_t>(t));
            // mid-run changes move continuous controls only: discrete switches (mode, pattern, sync, ...) of the staged
            // effect are crossfaded by the engine (Engine staged switch crossfade, tested in test_v4), and committed
            // effects never change parameters (macros reach continuous parameters only). IH1_CLICK_DISCRETE=1 adds them.
            const bool discrete = std::getenv("IH1_CLICK_DISCRETE") != nullptr;
            auto randomise = [&](ParamValues& v, float fraction) {
                for (size_t i = 0; i < d.params.size(); ++i) {
                    const auto& s = d.params[i];
                    if (skipParam(type, s.id) || rng.uniform() > fraction) continue;
                    if (fraction < 1.0f && s.kind != ParamKind::Float && !discrete) continue;
                    if (const char* only = std::getenv("IH1_CLICK_ONLY"); only && fraction < 1.0f && s.id != only) continue;
                    v[i] = s.fromNormalised(rng.uniform());
                }
            };
            ParamValues v = d.defaults();
            randomise(v, 1.0f);
            if (const char* set = std::getenv("IH1_CLICK_SET")) {  // e.g. "lofi=0,gate=0": isolate a mechanism
                std::string all = set;
                size_t p0 = 0;
                while (p0 < all.size()) {
                    const size_t p1 = std::min(all.find(',', p0), all.size());
                    const std::string kv = all.substr(p0, p1 - p0);
                    const size_t eq = kv.find('=');
                    if (eq != std::string::npos) { const int pi = d.paramIndex(kv.substr(0, eq)); if (pi >= 0) v[static_cast<size_t>(pi)] = static_cast<float>(std::atof(kv.c_str() + eq + 1)); }
                    p0 = p1 + 1;
                }
            }
            const int mi = d.paramIndex("mix");
            if (mi >= 0) v[static_cast<size_t>(mi)] = d.params[static_cast<size_t>(mi)].maxValue;
            if (std::getenv("IH1_CLICK_VERBOSE")) {
                std::printf("   [case %d] bpm-seeded settings:", k);
                for (size_t i = 0; i < d.params.size(); ++i) std::printf(" %s=%.3g", d.params[i].id.c_str(), static_cast<double>(v[i]));
                std::printf("\n");
            }
            auto fx = d.create();
            fx->setLatencyMode(LatencyMode::Quality);
            fx->prepare(sr, block);
            fx->setParams(d.clamp(v), true);
            ProcessContext pc;
            pc.sampleRate = sr;
            pc.bpm = 90.0 + 80.0 * rng.uniform();
            const float f0 = (k % 2) ? 440.0f : 110.0f;
            const float dc = (k % 3 == 0) ? 0.2f : 0.0f;
            const float amp = dc > 0 ? 0.3f : 0.5f;
            std::vector<float> outL(static_cast<size_t>(N)), outR(static_cast<size_t>(N));
            std::vector<float> L(block), R(block);
            int nextChange = static_cast<int>((0.35 + 0.55 * rng.uniform()) * sr);
            const int resetAt = (N / 2 / block) * block;  // on a block boundary
            for (int pos = 0; pos < N; pos += block) {
                const int n = std::min(block, N - pos);
                for (int i = 0; i < n; ++i) {
                    const double ph = 2.0 * 3.14159265358979 * f0 * (pos + i) / sr;
                    // the engine fades the input of a fresh or reset processor in over 12 ms (Engine kFreshInMs):
                    // the audit does the same, so the input's own start is never counted as the effect's click
                    const int since = (pos + i) >= resetAt ? (pos + i) - resetAt : (pos + i);
                    const float g = since < 576 ? 0.5f - 0.5f * std::cos(3.14159265f * since / 576.0f) : 1.0f;
                    L[static_cast<size_t>(i)] = g * (dc + amp * static_cast<float>(std::sin(ph)));
                    R[static_cast<size_t>(i)] = g * (dc + amp * static_cast<float>(std::sin(ph + 0.9)));
                }
                if (pos >= nextChange && !std::getenv("IH1_CLICK_STATIC")) {
                    if (std::getenv("IH1_CLICK_VERBOSE")) std::printf("   [case %d] params change at %.3f s\n", k, pos / sr);
                    randomise(v, 0.5f);
                    fx->setParams(d.clamp(v), false);
                    nextChange = pos + static_cast<int>((0.35 + 0.55 * rng.uniform()) * sr);
                }
                if (pos == resetAt) fx->reset();
                float* ch[2] = {L.data(), R.data()};
                pc.ppqPosition = pos / sr * pc.bpm / 60.0;
                pc.ppqValid = true;
                pc.hostPlaying = true;
                fx->process(ch, n, pc);
                for (int i = 0; i < n; ++i) { outL[static_cast<size_t>(pos + i)] = L[static_cast<size_t>(i)]; outR[static_cast<size_t>(pos + i)] = R[static_cast<size_t>(i)]; }
            }
            if (const char* dump = std::getenv("IH1_CLICK_DUMP"); dump && std::atoi(dump) == k) {
                if (FILE* f = std::fopen("click_dump.f32", "wb")) { std::fwrite(outL.data(), 4, outL.size(), f); std::fclose(f); }
            }
            // high-passed window energies (both channels)
            Hp4 hp;
            hp.design(6000.0f, static_cast<float>(sr));
            const int W = N / win;
            std::vector<float> e(static_cast<size_t>(W), 0.0f), full(static_cast<size_t>(W), 0.0f);
            for (int i = 0; i < W * win; ++i) {
                float a = outL[static_cast<size_t>(i)], b = outR[static_cast<size_t>(i)];
                if (!std::isfinite(a) || !std::isfinite(b)) { a = b = 10.0f; }
                full[static_cast<size_t>(i / win)] += (a * a + b * b) / (2.0f * win);
                const float ya = hp.run(1, 0, hp.run(0, 0, a));
                const float yb = hp.run(1, 1, hp.run(0, 1, b));
                e[static_cast<size_t>(i / win)] += (ya * ya + yb * yb) / (2.0f * win);
            }
            int caseClicks = 0, caseTexture = 0;
            float caseWorst = -99.0f;
            std::vector<float> nb;
            std::vector<float> over(static_cast<size_t>(W), -99.0f);
            // skip the first 60 ms (the high-pass and the effect's own start-up) and the windows right after the
            // deliberate reset (the reset itself is a state wipe; the engine fades a restarted processor's input)
            const int resetW = resetAt / win;
            // V5: a window is judged only where its isolation test (20 windows after) and its local median fit inside the
            // render: the last ~25 ms cannot be told apart from a level change (a gate re-opening at the very end)
            for (int w = ctx; w < W - 25; ++w) {
                if (w >= resetW - 2 && w <= resetW + 30) continue;
                nb.clear();
                for (int j = std::max(0, w - ctx); j < std::min(W, w + ctx); ++j) if (std::abs(j - w) > 2) nb.push_back(e[static_cast<size_t>(j)]);
                std::nth_element(nb.begin(), nb.begin() + static_cast<long>(nb.size() / 2), nb.end());
                const float med = std::max(nb[nb.size() / 2], 1e-12f);
                const float x = e[static_cast<size_t>(w)];
                if (x < floorE) continue;
                // second criterion (calibrated, docs/V4_IMPLEMENTATION.md): the burst must also be loud relative to the
                // local signal, i.e. sharper than a ~0.4 ms raised-cosine fade (-50 dB HF re. signal power). A hard
                // cut measures -20..-30 dB, a 0.5 ms fade -57..-63 dB, a 1 ms fade -71 dB (110 / 440 Hz sines).
                float ref = 1e-12f;
                for (int j = std::max(0, w - 20); j < std::min(W, w + 21); ++j) ref = std::max(ref, full[static_cast<size_t>(j)]);
                if (10.0f * std::log10(x / ref) < -50.0f) continue;
                over[static_cast<size_t>(w)] = 10.0f * std::log10(x / med);
            }
            // a click is SHORT: a run of at most 4 consecutive elevated 1 ms windows. Longer elevated runs are a
            // sustained change of texture (a lo-fi step, a scrub, a pitched fragment) and are counted separately.
            for (int w = 0; w < W;) {
                if (over[static_cast<size_t>(w)] <= ratioDb) { ++w; continue; }
                int r = w;
                float pk = -99.0f;
                while (r < W && over[static_cast<size_t>(r)] > ratioDb) { pk = std::max(pk, over[static_cast<size_t>(r)]); ++r; }
                // isolated? a click falls back right after; the onset of a sustained texture (a lo-fi step) stays up
                std::vector<float> post;
                for (int j = r + 2; j < std::min(W, r + 20); ++j) post.push_back(e[static_cast<size_t>(j)]);
                float postMed = 1e-12f;
                if (!post.empty()) { std::nth_element(post.begin(), post.begin() + static_cast<long>(post.size() / 2), post.end()); postMed = std::max(post[post.size() / 2], 1e-12f); }
                float peakE = 0.0f;
                for (int j = w; j < r; ++j) peakE = std::max(peakE, e[static_cast<size_t>(j)]);
                const bool sustained = 10.0f * std::log10(peakE / postMed) < 15.0f;
                if (r - w <= 4 && !sustained) {
                    ++caseClicks;
                    caseWorst = std::max(caseWorst, pk);
                    if (std::getenv("IH1_CLICK_VERBOSE")) std::printf("   [case %d] click at %.3f s: %.1f dB over median (%d ms)\n", k, w * win / sr, pk, r - w);
                } else {
                    ++caseTexture;
                }
                w = r;
            }
            worst = std::max(worst, caseWorst);
            if (caseClicks > 0) {
                ++bad;
                if (firstBad.empty()) {
                    char buf[64];
                    std::snprintf(buf, sizeof buf, "case %d (%d windows, %.1f dB)", k, caseClicks, caseWorst);
                    firstBad = buf;
                }
            }
        }
        totalBad += bad;
        std::printf("%-18s %6d %6d %8.1f  %s\n", type.c_str(), cases, bad, worst, firstBad.c_str());
        std::fflush(stdout);
    }
    return totalBad > 0 ? 1 : 0;
}
