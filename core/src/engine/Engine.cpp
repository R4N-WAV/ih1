#include "ih1/engine/Engine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "ih1/dsp/Denormals.h"

namespace ih1 {

namespace {
constexpr float kOutputFadeMs = 5.0f;
constexpr float kStagedFadeMs = 10.0f;
constexpr float kSoloMs = 250.0f;
constexpr float kMuteMs = 30.0f;
constexpr float kBypassMs = 20.0f;
constexpr float kGainMs = 20.0f;
constexpr float kFreshInMs = 12.0f;  // raised-cosine input fade for a freshly built processor
constexpr float kSlotBypassMs = 15.0f;  // V4: per-instance enable / bypass crossfade
constexpr float kMacroMs = 40.0f;       // V4: macro smoothing (on top of each effect's own parameter smoothing)
constexpr float kIdleFloor = 1e-5f;     // V4: a bypassed tail below -100 dBFS for kIdleSeconds stops being computed
constexpr float kIdleSeconds = 1.5f;
constexpr float kSafetyLimit = 4.0f;  // +12 dBFS hard safety ceiling
constexpr int kCompMask = Engine::kCompSize - 1;

inline float dbToGain(float db) { return std::pow(10.0f, std::clamp(db, -60.0f, 24.0f) / 20.0f); }

// Linear ramp of `cur` toward `target` with fixed per-sample step; writes per-sample values.
inline void ramp(float& cur, float target, float step, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        if (cur < target) cur = std::min(target, cur + step);
        else if (cur > target) cur = std::max(target, cur - step);
        out[i] = cur;
    }
}
}  // namespace

bool sanitize(float* x, int n, float limit) {
    bool bad = false;
    for (int i = 0; i < n; ++i) {
        float v = x[i];
        if (!std::isfinite(v)) { v = 0.0f; bad = true; }
        x[i] = std::clamp(v, -limit, limit);
    }
    return bad;
}

Engine::Engine() {
    active_ = new Program();  // empty program; ownership passes to the message thread on retire
}

Engine::~Engine() {
    // Programs only reference processors (owned by Session), so deleting them here is safe.
    delete active_;
    delete incoming_;
    delete mailbox_.load();
    Program* r = nullptr;
    while (retired_.pop(r)) delete r;
    for (int i = 0; i < overflowCount_; ++i) delete overflow_[static_cast<size_t>(i)];
}

void Engine::prepare(double sampleRate, int maxBlock) {
    sampleRate_ = sampleRate > 0 ? sampleRate : 48000.0;
    maxBlock_ = std::max(1, maxBlock);
    const size_t b = static_cast<size_t>(maxBlock_) * 2;
    chainBuf_.assign(b, 0.0f);
    stagedBuf_.assign(b, 0.0f);
    sinBuf_.assign(b, 0.0f);
    dryHist_.assign(static_cast<size_t>(kCompSize) * 2, 0.0f);
    sinHist_.assign(static_cast<size_t>(kCompSize) * 2, 0.0f);
    rampSolo_.assign(maxBlock_, 0.0f);
    rampWet_.assign(maxBlock_, 0.0f);
    rampBypass_.assign(maxBlock_, 0.0f);
    rampTrans_.assign(maxBlock_, 0.0f);
    rampMix_.assign(maxBlock_, 0.0f);
    rampIn_.assign(maxBlock_, 0.0f);
    rampOut_.assign(maxBlock_, 0.0f);
    dryPos_ = sinPos_ = 0;
    soloAmt_ = solo_.load() ? 1.0f : 0.0f;
    muteAmt_ = mute_.load() ? 1.0f : 0.0f;
    bypassAmt_ = bypass_.load() ? 1.0f : 0.0f;
    transGain_ = 1.0f;
    stagedFade_ = 1.0f;
    inGain_ = dbToGain(inGainDb_.load());
    outGain_ = dbToGain(outGainDb_.load());
    mixAmt_ = std::clamp(mix_.load(), 0.0f, 1.0f);
    trans_ = Transition::None;
    bandLowState_[0] = bandLowState_[1] = bandHighState_[0] = bandHighState_[1] = 0.0f;
    safety_.setMode(safetyMode_.load());
    safety_.setCeilingDb(safetyCeil_.load());
    safety_.prepare(sampleRate_, maxBlock_);
    slotInRamp_.fill(1.0f);
    stagedInRamp_ = 1.0f;
    slotDly_.assign(static_cast<size_t>(kMaxChain) * 2 * kSlotDly, 0.0f);
    slotDlyPos_ = 0;
    slotTmp_.assign(static_cast<size_t>(maxBlock_) * 4, 0.0f);
    for (int s = 0; s < kMaxChain; ++s) {
        slotOn_[static_cast<size_t>(s)] = (active_ && s < active_->chainSize && !active_->enabled[static_cast<size_t>(s)]) ? 0.0f : 1.0f;
        slotIdle_[static_cast<size_t>(s)] = 0;
        slotQuiet_[static_cast<size_t>(s)] = 0;
        slotEnvFast_[static_cast<size_t>(s)] = slotEnvSlow_[static_cast<size_t>(s)] = 0.0f;
    }
    for (int m = 0; m < kNumPresetMacros; ++m) macroSm_[static_cast<size_t>(m)] = macro_[static_cast<size_t>(m)].load();
    macroApplied_.fill(-1.0f);
}

