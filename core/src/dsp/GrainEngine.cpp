#include "ih1/dsp/GrainEngine.h"

#include <algorithm>
#include <cmath>

namespace ih1::dsp {

namespace {
constexpr double kMinLag = 8.0;  // never read closer than this to the write head
}

void GrainEngine::prepare(double sr, float bufferSeconds, int maxBlock, uint32_t seed) {
    sr_ = sr > 0 ? sr : 48000.0;
    int64_t size = 1;
    while (size < static_cast<int64_t>(bufferSeconds * sr_)) size <<= 1;
    for (auto& b : buf_) b.assign(static_cast<size_t>(size), 0.0f);
    mask_ = size - 1;
    seed_ = seed ? seed : 1u;
    // window tables (index 0..kWinSize inclusive), all exactly 0 at both ends
    for (int w = 0; w < kWindows; ++w) {
        auto& t = win_[static_cast<size_t>(w)];
        t.assign(kWinSize + 1, 0.0f);
        for (int i = 0; i <= kWinSize; ++i) {
            const float x = static_cast<float>(i) / kWinSize;
            float v = 0.0f;
            switch (w) {
                case 1: {  // Gaussian (sigma 0.16), edge value removed so it starts/ends at exactly 0
                    const float e = std::exp(-0.5f * (0.5f / 0.16f) * (0.5f / 0.16f));
                    const float g = std::exp(-0.5f * ((x - 0.5f) / 0.16f) * ((x - 0.5f) / 0.16f));
                    v = (g - e) / (1.0f - e);
                    break;
                }
                case 2:  // Tukey (25 % cosine tapers): fuller body
                    v = x < 0.125f ? 0.5f - 0.5f * std::cos(kPi * x / 0.125f)
                                   : (x > 0.875f ? 0.5f - 0.5f * std::cos(kPi * (1.0f - x) / 0.125f) : 1.0f);
                    break;
                case 3:  // Pluck: 3 % raised-cosine attack, exponential decay, short release to 0
                    v = (x < 0.03f ? 0.5f - 0.5f * std::cos(kPi * x / 0.03f) : std::exp(-4.0f * (x - 0.03f))) *
                        (x > 0.9f ? 0.5f + 0.5f * std::cos(kPi * (x - 0.9f) / 0.1f) : 1.0f);
                    break;
                case 4:  // Swell: the pluck reversed (reverse-envelope bloom)
                {
                    const float y = 1.0f - x;
                    v = (y < 0.03f ? 0.5f - 0.5f * std::cos(kPi * y / 0.03f) : std::exp(-4.0f * (y - 0.03f))) *
                        (y > 0.9f ? 0.5f + 0.5f * std::cos(kPi * (y - 0.9f) / 0.1f) : 1.0f);
                    break;
                }
                default:  // Hann
                    v = 0.5f - 0.5f * std::cos(kTwoPi * x);
            }
            t[static_cast<size_t>(i)] = std::clamp(v, 0.0f, 1.0f);
        }
        t[0] = 0.0f;
        t[kWinSize] = 0.0f;
    }
    for (auto& d : delay_) d.prepare(static_cast<int>(2.05 * sr_) + maxBlock);
    energyA_ = 1.0f - std::exp(-1.0f / (0.03f * static_cast<float>(sr_)));
    coherA_ = 1.0f - std::exp(-1.0f / (0.12f * static_cast<float>(sr_)));
    const double span = std::min(static_cast<double>(size) / sr_, 8.0);
    binLen_ = std::max(1, static_cast<int>(span * sr_ / 96.0));
    disp_.spanSeconds.store(static_cast<float>(span));
    lastHp_ = lastLp_ = -1.0f;
    fbRate_.prepare(sr_, seed_ ^ 0xFEEDu);
    fbWeightRel_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.08 * sr_)));
    reset();
}

