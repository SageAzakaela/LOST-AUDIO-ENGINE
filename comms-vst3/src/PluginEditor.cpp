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
    { "Landline Clean", { { "mode", 0.0f }, { "bandwidth", 0.55f }, { "drive", 0.22f }, { "glitch", 0.06f }, { "noise", 0.12f }, { "echoMix", 0.02f }, { "verbMix", 0.04f } } },
    { "Cell Bad", { { "mode", 1.0f }, { "bandwidth", 0.32f }, { "drive", 0.45f }, { "glitch", 0.62f }, { "noise", 0.18f }, { "echoMix", 0.06f }, { "verbMix", 0.03f } } },
    { "Intercom Old", { { "mode", 2.0f }, { "bandwidth", 0.28f }, { "drive", 0.52f }, { "glitch", 0.26f }, { "noise", 0.32f }, { "echoMix", 0.12f }, { "verbMix", 0.22f } } },
    { "PA Hot", { { "mode", 3.0f }, { "bandwidth", 0.66f }, { "drive", 0.70f }, { "glitch", 0.14f }, { "noise", 0.12f }, { "echoMix", 0.20f }, { "verbMix", 0.14f } } },
    { "Alarm Panel", { { "mode", 4.0f }, { "bandwidth", 0.72f }, { "drive", 0.34f }, { "glitch", 0.12f }, { "noise", 0.25f }, { "alarmTone", 1.0f }, { "toneMix", 0.55f } } },
    { "VoIP", { { "mode", 1.0f }, { "bandwidth", 0.42f }, { "drive", 0.35f }, { "glitch", 0.78f }, { "noise", 0.14f }, { "echoMix", 0.03f }, { "verbMix", 0.02f } } },
};
}

CommsEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& id, const juce::String& text)
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

void CommsEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(20));
    slider.setBounds(area.reduced(2));
}

CommsEngineAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& id, const juce::String& text)
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

void CommsEngineAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    combo.setBounds(area.reduced(1));
}

CommsEngineAudioProcessorEditor::Switch::Switch(APVTS& state, const juce::String& id, const juce::String& text)
{
    button.setButtonText(text);
    button.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd2d7dc));
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff96d2ff));
    addAndMakeVisible(button);
    attachment = std::make_unique<APVTS::ButtonAttachment>(state, id, button);
}

void CommsEngineAudioProcessorEditor::Switch::resized()
{
    button.setBounds(getLocalBounds());
}

CommsEngineAudioProcessorEditor::CommsEngineAudioProcessorEditor(CommsEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.getAPVTS())
{
    title.setText("Comms Engine", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xffe1f4ff));
    addAndMakeVisible(title);

    subtitle.setText("Communication Channel Rack", juce::dontSendNotification);
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
    presetBox.setTooltip("Factory comms channel presets.");
    addAndMakeVisible(presetBox);

    tabs.setOutline(0);
    tabs.setTabBarDepth(34);
    tabs.addTab("Macro", juce::Colour(0xff2c3640), &macroPage, true);
    tabs.addTab("Core", juce::Colour(0xff2c3640), &corePage, true);
    tabs.addTab("Space", juce::Colour(0xff2c3640), &fxPage, true);
    tabs.addTab("Output", juce::Colour(0xff2c3640), &outputPage, true);
    addAndMakeVisible(tabs);

    addChoice(macroPage, "mode", "Mode", "Target comms system model.");
    addKnob(macroPage, "bandwidth", "Bandwidth", "Narrow/wide channel voice range.");
    addKnob(macroPage, "drive", "Drive", "Nonlinear transmitter crunch.");
    addKnob(macroPage, "glitch", "Glitch", "Packet loss and rate artifacts.");
    addKnob(macroPage, "noise", "Noise", "Hum/hiss and channel grunge.");
    addSwitch(macroPage, "alarmTone", "Alarm Tone", "Inject warbling alarm panel tone.");

    addKnob(corePage, "hpHz", "HP", "Low-cut for narrow comms body.");
    addKnob(corePage, "lpHz", "LP", "Top-end cutoff for line quality.");
    addKnob(corePage, "midHumpDb", "Mid Hump", "Intelligibility boost.");
    addKnob(corePage, "midFreq", "Mid Freq", "Presence center frequency.");
    addKnob(corePage, "comp", "Comp", "AGC-like dynamic squeeze.");
    addKnob(corePage, "bits", "Bits", "Bit-depth reduction.");
    addKnob(corePage, "rate", "Rate", "Sample-rate reduction.");
    addKnob(corePage, "packet", "Packet", "Packet dropout chance.");
    addKnob(corePage, "packetMs", "Pkt Ms", "Dropout block length.");
    addKnob(corePage, "hum", "Hum", "Powerline/low tone content.");
    addKnob(corePage, "hiss", "Hiss", "Broadband static.");
    addKnob(corePage, "toneMix", "Tone Mix", "Alarm tone blend level.");

    addKnob(fxPage, "echoMix", "Echo Mix", "Parallel slap/line echo amount.");
    addKnob(fxPage, "echoMs", "Echo Ms", "Echo delay time.");
    addKnob(fxPage, "echoFb", "Echo Fb", "Echo feedback amount.");
    addKnob(fxPage, "echoTone", "Echo Tone", "Echo feedback lowpass tone.");
    addKnob(fxPage, "verbMix", "Verb Mix", "Parallel room contribution.");
    addKnob(fxPage, "verbMs", "Verb Ms", "Approximate room tail size.");
    addKnob(fxPage, "verbDamp", "Verb Damp", "Reverb high-frequency damping.");

    addKnob(outputPage, "ceiling", "Ceiling", "Limiter ceiling.");
    addKnob(outputPage, "outGain", "Out Gain", "Final level trim.");

    setResizable(false, false);
    setSize(860, 540);

    lastMode = (int) getParamValue("mode");
    lastBandwidth = getParamValue("bandwidth");
    lastDrive = getParamValue("drive");
    lastGlitch = getParamValue("glitch");
    lastNoise = getParamValue("noise");

    startTimerHz(18);
}

