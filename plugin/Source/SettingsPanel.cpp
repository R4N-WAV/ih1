#include "SettingsPanel.h"

#include "Ih1LookAndFeel.h"
#include "PluginProcessor.h"

namespace ih1::plugin {

namespace {
constexpr int kCellW = 96, kCellH = 108;
}

SettingsPanel::SettingsPanel(Processor& p) : proc_(p) {
    addAndMakeVisible(viewport_);
    viewport_.setViewedComponent(&content_, false);
    viewport_.setScrollBarsShown(true, false);
    viewport_.setScrollBarThickness(6);

    solo_.setClickingTogglesState(false);
    mute_.setClickingTogglesState(false);
    solo_.getProperties().set("activeColour", Colours::accent.toString());
    mute_.getProperties().set("activeColour", Colours::accent2.toString());
    solo_.setTooltip("SOLO FX: hear only the staged effect. The committed chain fades away (and its formation fades to grey) until you turn SOLO off.");
    mute_.setTooltip("MUTE FX: silence only the staged effect so you hear the committed chain on its own. Nothing committed is changed.");
    solo_.onClick = [this] { proc_.uiSetSolo(!solo_.getToggleState()); };
    mute_.onClick = [this] { proc_.uiSetMute(!mute_.getToggleState()); };
    addAndMakeVisible(solo_);
    addAndMakeVisible(mute_);

    more_.setClickingTogglesState(true);
    more_.setTooltip("Show advanced controls for this effect.");
    more_.onClick = [this] { showAdvanced_ = more_.getToggleState(); more_.setButtonText(showAdvanced_ ? "Less" : "More"); layoutControls(); };
    addAndMakeVisible(more_);

    desc_.setFont(LookAndFeel::font(13.0f));
    desc_.setColour(juce::Label::textColourId, Colours::textDim);
    desc_.setJustificationType(juce::Justification::topLeft);
    desc_.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(desc_);

    empty_.setFont(LookAndFeel::font(14.0f));
    empty_.setColour(juce::Label::textColourId, Colours::textDim);
    empty_.setJustificationType(juce::Justification::centred);
    empty_.setText("Choose an effect above to stage it.\nIt plays after your committed chain until you press COMMIT.", juce::dontSendNotification);
    addAndMakeVisible(empty_);
    sync();
}

SettingsPanel::~SettingsPanel() { ctls_.clear(); }

void SettingsPanel::sync() {
    int sel;
    bool solo, mute;
    {
        const juce::ScopedLock sl(proc_.sessionLock());
        sel = proc_.session().selected();
        solo = proc_.session().solo();
        mute = proc_.session().mute();
    }
    if (sel != type_) rebuild(sel);
    refreshValues();
    solo_.setToggleState(solo, juce::dontSendNotification);
    mute_.setToggleState(mute, juce::dontSendNotification);
    solo_.setEnabled(sel >= 0);
    mute_.setEnabled(sel >= 0);
}

void SettingsPanel::rebuild(int typeIndex) {
    type_ = typeIndex;
    ctls_.clear();
    empty_.setVisible(typeIndex < 0);
    desc_.setVisible(typeIndex >= 0);
    more_.setVisible(false);
    if (typeIndex < 0) { layoutControls(); return; }
    const auto& d = fxRegistry()[static_cast<size_t>(typeIndex)];
    desc_.setText(d.tooltip, juce::dontSendNotification);
    bool anyAdvanced = false;
    for (size_t i = 0; i < d.params.size(); ++i) {
        const auto& spec = d.params[i];
        auto c = std::make_unique<Ctl>();
        c->index = static_cast<int>(i);
        c->advanced = spec.advanced;
        anyAdvanced = anyAdvanced || spec.advanced;
        c->label = std::make_unique<juce::Label>(juce::String(), juce::String(spec.name));
        c->label->setFont(LookAndFeel::font(12.5f, true));
        c->label->setColour(juce::Label::textColourId, Colours::text.withAlpha(0.85f));
        c->label->setJustificationType(juce::Justification::centred);
        c->label->setTooltip(spec.tooltip);
        content_.addAndMakeVisible(*c->label);
        const int idx = c->index;
        Ctl* raw = c.get();
        if (spec.kind == ParamKind::Choice) {
            c->combo = std::make_unique<juce::ComboBox>();
            for (size_t k = 0; k < spec.choices.size(); ++k) c->combo->addItem(spec.choices[k], static_cast<int>(k) + 1);
            c->combo->setTooltip(spec.tooltip);
            c->combo->onChange = [this, idx, raw] {
                proc_.uiGesture(idx, true);
                proc_.uiSetParam(idx, static_cast<float>(raw->combo->getSelectedItemIndex()));
                proc_.uiGesture(idx, false);
            };
            content_.addAndMakeVisible(*c->combo);
        } else if (spec.kind == ParamKind::Toggle) {
            c->toggle = std::make_unique<juce::ToggleButton>(juce::String());
            c->toggle->setTooltip(spec.tooltip);
            c->toggle->onClick = [this, idx, raw] {
                proc_.uiGesture(idx, true);
                proc_.uiSetParam(idx, raw->toggle->getToggleState() ? 1.0f : 0.0f);
                proc_.uiGesture(idx, false);
            };
            content_.addAndMakeVisible(*c->toggle);
        } else {
            c->slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            c->slider->setRange(0.0, 1.0, 0.0);
            c->slider->setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            c->slider->setDoubleClickReturnValue(true, spec.toNormalised(spec.defaultValue));
            c->slider->setMouseDragSensitivity(220);
            c->slider->setTooltip(spec.tooltip);
            if (spec.minValue < 0.0f && spec.maxValue > 0.0f) c->slider->getProperties().set("zeroNorm", spec.toNormalised(0.0f));
            c->slider->onDragStart = [this, idx, raw] { raw->dragging = true; proc_.uiGesture(idx, true); };
            c->slider->onDragEnd = [this, idx, raw] { raw->dragging = false; proc_.uiGesture(idx, false); };
            c->slider->onValueChange = [this, idx, raw] {
                const auto& sp = fxRegistry()[static_cast<size_t>(type_)].params[static_cast<size_t>(idx)];
                const float v = sp.fromNormalised(static_cast<float>(raw->slider->getValue()));
                proc_.uiSetParam(idx, v);
                raw->value->setText(sp.format(v), juce::dontSendNotification);
            };
            content_.addAndMakeVisible(*c->slider);
        }
        c->value = std::make_unique<juce::Label>();
        c->value->setFont(LookAndFeel::font(12.0f));
        c->value->setColour(juce::Label::textColourId, Colours::textDim);
        c->value->setJustificationType(juce::Justification::centred);
        c->value->setInterceptsMouseClicks(false, false);
        content_.addAndMakeVisible(*c->value);
        ctls_.push_back(std::move(c));
    }
    more_.setVisible(anyAdvanced);
    refreshValues();
    layoutControls();
}

void SettingsPanel::refreshValues() {
    if (type_ < 0) return;
    ParamValues v;
    {
        const juce::ScopedLock sl(proc_.sessionLock());
        v = proc_.session().draft(type_);
    }
    const auto& d = fxRegistry()[static_cast<size_t>(type_)];
    for (auto& c : ctls_) {
        const auto& spec = d.params[static_cast<size_t>(c->index)];
        const float x = v[static_cast<size_t>(c->index)];
        if (c->slider && !c->dragging) c->slider->setValue(spec.toNormalised(x), juce::dontSendNotification);
        if (c->combo && c->combo->getSelectedItemIndex() != static_cast<int>(x)) c->combo->setSelectedItemIndex(static_cast<int>(x), juce::dontSendNotification);
        if (c->toggle) c->toggle->setToggleState(x >= 0.5f, juce::dontSendNotification);
        if (!c->toggle) c->value->setText(c->combo ? juce::String() : juce::String(spec.format(x)), juce::dontSendNotification);
    }
}

void SettingsPanel::layoutControls() {
    const int w = std::max(kCellW, viewport_.getWidth() - 8);
    const int cols = std::max(1, w / kCellW);
    const int cellW = w / cols;
    int i = 0;
    for (auto& c : ctls_) {
        const bool vis = !c->advanced || showAdvanced_;
        for (juce::Component* comp : {static_cast<juce::Component*>(c->label.get()), static_cast<juce::Component*>(c->value.get()),
                                      static_cast<juce::Component*>(c->slider.get()), static_cast<juce::Component*>(c->combo.get()),
                                      static_cast<juce::Component*>(c->toggle.get())})
            if (comp) comp->setVisible(vis);
        if (!vis) continue;
        const juce::Rectangle<int> cell((i % cols) * cellW, (i / cols) * kCellH, cellW, kCellH);
        auto r = cell.reduced(4, 2);
        c->label->setBounds(r.removeFromTop(18));
        auto ctrl = r.removeFromTop(64);
        if (c->slider) c->slider->setBounds(ctrl.withSizeKeepingCentre(62, 62));
        if (c->combo) c->combo->setBounds(ctrl.withSizeKeepingCentre(std::min(cellW - 10, 120), 26));
        if (c->toggle) c->toggle->setBounds(ctrl.withSizeKeepingCentre(44, 26));
        c->value->setBounds(r.removeFromTop(18));
        ++i;
    }
    const int rows = (i + cols - 1) / cols;
    content_.setSize(w, std::max(rows * kCellH, 10));
}

void SettingsPanel::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(Colours::border.brighter(0.25f));
    g.drawRoundedRectangle(r, 6.0f, 1.6f);
}

void SettingsPanel::resized() {
    auto r = getLocalBounds().reduced(12);
    auto top = r.removeFromTop(64);
    auto sm = top.removeFromRight(30);
    solo_.setBounds(sm.removeFromTop(28));
    sm.removeFromTop(6);
    mute_.setBounds(sm.removeFromTop(28));
    desc_.setBounds(top.withTrimmedRight(8));
    auto bottom = r.removeFromBottom(26);
    more_.setBounds(bottom.removeFromRight(70));
    r.removeFromBottom(4);
    viewport_.setBounds(r);
    empty_.setBounds(getLocalBounds().reduced(24));
    layoutControls();
}

}  // namespace ih1::plugin
