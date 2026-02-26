#include "PluginEditor.h"

namespace
{
struct PresetDef
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;
};

static const PresetDef kPresets[] = {
    { "Clean Player", {
        { "clarity", 0.90f }, { "damage", 0.05f }, { "tracking", 0.05f }, { "jitterMacro", 0.05f },
        { "mode", 2.0f }, { "errorRate", 0.03f }, { "burstMs", 10.0f }, { "repeatMs", 28.0f },
        { "scratchRate", 0.05f }, { "scratchAmt", 0.15f }, { "jitterMs", 0.03f }, { "jitterRate", 22.0f },
        { "hfLoss", 0.02f }, { "servoNoise", 0.05f }, { "carComp", 0.12f }, { "softClip", 1.0f }, { "ceiling", 0.96f }, { "outGain", 0.98f }
    } },
    { "Dusty Disc", {
        { "clarity", 0.55f }, { "damage", 0.30f }, { "tracking", 0.22f }, { "jitterMacro", 0.18f },
        { "mode", 0.0f }, { "errorRate", 0.18f }, { "burstMs", 22.0f }, { "repeatMs", 42.0f },
        { "scratchRate", 0.25f }, { "scratchAmt", 0.35f }, { "jitterMs", 0.16f }, { "jitterRate", 38.0f },
        { "hfLoss", 0.12f }, { "servoNoise", 0.12f }, { "carComp", 0.22f }, { "softClip", 1.0f }, { "ceiling", 0.94f }, { "outGain", 0.98f }
    } },
    { "Scratched Hook", {
        { "clarity", 0.28f }, { "damage", 0.75f }, { "tracking", 0.65f }, { "jitterMacro", 0.32f },
        { "mode", 3.0f }, { "errorRate", 0.55f }, { "burstMs", 55.0f }, { "repeatMs", 85.0f },
        { "scratchRate", 0.72f }, { "scratchAmt", 0.65f }, { "jitterMs", 0.35f }, { "jitterRate", 62.0f },
        { "hfLoss", 0.22f }, { "servoNoise", 0.18f }, { "carComp", 0.60f }, { "softClip", 1.0f }, { "ceiling", 0.90f }, { "outGain", 1.03f }
    } },
    { "Portable Skip", {
        { "clarity", 0.34f }, { "damage", 0.35f }, { "tracking", 0.82f }, { "jitterMacro", 0.22f },
        { "mode", 0.0f }, { "errorRate", 0.50f }, { "burstMs", 70.0f }, { "repeatMs", 55.0f },
        { "scratchRate", 0.22f }, { "scratchAmt", 0.25f }, { "jitterMs", 0.18f }, { "jitterRate", 48.0f },
        { "hfLoss", 0.16f }, { "servoNoise", 0.22f }, { "carComp", 0.75f }, { "softClip", 1.0f }, { "ceiling", 0.92f }, { "outGain", 1.05f }
    } },
    { "Car CD Player", {
        { "clarity", 0.46f }, { "damage", 0.22f }, { "tracking", 0.55f }, { "jitterMacro", 0.24f },
        { "mode", 2.0f }, { "errorRate", 0.28f }, { "burstMs", 35.0f }, { "repeatMs", 60.0f },
        { "scratchRate", 0.14f }, { "scratchAmt", 0.22f }, { "jitterMs", 0.22f }, { "jitterRate", 55.0f },
        { "hfLoss", 0.08f }, { "servoNoise", 0.25f }, { "carComp", 0.88f }, { "softClip", 1.0f }, { "ceiling", 0.92f }, { "outGain", 1.02f }
    } },
    { "Rotted Rip", {
        { "clarity", 0.18f }, { "damage", 0.55f }, { "tracking", 0.50f }, { "jitterMacro", 0.55f },
        { "mode", 1.0f }, { "errorRate", 0.62f }, { "burstMs", 90.0f }, { "repeatMs", 120.0f },
        { "scratchRate", 0.35f }, { "scratchAmt", 0.45f }, { "jitterMs", 0.60f }, { "jitterRate", 90.0f },
        { "hfLoss", 0.28f }, { "servoNoise", 0.20f }, { "carComp", 0.50f }, { "softClip", 0.0f }, { "ceiling", 0.88f }, { "outGain", 1.10f }
    } },
};
}

CDEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff9ce7ff));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff9adaff));
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff58a8cf));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2b3238));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff3a3f45));
    addAndMakeVisible(slider);

    attachment = std::make_unique<APVTS::SliderAttachment>(state, paramID, slider);
}

void CDEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(20));
    slider.setBounds(area.reduced(2));
}

CDEngineAudioProcessorEditor::Switch::Switch(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    button.setButtonText(text);
    button.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd2d7dc));
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff9adaff));
    addAndMakeVisible(button);
    attachment = std::make_unique<APVTS::ButtonAttachment>(state, paramID, button);
}

void CDEngineAudioProcessorEditor::Switch::resized()
{
    button.setBounds(getLocalBounds());
}

CDEngineAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff9ce7ff));
    addAndMakeVisible(label);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(paramID)))
        for (int i = 0; i < p->choices.size(); ++i)
            combo.addItem(p->choices[i], i + 1);

    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1f2326));
    combo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3a3f45));
    addAndMakeVisible(combo);
    attachment = std::make_unique<APVTS::ComboBoxAttachment>(state, paramID, combo);
}

void CDEngineAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    combo.setBounds(area.reduced(1));
}

CDEngineAudioProcessorEditor::CDEngineAudioProcessorEditor(CDEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.getAPVTS())
{
    title.setText("CD Engine", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xffdff5ff));
    addAndMakeVisible(title);

    subtitle.setText("Compact Disc Error Rack", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff8aa4b2));
    addAndMakeVisible(subtitle);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ce7ff));
    addAndMakeVisible(presetLabel);

    presetBox.addItem("Custom", 1);
    for (int i = 0; i < (int) std::size(kPresets); ++i)
        presetBox.addItem(kPresets[i].name, i + 2);
    presetBox.onChange = [this]() {
        const auto id = presetBox.getSelectedId();
        if (id >= 2)
            applyPreset(id - 2);
    };
    presetBox.setSelectedId(1, juce::dontSendNotification);
    presetBox.setTooltip("Factory CD artifact profiles for quick setup.");
    addAndMakeVisible(presetBox);

    tabs.setOutline(0);
    tabs.setTabBarDepth(34);
    tabs.addTab("Macro", juce::Colour(0xff2c3640), &macroPage, true);
    tabs.addTab("Errors", juce::Colour(0xff2c3640), &errorsPage, true);
    tabs.addTab("Mechanics", juce::Colour(0xff2c3640), &mechanicsPage, true);
    tabs.addTab("Output", juce::Colour(0xff2c3640), &outputPage, true);
    addAndMakeVisible(tabs);

    addKnob(macroPage, "clarity", "Clarity", "Higher clarity reduces burst errors, hissy artifacts and HF rolloff.");
    addKnob(macroPage, "damage", "Damage", "Raises disc read errors, concealment events and scratch activity.");
    addKnob(macroPage, "tracking", "Tracking", "Pushes anti-skip behavior toward more obvious hold/repeat behavior.");
    addKnob(macroPage, "jitterMacro", "Jitter", "Adds sample-time wobble from transport timing instability.");
    addKnob(macroPage, "carComp", "Car Comp", "Car-stereo style levelling that squashes dynamics and adds density.");

    addChoice(errorsPage, "mode", "Concealment", "How missing sectors are reconstructed: Hold, Mute, Interp or Repeat.");
    addKnob(errorsPage, "errorRate", "Error Rate", "How often the decoder enters a bad-read burst.");
    addKnob(errorsPage, "burstMs", "Burst", "Length of each read-error burst in milliseconds.");
    addKnob(errorsPage, "repeatMs", "Repeat", "How far back repeated data is pulled for repeat mode.");
    addKnob(errorsPage, "scratchRate", "Scratch Rate", "Probability of short transient click events.");
    addKnob(errorsPage, "scratchAmt", "Scratch Amt", "Loudness and bite of scratch transients.");

    addKnob(mechanicsPage, "jitterMs", "Depth", "Maximum timing wander in milliseconds.");
    addKnob(mechanicsPage, "jitterRate", "Rate", "Speed of jitter modulation.");
    addKnob(mechanicsPage, "hfLoss", "HF Loss", "Optical/read path blur that removes top end.");
    addKnob(mechanicsPage, "servoNoise", "Servo", "Subtle spindle/servo whirr and chatter.");

    addSwitch(outputPage, "softClip", "Soft Clip", "Saturate peaks before the output ceiling limiter.");
    addKnob(outputPage, "ceiling", "Ceiling", "Hard ceiling limiter output cap.");
    addKnob(outputPage, "outGain", "Out Gain", "Final output trim.");

    setResizable(false, false);
    setSize(860, 540);

    lastClarity = getParamValue("clarity");
    lastDamage = getParamValue("damage");
    lastTracking = getParamValue("tracking");
    lastJitter = getParamValue("jitterMacro");

    startTimerHz(18);
}