CommsEngineAudioProcessorEditor::~CommsEngineAudioProcessorEditor() {}

void CommsEngineAudioProcessorEditor::addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Knob>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    knobs.push_back(std::move(c));
}

void CommsEngineAudioProcessorEditor::addChoice(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Choice>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    choices.push_back(std::move(c));
}

void CommsEngineAudioProcessorEditor::addSwitch(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Switch>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    switches.push_back(std::move(c));
}

void CommsEngineAudioProcessorEditor::layoutPage(juce::Component& page, int columns)
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

void CommsEngineAudioProcessorEditor::setParamValue(const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
}

float CommsEngineAudioProcessorEditor::getParamValue(const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue(id))
        return v->load();
    return 0.0f;
}

void CommsEngineAudioProcessorEditor::applyMacroTargets(int mode, float bandwidth, float drive, float glitch, float noise)
{
    const auto bw = clampf(bandwidth, 0.0f, 1.0f);
    const auto drv = clampf(drive, 0.0f, 1.0f);
    const auto gch = clampf(glitch, 0.0f, 1.0f);
    const auto noi = clampf(noise, 0.0f, 1.0f);

    const auto narrow = std::pow(1.0f - bw, 1.35f);
    const auto d = std::pow(drv, 1.25f);
    const auto g = std::pow(gch, 1.35f);
    const auto n = std::pow(noi, 1.2f);

    float hp = 250.0f, hpR = 320.0f, lp = 4300.0f, lpR = 1900.0f, hump = 2.8f, humpR = 5.4f, mid = 1850.0f, midR = 380.0f, comp = 0.54f, out = 0.95f, ceil = 0.92f;
    if (mode == 1) { hp = 220.0f; hpR = 360.0f; lp = 3700.0f; lpR = 1600.0f; hump = 2.0f; humpR = 4.2f; mid = 1700.0f; midR = 450.0f; comp = 0.58f; out = 1.02f; ceil = 0.92f; }
    if (mode == 2) { hp = 340.0f; hpR = 380.0f; lp = 3300.0f; lpR = 1100.0f; hump = 4.2f; humpR = 6.0f; mid = 1950.0f; midR = 450.0f; comp = 0.66f; out = 0.98f; ceil = 0.9f; }
    if (mode == 3) { hp = 160.0f; hpR = 280.0f; lp = 6800.0f; lpR = 2600.0f; hump = 1.6f; humpR = 3.2f; mid = 1500.0f; midR = 450.0f; comp = 0.42f; out = 1.15f; ceil = 0.88f; }
    if (mode == 4) { hp = 250.0f; hpR = 360.0f; lp = 7600.0f; lpR = 3400.0f; hump = 1.0f; humpR = 3.0f; mid = 1600.0f; midR = 700.0f; comp = 0.46f; out = 1.05f; ceil = 0.9f; }

    const auto hpHz = std::round(hp + narrow * hpR);
    const auto lpHz = std::round(lp - narrow * lpR);
    const auto midDb = std::round((hump + narrow * humpR) * 20.0f) / 20.0f;
    const auto midFreq = std::round(mid + (0.55f - narrow) * midR);

    const auto compV = clampf(comp + d * 0.38f, 0.0f, 1.0f);
    const auto bitsBase = mode == 3 ? 14.0f : 13.0f;
    const auto bits = std::round(clampf(1.0f - g, 0.0f, 1.0f) * (bitsBase - 4.0f) + 4.0f);
    const auto rate = std::round(46000.0f - g * 38000.0f);

    const auto packetScale = mode == 1 ? 0.72f : (mode == 4 ? 0.35f : 0.25f);
    const auto packet = clampf(g * packetScale, 0.0f, 1.0f);
    const auto packetMs = std::round(10.0f + g * (mode == 1 ? 120.0f : 75.0f));

    const auto hum = clampf(0.06f + n * (mode == 2 ? 0.55f : 0.4f), 0.0f, 1.0f);
    const auto hiss = clampf(0.08f + n * 0.55f, 0.0f, 1.0f);
    const auto toneMix = clampf((mode == 4 ? 0.45f : 0.22f) + n * 0.15f, 0.0f, 1.0f);

    const auto outGain = std::round((out + d * 0.12f) * 100.0f) / 100.0f;

    const auto roomBase = mode == 2 ? 0.18f : (mode == 3 ? 0.12f : (mode == 4 ? 0.14f : 0.05f));
    const auto verbMix = clampf(roomBase + n * 0.12f, 0.0f, 1.0f);
    const auto verbMs = std::round((mode == 2 ? 420.0f : (mode == 3 ? 540.0f : (mode == 4 ? 360.0f : 220.0f))) * (0.75f + 0.55f * n));
    const auto verbDamp = clampf(mode == 2 ? 0.7f : (mode == 3 ? 0.55f : 0.45f + n * 0.1f), 0.0f, 1.0f);

    const auto echoBase = mode == 3 ? 0.14f : (mode == 2 ? 0.08f : (mode == 4 ? 0.05f : 0.03f));
    const auto echoMix = clampf(echoBase + g * 0.08f, 0.0f, 1.0f);
    const auto echoMs = std::round((mode == 3 ? 240.0f : (mode == 2 ? 260.0f : 180.0f)) * (0.85f + 0.35f * g));
    const auto echoFb = clampf(0.12f + (mode == 3 ? 0.35f : 0.22f) * g, 0.0f, 1.0f);
    const auto echoTone = clampf(mode == 2 ? 0.45f : 0.6f + n * 0.15f, 0.0f, 1.0f);

    setParamValue("hpHz", hpHz);
    setParamValue("lpHz", lpHz);
    setParamValue("midHumpDb", midDb);
    setParamValue("midFreq", midFreq);
    setParamValue("comp", compV);
    setParamValue("bits", bits);
    setParamValue("rate", rate);
    setParamValue("packet", packet);
    setParamValue("packetMs", packetMs);
    setParamValue("hum", hum);
    setParamValue("hiss", hiss);
    setParamValue("toneMix", toneMix);
    setParamValue("ceiling", ceil);
    setParamValue("outGain", outGain);
    setParamValue("echoMix", echoMix);
    setParamValue("echoMs", echoMs);
    setParamValue("echoFb", echoFb);
    setParamValue("echoTone", echoTone);
    setParamValue("verbMix", verbMix);
    setParamValue("verbMs", verbMs);
    setParamValue("verbDamp", verbDamp);
}

