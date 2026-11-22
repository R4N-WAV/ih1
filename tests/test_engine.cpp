// Engine + Session semantics: commit workflow, SOLO/MUTE, reorder/delete, latency, state, threading.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <cmath>
#include <random>
#include <thread>
#include <vector>

#include "ih1/engine/Engine.h"
#include "ih1/model/Session.h"
#include "ih1/model/Json.h"

using namespace ih1;

namespace {

constexpr double kSr = 48000.0;
constexpr int kBlock = 256;

struct Harness {
    Engine engine;
    Session session{engine};
    double phase = 0.0;
    float freq = 220.0f;
    std::vector<float> L, R, inL;
    float maxJump = 0.0f;
    float lastOut = 0.0f;
    bool first = true;

    Harness() { session.prepare(kSr, kBlock); L.resize(kBlock); R.resize(kBlock); inL.resize(kBlock); }

    // Process `seconds` of a sine (amplitude 0.5); returns RMS of the last block (left).
    float run(double seconds, int block = kBlock) {
        const int total = static_cast<int>(seconds * kSr);
        const int tailStart = std::max(0, total - static_cast<int>(0.1 * kSr));  // 22 whole cycles of 220 Hz
        double tailSq = 0.0;
        int tailN = 0;
        for (int done = 0; done < total; done += block) {
            const int n = std::min(block, total - done);
            for (int i = 0; i < n; ++i) {
                const float x = 0.5f * static_cast<float>(std::sin(phase));
                phase += 2.0 * M_PI * freq / kSr;
                L[i] = R[i] = inL[i] = x;
            }
            ProcessContext ctx;
            ctx.sampleRate = kSr;
            engine.process(L.data(), R.data(), n, ctx);
            for (int i = 0; i < n; ++i) {
                REQUIRE(std::isfinite(L[i]));
                if (done + i >= tailStart) { tailSq += L[i] * L[i]; ++tailN; }
                if (!first) maxJump = std::max(maxJump, std::fabs(L[i] - lastOut));
                lastOut = L[i];
                first = false;
            }
            session.service();
        }
        return static_cast<float>(std::sqrt(tailSq / std::max(1, tailN)));
    }
    void resetJump() { maxJump = 0.0f; }
};

const float kSineRms = 0.5f / std::sqrt(2.0f);
// 220 Hz at amplitude 0.5 moves up to 0.0144 per sample by itself; a 5 ms fade adds ~0.002.
// A real click (hard switch) is 0.1..0.5. Anything below this limit is smooth.
const float kClickLimit = 0.025f;
float db(float lin) { return 20.0f * std::log10(std::max(lin, 1e-9f)); }

int T(const char* id) {
    const int t = fxTypeIndex(id);
    REQUIRE(t >= 0);
    return t;
}

void stageGain(Session& s, float gainDb) {
    s.select(T("test_gain"));
    s.auditionEdit();  // V5: after COMMIT the same effect stays selected, muted; editing it (the UI gesture) auditions it
    s.setDraftParam(0, gainDb);
}

}  // namespace

TEST_CASE("empty chain is transparent") {
    Harness h;
    const float rms = h.run(0.2);
    CHECK(rms == doctest::Approx(kSineRms).epsilon(0.01));
}

TEST_CASE("staged FX runs after the committed chain") {
    Harness h;
    stageGain(h.session, -6.0f);
    CHECK(h.session.commit());
    stageGain(h.session, -6.0f);
    const float rms = h.run(0.3);
    CHECK(db(rms / kSineRms) == doctest::Approx(-12.0).epsilon(0.02));
}

