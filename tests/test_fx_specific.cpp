// Behavioural tests per FX: each checks the effect's sonic contract, not just that it runs.
#include <doctest/doctest.h>

#include <cmath>
#include <vector>

#include "ih1/dsp/Basics.h"
#include "ih1/dsp/Fft.h"
#include "ih1/fx/FxProcessor.h"
#include "TestUtil.h"

using namespace ih1;

namespace {

constexpr double kSr = 48000.0;

struct Fx {
    std::unique_ptr<FxProcessor> p;
    const FxDescriptor* d;
    ParamValues v;
    explicit Fx(const char* id) {
        const int t = fxTypeIndex(id);
        REQUIRE(t >= 0);
        d = &fxRegistry()[static_cast<size_t>(t)];
        v = d->defaults();
        p = d->create();
        p->prepare(kSr, 512);
        p->setParams(v, true);
    }
    Fx& set(const char* pid, float val) {
        const int i = d->paramIndex(pid);
        REQUIRE_MESSAGE(i >= 0, pid);
        v[static_cast<size_t>(i)] = d->params[static_cast<size_t>(i)].clamp(val);
        p->setParams(v, true);
        return *this;
    }
    void run(std::vector<float>& L, std::vector<float>& R, double bpm = 120.0, const std::vector<MidiEvent>* midi = nullptr) {
        test::runFx(*p, L, R, 256, kSr, bpm, midi);
    }
};

std::vector<float> sine(double f, size_t n, float a = 0.5f) {
    std::vector<float> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = a * static_cast<float>(std::sin(2 * M_PI * f * static_cast<double>(i) / kSr));
    return v;
}
std::vector<float> harmonicTone(double f, size_t n) {  // sawtooth-like, 20 harmonics
    std::vector<float> v(n, 0.0f);
    for (int h = 1; h <= 20; ++h)
        for (size_t i = 0; i < n; ++i) v[i] += 0.3f / h * static_cast<float>(std::sin(2 * M_PI * f * h * static_cast<double>(i) / kSr));
    return v;
}

// dominant frequency (Hz) in x[from, from+N) with parabolic peak interpolation
double dominantHz(const std::vector<float>& x, size_t from, int N = 16384) {
    dsp::Fft fft;
    fft.prepare(N);
    std::vector<dsp::cfloat> d(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) {
        const float w = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * i / N);
        d[static_cast<size_t>(i)] = dsp::cfloat(x[from + static_cast<size_t>(i)] * w, 0.0f);
    }
    fft.forward(d.data());
    int best = 1;
    for (int k = 2; k < N / 2 - 1; ++k) if (std::abs(d[static_cast<size_t>(k)]) > std::abs(d[static_cast<size_t>(best)])) best = k;
    const double a = std::log(std::abs(d[static_cast<size_t>(best - 1)]) + 1e-12), b = std::log(std::abs(d[static_cast<size_t>(best)]) + 1e-12), c = std::log(std::abs(d[static_cast<size_t>(best + 1)]) + 1e-12);
    const double off = 0.5 * (a - c) / (a - 2 * b + c);
    return (best + off) * kSr / N;
}
double centroidHz(const std::vector<float>& x, size_t from, int N = 8192) {
    dsp::Fft fft;
    fft.prepare(N);
    std::vector<dsp::cfloat> d(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) d[static_cast<size_t>(i)] = dsp::cfloat(x[from + static_cast<size_t>(i)] * (0.5f - 0.5f * std::cos(2.0f * 3.14159265f * i / N)), 0.0f);
    fft.forward(d.data());
    double num = 0, den = 0;
    for (int k = 1; k < N / 2; ++k) { const double m = std::abs(d[static_cast<size_t>(k)]); num += m * k; den += m; }
    return num / den * kSr / N;
}
double rmsDb(const std::vector<float>& x, size_t a, size_t b) { return 20.0 * std::log10(test::rms(x, a, b) + 1e-12); }

}  // namespace

