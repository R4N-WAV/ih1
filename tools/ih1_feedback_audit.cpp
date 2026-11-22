// IH1 feedback-runaway audit (V5 Phase 5).
//
// Renders the real Engine + Session with the Output Safety stage OFF, so a loop that grows is seen as it is and
// is not hidden by the limiter. Two passes:
//
//   presets : every factory preset, macros at 0 and at 100 %, and a macro sweep 0 -> 100 % during the excitation.
//   fx      : every production FX type committed alone with its recursive controls (feedback / regeneration /
//             decay / resonance / density ...) at the legal maximum, plus seeded random drafts with those
//             controls at 85-100 %.
//
// Each case is excited by four sources in turn: an impulse, a 0.4 s noise burst, a 2 s chord and a 2 s drum loop,
// each followed by `tail` seconds of silence (default 20). Per 100 ms window the output RMS and peak are recorded.
//
//   ref    loudest window while excited or in the first second after it stops
//   growth loudest window from +1 s to the end, relative to ref
//   slope  mean level of the last 2 s against +1..+3 s, in dB per second
//
// Verdict
//   FAULT  non-finite output or an engine guard reset (Telemetry::faults): the loop blew up internally
//   GROW   the last 3 s are > 3 dB above ref (and above -60 dBFS), or a > 6 dB rise still rising at the end, or any
//          pre-safety peak above +12 dBFS: runaway
//   HOLD   still above -50 dBFS at the end and decaying slower than 0.4 dB/s (T60 > 150 s): the tail does not decay (fine only for
//          a deliberate freeze / loop / infinite mode, which the report marks; otherwise a failure)
//   LONG   decaying, but slower than 1.5 dB/s (a long, deliberate tail; reported, not a failure)
//   ok     decaying (or silent)
//
// Usage: ih1_feedback_audit [presets|fx|all] [tailSeconds=20] [filter-substring] [--md out.md]
// Exit code = number of GROW + FAULT cases (+ unexplained HOLD cases).
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
#include "ih1/presets/Presets.h"

using namespace ih1;

namespace {
constexpr double kSr = 48000.0;
constexpr int kBlock = 256;
constexpr int kWin = 4800;  // 100 ms

enum class Src { Impulse, Noise, Chord, Drums };
const char* srcName(Src s) {
    switch (s) {
        case Src::Impulse: return "impulse";
        case Src::Noise: return "noise";
        case Src::Chord: return "chord";
        default: return "drums";
    }
}
int srcLen(Src s) {
    switch (s) {
        case Src::Impulse: return 1;
        case Src::Noise: return static_cast<int>(0.4 * kSr);
        default: return static_cast<int>(2.0 * kSr);
    }
}
uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}
float white(uint32_t n) { return static_cast<float>(hash32(n) & 0xffffff) / 8388608.0f - 1.0f; }
float sample(Src s, long n, int c) {
    const double t = n / kSr;
    switch (s) {
        case Src::Impulse: return n == 0 ? 0.9f : 0.0f;
        case Src::Noise: return 0.35f * white(static_cast<uint32_t>(n * 2 + c));
        case Src::Chord: {
            const double ph = 0.3 * c;
            return 0.18f * static_cast<float>(std::sin(2 * dsp::kPi * 110.0 * t + ph) + std::sin(2 * dsp::kPi * 164.8 * t + ph) +
                                              0.7 * std::sin(2 * dsp::kPi * 220.0 * t) + 0.5 * std::sin(2 * dsp::kPi * 1318.5 * t + ph));
        }
        case Src::Drums: {
            const double beat = 60.0 / 120.0;
            const double pos = std::fmod(t, beat * 4.0);
            const double kick = std::fmod(pos, beat * 2.0), snare = std::fmod(pos + beat, beat * 2.0), hat = std::fmod(pos, beat * 0.5);
            float v = 0.8f * static_cast<float>(std::exp(-kick * 22.0) * std::sin(2 * dsp::kPi * (48.0 + 90.0 * std::exp(-kick * 40.0)) * kick));
            v += 0.45f * static_cast<float>(std::exp(-snare * 28.0)) * white(static_cast<uint32_t>(n * 3 + c)) +
                 0.3f * static_cast<float>(std::exp(-snare * 18.0) * std::sin(2 * dsp::kPi * 190.0 * snare));
            v += 0.12f * static_cast<float>(std::exp(-hat * 90.0)) * white(static_cast<uint32_t>(n * 7 + 11 + c));
            return v;
        }
    }
    return 0.0f;
}

