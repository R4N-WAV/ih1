// IH1 V5 (feedback round 5) behaviour tests: commit keeps the browser position (Phase 7), feedback stability
// (Phase 5), the new processors (Phases 2 / 3), combination macros (Phase 8) and the bank gate (Phase 10).
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <set>
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

float sine(long n, int c) { return 0.4f * static_cast<float>(std::sin(2 * dsp::kPi * 330.0 * n / kSr + 0.3 * c)); }
float silence(long, int) { return 0.0f; }

struct Rig {
    Engine engine;
    Session session{engine};
    long n = 0;
    Rig() {
        engine.setSafety(0, 0.0f);
        session.prepare(kSr, kBlock);
    }
    // returns the largest sample-to-sample step of the left output
    float run(int blocks, std::vector<float>* outL = nullptr, float (*src)(long, int) = sine) {
        std::vector<float> L(kBlock), R(kBlock);
        ProcessContext ctx;
        ctx.sampleRate = kSr;
        float jump = 0.0f;
        for (int b = 0; b < blocks; ++b) {
            session.service();
            for (int i = 0; i < kBlock; ++i) { L[static_cast<size_t>(i)] = src(n + i, 0); R[static_cast<size_t>(i)] = src(n + i, 1); }
            engine.process(L.data(), R.data(), kBlock, ctx);
            for (int i = 0; i < kBlock; ++i) {
                jump = std::max(jump, std::fabs(L[static_cast<size_t>(i)] - last_));
                last_ = L[static_cast<size_t>(i)];
            }
            n += kBlock;
            if (outL) outL->insert(outL->end(), L.begin(), L.end());
        }
        return jump;
    }
    float last_ = 0.0f;
    int type(const char* id) { const int t = fxTypeIndex(id); REQUIRE(t >= 0); return t; }
    void stage(const char* id, std::vector<std::pair<const char*, float>> params) {
        session.select(type(id));
        session.auditionEdit();
        const auto& d = fxRegistry()[static_cast<size_t>(type(id))];
        for (auto& [k, v] : params) session.setDraftParam(d.paramIndex(k), v);
    }
};

double rmsOf(const std::vector<float>& x, size_t from) {
    double a = 0;
    for (size_t i = from; i < x.size(); ++i) a += static_cast<double>(x[i]) * x[i];
    return std::sqrt(a / std::max<size_t>(1, x.size() - from));
}
}  // namespace

// ============================================================================================ Phase 7: commit
TEST_CASE("V5 commit: browser stays on the effect, committed snapshot exact, staged copy reset + muted, no double processing") {
    Rig r;
    r.stage("test_gain", {{"gain_db", -9.0f}});
    r.run(40);
    const float steady = r.run(20);
    REQUIRE(r.session.commit());
    // the hand-over is seamless: no step larger than the sine's own slope
    const float atCommit = r.run(20);
    CHECK(atCommit < steady * 1.05f + 1e-4f);
    CHECK(r.session.selected() == r.type("test_gain"));
    CHECK(r.session.standby());
    CHECK(r.session.chain().back().values()[0] == doctest::Approx(-9.0f));
    CHECK(r.session.draft(r.type("test_gain"))[0] == doctest::Approx(0.0f));
    std::vector<float> out;
    r.run(60, &out);
    CHECK(20.0 * std::log10(rmsOf(out, 0) / (0.4 / std::sqrt(2.0))) == doctest::Approx(-9.0).epsilon(0.02));
}

TEST_CASE("V5 commit of an effect with latency: no added host latency, seamless, the next audition restores the path") {
    Rig r;
    r.stage("test_latency", {});
    r.run(40);
    const int before = r.session.latencySamples();
    REQUIRE(before > 0);
    const float steady = r.run(20);
    REQUIRE(r.session.commit());
    CHECK(r.session.latencySamples() == before);  // the standby copy is silent and adds no delay
    const float atCommit = r.run(20);
    CHECK(atCommit < steady * 1.05f + 1e-4f);
    CHECK(r.engine.activeLatency() == before);
    // auditioning the new copy brings its latency back (truthfully reported)
    CHECK(r.session.auditionEdit());
    r.run(20);
    CHECK(r.session.latencySamples() == 2 * before);
    CHECK(r.engine.activeLatency() == 2 * before);
}

