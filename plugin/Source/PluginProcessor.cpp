#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "ih1/model/Json.h"

namespace ih1::plugin {

namespace {
const char* kStateMagic = "IH1STATE1";
juce::String macroId(int k) { return "staged_" + juce::String(k + 1); }
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout Processor::createLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    // IDs and ranges are permanent (append-only): see docs/ARCHITECTURE.md section 5.
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass", 1}, "Bypass", false));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"solo", 1}, "Solo FX", false));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"mute", 1}, "Mute FX", false));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"in_gain", 1}, "Input Gain", NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"out_gain", 1}, "Output Gain", NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"mix", 1}, "Global Mix", NormalisableRange<float>(0.0f, 100.0f, 0.01f), 100.0f, AudioParameterFloatAttributes().withLabel("%")));
    for (int k = 0; k < kNumMacros; ++k)
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{macroId(k), 1}, "Staged Control " + String(k + 1), NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    return l;
}

Processor::Processor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "IH1", createLayout()) {
    session_ = std::make_unique<Session>(engine_);
    bypass_ = dynamic_cast<juce::AudioParameterBool*>(apvts_.getParameter("bypass"));
    pBypass_ = apvts_.getRawParameterValue("bypass");
    pSolo_ = apvts_.getRawParameterValue("solo");
    pMute_ = apvts_.getRawParameterValue("mute");
    pIn_ = apvts_.getRawParameterValue("in_gain");
    pOut_ = apvts_.getRawParameterValue("out_gain");
    pMix_ = apvts_.getRawParameterValue("mix");
    for (int k = 0; k < kNumMacros; ++k) {
        macros_[static_cast<size_t>(k)] = apvts_.getParameter(macroId(k));
        lastMacro_[static_cast<size_t>(k)] = 0.0f;
    }
    midi_.reserve(4096);
    startTimerHz(30);
}

Processor::~Processor() {
    stopTimer();
    const juce::ScopedLock sl(lock_);
    session_.reset();
}

bool Processor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out;
}

void Processor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    const juce::ScopedLock sl(lock_);
    session_->prepare(sampleRate, std::max(32, samplesPerBlock));
    monoScratch_.assign(static_cast<size_t>(std::max(32, samplesPerBlock)) * 4, 0.0f);
    afterModelChange();
}

void Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    for (int c = inCh; c < outCh; ++c) buffer.clear(c, 0, n);
    if (outCh == 0 || n == 0) return;

    engine_.setBypass(pBypass_->load() > 0.5f);
    const bool solo = pSolo_->load() > 0.5f;
    engine_.setSolo(solo);
    engine_.setMute(pMute_->load() > 0.5f && !solo);
    engine_.setInputGainDb(pIn_->load());
    engine_.setOutputGainDb(pOut_->load());
    engine_.setMix(pMix_->load() * 0.01f);

    midi_.clear();
    for (const auto meta : midi) {
        if (midi_.size() >= midi_.capacity()) break;  // never allocate on the audio thread
        const auto msg = meta.getMessage();
        if (msg.getRawDataSize() < 1 || msg.getRawDataSize() > 3) continue;
        const auto* d = msg.getRawData();
        midi_.push_back({meta.samplePosition, d[0], msg.getRawDataSize() > 1 ? d[1] : uint8_t(0), msg.getRawDataSize() > 2 ? d[2] : uint8_t(0)});
    }
    midi.clear();

    ProcessContext ctx;
    ctx.sampleRate = getSampleRate();
    if (auto* ph = getPlayHead()) {
        if (const auto pos = ph->getPosition()) {
            if (const auto bpm = pos->getBpm()) ctx.bpm = *bpm;
            if (const auto ppq = pos->getPpqPosition()) { ctx.ppqPosition = *ppq; ctx.ppqValid = true; }
            ctx.hostPlaying = pos->getIsPlaying();
        }
    }
    ctx.midi = midi_.data();
    ctx.numMidi = static_cast<int>(midi_.size());

    float* L = buffer.getWritePointer(0);
    if (outCh >= 2) {
        engine_.process(L, buffer.getWritePointer(1), n, ctx);
    } else {
        // mono: process as dual-mono and fold back (sum / 2 keeps unity for centred material)
        int done = 0;
        while (done < n) {
            const int m = std::min(n - done, static_cast<int>(monoScratch_.size()));
            float* R = monoScratch_.data();
            std::copy(L + done, L + done + m, R);
            engine_.process(L + done, R, m, ctx);
            for (int i = 0; i < m; ++i) L[done + i] = 0.5f * (L[done + i] + R[i]);
            done += m;
        }
    }
}

// ---- message-thread model operations ----------------------------------------------------------