struct CaseResult {
    double e0 = 0, late = 0, growthDb = -200, peak = 0;
    uint32_t faults = 0;
    bool finite = true;
    std::string verdict = "ok";
    std::string worstSrc;
    double slope = -99.0;
};

double db(double x) { return 20.0 * std::log10(std::max(x, 1e-12)); }

struct Rig {
    Engine engine;
    Session session{engine};
    long n = 0;
    Rig() {
        engine.setSafety(0, 0.0f);
        session.prepare(kSr, kBlock);
    }
};

// Runs one source + tail through an already configured rig. sweep: macros ramp 0 -> 1 during the excitation.
void excite(Rig& r, Src s, double tailSec, bool sweep, CaseResult& res) {
    const int len = srcLen(s);
    const int tail = static_cast<int>(tailSec * kSr);
    const int total = len + tail;
    std::vector<float> L(kBlock), R(kBlock);
    ProcessContext ctx;
    ctx.sampleRate = kSr;
    ctx.bpm = 120.0;
    ctx.ppqValid = true;
    ctx.hostPlaying = true;
    std::vector<double> winRms, winPeak;
    double acc = 0;
    float pk = 0;
    int inWin = 0;
    const uint32_t faults0 = r.engine.telemetry().faults.load();
    for (long i = 0; i < total; i += kBlock) {
        r.session.service();
        if (sweep) {
            const float v = std::clamp(static_cast<float>(i) / std::max(1, len), 0.0f, 1.0f);
            for (int m = 0; m < kNumPresetMacros; ++m) r.engine.setMacro(m, v);
        }
        for (int k = 0; k < kBlock; ++k) {
            const long idx = i + k;
            L[static_cast<size_t>(k)] = idx < len ? sample(s, idx, 0) : 0.0f;
            R[static_cast<size_t>(k)] = idx < len ? sample(s, idx, 1) : 0.0f;
        }
        ctx.ppqPosition = (r.n + i) / kSr * 2.0;
        r.engine.process(L.data(), R.data(), kBlock, ctx);
        const float pre = r.engine.telemetry().preSafetyPeak.load();
        res.peak = std::max(res.peak, static_cast<double>(pre));
        for (int k = 0; k < kBlock; ++k) {
            const float a = L[static_cast<size_t>(k)], b = R[static_cast<size_t>(k)];
            if (!std::isfinite(a) || !std::isfinite(b)) res.finite = false;
            acc += 0.5 * (static_cast<double>(a) * a + static_cast<double>(b) * b);
            pk = std::max(pk, std::max(std::fabs(a), std::fabs(b)));
            if (++inWin == kWin) {
                winRms.push_back(std::sqrt(acc / kWin));
                winPeak.push_back(pk);
                acc = 0;
                pk = 0;
                inWin = 0;
            }
        }
    }
    r.n += total;
    res.faults += r.engine.telemetry().faults.load() - faults0;
    // windows after the excitation stops. ref = loudest window while excited or in the first second after (an
    // effect that delays its output by > 1 s is still compared with what it played, not with silence).
    const int w0 = (len + kWin - 1) / kWin;
    const int nW = static_cast<int>(winRms.size());
    double ref = 0, e0 = 0, after = 0, late = 0, early = 0, lateMean = 0;
    for (int w = 0; w < std::min(nW, w0 + 10); ++w) ref = std::max(ref, winRms[static_cast<size_t>(w)]);
    for (int w = w0; w < std::min(nW, w0 + 10); ++w) e0 = std::max(e0, winRms[static_cast<size_t>(w)]);
    for (int w = w0 + 10; w < nW; ++w) after = std::max(after, winRms[static_cast<size_t>(w)]);
    for (int w = std::max(w0 + 10, nW - 30); w < nW; ++w) late = std::max(late, winRms[static_cast<size_t>(w)]);
    // decay slope: mean power over +1..+3 s versus the last 2 s (dB per second)
    int ne = 0, nl = 0;
    for (int w = w0 + 10; w < std::min(nW, w0 + 30); ++w, ++ne) early += winRms[static_cast<size_t>(w)] * winRms[static_cast<size_t>(w)];
    for (int w = std::max(w0 + 30, nW - 20); w < nW; ++w, ++nl) lateMean += winRms[static_cast<size_t>(w)] * winRms[static_cast<size_t>(w)];
    const double eDb = 10.0 * std::log10(std::max(early / std::max(1, ne), 1e-24));
    const double lDb = 10.0 * std::log10(std::max(lateMean / std::max(1, nl), 1e-24));
    const double dt = std::max(1.0, ((nW - 10) - (w0 + 20)) * kWin / kSr);
    const double slope = (lDb - eDb) / dt;
    const double growth = db(after) - db(std::max(ref, 1e-9));
    std::string v = "ok";
    if (!res.finite || res.faults > 0) v = "FAULT";
    // runaway = still louder than the excited level at the end, or a rise that is still rising, or > +12 dBFS. (A
    // multi-head echo whose repeats are louder than the dry burst, or a delayed replay, rises and then decays: not GROW.)
    else if ((db(late) > db(std::max(ref, 1e-9)) + 3.0 && db(late) > -60.0) || (growth > 6.0 && slope > 0.0 && db(late) > -60.0) || res.peak > 4.0) v = "GROW";
    else if (lDb > -50.0 && slope > -0.4) v = "HOLD";  // T60 beyond ~150 s
    else if (lDb > -50.0 && slope > -1.5) v = "LONG";
    res.slope = std::max(res.slope, (v == "HOLD" || v == "LONG") ? slope : -99.0);
    auto rank = [](const std::string& x) { return x == "FAULT" ? 4 : x == "GROW" ? 3 : x == "HOLD" ? 2 : x == "LONG" ? 1 : 0; };
    if (rank(v) > rank(res.verdict) || (rank(v) == rank(res.verdict) && growth > res.growthDb)) {
        res.verdict = v;
        res.e0 = std::max(e0, ref);
        res.late = late;
        res.growthDb = growth;
        res.worstSrc = srcName(s);
    }
}

