// Offline CPU renderer for the formation (same simulation as the plug-in; used to compare against the
// reference video). Usage: formation_preview <outDir> <numInstances> [seconds=6] [width=1280] [height=800] [solo=0]
// Writes PPM frames (every 0.5 s) to outDir.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "ih1/visual/Formation.h"

using namespace ih1::visual;

namespace {

struct Canvas {
    int w, h;
    std::vector<float> r, g, b;
    Canvas(int W, int H) : w(W), h(H), r(static_cast<size_t>(W * H), 0.0f), g(r), b(r) {}
    void decay(float d) { for (size_t i = 0; i < r.size(); ++i) { r[i] *= d; g[i] *= d; b[i] *= d; } }
    void plot(int x, int y, float a, const Segment& s) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        const size_t i = static_cast<size_t>(y * w + x);
        r[i] += s.r * a; g[i] += s.g * a; b[i] += s.b * a;
    }
    // anti-aliased additive line (Wu-style, constant intensity per unit length)
    void line(const Segment& s, float scale) {
        float x0 = s.x0 * scale, y0 = s.y0 * scale, x1 = s.x1 * scale, y1 = s.y1 * scale;
        const float dx = x1 - x0, dy = y1 - y0;
        const float len = std::max(std::fabs(dx), std::fabs(dy));
        const int steps = std::max(1, static_cast<int>(std::ceil(len)));
        for (int k = 0; k <= steps; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(steps);
            const float x = x0 + dx * t, y = y0 + dy * t;
            const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
            const float fx = x - ix, fy = y - iy;
            const float wgt = 1.0f / static_cast<float>(steps + 1) * std::max(1.0f, len) ;
            plot(ix, iy, (1 - fx) * (1 - fy) * wgt, s);
            plot(ix + 1, iy, fx * (1 - fy) * wgt, s);
            plot(ix, iy + 1, (1 - fx) * fy * wgt, s);
            plot(ix + 1, iy + 1, fx * fy * wgt, s);
        }
    }
};

void blur(std::vector<float>& v, int w, int h, int r) {
    std::vector<float> t(v.size());
    for (int y = 0; y < h; ++y) {
        float acc = 0;
        for (int x = -r; x < w + r; ++x) {
            if (x + r < w) acc += v[static_cast<size_t>(y * w + std::min(w - 1, x + r))];
            if (x - r - 1 >= 0) acc -= v[static_cast<size_t>(y * w + x - r - 1)];
            if (x >= 0 && x < w) t[static_cast<size_t>(y * w + x)] = acc / (2 * r + 1);
        }
    }
    for (int x = 0; x < w; ++x) {
        float acc = 0;
        for (int y = -r; y < h + r; ++y) {
            if (y + r < h) acc += t[static_cast<size_t>(std::min(h - 1, y + r) * w + x)];
            if (y - r - 1 >= 0) acc -= t[static_cast<size_t>((y - r - 1) * w + x)];
            if (y >= 0 && y < h) v[static_cast<size_t>(y * w + x)] = acc / (2 * r + 1);
        }
    }
}

