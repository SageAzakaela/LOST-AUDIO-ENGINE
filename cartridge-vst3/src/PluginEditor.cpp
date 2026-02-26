#include "PluginEditor.h"

#include <cmath>

namespace
{
struct PresetDef
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;
};

static const PresetDef kPresets[] = {
    { "Game Boy", { { "quality", 0.18f }, { "codec", 0.18f }, { "grit", 0.22f }, { "noise", 0.12f }, { "dither", 1.0f }, { "noiseShaping", 0.0f } } },
    { "NES", { { "quality", 0.14f }, { "codec", 0.12f }, { "grit", 0.18f }, { "noise", 0.08f }, { "dither", 1.0f }, { "noiseShaping", 0.0f } } },
    { "SNES", { { "quality", 0.46f }, { "codec", 0.22f }, { "grit", 0.16f }, { "noise", 0.06f }, { "dither", 1.0f }, { "noiseShaping", 1.0f } } },
    { "PS1", { { "quality", 0.32f }, { "codec", 0.70f }, { "grit", 0.22f }, { "noise", 0.06f }, { "dither", 1.0f }, { "noiseShaping", 1.0f } } },
    { "Arcade Cab", {
        { "quality", 0.3f }, { "codec", 0.2f }, { "grit", 0.65f }, { "noise", 0.12f },
        { "speaker", 0.55f }, { "edge", 0.55f }, { "limiter", 0.55f }, { "ceiling", 0.9f }, { "hpHz", 90.0f },
        { "verb", 0.18f }, { "verbMs", 40.0f }, { "microDelayMs", 10.0f }, { "microDelayMix", 0.22f }
    } },
    { "Blip Test", {
        { "quality", 0.22f }, { "codec", 0.18f }, { "grit", 0.22f }, { "noise", 0.06f },
        { "bleepsEnable", 1.0f }, { "bleepsMix", 0.22f }, { "bleepsRate", 5.5f }, { "bleepsWave", 0.0f },
        { "bleepsVibrato", 0.55f }, { "bleepsPitch", 0.6f }, { "verb", 0.28f }, { "verbMs", 55.0f }
    } },
};

float clamp01(float x)
{
    return juce::jlimit(0.0f, 1.0f, x);
}
} // namespace

CartridgeEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& id, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffffdf8d));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffcf68));
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffcc8f3f));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff33363c));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff43474d));
    addAndMakeVisible(slider);

    att = std::make_unique<APVTS::SliderAttachment>(state, id, slider);
}

void CartridgeEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(20));
    slider.setBounds(area.reduced(2));
}

CartridgeEngineAudioProcessorEditor::Switch::Switch(APVTS& state, const juce::String& id, const juce::String& text)
{
    button.setButtonText(text);
    button.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd6dde5));
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffffcf68));
    addAndMakeVisible(button);
    att = std::make_unique<APVTS::ButtonAttachment>(state, id, button);
}

void CartridgeEngineAudioProcessorEditor::Switch::resized()
{
    button.setBounds(getLocalBounds());
}

CartridgeEngineAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& id, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffffdf8d));
    addAndMakeVisible(label);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id)))
        for (int i = 0; i < p->choices.size(); ++i)
            combo.addItem(p->choices[i], i + 1);

    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff21262d));
    combo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff454b53));
    addAndMakeVisible(combo);

    att = std::make_unique<APVTS::ComboBoxAttachment>(state, id, combo);
}

void CartridgeEngineAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    combo.setBounds(area.reduced(1));
}

