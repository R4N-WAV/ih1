#include "PluginEditor.h"

#include "PluginProcessor.h"

namespace ih1::plugin {

juce::Point<int> Editor::defaultSize(juce::Rectangle<int> user) {
    // A wide layout with room for the formation: 1500 x 900 on a typical desktop. On small or high-DPI
    // screens it shrinks to 92 % of the usable area, never below the minimum (hosts can scroll that).
    int w = 1500, h = 900;
    if (!user.isEmpty()) {
        w = std::min(w, static_cast<int>(user.getWidth() * 0.92));
        h = std::min(h, static_cast<int>(user.getHeight() * 0.92));
    }
    return {juce::jlimit(kMinW, kMaxW, w), juce::jlimit(kMinH, kMaxH, h)};
}

Editor::Editor(Processor& p)
    : AudioProcessorEditor(p), proc_(p), renderer_(p, gl_), settings_(p), formation_(p, renderer_), presetBar_(p), browser_(p) {
    const auto last = proc_.lastEditorSize();  // read first: layout below records sizes as it goes
    // The look and feel is set on this editor's component tree only. V1 also made it the global default,
    // which left a dangling global when one of several open IH1 editors closed.
    setLookAndFeel(&lnf_);
    setOpaque(false);

    carousel_.onSelect = [this](int type) { proc_.uiSelect(type); };
    addAndMakeVisible(carousel_);
    addChildComponent(settings_);
    addAndMakeVisible(formation_);
    addAndMakeVisible(presetBar_);

    commit_.getProperties().set("primary", true);
    commit_.getProperties().set("fontSize", 14.0);
    reset_.getProperties().set("fontSize", 13.0);
    commit_.setTooltip("Add the staged effect, with its current settings, to the end of the committed chain. Committed effects can be moved or deleted but never edited. The browser stays on this effect with a fresh copy at its defaults, muted: touch a control or press M to audition the next one.");
    reset_.setTooltip("Return the staged effect to its defaults and clear what it holds (a Looper's recording, echoes, tails). Nothing committed is touched.");
    commit_.onClick = [this] { doCommit(); };
    reset_.onClick = [this] { proc_.uiReset(); };
    addAndMakeVisible(commit_);
    addAndMakeVisible(reset_);

    presetBar_.onOpenBrowser = [this] {
        if (browser_.isOpen()) { browser_.close(); return; }
        browser_.open(getLocalArea(&presetBar_, presetBar_.getLocalBounds()));
    };
    presetBar_.onMessage = [this](const juce::String& m) { if (m.isNotEmpty()) formation_.showToast(m); };
    browser_.onMessage = [this](const juce::String& m) { if (m.isNotEmpty()) formation_.showToast(m); presetBar_.refresh(); };
    addChildComponent(browser_);
    addChildComponent(menu_);  // added last: above everything

    if (juce::SystemStats::getEnvironmentVariable("IH1_NO_OPENGL", {}).isNotEmpty()) {
        formation_.setSoftwareFallback(true);
    } else {
        gl_.setRenderer(&renderer_);
        gl_.setPreferredVersion(juce::OpenGLContext::Version{3, 2});
        gl_.setComponentPaintingEnabled(true);
        gl_.setContinuousRepainting(true);
        gl_.attachTo(*this);
        glAttached_ = true;
    }

    setResizable(true, true);
    setResizeLimits(kMinW, kMinH, kMaxW, kMaxH);
    juce::Rectangle<int> user;
    if (const auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) user = d->userBounds.toNearestInt();
    auto size = defaultSize(user);
    if (last.x >= kMinW && last.y >= kMinH) size = {std::min(last.x, kMaxW), std::min(last.y, kMaxH)};
    setSize(size.x, size.y);
    startTimerHz(30);
}

Editor::~Editor() {
    stopTimer();
    // stop the render thread and free GL resources while every component it reads still exists
    if (gl_.isAttached()) gl_.detach();
    setLookAndFeel(nullptr);
}

void Editor::paint(juce::Graphics& g) {
    // Only the left column is painted here. The field must stay transparent in the component layer: with
    // OpenGL, components are composited over the GL frame, so painting it would hide the formation. The GL
    // renderer clears it to pure black; the software fallback paints it itself.
    g.setColour(Colours::background);
    g.fillRect(left_);
    g.setColour(Colours::text);
    g.setFont(LookAndFeel::font(19.0f, true).withExtraKerningFactor(0.18f));
    g.drawText("IH1", header_.withWidth(64), juce::Justification::centredLeft);
    // build identity, quiet: tells at a glance which build the host has loaded
#if defined(IH1_VERSION_STRING) && defined(IH1_GIT_HASH)
    g.setColour(Colours::textFaint);
    g.setFont(LookAndFeel::font(9.5f));
    g.drawText(juce::String("v") + IH1_VERSION_STRING + " " + IH1_GIT_HASH,
               header_.withWidth(84).withTrimmedTop(header_.getHeight() - 6).translated(0, 8), juce::Justification::centredLeft);
#endif
}

void Editor::resized() {
    const int W = std::max(1, getWidth()), H = std::max(1, getHeight());
    proc_.setLastEditorSize(W, H);
    // V2.1 spacing (Phase 5): a wider column with a deliberate hierarchy -- outer inset 32 px, header to
    // carousel 16, carousel to settings 24, settings to the COMMIT row 24. The column grows a little with
    // the window so the settings area is not squeezed by the larger insets.
    const int lw = juce::jlimit(440, 560, static_cast<int>(W * 0.32));
    left_ = juce::Rectangle<int>(0, 0, lw, H);
    auto r = left_.reduced(Tokens::gutter, Tokens::gutterV);
    header_ = r.removeFromTop(36);
    presetBar_.setBounds(header_.withTrimmedLeft(84));
    r.removeFromTop(Tokens::space4);
    carousel_.setBounds(r.removeFromTop(juce::jlimit(170, 240, H / 4)));
    r.removeFromTop(Tokens::space5);
    auto buttons = r.removeFromBottom(40);
    commit_.setBounds(buttons.removeFromLeft(buttons.getWidth() / 2 - Tokens::space2));
    buttons.removeFromLeft(Tokens::space4);
    reset_.setBounds(buttons);
    r.removeFromBottom(Tokens::space5);
    settings_.setBounds(r);
    const auto field = juce::Rectangle<int>(lw, 0, W - lw, H);
    formation_.setBounds(field);
    renderer_.setArea(formation_.artworkArea().translated(field.getX(), field.getY()), H);
    menu_.setBounds(getLocalBounds());
    if (browser_.isOpen()) browser_.close();
}

void Editor::doCommit() {
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now < commitLockMs_) return;  // a double-click never commits twice
    commitLockMs_ = now + 350.0;
    bool staged;
    {
        const juce::ScopedLock sl(proc_.sessionLock());
        staged = proc_.session().selected() >= 0;
    }
    if (!staged) return;
    if (const uint64_t id = proc_.uiCommit()) {
        // the audio chain has already changed; the transfer animation is UI-only and starts at the
        // workflow column's edge, level with the carousel
        const float originY = static_cast<float>(carousel_.getBounds().getCentreY()) / static_cast<float>(std::max(1, formation_.artworkArea().getHeight()));
        formation_.requestTransfer(id, originY);
    } else {
        formation_.showToast("The chain is full (" + juce::String(kMaxChain) + " effects)");
    }
}