void Engine::markFresh(const Program* old, const Program* p) {
    // A processor is fresh when the outgoing program did not run it (new instance, preset load, RESET, latency
    // mode rebuild). COMMIT hands the staged processor over to the chain: not fresh, it keeps playing.
    auto known = [old](const FxProcessor* fx) {
        if (!old || !fx) return false;
        if (old->staged == fx) return true;
        for (int i = 0; i < old->chainSize; ++i) if (old->chain[static_cast<size_t>(i)] == fx) return true;
        return false;
    };
    std::array<float, kMaxChain> next{};
    next.fill(1.0f);
    for (int i = 0; i < p->chainSize; ++i) {
        const FxProcessor* fx = p->chain[static_cast<size_t>(i)];
        if (known(fx)) {
            // carry an in-progress fade along with its processor across a reorder
            for (int j = 0; old && j < old->chainSize; ++j)
                if (old->chain[static_cast<size_t>(j)] == fx) next[static_cast<size_t>(i)] = slotInRamp_[static_cast<size_t>(j)];
            if (old && old->staged == fx) next[static_cast<size_t>(i)] = stagedInRamp_;
        } else {
            next[static_cast<size_t>(i)] = 0.0f;
        }
    }
    slotInRamp_ = next;
    stagedInRamp_ = (p->staged && !(old && old->staged == p->staged)) ? 0.0f : 1.0f;
    // V4: per-slot enable ramps and idle flags travel with their processor (reorder); new processors start at their
    // target (they are faded in by the fresh-input ramp anyway). A slot whose processor changed gets a clean
    // latency-compensation history.
    std::array<float, kMaxChain> on{};
    std::array<uint8_t, kMaxChain> idle{};
    for (int i = 0; i < p->chainSize; ++i) {
        const size_t si = static_cast<size_t>(i);
        on[si] = p->enabled[si] ? 1.0f : 0.0f;
        idle[si] = 0;
        bool same = false;
        for (int j = 0; old && j < old->chainSize; ++j)
            if (old->chain[static_cast<size_t>(j)] == p->chain[si]) {
                on[si] = slotOn_[static_cast<size_t>(j)];
                idle[si] = slotIdle_[static_cast<size_t>(j)];
                same = j == i;
            }
        if (!same && p->slotLatency[si] > 0 && !slotDly_.empty()) {
            float* d = slotDly_.data() + si * 2 * kSlotDly;
            std::fill(d, d + 2 * kSlotDly, 0.0f);
        }
    }
    slotOn_ = on;
    slotIdle_ = idle;
}

void Engine::applyMacros(bool force) {
    const Program& p = *active_;
    if (p.numMacroTargets <= 0) return;
    bool changed = force;
    for (int m = 0; m < kNumPresetMacros; ++m) if (std::fabs(macroSm_[static_cast<size_t>(m)] - macroApplied_[static_cast<size_t>(m)]) > 1e-4f) changed = true;
    if (!changed) return;
    macroApplied_ = macroSm_;
    // one setParams per affected slot: stored snapshot + every target's offset (snapshot is never modified)
    uint32_t done = 0;
    for (int t = 0; t < p.numMacroTargets; ++t) {
        const int slot = p.macroTargets[static_cast<size_t>(t)].slot;
        if (slot < 0 || slot >= p.chainSize || (done & (1u << slot))) continue;
        done |= 1u << slot;
        ParamValues v = p.chainValues[static_cast<size_t>(slot)];
        for (int u = t; u < p.numMacroTargets; ++u) {
            const auto& mt = p.macroTargets[static_cast<size_t>(u)];
            if (mt.slot != slot || mt.param < 0 || mt.param >= kMaxParams) continue;
            float& x = v[static_cast<size_t>(mt.param)];
            x = std::clamp(x + mt.depth * macroSm_[static_cast<size_t>(std::clamp(mt.macro, 0, kNumPresetMacros - 1))], mt.lo, mt.hi);
        }
        p.chain[static_cast<size_t>(slot)]->setParams(v, false);
    }
}

bool Engine::post(Program* p) {
    Program* expected = nullptr;
    return mailbox_.compare_exchange_strong(expected, p, std::memory_order_acq_rel);
}

