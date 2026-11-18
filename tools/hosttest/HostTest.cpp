// ih1_hosttest: loads the built IH1.vst3 through JUCE's VST3 hosting and runs host-lifecycle checks
// (a project-specific complement to pluginval / the Steinberg validator, which CI runs on Windows).
// Usage: ih1_hosttest <path/to/IH1.vst3>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <cstdio>
#include <atomic>
#include <chrono>
#include <random>
#include <thread>

using namespace juce;

namespace {
int failures = 0;
void check(bool ok, const String& what) {
    std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8());
    if (!ok) ++failures;
}

std::unique_ptr<AudioPluginInstance> load(AudioPluginFormatManager& fm, const String& path, double sr, int block) {
    OwnedArray<PluginDescription> descs;
    for (auto* f : fm.getFormats()) f->findAllTypesForFile(descs, path);
    if (descs.isEmpty()) return nullptr;
    String err;
    auto inst = fm.createPluginInstance(*descs[0], sr, block, err);
    if (!inst) std::printf("load error: %s\n", err.toRawUTF8());
    return inst;
}

// A chain of five committed FX (Delay, Reverb, Harmony, Saturation, Chorus) as an IH1 state blob.
MemoryBlock chainState() {
    const String json = R"({"format":"ih1-state","version":1,"chain":[
      {"id":"00000000000000a1","type":"delay","params":{"sync":0,"time_ms":250,"feedback":40,"mix":40}},
      {"id":"00000000000000a2","type":"reverb","params":{"mode":1,"decay":2.0,"mix":30}},
      {"id":"00000000000000a3","type":"harmony","params":{"mode":0,"voices":1,"int1":7,"int2":-5}},
      {"id":"00000000000000a4","type":"saturation","params":{"drive":12}},
      {"id":"00000000000000a5","type":"chorus","params":{"mix":40}}],
      "session":{"selected":"granular","solo":false,"mute":false}})";
    MemoryBlock raw;
    raw.append("IH1STATE1", 9);
    raw.append(json.toRawUTF8(), json.getNumBytesAsUTF8());
    // a hosted VST3 instance expects JUCE's host-side envelope with the component state inside
    XmlElement env("VST3PluginState");
    env.createNewChildElement("IComponent")->addTextElement(raw.toBase64Encoding());
    MemoryBlock mb;
    AudioProcessor::copyXmlToBinary(env, mb);
    return mb;
}

// The component state (IH1's own JSON) inside a hosted instance's state blob.
String componentJson(AudioPluginInstance& p) {
    MemoryBlock mb;
    p.getStateInformation(mb);
    if (auto xml = AudioProcessor::getXmlFromBinary(mb.getData(), static_cast<int>(mb.getSize())))
        if (auto* c = xml->getChildByName("IComponent")) {
            MemoryBlock raw;
            raw.fromBase64Encoding(c->getAllSubText());
            return String::fromUTF8(static_cast<const char*>(raw.getData()), static_cast<int>(raw.getSize()));
        }
    return {};
}

struct RunResult { bool finite = true; float peak = 0.0f; std::vector<float> left; };

RunResult run(AudioPluginInstance& p, double sr, int block, int numBlocks, uint32_t seed, bool silence = false,
              std::function<void(int)> perBlock = {}) {
    RunResult r;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(-0.4f, 0.4f);
    AudioBuffer<float> buf(2, block);
    MidiBuffer midi;
    double ph = 0.0;
    for (int b = 0; b < numBlocks; ++b) {
        if (perBlock) perBlock(b);
        for (int i = 0; i < block; ++i) {
            const float x = silence ? 0.0f : 0.3f * static_cast<float>(std::sin(ph)) + 0.05f * u(rng);
            ph += 2.0 * MathConstants<double>::pi * 220.0 / sr;
            buf.setSample(0, i, x);
            buf.setSample(1, i, x);
        }
        midi.clear();
        if (b % 40 == 5) midi.addEvent(MidiMessage::noteOn(1, 64 + (b / 40) % 12, 0.8f), 3);
        if (b % 40 == 25) midi.addEvent(MidiMessage::allNotesOff(1), 0);
        p.processBlock(buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i) {
                const float y = buf.getSample(c, i);
                if (!std::isfinite(y)) r.finite = false;
                r.peak = std::max(r.peak, std::fabs(y));
                if (c == 0) r.left.push_back(y);
            }
    }
    return r;
}
}  // namespace