TEST_CASE("V5 commit: rapid repeated commits of duplicates, every snapshot keeps its own values") {
    Rig r;
    const int t = r.type("test_gain");
    for (int k = 0; k < 6; ++k) {
        r.session.select(t);
        r.session.auditionEdit();
        r.session.setDraftParam(0, -1.0f - static_cast<float>(k));
        REQUIRE(r.session.commit());
        r.run(1);
    }
    REQUIRE(r.session.chain().size() == 6);
    for (int k = 0; k < 6; ++k) CHECK(r.session.chain()[static_cast<size_t>(k)].values()[0] == doctest::Approx(-1.0f - static_cast<float>(k)));
    std::vector<float> out;
    r.run(80, &out);
    // -1 -2 ... -6 dB = -21 dB in total, heard once (the standby copy is silent)
    CHECK(20.0 * std::log10(rmsOf(out, 4000) / (0.4 / std::sqrt(2.0))) == doctest::Approx(-21.0).epsilon(0.02));
    // a preset recall still lands on None and ends standby
    REQUIRE(r.session.fromJson(r.session.toJson(false), false));
    CHECK(r.session.selected() == Session::kNone);
    CHECK_FALSE(r.session.standby());
}

TEST_CASE("V5 commit: selecting another effect or pressing MUTE ends standby; RESET keeps it") {
    Rig r;
    r.stage("test_gain", {{"gain_db", -4.0f}});
    REQUIRE(r.session.commit());
    r.session.resetDraft();
    CHECK(r.session.standby());
    r.session.setMute(false);
    CHECK_FALSE(r.session.standby());
    CHECK_FALSE(r.session.mute());
    REQUIRE(r.session.commit());
    CHECK(r.session.standby());
    r.session.select(r.type("test_latency"));
    CHECK_FALSE(r.session.standby());
    CHECK_FALSE(r.session.mute());
}

// ============================================================================================ Phases 2 / 3: new FX
namespace {
struct FxRig {
    Engine engine;
    Session session{engine};
    long n = 0;
    bool host = false;
    FxRig() {
        engine.setSafety(0, 0.0f);
        session.prepare(kSr, kBlock);
    }
    void add(const char* type, std::vector<std::pair<const char*, float>> params) {
        const int t = fxTypeIndex(type);
        REQUIRE(t >= 0);
        session.select(t);
        session.auditionEdit();
        const auto& d = fxRegistry()[static_cast<size_t>(t)];
        for (auto& [k, v] : params) {
            REQUIRE_MESSAGE(d.paramIndex(k) >= 0, k);
            session.setDraftParam(d.paramIndex(k), v);
        }
        REQUIRE(session.commit());
        session.select(Session::kNone);
        // let the program hand-overs (staged -> committed -> nothing staged) complete before measuring
        run(0.1, [](long, int) { return 0.0f; });
        n = 0;
    }
    template <class F>
    std::vector<float> run(double seconds, F src, std::vector<float>* right = nullptr) {
        std::vector<float> out, L(kBlock), R(kBlock);
        ProcessContext ctx;
        ctx.sampleRate = kSr;
        ctx.bpm = 120.0;
        const long total = static_cast<long>(seconds * kSr);
        for (long b = 0; b < total; b += kBlock) {
            session.service();
            for (int i = 0; i < kBlock; ++i) { L[static_cast<size_t>(i)] = src(n + i, 0); R[static_cast<size_t>(i)] = src(n + i, 1); }
            ctx.ppqValid = host;
            ctx.hostPlaying = host;
            ctx.ppqPosition = n / kSr * 2.0;
            engine.process(L.data(), R.data(), kBlock, ctx);
            out.insert(out.end(), L.begin(), L.end());
            if (right) right->insert(right->end(), R.begin(), R.end());
            n += kBlock;
        }
        return out;
    }
};
double rmsRange(const std::vector<float>& x, double t0, double t1) {
    const size_t a = static_cast<size_t>(t0 * kSr), b = std::min(x.size(), static_cast<size_t>(t1 * kSr));
    double s = 0;
    for (size_t i = a; i < b; ++i) s += static_cast<double>(x[i]) * x[i];
    return std::sqrt(s / std::max<size_t>(1, b - a));
}
double dbv(double x) { return 20.0 * std::log10(std::max(x, 1e-12)); }
float hitTrain(long n, int) {  // decaying noise hits every 0.5 s
    const long p = n % 24000;
    uint32_t h = static_cast<uint32_t>(n) * 0x9E3779B1u;
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;  // white
    return p < 9600 ? 0.6f * std::exp(-p / 900.0f) * (static_cast<float>(h >> 8) / 8388608.0f - 1.0f) : 0.0f;
}
}  // namespace