bool Editor::keyPressed(const juce::KeyPress& k) {
    if ((k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown()) && k.getKeyCode() == juce::KeyPress::returnKey) { doCommit(); return true; }
    return false;
}

void Editor::timerCallback() {
    if (glAttached_ && renderer_.glFailed()) {
        // OpenGL 3.2 unavailable or a render target could not be created: software formation, audio unaffected
        gl_.detach();
        glAttached_ = false;
        formation_.setSoftwareFallback(true);
        repaint();
    }
    if (proc_.revision() != seenRevision_) {
        seenRevision_ = proc_.revision();
        int sel, chain;
        {
            const juce::ScopedLock sl(proc_.sessionLock());
            sel = proc_.session().selected();
            chain = static_cast<int>(proc_.session().chain().size());
        }
        bool muted, standby;
        {
            const juce::ScopedLock sl(proc_.sessionLock());
            muted = proc_.session().mute();
            standby = proc_.session().standby();
        }
        carousel_.setSelected(sel);
        // MUTE (V2.1 9.7): the whole staged area is greyed and inactive -- carousel, settings, COMMIT, RESET.
        // Only S / M stay live so the user can unmute. Matches the DSP: the staged FX is silent while muted.
        // V5 Phase 7: after COMMIT the browser stays on the same effect with a fresh, muted copy ("standby"): the
        // carousel and RESET stay usable (browsing / editing is the next step); COMMIT waits for an audition, so a
        // silent copy is never committed by a double press.
        const bool hard = muted && !standby;
        carousel_.setEnabled(!hard);
        carousel_.setAlpha(hard ? 0.35f : 1.0f);
        commit_.setEnabled(sel >= 0 && chain < kMaxChain && !muted);
        reset_.setEnabled(sel >= 0 && !hard);
        commit_.setAlpha(muted ? 0.4f : 1.0f);
        reset_.setAlpha(hard ? 0.4f : 1.0f);
        settings_.setVisible(sel >= 0);  // no empty panel when nothing is staged
        presetBar_.refresh();
    }
    if (settings_.isVisible()) settings_.sync();
}

}  // namespace ih1::plugin