CDEngineAudioProcessorEditor::~CDEngineAudioProcessorEditor() {}

void CDEngineAudioProcessorEditor::addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Knob>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    knobs.push_back(std::move(c));
}

void CDEngineAudioProcessorEditor::addSwitch(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Switch>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    switches.push_back(std::move(c));
}

void CDEngineAudioProcessorEditor::addChoice(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Choice>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    choices.push_back(std::move(c));
}

void CDEngineAudioProcessorEditor::layoutPage(juce::Component& page, int columns)
{
    auto area = page.getLocalBounds().reduced(10);
    auto& items = pageItems[&page];
    if (items.empty())
        return;

    const auto count = (int) items.size();
    const auto cols = juce::jmax(1, columns);
    const auto rows = juce::jmax(1, (count + cols - 1) / cols);
    const auto cellW = area.getWidth() / cols;
    const auto cellH = area.getHeight() / rows;

    for (int i = 0; i < count; ++i)
    {
        const auto r = i / cols;
        const auto c = i % cols;
        auto cell = juce::Rectangle<int>(area.getX() + c * cellW, area.getY() + r * cellH, cellW, cellH).reduced(6);
        items[(size_t) i]->setBounds(cell);
    }
}

void CDEngineAudioProcessorEditor::setParamValue(const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
}

float CDEngineAudioProcessorEditor::getParamValue(const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue(id))
        return v->load();
    return 0.0f;
}

void CDEngineAudioProcessorEditor::applyMacroClarity(float clarity)
{
    applyMacroTargets(clarity, getParamValue("damage"), getParamValue("tracking"), getParamValue("jitterMacro"));
}

void CDEngineAudioProcessorEditor::applyMacroDamage(float damage)
{
    applyMacroTargets(getParamValue("clarity"), damage, getParamValue("tracking"), getParamValue("jitterMacro"));
}

void CDEngineAudioProcessorEditor::applyMacroTracking(float tracking)
{
    applyMacroTargets(getParamValue("clarity"), getParamValue("damage"), tracking, getParamValue("jitterMacro"));
}

void CDEngineAudioProcessorEditor::applyMacroJitter(float jitter)
{
    applyMacroTargets(getParamValue("clarity"), getParamValue("damage"), getParamValue("tracking"), jitter);
}