// ------------------------------------------------------------------------------------------ Delay
TEST_CASE("Delay: echo lands exactly on the synced time and feedback decays") {
    Fx fx("delay");
    fx.set("mix", 100).set("mod_depth", 0).set("feedback", 50).set("low_cut", 20).set("high_cut", 20000);
    std::vector<float> L(96000, 0.0f), R(96000, 0.0f);
    L[100] = R[100] = 1.0f;
    fx.run(L, R, 120.0);  // 1/8 dotted at 120 bpm = 0.375 s = 18000 samples
    size_t pk = 0;
    for (size_t i = 200; i < 30000; ++i) if (std::fabs(L[i]) > std::fabs(L[pk])) pk = i;
    CHECK(pk == doctest::Approx(18100).epsilon(0.0003));
    float second = 0;
    for (size_t i = 36000; i < 36300; ++i) second = std::max(second, std::fabs(L[i]));
    CHECK(second < std::fabs(L[pk]));
    CHECK(second > 0.2f * std::fabs(L[pk]));
}

TEST_CASE("Delay vs Tape Echo: changing time keeps pitch (Delay) but bends it (Tape Echo)") {
    auto measure = [](const char* id) {
        Fx fx(id);
        fx.set("sync", 0).set("time_ms", 300).set("mix", 100);
        if (std::string(id) == "delay") fx.set("mod_depth", 0).set("feedback", 0);
        else fx.set("intensity", 0).set("wow", 0).set("flutter", 0).set("motor", 300).set("heads", 0).set("saturation", 0);
        auto L = sine(1000, 48000 * 2), R = L;
        std::vector<float> a(L.begin(), L.begin() + 48000), b(R.begin(), R.begin() + 48000);
        fx.run(a, b);
        fx.set("time_ms", 150);  // speed doubles for the tape
        std::vector<float> c(L.begin() + 48000, L.end()), d2(c);
        fx.p->setParams(fx.v, false);
        fx.run(c, d2);
        return dominantHz(c, 2000, 4096);  // shortly after the change
    };
    const double clean = measure("delay");
    const double tape = measure("tape_echo");
    MESSAGE("after time change: delay " << clean << " Hz, tape " << tape << " Hz");
    CHECK(clean == doctest::Approx(1000).epsilon(0.01));
    CHECK(std::fabs(tape - 1000.0) > 30.0);
}

TEST_CASE("Tape Echo: first head repeats at the repeat rate; heads 1+2+3 make three echoes") {
    Fx fx("tape_echo");
    fx.set("sync", 0).set("time_ms", 200).set("mix", 100).set("wow", 0).set("flutter", 0).set("intensity", 0).set("heads", 6).set("saturation", 0).set("spread", 0);
    std::vector<float> L(48000, 0.0f), R(48000, 0.0f);
    for (int i = 0; i < 48; ++i) L[1000 + static_cast<size_t>(i)] = R[1000 + static_cast<size_t>(i)] = std::sin(3.14159f * i / 48.0f);
    fx.run(L, R);
    auto peakNear = [&](size_t c) { float m = 0; for (size_t i = c - 400; i < c + 400; ++i) m = std::max(m, std::fabs(L[i])); return m; };
    CHECK(peakNear(1000 + 9600) > 0.1f);
    CHECK(peakNear(1000 + 19200) > 0.1f);
    CHECK(peakNear(1000 + 28800) > 0.1f);
    CHECK(peakNear(1000 + 4800) < 0.02f);
}

// ------------------------------------------------------------------------------------------ Reverb
namespace {
double t60FromImpulse(const std::vector<float>& h) {
    // Schroeder backward integration; fit -5 .. -25 dB and extrapolate to -60
    std::vector<double> edc(h.size());
    double acc = 0;
    for (size_t i = h.size(); i-- > 0;) { acc += static_cast<double>(h[i]) * h[i]; edc[i] = acc; }
    const double e0 = edc[0];
    size_t i5 = 0, i25 = 0;
    for (size_t i = 0; i < h.size(); ++i) {
        const double db = 10 * std::log10(edc[i] / e0 + 1e-30);
        if (!i5 && db <= -5) i5 = i;
        if (!i25 && db <= -25) { i25 = i; break; }
    }
    if (!i25) return 1e9;
    return (static_cast<double>(i25 - i5) / kSr) * 3.0;
}
}  // namespace