void Engine::installDirect(Program* p, std::vector<Program*>& released) {
    if (Program* m = mailbox_.exchange(nullptr)) released.push_back(m);
    if (incoming_) { released.push_back(incoming_); incoming_ = nullptr; }
    Program* r = nullptr;
    while (retired_.pop(r)) released.push_back(r);
    for (int i = 0; i < overflowCount_; ++i) released.push_back(overflow_[static_cast<size_t>(i)]);
    overflowCount_ = 0;
    if (active_) released.push_back(active_);
    active_ = p;
    slotInRamp_.fill(1.0f);
    stagedInRamp_ = 1.0f;
    for (int i = 0; i < p->chainSize; ++i) p->chain[static_cast<size_t>(i)]->setParams(p->chainValues[static_cast<size_t>(i)], true);
    for (int i = 0; i < kMaxChain; ++i) {
        slotOn_[static_cast<size_t>(i)] = (i < p->chainSize && !p->enabled[static_cast<size_t>(i)]) ? 0.0f : 1.0f;
        slotIdle_[static_cast<size_t>(i)] = 0;
    }
    macroApplied_.fill(-1.0f);
    applyMacros(true);
    trans_ = Transition::None;
    transGain_ = 1.0f;
    stagedFade_ = 1.0f;
    haveLastStaged_ = false;
    switchPending_ = false;
    switchAmt_ = 1.0f;
    appliedVersion_ = stagedParamVersion_.load() - 1u;
    activeLatency_.store(p->chainLatency + p->stagedLatency);
}

void Engine::publishStagedParams(int typeIndex, const ParamValues& v, const FxProcessor* target) {
    stagedParamTarget_.store(target, std::memory_order_relaxed);
    for (int i = 0; i < kMaxParams; ++i) stagedParams_[static_cast<size_t>(i)].store(v[static_cast<size_t>(i)], std::memory_order_relaxed);
    stagedParamType_.store(typeIndex, std::memory_order_relaxed);
    stagedParamVersion_.fetch_add(1, std::memory_order_release);
}

void Engine::applyStagedParamsIfChanged() {
    const uint32_t v = stagedParamVersion_.load(std::memory_order_acquire);
    if (v == appliedVersion_ || !active_->staged) return;
    if (stagedParamType_.load(std::memory_order_relaxed) != active_->stagedType) return;  // not for us (yet)
    {
        const FxProcessor* t = stagedParamTarget_.load(std::memory_order_relaxed);
        if (t && t != active_->staged) return;  // V5: for the next staged processor (still in the mailbox)
    }
    ParamValues vals;
    for (int i = 0; i < kMaxParams; ++i) vals[static_cast<size_t>(i)] = stagedParams_[static_cast<size_t>(i)].load(std::memory_order_relaxed);
    appliedVersion_ = v;
    // discrete change? (registry is immutable after start-up: reading it here is lock- and allocation-free)
    bool discrete = false;
    const int type = active_->stagedType;
    if (haveLastStaged_ && type >= 0 && type < static_cast<int>(fxRegistry().size())) {
        const auto& ps = fxRegistry()[static_cast<size_t>(type)].params;
        for (size_t i = 0; i < ps.size(); ++i)
            if (ps[i].kind != ParamKind::Float && vals[i] != lastStaged_[i]) { discrete = true; break; }
    }
    if (switchPending_ || discrete) {
        pendingStaged_ = vals;  // applied when the dip reaches zero (latest values win)
        switchPending_ = true;
        return;
    }
    active_->staged->setParams(vals, false);
    lastStaged_ = vals;
    haveLastStaged_ = true;
}

void Engine::installNow(Program* p) {
    for (int i = 0; i < p->chainSize; ++i) p->chain[static_cast<size_t>(i)]->setParams(p->chainValues[static_cast<size_t>(i)], false);
    // retire the old one
    Program* old = active_;
    markFresh(old, p);
    active_ = p;
    if (old) {
        // flush overflow first to preserve order
        int k = 0;
        while (k < overflowCount_ && retired_.push(overflow_[static_cast<size_t>(k)])) ++k;
        if (k > 0) {
            for (int i = k; i < overflowCount_; ++i) overflow_[static_cast<size_t>(i - k)] = overflow_[static_cast<size_t>(i)];
            overflowCount_ -= k;
        }
        if (overflowCount_ > 0 || !retired_.push(old)) {
            if (overflowCount_ < static_cast<int>(overflow_.size())) overflow_[static_cast<size_t>(overflowCount_++)] = old;
            // else: leak rather than free on the audio thread (cannot happen with a serviced queue)
        }
    }
    appliedVersion_ = stagedParamVersion_.load() - 1u;  // force re-apply of staged params
    activeLatency_.store(p->chainLatency + p->stagedLatency);
    if (!old || old->staged != p->staged) { haveLastStaged_ = false; switchPending_ = false; switchAmt_ = 1.0f; }
    if (p->stagedStartsMuted && p->staged && mute_.load(std::memory_order_relaxed)) {
        // V5 Phase 7: the freshly staged copy is silent from its first sample. The MUTE flag itself belongs to the
        // message thread (the session sets it with the COMMIT); if the user already un-muted before this program
        // arrived, the copy simply starts audible (with its fresh-input ramp).
        muteAmt_ = 1.0f;
    }
    macroApplied_.fill(-1.0f);
    applyMacros(false);
}