// Parameters treated as recursive / energy-holding (by stable id fragments).
bool recursiveParam(const std::string& id) {
    static const char* keys[] = {"feedback", "fb", "regen", "decay", "reso", "recirc", "sustain", "density", "repeats", "size", "spread_fb", "loop"};
    for (const char* k : keys) if (id.find(k) != std::string::npos) return true;
    return false;
}
// Deliberate infinite / hold switches: a HOLD verdict with one of these on is intended behaviour.
bool holdParam(const std::string& id) {
    return id == "freeze" || id == "hold" || id == "infinite" || id.find("freeze") != std::string::npos;
}
// Documented self-oscillation ranges (a parameter above this value is a deliberate, bounded "singing" mode).
bool selfOscillating(const std::string& type, const std::string& id, float v) {
    return type == "tape_echo" && id == "intensity" && v > 100.0f;
}
// Effects whose purpose is to sustain captured audio (looping, freezing): HOLD is the effect, not a bug.
bool holdByDesign(const std::string& type) {
    return type == "looper" || type == "spectral" || type == "slice_shuffle" || type == "stutter";
}

FILE* gMd = nullptr;
int gFail = 0, gHoldUnexplained = 0, gCases = 0, gLong = 0;

void report(const std::string& name, const std::string& chain, const CaseResult& r, bool holdOk) {
    ++gCases;
    std::string v = r.verdict;
    if (v == "HOLD" && holdOk) v = "hold (intended)";
    if (v == "GROW" || v == "FAULT") ++gFail;
    if (v == "HOLD") ++gHoldUnexplained;
    if (v == "LONG") ++gLong;
    if ((v != "ok" && v != "LONG") || std::getenv("IH1_FB_VERBOSE"))
        std::printf("%-6s %-40s %-8s ref %6.1f late %6.1f growth %+6.1f dB slope %+5.2f dB/s peak %+5.1f dBFS faults %u  [%s]\n", v.c_str(), name.c_str(),
                    r.worstSrc.c_str(), db(r.e0), db(r.late), r.growthDb, r.slope, db(r.peak), r.faults, chain.c_str());
    if (gMd)
        std::fprintf(gMd, "| %s | %s | %s | %.1f | %.1f | %+.1f | %+.2f | %+.1f | %u | %s |\n", name.c_str(), chain.c_str(), r.worstSrc.c_str(), db(r.e0), db(r.late),
                     r.growthDb, r.slope, db(r.peak), r.faults, v.c_str());
}

