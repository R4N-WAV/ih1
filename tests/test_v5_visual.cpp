// V5 Phase 1: FX-tree framing (no cut by the panel), commit continuity (no reset, no pop), stable accumulation.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "ih1/visual/FieldVolume.h"
#include "ih1/visual/Formation.h"

using namespace ih1::visual;

namespace {
FrameInput chainOf(int n, float aspect) {
    FrameInput in;
    in.aspect = aspect;
    for (int i = 0; i < n; ++i) {
        InstanceInput ii;
        ii.id = 1000 + static_cast<uint64_t>(i);
        ii.seed = 77u + 13u * static_cast<uint32_t>(i);
        ii.level = 0.1f;
        in.chain.push_back(ii);
    }
    return in;
}
}  // namespace

TEST_CASE("V5 FX tree: the dense medium stays inside the panel at every aspect ratio and chain length (auto-framing)") {
    for (float aspect : {0.9f, 1.2f, 1.6f, 2.2f}) {
        for (int n : {1, 3, 6, 12, 24}) {
            CAPTURE(aspect);
            CAPTURE(n);
            Formation f;
            auto in = chainOf(n, aspect);
            for (int k = 0; k < 600; ++k) f.update(in);  // 10 s at 60 Hz: settled
            for (const auto& ii : in.chain) {
                float x, y, d;
                f.projectPoint(f.nodePosition(ii.id), x, y, d);
                CHECK(x > 0.03f * aspect);
                CHECK(x < 0.97f * aspect);
                CHECK(y > 0.03f);
                CHECK(y < 0.97f);
            }
            CHECK(f.zoom() <= 1.0f);
            CHECK(f.zoom() >= 0.45f);
        }
    }
}

TEST_CASE("V5 FX tree: committing more effects never resets or pops the field (identity kept, motion bounded per frame)") {
    Formation f;
    auto in = chainOf(4, 1.4f);
    for (int k = 0; k < 300; ++k) f.update(in);
    // the existing instances keep their identity; positions move only by gliding
    std::vector<std::pair<float, float>> prev;
    auto snap = [&](const FrameInput& fi) {
        std::vector<std::pair<float, float>> v;
        for (int i = 0; i < 4; ++i) {
            float x, y, d;
            f.projectPoint(f.nodePosition(fi.chain[static_cast<size_t>(i)].id), x, y, d);
            v.emplace_back(x, y);
        }
        return v;
    };
    prev = snap(in);
    const float z0 = f.zoom();
    // commit three more effects in quick succession
    for (int add = 0; add < 3; ++add) {
        InstanceInput ii;
        ii.id = 5000 + static_cast<uint64_t>(add);
        ii.seed = 991u + static_cast<uint32_t>(add);
        in.chain.push_back(ii);
        TransferRequest tr;
        tr.id = ii.id;
        in.transfers = {tr};
        float worst = 0.0f, worstZoom = 0.0f, lastZoom = f.zoom();
        for (int k = 0; k < 120; ++k) {
            f.update(in);
            in.transfers.clear();
            const auto now = snap(in);
            for (size_t i = 0; i < now.size(); ++i)
                worst = std::max(worst, std::hypot(now[i].first - prev[i].first, now[i].second - prev[i].second));
            worstZoom = std::max(worstZoom, std::fabs(f.zoom() - lastZoom));
            lastZoom = f.zoom();
            prev = now;
        }
        CHECK(worst < 0.012f);      // < 1.2 % of the panel height per frame: a glide, never a jump
        CHECK(worstZoom < 0.006f);  // the camera eases back, it does not cut
    }
    CHECK(f.zoom() <= z0 + 1e-4f);
}

TEST_CASE("V5 FX tree: adaptive accumulation cuts the frame-to-frame noise of a calm field") {
    // CPU reference of the GL accumulation: the same jittered volume, accumulated with the V4 fixed weight (22 %) and
    // with the V5 calm weight (9 %); the temporal noise is the RMS frame-to-frame change once settled.
    Formation f;
    auto in = chainOf(5, 1.5f);
    for (int k = 0; k < 120; ++k) f.update(in);
    FieldScene sc = f.scene();
    const int w = 96, h = 64;
    std::vector<float> a4(static_cast<size_t>(w * h * 3), 0.0f), a5 = a4, prev4, prev5, lin;
    double n4 = 0, n5 = 0, lum = 0;
    for (int fr = 0; fr < 70; ++fr) {
        renderVolume(sc, w, h, lin, 24, 1, fr);  // a static scene: only the jitter changes
        for (size_t i = 0; i < lin.size(); ++i) {
            a4[i] = fr == 0 ? lin[i] : a4[i] + 0.22f * (lin[i] - a4[i]);
            a5[i] = fr == 0 ? lin[i] : a5[i] + 0.09f * (lin[i] - a5[i]);
        }
        if (fr >= 40) {
            for (size_t i = 0; i < lin.size(); ++i) {
                n4 += (a4[i] - prev4[i]) * (a4[i] - prev4[i]);
                n5 += (a5[i] - prev5[i]) * (a5[i] - prev5[i]);
                lum += a5[i] * a5[i];
            }
        }
        prev4 = a4;
        prev5 = a5;
    }
    MESSAGE("frame-to-frame noise V4 " << 10 * std::log10(n4 / lum) << " dB, V5 calm " << 10 * std::log10(n5 / lum) << " dB (relative to the image)");
    CHECK(n5 < 0.45 * n4);
}
