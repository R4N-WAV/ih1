#include "ih1/dsp/OutputSafety.h"

#include "ih1/dsp/Basics.h"

namespace ih1::dsp {

const std::array<float, OutputSafety::kTaps>& OutputSafety::fir() {
    // 4x interpolation low-pass (windowed sinc, cut-off at the base Nyquist, Blackman window); each polyphase
    // branch sums to ~1, so a DC input reads exactly 1.
    static const std::array<float, kTaps> h = [] {
        std::array<float, kTaps> t{};
        const double c = (kTaps - 1) * 0.5;
        for (int k = 0; k < kTaps; ++k) {
            const double x = (k - c) / kPhases;
            const double s = std::fabs(x) < 1e-9 ? 1.0 : std::sin(kPi * x) / (kPi * x);
            const double w = 0.42 - 0.5 * std::cos(2.0 * kPi * k / (kTaps - 1)) + 0.08 * std::cos(4.0 * kPi * k / (kTaps - 1));
            t[static_cast<size_t>(k)] = static_cast<float>(s * w);
        }
        for (int p = 0; p < kPhases; ++p) {
            double sum = 0.0;
            for (int j = p; j < kTaps; j += kPhases) sum += t[static_cast<size_t>(j)];
            for (int j = p; j < kTaps; j += kPhases) t[static_cast<size_t>(j)] = static_cast<float>(t[static_cast<size_t>(j)] / sum);
        }
        return t;
    }();
    return h;
}

void OutputSafety::prepare(double sr, int /*maxBlock*/) {
    sr_ = sr > 0 ? sr : 48000.0;
    lookahead_ = std::clamp(static_cast<int>(std::lround(0.0015 * sr_)), 16, kMaxLookahead - 1);  // 1.5 ms
    dSize_ = 1;
    while (dSize_ < lookahead_ + kDelayFir + 4) dSize_ <<= 1;
    delayL_.assign(static_cast<size_t>(dSize_), 0.0f);
    delayR_.assign(static_cast<size_t>(dSize_), 0.0f);
    gtRing_.assign(static_cast<size_t>(lookahead_ + 1), 1.0f);
    boxRing_.assign(static_cast<size_t>(lookahead_ + 1), 1.0f);
    fadeStep_ = static_cast<float>(1.0 / (0.005 * sr_));
    mode_ = reqMode_;
    reset();
}

void OutputSafety::reset() {
    for (auto& h : hist_) h.fill(0.0f);
    histPos_ = 0;
    std::fill(delayL_.begin(), delayL_.end(), 0.0f);
    std::fill(delayR_.begin(), delayR_.end(), 0.0f);
    std::fill(gtRing_.begin(), gtRing_.end(), 1.0f);
    std::fill(boxRing_.begin(), boxRing_.end(), 1.0f);
    boxSum_ = static_cast<double>(boxRing_.size());
    boxPos_ = 0;
    dPos_ = 0;
    t_ = 0;
    hRel_ = 1.0f;
    gGuard_ = 1.0f;
    ceil_ = std::pow(10.0f, reqCeil_ / 20.0f);
    fade_ = 1.0f;
    fadingOut_ = false;
}

float OutputSafety::detect(int c, float x) {
    auto& h = hist_[static_cast<size_t>(c)];
    constexpr int kLen = kTaps / kPhases;
    // history as a small shift register (8 values): newest at index 0
    for (int j = kLen - 1; j > 0; --j) h[static_cast<size_t>(j)] = h[static_cast<size_t>(j - 1)];
    h[0] = x;
    const auto& f = fir();
    float m = 0.0f;
    for (int p = 0; p < kPhases; ++p) {
        float y = 0.0f;
        for (int j = 0; j < kLen; ++j) y += f[static_cast<size_t>(p + kPhases * j)] * h[static_cast<size_t>(j)];
        m = std::max(m, std::fabs(y));
    }
    return m;
}

float OutputSafety::estimateTruePeak(const float* x, int n) {
    OutputSafety s;
    float m = 0.0f;
    for (int i = 0; i < n + kTaps; ++i) m = std::max(m, s.detect(0, i < n ? x[i] : 0.0f));
    for (int i = 0; i < n; ++i) m = std::max(m, std::fabs(x[i]));
    return m;
}

float OutputSafety::process(float* L, float* R, int n) {
    // ceiling follows its request smoothly (per block, ~20 ms)
    const float tgtCeil = std::pow(10.0f, reqCeil_ / 20.0f);
    ceil_ += std::min(1.0f, static_cast<float>(n) / static_cast<float>(0.02 * sr_)) * (tgtCeil - ceil_);
    relCoef_ = std::exp(-1.0f / (releaseMs_ * 0.001f * static_cast<float>(sr_)));
    const float rel = 1.0f - relCoef_;
    float minG = 1.0f, tp = 0.0f;
    const int LA = lookahead_ + 1;
    const float tpCeil = ceil_ * 0.944f;  // 0.5 dB margin for the 4x detector's residual under-read
    for (int i = 0; i < n; ++i) {
        // mode changes: fade out, switch (state cleared), fade in. Never an abrupt latency jump.
        if (reqMode_ != mode_ && !fadingOut_) fadingOut_ = true;
        if (fadingOut_) {
            fade_ -= fadeStep_;
            if (fade_ <= 0.0f) {
                fade_ = 0.0f;
                const int next = reqMode_;
                reset();
                mode_ = next;
                fade_ = 0.0f;
                fadingOut_ = false;
            }
        } else if (fade_ < 1.0f) {
            fade_ = std::min(1.0f, fade_ + fadeStep_);
        }
        float l = L[i], r = R[i];
        float g = 1.0f;
        if (mode_ == Guard) {
            const float pk = std::max(std::fabs(l), std::fabs(r));
            const float tgt = pk > ceil_ ? ceil_ / pk : 1.0f;
            gGuard_ = tgt < gGuard_ ? tgt : gGuard_ + rel * (1.0f - gGuard_);
            gGuard_ = std::min(gGuard_, tgt);
            if (gGuard_ > 0.999999f) gGuard_ = 1.0f;  // fully released: bit-transparent
            g = gGuard_;
            l *= g;
            r *= g;
        } else if (mode_ == TruePeak) {
            const float pk = std::max(detect(0, l), detect(1, r));
            const float gt = pk > tpCeil ? tpCeil / pk : 1.0f;
            const int slot = static_cast<int>(t_ % LA);
            gtRing_[static_cast<size_t>(slot)] = gt;
            float h = 1.0f;
            for (float v : gtRing_) h = std::min(h, v);
            hRel_ = h < hRel_ ? h : std::min(h, hRel_ + rel * (1.0f - hRel_));
            if (hRel_ > 0.999999f) hRel_ = 1.0f;
            boxSum_ += static_cast<double>(hRel_) - boxRing_[static_cast<size_t>(boxPos_)];
            boxRing_[static_cast<size_t>(boxPos_)] = hRel_;
            boxPos_ = (boxPos_ + 1) % LA;
            if (slot == 0) {  // refresh the running sum exactly once per window: no float drift
                double s = 0.0;
                for (float v : boxRing_) s += v;
                boxSum_ = s;
            }
            g = std::min(1.0f, static_cast<float>(boxSum_ / LA));
            delayL_[static_cast<size_t>(dPos_)] = l;
            delayR_[static_cast<size_t>(dPos_)] = r;
            const int rd = (dPos_ - (LA - 1 + kDelayFir)) & (dSize_ - 1);
            dPos_ = (dPos_ + 1) & (dSize_ - 1);
            l = delayL_[static_cast<size_t>(rd)] * g;
            r = delayR_[static_cast<size_t>(rd)] * g;
            ++t_;
            // backstop: a sample above the ceiling can only come from a release faster than the detector window
            l = std::clamp(l, -ceil_, ceil_);
            r = std::clamp(r, -ceil_, ceil_);
        }
        l *= fade_;
        r *= fade_;
        L[i] = l;
        R[i] = r;
        minG = std::min(minG, g);
        tp = std::max(tp, std::max(std::fabs(l), std::fabs(r)));
    }
    lastTp_ = tp;
    return minG;
}

}  // namespace ih1::dsp