TEST_CASE("Reverb: measured RT60 follows Decay for FDN and Plate modes") {
    for (int mode : {1, 2, 0}) {  // Hall, Plate, Room
        for (float decay : {1.0f, 3.0f}) {
            CAPTURE(mode);
            CAPTURE(decay);
            Fx fx("reverb");
            fx.set("mode", static_cast<float>(mode)).set("decay", decay).set("mix", 100).set("predelay", 0).set("early", 0).set("low_cut", 20).set("high_cut", 20000).set("damping", 0);
            std::vector<float> L(static_cast<size_t>(kSr * (decay * 2.0 + 1.0)), 0.0f), R(L);
            L[0] = R[0] = 1.0f;
            fx.run(L, R);
            const double t60 = t60FromImpulse(L);
            MESSAGE("mode " << mode << " decay " << decay << " -> measured RT60 " << t60);
            CHECK(t60 == doctest::Approx(decay).epsilon(0.35));
        }
    }
}

TEST_CASE("Reverb: Freeze sustains, Gated cuts the tail") {
    {
        Fx fx("reverb");
        fx.set("mix", 100).set("decay", 2.0f);
        auto L = sine(440, 48000), R = L;
        fx.run(L, R);
        fx.set("freeze", 1);
        fx.p->setParams(fx.v, false);
        std::vector<float> l2(48000 * 4, 0.0f), r2(l2);
        fx.run(l2, r2);
        CHECK(rmsDb(l2, 48000 * 3, 48000 * 4) > rmsDb(l2, 0, 48000) - 6.0);
    }
    {
        Fx fx("reverb");
        fx.set("mode", 5).set("mix", 100).set("decay", 2.0f).set("predelay", 0);
        auto L = sine(440, 24000), R = L;
        L.resize(48000 * 2, 0.0f);
        R.resize(48000 * 2, 0.0f);
        fx.run(L, R);
        CHECK(rmsDb(L, 60000, 96000) < rmsDb(L, 10000, 24000) - 60.0);  // gate holds ~0.25 s after the input falls below -50 dBFS, then closes
    }
}

// ------------------------------------------------------------------------------------------ Reverse
TEST_CASE("Reverse Delay plays a rising swell backwards (falling)") {
    Fx fx("reverse");
    fx.set("mode", 1).set("sync", 0).set("length_ms", 200).set("mix", 100).set("fade", 2).set("spread", 0);
    std::vector<float> L(48000, 0.0f), R;
    for (size_t i = 0; i < 9600; ++i) L[i] = static_cast<float>(i) / 9600.0f * static_cast<float>(std::sin(2 * M_PI * 1000 * static_cast<double>(i) / kSr));
    R = L;
    fx.run(L, R);
    // chunk [0,200ms) plays reversed during [200,400ms): loud at its start, quiet at its end
    const double early = rmsDb(L, 9600 + 500, 9600 + 2500), late = rmsDb(L, 19200 - 2500, 19200 - 500);
    MESSAGE("reversed chunk start " << early << " dB, end " << late << " dB");
    CHECK(early > late + 10.0);
}

TEST_CASE("Reverse Buffer output is continuous for a continuous input") {
    // broadband input: two overlapped reversed readers must not leave level holes (a pure tone can comb)
    Fx fx("reverse");
    fx.set("mode", 0).set("mix", 100);
    dsp::Rng rng(11);
    std::vector<float> L(96000), R;
    for (auto& x : L) x = 0.3f * rng.bipolar();
    R = L;
    fx.run(L, R);
    double mn = 1e9, mx = -1e9;
    for (size_t s = 48000; s + 2400 < 96000; s += 2400)  // after the 2L read window has filled { const double r = rmsDb(L, s, s + 2400); mn = std::min(mn, r); mx = std::max(mx, r); }
    CHECK(mx - mn < 3.0);
}