TEST_CASE("V5 Cloud Chamber: lows and highs decay at their own rates; Freeze holds a bounded field") {
    auto burst = [](double hz) {
        return [hz](long n, int) { return n < 24000 ? 0.4f * static_cast<float>(std::sin(2 * dsp::kPi * hz * n / kSr)) : 0.0f; };
    };
    double tail[2];
    const double hz[2] = {110.0, 6000.0};
    for (int k = 0; k < 2; ++k) {
        FxRig r;
        r.add("cloud_chamber", {{"decay", 3.0f}, {"low_decay", 2.0f}, {"high_decay", 0.2f}, {"mix", 100.0f}, {"bloom", 0.0f}});
        const auto out = r.run(4.0, burst(hz[k]));
        tail[k] = dbv(rmsRange(out, 2.0, 2.5)) - dbv(rmsRange(out, 0.6, 0.9));
    }
    CHECK(tail[0] > tail[1] + 15.0);  // the lows ring on, the highs are long gone
    FxRig f;
    f.add("cloud_chamber", {{"decay", 2.0f}, {"freeze", 1.0f}, {"mix", 100.0f}});
    // the freeze waits for no signal: excite first with freeze off, then the frozen field must stay level
    FxRig g;
    g.add("cloud_chamber", {{"decay", 2.0f}, {"mix", 100.0f}});
    const auto pre = g.run(0.5, burst(440.0));
    (void)pre;
    const auto frozenOut = f.run(6.0, burst(440.0));
    const double a = rmsRange(frozenOut, 1.5, 2.0), b = rmsRange(frozenOut, 5.5, 6.0);
    for (float v : frozenOut) REQUIRE(std::isfinite(v));
    CHECK(std::fabs(dbv(b) - dbv(a)) < 3.0);  // held, neither growing nor dying
}

TEST_CASE("V5 Reflection Sculptor: a finite set of discrete reflections inside the span, then silence (no loop)") {
    FxRig r;
    r.add("reflections", {{"pattern", 2.0f}, {"time", 100.0f}, {"taps", 6.0f}, {"diffusion", 0.0f}, {"motion", 0.0f}, {"mix", 100.0f}});
    const auto out = r.run(1.0, [](long n, int) { return n == 4800 ? 1.0f : 0.0f; });
    int peaks = 0;
    for (size_t i = 4801; i + 1 < out.size(); ++i)
        if (std::fabs(out[i]) > 0.05f && std::fabs(out[i]) >= std::fabs(out[i - 1]) && std::fabs(out[i]) > std::fabs(out[i + 1])) ++peaks;
    CHECK(peaks >= 5);
    CHECK(peaks <= 14);
    CHECK(rmsRange(out, 0.3, 1.0) < 1e-6);  // nothing after the span: no recursion
}

TEST_CASE("V5 Reverse Bloom: the tail of a hit arrives reversed (rising) a chunk later") {
    FxRig r;
    r.add("reverse_bloom", {{"sync", 0.0f}, {"length_ms", 800.0f}, {"tail", 3.0f}, {"mix", 100.0f}});
    const auto out = r.run(3.0, [](long n, int) { return n < 480 ? 0.8f * std::sin(0.3f * n) : 0.0f; });
    // in some chunk the reversed tail rises: its second half is louder than its first half
    double best = -100.0;
    for (double t = 0.4; t < 2.2; t += 0.1) best = std::max(best, dbv(rmsRange(out, t + 0.2, t + 0.4)) - dbv(rmsRange(out, t, t + 0.2)));
    CHECK(best > 6.0);
    for (float v : out) REQUIRE(std::isfinite(v));
}

TEST_CASE("V5 Swell: attacks are removed and fade in; a steady tone passes once the swell has risen") {
    FxRig r;
    r.add("swell", {{"rise", 300.0f}, {"depth", 100.0f}, {"wash", 0.0f}, {"mix", 100.0f}});
    const auto out = r.run(2.0, [](long n, int) { return n >= 24000 ? 0.5f * std::sin(2.0f * dsp::kPi * 220.0f * n / 48000.0f) : 0.0f; });
    const double early = rmsRange(out, 0.5, 0.52), late = rmsRange(out, 1.5, 1.9);
    CHECK(dbv(late) - dbv(early) > 15.0);
    CHECK(dbv(late) == doctest::Approx(dbv(0.5 / std::sqrt(2.0))).epsilon(0.05));
}

