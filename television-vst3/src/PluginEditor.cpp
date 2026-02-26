#include "PluginEditor.h"

namespace
{
struct PresetDef
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;
};

static const PresetDef kPresets[] = {
    { "Living Room CRT", { { "vibe", 0.45f }, { "speaker", 0.55f }, { "agc", 0.22f }, { "static", 0.12f }, { "hum", 0.18f }, { "whine", 0.08f }, { "bedEnable", 1.0f }, { "bedLevel", 0.22f } } },
    { "Late Night Broadcast", { { "vibe", 0.35f }, { "speaker", 0.65f }, { "agc", 0.18f }, { "static", 0.08f }, { "hum", 0.12f }, { "whine", 0.06f }, { "bedEnable", 1.0f }, { "bedLevel", 0.16f } } },
    { "Small Kitchen TV", { { "vibe", 0.60f }, { "speaker", 0.35f }, { "agc", 0.35f }, { "static", 0.18f }, { "hum", 0.22f }, { "whine", 0.10f }, { "bedEnable", 1.0f }, { "bedLevel", 0.26f } } },
    { "Antenna Snow Soft", { { "vibe", 0.55f }, { "speaker", 0.50f }, { "agc", 0.20f }, { "static", 0.35f }, { "hum", 0.10f }, { "whine", 0.05f }, { "bedEnable", 0.0f }, { "bedLevel", 0.20f } } },
};
}

TelevisionEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& id, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffffcf80));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffc26d));
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffb3833d));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2b3238));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff3a3f45));
    addAndMakeVisible(slider);

    attachment = std::make_unique<APVTS::SliderAttachment>(state, id, slider);
}

void TelevisionEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(20));
    slider.setBounds(area.reduced(2));
}

TelevisionEngineAudioProcessorEditor::Switch::Switch(APVTS& state, const juce::String& id, const juce::String& text)
{
    button.setButtonText(text);
    button.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd2d7dc));
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffffc26d));
    addAndMakeVisible(button);
    attachment = std::make_unique<APVTS::ButtonAttachment>(state, id, button);
}

void TelevisionEngineAudioProcessorEditor::Switch::resized() { button.setBounds(getLocalBounds()); }

TelevisionEngineAudioProcessorEditor::TelevisionEngineAudioProcessorEditor(TelevisionEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.getAPVTS())
{
    title.setText("Television Engine", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfffff0d8));
    addAndMakeVisible(title);

    subtitle.setText("Compact CRT Console", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xffae9c7d));
    addAndMakeVisible(subtitle);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffcf80));
    addAndMakeVisible(presetLabel);

    presetBox.addItem("Custom", 1);
    for (int i = 0; i < (int) std::size(kPresets); ++i) presetBox.addItem(kPresets[i].name, i + 2);
    presetBox.onChange = [this]() { const auto id = presetBox.getSelectedId(); if (id >= 2) applyPreset(id - 2); };
    presetBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(presetBox);

    tabs.setOutline(0);
    tabs.setTabBarDepth(34);
    tabs.addTab("Macro", juce::Colour(0xff2c3640), &macroPage, true);
    tabs.addTab("Tone", juce::Colour(0xff2c3640), &tonePage, true);
    tabs.addTab("Noise", juce::Colour(0xff2c3640), &noisePage, true);
    addAndMakeVisible(tabs);

    addKnob(macroPage, "vibe", "Vibe", "Overall old-TV character and harshness.");
    addKnob(macroPage, "speaker", "Speaker", "Bandwidth + boxiness of tiny CRT speakers.");
    addKnob(macroPage, "agc", "AGC", "Broadcast style auto-gain pumping.");
    addKnob(macroPage, "static", "Static", "Amount of TV snow/noise wash.");

    addKnob(tonePage, "hpHz", "HP", "Low cut to mimic small speaker low-end loss.");
    addKnob(tonePage, "lpHz", "LP", "High cut for bandwidth-limited TV sound.");
    addKnob(tonePage, "midHumpDb", "Mid Hump", "Presence bump around speech region.");
    addKnob(tonePage, "midFreq", "Mid Freq", "Frequency of TV presence hump.");
    addKnob(tonePage, "outGain", "Out Gain", "Final level trim.");

    addKnob(noisePage, "noiseHiss", "Hiss", "Brightness of static hiss.");
    addKnob(noisePage, "noiseCrackle", "Crackle", "Sporadic noisy crackle bursts.");
    addKnob(noisePage, "hum", "Hum", "Power hum leakage.");
    addKnob(noisePage, "whine", "Whine", "CRT high-frequency line whistle.");
    addSwitch(noisePage, "bedEnable", "CRT Bed", "Enable embedded CRT bed recording.");
    addKnob(noisePage, "bedLevel", "Bed Level", "Mix level of CRT bed ambience.");

    setResizable(false, false);
    setSize(860, 540);

    lastVibe = getParamValue("vibe");
    lastSpeaker = getParamValue("speaker");
    lastAgc = getParamValue("agc");
    lastStatic = getParamValue("static");

    startTimerHz(18);
}