TEST_CASE("commit snapshots, keeps the browser on the effect, resets and mutes the staged copy, and is immutable") {
    Harness h;
    stageGain(h.session, -10.0f);
    h.run(0.1);
    REQUIRE(h.session.commit());
    // V5 Phase 7: the carousel stays on the same effect; the staged copy is fresh (defaults) and muted ("standby")
    CHECK(h.session.selected() == T("test_gain"));
    CHECK(h.session.mute());
    CHECK(h.session.standby());
    REQUIRE(h.session.chain().size() == 1);
    CHECK(h.session.chain()[0].values()[0] == doctest::Approx(-10.0f));
    CHECK(h.session.draft(T("test_gain"))[0] == doctest::Approx(0.0f));  // the draft is back at its default
    CHECK(h.session.stagedProcessor() != h.session.chain()[0].processor());  // nothing shared with the snapshot
    // no double processing: the committed copy alone is heard (-10 dB, not -10 + default)
    CHECK(db(h.run(0.3) / kSineRms) == doctest::Approx(-10.0).epsilon(0.02));
    // host automation / programmatic edits do not end standby; a UI edit does (auditionEdit)
    h.session.setDraftParam(0, -3.0f);
    CHECK(h.session.standby());
    CHECK(db(h.run(0.3) / kSineRms) == doctest::Approx(-10.0).epsilon(0.02));
    CHECK(h.session.auditionEdit());
    CHECK_FALSE(h.session.mute());
    CHECK(db(h.run(0.3) / kSineRms) == doctest::Approx(-13.0).epsilon(0.02));
    // committing again makes a deliberate duplicate with its own values
    REQUIRE(h.session.commit());
    CHECK(h.session.chain().size() == 2);
    CHECK(h.session.chain()[1].values()[0] == doctest::Approx(-3.0f));
    CHECK(h.session.chain()[0].values()[0] == doctest::Approx(-10.0f));
    CHECK(db(h.run(0.3) / kSineRms) == doctest::Approx(-13.0).epsilon(0.02));
    // with nothing staged, COMMIT is impossible
    h.session.setMute(false);
    h.session.select(Session::kNone);
    CHECK_FALSE(h.session.commit());
    CHECK(h.session.chain().size() == 2);
    h.session.remove(h.session.chain()[1].id());
    // re-select and edit the draft: committed output must not change
    h.session.select(T("test_gain"));
    h.session.setDraftParam(0, 6.0f);
    h.session.setMute(true);  // listen to the committed chain only
    const float rms = h.run(0.3);
    CHECK(db(rms / kSineRms) == doctest::Approx(-10.0).epsilon(0.02));
    CHECK(h.session.chain()[0].values()[0] == doctest::Approx(-10.0f));
}

TEST_CASE("commit hand-over is seamless") {
    Harness h;
    stageGain(h.session, -3.0f);
    h.run(0.2);
    h.resetJump();
    h.session.commit();
    h.run(0.2);
    // sine at 220 Hz, amplitude 0.5*0.71: max per-sample delta ~ 0.0072; allow small margin
    CHECK(h.maxJump < kClickLimit);
}

TEST_CASE("duplicate instances are independent") {
    Harness h;
    stageGain(h.session, -6.0f);
    h.session.commit();
    stageGain(h.session, -12.0f);
    h.session.commit();
    REQUIRE(h.session.chain().size() == 2);
    CHECK(h.session.chain()[0].typeIndex() == h.session.chain()[1].typeIndex());
    CHECK(h.session.chain()[0].id() != h.session.chain()[1].id());
    CHECK(h.session.chain()[0].values()[0] == doctest::Approx(-6.0f));
    CHECK(h.session.chain()[1].values()[0] == doctest::Approx(-12.0f));
    const float rms = h.run(0.3);
    CHECK(db(rms / kSineRms) == doctest::Approx(-18.0).epsilon(0.02));
}

TEST_CASE("reorder changes real processing order, delete removes one instance, both click-free") {
    Harness h;
    stageGain(h.session, -20.0f);
    h.session.commit();
    stageGain(h.session, 0.0f);
    h.session.commit();
    h.run(0.2);
    auto& tel = h.engine.telemetry();
    CHECK(std::fabs(db(tel.slotRms[0].load() / kSineRms) + 20.0f) < 1.0f);  // block RMS: approximate
    const uint64_t firstId = h.session.chain()[0].id();
    h.resetJump();
    REQUIRE(h.session.move(0, 1));
    h.run(0.2);
    CHECK(h.session.chain()[1].id() == firstId);
    CHECK(tel.slotIds[1].load() == firstId);
    CHECK(std::fabs(db(tel.slotRms[0].load() / kSineRms)) < 1.0f);
    CHECK(h.maxJump < kClickLimit);

    h.resetJump();
    REQUIRE(h.session.remove(firstId));
    const float rms = h.run(0.2);
    CHECK(h.session.chain().size() == 1);
    CHECK(std::fabs(db(rms / kSineRms)) < 0.1f);
    CHECK(h.maxJump < kClickLimit);
    CHECK_FALSE(h.session.remove(firstId));
}