std::vector<float> g_lum;
void writePpm(const std::string& path, const Canvas& c, float exposure) {
    g_lum.clear();
    // bloom/haze layer
    std::vector<float> br = c.r, bg = c.g, bb = c.b;
    for (int pass = 0; pass < 3; ++pass) { blur(br, c.w, c.h, 6); blur(bg, c.w, c.h, 6); blur(bb, c.w, c.h, 6); }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", c.w, c.h);
    std::vector<unsigned char> row(static_cast<size_t>(c.w) * 3);
    for (int y = 0; y < c.h; ++y) {
        for (int x = 0; x < c.w; ++x) {
            const size_t i = static_cast<size_t>(y * c.w + x);
            // chromatic fringe: red sampled slightly right, blue slightly left
            const size_t ir = static_cast<size_t>(y * c.w + std::min(c.w - 1, x + 1));
            const size_t ib = static_cast<size_t>(y * c.w + std::max(0, x - 1));
            Rgb a{0.8f * c.r[i] + 0.2f * c.r[ir] + 0.9f * br[i], 1.0f * c.g[i] + 0.9f * bg[i], 0.8f * c.b[i] + 0.2f * c.b[ib] + 0.9f * bb[i]};
            const Rgb o = toneMap(a, exposure);
            g_lum.push_back((std::min(1.0f, o.r) + std::min(1.0f, o.g) + std::min(1.0f, o.b)) / 3.0f);
            row[static_cast<size_t>(x) * 3 + 0] = static_cast<unsigned char>(std::clamp(o.r, 0.0f, 1.0f) * 255.0f + 0.5f);
            row[static_cast<size_t>(x) * 3 + 1] = static_cast<unsigned char>(std::clamp(o.g, 0.0f, 1.0f) * 255.0f + 0.5f);
            row[static_cast<size_t>(x) * 3 + 2] = static_cast<unsigned char>(std::clamp(o.b, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
        std::fwrite(row.data(), 1, row.size(), f);
    }
    std::fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: %s outDir numInstances [seconds] [w] [h] [solo]\n", argv[0]); return 1; }
    const std::string out = argv[1];
    const int n = std::atoi(argv[2]);
    const float seconds = argc > 3 ? static_cast<float>(std::atof(argv[3])) : 6.0f;
    const int W = argc > 4 ? std::atoi(argv[4]) : 1280, H = argc > 5 ? std::atoi(argv[5]) : 800;
    const float solo = argc > 6 ? static_cast<float>(std::atof(argv[6])) : 0.0f;
    const float exposure = std::getenv("IH1_EXPOSURE") ? static_cast<float>(std::atof(std::getenv("IH1_EXPOSURE"))) : 1.4f;
    static const char* types[] = {"delay", "reverb", "harmony", "chorus", "granular", "saturation", "tape_echo", "spectral", "filter", "eq", "reverse", "pitch_shift"};
    Formation form(26000);
    Canvas c(W, H);
    FrameInput in;
    in.aspect = static_cast<float>(W) / static_cast<float>(H);
    in.dt = 1.0f / 60.0f;
    for (int i = 0; i < n; ++i) in.chain.push_back({static_cast<uint64_t>(1000 + i), hueForType(types[i % 12]), static_cast<uint32_t>(7919 * (i + 3)), 0.0f});
    const int frames = static_cast<int>(seconds * 60.0f);
    int saved = 0;
    for (int fr = 0; fr < frames; ++fr) {
        const float t = static_cast<float>(fr) / 60.0f;
        // synthetic music: a phrase envelope with slow swells
        const float lv = 0.04f + 0.03f * std::sin(t * 1.3f) + 0.02f * std::sin(t * 4.1f);
        in.outLevel = lv;
        in.low = lv * 0.6f;
        in.mid = lv * 0.3f;
        in.high = lv * 0.1f;
        for (size_t i = 0; i < in.chain.size(); ++i) in.chain[i].level = lv * (0.8f + 0.4f * std::sin(t * 0.7f + static_cast<float>(i)));
        in.solo = solo;
        form.update(in);
        c.decay(form.decay(in.dt));
        const float scale = static_cast<float>(H);
        for (const auto& s : form.segments()) c.line(s, scale);
        if (fr % 30 == 29 && std::getenv("IH1_STATS")) {
            std::vector<float> l(c.r.size());
            for (size_t i = 0; i < l.size(); ++i) l[i] = 0.2126f * c.r[i] + 0.7152f * c.g[i] + 0.0722f * c.b[i];
            std::vector<float> s2 = l;
            std::sort(s2.begin(), s2.end());
            std::printf("t=%.1f accum lum: p50 %.4f p90 %.4f p99 %.4f p999 %.4f max %.4f\n", t, s2[s2.size() / 2], s2[s2.size() * 9 / 10], s2[s2.size() * 99 / 100], s2[s2.size() * 999 / 1000], s2.back());
        }
        if (fr % 30 == 29) {
            char name[64];
            std::snprintf(name, sizeof name, "/f%03d.ppm", saved++);
            writePpm(out + name, c, exposure);
            if (std::getenv("IH1_STATS")) {
                auto v = g_lum;
                std::sort(v.begin(), v.end());
                double mean = 0; size_t dark = 0;
                for (float x : v) { mean += x; dark += x < 0.05f; }
                std::printf("  out: mean %.3f median %.3f p90 %.3f p99 %.3f dark<0.05 %.2f\n", mean / v.size(), v[v.size() / 2], v[v.size() * 9 / 10], v[v.size() * 99 / 100], double(dark) / v.size());
            }
        }
    }
    std::printf("instances %d, live particles %d\n", n, form.liveParticles());
    return 0;
}
