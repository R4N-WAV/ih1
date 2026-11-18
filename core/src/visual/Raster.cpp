#include "ih1/visual/Raster.h"

#include <algorithm>
#include <cmath>

namespace ih1::visual {

void Canvas::resize(int w, int h) {
    w_ = std::clamp(w, 1, 8192);
    h_ = std::clamp(h, 1, 8192);
    acc_.assign(static_cast<size_t>(w_) * static_cast<size_t>(h_) * 3, 0.0f);
}

void Canvas::clear() { std::fill(acc_.begin(), acc_.end(), 0.0f); }

void Canvas::decay(float d) {
    d = std::isfinite(d) ? std::clamp(d, 0.0f, 1.0f) : 0.0f;
    for (auto& v : acc_) v *= d;
}

void Canvas::draw(const std::vector<Segment>& segs, float scale, float gain) {
    for (const auto& s : segs) line(s, scale, gain);
}

void Canvas::line(const Segment& s, float scale, float gain) {
    const float x0 = s.x0 * scale, y0 = s.y0 * scale, x1 = s.x1 * scale, y1 = s.y1 * scale;
    if (!std::isfinite(x0 + y0 + x1 + y1)) return;
    const float dx = x1 - x0, dy = y1 - y0;
    const float len = std::max(std::fabs(dx), std::fabs(dy));
    if (len > 4.0f * static_cast<float>(std::max(w_, h_))) return;
    const int steps = std::max(1, static_cast<int>(std::ceil(len)));
    // one pixel of intensity per pixel of length, like a 1-px GL line
    const float r = s.r * gain, g = s.g * gain, b = s.b * gain;
    for (int k = 0; k <= steps; ++k) {
        if (k == steps && steps > 1) break;  // shared endpoint drawn by the next segment
        const float t = static_cast<float>(k) / static_cast<float>(steps);
        const float x = x0 + dx * t, y = y0 + dy * t;
        const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
        const float fx = x - static_cast<float>(ix), fy = y - static_cast<float>(iy);
        const float wts[4] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
        const int ox[4] = {0, 1, 0, 1}, oy[4] = {0, 0, 1, 1};
        for (int q = 0; q < 4; ++q) {
            const int px = ix + ox[q], py = iy + oy[q];
            if (px < 0 || py < 0 || px >= w_ || py >= h_) continue;
            float* p = &acc_[static_cast<size_t>((py * w_ + px) * 3)];
            p[0] += r * wts[q];
            p[1] += g * wts[q];
            p[2] += b * wts[q];
        }
    }
}

namespace {
void boxBlur(std::vector<float>& v, std::vector<float>& t, int w, int h, int r) {
    // separable running-sum box blur, edges clamped
    t.resize(v.size());
    const float inv = 1.0f / static_cast<float>(2 * r + 1);
    auto H = [&](int x, int y, int c) { return v[static_cast<size_t>((y * w + std::clamp(x, 0, w - 1)) * 3 + c)]; };
    for (int y = 0; y < h; ++y)
        for (int c = 0; c < 3; ++c) {
            float acc = 0;
            for (int k = -r; k <= r; ++k) acc += H(k, y, c);
            for (int x = 0; x < w; ++x) {
                t[static_cast<size_t>((y * w + x) * 3 + c)] = acc * inv;
                acc += H(x + r + 1, y, c) - H(x - r, y, c);
            }
        }
    auto V = [&](int x, int y, int c) { return t[static_cast<size_t>((std::clamp(y, 0, h - 1) * w + x) * 3 + c)]; };
    for (int x = 0; x < w; ++x)
        for (int c = 0; c < 3; ++c) {
            float acc = 0;
            for (int k = -r; k <= r; ++k) acc += V(x, k, c);
            for (int y = 0; y < h; ++y) {
                v[static_cast<size_t>((y * w + x) * 3 + c)] = acc * inv;
                acc += V(x, y + r + 1, c) - V(x, y - r, c);
            }
        }
}
}  // namespace

void Canvas::composite(std::vector<uint8_t>& rgb, float exposure, float bloomGain) const {
    // bloom at quarter resolution: downsample, blur twice, sample back bilinearly
    const int bw = std::max(1, w_ / 4), bh = std::max(1, h_ / 4);
    bloom_.assign(static_cast<size_t>(bw * bh * 3), 0.0f);
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
            const int bx = std::min(bw - 1, x / 4), by = std::min(bh - 1, y / 4);
            const float* a = &acc_[static_cast<size_t>((y * w_ + x) * 3)];
            float* b = &bloom_[static_cast<size_t>((by * bw + bx) * 3)];
            b[0] += a[0] * (1.0f / 16.0f); b[1] += a[1] * (1.0f / 16.0f); b[2] += a[2] * (1.0f / 16.0f);
        }
    for (int pass = 0; pass < 2; ++pass) boxBlur(bloom_, tmp_, bw, bh, 3);
    rgb.resize(static_cast<size_t>(w_ * h_ * 3));
    for (int y = 0; y < h_; ++y) {
        const float fy = std::clamp((static_cast<float>(y) + 0.5f) / 4.0f - 0.5f, 0.0f, static_cast<float>(bh - 1));
        const int y0 = static_cast<int>(fy), y1 = std::min(bh - 1, y0 + 1);
        const float ty = fy - static_cast<float>(y0);
        for (int x = 0; x < w_; ++x) {
            const float fx = std::clamp((static_cast<float>(x) + 0.5f) / 4.0f - 0.5f, 0.0f, static_cast<float>(bw - 1));
            const int x0 = static_cast<int>(fx), x1 = std::min(bw - 1, x0 + 1);
            const float tx = fx - static_cast<float>(x0);
            const float* a = &acc_[static_cast<size_t>((y * w_ + x) * 3)];
            float c[3];
            for (int k = 0; k < 3; ++k) {
                auto B = [&](int xx, int yy) { return bloom_[static_cast<size_t>((yy * bw + xx) * 3 + k)]; };
                const float bl = (B(x0, y0) * (1 - tx) + B(x1, y0) * tx) * (1 - ty) + (B(x0, y1) * (1 - tx) + B(x1, y1) * tx) * ty;
                c[k] = a[k] + bloomGain * bl;
            }
            const Rgb o = toneMap({c[0], c[1], c[2]}, exposure);
            uint8_t* d = &rgb[static_cast<size_t>((y * w_ + x) * 3)];
            d[0] = static_cast<uint8_t>(std::clamp(o.r, 0.0f, 1.0f) * 255.0f + 0.5f);
            d[1] = static_cast<uint8_t>(std::clamp(o.g, 0.0f, 1.0f) * 255.0f + 0.5f);
            d[2] = static_cast<uint8_t>(std::clamp(o.b, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    }
}

}  // namespace ih1::visual
