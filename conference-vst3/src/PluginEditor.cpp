#include "PluginEditor.h"

namespace
{
float clampf(float x, float lo, float hi)
{
    return juce::jlimit(lo, hi, x);
}

struct PresetDef
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;
};

static const PresetDef kPresets[] = {
    { "Clean Call", { { "mode", 1.0f }, { "bandwidth", 0.75f }, { "codec", 0.15f }, { "dropouts", 0.05f }, { "jitter", 0.06f }, { "robot", 0.03f }, { "noise", 0.05f } } },
    { "Discord Crunch", { { "mode", 0.0f }, { "bandwidth", 0.50f }, { "codec", 0.55f }, { "dropouts", 0.35f }, { "jitter", 0.22f }, { "robot", 0.18f }, { "noise", 0.12f } } },
    { "Zoom Robot", { { "mode", 1.0f }, { "bandwidth", 0.55f }, { "codec", 0.48f }, { "dropouts", 0.28f }, { "jitter", 0.25f }, { "robot", 0.45f }, { "noise", 0.10f } } },
    { "Skype 2008", { { "mode", 2.0f }, { "bandwidth", 0.45f }, { "codec", 0.40f }, { "dropouts", 0.20f }, { "jitter", 0.18f }, { "robot", 0.12f }, { "noise", 0.09f } } },
    { "Bad Hotspot", { { "mode", 3.0f }, { "bandwidth", 0.35f }, { "codec", 0.60f }, { "dropouts", 0.60f }, { "jitter", 0.35f }, { "robot", 0.25f }, { "noise", 0.16f } } },
    { "Conference Meltdown", { { "mode", 0.0f }, { "bandwidth", 0.25f }, { "codec", 0.85f }, { "dropouts", 0.85f }, { "jitter", 0.75f }, { "robot", 0.65f }, { "noise", 0.25f } } },
};
}

ConferenceEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& id, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff9cd8ff));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff96d2ff));
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff5ba5d2));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2b3238));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff3a3f45));
    addAndMakeVisible(slider);

    attachment = std::make_unique<APVTS::SliderAttachment>(state, id, slider);
}

void ConferenceEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(20));
    slider.setBounds(area.reduced(2));
}

ConferenceEngineAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& id, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff9cd8ff));
    addAndMakeVisible(label);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id)))
        for (int i = 0; i < p->choices.size(); ++i)
            combo.addItem(p->choices[i], i + 1);

    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1f2326));
    combo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3a3f45));
    addAndMakeVisible(combo);

    attachment = std::make_unique<APVTS::ComboBoxAttachment>(state, id, combo);
}

void ConferenceEngineAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    combo.setBounds(area.reduced(1));
}

ConferenceEngineAudioProcessorEditor::ConferenceEngineAudioProcessorEditor(ConferenceEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.getAPVTS())
{
    title.setText("Conference Engine", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xffe1f4ff));
    addAndMakeVisible(title);

    subtitle.setText("Conference Codec Rack", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff8ba6b8));
    addAndMakeVisible(subtitle);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9cd8ff));
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
    presetBox.setTooltip("Conference call presets.");
    addAndMakeVisible(presetBox);

    tabs.setOutline(0);
    tabs.setTabBarDepth(34);
    tabs.addTab("Macro", juce::Colour(0xff2c3640), &macroPage, true);
    tabs.addTab("Core", juce::Colour(0xff2c3640), &corePage, true);
    tabs.addTab("Output", juce::Colour(0xff2c3640), &outputPage, true);
    addAndMakeVisible(tabs);

    addChoice(macroPage, "mode", "Mode", "Conference platform profile.");
    addKnob(macroPage, "bandwidth", "Bandwidth", "Voice band width.");
    addKnob(macroPage, "codec", "Codec", "Codec degradation amount.");
    addKnob(macroPage, "dropouts", "Dropouts", "Packet-loss intensity.");
    addKnob(macroPage, "jitter", "Jitter", "Temporal jitter amount.");
    addKnob(macroPage, "robot", "Robot", "Robotic buffer-loop artifacts.");
    addKnob(macroPage, "noise", "Noise", "Background coded noise.");

    addKnob(corePage, "hpHz", "HP", "High-pass for comms body.");
    addKnob(corePage, "lpHz", "LP", "Low-pass for bandwidth.");
    addKnob(corePage, "midHumpDb", "Mid Hump", "Speech intelligibility boost.");
    addKnob(corePage, "midFreq", "Mid Freq", "Presence center frequency.");
    addChoice(corePage, "concealMode", "Conceal", "How missing packets are reconstructed.");
    addKnob(corePage, "packetLoss", "Packet", "Packet loss probability.");
    addKnob(corePage, "packetMs", "Pkt Ms", "Packet block size.");
    addKnob(corePage, "repeatMs", "Repeat", "Repeat concealment look-back.");
    addKnob(corePage, "jitterMs", "Jit Ms", "Jitter delay depth.");
    addKnob(corePage, "jitterRate", "Jit Rate", "Jitter modulation speed.");
    addKnob(corePage, "gate", "Gate", "Noise gate threshold.");
    addKnob(corePage, "bits", "Bits", "Bit depth.");
    addKnob(corePage, "rate", "Rate", "Sample rate.");

    addKnob(outputPage, "ceiling", "Ceiling", "Limiter ceiling.");
    addKnob(outputPage, "outGain", "Out Gain", "Final level trim.");

    setResizable(false, false);
    setSize(860, 540);

    startTimerHz(18);
}