TEST_CASE("V5 Pulse Shaper: the pattern follows the host grid; Band = Lows moves only the lows") {
    FxRig r;
    r.host = true;
    r.add("pulse_shaper", {{"pattern", 0.0f}, {"division", 4.0f}, {"depth", 100.0f}, {"gate", 40.0f}, {"release", 5.0f}, {"accent", 0.0f}, {"mix", 100.0f}});
    const auto out = r.run(2.0, [](long n, int) { return 0.5f * std::sin(2.0f * dsp::kPi * 440.0f * n / 48000.0f); });
    // 120 bpm, 1/16 steps = 125 ms; "Eighths" = every other step open
    const double on = rmsRange(out, 1.0 + 0.01, 1.0 + 0.04), off = rmsRange(out, 1.125 + 0.06, 1.125 + 0.11);
    CHECK(dbv(on) - dbv(off) > 30.0);
    FxRig b;
    b.host = true;
    b.add("pulse_shaper", {{"pattern", 0.0f}, {"depth", 100.0f}, {"band", 1.0f}, {"mix", 100.0f}});
    const auto hi = b.run(2.0, [](long n, int) { return 0.3f * std::sin(2.0f * dsp::kPi * 8000.0f * n / 48000.0f); });
    double mn = 1e9, mx = 0;
    for (double t = 1.0; t < 1.9; t += 0.0625) { const double v = rmsRange(hi, t, t + 0.03); mn = std::min(mn, v); mx = std::max(mx, v); }
    CHECK(dbv(mx) - dbv(mn) < 1.0);  // an 8 kHz tone is not pulsed when only the lows move
}

TEST_CASE("V5 Drum Crush: Blend 0 is the dry signal; crushing lowers the crest factor of a hit train") {
    FxRig dry;
    dry.add("drum_crush", {{"blend", 0.0f}, {"crush", 100.0f}});
    const auto a = dry.run(1.0, hitTrain);
    for (size_t i = 0; i < a.size(); ++i) REQUIRE(std::fabs(a[i] - hitTrain(static_cast<long>(i), 0)) < 1e-4f);
    FxRig c;
    c.add("drum_crush", {{"blend", 100.0f}, {"crush", 90.0f}, {"drive", 12.0f}, {"release", 0.0f}});
    const auto b = c.run(2.0, hitTrain);
    auto crest = [](const std::vector<float>& x) {
        float pk = 0; double s = 0;
        for (size_t i = 24000; i < x.size(); ++i) { pk = std::max(pk, std::fabs(x[i])); s += static_cast<double>(x[i]) * x[i]; }
        return dbv(pk) - dbv(std::sqrt(s / (x.size() - 24000)));
    };
    std::vector<float> src(b.size());
    for (size_t i = 0; i < src.size(); ++i) src[i] = hitTrain(static_cast<long>(i), 0);
    CHECK(crest(b) < crest(src) - 3.0);
}

TEST_CASE("V5 Drum Space: the Gated room closes shortly after every hit; a live room keeps ringing") {
    double tail[2];
    for (int k = 0; k < 2; ++k) {
        FxRig r;
        r.add("drum_space", {{"mode", k == 0 ? 2.0f : 1.0f}, {"decay", 1.5f}, {"mix", 100.0f}});
        const auto out = r.run(1.0, [](long n, int) { return n < 2000 ? 0.7f * std::exp(-n / 300.0f) * std::sin(0.2f * n) : 0.0f; });
        tail[k] = dbv(rmsRange(out, 0.35, 0.5)) - dbv(rmsRange(out, 0.02, 0.1));
    }
    CHECK(tail[0] < tail[1] - 20.0);
}

TEST_CASE("V5 Band Transients: neutral settings null; cutting the high sustain shortens a bright tail only") {
    FxRig n0;
    n0.add("band_transients", {{"clip", 0.0f}});
    const auto a = n0.run(1.0, hitTrain);
    for (size_t i = 0; i < a.size(); ++i) REQUIRE(std::fabs(a[i] - hitTrain(static_cast<long>(i), 0)) < 2e-3f);
    // a hit with a real tail (a ringing snare / cymbal-like noise decay, tau 100 ms)
    auto ringing = [](long n, int c) { const long p = n % 24000; return hitTrain(n, c) / (0.6f * std::exp(-p / 900.0f) + 1e-9f) * (p < 20000 ? 0.6f * std::exp(-p / 4800.0f) : 0.0f); };
    FxRig s;
    s.add("band_transients", {{"high_sustain", -100.0f}, {"mid_sustain", -100.0f}, {"clip", 0.0f}});
    const auto b = s.run(1.5, ringing);
    std::vector<float> src(b.size());
    for (size_t i = 0; i < src.size(); ++i) src[i] = ringing(static_cast<long>(i), 0);
    // the tail (150-300 ms after the hit) loses level; the attack (first 3 ms) is kept
    CHECK(dbv(rmsRange(b, 1.15, 1.3)) < dbv(rmsRange(src, 1.15, 1.3)) - 4.0);
    CHECK(std::fabs(dbv(rmsRange(b, 1.0, 1.003)) - dbv(rmsRange(src, 1.0, 1.003))) < 2.0);
}

