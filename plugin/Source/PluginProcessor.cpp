#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "ih1/model/Json.h"
#include "ih1/presets/Presets.h"

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
            if (const auto bpm = pos->getBpm()) { ctx.bpm = *bpm; hostBpm_.store(*bpm); }
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
        uiEditMs_[static_cast<size_t>(paramIndex)] = juce::Time::getMillisecondCounterHiRes();
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

uint64_t Processor::uiCommit() {
    const juce::ScopedLock sl(lock_);
    const bool ok = session_->commit();
    if (!ok) return 0;
    apvts_.getParameter("solo")->setValueNotifyingHost(0.0f);
    apvts_.getParameter("mute")->setValueNotifyingHost(0.0f);
    lastSolo_ = lastMute_ = false;
    syncMacrosFromDraft();
    afterModelChange();
    return session_->chain().back().id();
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

void Processor::pullHostParams(bool enforceExclusive) {
    // SOLO / MUTE: exclusive; the one switched on last wins. With nothing staged the engine ignores both.
    const bool solo = pSolo_->load() > 0.5f, mute = pMute_->load() > 0.5f;
    if (solo != lastSolo_ || mute != lastMute_) {
        bool s = solo, m = mute;
        if (s && m) { if (!lastSolo_) m = false; else s = false; }
        if (enforceExclusive) {
            if (s != solo) apvts_.getParameter("solo")->setValueNotifyingHost(s ? 1.0f : 0.0f);
            if (m != mute) apvts_.getParameter("mute")->setValueNotifyingHost(m ? 1.0f : 0.0f);
        }
        session_->setSolo(s);
        session_->setMute(m);
        lastSolo_ = solo;
        lastMute_ = mute;
        if (enforceExclusive) { lastSolo_ = s; lastMute_ = m; }
        revision_.fetch_add(1);
    }
    // staged macros -> staged draft (host automation; control rate, smoothed inside the FX)
    const int sel = session_->selected();
    if (sel >= 0) {
        const auto& d = fxRegistry()[static_cast<size_t>(sel)];
        for (int k = 0; k < kNumMacros && k < static_cast<int>(d.params.size()); ++k) {
            const float v = macros_[static_cast<size_t>(k)]->getValue();
            // A UI edit owns this macro briefly: a host that echoes (or replays a stale value of) the
            // parameter we just set must not overwrite the draft. In V1 that echo could revert a dropdown
            // choice right after it was made.
            if (juce::Time::getMillisecondCounterHiRes() - uiEditMs_[static_cast<size_t>(k)] < 400.0) {
                lastMacro_[static_cast<size_t>(k)] = v;
                continue;
            }
            if (std::fabs(v - lastMacro_[static_cast<size_t>(k)]) > 1e-5f) {
                lastMacro_[static_cast<size_t>(k)] = v;
                session_->setDraftParam(k, d.params[static_cast<size_t>(k)].fromNormalised(v));
                revision_.fetch_add(1);
            }
        }
    }
}

void Processor::timerCallback() {
    const juce::ScopedLock sl(lock_);
    session_->service();
    pullHostParams(true);
    afterModelChange();
}

// ---- state ------------------------------------------------------------------------------------

void Processor::getStateInformation(juce::MemoryBlock& dest) {
    const juce::ScopedLock sl(lock_);
    pullHostParams(false);  // a save straight after automation must include it
    json::Value root;
    std::string err;
    json::parse(session_->toJson(true), root, &err);
    json::Value plug = json::Value::object();
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(prm))
            plug.set(rp->getParameterID().toStdString(), json::Value(static_cast<double>(rp->getValue())));  // normalised
    root.set("plugin", std::move(plug));
    json::Value pref = json::Value::object();
    pref.set("key", presetKey_.toStdString());
    pref.set("name", presetName_.toStdString());
    pref.set("sig", presetSig_);
    root.set("presetRef", std::move(pref));
    json::Value ed = json::Value::object();
    ed.set("w", editorW_.load());
    ed.set("h", editorH_.load());
    root.set("editor", std::move(ed));
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
    syncMacrosFromDraft();
    json::Value root;
    if (json::parse(text, root, &err)) {
        // host parameters come back exactly as saved (normalised values)
        const auto& plug = root.get("plugin");
        for (auto* prm : getParameters()) {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*>(prm);
            if (!rp) continue;
            const auto& v = plug.get(rp->getParameterID().toStdString());
            if (v.isNumber()) rp->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, static_cast<float>(v.asNumber())));
        }
    }
    if (json::parse(text, root, &err)) {
        const auto& pref = root.get("presetRef");
        presetKey_ = juce::String::fromUTF8(pref.get("key").asString().c_str());
        presetName_ = juce::String::fromUTF8(pref.get("name").asString().c_str());
        presetSig_ = pref.get("sig").asString();
        const auto& ed = root.get("editor");
        const int w = static_cast<int>(ed.get("w").asNumber(0)), h = static_cast<int>(ed.get("h").asNumber(0));
        if (w >= 600 && w <= 8000 && h >= 400 && h <= 8000) { editorW_.store(w); editorH_.store(h); }
    }
    for (int k = 0; k < kNumMacros; ++k) lastMacro_[static_cast<size_t>(k)] = macros_[static_cast<size_t>(k)]->getValue();
    lastSolo_ = pSolo_->load() > 0.5f;
    lastMute_ = pMute_->load() > 0.5f;
    afterModelChange();
}