// ------------------------------------------------------------------------------------------ Harmony / pitch
TEST_CASE("Harmony intervals mode: +7 semitones from 220 Hz gives 329.6 Hz") {
    Fx fx("harmony");
    fx.set("mode", 0).set("voices", 0).set("int1", 7).set("dry", -60).set("detune", 0).set("humanize", 0).set("pan1", 0);
    auto L = harmonicTone(220, 48000 * 2), R = L;
    fx.run(L, R);
    CHECK(dominantHz(L, 48000) == doctest::Approx(329.63).epsilon(0.01));
}

TEST_CASE("Harmony key & scale: A3 in C major, +2 scale steps -> C4 (a minor third)") {
    Fx fx("harmony");
    fx.set("mode", 1).set("key", 0).set("scale", 0).set("voices", 0).set("int1", 2).set("dry", -60).set("detune", 0).set("humanize", 0).set("pan1", 0);
    auto L = harmonicTone(220, 48000 * 2), R = L;
    fx.run(L, R);
    CHECK(dominantHz(L, 48000) == doctest::Approx(261.63).epsilon(0.012));
}

TEST_CASE("Harmony MIDI mode: voices follow held notes; release, sustain and all-notes-off never stick") {
    Fx fx("harmony");
    fx.set("mode", 2).set("voices", 3).set("dry", -60).set("detune", 0).set("humanize", 0).set("pan1", 0);
    const size_t n = 48000 * 4;
    auto L = harmonicTone(220, n), R = L;
    std::vector<MidiEvent> midi = {
        {1000, 0x90, 72, 100},     // C5 on
        {48000, 0x80, 72, 0},      // off at 1 s
        {60000, 0xB0, 64, 127},    // sustain down
        {62000, 0x90, 67, 100},    // G4
        {70000, 0x80, 67, 0},      // released under sustain -> keeps sounding
        {120000, 0xB0, 64, 0},     // sustain up at 2.5 s -> stops
        {130000, 0x90, 60, 100},   // C4
        {150000, 0xB0, 123, 0},    // all notes off
    };
    fx.run(L, R, 120.0, &midi);
    CHECK(dominantHz(L, 20000, 16384) == doctest::Approx(523.25).epsilon(0.012));
    CHECK(rmsDb(L, 55000, 61000) < -50.0);                      // released (window shifted by the 2048-sample latency)
    CHECK(dominantHz(L, 90000, 16384) == doctest::Approx(392.0).epsilon(0.012));  // held by pedal
    CHECK(rmsDb(L, 124500, 127500) < -50.0);                    // pedal up (before the next note-on frame)
    CHECK(rmsDb(L, 158000, 190000) < -50.0);                    // all notes off
}

TEST_CASE("Pitch Shift: +12 st doubles the frequency; Formant Shift moves timbre but not pitch") {
    {
        Fx fx("pitch_shift");
        fx.set("shift_a", 12).set("mix", 100).set("width", 0);
        auto L = harmonicTone(220, 48000 * 2), R = L;
        fx.run(L, R);
        CHECK(dominantHz(L, 48000) == doctest::Approx(440.0).epsilon(0.01));
    }
    {
        Fx up("formant"), down("formant");
        up.set("shift", 6).set("mix", 100);
        down.set("shift", -6).set("mix", 100);
        auto a = harmonicTone(220, 48000 * 2), b = a, c = a, d = a;
        up.run(a, b);
        down.run(c, d);
        CHECK(dominantHz(a, 48000) == doctest::Approx(220.0).epsilon(0.01));
        CHECK(dominantHz(c, 48000) == doctest::Approx(220.0).epsilon(0.01));
        const double cu = centroidHz(a, 48000), cd = centroidHz(c, 48000);
        MESSAGE("centroid up " << cu << " Hz, down " << cd << " Hz");
        CHECK(cu > cd * 1.3);
    }
}

