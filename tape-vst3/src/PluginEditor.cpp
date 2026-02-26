#include "PluginEditor.h"

namespace
{
struct PresetDef
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;
};

static const PresetDef kPresets[] = {
    { "Fresh Cassette", {
        { "quality", 0.72f }, { "age", 0.18f }, { "wow", 0.14f }, { "glitch", 0.06f },
        { "sfxEnable", 1.0f }, { "sfxBank", 0.0f }, { "sfxMode", 0.0f }, { "sfxLevel", 0.18f }
    } },
    { "Worn Cassette", {
        { "quality", 0.32f }, { "age", 0.62f }, { "wow", 0.55f }, { "glitch", 0.35f },
        { "sfxEnable", 1.0f }, { "sfxBank", 0.0f }, { "sfxMode", 1.0f }, { "sfxLevel", 0.30f }
    } },
    { "VHS HiFi", {
        { "quality", 0.78f }, { "age", 0.24f }, { "wow", 0.12f }, { "glitch", 0.08f },
        { "sfxEnable", 1.0f }, { "sfxBank", 1.0f }, { "sfxMode", 0.0f }, { "sfxLevel", 0.16f }
    } },
    { "VHS Linear", {
        { "quality", 0.42f }, { "age", 0.48f }, { "wow", 0.30f }, { "glitch", 0.22f },
        { "sfxEnable", 1.0f }, { "sfxBank", 1.0f }, { "sfxMode", 2.0f }, { "sfxLevel", 0.25f }
    } },
    { "Rewind Melt", {
        { "quality", 0.08f }, { "age", 0.82f }, { "wow", 0.92f }, { "glitch", 0.62f },
        { "speed", 0.93f }, { "sfxEnable", 1.0f }, { "sfxBank", 0.0f }, { "sfxMode", 2.0f }, { "sfxLevel", 0.35f }
    } },
};
}

TapeEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff98ffb4));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff90f3a0));
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4ab36b));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2b3238));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff3a3f45));
    addAndMakeVisible(slider);

    attachment = std::make_unique<APVTS::SliderAttachment>(state, paramID, slider);
}

void TapeEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(20));
    slider.setBounds(area.reduced(2));
}

TapeEngineAudioProcessorEditor::Switch::Switch(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    button.setButtonText(text);
    button.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd2d7dc));
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff90f3a0));
    addAndMakeVisible(button);
    attachment = std::make_unique<APVTS::ButtonAttachment>(state, paramID, button);
}

void TapeEngineAudioProcessorEditor::Switch::resized()
{
    button.setBounds(getLocalBounds());
}

TapeEngineAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff98ffb4));
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

void TapeEngineAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    combo.setBounds(area.reduced(1));
}

TapeEngineAudioProcessorEditor::TapeEngineAudioProcessorEditor(TapeEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.getAPVTS())
{
    title.setText("Tape Engine", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xffdfffe4));
    addAndMakeVisible(title);

    subtitle.setText("Compact Deck Console", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff8aa79a));
    addAndMakeVisible(subtitle);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff98ffb4));
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
    presetBox.setTooltip("Factory tape profiles for quick starting points.");
    addAndMakeVisible(presetBox);

    tabs.setOutline(0);
    tabs.setTabBarDepth(34);
    tabs.addTab("Macro", juce::Colour(0xff2c3640), &macroPage, true);
    tabs.addTab("Tone", juce::Colour(0xff2c3640), &tonePage, true);
    tabs.addTab("Mechanics", juce::Colour(0xff2c3640), &mechanicsPage, true);
    tabs.addTab("SFX", juce::Colour(0xff2c3640), &sfxPage, true);
    addAndMakeVisible(tabs);

    addKnob(macroPage, "quality", "Quality", "Higher quality keeps bandwidth and lowers noise/hum.");
    addKnob(macroPage, "age", "Age", "Older tape adds saturation, compression and head bump.");
    addKnob(macroPage, "wow", "Wow", "Slower pitch wobble and flutter movement depth.");
    addKnob(macroPage, "glitch", "Glitch", "Controls dropout amount and mechanical instability.");

    addKnob(tonePage, "hpHz", "HP", "Cuts low-end rumble from motor/mechanism build-up.");
    addKnob(tonePage, "lpHz", "LP", "Rolls off highs like tape head bandwidth limits.");
    addKnob(tonePage, "headBumpDb", "Head Bump", "Low-frequency resonance from tape head coupling.");
    addKnob(tonePage, "headBumpHz", "Bump Hz", "Center frequency for tape low-end bloom.");
    addKnob(tonePage, "outGain", "Out Gain", "Final level trim after coloration.");

    addKnob(mechanicsPage, "speed", "Speed", "Varispeed-style playback tilt.");
    addKnob(mechanicsPage, "wowDepthMs", "Wow Depth", "Slow capstan wow depth in ms.");
    addKnob(mechanicsPage, "flutterDepthMs", "Flutter", "Fast modulation from transport flutter.");
    addKnob(mechanicsPage, "drive", "Drive", "Tape saturation amount before limiting.");
    addKnob(mechanicsPage, "comp", "Comp", "AGC style leveling/compression.");
    addKnob(mechanicsPage, "dropout", "Dropout", "How often tape level dropouts occur.");
    addKnob(mechanicsPage, "dropoutMs", "Drop Len", "Duration of each dropout event.");
    addKnob(mechanicsPage, "hiss", "Hiss", "Wideband tape hiss level.");
    addKnob(mechanicsPage, "hum", "Hum", "Power hum and low tone leakage.");
    addKnob(mechanicsPage, "ceiling", "Ceiling", "Output limiter threshold.");

    addSwitch(sfxPage, "sfxEnable", "Enable Deck SFX", "Mix in authentic cassette/VHS transport noises.");
    addChoice(sfxPage, "sfxBank", "Deck", "Select cassette or VHS mechanism noise set.");
    addChoice(sfxPage, "sfxMode", "Mode", "Bed = looped mechanism, Edges = trigger on gate open/close, Sequence = random actions.");
    addKnob(sfxPage, "sfxLevel", "SFX Level", "How loud the transport effects sit under program audio.");

    setResizable(false, false);
    setSize(860, 540);

    lastQuality = getParamValue("quality");
    lastAge = getParamValue("age");
    lastWow = getParamValue("wow");
    lastGlitch = getParamValue("glitch");

    startTimerHz(18);
}

