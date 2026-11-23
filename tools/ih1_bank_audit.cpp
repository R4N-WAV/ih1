// IH1 factory bank audit (V6 Phase 3, prompt 5.1). Renders every factory preset on the first source its tags name and
// on a vocal phrase, measures it through the real Engine + Session (Output Safety off), compares the rendered sounds
// and applies stated rules to classify each preset Keep / Improve / Merge / Remove. Prints a Markdown table.
//
// Measurements (on the preset's primary source, after the first second):
//   gain       output RMS vs input RMS (dB)          peak       sample peak (linear, safety off)
//   class      audition transformation class (input transformed: Subtle / Moderate / Transformative)
//   added      residual after the best-aligned input, 10 log10(1 - r^2): how much NEW content the preset adds
//   macros     how many macro knobs the recalled chain shows (authored or combination)
// Likeness of two presets compares what each one CHANGES: its output minus the latency-aligned input, per channel (a
// stereo-only effect such as an auto-pan is not cancelled by a mono sum). The second preset's change is aligned to the
// first's by the cross-correlation peak (lags up to +-170 ms) and its residual after that best-aligned copy is
// 10 log10(1 - r^2). Below -12 dB, one preset's change is (to within 6 % of its energy) a delayed, scaled copy of the
// other's. Two first attempts were rejected: a long-term spectral fingerprint (every preset that keeps the dry voice
// shares its spectrum: an echo throw "matched" a polished lead within 0.6 dB) and the residual of whole outputs (two
// subtle presets both stay near the dry input, so they "matched" each other).
//
// Rules (in order):
//   Remove   not finite, or silent (gain < -30 dB), or no audible effect at all (Subtle and added < -30 dB)
//   Improve  level outside -15 .. +8 dB, or peak >= 2.0 (+6 dBFS), or labelled Strong / Extreme while measuring Subtle
//            with added < -12 dB (the label promises more than the sound delivers)
//   Merge    changes the sound alike (aligned residual < -12 dB on the vocal phrase AND on the shared primary source,
//            both channels) as an earlier preset of the same category and the same intended source
//   Keep     otherwise
// Rules are a screen, not a listening test: every Improve / Merge / Remove verdict was opened and is explained in
// docs/V6_PRESET_AUDIT.md.
//
// Usage: ih1_bank_audit [--md out.md] [filter-substring]
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ih1/dsp/Fft.h"
#include "ih1/engine/Engine.h"
#include "ih1/model/Session.h"
#include "ih1/presets/Audition.h"
#include "ih1/presets/Presets.h"

using namespace ih1;

namespace {
// likeness is computed at 12 kHz (4-sample averages): enough to tell echoes, doubles and filters apart, 16x less memory
constexpr int kDec = 4;
constexpr int kFft = 1 << 16;   // 3 s at 12 kHz + the lag range: no circular wrap
constexpr int kMaxLag = 2048;   // +-170 ms

// the render of the vocal phrase (mono, after the first second), zero-padded spectrum for correlation
struct Spec { std::vector<dsp::cfloat> X; double energy = 0.0; };
// the change made by the preset on one channel: output minus the input delayed by the preset's latency
std::vector<float> change(const std::vector<float>& out, const std::vector<float>& in, int latency) {
    std::vector<float> d(out.size(), 0.0f);
    for (size_t i = 0; i < out.size(); ++i) {
        const long j = static_cast<long>(i) - latency;
        d[i] = out[i] - (j >= 0 && j < static_cast<long>(in.size()) ? in[static_cast<size_t>(j)] : 0.0f);
    }
    return d;
}
Spec spectrum(const std::vector<float>& L, const std::vector<float>& R, const dsp::Fft& fft) {
    Spec sp;
    sp.X.assign(static_cast<size_t>(kFft), dsp::cfloat());
    const size_t start = 48000;
    for (size_t j = 0; start + (j + 1) * kDec <= L.size() && j < static_cast<size_t>(kFft - kMaxLag); ++j) {
        float m = 0.0f;
        for (int q = 0; q < kDec; ++q) m += 0.5f * (L[start + j * kDec + static_cast<size_t>(q)] + R[start + j * kDec + static_cast<size_t>(q)]);
        m /= kDec;
        sp.X[j] = dsp::cfloat(m, 0.0f);
        sp.energy += static_cast<double>(m) * m;
    }
    fft.forward(sp.X.data());
    return sp;
}
// residual (dB) of b after its best-aligned, best-scaled copy of a
double likeness(const Spec& a, const Spec& b, const dsp::Fft& fft, std::vector<dsp::cfloat>& work) {
    if (a.energy < 1e-9 || b.energy < 1e-9) return 0.0;
    for (int k = 0; k < kFft; ++k) work[static_cast<size_t>(k)] = a.X[static_cast<size_t>(k)] * std::conj(b.X[static_cast<size_t>(k)]);
    fft.inverse(work.data());
    double best = 0.0;
    for (int lag = -kMaxLag; lag <= kMaxLag; ++lag) best = std::max(best, std::fabs(static_cast<double>(work[static_cast<size_t>((lag + kFft) % kFft)].real())));
    const double r = best / kFft / std::sqrt(a.energy * b.energy);
    return 10.0 * std::log10(std::max(1e-6, 1.0 - std::min(1.0, r * r)));
}

struct Row {
    const FactoryPreset* p = nullptr;
    int src = 0;
    audition::Result r;
    double added = 0.0;
    int macros = 0;
    Spec voice, primaryL, primaryR;
    std::string verdict, reason;
    std::string closest;
    double closestDb = 0.0;
};
}  // namespace