void GrainEngine::reset() {
    for (auto& b : buf_) std::fill(b.begin(), b.end(), 0.0f);
    for (auto& g : grains_) g = Grain{};
    head_ = 0;
    filled_ = 0;
    countdown_[0] = countdown_[1] = 0.0f;
    playhead_ = 0.0;
    loopStart_ = loopLen_ = 0.0;
    lastMode_ = -1;
    lastFreeze_ = false;
    latched_ = false;
    holding_ = false;
    heard_ = 0;
    retrigCount_ = 0.0;
    energy_ = 0.0f;
    sumPow_ = indPow_ = 0.0f;
    spawnOrder_ = 0;
    rng_.seed_(seed_);
    for (auto& d : delay_) d.reset();
    delayCur_ = -1.0f;
    fbRate_.reset();
    fbWeight_ = 0.0f;
    for (int c = 0; c < 2; ++c) {
        fbHp_[c].reset();
        fbLp_[c].reset();
        fbState_[c] = dcX_[c] = dcY_[c] = 0.0f;
    }
    binCount_ = binIndex_ = 0;
    binPeak_ = 0.0f;
    for (auto& p : disp_.peaks) p.store(0.0f, std::memory_order_relaxed);
    for (auto& g : disp_.grainPos) g.store(-1.0f, std::memory_order_relaxed);
    disp_.playhead.store(-1.0f, std::memory_order_relaxed);
    disp_.active.store(0, std::memory_order_relaxed);
}

int GrainEngine::activeVoices() const {
    int n = 0;
    for (const auto& g : grains_) n += g.active ? 1 : 0;
    return n;
}