void Engine::beginTransitionIfPending() {
    if (trans_ != Transition::None || incoming_) return;
    Program* p = mailbox_.exchange(nullptr, std::memory_order_acq_rel);
    if (!p) return;
    const Program* a = active_;
    bool sameChain = p->chainSize == a->chainSize;
    for (int i = 0; sameChain && i < a->chainSize; ++i) sameChain = p->chain[static_cast<size_t>(i)] == a->chain[static_cast<size_t>(i)];

    // V4: a program that only changes per-instance enable flags (same processors in the same order, same staged
    // processor) installs at once: each slot's own 15 ms bypass crossfade does the transition, no output fade.
    if (sameChain && p->staged == a->staged && p->stagedType == a->stagedType && !p->commitHandover) {
        installNow(p);
        return;
    }

    // COMMIT hand-over: identical prefix + old staged appended, nothing staged after. Seamless when the
    // staged FX was fully audible (no SOLO, no MUTE, no fade in progress).
    // V5: the program may also carry a fresh staged processor (same type, defaults) that starts muted; it is seamless
    // only when that processor adds no latency (otherwise the chain output would move in time).
    const bool newStagedSilent = p->staged == nullptr || (p->stagedStartsMuted && p->stagedLatency == 0);
    bool handover = p->commitHandover && a->staged != nullptr && newStagedSilent &&
                    p->chainSize == a->chainSize + 1 && p->chain[static_cast<size_t>(a->chainSize)] == a->staged &&
                    soloAmt_ == 0.0f && muteAmt_ == 0.0f && stagedFade_ == 1.0f && transGain_ == 1.0f;
    for (int i = 0; handover && i < a->chainSize; ++i) handover = p->chain[static_cast<size_t>(i)] == a->chain[static_cast<size_t>(i)];

    if (handover) {
        installNow(p);
        return;
    }
    incoming_ = p;
    trans_ = (sameChain && p->stagedLatency == a->stagedLatency) ? Transition::StagedFadeOut : Transition::OutputFadeOut;
}

void Engine::process(float* left, float* right, int numSamples, const ProcessContext& ctx) {
    dsp::ScopedNoDenormals noDenormals;
    if (maxBlock_ <= 0) return;
    int offset = 0;
    while (offset < numSamples) {
        beginTransitionIfPending();
        int n = std::min(numSamples - offset, maxBlock_);
        // stop the segment exactly where a fade-out reaches zero so the swap happens on a sample boundary
        if (trans_ == Transition::OutputFadeOut) {
            const float step = 1.0f / (kOutputFadeMs * 0.001f * static_cast<float>(sampleRate_));
            n = std::min(n, std::max(1, static_cast<int>(std::ceil(transGain_ / step))));
        } else if (trans_ == Transition::StagedFadeOut) {
            const float step = 1.0f / (kStagedFadeMs * 0.001f * static_cast<float>(sampleRate_));
            n = std::min(n, std::max(1, static_cast<int>(std::ceil(stagedFade_ / step))));
        }
        ProcessContext sub = ctx;
        // MIDI events belonging to this segment
        int first = 0;
        while (first < ctx.numMidi && ctx.midi[first].sampleOffset < offset) ++first;
        int last = first;
        while (last < ctx.numMidi && ctx.midi[last].sampleOffset < offset + n) ++last;
        sub.midi = ctx.midi ? ctx.midi + first : nullptr;
        sub.numMidi = last - first;
        // NOTE: event offsets inside sub are still relative to the full block; FX subtract `blockOffset`.
        sub.blockOffset = offset;
        if (ctx.ppqValid) sub.ppqPosition = ctx.ppqPosition + offset / sampleRate_ * ctx.bpm / 60.0;
        processChunk(left + offset, right + offset, n, sub);
        offset += n;

        if (trans_ == Transition::OutputFadeOut && transGain_ <= 0.0f) {
            installNow(incoming_);
            incoming_ = nullptr;
            trans_ = Transition::OutputFadeIn;
        } else if (trans_ == Transition::StagedFadeOut && stagedFade_ <= 0.0f) {
            installNow(incoming_);
            incoming_ = nullptr;
            trans_ = Transition::StagedFadeIn;
        } else if (trans_ == Transition::OutputFadeIn && transGain_ >= 1.0f) {
            trans_ = Transition::None;
        } else if (trans_ == Transition::StagedFadeIn && stagedFade_ >= 1.0f) {
            trans_ = Transition::None;
        }
    }
}