TEST_CASE("SOLO auditions only the staged FX; MUTE hears only the committed chain") {
    Harness h;
    stageGain(h.session, -20.0f);
    h.session.commit();
    stageGain(h.session, -6.0f);
    CHECK(db(h.run(0.3) / kSineRms) == doctest::Approx(-26.0).epsilon(0.02));

    h.session.setSolo(true);
    CHECK(db(h.run(0.5) / kSineRms) == doctest::Approx(-6.0).epsilon(0.02));
    CHECK(h.engine.telemetry().soloAmount.load() == doctest::Approx(1.0f));

    h.session.setMute(true);  // exclusive: releases SOLO
    CHECK_FALSE(h.session.solo());
    CHECK(db(h.run(0.5) / kSineRms) == doctest::Approx(-20.0).epsilon(0.02));

    h.session.setSolo(true);
    CHECK_FALSE(h.session.mute());
    h.session.setSolo(false);
    CHECK(db(h.run(0.5) / kSineRms) == doctest::Approx(-26.0).epsilon(0.02));
}

TEST_CASE("SOLO fades the committed chain smoothly") {
    Harness h;
    stageGain(h.session, -20.0f);
    h.session.commit();
    stageGain(h.session, 0.0f);
    h.run(0.2);
    h.resetJump();
    h.session.setSolo(true);
    h.run(0.4);
    CHECK(h.maxJump < kClickLimit);
}

TEST_CASE("RESET resets only the staged draft") {
    Harness h;
    stageGain(h.session, -6.0f);
    h.session.commit();
    stageGain(h.session, -9.0f);
    h.session.resetDraft();
    CHECK(h.session.draft(T("test_gain"))[0] == doctest::Approx(0.0f));
    CHECK(h.session.chain()[0].values()[0] == doctest::Approx(-6.0f));
    CHECK(db(h.run(0.3) / kSineRms) == doctest::Approx(-6.0).epsilon(0.02));
}

TEST_CASE("RESET clears the staged Looper's recording but never a committed Looper") {
    Harness h;
    const int looper = T("looper");
    const auto& d = fxRegistry()[static_cast<size_t>(looper)];
    const int pLen = d.paramIndex("length"), pMix = d.paramIndex("mix");
    REQUIRE(pLen >= 0);
    REQUIRE(pMix >= 0);
    auto stageLooper = [&] {
        h.session.select(looper);
        h.session.setDraftParam(pLen, 0.0f);    // 1 beat (0.5 s at 120 bpm)
        h.session.setDraftParam(pMix, 100.0f);  // loop only
    };
    // 1. a staged loop records and plays back into silence
    stageLooper();
    h.freq = 220.0f;
    h.run(0.7);                  // records the first 0.5 s of sound, then plays
    h.freq = 0.0f;               // silence in: only the loop sounds
    CHECK(h.run(0.4) > 0.05f);
    // 2. RESET: loop gone, back to waiting (silence stays silence), no click
    h.resetJump();
    h.session.resetDraft();
    h.run(0.1);                  // staged crossfade
    CHECK(h.run(0.4) < 1e-4f);
    CHECK(h.maxJump < kClickLimit);
    CHECK(h.session.draft(looper)[static_cast<size_t>(pMix)] == doctest::Approx(d.params[static_cast<size_t>(pMix)].defaultValue));

    // 3. a committed Looper's recording survives a staged Looper RESET
    Harness g;
    g.session.select(looper);
    g.session.setDraftParam(pLen, 0.0f);
    g.session.setDraftParam(pMix, 100.0f);
    g.freq = 220.0f;
    g.run(0.7);
    REQUIRE(g.session.commit());
    g.session.select(looper);    // stage a second Looper (draft kept: 1 beat, 100 %)
    g.session.setMute(true);     // hear the committed chain only
    g.freq = 0.0f;
    const float before = g.run(0.4);
    CHECK(before > 0.05f);
    g.session.resetDraft();
    g.run(0.1);
    CHECK(g.run(0.4) == doctest::Approx(before).epsilon(0.15));
    REQUIRE(g.session.chain().size() == 1);
}