int main(int argc, char** argv) {
    ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf("usage: %s IH1.vst3\n", argv[0]); return 2; }
    const String path = File::getCurrentWorkingDirectory().getChildFile(argv[1]).getFullPathName();
    AudioPluginFormatManager fm;
    addDefaultFormatsToManager(fm);

    // 1. load / describe
    auto p = load(fm, path, 48000.0, 512);
    check(p != nullptr, "plug-in loads from " + path);
    if (!p) return 1;
    check(p->getName() == "IH1", "name is IH1");
    check(p->acceptsMidi(), "accepts MIDI (Harmony MIDI mode)");
    {
        StringArray names;
        for (auto* prm : p->getParameters()) names.add(prm->getName(64));
        bool all = true;
        for (const char* n : {"Bypass", "Solo FX", "Mute FX", "Input Gain", "Output Gain", "Global Mix", "Staged Control 1", "Staged Control 8"})
            all = all && names.contains(n);
        check(all, "exports the fixed IH1 host parameters (plus JUCE's hidden MIDI-CC proxies: " + String(names.size()) + " total)");
    }
    check(p->getBus(true, 0) != nullptr && p->getBus(false, 0) != nullptr, "has main input and output buses");

    // 2. sample rates / block sizes, empty chain is transparent-ish and finite
    for (double sr : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0}) {
        for (int block : {32, 64, 256, 1024}) {
            p->releaseResources();
            p->prepareToPlay(sr, block);
            auto r = run(*p, sr, block, 40, 1);
            check(r.finite && r.peak < 4.0f, "empty chain @" + String(sr) + " Hz / " + String(block) + " finite and bounded");
        }
    }

    // 3. load a committed chain via state and process with odd block sizes + automation storms
    p->releaseResources();
    p->prepareToPlay(48000.0, 512);
    auto st = chainState();
    p->setStateInformation(st.getData(), static_cast<int>(st.getSize()));
    MessageManager::getInstance()->runDispatchLoopUntil(300);  // host learns latency via async restartComponent
    {
        const auto js = componentJson(*p);
        check(js.contains("\"type\":\"harmony\"") && js.contains("00000000000000a5"), "committed chain loaded through the VST3 state path");
    }
    check(p->getLatencySamples() > 0, "latency reported after loading a chain with Harmony (" + String(p->getLatencySamples()) + " samples)");
    std::mt19937 rng(9);
    auto params = p->getParameters();
    for (int block : {512, 17, 1, 300}) {
        auto r = run(*p, 48000.0, block, block == 1 ? 4000 : 200, 3, false, [&](int b) {
            if (b % 7 == 0)
                for (auto* prm : params) {
                    const auto nm = prm->getName(32);
                    // storm the DSP-facing controls; gain staging (+/-24 dB in and out) is checked separately
                    if (nm.containsIgnoreCase("bypass") || nm.containsIgnoreCase("gain")) continue;
                    prm->setValue(std::uniform_real_distribution<float>(0.0f, 1.0f)(rng));
                }
        });
        check(r.finite && r.peak < 4.0f, "chain + automation storm, block " + String(block) + ": finite and bounded (peak " + String(r.peak, 3) + ")");
    }
    for (auto* prm : params) prm->setValue(prm->getDefaultValue());
    {
        // gain staging extremes are caught by the output safety ceiling (+12 dBFS), never NaN/Inf
        for (auto* prm : params) if (prm->getName(32).containsIgnoreCase("gain")) prm->setValue(1.0f);
        auto r = run(*p, 48000.0, 512, 100, 6);
        check(r.finite && r.peak <= 4.0f, "+24 dB in and out: output held at the safety ceiling (peak " + String(r.peak, 3) + ")");
        for (auto* prm : params) prm->setValue(prm->getDefaultValue());
    }

    // 4. silence in -> silence out after tails
    {
        auto r = run(*p, 48000.0, 512, 1600, 4, true);
        float tailPeak = 0.0f;
        for (size_t i = r.left.size() - 48000; i < r.left.size(); ++i) tailPeak = std::max(tailPeak, std::fabs(r.left[i]));
        check(tailPeak < 1e-4f, "silence in -> silence out after tails (" + String(tailPeak, 7) + ")");
    }

    // 5. bypass: output = input delayed by the reported latency
    {
        auto* bypass = p->getBypassParameter();
        check(bypass != nullptr, "exposes a bypass parameter");
        if (bypass) {
            bypass->setValue(1.0f);
            run(*p, 48000.0, 512, 40, 5);  // let the 20 ms ramp settle
            const int lat = p->getLatencySamples();
            AudioBuffer<float> buf(2, 512);
            MidiBuffer midi;
            std::vector<float> in, out;
            for (int b = 0; b < 40; ++b) {
                for (int i = 0; i < 512; ++i) { const float x = static_cast<float>(std::sin(0.01 * (b * 512 + i))) * 0.5f; buf.setSample(0, i, x); buf.setSample(1, i, x); in.push_back(x); }
                p->processBlock(buf, midi);
                for (int i = 0; i < 512; ++i) out.push_back(buf.getSample(0, i));
            }
            double err = 0;
            for (size_t i = static_cast<size_t>(lat) + 10; i < in.size(); ++i) err = std::max(err, static_cast<double>(std::fabs(out[i] - in[i - static_cast<size_t>(lat)])));
            check(err < 1e-4, "bypassed output equals input delayed by latency (err " + String(err, 7) + ")");
            bypass->setValue(0.0f);
        }
    }

    // 6. state round trip into a fresh instance: identical state blob and identical audio (null test)
    {
        MemoryBlock a;
        p->getStateInformation(a);
        auto q = load(fm, path, 48000.0, 512);
        auto r2 = load(fm, path, 48000.0, 512);
        q->prepareToPlay(48000.0, 512);
        r2->prepareToPlay(48000.0, 512);
        q->setStateInformation(a.getData(), static_cast<int>(a.getSize()));
        r2->setStateInformation(a.getData(), static_cast<int>(a.getSize()));
        MessageManager::getInstance()->runDispatchLoopUntil(300);
        MemoryBlock b;
        q->getStateInformation(b);
        check(componentJson(*q).contains("00000000000000a3"), "fresh instance restored the committed chain");
        check(a == b, "state blob round-trips exactly (" + String(static_cast<int>(a.getSize())) + " bytes)");
        auto x = run(*q, 48000.0, 512, 300, 11), y = run(*r2, 48000.0, 512, 300, 11);
        double diff = 0;
        for (size_t i = 0; i < x.left.size(); ++i) diff = std::max(diff, static_cast<double>(std::fabs(x.left[i] - y.left[i])));
        check(diff < 1e-6, "two instances restored from the same state produce identical audio (max diff " + String(diff, 9) + ")");
        check(q->getLatencySamples() == p->getLatencySamples(), "restored instance reports the same latency");
    }

    // 7. hostile state is ignored safely
    {
        MemoryBlock before;
        p->getStateInformation(before);
        const char junk[] = "IH1STATE1{not json at all";
        p->setStateInformation(junk, sizeof(junk));
        p->setStateInformation("garbage", 7);
        MemoryBlock after;
        p->getStateInformation(after);
        check(before == after, "malformed state leaves the current state untouched");
        auto r = run(*p, 48000.0, 512, 20, 12);
        check(r.finite, "still processes after malformed state");
    }

    // 8. repeated create/destroy and re-prepare cycles
    for (int k = 0; k < 5; ++k) {
        auto t = load(fm, path, 44100.0, 256);
        t->prepareToPlay(44100.0, 256);
        t->setStateInformation(st.getData(), static_cast<int>(st.getSize()));
        run(*t, 44100.0, 256, 20, static_cast<uint32_t>(20 + k));
        t->releaseResources();
        t->prepareToPlay(96000.0, 1024);
        run(*t, 96000.0, 1024, 10, static_cast<uint32_t>(30 + k));
    }
    check(true, "5 create / prepare / re-prepare / destroy cycles");

    // 9. editor lifecycle under host-driven resizing (V1 feedback 4.8: FL Studio crashed on fast, dramatic
    //    resizes). The hosted VST3 view is resized through IPlugView::onSize exactly as a host does, 300+
    //    times with random and extreme sizes, while an audio thread keeps processing and the chain changes;
    //    the editor is closed and reopened right after resizes. Set IH1_HOSTTEST_NO_EDITOR=1 to skip.
    if (SystemStats::getEnvironmentVariable("IH1_HOSTTEST_NO_EDITOR", {}).isEmpty()) {
        auto e = load(fm, path, 48000.0, 512);
        e->prepareToPlay(48000.0, 512);
        e->setStateInformation(st.getData(), static_cast<int>(st.getSize()));
        std::atomic<bool> stop{false}, audioOk{true};
        std::atomic<int> blocks{0};
        std::thread audio([&] {
            AudioBuffer<float> buf(2, 512);
            MidiBuffer m;
            std::mt19937 r(5);
            std::uniform_real_distribution<float> u(-0.2f, 0.2f);
            while (!stop.load()) {
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) buf.setSample(c, i, u(r));
                e->processBlock(buf, m);
                for (int i = 0; i < 512; ++i) if (!std::isfinite(buf.getSample(0, i))) audioOk = false;
                ++blocks;
                std::this_thread::sleep_for(std::chrono::milliseconds(4));
            }
        });
        std::unique_ptr<DocumentWindow> win;
        AudioProcessorEditor* ed = nullptr;
        auto openEditor = [&] {
            ed = e->createEditorAndMakeActive();
            if (!ed) return;
            win = std::make_unique<DocumentWindow>("IH1 host test", Colours::black, 0);
            win->setUsingNativeTitleBar(true);
            win->setContentNonOwned(ed, true);
            win->setTopLeftPosition(20, 20);
            win->setVisible(true);
        };
        auto closeEditor = [&] {
            if (win) win->clearContentComponent();
            win.reset();
            delete ed;  // the host owns the editor wrapper
            ed = nullptr;
        };
        openEditor();
        MessageManager::getInstance()->runDispatchLoopUntil(1500);
        check(ed != nullptr, "editor opens in a host window");
        if (ed) {
            const auto w0 = ed->getWidth(), h0 = ed->getHeight();
            check(w0 >= 1400 && h0 >= 850, "first-run editor size is large (" + String(w0) + " x " + String(h0) + ")");
        }
        std::mt19937 rr(77);
        int resizes = 0, reopens = 0;
        const auto t0 = Time::getMillisecondCounterHiRes();
        for (int k = 0; k < 320 && ed != nullptr; ++k) {
            int w, h;
            switch (k % 5) {
                case 0: w = 300 + static_cast<int>(rr() % 4000); h = 200 + static_cast<int>(rr() % 3000); break;  // anything, incl. out of range
                case 1: w = (k % 2) ? 400 : 4000; h = (k % 2) ? 300 : 3000; break;                                 // huge <-> tiny
                case 2: w = 1120 + static_cast<int>(rr() % 40); h = 700 + static_cast<int>(rr() % 40); break;      // near the minimum
                case 3: w = 2400 + static_cast<int>(rr() % 300); h = 700; break;                                   // wide and short
                default: w = 1120; h = 2000 + static_cast<int>(rr() % 300); break;                                 // narrow and tall
            }
            ed->setSize(w, h);
            ++resizes;
            MessageManager::getInstance()->runDispatchLoopUntil(static_cast<int>(rr() % 25));  // often no frame in between
            if (k % 40 == 20) e->setStateInformation(st.getData(), static_cast<int>(st.getSize()));  // chain swap mid-resize
            if (k % 64 == 63) {  // close immediately after a resize, then reopen
                closeEditor();
                MessageManager::getInstance()->runDispatchLoopUntil(30);
                openEditor();
                ++reopens;
                MessageManager::getInstance()->runDispatchLoopUntil(100);
            }
        }
        MessageManager::getInstance()->runDispatchLoopUntil(800);
        const auto secs = (Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        check(ed != nullptr, String(resizes) + " host resizes (" + String(secs, 1) + " s) and " + String(reopens) + " close/reopen cycles survived");
        if (ed) {
            ed->setSize(1700, 1000);
            MessageManager::getInstance()->runDispatchLoopUntil(600);
            check(std::abs(ed->getWidth() - 1700) <= 2 && std::abs(ed->getHeight() - 1000) <= 2, "editor settles at a requested in-range size (" + String(ed->getWidth()) + " x " + String(ed->getHeight()) + ")");
        }
        closeEditor();
        MessageManager::getInstance()->runDispatchLoopUntil(200);
        stop = true;
        audio.join();
        check(audioOk.load() && blocks.load() > 50, "audio kept processing, finite, throughout (" + String(blocks.load()) + " blocks)");
        // repeated open / close cycles with no resizing in between
        for (int k = 0; k < 8; ++k) { openEditor(); MessageManager::getInstance()->runDispatchLoopUntil(120); closeEditor(); }
        check(true, "8 quick editor open / close cycles");
    }

    std::printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    p.reset();
    return failures ? 1 : 0;
}