TelevisionEngineAudioProcessorEditor::~TelevisionEngineAudioProcessorEditor() {}

void TelevisionEngineAudioProcessorEditor::addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Knob>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    knobs.push_back(std::move(c));
}

void TelevisionEngineAudioProcessorEditor::addSwitch(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Switch>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    switches.push_back(std::move(c));
}

void TelevisionEngineAudioProcessorEditor::layoutPage(juce::Component& page, int columns)
{
    auto area = page.getLocalBounds().reduced(10);
    auto& items = pageItems[&page];
    if (items.empty()) return;

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

void TelevisionEngineAudioProcessorEditor::setParamValue(const juce::String& id, float plain)
{
    if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(plain));
}

float TelevisionEngineAudioProcessorEditor::getParamValue(const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue(id)) return v->load();
    return 0.0f;
}

void TelevisionEngineAudioProcessorEditor::applyMacroVibe(float v)
{
    const auto vv = std::pow(juce::jlimit(0.0f, 1.0f, v), 1.15f);
    setParamValue("noiseCrackle", juce::jlimit(0.0f, 1.0f, 0.04f + vv * 0.12f));
}

void TelevisionEngineAudioProcessorEditor::applyMacroSpeaker(float s)
{
    const auto sp = std::pow(juce::jlimit(0.0f, 1.0f, s), 1.15f);
    setParamValue("hpHz", 45.0f + (1.0f - sp) * 110.0f);
    setParamValue("lpHz", 16000.0f - (1.0f - sp) * 10000.0f);
    setParamValue("midHumpDb", 0.6f + (1.0f - sp) * 2.4f);
    setParamValue("midFreq", 1550.0f + (1.0f - sp) * 650.0f);
}

void TelevisionEngineAudioProcessorEditor::applyMacroAgc(float a)
{
    juce::ignoreUnused(a);
}

void TelevisionEngineAudioProcessorEditor::applyMacroStatic(float st)
{
    const auto t = std::pow(juce::jlimit(0.0f, 1.0f, st), 1.2f);
    setParamValue("noiseHiss", juce::jlimit(0.0f, 1.0f, 0.45f + t * 0.5f));
}

void TelevisionEngineAudioProcessorEditor::applyPreset(int idx)
{
    if (idx < 0 || idx >= (int) std::size(kPresets)) return;
    suppressMacros = true;
    for (const auto& kv : kPresets[(size_t) idx].values) setParamValue(kv.first, kv.second);
    suppressMacros = false;

    lastVibe = getParamValue("vibe");
    lastSpeaker = getParamValue("speaker");
    lastAgc = getParamValue("agc");
    lastStatic = getParamValue("static");
}

void TelevisionEngineAudioProcessorEditor::timerCallback()
{
    if (suppressMacros) return;

    const auto v = getParamValue("vibe");
    const auto s = getParamValue("speaker");
    const auto a = getParamValue("agc");
    const auto st = getParamValue("static");

    suppressMacros = true;
    if (std::abs(v - lastVibe) > 0.0005f) { applyMacroVibe(v); lastVibe = v; presetBox.setSelectedId(1, juce::dontSendNotification); }
    if (std::abs(s - lastSpeaker) > 0.0005f) { applyMacroSpeaker(s); lastSpeaker = s; presetBox.setSelectedId(1, juce::dontSendNotification); }
    if (std::abs(a - lastAgc) > 0.0005f) { applyMacroAgc(a); lastAgc = a; presetBox.setSelectedId(1, juce::dontSendNotification); }
    if (std::abs(st - lastStatic) > 0.0005f) { applyMacroStatic(st); lastStatic = st; presetBox.setSelectedId(1, juce::dontSendNotification); }
    suppressMacros = false;
}

void TelevisionEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(juce::Colour(0xff191817), 0.0f, 0.0f, juce::Colour(0xff2a2520), 0.0f, (float) getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();

    auto header = getLocalBounds().removeFromTop(82).reduced(8, 8);
    g.setColour(juce::Colour(0xff2a241d));
    g.fillRoundedRectangle(header.toFloat(), 10.0f);
    g.setColour(juce::Colour(0xff5f4f3a));
    g.drawRoundedRectangle(header.toFloat(), 10.0f, 1.2f);

    g.setColour(juce::Colour(0xffffd8a0));
    g.fillRoundedRectangle((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 278.0f, 48.0f, 8.0f);
    g.setColour(juce::Colour(0xff4b3923));
    g.drawRoundedRectangle(juce::Rectangle<float>((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 278.0f, 48.0f), 8.0f, 1.0f);
}

void TelevisionEngineAudioProcessorEditor::resized()
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
    layoutPage(noisePage, 3);
}