// ---- presets --------------------------------------------------------------------------------

juce::File Processor::userPresetDir() {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("IH1").getChildFile("Presets");
}

std::string Processor::chainSignature() const {
    std::string sig;
    for (const auto& ci : session_->chain()) {
        sig += ci.descriptor().typeId + "{";
        char buf[32];
        for (size_t i = 0; i < ci.descriptor().params.size(); ++i) { std::snprintf(buf, sizeof buf, "%.5g,", static_cast<double>(ci.values()[i])); sig += buf; }
        sig += "}";
    }
    return sig;
}

bool Processor::presetModified() {
    if (presetKey_.isEmpty()) return false;
    const juce::ScopedLock sl(lock_);
    return chainSignature() != presetSig_;
}

namespace {
struct UserMeta { juce::String name, description; };
UserMeta readUserMeta(const juce::File& f) {
    UserMeta m{f.getFileNameWithoutExtension(), {}};
    if (f.getSize() > (8 << 20)) return m;
    ih1::json::Value root;
    std::string err;
    if (ih1::json::parse(f.loadFileAsString().toStdString(), root, &err)) {
        const auto& p = root.get("preset");
        if (p.get("name").isString() && !p.get("name").asString().empty()) m.name = juce::String::fromUTF8(p.get("name").asString().c_str());
        if (p.get("description").isString()) m.description = juce::String::fromUTF8(p.get("description").asString().c_str());
    }
    return m;
}
juce::String safeFileName(const juce::String& name) {
    auto n = juce::File::createLegalFileName(name.trim()).trim();
    return n.isEmpty() ? juce::String("Preset") : n;
}
}  // namespace

std::vector<Processor::PresetEntry> Processor::presetList() {
    std::vector<PresetEntry> v;
    for (const auto& p : factoryPresets())
        v.push_back({"f:" + juce::String(p.id), juce::String::fromUTF8(p.name.c_str()), juce::String::fromUTF8(p.category.c_str()), juce::String::fromUTF8(p.description.c_str()), true});
    auto files = userPresetDir().findChildFiles(juce::File::findFiles, false, "*.ih1chain");
    std::vector<PresetEntry> user;
    for (const auto& f : files) {
        const auto m = readUserMeta(f);
        user.push_back({"u:" + f.getFullPathName(), m.name, "User", m.description, false});
    }
    std::sort(user.begin(), user.end(), [](const PresetEntry& a, const PresetEntry& b) { return a.name.compareNatural(b.name) < 0; });
    v.insert(v.end(), user.begin(), user.end());
    return v;
}

bool Processor::applyPresetJson(const std::string& text, const juce::String& key, const juce::String& name, juce::String& message) {
    const juce::ScopedLock sl(lock_);
    std::string err;
    // Session::fromJson validates everything first and swaps the whole chain in one program hand-over;
    // on failure nothing changes and the preset name is not updated.
    const bool ok = session_->fromJson(text, false, &err);
    message = juce::String(err);
    if (ok) {
        presetKey_ = key;
        presetName_ = name;
        presetSig_ = chainSignature();
    }
    afterModelChange();
    return ok;
}