CartridgeEngineAudioProcessorEditor::CartridgeEngineAudioProcessorEditor(CartridgeEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.getAPVTS())
{
    title.setText("Cartridge Engine", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(29.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff2f4f7));
    addAndMakeVisible(title);

    subtitle.setText("Compact Retro Console Bus", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff9ca8b5));
    addAndMakeVisible(subtitle);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffdf8d));
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
    presetBox.setTooltip("Retro console profiles for fast setup.");
    addAndMakeVisible(presetBox);

    tabs.setOutline(0);
    tabs.setTabBarDepth(34);
    tabs.addTab("Macro", juce::Colour(0xff303a44), &macroPage, true);
    tabs.addTab("Core", juce::Colour(0xff303a44), &corePage, true);
    tabs.addTab("Vibe", juce::Colour(0xff303a44), &vibePage, true);
    tabs.addTab("Bleeps", juce::Colour(0xff303a44), &bleepPage, true);
    addAndMakeVisible(tabs);

    addKnob(macroPage, "quality", "Quality", "Higher quality raises bit depth, sample rate and bandwidth.");
    addKnob(macroPage, "codec", "Codec", "More codec means stronger companding and block artifacts.");
    addKnob(macroPage, "grit", "Grit", "Adds saturation, edge aliasing, hum and whine.");
    addKnob(macroPage, "noise", "Noise", "Background digital hiss and floor roughness.");
    addSwitch(macroPage, "dither", "Dither", "Adds quantization dither like vintage converter noise floors.");
    addSwitch(macroPage, "noiseShaping", "Noise Shaping", "Pushes quantization error up-band for sharper detail.");

    addKnob(corePage, "bits", "Bits", "Converter bit depth.");
    addKnob(corePage, "rate", "Rate", "Effective sample rate before the virtual DAC.");
    addKnob(corePage, "jitter", "Jitter", "Clock instability for gritty timing variance.");
    addKnob(corePage, "lpHz", "LP", "Bandwidth limit before conversion.");
    addKnob(corePage, "hpHz", "HP", "Removes low rumble from the output stage.");
    addKnob(corePage, "preEmph", "PreEmph", "Pre-emphasis tilt before degradation.");
    addKnob(corePage, "mulaw", "MuLaw", "Companding amount for ADPCM/voice-like crunch.");
    addKnob(corePage, "blockMs", "Block", "Blocky sample hold behavior.");

    addKnob(vibePage, "sat", "Sat", "DAC/output saturation amount.");
    addKnob(vibePage, "edge", "Edge", "Hard edge clipping before downsample stage.");
    addKnob(vibePage, "speaker", "Speaker", "Small speaker curve: dip+presence+top rolloff.");
    addKnob(vibePage, "hum", "Hum", "Power hum content.");
    addKnob(vibePage, "whine", "Whine", "High-pitched oscillator whine.");
    addKnob(vibePage, "dcDrift", "DC Drift", "Bias wander and coupling drift.");
    addKnob(vibePage, "noiseTrack", "Noise Trk", "How much noise follows signal envelope.");
    addKnob(vibePage, "microDelayMs", "uDelay", "Tiny smear delay for cabinet body.");
    addKnob(vibePage, "microDelayMix", "uMix", "Amount of microdelay blend.");
    addKnob(vibePage, "verb", "Verb", "Short conduction/cabinet reverb amount.");
    addKnob(vibePage, "verbMs", "Verb ms", "Size of conduction reverb taps.");
    addKnob(vibePage, "limiter", "Limiter", "End limiter strength.");
    addKnob(vibePage, "ceiling", "Ceiling", "Limiter top ceiling.");
    addKnob(vibePage, "wet", "Wet", "Dry/wet blend.");
    addKnob(vibePage, "outGain", "Out", "Final output trim.");

    addSwitch(bleepPage, "bleepsEnable", "Enable Bleeps", "Inject procedural retro synth bleeps into the bus.");
    addKnob(bleepPage, "bleepsMix", "Mix", "Bleep level under program audio.");
    addKnob(bleepPage, "bleepsRate", "Rate", "How often bleeps are triggered.");
    addChoice(bleepPage, "bleepsWave", "Wave", "Random, pulse, saw, or triangle bleep source.");
    addKnob(bleepPage, "bleepsVibrato", "Vibrato", "Pitch wobble depth/probability.");
    addKnob(bleepPage, "bleepsPitch", "Pitch", "Overall register of generated bleeps.");

    setResizable(false, false);
    setSize(860, 540);

    lastQuality = getParamValue("quality");
    lastCodec = getParamValue("codec");
    lastGrit = getParamValue("grit");

    // Match web behavior: macro knobs define advanced controls at startup.
    suppressMacros = true;
    applyMacroQuality(lastQuality);
    applyMacroCodec(lastCodec);
    applyMacroGrit(lastGrit);
    suppressMacros = false;

    startTimerHz(18);
}

CartridgeEngineAudioProcessorEditor::~CartridgeEngineAudioProcessorEditor() {}

void CartridgeEngineAudioProcessorEditor::addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Knob>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    knobs.push_back(std::move(c));
}