void CDEngineAudioProcessorEditor::applyMacroTargets(float clarity, float damage, float tracking, float jitter)
{
    const auto c0 = juce::jlimit(0.0f, 1.0f, clarity);
    const auto d0 = juce::jlimit(0.0f, 1.0f, damage);
    const auto t0 = juce::jlimit(0.0f, 1.0f, tracking);
    const auto j0 = juce::jlimit(0.0f, 1.0f, jitter);

    const auto c = std::pow(1.0f - c0, 1.35f);
    const auto d = std::pow(d0, 1.25f);
    const auto t = std::pow(t0, 1.35f);
    const auto j = std::pow(j0, 1.25f);

    const auto errorRate = juce::jlimit(0.0f, 1.0f, 0.02f + c * 0.6f + t * 0.25f);
    const auto burstMs = std::round(8.0f + (c * 60.0f + t * 120.0f) * (0.35f + 0.65f * d));
    const auto repeatMs = std::round(18.0f + t * 120.0f);
    const auto scratchRate = juce::jlimit(0.0f, 1.0f, 0.03f + d * 0.85f);
    const auto scratchAmt = juce::jlimit(0.0f, 1.0f, 0.08f + d * 0.75f);
    const auto jitterMs = std::round((0.02f + j * 0.75f) * 100.0f) / 100.0f;
    const auto jitterRate = std::round(18.0f + j * 85.0f);
    const auto hfLoss = juce::jlimit(0.0f, 1.0f, 0.02f + c * 0.12f + d * 0.22f);
    const auto servoNoise = juce::jlimit(0.0f, 1.0f, 0.03f + t * 0.25f + d * 0.1f);
    const auto ceiling = 0.96f - d * 0.08f;
    const auto outGain = std::round((0.98f + d * 0.12f) * 100.0f) / 100.0f;
    const auto carComp = juce::jlimit(0.0f, 1.0f, t * 0.65f + d * 0.2f);
    const auto mode = errorRate > 0.52f ? 3.0f : (errorRate > 0.25f ? 0.0f : 2.0f);

    setParamValue("mode", mode);
    setParamValue("errorRate", errorRate);
    setParamValue("burstMs", burstMs);
    setParamValue("repeatMs", repeatMs);
    setParamValue("scratchRate", scratchRate);
    setParamValue("scratchAmt", scratchAmt);
    setParamValue("jitterMs", jitterMs);
    setParamValue("jitterRate", jitterRate);
    setParamValue("hfLoss", hfLoss);
    setParamValue("servoNoise", servoNoise);
    setParamValue("ceiling", ceiling);
    setParamValue("outGain", outGain);
    setParamValue("carComp", carComp);
}

void CDEngineAudioProcessorEditor::applyPreset(int idx)
{
    if (idx < 0 || idx >= (int) std::size(kPresets))
        return;

    suppressMacros = true;
    for (const auto& kv : kPresets[(size_t) idx].values)
        setParamValue(kv.first, kv.second);
    suppressMacros = false;

    lastClarity = getParamValue("clarity");
    lastDamage = getParamValue("damage");
    lastTracking = getParamValue("tracking");
    lastJitter = getParamValue("jitterMacro");
}

void CDEngineAudioProcessorEditor::timerCallback()
{
    if (suppressMacros)
        return;

    const auto clarity = getParamValue("clarity");
    const auto damage = getParamValue("damage");
    const auto tracking = getParamValue("tracking");
    const auto jitter = getParamValue("jitterMacro");

    suppressMacros = true;
    if (std::abs(clarity - lastClarity) > 0.0005f)
    {
        applyMacroClarity(clarity);
        lastClarity = clarity;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(damage - lastDamage) > 0.0005f)
    {
        applyMacroDamage(damage);
        lastDamage = damage;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(tracking - lastTracking) > 0.0005f)
    {
        applyMacroTracking(tracking);
        lastTracking = tracking;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(jitter - lastJitter) > 0.0005f)
    {
        applyMacroJitter(jitter);
        lastJitter = jitter;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    suppressMacros = false;
}

void CDEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(juce::Colour(0xff13191f), 0.0f, 0.0f, juce::Colour(0xff202a33), 0.0f, (float) getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();

    auto header = getLocalBounds().removeFromTop(82).reduced(8, 8);
    g.setColour(juce::Colour(0xff1e2730));
    g.fillRoundedRectangle(header.toFloat(), 10.0f);
    g.setColour(juce::Colour(0xff4f5f70));
    g.drawRoundedRectangle(header.toFloat(), 10.0f, 1.2f);

    g.setColour(juce::Colour(0xffa7e8ff));
    g.fillRoundedRectangle((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 278.0f, 48.0f, 8.0f);
    g.setColour(juce::Colour(0xff2b5360));
    g.drawRoundedRectangle(juce::Rectangle<float>((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 278.0f, 48.0f), 8.0f, 1.0f);
}

void CDEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    auto header = area.removeFromTop(74);

    auto left = header.removeFromLeft(390);
    title.setBounds(left.removeFromTop(42).withTrimmedLeft(18));
    subtitle.setBounds(left.withTrimmedLeft(20));

    auto right = header.withTrimmedLeft(26);
    presetLabel.setBounds(right.removeFromTop(18));
    presetBox.setBounds(right.removeFromTop(30).removeFromLeft(230));

    tabs.setBounds(area);

    layoutPage(macroPage, 5);
    layoutPage(errorsPage, 3);
    layoutPage(mechanicsPage, 4);
    layoutPage(outputPage, 3);
}