TEST_CASE("V5 Hit Melody: successive hits ring at different pitches of the scale") {
    FxRig r;
    r.add("hit_melody", {{"pattern", 0.0f}, {"decay", 0.4f}, {"mix", 100.0f}});
    const auto out = r.run(2.5, hitTrain);
    auto zc = [&](double t0) {
        int z = 0;
        const size_t a = static_cast<size_t>(t0 * kSr), b = a + 4800;
        for (size_t i = a + 1; i < b; ++i) z += (out[i - 1] < 0.0f) != (out[i] < 0.0f);
        return z;
    };
    // pattern Up: consecutive hits climb the scale (A minor pentatonic): the pitch (zero-crossing rate) rises
    const int z1 = zc(1.02), z2 = zc(1.52), z3 = zc(2.02);
    CHECK(z1 > 4);
    CHECK(z2 > z1);
    CHECK(z3 > z2);
    CHECK(rmsRange(out, 1.05, 1.15) > 0.01);  // clearly audible rings
}

// ============================================================================================ Phase 8: combination macros
namespace {
float drumsAndChord(long n, int c) {
    const float chord = 0.12f * static_cast<float>(std::sin(2 * dsp::kPi * 220.0 * n / kSr + 0.2 * c) + std::sin(2 * dsp::kPi * 329.6 * n / kSr));
    return hitTrain(n, c) + chord;
}
struct ComboCase { const char* name; std::vector<std::pair<const char*, std::vector<std::pair<const char*, float>>>> chain; std::vector<const char*> macros; };
std::vector<float> renderWithMacros(const ComboCase& cc, float v, int only = -1, double seconds = 1.5) {
    FxRig r;
    for (const auto& [t, ps] : cc.chain) r.add(t, ps);
    for (int m = 0; m < kNumPresetMacros; ++m) r.engine.setMacro(m, (only < 0 || m == only) ? v : 0.0f);
    r.run(0.3, drumsAndChord);  // macro smoothing settles
    r.n = 0;
    return r.run(seconds, drumsAndChord);
}
double relDiffDb(const std::vector<float>& a, const std::vector<float>& b) {
    double e = 0, r = 0;
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) { e += (a[i] - b[i]) * (a[i] - b[i]); r += static_cast<double>(b[i]) * b[i]; }
    return 10.0 * std::log10(std::max(e, 1e-24) / std::max(r, 1e-24));
}
}  // namespace

TEST_CASE("V5 macros: eligible combinations get their own named, audible, bounded, monotonic macros") {
    const std::vector<ComboCase> cases = {
        {"Drum Bus", {{"transient", {}}, {"drum_crush", {{"blend", 40.0f}}}}, {"Impact", "Body", "Dirt"}},
        {"Pulse into Echo", {{"pulse_shaper", {{"depth", 40.0f}}}, {"delay", {{"mix", 30.0f}}}}, {"Pulse", "Swing", "Echo Motion"}},
        {"Groove Room", {{"glitch", {}}, {"drum_space", {}}}, {"Groove", "Room"}},
        {"Granular Space", {{"granular", {{"mix", 50.0f}}}, {"reverb", {}}}, {"Scatter", "Cloud"}},
        {"Dub Wear", {{"degrade", {{"mix", 60.0f}}}, {"tape_echo", {}}}, {"Wear", "Dub"}},
        {"Pitch Halo", {{"pitch_shift", {{"mix", 30.0f}}}, {"shimmer", {}}}, {"Halo", "Decay"}},
        {"Resonant Space", {{"modal", {{"mix", 30.0f}}}, {"cloud_chamber", {}}}, {"Ring", "Strike"}},
        {"Spectral Blur", {{"spectral_smear", {{"mix", 50.0f}}}, {"reverb", {}}}, {"Blur", "Space"}},
        {"Atmosphere", {{"reverb", {}}, {"delay", {}}}, {"Bloom", "Decay", "Drift"}},
        {"Moving Space", {{"chorus", {}}, {"cloud_chamber", {}}}, {"Drift", "Bloom"}},
        {"Deep Space", {{"reflections", {}}, {"cloud_chamber", {}}}, {"Bloom", "Drift"}},
    };
    for (const auto& cc : cases) {
        const std::string cname = cc.name;
        CAPTURE(cname);
        FxRig probe;
        for (const auto& [t, ps] : cc.chain) probe.add(t, ps);
        const auto& ms = probe.session.macros();
        std::string got;
        for (int k = 0; k < static_cast<int>(ms.size()); ++k) got += probe.session.macroDescription(k) + " | ";
        MESSAGE(cname << " -> " << got);
        CHECK(ms.size() >= cc.macros.size());
        if (ms.size() < cc.macros.size()) continue;
        for (size_t k = 0; k < cc.macros.size(); ++k) CHECK(ms[k].name == cc.macros[k]);
        CHECK(probe.session.macroCombination().find(cc.name) != std::string::npos);
        // every macro moves >= 2 parameters, the set spans both effects, and no parameter belongs to two macros
        std::set<std::pair<uint64_t, std::string>> seen;
        std::set<uint64_t> inst;
        for (const auto& m : ms) {
            CHECK(m.targets.size() >= 2);
            for (const auto& t : m.targets) { inst.insert(t.id); CHECK(seen.insert({t.id, t.param}).second); }
        }
        CHECK(inst.size() >= 2);
        // EACH macro on its own: audible and growing (the difference from 0 rises at 25 / 50 / 75 / 100 %), bounded
        const auto y0 = renderWithMacros(cc, 0.0f);
        for (int m = 0; m < static_cast<int>(ms.size()); ++m) {
            double prev = -300.0;
            for (float v : {0.25f, 0.5f, 0.75f, 1.0f}) {
                const auto y = renderWithMacros(cc, v, m);
                float pk = 0;
                for (float s : y) { REQUIRE(std::isfinite(s)); pk = std::max(pk, std::fabs(s)); }
                CHECK(pk < 4.0f);
                const double d = relDiffDb(y, y0);
                // monotonic until the difference saturates (near 0 dB the whole wet field already differs)
                if (prev < -3.0) CHECK_MESSAGE(d > prev - 1.5, ms[static_cast<size_t>(m)].name << " not monotonic at " << v);
                prev = std::max(prev, d);
            }
            CHECK_MESSAGE(prev > -26.0, cname << " / " << ms[static_cast<size_t>(m)].name << " difference at 100 % only " << prev << " dB");
        }
    }
}

