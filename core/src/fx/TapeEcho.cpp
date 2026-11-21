// Tape Echo — "speed-type" tape loop (KADAL KNW-SA0JJFX4HE, tape equation; KNW-DKQ16F12WF wow/flutter).
// A motor speed v (slewed toward the target) advances the tape position V. Three playback heads sit at
// fixed tape distances; each read finds where the tape now under the head was written (monotone search
// on the stored positions). Changing the time therefore bends the pitch of echoes already on tape,
// exactly like a real tape echo. Saturation (ADAA tanh) and bandwidth loss live inside the loop.
#include <array>

#include "ih1/dsp/Nonlinear.h"
#include "ih1/dsp/Svf.h"
#include "ih1/fx/AllFx.h"
#include "ih1/fx/FxBase.h"

namespace ih1::fx {
namespace {

enum { kSync, kTime, kDiv, kHeads, kIntensity, kSaturation, kWow, kFlutter, kAge, kBass, kTreble, kSpread, kMotor, kMix };

constexpr double kRefSeconds = 0.1;   // tape distance unit (head 1) at speed 1
constexpr double kMaxHead1 = 1.2;     // seconds
constexpr int kHeadMask[7] = {0b001, 0b010, 0b100, 0b011, 0b110, 0b101, 0b111};

class TapeEcho final : public FxBase {
    void onPrepare() override {
        int size = 1;
        const int need = static_cast<int>((kMaxHead1 * 3.0 * 1.15 + 0.2) * sr_);
        while (size < need) size <<= 1;
        mask_ = size - 1;
        for (auto& a : audio_) a.assign(static_cast<size_t>(size), 0.0f);
        pos_.assign(static_cast<size_t>(size), 0.0);
        wow_.prepare(sr_, 3);
        flutter_.prepare(sr_, 5);
        wow_.setShape(dsp::Lfo::Drift);
        flutter_.setShape(dsp::Lfo::Sine);
        mix_.prepare(sr_, maxBlock_, 0, MixLaw::Flat);
    }
    void reset() override {
        for (auto& a : audio_) std::fill(a.begin(), a.end(), 0.0f);
        V_ = 0.0;
        std::fill(pos_.begin(), pos_.end(), -1e30);  // unwritten tape: heads read silence
        w_ = 0;
        for (auto& h : headIdx_) h = 0;
        for (auto& s : sat_) s.reset();
        for (auto& f : loopLp_) f.reset();
        for (auto& f : loopHp_) f.reset();
        for (auto& f : bass_) f.reset();
        for (auto& f : treble_) f.reset();
        mix_.reset();
        speed_ = -1.0;
    }
    void applyParams(bool snap) override {
        const float age = pct(kAge);
        const float bw = 14000.0f * std::pow(0.15f, age);  // 14 kHz .. ~2 kHz
        for (int c = 0; c < 2; ++c) {
            loopLp_[c].setup(dsp::Svf::Lowpass, bw, 0.6f, 0, sr_);
            loopHp_[c].setup(dsp::Svf::Highpass, 60.0f + 120.0f * age, 0.6f, 0, sr_);
            bass_[c].setup(dsp::Svf::LowShelf, 200.0f, 0.7f, fp(kBass), sr_);
            treble_[c].setup(dsp::Svf::HighShelf, 3500.0f, 0.7f, fp(kTreble), sr_);
            sat_[c].setShape(dsp::Shape::Tanh);
        }
        flutter_.setRate(5.5f + 2.0f * pct(kFlutter));
        wow_.setRate(0.6f);
        mix_.setMix(pct(kMix), snap);
        if (snap) speed_ = -1.0;
    }
    double tailSeconds() const override {
        const double fb = std::clamp(p_[kIntensity] * 0.01, 0.0, 0.98);
        return fb <= 0.0 ? 4.0 : std::min(40.0, 3.0 * std::log(1e-3) / std::log(fb));
    }