void auditPresets(double tail, const std::string& filter) {
    if (gMd) std::fprintf(gMd, "\n## Factory presets (Output Safety off)\n\n| Preset / pass | Chain | Worst source | ref dBFS | late dBFS | growth | slope dB/s | peak | faults | verdict |\n|---|---|---|---|---|---|---|---|---|---|\n");
    for (const auto& p : factoryPresets()) {
        if (p.id == kInitPresetId) continue;
        if (!filter.empty() && p.id.find(filter) == std::string::npos) continue;
        std::string chain;
        bool holdOk = false;
        for (const auto& f : p.chain) {
            chain += (chain.empty() ? "" : " > ") + f.type;
            if (holdByDesign(f.type)) holdOk = true;
            for (const auto& [k, v] : f.params) if ((holdParam(k) && v >= 0.5f) || selfOscillating(f.type, k, v)) holdOk = true;
        }
        for (int pass = 0; pass < 3; ++pass) {
            if (pass > 0 && p.macros.empty()) break;
            Rig r;
            std::string err;
            if (!r.session.fromJson(presetToJson(p), false, &err)) {
                std::printf("LOADFAIL %s: %s\n", p.id.c_str(), err.c_str());
                ++gFail;
                break;
            }
            for (int m = 0; m < kNumPresetMacros; ++m) r.engine.setMacro(m, pass == 1 ? 1.0f : 0.0f);
            CaseResult res;
            for (Src s : {Src::Impulse, Src::Noise, Src::Chord, Src::Drums}) excite(r, s, tail, pass == 2, res);
            report(p.id + (pass == 0 ? "" : pass == 1 ? " @macros100" : " @sweep"), chain, res, holdOk);
        }
        std::fflush(stdout);
    }
}