ConferenceEngineAudioProcessorEditor::~ConferenceEngineAudioProcessorEditor() {}

void ConferenceEngineAudioProcessorEditor::addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Knob>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    knobs.push_back(std::move(c));
}

void ConferenceEngineAudioProcessorEditor::addChoice(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Choice>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    choices.push_back(std::move(c));
}

void ConferenceEngineAudioProcessorEditor::layoutPage(juce::Component& page, int columns)
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

void ConferenceEngineAudioProcessorEditor::setParamValue(const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
}

float ConferenceEngineAudioProcessorEditor::getParamValue(const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue(id))
        return v->load();
    return 0.0f;
}

void ConferenceEngineAudioProcessorEditor::applyMacroTargets(int mode, float bandwidth, float codec, float dropouts, float jitter, float robot, float noise)
{
    const auto bw = clampf(bandwidth, 0.0f, 1.0f);
    const auto c = std::pow(clampf(codec, 0.0f, 1.0f), 1.25f);
    const auto d = std::pow(clampf(dropouts, 0.0f, 1.0f), 1.3f);
    const auto j = std::pow(clampf(jitter, 0.0f, 1.0f), 1.2f);
    const auto n = std::pow(clampf(noise, 0.0f, 1.0f), 1.2f);
    const auto narrow = std::pow(1.0f - bw, 1.35f);

    float hp = 210.0f, hpR = 320.0f, lp = 5200.0f, lpR = 1700.0f, mid = 2000.0f, midR = 560.0f, hump = 1.8f, out = 0.98f, ceil = 0.93f;
    if (mode == 3) { hp = 260.0f; hpR = 380.0f; lp = 3600.0f; lpR = 1700.0f; mid = 1900.0f; midR = 520.0f; hump = 2.2f; out = 1.02f; ceil = 0.92f; }
    if (mode == 2) { hp = 220.0f; hpR = 340.0f; lp = 4200.0f; lpR = 1600.0f; mid = 1700.0f; midR = 440.0f; hump = 2.0f; out = 0.98f; ceil = 0.92f; }
    if (mode == 1) { hp = 180.0f; hpR = 260.0f; lp = 6200.0f; lpR = 1900.0f; mid = 2100.0f; midR = 600.0f; hump = 1.5f; out = 0.98f; ceil = 0.94f; }

    const auto hpHz = std::round(hp + narrow * hpR);
    const auto lpHz = std::round(lp - narrow * lpR);
    const auto midFreq = std::round(mid + (0.45f - narrow) * midR);
    const auto midHumpDb = std::round((hump + narrow * 2.8f) * 20.0f) / 20.0f;

    const auto conceal = d > 0.62f ? 3.0f : (d > 0.22f ? 0.0f : 2.0f);
    const auto packetLoss = clampf(0.02f + d * (mode == 3 ? 0.75f : 0.55f), 0.0f, 1.0f);
    const auto packetMs = std::round(12.0f + d * (mode == 1 ? 70.0f : 95.0f));
    const auto repeatMs = std::round(18.0f + d * 120.0f);

    const auto jitterMs = std::round((0.02f + j * 0.55f) * 100.0f) / 100.0f;
    const auto jitterRate = std::round(18.0f + j * 80.0f);

    const auto bits = std::round(14.0f - c * 8.0f);
    const auto rate = std::round(46000.0f - c * (mode == 3 ? 38000.0f : 32000.0f));
    const auto gate = clampf(0.05f + (mode == 1 ? 0.12f : 0.08f) + c * 0.25f + d * 0.2f, 0.0f, 1.0f);

    const auto ceiling = clampf(ceil - c * 0.05f, 0.2f, 1.0f);
    const auto outGain = std::round((out + c * 0.12f) * 100.0f) / 100.0f;

    setParamValue("hpHz", hpHz);
    setParamValue("lpHz", lpHz);
    setParamValue("midFreq", midFreq);
    setParamValue("midHumpDb", midHumpDb);
    setParamValue("concealMode", conceal);
    setParamValue("packetLoss", packetLoss);
    setParamValue("packetMs", packetMs);
    setParamValue("repeatMs", repeatMs);
    setParamValue("jitterMs", jitterMs);
    setParamValue("jitterRate", jitterRate);
    setParamValue("gate", gate);
    setParamValue("bits", bits);
    setParamValue("rate", rate);
    setParamValue("ceiling", ceiling);
    setParamValue("outGain", outGain);

    setParamValue("robot", robot);
    setParamValue("noise", n);
}