TEST_CASE("draft settings survive carousel navigation") {
    Harness h;
    h.session.select(T("test_gain"));
    h.session.setDraftParam(0, -4.5f);
    h.session.select(T("test_latency"));
    h.session.setDraftParam(0, 3.0f);
    h.session.select(T("test_gain"));
    CHECK(h.session.draft(T("test_gain"))[0] == doctest::Approx(-4.5f));
    CHECK(h.session.draft(T("test_latency"))[0] == doctest::Approx(3.0f));
}

TEST_CASE("latency is compensated on the SOLO and dry paths") {
    Harness h;
    h.session.select(T("test_latency"));
    h.session.commit();
    CHECK(h.session.latencySamples() == 64);
    h.run(0.1);
    CHECK(h.engine.activeLatency() == 64);
    // MIX = 0 must output the input delayed by exactly the reported latency
    h.engine.setMix(0.0f);
    h.run(0.1);
    std::vector<float> in, out;
    for (int b = 0; b < 8; ++b) {
        for (int i = 0; i < kBlock; ++i) {
            const float x = 0.5f * static_cast<float>(std::sin(h.phase));
            h.phase += 2.0 * M_PI * 1000.0 / kSr;
            h.L[i] = h.R[i] = x;
            in.push_back(x);
        }
        ProcessContext ctx;
        h.engine.process(h.L.data(), h.R.data(), kBlock, ctx);
        out.insert(out.end(), h.L.begin(), h.L.end());
    }
    double err = 0;
    for (size_t i = 64; i < in.size(); ++i) err = std::max(err, static_cast<double>(std::fabs(out[i] - in[i - 64])));
    CHECK(err < 1e-5);
}

TEST_CASE("state: preset holds the chain only, session holds drafts; round trip is exact") {
    Harness h;
    stageGain(h.session, -6.5f);
    h.session.commit();
    stageGain(h.session, -1.25f);
    h.session.commit();
    h.session.select(T("test_latency"));
    h.session.setDraftParam(1, 1234.0f);
    h.session.setMute(true);

    const std::string preset = h.session.toJson(false);
    const std::string full = h.session.toJson(true);
    json::Value pv;
    REQUIRE(json::parse(preset, pv));
    CHECK_FALSE(pv.has("session"));
    CHECK(pv.get("chain").size() == 2);

    Harness h2;
    std::string err;
    REQUIRE(h2.session.fromJson(full, true, &err));
    REQUIRE(h2.session.chain().size() == 2);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(h2.session.chain()[i].id() == h.session.chain()[i].id());
        CHECK(h2.session.chain()[i].values() == h.session.chain()[i].values());
        CHECK(h2.session.chain()[i].visualSeed() == h.session.chain()[i].visualSeed());
    }
    // V3 Phase 1 (supersedes V2.1): a host state recall lands on None, so only the recalled chain is heard; the
    // drafts it saved are restored, so returning to the FX finds its cached settings.
    CHECK(h2.session.selected() == Session::kNone);
    CHECK_FALSE(h2.session.hasStaged());
    CHECK(h2.session.draft(T("test_latency"))[1] == doctest::Approx(1234.0f));
    CHECK_FALSE(h2.session.mute());
    // the saved drafts and chain round-trip exactly
    Harness h2b;
    REQUIRE(h2b.session.fromJson(h2.session.toJson(true), true));
    CHECK(h2b.session.toJson(true) == h2.session.toJson(true));

    // a user preset replaces the chain, deselects the staged FX (None) and keeps its draft cached
    Harness h3;
    h3.session.select(T("test_gain"));
    h3.session.setDraftParam(0, 5.0f);
    REQUIRE(h3.session.fromJson(preset, false));
    CHECK(h3.session.chain().size() == 2);
    CHECK(h3.session.selected() == Session::kNone);
    CHECK(h3.session.draft(T("test_gain"))[0] == doctest::Approx(5.0f));
    h3.session.select(T("test_gain"));
    CHECK(h3.session.draft(T("test_gain"))[0] == doctest::Approx(5.0f));
}