int main(int argc, char** argv) {
    std::string md, filter;
    for (int a = 1; a < argc; ++a) {
        if (std::strcmp(argv[a], "--md") == 0 && a + 1 < argc) md = argv[++a];
        else filter = argv[a];
    }
    std::vector<Row> rows;
    dsp::Fft fft;
    fft.prepare(kFft);
    std::vector<dsp::cfloat> work(static_cast<size_t>(kFft));
    for (const auto& p : factoryPresets()) {
        if (p.id == kInitPresetId || p.chain.empty()) continue;
        if (!filter.empty() && p.id.find(filter) == std::string::npos) continue;
        Row row;
        row.p = &p;
        row.src = audition::sourcesForTags(p.tags).front();
        const std::string js = presetToJson(p);
        row.r = audition::run(js, row.src, 4.0);
        row.added = 10.0 * std::log10(std::max(1e-6, 1.0 - row.r.correlation * row.r.correlation));
        const auto v = audition::run(js, 0, 4.0);  // the vocal phrase: one common source for every preset
        {
            const auto x0 = audition::source(0, 4.0), xs = audition::source(row.src, 4.0);
            const auto vl = change(v.outL, x0, v.latency), vr = change(v.outR, x0, v.latency);
            row.voice = spectrum(vl, vr, fft);
            const auto pl = change(row.r.outL, xs, row.r.latency), pr = change(row.r.outR, xs, row.r.latency);
            row.primaryL = spectrum(pl, pl, fft);
            row.primaryR = spectrum(pr, pr, fft);
        }
        {
            Engine e;
            Session s(e);
            s.prepare(48000.0, 256);
            s.fromJson(js, false);
            row.macros = static_cast<int>(s.macros().size());
        }
        rows.push_back(std::move(row));
        std::fprintf(stderr, "\r%zu presets", rows.size());
    }
    std::fprintf(stderr, "\n");
    int counts[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < rows.size(); ++i) {
        auto& w = rows[i];
        const auto& r = w.r;
        const bool strong = w.p->intensity == "Strong" || w.p->intensity == "Extreme";
        char buf[256];
        if (!r.finite || r.gainDb < -30.0 || (r.cls == audition::Class::Subtle && w.added < -30.0)) {
            w.verdict = "Remove";
            std::snprintf(buf, sizeof buf, "%s", !r.finite ? "non-finite output" : (r.gainDb < -30.0 ? "near silent" : "no audible effect"));
            w.reason = buf;
        } else if (r.gainDb < -15.0 || r.gainDb > 8.0 || r.peak >= 2.0 || (strong && r.cls == audition::Class::Subtle && w.added < -12.0)) {
            w.verdict = "Improve";
            if (r.gainDb < -15.0 || r.gainDb > 8.0) std::snprintf(buf, sizeof buf, "level %+.1f dB", r.gainDb);
            else if (r.peak >= 2.0) std::snprintf(buf, sizeof buf, "peak %.2f", r.peak);
            else std::snprintf(buf, sizeof buf, "labelled %s, adds only %.0f dB", w.p->intensity.c_str(), w.added);
            w.reason = buf;
        } else {
            for (size_t j = 0; j < i; ++j) {
                const auto& o = rows[j];
                if (o.p->category != w.p->category || o.verdict == "Remove") continue;
                // alike on the vocal phrase AND on the primary source the two presets share (a drum preset is judged
                // on drums: on a voice, hat and kick processors barely act, so their vocal renders look alike)
                double like = likeness(o.voice, w.voice, fft, work);
                if (o.src == w.src && w.src != 0)
                    like = std::max({like, likeness(o.primaryL, w.primaryL, fft, work), likeness(o.primaryR, w.primaryR, fft, work)});
                else if (o.src != w.src) like = 0.0;  // different intended sources: not interchangeable presets
                if (like < w.closestDb) { w.closestDb = like; w.closest = o.p->id; }
                if (like < -12.0) {
                    w.verdict = "Merge";
                    std::snprintf(buf, sizeof buf, "sounds like %s (residual %.1f dB)", o.p->id.c_str(), like);
                    w.reason = buf;
                    break;
                }
            }
            if (w.verdict.empty()) { w.verdict = "Keep"; w.reason = ""; }
        }
        counts[w.verdict == "Keep" ? 0 : w.verdict == "Improve" ? 1 : w.verdict == "Merge" ? 2 : 3]++;
    }
    FILE* out = md.empty() ? stdout : std::fopen(md.c_str(), "w");
    if (!out) return 2;
    std::fprintf(out, "| Preset | Name | Category | Intensity | Source | Gain dB | Peak | Class | Added dB | Macros | Closest earlier (residual dB) | Verdict | Reason |\n");
    std::fprintf(out, "|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    for (const auto& w : rows)
    {
        char near[96] = "";
        if (!w.closest.empty()) std::snprintf(near, sizeof near, "`%s` (%.1f)", w.closest.c_str(), w.closestDb);
        std::fprintf(out, "| `%s` | %s | %s | %s | %s | %+.1f | %.2f | %s | %.0f | %d | %s | %s | %s |\n", w.p->id.c_str(), w.p->name.c_str(), w.p->category.c_str(),
                     w.p->intensity.c_str(), audition::sourceName(w.src), w.r.gainDb, w.r.peak, audition::className(w.r.cls), w.added, w.macros,
                     near, w.verdict.c_str(), w.reason.c_str());
    }
    std::fprintf(out, "\n%zu presets: %d Keep, %d Improve, %d Merge, %d Remove\n", rows.size(), counts[0], counts[1], counts[2], counts[3]);
    if (out != stdout) std::fclose(out);
    std::printf("%zu presets: %d Keep, %d Improve, %d Merge, %d Remove\n", rows.size(), counts[0], counts[1], counts[2], counts[3]);
    return 0;
}
