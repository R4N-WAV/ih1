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

TEST_CASE("factory library: at least 24 distinct presets across the categories, unique ids and names") {
    const auto& all = factoryPresets();
    CHECK(all.size() >= 24);
    std::set<std::string> ids, names, chains;
    std::map<std::string, int> perCat;
    std::set<std::string> cats(presetCategories().begin(), presetCategories().end());
    for (const auto& p : all) {
        CHECK_MESSAGE(ids.insert(p.id).second, "duplicate id " << p.id);
        CHECK_MESSAGE(names.insert(p.name).second, "duplicate name " << p.name);
        CHECK_MESSAGE(cats.count(p.category) == 1, p.id << " has unknown category " << p.category);
        CHECK(p.category != "User");
        CHECK(!p.description.empty());
        ++perCat[p.category];
        std::string sig;
        for (const auto& fx : p.chain) sig += fx.type + ">";
        CHECK_MESSAGE(chains.insert(sig).second, p.id << " repeats another preset's chain " << sig);
        CHECK_MESSAGE(p.chain.size() >= 2, p.id << " should combine effects");
    }
    for (const auto& c : presetCategories())
        if (c != "User") CHECK_MESSAGE(perCat[c] >= 3, c << " has only " << perCat[c]);
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
    FactoryPreset bad{"x.bad", "Bad", "Signature", "d", 1, {{"delay", {{"no_such_param", 1.0f}}}}};
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
        CHECK(session.selected() == fxTypeIndex("delay"));  // staged draft untouched
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