TEST_CASE("malformed or hostile state fails safely") {
    Harness h;
    stageGain(h.session, -6.0f);
    h.session.commit();
    const std::string before = h.session.toJson(true);
    std::string err;
    CHECK_FALSE(h.session.fromJson("", true, &err));
    CHECK_FALSE(h.session.fromJson("{\"format\":\"ih1-state\",\"version\":99,\"chain\":[]}", true, &err));
    CHECK_FALSE(h.session.fromJson("{\"format\":\"other\",\"version\":1,\"chain\":[]}", true, &err));
    CHECK_FALSE(h.session.fromJson("{\"format\":\"ih1-state\",\"version\":1,\"chain\":[", true, &err));
    CHECK_FALSE(h.session.fromJson(std::string(200, '['), true, &err));
    CHECK(h.session.toJson(true) == before);

    // unknown type skipped, out-of-range values clamped, duplicate ids re-issued, NaN-ish rejected
    const char* odd = R"({"format":"ih1-state","version":1,"chain":[
        {"id":"00000000000000aa","type":"no_such_fx","params":{}},
        {"id":"00000000000000bb","type":"test_gain","params":{"gain_db":999}},
        {"id":"00000000000000bb","type":"test_gain","params":{"gain_db":-3,"bogus":1}}]})";
    REQUIRE(h.session.fromJson(odd, false, &err));
    CHECK(err.find("no_such_fx") != std::string::npos);
    REQUIRE(h.session.chain().size() == 2);
    CHECK(h.session.chain()[0].values()[0] == doctest::Approx(12.0f));
    CHECK(h.session.chain()[1].values()[0] == doctest::Approx(-3.0f));
    CHECK(h.session.chain()[0].id() != h.session.chain()[1].id());
    CHECK(std::isfinite(h.run(0.1)));
}

TEST_CASE("chain length is bounded") {
    Harness h;
    for (int i = 0; i < kMaxChain + 5; ++i) {
        h.session.select(T("test_gain"));
        h.session.commit();
    }
    CHECK(static_cast<int>(h.session.chain().size()) == kMaxChain);
    CHECK(std::isfinite(h.run(0.05)));
}

TEST_CASE("variable block sizes and silence") {
    Harness h;
    stageGain(h.session, -6.0f);
    h.session.commit();
    for (int b : {1, 7, 64, 256, 255, 13}) CHECK(std::isfinite(h.run(0.02, b)));
}

TEST_CASE("threaded stress: random UI operations while audio runs") {
    Harness h;
    std::atomic<bool> stop{false};
    std::thread audio([&] {
        std::vector<float> L(kBlock), R(kBlock);
        std::mt19937 rng(1);
        double ph = 0;
        while (!stop.load()) {
            const int n = 1 + static_cast<int>(rng() % kBlock);
            for (int i = 0; i < n; ++i) { L[i] = R[i] = 0.3f * static_cast<float>(std::sin(ph)); ph += 0.05; }
            ProcessContext ctx;
            h.engine.process(L.data(), R.data(), n, ctx);
            for (int i = 0; i < n; ++i) REQUIRE(std::isfinite(L[i]));
        }
    });
    std::mt19937 rng(7);
    for (int it = 0; it < 4000; ++it) {
        switch (rng() % 10) {
            case 0: h.session.select(rng() % 2 ? T("test_gain") : T("test_latency")); break;
            case 1: h.session.setDraftParam(0, static_cast<float>(rng() % 30) - 20.0f); break;
            case 2: h.session.commit(); break;
            case 3: if (!h.session.chain().empty()) h.session.move(static_cast<int>(rng() % h.session.chain().size()), static_cast<int>(rng() % h.session.chain().size())); break;
            case 4: if (!h.session.chain().empty()) h.session.remove(h.session.chain()[rng() % h.session.chain().size()].id()); break;
            case 5: h.session.setSolo(rng() % 2); break;
            case 6: h.session.setMute(rng() % 2); break;
            case 7: h.session.select(-1); break;
            case 8: if (rng() % 4 == 0) h.session.clearChain(); break;
            case 9: if (rng() % 8 == 0) h.session.setLatencyMode(static_cast<LatencyMode>(rng() % 3)); break;
        }
        h.session.service();
        if (it % 50 == 0) std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    stop = true;
    audio.join();
    h.session.service();
    CHECK(h.session.chain().size() <= static_cast<size_t>(kMaxChain));
}
