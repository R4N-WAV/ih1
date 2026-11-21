// Factory preset library (V1 feedback 4.15, 4.16): every preset is real, valid, loads atomically and plays.
#include <doctest/doctest.h>

#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "ih1/engine/Engine.h"
#include "ih1/fx/FxProcessor.h"
#include "ih1/model/Json.h"
#include "ih1/model/Session.h"
#include "ih1/presets/Presets.h"

using namespace ih1;

TEST_CASE("factory library: at least 50 distinct presets across the categories, unique ids and names") {
    const auto& all = factoryPresets();
    CHECK(all.size() >= 50);
    std::set<std::string> ids, names, chains;
    std::map<std::string, int> perCat;
    std::set<std::string> cats(presetCategories().begin(), presetCategories().end());
    for (const auto& p : all) {
        CHECK_MESSAGE(ids.insert(p.id).second, "duplicate id " << p.id);
        CHECK_MESSAGE(names.insert(p.name).second, "duplicate name " << p.name);
        CHECK_MESSAGE(cats.count(p.category) == 1, p.id << " has unknown category " << p.category);
        CHECK(p.category != "User");
        CHECK(!p.description.empty());
        CHECK_MESSAGE(!p.tags.empty(), p.id << " has no source tags");
        ++perCat[p.category];
        // no near-duplicates: the full chain (types and every set value) must differ from all others
        std::string sig;
        for (const auto& fx : p.chain) {
            sig += fx.type + "{";
            for (const auto& [k, v] : fx.params) sig += k + "=" + std::to_string(v) + ",";
            sig += "}>";
        }
        CHECK_MESSAGE(chains.insert(sig).second, p.id << " repeats another preset's chain " << sig);
        if (p.id != kInitPresetId) CHECK_MESSAGE(p.chain.size() >= 2, p.id << " should combine effects");
    }
    for (const auto& c : presetCategories())
        if (c != "User") CHECK_MESSAGE(perCat[c] >= (c == "Init / Utility" ? 1 : 3), c << " has only " << perCat[c]);
    // presets are listed grouped by category, in category order (the browser and prev/next rely on it)
    int lastCat = -1;
    for (const auto& p : all) {
        int ci = 0;
        while (presetCategories()[static_cast<size_t>(ci)] != p.category) ++ci;
        CHECK(ci >= lastCat);
        lastCat = ci;
    }
}

TEST_CASE("every factory preset validates against the registry") {
    for (const auto& p : factoryPresets()) {
        std::string err;
        CHECK_MESSAGE(validatePreset(p, &err), err);
    }
    FactoryPreset bad{"x.bad", "Bad", "Experimental", "d", 1, {{"delay", {{"no_such_param", 1.0f}}}}, "tags"};
    CHECK_FALSE(validatePreset(bad));
    bad.chain = {{"no_such_fx", {}}};
    CHECK_FALSE(validatePreset(bad));
    bad.chain = {{"delay", {{"feedback", 1000.0f}}}};
    CHECK_FALSE(validatePreset(bad));
    bad.chain.clear();
    CHECK_FALSE(validatePreset(bad));
}

TEST_CASE("every factory preset loads as one atomic chain with exact order and values, and plays cleanly") {
    for (const auto& p : factoryPresets()) {
        Engine engine;
        Session session(engine);
        session.prepare(48000.0, 256);
        // something staged and something committed beforehand: the preset replaces the chain only
        session.select(fxTypeIndex("chorus"));
        session.commit();
        session.select(fxTypeIndex("delay"));
        std::string err;
        REQUIRE_MESSAGE(session.fromJson(presetToJson(p), false, &err), p.id << ": " << err);
        CHECK_MESSAGE(err.empty(), p.id << " warnings: " << err);
        REQUIRE(session.chain().size() == p.chain.size());
        CHECK(session.selected() == Session::kNone);  // V3: the preset is auditioned alone (staged -> None)
        for (size_t i = 0; i < p.chain.size(); ++i) {
            const auto& ci = session.chain()[i];
            CHECK(ci.descriptor().typeId == p.chain[i].type);
            for (const auto& [pid, val] : p.chain[i].params) {
                const int k = ci.descriptor().paramIndex(pid);
                CHECK(ci.values()[static_cast<size_t>(k)] == doctest::Approx(val));
            }
        }
        // deterministic identity: loading twice gives the same instance ids (same formation)
        const auto firstId = session.chain()[0].id();
        REQUIRE(session.fromJson(presetToJson(p), false, &err));
        CHECK(session.chain()[0].id() == firstId);
        // audio: finite and bounded with a sine plus noise, no runaway feedback
        session.setMute(true);  // hear the committed chain only
        std::vector<float> L(256), R(256);
        double ph = 0;
        uint32_t rng = 12345;
        float peak = 0.0f;
        bool finite = true;
        for (int b = 0; b < 48000 * 3 / 256; ++b) {
            for (int i = 0; i < 256; ++i) {
                rng = rng * 1664525u + 1013904223u;
                const float noise = (static_cast<float>(rng >> 9) / 8388608.0f - 1.0f) * 0.05f;
                L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = (b < 300 ? 0.3f * static_cast<float>(std::sin(ph)) : 0.0f) + noise;
                ph += 2.0 * 3.14159265358979 * 220.0 / 48000.0;
            }
            ProcessContext ctx;
            ctx.sampleRate = 48000.0;
            ctx.bpm = 120.0;
            ctx.hostPlaying = true;
            ctx.ppqValid = true;
            ctx.ppqPosition = b * 256.0 / 48000.0 * 2.0;
            engine.process(L.data(), R.data(), 256, ctx);
            for (int i = 0; i < 256; ++i) {
                finite = finite && std::isfinite(L[static_cast<size_t>(i)]) && std::isfinite(R[static_cast<size_t>(i)]);
                peak = std::max({peak, std::fabs(L[static_cast<size_t>(i)]), std::fabs(R[static_cast<size_t>(i)])});
            }
            session.service();
        }
        CHECK_MESSAGE(finite, p.id);
        CHECK_MESSAGE(peak < 4.0f, p.id << " peak " << peak);
        CHECK_MESSAGE(peak > 0.005f, p.id << " is silent");
    }
}