void Engine::processSlot(int s, float* cL, float* cR, int n, const ProcessContext& ctx) {
    const Program& p = *active_;
    const size_t si = static_cast<size_t>(s);
    FxProcessor* fx = p.chain[si];
    const float sr = static_cast<float>(sampleRate_);
    const float freshStep = 1.0f / (kFreshInMs * 0.001f * sr);
    const float onStep = 1.0f / (kSlotBypassMs * 0.001f * sr);
    const float target = p.enabled[si] ? 1.0f : 0.0f;
    float& on = slotOn_[si];
    // latency-matched copy of this slot's input (always written, so a bypass can start at any moment)
    const int lat = std::clamp(p.slotLatency[si], 0, kSlotDly - maxBlock_ - 1);
    float* dlyL = slotDly_.data() + si * 2 * kSlotDly;
    float* dlyR = dlyL + kSlotDly;
    float* dryL = slotTmp_.data();
    float* dryR = dryL + maxBlock_;
    for (int i = 0; i < n; ++i) {
        const int w = (slotDlyPos_ + i) & (kSlotDly - 1);
        dlyL[w] = cL[i];
        dlyR[w] = cR[i];
        const int r = (slotDlyPos_ + i - lat) & (kSlotDly - 1);
        dryL[i] = dlyL[r];
        dryR[i] = dlyR[r];
    }
    const bool fullyOn = on >= 1.0f && target >= 1.0f;
    const bool tail = p.tailOnBypass[si] != 0;
    auto fadeIn = [n, freshStep](float& ramp, float* a, float* b) {
        for (int i = 0; i < n && ramp < 1.0f; ++i) {
            const float g = 0.5f - 0.5f * std::cos(3.14159265f * ramp);
            a[i] *= g;
            b[i] *= g;
            ramp = std::min(1.0f, ramp + freshStep);
        }
    };
    if (fullyOn) {
        slotIdle_[si] = 0;
        float* cbuf[2] = {cL, cR};
        if (slotInRamp_[si] < 1.0f) fadeIn(slotInRamp_[si], cL, cR);
        fx->process(cbuf, n, ctx);
    } else {
        // per-sample enable ramp
        float* onR = slotTmp_.data() + 2 * static_cast<size_t>(maxBlock_);  // reuse as ramp storage (n floats)
        for (int i = 0; i < n; ++i) {
            if (on < target) on = std::min(target, on + onStep);
            else if (on > target) on = std::max(target, on - onStep);
            onR[i] = on;
        }
        const bool idleNow = slotIdle_[si] && target <= 0.0f && on <= 0.0f;
        if (idleNow) {
            std::memcpy(cL, dryL, sizeof(float) * static_cast<size_t>(n));
            std::memcpy(cR, dryR, sizeof(float) * static_cast<size_t>(n));
        } else {
            if (slotIdle_[si]) { slotIdle_[si] = 0; slotInRamp_[si] = 0.0f; }  // waking a parked tail effect
            if (tail) {
                // stop new input (faded with the ramp), let the tail ring; the bypassed dry path fades in beside it
                for (int i = 0; i < n; ++i) { cL[i] *= onR[i]; cR[i] *= onR[i]; }
                float* cbuf[2] = {cL, cR};
                if (slotInRamp_[si] < 1.0f) fadeIn(slotInRamp_[si], cL, cR);
                fx->process(cbuf, n, ctx);
                double sq = 0.0;
                for (int i = 0; i < n; ++i) {
                    sq += static_cast<double>(cL[i]) * cL[i] + static_cast<double>(cR[i]) * cR[i];
                    cL[i] += (1.0f - onR[i]) * dryL[i];
                    cR[i] += (1.0f - onR[i]) * dryR[i];
                }
                // park the effect once its tail has decayed (saves CPU while bypassed)
                if (target <= 0.0f && on <= 0.0f) {
                    const float rms = static_cast<float>(std::sqrt(sq / (2.0 * n)));
                    slotQuiet_[si] = rms < kIdleFloor ? slotQuiet_[si] + n : 0;
                    if (slotQuiet_[si] > static_cast<int>(kIdleSeconds * sr)) slotIdle_[si] = 1;
                } else {
                    slotQuiet_[si] = 0;
                }
            } else {
                // switch off with a crossfade; the effect keeps running underneath (re-enable is seamless)
                float* cbuf[2] = {cL, cR};
                if (slotInRamp_[si] < 1.0f) fadeIn(slotInRamp_[si], cL, cR);
                fx->process(cbuf, n, ctx);
                for (int i = 0; i < n; ++i) {
                    const float g = onR[i];
                    cL[i] = g * cL[i] + (1.0f - g) * dryL[i];
                    cR[i] = g * cR[i] + (1.0f - g) * dryR[i];
                }
            }
        }
    }
    const bool bad = sanitize(cL, n, 32.0f) | sanitize(cR, n, 32.0f);
    if (bad) { fx->reset(); slotInRamp_[si] = 0.0f; tele_.faults.fetch_add(1, std::memory_order_relaxed); }
    // stage analysis (V4 Phase 7.3): RMS, sample peak, onset strength (fast vs slow envelope), never audio buffers
    double sq = 0.0;
    float pk = 0.0f;
    for (int i = 0; i < n; ++i) {
        sq += 0.5 * (static_cast<double>(cL[i]) * cL[i] + static_cast<double>(cR[i]) * cR[i]);
        pk = std::max(pk, std::max(std::fabs(cL[i]), std::fabs(cR[i])));
    }
    const float rms = static_cast<float>(std::sqrt(sq / n));
    const float af = 1.0f - std::exp(-static_cast<float>(n) / (0.005f * sr));
    const float as = 1.0f - std::exp(-static_cast<float>(n) / (0.150f * sr));
    slotEnvFast_[si] += af * (rms - slotEnvFast_[si]);
    slotEnvSlow_[si] += as * (rms - slotEnvSlow_[si]);
    const float onset = std::clamp((slotEnvFast_[si] - 1.4f * slotEnvSlow_[si]) / std::max(slotEnvSlow_[si], 1e-4f), 0.0f, 1.0f);
    tele_.slotRms[si].store(rms, std::memory_order_relaxed);
    tele_.slotPeak[si].store(pk, std::memory_order_relaxed);
    tele_.slotOnset[si].store(onset, std::memory_order_relaxed);
    tele_.slotEnabled[si].store(on, std::memory_order_relaxed);
    tele_.slotIds[si].store(p.ids[si], std::memory_order_relaxed);
}