void CommsEngineAudioProcessorEditor::applyPreset(int idx)
{
    if (idx < 0 || idx >= (int) std::size(kPresets))
        return;

    suppressMacros = true;
    for (const auto& kv : kPresets[(size_t) idx].values)
        setParamValue(kv.first, kv.second);

    applyMacroTargets((int) getParamValue("mode"), getParamValue("bandwidth"), getParamValue("drive"), getParamValue("glitch"), getParamValue("noise"));
    suppressMacros = false;

    lastMode = (int) getParamValue("mode");
    lastBandwidth = getParamValue("bandwidth");
    lastDrive = getParamValue("drive");
    lastGlitch = getParamValue("glitch");
    lastNoise = getParamValue("noise");
}

void CommsEngineAudioProcessorEditor::timerCallback()
{
    if (suppressMacros)
        return;

    const auto mode = (int) getParamValue("mode");
    const auto bw = getParamValue("bandwidth");
    const auto drive = getParamValue("drive");
    const auto glitch = getParamValue("glitch");
    const auto noise = getParamValue("noise");

    if (mode != lastMode || std::abs(bw - lastBandwidth) > 0.0005f || std::abs(drive - lastDrive) > 0.0005f || std::abs(glitch - lastGlitch) > 0.0005f || std::abs(noise - lastNoise) > 0.0005f)
    {
        suppressMacros = true;
        applyMacroTargets(mode, bw, drive, glitch, noise);
        suppressMacros = false;

        lastMode = mode;
        lastBandwidth = bw;
        lastDrive = drive;
        lastGlitch = glitch;
        lastNoise = noise;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
}

void CommsEngineAudioProcessorEditor::paint(juce::Graphics& g)
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

void CommsEngineAudioProcessorEditor::resized()
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
    layoutPage(macroPage, 3);
    layoutPage(corePage, 4);
    layoutPage(fxPage, 4);
    layoutPage(outputPage, 2);
}