void CartridgeEngineAudioProcessorEditor::addSwitch(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Switch>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    switches.push_back(std::move(c));
}

void CartridgeEngineAudioProcessorEditor::addChoice(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Choice>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    choices.push_back(std::move(c));
}

void CartridgeEngineAudioProcessorEditor::layoutPage(juce::Component& page, int columns)
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

void CartridgeEngineAudioProcessorEditor::setParamValue(const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
}

float CartridgeEngineAudioProcessorEditor::getParamValue(const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue(id))
        return v->load();
    return 0.0f;
}

void CartridgeEngineAudioProcessorEditor::applyMacroQuality(float quality)
{
    const auto q = std::pow(1.0f - clamp01(quality), 1.6f);
    setParamValue("bits", (float) std::lround(14.0f - q * 10.0f));
    setParamValue("rate", std::round(42000.0f - q * 34000.0f));
    setParamValue("lpHz", std::round(16000.0f - q * 13500.0f));
    setParamValue("jitter", clamp01(0.02f + q * 0.45f));
}

void CartridgeEngineAudioProcessorEditor::applyMacroCodec(float codec)
{
    const auto c = std::pow(clamp01(codec), 1.15f);
    setParamValue("mulaw", clamp01(c * 0.95f));
    setParamValue("blockMs", std::round(c * c * 42.0f));
    setParamValue("preEmph", clamp01(0.08f + c * 0.7f));
}

void CartridgeEngineAudioProcessorEditor::applyMacroGrit(float grit)
{
    const auto g = std::pow(clamp01(grit), 1.25f);
    setParamValue("sat", clamp01(0.12f + g * 0.88f));
    setParamValue("hum", clamp01(g * 0.25f));
    setParamValue("whine", clamp01(0.08f + g * 0.7f));
    setParamValue("outGain", 0.98f - g * 0.18f);
}

void CartridgeEngineAudioProcessorEditor::applyPreset(int idx)
{
    if (idx < 0 || idx >= (int) std::size(kPresets))
        return;

    suppressMacros = true;
    for (const auto& kv : kPresets[(size_t) idx].values)
        setParamValue(kv.first, kv.second);
    suppressMacros = false;

    lastQuality = getParamValue("quality");
    lastCodec = getParamValue("codec");
    lastGrit = getParamValue("grit");
}

void CartridgeEngineAudioProcessorEditor::timerCallback()
{
    if (suppressMacros)
        return;

    const auto q = getParamValue("quality");
    const auto c = getParamValue("codec");
    const auto g = getParamValue("grit");

    suppressMacros = true;
    if (std::abs(q - lastQuality) > 0.0005f)
    {
        applyMacroQuality(q);
        lastQuality = q;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(c - lastCodec) > 0.0005f)
    {
        applyMacroCodec(c);
        lastCodec = c;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(g - lastGrit) > 0.0005f)
    {
        applyMacroGrit(g);
        lastGrit = g;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    suppressMacros = false;
}

void CartridgeEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(juce::Colour(0xff15191f), 0.0f, 0.0f, juce::Colour(0xff272f39), 0.0f, (float) getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();

    auto header = getLocalBounds().removeFromTop(82).reduced(8, 8);
    g.setColour(juce::Colour(0xff1f2630));
    g.fillRoundedRectangle(header.toFloat(), 10.0f);
    g.setColour(juce::Colour(0xff5d6672));
    g.drawRoundedRectangle(header.toFloat(), 10.0f, 1.2f);

    auto lcd = juce::Rectangle<float>((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 280.0f, 48.0f);
    g.setColour(juce::Colour(0xffffe1a8));
    g.fillRoundedRectangle(lcd, 8.0f);
    g.setColour(juce::Colour(0xff6c562f));
    g.drawRoundedRectangle(lcd, 8.0f, 1.1f);
    g.setColour(juce::Colour(0xff31280f));
    g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    g.drawFittedText("CART BUS 8-BIT", lcd.toNearestInt().reduced(14, 12), juce::Justification::centredLeft, 1);
}

void CartridgeEngineAudioProcessorEditor::resized()
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

    layoutPage(macroPage, 3);
    layoutPage(corePage, 4);
    layoutPage(vibePage, 5);
    layoutPage(bleepPage, 3);
}