// ------------------------------------------------------------------------------------------ Modulation
TEST_CASE("Doubler decorrelates L/R but stays mono-safe; Chorus spreads a sine into sidebands") {
    {
        Fx fx("doubler");
        fx.set("mix", 50);
        auto L = harmonicTone(220, 48000), R = L;
        const double inDb = rmsDb(L, 24000, 48000);
        fx.run(L, R);
        double corr = 0, ll = 0, rr = 0;
        std::vector<float> mono(L.size());
        for (size_t i = 24000; i < L.size(); ++i) { corr += L[i] * R[i]; ll += L[i] * L[i]; rr += R[i] * R[i]; mono[i] = 0.5f * (L[i] + R[i]); }
        const double c = corr / std::sqrt(ll * rr);
        CHECK(c < 0.98);
        CHECK(rmsDb(mono, 24000, 48000) > inDb - 3.0);
    }
    {
        Fx fx("chorus");
        fx.set("mix", 100).set("depth", 80);
        auto L = sine(1000, 48000 * 2), R = L;
        fx.run(L, R);
        // energy outside +-3 Hz of 1 kHz (FM sidebands) must be significant
        dsp::Fft fft;
        const int N = 32768;
        fft.prepare(N);
        std::vector<dsp::cfloat> d(N);
        for (int i = 0; i < N; ++i) d[static_cast<size_t>(i)] = dsp::cfloat(L[48000 + static_cast<size_t>(i)] * (0.5f - 0.5f * std::cos(6.2831853f * i / N)), 0.0f);
        fft.forward(d.data());
        double in = 0, out = 0;
        for (int k = 1; k < N / 2; ++k) {
            const double hz = k * kSr / N, m = std::norm(d[static_cast<size_t>(k)]);
            (std::fabs(hz - 1000) < 3 ? in : out) += m;
        }
        CHECK(10 * std::log10(out / in) > -20.0);
    }
}

TEST_CASE("Filter: 24 dB low-pass attenuates an octave+ above cutoff; envelope opens on loud input") {
    Fx fx("filter");
    fx.set("type", 1).set("cutoff", 1000).set("resonance", 0).set("mix", 100);
    auto L = sine(4000, 24000), R = L;
    fx.run(L, R);
    CHECK(rmsDb(L, 12000, 24000) < rmsDb(sine(4000, 24000), 12000, 24000) - 40.0);

    auto brightness = [](float level) {
        Fx f("filter");
        f.set("type", 1).set("cutoff", 300).set("mod", 2).set("env_amt", 4).set("mix", 100);
        auto a = harmonicTone(110, 48000);
        for (auto& x : a) x *= level;
        auto b = a;
        f.run(a, b);
        return centroidHz(a, 24000);
    };
    CHECK(brightness(1.0f) > brightness(0.03f) * 1.5);
}

TEST_CASE("EQ: bell boosts by its gain; dynamic band reduces loud material only") {
    Fx fx("eq");
    fx.set("b1_freq", 1000).set("b1_gain", 6).set("b1_q", 1);
    auto L = sine(1000, 48000), R = L;
    fx.run(L, R);
    CHECK(rmsDb(L, 24000, 48000) - rmsDb(sine(1000, 48000), 24000, 48000) == doctest::Approx(6.0).epsilon(0.03));

    auto dynGain = [](float level) {
        Fx f("eq");
        f.set("b2_freq", 3000).set("b2_gain", 0).set("dyn_range", -12).set("dyn_thr", -30);
        auto a = sine(3000, 48000, level), b = a;
        f.run(a, b);
        return rmsDb(a, 24000, 48000) - rmsDb(sine(3000, 48000, level), 24000, 48000);
    };
    CHECK(dynGain(0.5f) < -8.0);
    CHECK(std::fabs(dynGain(0.005f)) < 0.5);
}