void Engine::processChunk(float* L, float* R, int n, const ProcessContext& ctx) {
    applyStagedParamsIfChanged();
    const float sr = static_cast<float>(sampleRate_);
    {
        // macros: one-pole smoothing per block (each effect smooths its own parameters further)
        const float a = 1.0f - std::exp(-static_cast<float>(n) / (kMacroMs * 0.001f * sr));
        for (int m = 0; m < kNumPresetMacros; ++m) {
            float& v = macroSm_[static_cast<size_t>(m)];
            const float t = macro_[static_cast<size_t>(m)].load(std::memory_order_relaxed);
            v += a * (t - v);
            if (std::fabs(t - v) < 2e-4f) v = t;
        }
        applyMacros(false);
    }
    auto stepFor = [sr](float ms) { return 1.0f / (ms * 0.001f * sr); };

    const Program& p = *active_;

    // --- ramps --------------------------------------------------------------------------
    ramp(inGain_, dbToGain(inGainDb_.load(std::memory_order_relaxed)), stepFor(kGainMs) * 2.0f, rampIn_.data(), n);
    ramp(outGain_, dbToGain(outGainDb_.load(std::memory_order_relaxed)), stepFor(kGainMs) * 2.0f, rampOut_.data(), n);
    ramp(mixAmt_, std::clamp(mix_.load(std::memory_order_relaxed), 0.0f, 1.0f), stepFor(kGainMs), rampMix_.data(), n);
    ramp(bypassAmt_, bypass_.load(std::memory_order_relaxed) ? 1.0f : 0.0f, stepFor(kBypassMs), rampBypass_.data(), n);
    const bool hasStaged = p.staged != nullptr;
    const bool soloOn = solo_.load(std::memory_order_relaxed) && hasStaged;
    bool muteOn = mute_.load(std::memory_order_relaxed) && !soloOn;
    if (muteOn && muteAmt_ == 0.0f) {
        // V5 Phase 7: the host MUTE parameter can rise a block before the COMMIT program is picked up. While that
        // program waits, the outgoing staged effect (the one being committed) stays fully audible, so the hand-over
        // stays seamless; the new staged copy is installed muted.
        const Program* q = mailbox_.load(std::memory_order_acquire);
        if (q && q->commitHandover && q->stagedStartsMuted) muteOn = false;
    }
    ramp(soloAmt_, soloOn ? 1.0f : 0.0f, stepFor(kSoloMs), rampSolo_.data(), n);
    {
        float tgt = (trans_ == Transition::OutputFadeOut) ? 0.0f : 1.0f;
        ramp(transGain_, tgt, stepFor(kOutputFadeMs), rampTrans_.data(), n);
    }
    // wet weight of the staged stage = (1 - mute) * stagedFade
    {
        const float muteStep = stepFor(kMuteMs), fadeStep = stepFor(kStagedFadeMs);
        const float muteTgt = muteOn ? 1.0f : 0.0f;
        const float fadeTgt = (trans_ == Transition::StagedFadeOut) ? 0.0f : 1.0f;
        for (int i = 0; i < n; ++i) {
            if (muteAmt_ < muteTgt) muteAmt_ = std::min(muteTgt, muteAmt_ + muteStep);
            else if (muteAmt_ > muteTgt) muteAmt_ = std::max(muteTgt, muteAmt_ - muteStep);
            if (stagedFade_ < fadeTgt) stagedFade_ = std::min(fadeTgt, stagedFade_ + fadeStep);
            else if (stagedFade_ > fadeTgt) stagedFade_ = std::max(fadeTgt, stagedFade_ - fadeStep);
            rampWet_[static_cast<size_t>(i)] = (1.0f - muteAmt_) * stagedFade_;
        }
        // discrete switch dip (6 ms down, apply, 6 ms up)
        const float swStep = stepFor(6.0f), swTgt = switchPending_ ? 0.0f : 1.0f;
        for (int i = 0; i < n; ++i) {
            if (switchAmt_ < swTgt) switchAmt_ = std::min(swTgt, switchAmt_ + swStep);
            else if (switchAmt_ > swTgt) switchAmt_ = std::max(swTgt, switchAmt_ - swStep);
            const float sm = 0.5f - 0.5f * std::cos(3.14159265f * switchAmt_);
            rampWet_[static_cast<size_t>(i)] *= sm;
        }
    }

    // --- input gain, history ---------------------------------------------------------------
    double inSq = 0.0;
    float* cL = chainBuf_.data();
    float* cR = chainBuf_.data() + maxBlock_;
    float* dL = dryHist_.data();
    float* dR = dryHist_.data() + kCompSize;
    for (int i = 0; i < n; ++i) {
        const float g = rampIn_[static_cast<size_t>(i)];
        float l = L[i] * g, r = R[i] * g;
        if (!std::isfinite(l)) l = 0.0f;
        if (!std::isfinite(r)) r = 0.0f;
        cL[i] = l;
        cR[i] = r;
        const int w = (dryPos_ + i) & kCompMask;
        dL[w] = l;
        dR[w] = r;
        inSq += 0.5 * (static_cast<double>(l) * l + static_cast<double>(r) * r);
    }
    tele_.inRms.store(static_cast<float>(std::sqrt(inSq / std::max(1, n))), std::memory_order_relaxed);

    // --- committed chain ------------------------------------------------------------------
    float* cbuf[2] = {cL, cR};
    const float freshStep = stepFor(kFreshInMs);
    auto fadeIn = [n, freshStep](float& ramp, float* a, float* b) {
        for (int i = 0; i < n && ramp < 1.0f; ++i) {
            const float g = 0.5f - 0.5f * std::cos(3.14159265f * ramp);
            a[i] *= g;
            b[i] *= g;
            ramp = std::min(1.0f, ramp + freshStep);
        }
    };
    (void)cbuf;
    (void)fadeIn;
    for (int s = 0; s < p.chainSize; ++s) processSlot(s, cL, cR, n, ctx);
    slotDlyPos_ = (slotDlyPos_ + n) & (kSlotDly - 1);
    tele_.chainSize.store(p.chainSize, std::memory_order_relaxed);

    // --- SOLO mix: staged input = (1-a) * chain + a * dry (delayed by chain latency) ------
    const int Lc = std::clamp(p.chainLatency, 0, kCompSize - maxBlock_ - 1);
    const int Ls = std::clamp(p.stagedLatency, 0, kCompSize - maxBlock_ - 1);
    float* sL = sinBuf_.data();
    float* sR = sinBuf_.data() + maxBlock_;
    float* hL = sinHist_.data();
    float* hR = sinHist_.data() + kCompSize;
    for (int i = 0; i < n; ++i) {
        const float a = rampSolo_[static_cast<size_t>(i)];
        const int rd = (dryPos_ + i - Lc) & kCompMask;
        sL[i] = (1.0f - a) * cL[i] + a * dL[rd];
        sR[i] = (1.0f - a) * cR[i] + a * dR[rd];
        const int w = (sinPos_ + i) & kCompMask;
        hL[w] = sL[i];
        hR[w] = sR[i];
    }

    // --- staged FX --------------------------------------------------------------------------
    float* tL = stagedBuf_.data();
    float* tR = stagedBuf_.data() + maxBlock_;
    std::memcpy(tL, sL, sizeof(float) * static_cast<size_t>(n));
    std::memcpy(tR, sR, sizeof(float) * static_cast<size_t>(n));
    if (hasStaged) {
        float* tbuf[2] = {tL, tR};
        if (stagedInRamp_ < 1.0f) fadeIn(stagedInRamp_, tL, tR);
        p.staged->process(tbuf, n, ctx);
        const bool bad = sanitize(tL, n, 32.0f) | sanitize(tR, n, 32.0f);
        if (bad) { p.staged->reset(); stagedInRamp_ = 0.0f; tele_.faults.fetch_add(1, std::memory_order_relaxed); }
        double sq = 0.0;
        for (int i = 0; i < n; ++i) sq += 0.5 * (static_cast<double>(tL[i]) * tL[i] + static_cast<double>(tR[i]) * tR[i]);
        tele_.stagedRms.store(static_cast<float>(std::sqrt(sq / n)), std::memory_order_relaxed);
    } else {
        tele_.stagedRms.store(0.0f, std::memory_order_relaxed);
    }

    // --- output assembly -------------------------------------------------------------------
    const int T = Lc + Ls;
    double outSq = 0.0, lowSq = 0.0, midSq = 0.0, highSq = 0.0, lSq = 0.0, rSq = 0.0;
    float peak = 0.0f;
    const float aLow = 1.0f - std::exp(-2.0f * 3.14159265f * 250.0f / sr);
    const float aHigh = 1.0f - std::exp(-2.0f * 3.14159265f * 4000.0f / sr);
    for (int i = 0; i < n; ++i) {
        const size_t si = static_cast<size_t>(i);
        const float w = hasStaged ? rampWet_[si] : 0.0f;
        const int rs = (sinPos_ + i - Ls) & kCompMask;
        const int rt = (dryPos_ + i - T) & kCompMask;
        float oL = w * tL[i] + (1.0f - w) * hL[rs];
        float oR = w * tR[i] + (1.0f - w) * hR[rs];
        const float mix = rampMix_[si];
        oL = mix * oL + (1.0f - mix) * dL[rt];
        oR = mix * oR + (1.0f - mix) * dR[rt];
        const float b = rampBypass_[si];
        const float tg = rampTrans_[si];
        oL = (1.0f - b) * tg * oL + b * dL[rt];
        oR = (1.0f - b) * tg * oR + b * dR[rt];
        const float og = rampOut_[si];
        oL *= og;
        oR *= og;
        if (!std::isfinite(oL)) oL = 0.0f;
        if (!std::isfinite(oR)) oR = 0.0f;
        oL = std::clamp(oL, -kSafetyLimit, kSafetyLimit);
        oR = std::clamp(oR, -kSafetyLimit, kSafetyLimit);
        L[i] = oL;
        R[i] = oR;
        lSq += static_cast<double>(oL) * oL;
        rSq += static_cast<double>(oR) * oR;
        outSq += 0.5 * (static_cast<double>(oL) * oL + static_cast<double>(oR) * oR);
        peak = std::max(peak, std::max(std::fabs(oL), std::fabs(oR)));
        const float m = 0.5f * (oL + oR);
        bandLowState_[0] += aLow * (m - bandLowState_[0]);
        bandHighState_[0] += aHigh * (m - bandHighState_[0]);
        const float lo = bandLowState_[0];
        const float hi = m - bandHighState_[0];
        const float mid = m - lo - hi;
        lowSq += lo * lo;
        midSq += mid * mid;
        highSq += hi * hi;
    }
    // --- Output Safety (final stage) -------------------------------------------------------
    tele_.preSafetyPeak.store(peak, std::memory_order_relaxed);
    safety_.setMode(safetyMode_.load(std::memory_order_relaxed));
    safety_.setCeilingDb(safetyCeil_.load(std::memory_order_relaxed));
    {
        const float g = safety_.process(L, R, n);
        tele_.safetyGainReduction.store(g < 1.0f ? -20.0f * std::log10(std::max(g, 1e-6f)) : 0.0f, std::memory_order_relaxed);
        tele_.safetyMode.store(safety_.mode(), std::memory_order_relaxed);
        peak = safety_.lastPeak();
    }
    bandLowState_[0] = dsp::flushTiny(bandLowState_[0]);
    bandHighState_[0] = dsp::flushTiny(bandHighState_[0]);
    const double inv = 1.0 / std::max(1, n);
    tele_.outRms.store(static_cast<float>(std::sqrt(outSq * inv)), std::memory_order_relaxed);
    tele_.outPeak.store(peak, std::memory_order_relaxed);
    tele_.outLow.store(static_cast<float>(std::sqrt(lowSq * inv)), std::memory_order_relaxed);
    tele_.outMid.store(static_cast<float>(std::sqrt(midSq * inv)), std::memory_order_relaxed);
    tele_.outHigh.store(static_cast<float>(std::sqrt(highSq * inv)), std::memory_order_relaxed);
    {
        const double a = std::sqrt(lSq * inv), b = std::sqrt(rSq * inv);
        tele_.outBalance.store(a + b > 1e-6 ? static_cast<float>((b - a) / (a + b)) : 0.0f, std::memory_order_relaxed);
    }
    tele_.soloAmount.store(soloAmt_, std::memory_order_relaxed);
    tele_.muteAmount.store(muteAmt_, std::memory_order_relaxed);

    dryPos_ = (dryPos_ + n) & kCompMask;
    sinPos_ = (sinPos_ + n) & kCompMask;
    if (switchPending_ && switchAmt_ <= 0.0f && active_->staged) {
        active_->staged->setParams(pendingStaged_, false);
        lastStaged_ = pendingStaged_;
        haveLastStaged_ = true;
        switchPending_ = false;
    }
}

}  // namespace ih1