TEST_CASE("V5 macros: no combination, no macros; bypassing a member removes them; save / load restores them") {
    FxRig a;
    a.add("eq", {});
    a.add("reverb", {});
    CHECK(a.session.macros().empty());
    CHECK(a.session.activeMacroCount() == 0);
    FxRig b;
    b.add("reverb", {});
    b.add("delay", {});
    REQUIRE(!b.session.macros().empty());
    const auto gen = b.session.macroGeneration();
    REQUIRE(b.session.setEnabled(b.session.chain()[1].id(), false));
    CHECK(b.session.macros().empty());
    CHECK(b.session.macroGeneration() != gen);
    REQUIRE(b.session.setEnabled(b.session.chain()[1].id(), true));
    REQUIRE(!b.session.macros().empty());
    const auto names = b.session.macros().size();
    const std::string doc = b.session.toJson(true);
    FxRig c;
    REQUIRE(c.session.fromJson(doc, true));
    CHECK(c.session.macros().size() == names);
    CHECK(c.session.macrosGenerated());
    // a tape echo pushed by a macro never enters its self-oscillating range (> 100 %)
    FxRig d;
    d.add("degrade", {});
    d.add("tape_echo", {{"intensity", 95.0f}});
    for (int m = 0; m < kNumPresetMacros; ++m) d.engine.setMacro(m, 1.0f);
    d.run(0.5, drumsAndChord);
    const auto tail = d.run(14.0, [](long, int) { return 0.0f; });
    CHECK(rmsRange(tail, 12.0, 14.0) < rmsRange(tail, 0.5, 2.5));  // decays
}

// ============================================================================================ Phase 5: runaway regressions
// Each case is the worst legal setting found by tools/ih1_feedback_audit for a loop that used to grow or hold at full
// level. After an impulse, a noise burst and a chord, 12 s of silence must end at least 12 dB below the excited level
// (or under -70 dBFS), with no growth, no fault and no overs.
namespace {
void expectDecays(const char* type, std::vector<std::pair<const char*, float>> params) {
    CAPTURE(std::string(type));
    FxRig r;
    r.add(type, params);
    const auto ex = r.run(2.5, [](long n, int c) {
        if (n == 0) return 0.9f;
        if (n < 19200) { uint32_t h = static_cast<uint32_t>(n * 2 + c) * 0x9E3779B1u; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 13; return 0.35f * (static_cast<float>(h >> 8) / 8388608.0f - 1.0f); }
        if (n < 120000) return 0.25f * static_cast<float>(std::sin(2 * dsp::kPi * 110.0 * n / kSr) + std::sin(2 * dsp::kPi * 164.8 * n / kSr));
        return 0.0f;
    });
    const auto tail = r.run(12.0, [](long, int) { return 0.0f; });
    double exc = 0;
    for (double t = 0; t < 2.4; t += 0.1) exc = std::max(exc, rmsRange(ex, t, t + 0.1));
    double after = 0;
    for (double t = 1.0; t < 12.0; t += 0.1) after = std::max(after, rmsRange(tail, t, t + 0.1));
    const double late = rmsRange(tail, 10.0, 12.0);
    float pk = 0;
    for (float v : tail) { REQUIRE(std::isfinite(v)); pk = std::max(pk, std::fabs(v)); }
    CHECK(r.engine.telemetry().faults.load() == 0u);
    CHECK(pk < 2.0f);
    CHECK(dbv(after) < dbv(exc) + 3.0);                     // no growth after the excitation
    CHECK((dbv(late) < dbv(exc) - 12.0 || dbv(late) < -70.0));  // and it keeps decaying
}
}  // namespace