void ConferenceEngineAudioProcessorEditor::applyPreset(int idx)
{
    if (idx < 0 || idx >= (int) std::size(kPresets))
        return;

    suppressMacros = true;
    for (const auto& kv : kPresets[(size_t) idx].values)
        setParamValue(kv.first, kv.second);
    applyMacroTargets((int) getParamValue("mode"), getParamValue("bandwidth"), getParamValue("codec"), getParamValue("dropouts"), getParamValue("jitter"), getParamValue("robot"), getParamValue("noise"));
    suppressMacros = false;
}

void ConferenceEngineAudioProcessorEditor::timerCallback()
{
    if (suppressMacros)
        return;

    const auto mode = (int) getParamValue("mode");
    const auto bw = getParamValue("bandwidth");
    const auto codec = getParamValue("codec");
    const auto drops = getParamValue("dropouts");
    const auto jit = getParamValue("jitter");
    const auto rob = getParamValue("robot");
    const auto noi = getParamValue("noise");

    if (mode != lastMode || std::abs(bw - lastBandwidth) > 0.0005f || std::abs(codec - lastCodec) > 0.0005f || std::abs(drops - lastDropouts) > 0.0005f || std::abs(jit - lastJitter) > 0.0005f || std::abs(rob - lastRobot) > 0.0005f || std::abs(noi - lastNoise) > 0.0005f)
    {
        suppressMacros = true;
        applyMacroTargets(mode, bw, codec, drops, jit, rob, noi);
        suppressMacros = false;
        lastMode = mode;
        lastBandwidth = bw;
        lastCodec = codec;
        lastDropouts = drops;
        lastJitter = jit;
        lastRobot = rob;
        lastNoise = noi;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
}

void ConferenceEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(juce::Colour(0xff131a20), 0.0f, 0.0f, juce::Colour(0xff202b35), 0.0f, (float) getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();

    auto header = getLocalBounds().removeFromTop(82).reduced(8, 8);
    g.setColour(juce::Colour(0xff1f2832));
    g.fillRoundedRectangle(header.toFloat(), 10.0f);
    g.setColour(juce::Colour(0xff556776));
    g.drawRoundedRectangle(header.toFloat(), 10.0f, 1.2f);

    g.setColour(juce::Colour(0xffa6e2ff));
    g.fillRoundedRectangle((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 278.0f, 48.0f, 8.0f);
    g.setColour(juce::Colour(0xff275666));
    g.drawRoundedRectangle(juce::Rectangle<float>((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 278.0f, 48.0f), 8.0f, 1.0f);
}

void ConferenceEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    auto header = area.removeFromTop(74);

    auto left = header.removeFromLeft(420);
    title.setBounds(left.removeFromTop(42).withTrimmedLeft(18));
    subtitle.setBounds(left.withTrimmedLeft(20));

    auto right = header.withTrimmedLeft(26);
    presetLabel.setBounds(right.removeFromTop(18));
    presetBox.setBounds(right.removeFromTop(30).removeFromLeft(230));

    tabs.setBounds(area);
    layoutPage(macroPage, 4);
    layoutPage(corePage, 4);
    layoutPage(outputPage, 2);
}