namespace {
// Three synthetic sources: a drum loop (kick + noise snare + hats), a sustained pad chord, and vocal-like
// formant pulses (a 140 Hz pulse train through two resonances, phrased). Deterministic.
std::vector<float> source(int kind, int n) {
    std::vector<float> x(static_cast<size_t>(n));
    uint32_t rng = 777u + static_cast<uint32_t>(kind);
    auto noise = [&] { rng = rng * 1664525u + 1013904223u; return static_cast<float>(rng >> 9) / 8388608.0f - 1.0f; };
    double ph = 0, f1s = 0, f1c = 0, f2s = 0, f2c = 0;
    for (int i = 0; i < n; ++i) {
        const double t = i / 48000.0;
        float v = 0.0f;
        if (kind == 0) {
            const double beat = std::fmod(t, 0.5);
            v += 0.7f * static_cast<float>(std::exp(-beat * 18.0) * std::sin(2 * M_PI * (50.0 + 80.0 * std::exp(-beat * 30.0)) * beat));
            const double sn = std::fmod(t + 0.25, 1.0);
            if (sn < 0.2) v += 0.35f * static_cast<float>(std::exp(-sn * 25.0)) * noise();
            const double hh = std::fmod(t, 0.125);
            v += 0.08f * static_cast<float>(std::exp(-hh * 120.0)) * noise();
        } else if (kind == 1) {
            for (double f : {220.0, 261.63, 329.63, 392.0}) v += 0.12f * static_cast<float>(std::sin(2 * M_PI * f * t + f));
        } else {
            ph += 140.0 / 48000.0;
            const float pulse = ph >= 1.0 ? (ph -= 1.0, 1.0f) : 0.0f;
            // two resonators (formants ~700 Hz and ~1200 Hz)
            auto res = [](double& s, double& c, double f, float in) { const double w = 2 * M_PI * f / 48000.0, r = 0.995; const double ns = r * (s * std::cos(w) + c * std::sin(w)) + in; c = r * (c * std::cos(w) - s * std::sin(w)); s = ns; return static_cast<float>(s); };
            const float phrase = std::fmod(t, 1.2) < 0.8 ? 1.0f : 0.0f;
            v = phrase * 0.05f * (res(f1s, f1c, 700.0, pulse) + 0.6f * res(f2s, f2c, 1200.0, pulse));
        }
        x[static_cast<size_t>(i)] = v;
    }
    // every source at a typical working level: -18 dBFS RMS
    double sq = 0;
    for (float v : x) sq += static_cast<double>(v) * v;
    const float g = static_cast<float>(std::pow(10.0, -18.0 / 20.0) / std::sqrt(std::max(sq / n, 1e-20)));
    for (auto& v : x) v *= g;
    return x;
}
}  // namespace