TEST_CASE("Saturation: drive adds harmonics, oversampling keeps aliasing low, auto-gain holds level") {
    auto thd = [](float driveDb) {
        Fx f("saturation");
        f.set("style", 2).set("drive", driveDb);
        auto a = sine(1000, 48000), b = a;
        f.run(a, b);
        return test::largestAliasDbc(a, kSr, 99999.0);  // with f0 outside: reports the largest non-DC component vs ... (unused)
    };
    (void)thd;
    Fx f("saturation");
    f.set("style", 3).set("drive", 24).set("auto_gain", 1);
    auto a = sine(4999, 48000 * 2), b = a;
    const double inDb = rmsDb(a, 48000, 96000);
    f.run(a, b);
    const float alias = test::largestAliasDbc(a, kSr, 4999.0);
    MESSAGE("hard clip +24 dB @4999 Hz: largest alias " << alias << " dBc");
    CHECK(alias < -45.0f);
    CHECK(std::fabs(rmsDb(a, 48000, 96000) - inDb) < 2.0);
}

// ------------------------------------------------------------------------------------------ Texture
TEST_CASE("Granular and Spectral Freeze keep sounding after the input stops when frozen") {
    for (const char* id : {"granular", "spectral"}) {
        CAPTURE(id);
        Fx fx(id);
        fx.set("mix", 100).set("freeze", 1);
        auto L = harmonicTone(220, 48000), R = L;
        L.resize(48000 * 4, 0.0f);
        R.resize(48000 * 4, 0.0f);
        fx.run(L, R);
        CHECK(rmsDb(L, 48000 * 3, 48000 * 4) > -40.0);
    }
}

TEST_CASE("Stutter repeats the captured slice on the musical grid") {
    Fx fx("stutter");
    fx.set("interval", 2).set("offset", 0).set("gate", 5).set("slice", 2).set("chance", 100).set("mix", 100);
    // host playing from bar start at 120 bpm: 1/16 slice = 6000 samples
    const size_t n = 96000;
    dsp::Rng r(5);
    std::vector<float> L(n), R;
    for (auto& x : L) x = 0.3f * r.bipolar();
    R = L;
    ProcessContext ctx;
    ctx.sampleRate = kSr;
    ctx.bpm = 120;
    ctx.hostPlaying = true;
    ctx.ppqValid = true;
    for (size_t off = 0; off < n; off += 256) {
        ctx.ppqPosition = static_cast<double>(off) / kSr * 2.0;
        float* ch[2] = {L.data() + off, R.data() + off};
        fx.p->process(ch, 256, ctx);
    }
    // slice k (k>=1) should replay slice 0 of the input (noise is unique, so correlation proves capture)
    double c = 0, a = 0, b = 0;
    for (size_t i = 500; i < 5500; ++i) { c += L[6000 + i] * L[12000 + i]; a += L[6000 + i] * L[6000 + i]; b += L[12000 + i] * L[12000 + i]; }
    CHECK(c / std::sqrt(a * b) > 0.95);
}

TEST_CASE("Stereo Width: 0% is mono; Spread widens a mono source with an untouched mono sum") {
    {
        Fx fx("width");
        fx.set("width", 0);
        auto L = sine(500, 4800), R = sine(700, 4800);
        fx.run(L, R);
        for (size_t i = 0; i < L.size(); ++i) REQUIRE(L[i] == doctest::Approx(R[i]));
    }
    {
        Fx fx("width");
        fx.set("spread", 100);
        auto L = harmonicTone(220, 24000), R = L, in = L;
        fx.run(L, R);
        double diff = 0, monoErr = 0;
        for (size_t i = 0; i < L.size(); ++i) {
            diff += std::fabs(L[i] - R[i]);
            monoErr = std::max(monoErr, static_cast<double>(std::fabs(0.5f * (L[i] + R[i]) - in[i])));
        }
        CHECK(diff > 100.0);
        CHECK(monoErr < 1e-5);
    }
}