float GrainEngine::readCh(int c, double pos) const {
    const auto& b = buf_[static_cast<size_t>(c)];
    const double fl = std::floor(pos);
    const int64_t i = static_cast<int64_t>(fl);
    const float f = static_cast<float>(pos - fl);
    const float y0 = b[static_cast<size_t>((i - 1) & mask_)], y1 = b[static_cast<size_t>(i & mask_)];
    const float y2 = b[static_cast<size_t>((i + 1) & mask_)], y3 = b[static_cast<size_t>((i + 2) & mask_)];
    // 4-point, 3rd-order Hermite
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

float GrainEngine::quantise(float st, int scale) const {
    static constexpr int kMajor[7] = {0, 2, 4, 5, 7, 9, 11};
    static constexpr int kMinor[7] = {0, 2, 3, 5, 7, 8, 10};
    static constexpr int kPenta[5] = {0, 3, 5, 7, 10};
    static constexpr int kFifths[2] = {0, 7};
    if (scale == ScaleOff) return st;
    if (scale == Chromatic) return std::round(st);
    if (scale == Octaves) return 12.0f * std::round(st / 12.0f);
    const int* deg = kMajor;
    int nd = 7;
    if (scale == Minor) deg = kMinor;
    else if (scale == Pentatonic) { deg = kPenta; nd = 5; }
    else if (scale == Fifths) { deg = kFifths; nd = 2; }
    float best = st, bestD = 1e9f;
    const int oct = static_cast<int>(std::floor(st / 12.0f));
    for (int o = oct - 1; o <= oct + 1; ++o)
        for (int k = 0; k < nd; ++k) {
            const float c = static_cast<float>(o * 12 + deg[k]);
            const float d = std::fabs(c - st);
            if (d < bestD) { bestD = d; best = c; }
        }
    return best;
}

double GrainEngine::alignStart(double start, const Grain& ref, float len) const {
    // WSOLA-style: find the offset d in [-R, R] where the candidate continues the reference grain's waveform
    // best (normalised cross-correlation on the mono mix). Coarse search every 4 samples, then refine +-3.
    // Bounded cost: <= 64 reference points x (129 + 7) offsets per spawn.
    const int R = std::clamp(static_cast<int>(len * 0.25f), 16, 256);
    const int L = std::clamp(static_cast<int>(len * 0.5f), 32, 256);
    constexpr int kStep = 4;
    float refv[64];
    const int m = std::min(64, L / kStep);
    float xx = 1e-9f;
    for (int k = 0; k < m; ++k) {
        refv[k] = readMono(ref.pos + ref.inc * (k * kStep));
        xx += refv[k] * refv[k];
    }
    auto score = [&](int d) {
        float xy = 0.0f, yy = 1e-9f;
        for (int k = 0; k < m; ++k) {
            const float y = readMono(start + d + k * kStep);
            xy += refv[k] * y;
            yy += y * y;
        }
        return xy / std::sqrt(xx * yy);
    };
    float bestScore = -2.0f;
    int best = 0;
    for (int d = -R; d <= R; d += kStep) {
        const float sc = score(d);
        if (sc > bestScore) { bestScore = sc; best = d; }
    }
    const int c = best;
    for (int d = c - 3; d <= c + 3; ++d) {
        if (d == c) continue;
        const float sc = score(d);
        if (sc > bestScore) { bestScore = sc; best = d; }
    }
    return start + best;
}

void GrainEngine::spawn(const Params& p, int chan, bool forced) {
    int active = 0;
    Grain* g = nullptr;
    const Grain* newest = nullptr;
    for (auto& x : grains_) {
        if (x.active) {
            ++active;
            if (x.inc > 0.0 && (!newest || x.order > newest->order)) newest = &x;
        } else if (!g) {
            g = &x;
        }
    }
    const int limit = std::clamp(p.voices, 1, kMaxVoices);
    if (!g || (active >= limit && !forced)) return;  // bounded: never steal (stealing would click)
    if (active >= limit) return;

    float len = std::max(0.004f, p.sizeMs * 0.001f) * static_cast<float>(sr_);
    len *= 1.0f + std::clamp(p.sizeJitter, 0.0f, 1.0f) * 0.75f * rng_.bipolar();
    len = std::clamp(len, 32.0f, static_cast<float>(sr_) * 2.0f);

    float st = p.pitchSt + p.pitchJitterSt * rng_.bipolar();
    st = quantise(st, p.scale);
    double ratio = std::exp2(std::clamp(st, -36.0f, 36.0f) / 12.0f);
    const bool scan = p.mode == Stretch || p.mode == Loop;
    if (scan && p.varispeed) ratio *= std::clamp(std::fabs(static_cast<double>(p.speed)), 0.0625, 4.0);
    ratio = std::clamp(ratio, 0.0625, 8.0);
    bool rev = rng_.uniform() < p.reverseProb;
    if (scan && p.varispeed && p.speed < 0.0f) rev = !rev;

    const double head = static_cast<double>(head_);
    const double oldest = head - static_cast<double>(std::min<int64_t>(filled_, mask_ - 64)) + 8.0;
    const double spray = static_cast<double>(p.sprayMs) * 0.001 * sr_ * rng_.uniform();
    double start;
    if (p.mode == Cloud) start = head - static_cast<double>(p.positionMs) * 0.001 * sr_ - spray;
    else start = playhead_ - spray;

    // keep the whole grain inside recorded audio, behind the write head for its whole life
    const double travel = static_cast<double>(len) * ratio;
    // how far the write head moves during the grain's life. V3 fix: Loop holds the head exactly like Freeze; using
    // p.freeze alone let a forward grain at pitch > 1x in Loop run past the held head into stale ring data (a hard
    // discontinuity measured by ih1_grain_clicks).
    const double headGain = holding_ ? 0.0 : static_cast<double>(len);
    // the oldest region is overwritten while the head moves: stay ahead of that front too (V3 fix)
    const double safeOldest = oldest + headGain + kGuardFade;
    if (!rev) {
        const double latest = head - kMinLag - std::max(0.0, travel - headGain);
        start = std::min(start, latest);
        start = std::max(start, safeOldest);
        if (start > latest) return;  // not enough audio captured yet
    } else {
        start = std::min(start, head - kMinLag);
        if (start - travel < safeOldest) start = safeOldest + travel;
        if (start > head - kMinLag) return;
    }
    if (p.align && !rev && newest && (scan || p.freeze) && std::fabs(newest->inc - ratio) < 1e-3)
        start = std::clamp(alignStart(start, *newest, len), oldest, head - kMinLag - std::max(0.0, travel - headGain));

    g->active = true;
    g->pos = start;
    g->inc = rev ? -ratio : ratio;
    g->t = 0.0f;
    g->len = len;
    g->win = std::clamp(p.window, 0, kWindows - 1);
    g->chan = chan;
    g->order = ++spawnOrder_;
    g->fade = 0;
    float pan = std::clamp(p.spread, 0.0f, 1.0f) * rng_.bipolar();
    if (chan == 0) pan = -std::fabs(pan);
    else if (chan == 1) pan = std::fabs(pan);
    panGains(pan, g->gl, g->gr);
    g->gl *= 1.41421356f;
    g->gr *= 1.41421356f;
}

void GrainEngine::process(const float* inL, const float* inR, float* outL, float* outR, int n, const Params& pIn) {
    Params p = pIn;
    p.feedback = std::clamp(p.feedback, 0.0f, 0.95f);
    p.voices = std::clamp(p.voices, 1, kMaxVoices);
    if (!std::isfinite(p.speed)) p.speed = 1.0f;
    p.speed = std::clamp(p.speed, -2.0f, 2.0f);
    const double sr = sr_;
    const double loopLen = std::clamp(static_cast<double>(p.loopMs) * 0.001 * sr, 256.0, static_cast<double>(mask_) * 0.9);
    const bool scan = p.mode == Stretch || p.mode == Loop;
    // Loop holds its segment: writing stops while looping (the captured segment cannot be overwritten).
    // Both Loop and Freeze arm first and latch once enough real signal has been captured.
    const bool wantHold = p.freeze || p.mode == Loop;
    if (!wantHold) latched_ = false;
    const int64_t need = p.mode == Loop ? static_cast<int64_t>(loopLen) : static_cast<int64_t>(0.75 * sr);
    if (wantHold && !latched_ && heard_ >= need) latched_ = true;
    const bool hold = wantHold && latched_;
    holding_ = hold;

    if (p.fbHp != lastHp_ || p.fbLp != lastLp_) {
        for (int c = 0; c < 2; ++c) {
            fbHp_[c].setup(Svf::Highpass, std::clamp(p.fbHp, 10.0f, 4000.0f), 0.6f, 0, sr);
            fbLp_[c].setup(Svf::Lowpass, std::clamp(p.fbLp, 200.0f, 20000.0f), 0.6f, 0, sr);
        }
        lastHp_ = p.fbHp;
        lastLp_ = p.fbLp;
    }

    // mode / freeze transitions set the playhead anchor
    if (p.mode != lastMode_ || hold != lastFreeze_) {
        const double avail = static_cast<double>(std::min<int64_t>(filled_, mask_ - 64));
        if (p.mode == Loop && hold) {
            loopLen_ = std::min(loopLen, std::max(256.0, avail - 64.0));
            loopStart_ = static_cast<double>(head_) - loopLen_ - kMinLag;
            playhead_ = p.speed >= 0.0f ? loopStart_ : loopStart_ + loopLen_;
        } else {
            playhead_ = static_cast<double>(head_) - static_cast<double>(p.positionMs) * 0.001 * sr;
        }
        lastMode_ = p.mode;
        lastFreeze_ = hold;
    }

    RateReducer::Settings fbSet;
    const bool fbRateOn = p.fbRateHz > 0.0f && p.fbRateHz < static_cast<float>(sr) * 0.999f && p.feedback > 0.0f;
    fbSet.rateHz = fbRateOn ? p.fbRateHz : static_cast<float>(sr);
    fbSet.reconstruct = RateReducer::FilterSoft;
    fbSet.bits = fbRateOn ? 12.0f : 24.0f;
    const float fbDrive = dbToGain(std::clamp(p.fbDriveDb, 0.0f, 24.0f));
    const bool fbChar = (fbRateOn || fbDrive > 1.0001f) && p.feedback > 0.0f;
    const float interval = static_cast<float>(sr) / std::clamp(p.density, 0.05f, 1000.0f);
    const float delaySamples = std::clamp(p.delayMs, 0.0f, 2000.0f) * 0.001f * static_cast<float>(sr);
    const bool echo = delaySamples > 1.0f;
    if (delayCur_ < 0.0f) delayCur_ = delaySamples;
    float peakAcc = binPeak_;

    for (int i = 0; i < n; ++i) {
        // ---- playhead ----
        bool forced = false;
        if (p.retrig) {
            retrigCount_ += 1.0;
            if (retrigCount_ >= std::max(64.0, p.retrigSamples)) {
                retrigCount_ = 0.0;
                if (p.mode == Loop && hold) playhead_ = p.speed >= 0.0f ? loopStart_ : loopStart_ + loopLen_;
                else playhead_ = static_cast<double>(head_) - static_cast<double>(p.positionMs) * 0.001 * sr;
                forced = true;
            }
        }
        if (scan) {
            playhead_ += p.speed;
            const double head = static_cast<double>(head_);
            if (p.mode == Loop && hold) {
                if (playhead_ >= loopStart_ + loopLen_) playhead_ -= loopLen_;
                if (playhead_ < loopStart_) playhead_ += loopLen_;
            } else if (hold) {
                // frozen Stretch: scan the frozen buffer as an endless loop
                const double span = std::max(1024.0, static_cast<double>(std::min<int64_t>(filled_, mask_ - 64)) - 2.0 * kMinLag);
                const double lo = head - kMinLag - span;
                if (playhead_ >= head - kMinLag) playhead_ -= span;
                if (playhead_ < lo) playhead_ += span;
            } else {
                // live Stretch: cannot read the future (follows the input at > 1x); slower than 1x it drifts
                // back and, at the end of the buffer, wraps to the Offset point (an implicit retrigger)
                const double lo = head - static_cast<double>(std::min<int64_t>(filled_, mask_ - 64)) + 64.0 + 2.0 * sr;
                if (playhead_ < lo && filled_ > static_cast<int64_t>(4.0 * sr)) playhead_ = head - static_cast<double>(p.positionMs) * 0.001 * sr;
                playhead_ = std::clamp(playhead_, head - static_cast<double>(std::min<int64_t>(filled_, mask_ - 64)) + 64.0, head - kMinLag);
            }
        }
        // ---- scheduler ----
        const int streams = p.independent ? 2 : 1;
        for (int s = 0; s < streams; ++s) {
            countdown_[s] -= 1.0f;
            if (forced || countdown_[s] <= 0.0f) {
                spawn(p, p.independent ? s : -1, forced);
                countdown_[s] += interval * (1.0f + 0.35f * rng_.bipolar());
                if (countdown_[s] < 1.0f) countdown_[s] = interval;
            }
        }
        // ---- grains ----
        float l = 0.0f, r = 0.0f, e = 0.0f, ind = 0.0f, wsum = 0.0f;
        // valid read region relative to the write head, and how fast the head (and the overwrite front) moves
        const double headV = holding_ ? 0.0 : 1.0;
        const double hiRel = -kMinLag;
        const double loRel = -static_cast<double>(std::min<int64_t>(filled_, mask_ - 64)) + 8.0;
        const double headPos = static_cast<double>(head_);
        for (auto& g : grains_) {
            if (!g.active) continue;
            if (g.fade == 0) {
                const double future = (g.pos - headPos) + (g.inc - headV) * kGuardFade;
                const double nowRel = g.pos - headPos;
                if (future > hiRel - 2.0 || future < loRel + 1.0 || nowRel > hiRel || nowRel < loRel) g.fade = kGuardFade;
            }
            const float ph = g.t / g.len;
            const float fi = ph * kWinSize;
            const int ii = std::min(static_cast<int>(fi), kWinSize - 1);
            const auto& w = win_[static_cast<size_t>(g.win)];
            float a = w[static_cast<size_t>(ii)] + (fi - static_cast<float>(ii)) * (w[static_cast<size_t>(ii + 1)] - w[static_cast<size_t>(ii)]);
            if (g.fade > 0) {
                const float f = static_cast<float>(g.fade) / kGuardFade;
                a *= f * f * (3.0f - 2.0f * f);  // smoothstep to 0
                if (--g.fade == 0) g.active = false;
            }
            float gl, gr;
            if (g.chan < 0) {
                gl = a * g.gl * readCh(0, g.pos);
                gr = a * g.gr * readCh(1, g.pos);
            } else {
                const float s = readCh(g.chan, g.pos);
                gl = a * g.gl * s;
                gr = a * g.gr * s;
            }
            l += gl;
            r += gr;
            wsum += a * std::max(std::fabs(g.gl), std::fabs(g.gr));
            ind += gl * gl + gr * gr;
            e += a * a;
            g.pos += g.inc;
            g.t += 1.0f;
            if (g.t >= g.len) { g.active = false; g.fade = 0; }
            if (!g.active) g.fade = 0;
        }
        energy_ += energyA_ * (e - energy_);
        sumPow_ += coherA_ * (l * l + r * r - sumPow_);
        indPow_ += coherA_ * (ind - indPow_);
        const float coher = std::clamp(sumPow_ / std::max(indPow_, 1e-12f), 1.0f, static_cast<float>(kMaxVoices));
        const float norm = 1.0f / std::sqrt(std::max(1.0f, energy_ * coher * (p.independent ? 0.5f : 1.0f)));
        l *= norm;
        r *= norm;
        // V5 Phase 5: |out| <= wsum * norm * max|capture| exactly. The coherence-adaptive norm keeps the output POWER near
        // the input's, so with 95 % feedback the recirculation hardly decayed (measured: a held tail at -21 dBFS,
        // ih1_feedback_audit grain_engine max / rnd3 / rnd4). The fed-back signal is divided by the peak-held weight
        // bound (80 ms release), so the loop's gain is <= Feedback whatever the density, overlap or coherence.
        {
            const float wb = wsum * norm;
            fbWeight_ = wb > fbWeight_ ? wb : fbWeight_ + (wb - fbWeight_) * fbWeightRel_;
        }
        const float fbScale = 1.0f / std::max(1.0f, fbWeight_);
        // ---- grain delay + feedback (resampled into the capture buffer) ----
        float fbIn[2] = {0.0f, 0.0f};
        float wet[2] = {l, r};
        float fbPre[2];
        // V4 Phase 4: the grain-delay tap moves at most 0.5 samples per sample (a short tape-style bend), so a new
        // Grain Delay value never jumps the read point
        {
            const float diff = delaySamples - delayCur_;
            delayCur_ += std::clamp(diff, -0.5f, 0.5f);
            if (delayCur_ < 1.0f) delayCur_ = std::max(delayCur_, 0.0f);
        }
        const bool echoNow = echo || delayCur_ > 1.0f;
        for (int c = 0; c < 2; ++c) {
            delay_[c].push(wet[c]);
            const float d = echoNow ? delay_[c].readLinear(std::max(1.0f, delayCur_)) : wet[c];
            fbPre[c] = fbLp_[c].process(fbHp_[c].process(d * p.feedback * fbScale));
            if (echoNow) wet[c] += d * p.feedback;
        }
        if (fbChar) {
            if (fbRateOn) fbRate_.process(&fbPre[0], &fbPre[1], 1, fbSet);
            // V5: the drive colours the loop without small-signal gain (the old makeup 1 + 0.3 ln(drive) was > 1 inside
            // the recirculation)
            if (fbDrive > 1.0001f) for (float& f : fbPre) f = std::tanh(f * fbDrive) / fbDrive;
        }
        for (int c = 0; c < 2; ++c) {
            float f = fbPre[c];
            // DC blocker + soft limiter: the loop gain can never run away (|tanh| < 1, fb <= 0.95)
            const float y = f - dcX_[c] + 0.995f * dcY_[c];
            dcX_[c] = f;
            dcY_[c] = flushTiny(y);
            f = std::tanh(y);
            fbState_[c] = std::isfinite(f) ? f : 0.0f;
            fbIn[c] = fbState_[c];
        }
        // soft ceiling: transparent below 2.0 (+6 dBFS), approaches 3.0 (+9.5 dBFS)
        for (float& w : wet) {
            const float a = std::fabs(w);
            if (a > 2.0f) w = std::copysign(2.0f + std::tanh(a - 2.0f), w);
        }
        outL[i] = wet[0];
        outR[i] = wet[1];
        // ---- capture (after reading, so a grain never reads the sample being written) ----
        if (!hold) {
            const float xl = inL[i] + fbIn[0], xr = inR[i] + fbIn[1];
            if (std::fabs(inL[i]) + std::fabs(inR[i]) > 0.002f && heard_ < (int64_t{1} << 40)) ++heard_;  // real signal captured
            if (wantHold && !latched_ && heard_ >= need) latched_ = true;  // takes effect from the next block
            buf_[0][static_cast<size_t>(head_ & mask_)] = std::isfinite(xl) ? std::clamp(xl, -8.0f, 8.0f) : 0.0f;
            buf_[1][static_cast<size_t>(head_ & mask_)] = std::isfinite(xr) ? std::clamp(xr, -8.0f, 8.0f) : 0.0f;
            ++head_;
            if (filled_ < mask_) ++filled_;
            peakAcc = std::max(peakAcc, std::max(std::fabs(xl), std::fabs(xr)));
            if (++binCount_ >= binLen_) {
                disp_.peaks[static_cast<size_t>(binIndex_)].store(peakAcc, std::memory_order_relaxed);
                binIndex_ = (binIndex_ + 1) % 96;
                disp_.peakHead.store(binIndex_, std::memory_order_relaxed);
                binCount_ = 0;
                peakAcc = 0.0f;
            }
        } else {
            // frozen: time still passes for grains that were spawned against a moving head
        }
    }
    binPeak_ = peakAcc;
    for (int c = 0; c < 2; ++c) { fbHp_[c].flush(); fbLp_[c].flush(); }
    energy_ = flushTiny(energy_);
    sumPow_ = flushTiny(sumPow_);
    indPow_ = flushTiny(indPow_);
    updateDisplay(p);
}

void GrainEngine::updateDisplay(const Params& p) {
    const double span = static_cast<double>(binLen_) * 96.0;
    const double head = static_cast<double>(head_);
    auto toX = [&](double pos) { return static_cast<float>(1.0 - (head - pos) / span); };
    int k = 0, active = 0;
    for (const auto& g : grains_) {
        if (!g.active) continue;
        ++active;
        if (k < kDisplayGrains) disp_.grainPos[static_cast<size_t>(k++)].store(std::clamp(toX(g.pos), 0.0f, 1.0f), std::memory_order_relaxed);
    }
    for (; k < kDisplayGrains; ++k) disp_.grainPos[static_cast<size_t>(k)].store(-1.0f, std::memory_order_relaxed);
    const bool scan = p.mode == Stretch || p.mode == Loop;
    const double ph = scan ? playhead_ : head - static_cast<double>(p.positionMs) * 0.001 * sr_;
    disp_.playhead.store(std::clamp(toX(ph), 0.0f, 1.0f), std::memory_order_relaxed);
    disp_.active.store(active, std::memory_order_relaxed);
    disp_.frozen.store(p.freeze || p.mode == Loop, std::memory_order_relaxed);
}

}  // namespace ih1::dsp