    void processBlock(float* const* ch, int n, const ProcessContext& ctx) override {
        mix_.capture(ch, n);
        const double head1 = std::clamp(syncedSeconds(bp(kSync), fp(kTime), ip(kDiv), ctx.bpm), 0.04, kMaxHead1);
        const double vTarget = kRefSeconds / head1;  // tape speed (tape-distance units per second / ref)
        const double motorCoef = 1.0 - std::exp(-1.0 / (std::max(1.0f, fp(kMotor)) * 0.001 * sr_));
        if (speed_ < 0.0) speed_ = vTarget;
        const int mask = kHeadMask[std::clamp(ip(kHeads), 0, 6)];
        int active = 0;
        for (int h = 0; h < 3; ++h) active += (mask >> h) & 1;
        const float fb = pct(kIntensity) / static_cast<float>(std::max(1, active)) * (active > 1 ? 1.4f : 1.0f);
        const float sat = pct(kSaturation);
        const float drive = 1.0f + 5.0f * sat;
        const float wowAmt = pct(kWow) * 0.006f, flutAmt = pct(kFlutter) * 0.0025f;
        const float spread = pct(kSpread);
        const double ref = kRefSeconds * sr_;  // head-1 distance in "samples at speed 1"

        for (int i = 0; i < n; ++i) {
            speed_ += motorCoef * (vTarget - speed_);
            const double v = speed_ / kRefSeconds * (1.0 + wowAmt * wow_.next() + flutAmt * flutter_.next()) * kRefSeconds;
            // read heads at tape distance h*ref (in units where speed 1 == real time)
            float headOut[3][2] = {};
            for (int h = 0; h < 3; ++h) {
                if (!((mask >> h) & 1)) continue;
                const double X = V_ - (h + 1) * ref;  // tape position now under head h
                int64_t j = std::max<int64_t>(headIdx_[h], w_ - mask_ + 8);
                while (j + 1 < w_ && pos_[static_cast<size_t>((j + 1) & mask_)] <= X) ++j;
                headIdx_[h] = j;
                const double p0 = pos_[static_cast<size_t>(j & mask_)], p1 = pos_[static_cast<size_t>((j + 1) & mask_)];
                if (X < p0 || w_ < 4) continue;  // this part of the tape has not been recorded yet
                const float f = static_cast<float>(std::clamp((X - p0) / std::max(1e-12, p1 - p0), 0.0, 1.0));
                for (int c = 0; c < 2; ++c) {
                    const auto& a = audio_[c];
                    const float xm1 = a[static_cast<size_t>((j - 1) & mask_)], x0 = a[static_cast<size_t>(j & mask_)];
                    const float x1 = a[static_cast<size_t>((j + 1) & mask_)], x2 = a[static_cast<size_t>((j + 2) & mask_)];
                    const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
                    headOut[h][c] = ((c3 * f + c2) * f + c1) * f + x0;
                }
            }
            float out[2] = {0, 0}, loop[2] = {0, 0};
            for (int h = 0; h < 3; ++h) {
                if (!((mask >> h) & 1)) continue;
                const float pan = spread * (h == 0 ? -0.8f : (h == 1 ? 0.8f : 0.0f));
                float gl, gr;
                dsp::panGains(pan, gl, gr);
                const float m = 0.5f * (headOut[h][0] + headOut[h][1]);
                out[0] += (1.0f - spread) * headOut[h][0] + spread * m * gl * 1.41f;
                out[1] += (1.0f - spread) * headOut[h][1] + spread * m * gr * 1.41f;
                loop[0] += headOut[h][0];
                loop[1] += headOut[h][1];
            }
            // write: input + filtered feedback through the saturating record stage
            for (int c = 0; c < 2; ++c) {
                const float f = loopHp_[c].process(loopLp_[c].process(loop[c] * fb));
                const float rec = (ch[c][i] + f) * drive;
                audio_[c][static_cast<size_t>(w_ & mask_)] = sat_[c].process(rec) / drive * (1.0f + 0.25f * sat);
                ch[c][i] = treble_[c].process(bass_[c].process(out[c]));
            }
            V_ += v;
            pos_[static_cast<size_t>(w_ & mask_)] = V_;
            ++w_;
        }
        for (int c = 0; c < 2; ++c) { loopLp_[c].flush(); loopHp_[c].flush(); bass_[c].flush(); treble_[c].flush(); }
        mix_.apply(ch, n);
    }

    std::vector<float> audio_[2];
    std::vector<double> pos_;
    int64_t mask_ = 0, w_ = 0;
    int64_t headIdx_[3] = {0, 0, 0};
    double V_ = 0.0, speed_ = -1.0;
    dsp::AdaaShaper sat_[2];
    dsp::Svf loopLp_[2], loopHp_[2], bass_[2], treble_[2];
    dsp::Lfo wow_, flutter_;
    MixStage mix_;
};

}  // namespace

FxDescriptor describeTapeEcho() {
    FxDescriptor d;
    d.typeId = "tape_echo";
    d.name = "Tape Echo";
    d.family = FxFamily::TimeSpace;
    d.tooltip = "A spinning tape loop with three playback heads: warm, wobbly echoes that bend in pitch when you change the time.";
    d.params = {
        P::toggle("sync", "Sync", false, "Lock the first head's echo to the song tempo."),
        P::flt("time_ms", "Repeat Rate", "ms", 40.0f, 1200.0f, 300.0f, "Time to the first head. Changing it speeds the tape up or down, so echoes on the tape slide in pitch.", ParamCurve::Log),
        P::choice("division", "Division", divisionLabels(), 7, "First-head time as a note length when Sync is on."),
        P::choice("heads", "Heads", {"1", "2", "3", "1+2", "2+3", "1+3", "1+2+3"}, 0, "Which playback heads are on. Heads sit at 1x, 2x and 3x the repeat rate, making rhythmic patterns."),
        P::flt("intensity", "Intensity", "%", 0.0f, 120.0f, 40.0f, "How much of the echo is recorded again. Above 100% the tape starts to sing on its own."),
        P::pct("saturation", "Saturation", 30.0f, "How hard the tape is driven: rounder, thicker, compressed repeats."),
        P::pct("wow", "Wow", 20.0f, "Slow pitch drift from an uneven tape speed."),
        P::pct("flutter", "Flutter", 15.0f, "Fast, fine pitch wobble from the tape transport."),
        P::pct("age", "Tape Age", 35.0f, "Older tape loses treble and lows: each repeat gets darker and thinner."),
        P::flt("bass", "Bass", "dB", -12.0f, 12.0f, 0.0f, "Low-end tone of the echoes."),
        P::flt("treble", "Treble", "dB", -12.0f, 12.0f, 0.0f, "High-end tone of the echoes."),
        P::pct("spread", "Spread", 30.0f, "Places the heads across the stereo field."),
        P::logf("motor", "Motor", "ms", 20.0f, 2000.0f, 250.0f, "How slowly the tape speed follows time changes. Long = big pitch swoops."),
        P::pct("mix", "Mix", 35.0f, "Balance of original and echoes. 50% keeps both at full level."),
    };
    for (int i : {kBass, kTreble, kSpread, kMotor}) d.params[static_cast<size_t>(i)].advanced = true;
    d.create = [] { return std::make_unique<TapeEcho>(); };
    return d;
}

}  // namespace ih1::fx