TEST_CASE("factory audit: every preset on drums, a pad and a voice -- sane loudness, no clipping, mono-safe, deterministic") {
    const int n = 48000 * 4;
    const std::vector<float> src[3] = {source(0, n), source(1, n), source(2, n)};
    const char* names[3] = {"drums", "pad", "voice"};
    int loudest = 0, quietest = 0;
    for (const auto& p : factoryPresets()) {
        for (int s = 0; s < 3; ++s) {
            double inSq = 0, outSq = 0, lr = 0, ll = 0, rr = 0;
            float peak = 0.0f;
            std::vector<float> first;
            for (int pass = 0; pass < 2; ++pass) {
                Engine engine;
                Session session(engine);
                session.prepare(48000.0, 256);
                std::string err;
                REQUIRE(session.fromJson(presetToJson(p), false, &err));
                std::vector<float> L(256), R(256), out;
                out.reserve(static_cast<size_t>(n));
                for (int off = 0; off + 256 <= n; off += 256) {
                    for (int i = 0; i < 256; ++i) L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = src[s][static_cast<size_t>(off + i)];
                    ProcessContext ctx;
                    ctx.sampleRate = 48000.0;
                    ctx.bpm = 120.0;
                    ctx.hostPlaying = true;
                    ctx.ppqValid = true;
                    ctx.ppqPosition = off / 48000.0 * 2.0;
                    engine.process(L.data(), R.data(), 256, ctx);
                    session.service();
                    for (int i = 0; i < 256; ++i) {
                        out.push_back(L[static_cast<size_t>(i)]);
                        if (pass == 0 && off >= 48000) {  // after the first second (tails, captures warmed up)
                            const double x = src[s][static_cast<size_t>(off + i)], a = L[static_cast<size_t>(i)], b = R[static_cast<size_t>(i)];
                            inSq += x * x;
                            outSq += 0.5 * (a * a + b * b);
                            lr += a * b; ll += a * a; rr += b * b;
                            peak = std::max({peak, std::fabs(L[static_cast<size_t>(i)]), std::fabs(R[static_cast<size_t>(i)])});
                        }
                    }
                }
                if (pass == 0) first = out;
                else {
                    bool same = first.size() == out.size();
                    for (size_t i = 0; same && i < out.size(); ++i) same = first[i] == out[i];
                    CHECK_MESSAGE(same, p.id << " on " << names[s] << " is not deterministic");
                }
            }
            const double gainDb = 10.0 * std::log10(std::max(outSq, 1e-12) / std::max(inSq, 1e-12));
            const double corr = lr / std::sqrt(std::max(ll * rr, 1e-24));
            INFO(std::string(p.id) << " on " << names[s] << ": " << gainDb << " dB, peak " << peak << ", L/R correlation " << corr);
            CHECK(gainDb < 9.0);    // never much louder than the source (no hidden level boost)
            CHECK(gainDb > -24.0);  // never (nearly) silent
            CHECK(peak < 2.0f);     // within +6 dBFS on a source peaking around -3 dBFS
            CHECK(corr > -0.3);     // mono-safe: no strong anti-phase
            loudest = std::max(loudest, static_cast<int>(gainDb));
            quietest = std::min(quietest, static_cast<int>(gainDb));
        }
    }
    MESSAGE("factory audit level range: " << quietest << " .. " << loudest << " dB relative to the source");
}

TEST_CASE("preset JSON carries metadata and malformed preset files are rejected without touching the chain") {
    const auto& p = factoryPresets().front();
    json::Value root;
    std::string err;
    REQUIRE(json::parse(presetToJson(p), root, &err));
    CHECK(root.get("preset").get("name").asString() == p.name);
    CHECK(root.get("preset").get("category").asString() == p.category);
    Engine engine;
    Session s(engine);
    s.prepare(48000.0, 256);
    REQUIRE(s.fromJson(presetToJson(p), false, &err));
    const size_t n = s.chain().size();
    CHECK_FALSE(s.fromJson("{\"format\":\"ih1-state\",\"version\":1}", false, &err));
    CHECK_FALSE(s.fromJson("not json", false, &err));
    CHECK_FALSE(s.fromJson("{\"format\":\"other\",\"version\":1,\"chain\":[]}", false, &err));
    CHECK(s.chain().size() == n);
}

// V3 Phase 7.3: the creative bank is measurably bold. Each creative preset is rendered on its first two tagged
// sources (level-matched; see ih1/presets/Audition.h); at least two-thirds must be transformative on both.
#include "ih1/presets/Audition.h"
TEST_CASE("V3 factory bank: at least two-thirds of the creative presets are transformative (level-matched)") {
    int creative = 0, transformative = 0;
    std::string timid;
    for (const auto& p : factoryPresets()) {
        if (p.category == "Init / Utility") continue;
        ++creative;
        const auto srcs = audition::sourcesForTags(p.tags);
        int t = 0, n = 0;
        for (size_t k = 0; k < srcs.size() && k < 2; ++k, ++n) {
            const auto r = audition::run(presetToJson(p), srcs[k], 3.0);
            REQUIRE(r.finite);
            t += r.cls == audition::Class::Transformative;
        }
        if (t == n) ++transformative;
        else timid += p.id + " ";
    }
    MESSAGE(transformative << " of " << creative << " creative presets transformative; not: " << timid);
    CHECK(creative >= 60);
    CHECK(3 * transformative >= 2 * creative);
}