TEST_CASE("V5 runaway regression: shimmer pitch-in-loop (regen 100 %, unison shift, 30 s decay)") {
    expectDecays("shimmer", {{"decay", 30.0f}, {"size", 100.0f}, {"regen", 100.0f}, {"shift1", 0.0f}, {"shift2", 0.0f}, {"mix", 100.0f}});
    expectDecays("shimmer", {{"decay", 20.0f}, {"regen", 100.0f}, {"shift1", 12.0f}, {"shift2", 19.0f}, {"balance", 50.0f}, {"routing", 2.0f}, {"mix", 100.0f}});
}
TEST_CASE("V5 runaway regression: granular cloud feedback at maximum density x size") {
    expectDecays("granular", {{"size_ms", 500.0f}, {"density", 60.0f}, {"feedback", 100.0f}, {"mix", 100.0f}});
}
TEST_CASE("V5 runaway regression: reverse (grains, buffer, delay modes) at maximum feedback") {
    expectDecays("reverse", {{"mode", 2.0f}, {"feedback", 100.0f}, {"density", 60.0f}, {"grain_ms", 300.0f}, {"mix", 100.0f}});
    expectDecays("reverse", {{"mode", 0.0f}, {"sync", 0.0f}, {"length_ms", 1350.0f}, {"feedback", 100.0f}, {"pitch", 0.0f}, {"mix", 100.0f}});
    expectDecays("reverse", {{"mode", 1.0f}, {"sync", 0.0f}, {"length_ms", 600.0f}, {"feedback", 100.0f}, {"pitch", -7.0f}, {"mix", 100.0f}});
}
TEST_CASE("V5 runaway regression: frequency shifter feedback (out-of-quadrature band edges)") {
    expectDecays("freq_shift", {{"shift", 0.0f}, {"fine", 0.5f}, {"feedback", 100.0f}, {"delay_ms", 40.0f}, {"mix", 100.0f}});
}
TEST_CASE("V5 runaway regression: chorus feedback with four coherent voices") {
    expectDecays("chorus", {{"voices", 3.0f}, {"feedback", 100.0f}, {"depth", 0.0f}, {"mix", 100.0f}});
}
TEST_CASE("V5 runaway regression: delay at 100 % feedback with full drive; tape echo with three heads") {
    expectDecays("delay", {{"sync", 0.0f}, {"time_ms", 300.0f}, {"feedback", 100.0f}, {"drive", 100.0f}, {"mix", 100.0f}});
    expectDecays("tape_echo", {{"sync", 0.0f}, {"time_ms", 250.0f}, {"heads", 6.0f}, {"intensity", 100.0f}, {"saturation", 100.0f}, {"mix", 100.0f}});
}
TEST_CASE("V5 runaway regression: pitch shift feedback with coinciding voices") {
    expectDecays("pitch_shift", {{"shift_a", 0.0f}, {"shift_b", 0.0f}, {"level_b", 0.0f}, {"width", 100.0f}, {"delay_ms", 200.0f}, {"feedback", 100.0f}, {"mix", 100.0f}});
}
TEST_CASE("V5 runaway regression: grain engine feedback at maximum density") {
    expectDecays("grain_engine", {{"size_ms", 1000.0f}, {"density", 200.0f}, {"voices", 32.0f}, {"feedback", 100.0f}, {"fb_drive", 24.0f}, {"mix", 100.0f}});
}
TEST_CASE("V5 regression: bitcrusher dither leaves digital silence silent") {
    FxRig r;
    r.add("bitcrush", {{"bits", 4.0f}, {"dither", 1.0f}, {"mix", 100.0f}});
    r.run(0.5, [](long n, int) { return 0.3f * std::sin(0.05f * n); });
    const auto tail = r.run(1.0, [](long, int) { return 0.0f; });
    CHECK(rmsRange(tail, 0.2, 1.0) == 0.0);
}

