// IH1 V5 (feedback round 5) behaviour tests: commit keeps the browser position (Phase 7), feedback stability
// (Phase 5), the new processors (Phases 2 / 3), combination macros (Phase 8) and the bank gate (Phase 10).
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
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