void auditFx(double tail, const std::string& filter, int randomDrafts) {
    if (gMd) std::fprintf(gMd, "\n## FX types, recursive controls at the legal maximum (Output Safety off)\n\n| FX / draft | Raised controls | Worst source | ref dBFS | late dBFS | growth | slope dB/s | peak | faults | verdict |\n|---|---|---|---|---|---|---|---|---|---|\n");
    const auto& reg = fxRegistry();
    for (size_t t = 0; t < reg.size(); ++t) {
        const auto& d = reg[t];
        if (d.experimental) continue;
        if (!filter.empty() && d.typeId.find(filter) == std::string::npos) continue;
        std::vector<int> rec;
        for (size_t k = 0; k < d.params.size(); ++k)
            if (d.params[k].kind == ParamKind::Float && recursiveParam(d.params[k].id)) rec.push_back(static_cast<int>(k));
        std::string raised;
        for (int k : rec) raised += (raised.empty() ? "" : ",") + d.params[static_cast<size_t>(k)].id;
        if (raised.empty()) raised = "(none)";
        for (int dr = 0; dr <= randomDrafts; ++dr) {
            Rig r;
            dsp::Rng rng;
            rng.seed_(0xF00Du + static_cast<uint32_t>(t * 131 + dr));
            r.session.select(static_cast<int>(t));
            bool holdOk = holdByDesign(d.typeId);
            for (size_t k = 0; k < d.params.size(); ++k) {
                const auto& ps = d.params[k];
                float v = ps.defaultValue;
                const bool isRec = std::find(rec.begin(), rec.end(), static_cast<int>(k)) != rec.end();
                if (dr > 0) {
                    const float u = rng.uniform();
                    if (ps.kind == ParamKind::Float) v = ps.minValue + (ps.maxValue - ps.minValue) * u;
                    else v = std::round(ps.minValue + (ps.maxValue - ps.minValue) * u);
                    if (ps.id == "mix") v = std::max(v, ps.minValue + 0.5f * (ps.maxValue - ps.minValue));
                }
                if (isRec) v = dr == 0 ? ps.maxValue : ps.minValue + (ps.maxValue - ps.minValue) * (0.85f + 0.15f * rng.uniform());
                if (holdParam(ps.id) && ps.kind != ParamKind::Float && dr % 3 != 2) v = ps.minValue;  // freeze mostly off
                // IH1_FB_SET="id=value,id=value": exact draft for the "max" case (reproduce a reported failure)
                if (dr == 0) if (const char* set = std::getenv("IH1_FB_SET")) {
                    std::string ss = set;
                    size_t pos = 0;
                    while (pos < ss.size()) {
                        const size_t e = ss.find(',', pos);
                        const std::string kv = ss.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
                        const size_t eq = kv.find('=');
                        if (eq != std::string::npos && kv.substr(0, eq) == ps.id) v = static_cast<float>(std::atof(kv.c_str() + eq + 1));
                        if (e == std::string::npos) break;
                        pos = e + 1;
                    }
                }
                if ((holdParam(ps.id) && v >= 0.5f) || selfOscillating(d.typeId, ps.id, v)) holdOk = true;
                r.session.setDraftParam(static_cast<int>(k), v);
            }
            std::string draft;
            {
                const auto& v = r.session.draft(static_cast<int>(t));
                char b[64];
                for (size_t k = 0; k < d.params.size(); ++k) {
                    std::snprintf(b, sizeof b, "%s%s=%.3g", draft.empty() ? "" : " ", d.params[k].id.c_str(), v[k]);
                    draft += b;
                }
            }
            r.session.commit();
            r.session.select(Session::kNone);
            CaseResult res;
            for (Src s : {Src::Impulse, Src::Noise, Src::Chord, Src::Drums}) excite(r, s, tail, false, res);
            report(d.typeId + (dr == 0 ? " max" : " rnd" + std::to_string(dr)), raised, res, holdOk);
            if (res.verdict == "GROW" || res.verdict == "FAULT" || (res.verdict == "HOLD" && !holdOk)) std::printf("       draft: %s\n", draft.c_str());
        }
        std::fflush(stdout);
    }
}
}  // namespace

int main(int argc, char** argv) {
    std::string mode = argc > 1 ? argv[1] : "all";
    const double tail = argc > 2 ? std::atof(argv[2]) : 20.0;
    std::string filter = argc > 3 && std::strncmp(argv[3], "--", 2) != 0 ? argv[3] : "";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--md") == 0) gMd = std::fopen(argv[i + 1], "w");
    const int rnd = std::getenv("IH1_FB_RANDOM") ? std::atoi(std::getenv("IH1_FB_RANDOM")) : 6;
    if (gMd) std::fprintf(gMd, "# IH1 feedback audit\n\nTail %.0f s after each source; growth = loudest 100 ms window from +1 s to the end vs the first second after the excitation.\n", tail);
    if (mode == "presets" || mode == "all") auditPresets(tail, filter);
    if (mode == "fx" || mode == "all") auditFx(tail, filter, rnd);
    std::printf("\n%d cases, %d GROW/FAULT, %d unexplained HOLD, %d long (slow but decaying) tails\n", gCases, gFail, gHoldUnexplained, gLong);
    if (gMd) {
        std::fprintf(gMd, "\n**%d cases: %d GROW / FAULT, %d unexplained HOLD.**\n", gCases, gFail, gHoldUnexplained);
        std::fclose(gMd);
    }
    return gFail + gHoldUnexplained;
}