bool Processor::loadPresetKey(const juce::String& key, juce::String& message) {
    if (key.startsWith("f:")) {
        const int i = factoryPresetIndex(key.substring(2).toStdString());
        if (i < 0) { message = "Unknown factory preset."; return false; }
        const auto& p = factoryPresets()[static_cast<size_t>(i)];
        return applyPresetJson(presetToJson(p), key, juce::String::fromUTF8(p.name.c_str()), message);
    }
    if (key.startsWith("u:")) {
        const juce::File f(key.substring(2));
        if (!f.existsAsFile() || f.getSize() > (8 << 20)) { message = "Preset file not found or too large."; return false; }
        return applyPresetJson(f.loadFileAsString().toStdString(), key, readUserMeta(f).name, message);
    }
    message = "No preset selected.";
    return false;
}

bool Processor::stepPreset(int direction, juce::String& message) {
    const auto list = presetList();
    if (list.empty()) { message = "No presets."; return false; }
    int cur = -1;
    for (size_t i = 0; i < list.size(); ++i) if (list[i].key == presetKey_) cur = static_cast<int>(i);
    const int n = static_cast<int>(list.size());
    // from "no preset", next = first and previous = last; otherwise wrap around the whole list
    const int next = cur < 0 ? (direction >= 0 ? 0 : n - 1) : ((cur + (direction >= 0 ? 1 : -1)) % n + n) % n;
    return loadPresetKey(list[static_cast<size_t>(next)].key, message);
}

bool Processor::saveUserPreset(const juce::String& name, bool overwrite, juce::String& message, bool& exists) {
    const auto dir = userPresetDir();
    if (!dir.createDirectory()) { message = "Cannot create " + dir.getFullPathName(); return false; }
    const auto file = dir.getChildFile(safeFileName(name) + ".ih1chain");
    exists = file.existsAsFile();
    if (exists && !overwrite) { message = "A preset with that name exists."; return false; }
    std::string text, sig;
    {
        const juce::ScopedLock sl(lock_);
        text = session_->toJson(false);
        sig = chainSignature();
    }
    ih1::json::Value root;
    std::string err;
    if (ih1::json::parse(text, root, &err)) {
        ih1::json::Value meta = ih1::json::Value::object();
        meta.set("name", name.trim().toStdString());
        meta.set("category", "User");
        meta.set("description", "");
        root.set("preset", std::move(meta));
        text = root.dump(1);
    }
    // write to a temporary file and move it into place: a failed write never corrupts an existing preset
    const auto tmp = file.getSiblingFile(file.getFileName() + ".tmp");
    if (!tmp.replaceWithText(juce::String::fromUTF8(text.data(), static_cast<int>(text.size()))) || !tmp.moveFileTo(file)) {
        tmp.deleteFile();
        message = "Could not write " + file.getFullPathName();
        return false;
    }
    const juce::ScopedLock sl(lock_);
    presetKey_ = "u:" + file.getFullPathName();
    presetName_ = name.trim();
    presetSig_ = sig;
    revision_.fetch_add(1);
    return true;
}

bool Processor::renameUserPreset(const juce::String& key, const juce::String& newName, juce::String& message) {
    if (!key.startsWith("u:")) { message = "Factory presets are read-only."; return false; }
    const juce::File f(key.substring(2));
    const auto target = f.getSiblingFile(safeFileName(newName) + ".ih1chain");
    if (target != f && target.existsAsFile()) { message = "A preset with that name exists."; return false; }
    ih1::json::Value root;
    std::string err;
    if (!ih1::json::parse(f.loadFileAsString().toStdString(), root, &err)) { message = "That preset file is damaged."; return false; }
    ih1::json::Value meta = root.get("preset").isObject() ? root.get("preset") : ih1::json::Value::object();
    meta.set("name", newName.trim().toStdString());
    root.set("preset", std::move(meta));
    const auto text = root.dump(1);
    if (!target.replaceWithText(juce::String::fromUTF8(text.data(), static_cast<int>(text.size())))) { message = "Could not write the preset."; return false; }
    if (target != f) f.deleteFile();
    if (presetKey_ == key) { presetKey_ = "u:" + target.getFullPathName(); presetName_ = newName.trim(); revision_.fetch_add(1); }
    return true;
}

bool Processor::deleteUserPreset(const juce::String& key, juce::String& message) {
    if (!key.startsWith("u:")) { message = "Factory presets are read-only."; return false; }
    const juce::File f(key.substring(2));
    if (!f.isAChildOf(userPresetDir()) || !f.deleteFile()) { message = "Could not delete the preset."; return false; }
    if (presetKey_ == key) { presetKey_.clear(); presetName_.clear(); presetSig_.clear(); revision_.fetch_add(1); }
    return true;
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor(*this); }

}  // namespace ih1::plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ih1::plugin::Processor(); }