TapeEngineAudioProcessorEditor::~TapeEngineAudioProcessorEditor() {}

void TapeEngineAudioProcessorEditor::addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Knob>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    knobs.push_back(std::move(c));
}

void TapeEngineAudioProcessorEditor::addSwitch(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Switch>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    switches.push_back(std::move(c));
}

void TapeEngineAudioProcessorEditor::addChoice(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Choice>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    choices.push_back(std::move(c));
}

void TapeEngineAudioProcessorEditor::layoutPage(juce::Component& page, int columns)
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

void TapeEngineAudioProcessorEditor::setParamValue(const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
}

float TapeEngineAudioProcessorEditor::getParamValue(const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue(id))
        return v->load();
    return 0.0f;
}

void TapeEngineAudioProcessorEditor::applyMacroQuality(float quality)
{
    const auto q = std::pow(1.0f - juce::jlimit(0.0f, 1.0f, quality), 1.4f);
    setParamValue("lpHz", 17500.0f - q * 14500.0f);
    setParamValue("hpHz", 25.0f + q * 90.0f);
    setParamValue("hiss", juce::jlimit(0.0f, 1.0f, 0.03f + q * 0.22f));
    setParamValue("hum", juce::jlimit(0.0f, 1.0f, 0.01f + q * 0.06f));
}

void TapeEngineAudioProcessorEditor::applyMacroAge(float age)
{
    const auto a = std::pow(juce::jlimit(0.0f, 1.0f, age), 1.25f);
    setParamValue("drive", juce::jlimit(0.0f, 1.0f, 0.08f + a * 0.85f));
    setParamValue("comp", juce::jlimit(0.0f, 1.0f, 0.12f + a * 0.5f));
    setParamValue("headBumpDb", 1.4f + a * 5.8f);
    setParamValue("headBumpHz", 70.0f + a * 45.0f);
    setParamValue("outGain", 0.96f + a * 0.18f);
    setParamValue("ceiling", 0.92f - a * 0.06f);
}

void TapeEngineAudioProcessorEditor::applyMacroWow(float wow)
{
    const auto w = std::pow(juce::jlimit(0.0f, 1.0f, wow), 1.3f);
    setParamValue("wowDepthMs", 1.2f + w * 12.5f);
    setParamValue("flutterDepthMs", 0.4f + w * 4.8f);
    setParamValue("speed", 1.0f - w * 0.05f);
}

void TapeEngineAudioProcessorEditor::applyMacroGlitch(float glitch)
{
    const auto g = std::pow(juce::jlimit(0.0f, 1.0f, glitch), 1.35f);
    setParamValue("dropout", juce::jlimit(0.0f, 1.0f, g));
    setParamValue("dropoutMs", 18.0f + g * 140.0f);
}

void TapeEngineAudioProcessorEditor::applyPreset(int idx)
{
    if (idx < 0 || idx >= (int) std::size(kPresets))
        return;

    suppressMacros = true;
    for (const auto& kv : kPresets[(size_t) idx].values)
        setParamValue(kv.first, kv.second);
    suppressMacros = false;

    lastQuality = getParamValue("quality");
    lastAge = getParamValue("age");
    lastWow = getParamValue("wow");
    lastGlitch = getParamValue("glitch");
}

void TapeEngineAudioProcessorEditor::timerCallback()
{
    if (suppressMacros)
        return;

    const auto q = getParamValue("quality");
    const auto a = getParamValue("age");
    const auto w = getParamValue("wow");
    const auto g = getParamValue("glitch");

    suppressMacros = true;
    if (std::abs(q - lastQuality) > 0.0005f)
    {
        applyMacroQuality(q);
        lastQuality = q;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(a - lastAge) > 0.0005f)
    {
        applyMacroAge(a);
        lastAge = a;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(w - lastWow) > 0.0005f)
    {
        applyMacroWow(w);
        lastWow = w;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(g - lastGlitch) > 0.0005f)
    {
        applyMacroGlitch(g);
        lastGlitch = g;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    suppressMacros = false;
}

void TapeEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(juce::Colour(0xff151b21), 0.0f, 0.0f, juce::Colour(0xff202a24), 0.0f, (float) getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();

    auto header = getLocalBounds().removeFromTop(82).reduced(8, 8);
    g.setColour(juce::Colour(0xff1d2622));
    g.fillRoundedRectangle(header.toFloat(), 10.0f);
    g.setColour(juce::Colour(0xff4a5f53));
    g.drawRoundedRectangle(header.toFloat(), 10.0f, 1.2f);

    g.setColour(juce::Colour(0xffb7ffcf));
    g.fillRoundedRectangle((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 278.0f, 48.0f, 8.0f);
    g.setColour(juce::Colour(0xff2e513b));
    g.drawRoundedRectangle(juce::Rectangle<float>((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 278.0f, 48.0f), 8.0f, 1.0f);
}

void TapeEngineAudioProcessorEditor::resized()
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

    layoutPage(macroPage, 4);
    layoutPage(tonePage, 3);
    layoutPage(mechanicsPage, 5);
    layoutPage(sfxPage, 2);
}
