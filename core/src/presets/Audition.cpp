#include "ih1/presets/Audition.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ih1/dsp/Basics.h"
#include "ih1/dsp/Fft.h"
#include "ih1/engine/Engine.h"
#include "ih1/model/Session.h"

namespace ih1::audition {

namespace {
constexpr double kPi2 = 2.0 * 3.14159265358979323846;

struct Res2 {  // a decaying resonator (formants, bodies)
    double s = 0, c = 0;
    float run(double f, double r, float in) {
        const double w = kPi2 * f / kRate;
        const double ns = r * (s * std::cos(w) + c * std::sin(w)) + in;
        c = r * (c * std::cos(w) - s * std::sin(w));
        s = ns;
        return static_cast<float>(s);
    }
};

float saw(double ph, int harmonics) {  // band-limited saw from a phase in cycles
    float v = 0.0f;
    for (int h = 1; h <= harmonics; ++h) v += static_cast<float>(std::sin(kPi2 * ph * h) / h);
    return v * 0.55f;
}

void normalise(std::vector<float>& x) {
    double sq = 0;
    for (float v : x) sq += static_cast<double>(v) * v;
    const double rms = std::sqrt(std::max(sq / static_cast<double>(x.size()), 1e-20));
    float g = static_cast<float>(std::pow(10.0, -18.0 / 20.0) / rms);
    float pk = 0.0f;
    for (float v : x) pk = std::max(pk, std::fabs(v));
    if (pk * g > 0.89f) g = 0.89f / pk;  // peaky sources (plucks, hits) are capped at -1 dBFS instead
    for (auto& v : x) v *= g;
}
}  // namespace

const char* sourceName(int s) {
    static const char* n[kNumSources] = {"vocal phrase", "sustained vocal", "drum loop", "snare hits", "bass phrase",
                                         "guitar chord", "piano / keys", "synth pad", "field recording"};
    return s >= 0 && s < kNumSources ? n[s] : "?";
}

const char* className(Class c) {
    switch (c) {
        case Class::Transformative: return "Transformative";
        case Class::Moderate: return "Moderate";
        default: return "Subtle";
    }
}

std::vector<float> source(int s, double seconds) {
    const int n = static_cast<int>(seconds * kRate);
    std::vector<float> x(static_cast<size_t>(n), 0.0f);
    dsp::Rng rng;
    rng.seed_(1234u + static_cast<uint32_t>(s));
    Res2 r1, r2, r3, body;
    Res2 rs[6], rs2[6];  // guitar string bodies (local: every call is identical)
    double ph = 0, ph2 = 0, ph3 = 0, env = 0, lp = 0, lp2 = 0;
    for (int i = 0; i < n; ++i) {
        const double t = i / kRate;
        float v = 0.0f;
        switch (s) {
            case 0: {  // vocal phrase: glottal pulses with a pitch contour, moving formants, consonant noise
                const double f0 = 150.0 + 45.0 * std::sin(kPi2 * 0.35 * t) + 25.0 * std::sin(kPi2 * 1.3 * t);
                ph += f0 / kRate;
                const float pulse = ph >= 1.0 ? (ph -= 1.0, 1.0f) : 0.0f;
                const double vowel = 0.5 + 0.5 * std::sin(kPi2 * 0.7 * t);
                const double sy = std::fmod(t, 0.9);
                const float voiced = sy < 0.65 ? 1.0f : 0.0f;
                const float cons = (sy > 0.65 && sy < 0.72) ? 0.08f * rng.bipolar() : 0.0f;
                v = voiced * 0.05f * (r1.run(500.0 + 300.0 * vowel, 0.993, pulse) + 0.7f * r2.run(1100.0 + 700.0 * vowel, 0.99, pulse) +
                                      0.3f * r3.run(2600.0, 0.985, pulse)) + cons;
                break;
            }
            case 1: {  // sustained vocal "ah" with vibrato
                const double f0 = 196.0 * (1.0 + 0.012 * std::sin(kPi2 * 5.2 * t));
                ph += f0 / kRate;
                const float pulse = ph >= 1.0 ? (ph -= 1.0, 1.0f) : 0.0f;
                const float a = static_cast<float>(std::min(1.0, t / 0.15));
                v = a * 0.05f * (r1.run(750.0, 0.993, pulse) + 0.7f * r2.run(1200.0, 0.99, pulse) + 0.3f * r3.run(2700.0, 0.985, pulse));
                break;
            }
            case 2: {  // drum loop, 120 BPM sixteenths: kick 1 & 3, snare 2 & 4, hats
                const double step = std::fmod(t, 2.0) / 0.125;
                const int k = static_cast<int>(step);
                const double tt = (step - k) * 0.125;
                if (k % 8 == 0) v += 0.9f * static_cast<float>(std::sin(kPi2 * (45.0 * tt + 40.0 * (1.0 - std::exp(-tt * 30.0)))) * std::exp(-tt * 9.0));
                if (k % 8 == 4) v += static_cast<float>(std::exp(-tt * 22.0)) * (0.5f * rng.bipolar() + 0.4f * static_cast<float>(std::sin(kPi2 * 185.0 * tt)));
                if (k % 2 == 1) { lp += 0.6 * (rng.bipolar() - lp); v += static_cast<float>(std::exp(-tt * 60.0)) * 0.25f * static_cast<float>(rng.bipolar() - lp); }
                break;
            }
            case 3: {  // isolated snare hits every 0.5 s
                const double tt = std::fmod(t, 0.5);
                v = static_cast<float>(std::exp(-tt * 20.0)) * (0.6f * rng.bipolar() + 0.5f * static_cast<float>(std::sin(kPi2 * 190.0 * tt)));
                break;
            }
            case 4: {  // bass phrase: E1 G1 A1 D2, low-passed saw with a pluck envelope
                static const double notes[4] = {41.2, 49.0, 55.0, 73.4};
                const double seg = std::fmod(t, 2.0) / 0.5;
                const double f0 = notes[static_cast<int>(seg) % 4];
                const double tt = (seg - std::floor(seg)) * 0.5;
                ph += f0 / kRate;
                if (ph >= 1.0) ph -= 1.0;
                lp += (0.04 + 0.2 * std::exp(-tt * 8.0)) * (saw(ph, 24) - lp);
                v = static_cast<float>(lp * std::exp(-tt * 1.5));
                break;
            }
            case 5: {  // guitar chord: a strum every second (down, up), sustaining string resonators, alternating chords
                static const double chA[6] = {82.4, 123.5, 164.8, 207.7, 246.9, 329.6}, chB[6] = {110.0, 164.8, 220.0, 277.2, 329.6, 440.0};
                const int bar = static_cast<int>(t);
                const double tt = t - bar;
                const double* strings = (bar / 2) % 2 ? chB : chA;
                float acc = 0.0f;
                for (int k = 0; k < 6; ++k) {
                    const int kk = bar % 2 ? 5 - k : k;  // down / up strum
                    const double at = 0.015 * k;
                    const float in = (tt >= at && tt < at + 0.003) ? 0.5f * rng.bipolar() : 0.0f;
                    acc += rs[kk].run(strings[kk], 0.99993, in) + 0.4f * rs2[kk].run(strings[kk] * 2.0, 0.9998, in);
                }
                v = 0.02f * acc;
                break;
            }
            case 6: {  // piano / keys: inharmonic partials with exponential decay, chords every second
                static const double chords[4][3] = {{261.6, 329.6, 392.0}, {220.0, 261.6, 329.6}, {174.6, 220.0, 261.6}, {196.0, 246.9, 293.7}};
                const int c = static_cast<int>(t) % 4;
                const double tt = t - std::floor(t);
                for (int k = 0; k < 3; ++k)
                    for (int h = 1; h <= 6; ++h) {
                        const double f = chords[c][k] * h * std::sqrt(1.0 + 0.0004 * h * h);
                        v += static_cast<float>(std::sin(kPi2 * f * tt) * std::exp(-tt * (1.5 + 0.8 * h)) / h);
                    }
                v *= 0.15f;
                break;
            }
            case 7: {  // synth pad: three detuned saw stacks, slow attack
                ph += 110.0 / kRate;
                ph2 += 110.0 * 1.0059 / kRate;
                ph3 += 164.8 * 0.997 / kRate;
                ph -= std::floor(ph);
                ph2 -= std::floor(ph2);
                ph3 -= std::floor(ph3);
                const double a = std::min(1.0, t / 0.6);
                lp2 += 0.08 * (saw(ph, 30) + saw(ph2, 30) + 0.8f * saw(ph3, 20) - lp2);
                v = static_cast<float>(a * lp2 * 0.5);
                break;
            }
            default: {  // field recording: wind (modulated low-passed noise) + events (chirps, clicks)
                env = 0.6 + 0.4 * std::sin(kPi2 * 0.23 * t) * std::sin(kPi2 * 0.07 * t + 1.0);
                lp += 0.02 * (rng.bipolar() - lp);
                v = static_cast<float>(env * lp * 3.0);
                const double ev = std::fmod(t, 0.83);
                if (ev < 0.08) v += 0.15f * static_cast<float>(std::sin(kPi2 * (2500.0 + 9000.0 * ev) * ev) * std::exp(-ev * 30.0));
                if (rng.uniform() < 0.0004f) v += 0.6f * rng.bipolar();
                v += 0.02f * rng.bipolar();
                break;
            }
        }
        (void)body;
        x[static_cast<size_t>(i)] = v;
    }
    normalise(x);
    return x;
}

std::vector<int> sourcesForTags(const std::string& tags) {
    std::vector<int> v;
    auto has = [&](const char* k) { return tags.find(k) != std::string::npos; };
    if (has("any") || has("full mix")) for (int s = 0; s < kNumSources; ++s) v.push_back(s);
    if (has("vocal")) { v.push_back(0); v.push_back(1); }
    if (has("drum") || has("loop") || has("perc")) { v.push_back(2); v.push_back(3); }
    if (has("bass")) v.push_back(4);
    if (has("guitar")) v.push_back(5);
    if (has("keys") || has("piano")) v.push_back(6);
    if (has("synth") || has("pad")) v.push_back(7);
    if (has("field") || has("texture") || has("foley")) v.push_back(8);
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    if (v.empty()) v = {0, 2, 7};
    return v;
}

Result run(const std::string& presetJson, int s, double seconds) {
    Result r;
    const auto in = source(s, seconds);
    const int n = static_cast<int>(in.size());
    Engine engine;
    engine.setSafety(0, 0.0f);  // measure the preset, not the output stage
    Session session(engine);
    session.prepare(kRate, 256);
    std::string err;
    if (!session.fromJson(presetJson, false, &err)) { r.finite = false; return r; }
    r.latency = session.latencySamples();
    r.outL.assign(static_cast<size_t>(n), 0.0f);
    r.outR.assign(static_cast<size_t>(n), 0.0f);
    std::vector<float> L(256), R(256);
    for (int off = 0; off < n; off += 256) {
        const int m = std::min(256, n - off);
        for (int i = 0; i < m; ++i) L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = in[static_cast<size_t>(off + i)];
        ProcessContext ctx;
        ctx.sampleRate = kRate;
        ctx.bpm = 120.0;
        ctx.hostPlaying = true;
        ctx.ppqValid = true;
        ctx.ppqPosition = off / kRate * 2.0;
        engine.process(L.data(), R.data(), m, ctx);
        session.service();
        for (int i = 0; i < m; ++i) {
            const float a = L[static_cast<size_t>(i)], b = R[static_cast<size_t>(i)];
            if (!std::isfinite(a) || !std::isfinite(b)) r.finite = false;
            r.outL[static_cast<size_t>(off + i)] = a;
            r.outR[static_cast<size_t>(off + i)] = b;
        }
    }
    // ---- level (after the first second) ----
    const size_t a0 = static_cast<size_t>(kRate);
    double inSq = 0, outSq = 0;
    for (size_t i = a0; i < static_cast<size_t>(n); ++i) {
        inSq += static_cast<double>(in[i]) * in[i];
        const double m = 0.5 * (r.outL[i] + r.outR[i]);
        outSq += 0.5 * (static_cast<double>(r.outL[i]) * r.outL[i] + static_cast<double>(r.outR[i]) * r.outR[i]);
        (void)m;
        r.peak = std::max({r.peak, static_cast<double>(std::fabs(r.outL[i])), static_cast<double>(std::fabs(r.outR[i]))});
    }
    r.gainDb = 10.0 * std::log10(std::max(outSq, 1e-20) / std::max(inSq, 1e-20));
    const double match = std::sqrt(std::max(inSq, 1e-20) / std::max(outSq, 1e-20));
    // ---- correlation (mono, level-matched; FFT cross-correlation over the measured span) ----
    {
        int N = 1;
        const int span = n - static_cast<int>(a0);
        while (N < 2 * span) N <<= 1;
        dsp::Fft fft;
        fft.prepare(N);
        std::vector<dsp::cfloat> X(static_cast<size_t>(N)), Y(static_cast<size_t>(N));
        double ex = 0, ey = 0;
        for (int i = 0; i < span; ++i) {
            const float x = in[a0 + static_cast<size_t>(i)];
            const float y = static_cast<float>(0.5 * (r.outL[a0 + static_cast<size_t>(i)] + r.outR[a0 + static_cast<size_t>(i)]) * match);
            X[static_cast<size_t>(i)] = dsp::cfloat(x, 0.0f);
            Y[static_cast<size_t>(i)] = dsp::cfloat(y, 0.0f);
            ex += static_cast<double>(x) * x;
            ey += static_cast<double>(y) * y;
        }
        fft.forward(X.data());
        fft.forward(Y.data());
        for (int k = 0; k < N; ++k) Y[static_cast<size_t>(k)] *= std::conj(X[static_cast<size_t>(k)]);
        fft.inverse(Y.data());
        const int maxLag = std::min(span - 1, r.latency + static_cast<int>(0.05 * kRate));
        double best = 0.0;
        for (int lag = 0; lag <= maxLag; ++lag) best = std::max(best, static_cast<double>(Y[static_cast<size_t>(lag)].real()) / N);
        r.correlation = best / std::sqrt(std::max(ex * ey, 1e-30));
    }
    // ---- spectral distance: timbre change where the dry signal is active ----
    // 32 log bands, 1024-point frames aligned by the latency. Each frame's band vector is normalised by its own
    // total, so level riding (compression, tremolo, auto gain) does not count; only frames where the input is
    // within 25 dB of its loudest frame are used, and only bands within 30 dB of the frame's loudest input band,
    // so tails filling the gaps between notes do not count either. Mean |dB| difference.
    {
        constexpr int F = 1024, H = 512, kBands = 32;
        dsp::Fft fft;
        fft.prepare(F);
        std::vector<dsp::cfloat> bx(F), by(F);
        const int lat = r.latency;
        struct Frame { double x[kBands], y[kBands], ex; };
        std::vector<Frame> frames;
        double maxEx = 1e-30;
        for (int st = static_cast<int>(a0); st + F + lat < n; st += H) {
            for (int k = 0; k < F; ++k) {
                const double w = 0.5 - 0.5 * std::cos(kPi2 * k / F);
                bx[static_cast<size_t>(k)] = dsp::cfloat(static_cast<float>(in[static_cast<size_t>(st + k)] * w), 0.0f);
                const size_t j = static_cast<size_t>(st + k + lat);
                by[static_cast<size_t>(k)] = dsp::cfloat(static_cast<float>(0.5 * (r.outL[j] + r.outR[j]) * w), 0.0f);
            }
            fft.forward(bx.data());
            fft.forward(by.data());
            Frame fr{};
            double tx = 1e-30, ty = 1e-30;
            for (int b = 0; b < kBands; ++b) {
                const double f0 = 50.0 * std::pow(320.0, static_cast<double>(b) / kBands), f1 = 50.0 * std::pow(320.0, static_cast<double>(b + 1) / kBands);
                const int k0 = std::max(1, static_cast<int>(f0 / kRate * F)), k1 = std::max(k0 + 1, static_cast<int>(f1 / kRate * F));
                double ex = 1e-30, ey = 1e-30;
                for (int k = k0; k < k1 && k < F / 2; ++k) { ex += std::norm(bx[static_cast<size_t>(k)]); ey += std::norm(by[static_cast<size_t>(k)]); }
                fr.x[b] = ex;
                fr.y[b] = ey;
                tx += ex;
                ty += ey;
            }
            for (int b = 0; b < kBands; ++b) { fr.x[b] /= tx; fr.y[b] /= ty; }
            fr.ex = tx;
            maxEx = std::max(maxEx, tx);
            frames.push_back(fr);
        }
        double acc = 0.0;
        int cnt = 0;
        for (const auto& fr : frames) {
            if (fr.ex < maxEx * std::pow(10.0, -2.5)) continue;
            double mx = 0.0;
            for (double v : fr.x) mx = std::max(mx, v);
            for (int b = 0; b < kBands; ++b) {
                if (fr.x[b] < mx * 1e-3) continue;
                acc += std::min(30.0, std::fabs(10.0 * std::log10((fr.y[b] + 1e-12) / (fr.x[b] + 1e-12))));
                ++cnt;
            }
        }
        r.spectralDist = cnt ? acc / cnt : 0.0;
    }
    r.cls = (r.correlation < 0.5 || r.spectralDist > 6.0) ? Class::Transformative
                                                          : ((r.correlation < 0.8 || r.spectralDist > 3.0) ? Class::Moderate : Class::Subtle);
    return r;
}

bool writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, double sr) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t n = static_cast<uint32_t>(std::min(L.size(), R.size()));
    const uint32_t bytes = n * 4;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f);
    w32(36 + bytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16);
    w16(1);
    w16(2);
    w32(static_cast<uint32_t>(sr));
    w32(static_cast<uint32_t>(sr) * 4);
    w16(4);
    w16(16);
    std::fwrite("data", 1, 4, f);
    w32(bytes);
    for (uint32_t i = 0; i < n; ++i) {
        const int16_t a = static_cast<int16_t>(std::lround(std::clamp(L[i], -1.0f, 1.0f) * 32767.0f));
        const int16_t b = static_cast<int16_t>(std::lround(std::clamp(R[i], -1.0f, 1.0f) * 32767.0f));
        std::fwrite(&a, 2, 1, f);
        std::fwrite(&b, 2, 1, f);
    }
    std::fclose(f);
    return true;
}

}  // namespace ih1::audition