void Processor::afterModelChange() {
    const int lat = session_->latencySamples();
    if (lat != reportedLatency_) {
        reportedLatency_ = lat;
        setLatencySamples(lat);
    }
    tail_.store(session_->tailSeconds());
    revision_.fetch_add(1);
}

void Processor::syncMacrosFromDraft() {
    const int sel = session_->selected();
    for (int k = 0; k < kNumMacros; ++k) {
        float norm = 0.0f;
        if (sel >= 0) {
            const auto& d = fxRegistry()[static_cast<size_t>(sel)];
            if (k < static_cast<int>(d.params.size())) norm = d.params[static_cast<size_t>(k)].toNormalised(session_->draft(sel)[static_cast<size_t>(k)]);
        }
        lastMacro_[static_cast<size_t>(k)] = norm;
        macros_[static_cast<size_t>(k)]->setValueNotifyingHost(norm);
    }
}

void Processor::uiSelect(int typeIndex) {
    const juce::ScopedLock sl(lock_);
    session_->select(typeIndex);
    if (typeIndex < 0) {
        apvts_.getParameter("solo")->setValueNotifyingHost(0.0f);
        apvts_.getParameter("mute")->setValueNotifyingHost(0.0f);
    }
    syncMacrosFromDraft();
    afterModelChange();
}

void Processor::uiStep(int direction) {
    const juce::ScopedLock sl(lock_);
    session_->step(direction);
    syncMacrosFromDraft();
    afterModelChange();
}

void Processor::uiSetParam(int paramIndex, float plainValue) {
    const juce::ScopedLock sl(lock_);
    const int sel = session_->selected();
    if (sel < 0) return;
    session_->setDraftParam(paramIndex, plainValue);
    if (paramIndex >= 0 && paramIndex < kNumMacros) {
        const auto& spec = fxRegistry()[static_cast<size_t>(sel)].params[static_cast<size_t>(paramIndex)];
        const float norm = spec.toNormalised(session_->draft(sel)[static_cast<size_t>(paramIndex)]);
        lastMacro_[static_cast<size_t>(paramIndex)] = norm;
        macros_[static_cast<size_t>(paramIndex)]->setValueNotifyingHost(norm);
    }
    revision_.fetch_add(1);
}

void Processor::uiGesture(int paramIndex, bool begin) {
    if (paramIndex < 0 || paramIndex >= kNumMacros) return;
    auto* p = macros_[static_cast<size_t>(paramIndex)];
    if (begin) p->beginChangeGesture();
    else p->endChangeGesture();
}

void Processor::uiReset() {
    const juce::ScopedLock sl(lock_);
    session_->resetDraft();
    syncMacrosFromDraft();
    revision_.fetch_add(1);
}

bool Processor::uiCommit() {
    const juce::ScopedLock sl(lock_);
    const bool ok = session_->commit();
    if (ok) {
        apvts_.getParameter("solo")->setValueNotifyingHost(0.0f);
        apvts_.getParameter("mute")->setValueNotifyingHost(0.0f);
        lastSolo_ = lastMute_ = false;
        syncMacrosFromDraft();
        afterModelChange();
    }
    return ok;
}

bool Processor::uiMove(int from, int to) {
    const juce::ScopedLock sl(lock_);
    const bool ok = session_->move(from, to);
    afterModelChange();
    return ok;
}

bool Processor::uiRemove(uint64_t id) {
    const juce::ScopedLock sl(lock_);
    const bool ok = session_->remove(id);
    afterModelChange();
    return ok;
}

void Processor::uiClearChain() {
    const juce::ScopedLock sl(lock_);
    session_->clearChain();
    afterModelChange();
}

void Processor::uiSetSolo(bool on) {
    const juce::ScopedLock sl(lock_);
    if (on && session_->selected() < 0) on = false;
    auto* s = apvts_.getParameter("solo");
    s->beginChangeGesture();
    s->setValueNotifyingHost(on ? 1.0f : 0.0f);
    s->endChangeGesture();
    if (on) apvts_.getParameter("mute")->setValueNotifyingHost(0.0f);
    session_->setSolo(on);
    lastSolo_ = on;
    lastMute_ = session_->mute();
    revision_.fetch_add(1);
}

void Processor::uiSetMute(bool on) {
    const juce::ScopedLock sl(lock_);
    if (on && session_->selected() < 0) on = false;
    auto* m = apvts_.getParameter("mute");
    m->beginChangeGesture();
    m->setValueNotifyingHost(on ? 1.0f : 0.0f);
    m->endChangeGesture();
    if (on) apvts_.getParameter("solo")->setValueNotifyingHost(0.0f);
    session_->setMute(on);
    lastMute_ = on;
    lastSolo_ = session_->solo();
    revision_.fetch_add(1);
}

