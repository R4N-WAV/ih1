#include "SettingsPanel.h"

#include "Ih1LookAndFeel.h"
#include "PluginProcessor.h"

namespace ih1::plugin {

namespace {
constexpr int kLabelH = 16, kValueH = 16, kSelectorH = 50;
}

SettingsPanel::SettingsPanel(Processor& p) : proc_(p) {
    addAndMakeVisible(viewport_);
    viewport_.setViewedComponent(&content_, false);
    viewport_.setScrollBarsShown(true, false);
    viewport_.setScrollBarThickness(6);
    content_.addAndMakeVisible(graph_);

    for (auto* b : {&solo_, &mute_}) {
        b->getProperties().set("quiet", true);
        b->setClickingTogglesState(false);
        b->setWantsKeyboardFocus(true);
        addAndMakeVisible(*b);
    }
    solo_.getProperties().set("fontSize", 13.0);
    mute_.getProperties().set("fontSize", 13.0);
    solo_.getProperties().set("activeColour", Colours::accent.toString());
    mute_.getProperties().set("activeColour", Colours::accent2.toString());
    solo_.setTooltip("SOLO FX: hear only the staged effect, without the committed chain. Click again to hear everything.");
    mute_.setTooltip("MUTE FX: silence only the staged effect, so you hear the committed chain on its own. Nothing committed is changed.");
    solo_.onClick = [this] { proc_.uiSetSolo(!solo_.getToggleState()); };
    mute_.onClick = [this] { proc_.uiSetMute(!mute_.getToggleState()); };

    desc_.setFont(LookAndFeel::font(13.0f));
    desc_.setColour(juce::Label::textColourId, Colours::textDim);
    desc_.setJustificationType(juce::Justification::topLeft);
    desc_.setMinimumHorizontalScale(1.0f);
    desc_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(desc_);
    advHeading_.setText("Advanced", juce::dontSendNotification);
    advHeading_.setFont(LookAndFeel::font(11.0f, true).withExtraKerningFactor(0.06f));
    advHeading_.setColour(juce::Label::textColourId, Colours::textFaint);
    content_.addChildComponent(advHeading_);
    setVisible(false);
    sync();
}

SettingsPanel::~SettingsPanel() { ctls_.clear(); }

void SettingsPanel::sync() {
    int sel;
    bool solo, mute, standby;
    FxDisplay disp;
    bool hasDisp = false;
    {
        const juce::ScopedLock sl(proc_.sessionLock());
        sel = proc_.session().selected();
        solo = proc_.session().solo();
        mute = proc_.session().mute();
        standby = proc_.session().standby();
        if (const auto* sp = proc_.session().stagedProcessor()) hasDisp = sp->readDisplay(disp);
    }
    if (sel != type_) rebuild(sel);
    refreshValues();
    graph_.setDisplay(hasDisp ? &disp : nullptr);
    solo_.setToggleState(solo, juce::dontSendNotification);
    mute_.setToggleState(mute, juce::dontSendNotification);
    // V5 Phase 7: after COMMIT the fresh copy is muted ("standby"). It is shown muted (MUTE lit, controls dimmed,
    // a plain note in place of the description) but stays editable: editing a control or pressing M auditions it.
    const bool hardMute = mute && !standby;
    if (hardMute != muted_ || standby != standby_) {
        muted_ = hardMute;
        standby_ = standby;
        desc_.setText(standby_ ? juce::String("Committed. This fresh copy is muted (M), so you hear the committed one alone. Touch a control or press M to audition it.")
                               : descText_, juce::dontSendNotification);
        desc_.setColour(juce::Label::textColourId, standby_ ? Colours::accent : Colours::textDim);
        refreshValues();
        repaint();
    }
}

juce::Component* SettingsPanel::controlFor(int paramIndex) const {
    for (const auto& c : ctls_)
        if (c->index == paramIndex) {
            if (c->drop) return c->drop.get();
            if (c->seg) return c->seg.get();
            if (c->toggle) return c->toggle.get();
            return c->slider.get();
        }
    return nullptr;
}

void SettingsPanel::setFromUi(int index, float plain) { proc_.uiSetParam(index, plain); }

void SettingsPanel::rebuild(int typeIndex) {
    type_ = typeIndex;
    ctls_.clear();
    graph_.setType(typeIndex);
    if (typeIndex < 0) { showGraph_ = false; layoutControls(); return; }
    const auto& d = fxRegistry()[static_cast<size_t>(typeIndex)];
    const auto accent = readable(fxColourForIndex(typeIndex), 0.8f);
    showGraph_ = FxGraph::hasGraph(d.typeId);
    graph_.setVisible(showGraph_);
    descText_ = d.tooltip;
    if (!standby_) desc_.setText(descText_, juce::dontSendNotification);
    for (size_t i = 0; i < d.params.size(); ++i) {
        const auto& spec = d.params[i];
        auto c = std::make_unique<Ctl>();
        c->index = static_cast<int>(i);
        c->advanced = spec.advanced;
        c->tooltip = spec.tooltip;
        c->label = std::make_unique<juce::Label>(juce::String(), juce::String(spec.name));
        c->label->setFont(LookAndFeel::font(12.0f, true));
        c->label->setColour(juce::Label::textColourId, Colours::text.withAlpha(0.82f));
        c->label->setJustificationType(juce::Justification::centredLeft);
        c->label->setTooltip(spec.tooltip);
        content_.addAndMakeVisible(*c->label);
        const int idx = c->index;
        Ctl* raw = c.get();
        if (spec.kind == ParamKind::Choice) {
            juce::StringArray items;
            for (const auto& ch : spec.choices) items.add(ch);
            if (items.size() <= 4 && Segmented::suits(items, 210)) {
                c->seg = std::make_unique<Segmented>();
                c->seg->setItems(items);
                c->seg->setAccent(accent);
                c->seg->setTooltip(spec.tooltip);
                c->seg->onChange = [this, idx](int k) { proc_.uiGesture(idx, true); setFromUi(idx, static_cast<float>(k)); proc_.uiGesture(idx, false); };
                content_.addAndMakeVisible(*c->seg);
            } else {
                c->drop = std::make_unique<Dropdown>();
                c->drop->setItems(items);
                c->drop->setAccent(accent);
                c->drop->setTooltip(spec.tooltip);
                c->drop->onChange = [this, idx](int k) { proc_.uiGesture(idx, true); setFromUi(idx, static_cast<float>(k)); proc_.uiGesture(idx, false); };
                content_.addAndMakeVisible(*c->drop);
            }
            c->label->setJustificationType(juce::Justification::centredLeft);
        } else if (spec.kind == ParamKind::Toggle) {
            c->toggle = std::make_unique<juce::ToggleButton>(juce::String());
            c->toggle->setTooltip(spec.tooltip);
            c->toggle->getProperties().set("arcColour", accent.toString());
            c->toggle->onClick = [this, idx, raw] {
                proc_.uiGesture(idx, true);
                setFromUi(idx, raw->toggle->getToggleState() ? 1.0f : 0.0f);
                proc_.uiGesture(idx, false);
            };
            c->toggle->setClickingTogglesState(true);
            content_.addAndMakeVisible(*c->toggle);
        } else {
            c->slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            c->slider->setRange(0.0, 1.0, 0.0);
            c->slider->setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            c->slider->setDoubleClickReturnValue(true, spec.toNormalised(spec.defaultValue));
            c->slider->setMouseDragSensitivity(240);
            c->slider->setVelocityBasedMode(false);
            c->slider->setTooltip(spec.tooltip + "\n\nDrag to change. Double-click to reset; double-click the value to type one.");
            c->slider->getProperties().set("arcColour", accent.toString());
            if (spec.minValue < 0.0f && spec.maxValue > 0.0f) c->slider->getProperties().set("zeroNorm", spec.toNormalised(0.0f));
            c->slider->onDragStart = [this, idx, raw] { raw->dragging = true; proc_.uiGesture(idx, true); };
            c->slider->onDragEnd = [this, idx, raw] { raw->dragging = false; proc_.uiGesture(idx, false); };
            c->slider->onValueChange = [this, idx, raw] {
                const auto& sp = fxRegistry()[static_cast<size_t>(type_)].params[static_cast<size_t>(idx)];
                const float v = sp.fromNormalised(static_cast<float>(raw->slider->getValue()));
                setFromUi(idx, v);
                raw->value->setText(sp.format(v), juce::dontSendNotification);
            };
            content_.addAndMakeVisible(*c->slider);
            c->label->setJustificationType(juce::Justification::centred);
        }
        c->value = std::make_unique<juce::Label>();
        c->value->setFont(LookAndFeel::font(11.5f));
        c->value->setColour(juce::Label::textColourId, Colours::textDim);
        c->value->setColour(juce::Label::backgroundWhenEditingColourId, Colours::panelRaised);
        c->value->setColour(juce::Label::textWhenEditingColourId, Colours::text);
        c->value->setColour(juce::Label::outlineWhenEditingColourId, Colours::accent.withAlpha(0.5f));
        c->value->setJustificationType(juce::Justification::centred);
        if (c->slider) {
            // precise entry: double-click the value, type a number (units optional), Enter
            c->value->setEditable(false, true, false);
            c->value->setTooltip("Double-click to type a value.");
            c->value->onTextChange = [this, idx, raw] {
                const auto& sp = fxRegistry()[static_cast<size_t>(type_)].params[static_cast<size_t>(idx)];
                auto t = raw->value->getText().trim();
                double x = t.retainCharacters("0123456789.-+eE").getDoubleValue();
                if (t.containsIgnoreCase("k") && (sp.unit == "Hz")) x *= 1000.0;
                if (t.endsWithIgnoreCase(" s") && sp.unit == "ms") x *= 1000.0;
                if (std::isfinite(x)) {
                    proc_.uiGesture(idx, true);
                    setFromUi(idx, sp.clamp(static_cast<float>(x)));
                    proc_.uiGesture(idx, false);
                }
                refreshValues();
            };
        } else {
            c->value->setInterceptsMouseClicks(false, false);
        }
        content_.addAndMakeVisible(*c->value);
        ctls_.push_back(std::move(c));
    }
    refreshValues();
    resized();
}

void SettingsPanel::refreshValues() {
    if (type_ < 0) return;
    ParamValues v;
    {
        const juce::ScopedLock sl(proc_.sessionLock());
        v = proc_.session().draft(type_);
    }
    graph_.setValues(v, proc_.hostBpm());
    const auto& d = fxRegistry()[static_cast<size_t>(type_)];
    for (auto& c : ctls_) {
        const auto& spec = d.params[static_cast<size_t>(c->index)];
        const float x = v[static_cast<size_t>(c->index)];
        const int ix = static_cast<int>(std::lround(x));
        if (c->slider && !c->dragging) c->slider->setValue(spec.toNormalised(x), juce::dontSendNotification);
        if (c->drop && c->drop->getSelectedIndex() != ix) c->drop->setSelectedIndex(ix, false);
        if (c->seg && c->seg->getSelectedIndex() != ix) c->seg->setSelectedIndex(ix, false);
        if (c->toggle) c->toggle->setToggleState(x >= 0.5f, juce::dontSendNotification);
        if (c->slider && !c->value->isBeingEdited()) c->value->setText(spec.format(x), juce::dontSendNotification);
    }
    applyEnablement(v, muted_);
}

void SettingsPanel::applyEnablement(const ParamValues& v, bool muted) {
    // Data-driven dependencies (ParamSpec::enabledBy): a disabled JUCE component receives no mouse, wheel or
    // keyboard input, so an inactive control cannot be changed by accident. Its value stays as it was (dormant).
    if (type_ < 0) return;
    const auto& d = fxRegistry()[static_cast<size_t>(type_)];
    for (auto& c : ctls_) {
        const auto& spec = d.params[static_cast<size_t>(c->index)];
        const bool dep = d.paramEnabled(c->index, v);
        const bool active = dep && !muted;
        const juce::String tip = muted ? juce::String("Inactive while the staged effect is muted (M). Unmute to edit it.")
                                       : (dep ? c->tooltip : juce::String(spec.disabledReason));
        for (juce::Component* comp : {static_cast<juce::Component*>(c->slider.get()), static_cast<juce::Component*>(c->drop.get()),
                                      static_cast<juce::Component*>(c->seg.get()), static_cast<juce::Component*>(c->toggle.get()),
                                      static_cast<juce::Component*>(c->value.get())}) {
            if (!comp) continue;
            comp->setEnabled(active);
            comp->setAlpha(active ? (standby_ ? 0.6f : 1.0f) : 0.32f);
            if (auto* tc = dynamic_cast<juce::SettableTooltipClient*>(comp); tc && comp != c->value.get())
                tc->setTooltip(c->slider && dep && !muted ? tip + "\n\nDrag to change. Double-click to reset; double-click the value to type one." : tip);
        }
        c->label->setAlpha(active ? 1.0f : 0.38f);
        c->label->setTooltip(tip);
        if (c->active != active && c->dragging && !active) c->dragging = false;
        c->active = active;
    }
    for (juce::Component* comp : {static_cast<juce::Component*>(&desc_), static_cast<juce::Component*>(&graph_), static_cast<juce::Component*>(&advHeading_)})
        comp->setAlpha(muted ? 0.32f : (standby_ && comp != &desc_ ? 0.6f : 1.0f));
    graph_.setEnabled(!muted);
}

void SettingsPanel::layoutControls() {
    const int W = std::max(160, viewport_.getWidth() - 10);
    int y = 0;
    if (showGraph_) {
        const int gh = juce::jlimit(84, 150, viewport_.getHeight() / 4);
        graph_.setBounds(0, 0, W, gh);
        y = gh + Tokens::space5;
    }
    auto visible = [&](const Ctl&) { return true; };  // V2.1: advanced controls are always shown
    for (auto& c : ctls_)
        for (juce::Component* comp : {static_cast<juce::Component*>(c->label.get()), static_cast<juce::Component*>(c->value.get()),
                                      static_cast<juce::Component*>(c->slider.get()), static_cast<juce::Component*>(c->drop.get()),
                                      static_cast<juce::Component*>(c->seg.get()), static_cast<juce::Component*>(c->toggle.get())})
            if (comp) comp->setVisible(visible(*c));

    auto layoutGroup = [&](bool advanced, int availH) {
        // 1. selectors (choices, toggles): label + control, two per row when there is room
        std::vector<Ctl*> sel, knobs;
        for (auto& c : ctls_) {
            if (c->advanced != advanced || !visible(*c)) continue;
            (c->slider ? knobs : sel).push_back(c.get());
        }
        const int selCols = W >= 330 ? 2 : 1;
        const int selW = (W - (selCols - 1) * Tokens::space5) / selCols;
        for (size_t i = 0; i < sel.size(); ++i) {
            const int col = static_cast<int>(i) % selCols;
            if (col == 0 && i > 0) y += kSelectorH;
            auto cell = juce::Rectangle<int>(col * (selW + Tokens::space5), y, selW, kSelectorH - 8);
            sel[i]->label->setBounds(cell.removeFromTop(kLabelH));
            cell.removeFromTop(3);
            auto ctrl = cell.withHeight(24);
            if (sel[i]->drop) sel[i]->drop->setBounds(ctrl);
            if (sel[i]->seg) sel[i]->seg->setBounds(ctrl);
            if (sel[i]->toggle) sel[i]->toggle->setBounds(ctrl.withWidth(80));
            sel[i]->value->setBounds({});
        }
        if (!sel.empty()) y += kSelectorH + Tokens::space2;
        // 2. knobs: as large as the remaining space allows (52..80 px), never smaller than readable
        if (knobs.empty()) return;
        const int cols = juce::jlimit(2, 5, W / 96);
        const int rows = (static_cast<int>(knobs.size()) + cols - 1) / cols;
        const int cellW = W / cols;
        const int fitD = (availH - y - 12) / std::max(1, rows) - kLabelH - kValueH - 14;
        const int d = juce::jlimit(52, std::min(80, cellW - 16), advanced ? 52 : fitD);
        const int cellH = kLabelH + d + kValueH + 14;
        for (size_t i = 0; i < knobs.size(); ++i) {
            const int col = static_cast<int>(i) % cols, row = static_cast<int>(i) / cols;
            auto cell = juce::Rectangle<int>(col * cellW, y + row * cellH, cellW, cellH);
            knobs[i]->label->setBounds(cell.removeFromTop(kLabelH).reduced(2, 0));
            cell.removeFromTop(4);
            knobs[i]->slider->setBounds(cell.removeFromTop(d).withSizeKeepingCentre(d, d));
            cell.removeFromTop(2);
            knobs[i]->value->setBounds(cell.removeFromTop(kValueH).withSizeKeepingCentre(std::min(cellW, 96), kValueH));
        }
        y += rows * cellH;
    };
    layoutGroup(false, viewport_.getHeight());
    bool anyAdv = false;
    for (auto& c : ctls_) anyAdv = anyAdv || (c->advanced && visible(*c));
    advHeading_.setVisible(anyAdv);
    if (anyAdv) {
        y += Tokens::space5;
        advHeading_.setBounds(0, y, W, 16);
        y += 16 + Tokens::space3;
        layoutGroup(true, 0);
    }
    content_.setSize(W, std::max(y + 6, 10));
}

void SettingsPanel::paint(juce::Graphics&) {}  // no border, no card: spacing and type carry the structure

void SettingsPanel::resized() {
    auto r = getLocalBounds();
    auto top = r.removeFromTop(40);
    auto sm = top.removeFromRight(64);
    mute_.setBounds(sm.removeFromRight(28).withSizeKeepingCentre(28, 26));
    sm.removeFromRight(6);
    solo_.setBounds(sm.removeFromRight(28).withSizeKeepingCentre(28, 26));
    desc_.setBounds(top.withTrimmedRight(10));
    r.removeFromTop(Tokens::space2);
    viewport_.setBounds(r);
    layoutControls();
}

}  // namespace ih1::plugin