// ============================================================================================ Phases 4 / 10: preset gate
TEST_CASE("V5 bank: counts, metadata, and no near-duplicates") {
    int total = 0, v5 = 0, atmos = 0, drums = 0, groove = 0;
    for (const auto& p : factoryPresets()) {
        ++total;
        if (p.version < 5) continue;
        ++v5;
        CHECK_MESSAGE(!p.description.empty(), p.id);
        CHECK_MESSAGE(!p.tags.empty(), p.id);
        CHECK_MESSAGE(!p.intensity.empty(), p.id);
        CHECK_MESSAGE(!p.styles.empty(), p.id);
        std::string err;
        CHECK_MESSAGE(validatePreset(p, &err), p.id << ": " << err);
        atmos += p.category == "Atmosphere / Cinematic / Drone" || p.id == "v5.blue_hour" || p.id == "v5.soft_focus";
        drums += p.category == "Drums / Drum Bus" || p.tags.find("drums") == 0;
        bool pulse = false;
        for (const auto& f : p.chain) pulse = pulse || f.type == "pulse_shaper" || f.type == "tremolo";
        groove += pulse;
    }
    MESSAGE(total << " factory presets, " << v5 << " new in V5: " << atmos << " atmosphere, " << drums << " drums, " << groove << " tremolo / volume shaping");
    CHECK(total >= 200);
    CHECK(v5 >= 50);
    CHECK(atmos >= 20);
    CHECK(drums >= 20);
    CHECK(groove >= 15);
    // near-duplicates: the same effect chain (types in order) with every parameter within 5 % of its range
    const auto& bank = factoryPresets();
    for (size_t a = 0; a < bank.size(); ++a)
        for (size_t b = a + 1; b < bank.size(); ++b) {
            const auto& A = bank[a];
            const auto& B = bank[b];
            if (A.chain.size() != B.chain.size() || A.chain.empty()) continue;
            bool same = true;
            for (size_t k = 0; same && k < A.chain.size(); ++k) same = A.chain[k].type == B.chain[k].type;
            if (!same) continue;
            bool close = true;
            for (size_t k = 0; close && k < A.chain.size(); ++k) {
                const auto& d = fxRegistry()[static_cast<size_t>(fxTypeIndex(A.chain[k].type))];
                auto va = d.defaults(), vb = d.defaults();
                for (const auto& [id, v] : A.chain[k].params) if (d.paramIndex(id) >= 0) va[static_cast<size_t>(d.paramIndex(id))] = v;
                for (const auto& [id, v] : B.chain[k].params) if (d.paramIndex(id) >= 0) vb[static_cast<size_t>(d.paramIndex(id))] = v;
                for (size_t i = 0; close && i < d.params.size(); ++i) {
                    const float span = std::max(1e-6f, d.params[i].maxValue - d.params[i].minValue);
                    close = std::fabs(va[i] - vb[i]) <= 0.05f * span;
                }
            }
            CHECK_MESSAGE(!close, A.id << " and " << B.id << " are near-duplicates");
        }
}

TEST_CASE("V5 bank: every macro of every preset (authored or combination) audibly changes the sound on its own") {
    int checked = 0;
    std::string weak;
    for (const auto& p : factoryPresets()) {
        if (p.id == kInitPresetId) continue;
        FxRig probe;
        REQUIRE(probe.session.fromJson(presetToJson(p), false));
        const int nm = static_cast<int>(probe.session.macros().size());
        if (nm == 0) continue;
        auto render = [&](int only, float v) {
            FxRig r;
            REQUIRE(r.session.fromJson(presetToJson(p), false));
            for (int m = 0; m < kNumPresetMacros; ++m) r.engine.setMacro(m, m == only ? v : 0.0f);
            r.run(0.4, drumsAndChord);
            r.n = 0;
            return r.run(1.2, drumsAndChord);
        };
        const auto y0 = render(-1, 0.0f);
        for (int m = 0; m < nm; ++m) {
            if (probe.session.macros()[static_cast<size_t>(m)].targets.empty()) continue;
            const double d = relDiffDb(render(m, 1.0f), y0);
            ++checked;
            if (d < -30.0) weak += p.id + "/" + probe.session.macros()[static_cast<size_t>(m)].name + " (" + std::to_string(static_cast<int>(d)) + " dB) ";
        }
    }
    MESSAGE(checked << " macros checked; inaudible: " << (weak.empty() ? std::string("none") : weak));
    CHECK(weak.empty());
}