void Processor::timerCallback() {
    const juce::ScopedLock sl(lock_);
    session_->service();
    // host automation of SOLO / MUTE -> session. Exclusive: the one that switched on last wins.
    const bool solo = pSolo_->load() > 0.5f, mute = pMute_->load() > 0.5f;
    if (solo != lastSolo_ || mute != lastMute_) {
        bool s = solo, m = mute;
        if (s && m) { if (!lastSolo_) m = false; else s = false; }
        if (session_->selected() < 0) s = m = false;  // nothing staged: nothing to solo or mute
        if (s != solo) apvts_.getParameter("solo")->setValueNotifyingHost(s ? 1.0f : 0.0f);
        if (m != mute) apvts_.getParameter("mute")->setValueNotifyingHost(m ? 1.0f : 0.0f);
        session_->setSolo(s);
        session_->setMute(m);
        lastSolo_ = s;
        lastMute_ = m;
        revision_.fetch_add(1);
    }
    // host automation of the staged macros -> staged draft (control rate, smoothed inside the FX)
    const int sel = session_->selected();
    if (sel >= 0) {
        const auto& d = fxRegistry()[static_cast<size_t>(sel)];
        for (int k = 0; k < kNumMacros && k < static_cast<int>(d.params.size()); ++k) {
            const float v = macros_[static_cast<size_t>(k)]->getValue();
            if (std::fabs(v - lastMacro_[static_cast<size_t>(k)]) > 1e-5f) {
                lastMacro_[static_cast<size_t>(k)] = v;
                session_->setDraftParam(k, d.params[static_cast<size_t>(k)].fromNormalised(v));
                revision_.fetch_add(1);
            }
        }
    }
    afterModelChange();
}

// ---- state ------------------------------------------------------------------------------------

void Processor::getStateInformation(juce::MemoryBlock& dest) {
    const juce::ScopedLock sl(lock_);
    json::Value root;
    std::string err;
    json::parse(session_->toJson(true), root, &err);
    json::Value plug = json::Value::object();
    for (const char* id : {"bypass", "in_gain", "out_gain", "mix"})
        plug.set(id, json::Value(static_cast<double>(apvts_.getRawParameterValue(id)->load())));
    root.set("plugin", std::move(plug));
    const std::string text = root.dump(0);
    juce::MemoryOutputStream out(dest, false);
    out.write(kStateMagic, std::strlen(kStateMagic));
    out.write(text.data(), text.size());
}

void Processor::setStateInformation(const void* data, int size) {
    if (!data || size <= static_cast<int>(std::strlen(kStateMagic))) return;
    const char* c = static_cast<const char*>(data);
    if (std::memcmp(c, kStateMagic, std::strlen(kStateMagic)) != 0) return;  // unknown/corrupt: keep current state
    const std::string text(c + std::strlen(kStateMagic), static_cast<size_t>(size) - std::strlen(kStateMagic));
    const juce::ScopedLock sl(lock_);
    std::string err;
    if (!session_->fromJson(text, true, &err)) return;
    json::Value root;
    if (json::parse(text, root, &err)) {
        const auto& plug = root.get("plugin");
        for (const char* id : {"bypass", "in_gain", "out_gain", "mix"}) {
            const auto& v = plug.get(id);
            if (!v.isNumber()) continue;
            auto* p = apvts_.getParameter(id);
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(v.asNumber())));
        }
    }
    apvts_.getParameter("solo")->setValueNotifyingHost(session_->solo() ? 1.0f : 0.0f);
    apvts_.getParameter("mute")->setValueNotifyingHost(session_->mute() ? 1.0f : 0.0f);
    lastSolo_ = session_->solo();
    lastMute_ = session_->mute();
    syncMacrosFromDraft();
    afterModelChange();
}

bool Processor::savePreset(const juce::File& file) {
    std::string text;
    {
        const juce::ScopedLock sl(lock_);
        text = session_->toJson(false);
    }
    return file.replaceWithText(juce::String::fromUTF8(text.data(), static_cast<int>(text.size())));
}

bool Processor::loadPreset(const juce::File& file, juce::String& message) {
    if (!file.existsAsFile() || file.getSize() > (8 << 20)) { message = "Preset file not found or too large."; return false; }
    const auto text = file.loadFileAsString().toStdString();
    const juce::ScopedLock sl(lock_);
    std::string err;
    const bool ok = session_->fromJson(text, false, &err);
    message = juce::String(err);
    afterModelChange();
    return ok;
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor(*this); }

}  // namespace ih1::plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ih1::plugin::Processor(); }
