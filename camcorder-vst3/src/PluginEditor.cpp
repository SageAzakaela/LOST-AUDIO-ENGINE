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
    { "MiniDV", {
        { "coverage", 0.32f }, { "movement", 0.22f }, { "corruption", 0.18f }, { "agc", 0.38f },
        { "wind", 1.0f }, { "windLevel", 1.05f }, { "dropMode", 0.0f },
        { "hpHz", 60.0f }, { "lpHz", 9800.0f }, { "boxDb", 3.5f }, { "boxHz", 1650.0f }
    } },
    { "Windy Tape", {
        { "coverage", 0.58f }, { "movement", 0.55f }, { "corruption", 0.22f }, { "agc", 0.40f },
        { "wind", 1.0f }, { "windLevel", 1.25f }, { "dropMode", 2.0f },
        { "drop", 0.22f }, { "dropMs", 28.0f }, { "handling", 0.32f }, { "rub", 0.35f }
    } },
    { "DSLR Auto", {
        { "coverage", 0.22f }, { "movement", 0.18f }, { "corruption", 0.06f }, { "agc", 0.28f },
        { "wind", 1.0f }, { "windLevel", 0.85f }, { "dropMode", 2.0f },
        { "bits", 14.0f }, { "rate", 32000.0f }, { "clip", 0.12f }, { "hiss", 0.08f }
    } },
    { "Action Cam", {
        { "coverage", 0.15f }, { "movement", 0.62f }, { "corruption", 0.12f }, { "agc", 0.55f },
        { "wind", 1.0f }, { "windLevel", 1.35f },
        { "handling", 0.55f }, { "rub", 0.32f }, { "clip", 0.55f }, { "outGain", 1.10f }
    } },
    { "Found Footage", {
        { "coverage", 0.72f }, { "movement", 0.35f }, { "corruption", 0.75f }, { "agc", 0.45f },
        { "wind", 1.0f }, { "windLevel", 1.15f }, { "dropMode", 3.0f },
        { "drop", 0.65f }, { "dropMs", 70.0f }, { "repeatMs", 90.0f }, { "chirp", 0.45f }, { "ceiling", 0.86f }
    } },
    { "Pocket Cam", {
        { "coverage", 0.45f }, { "movement", 0.22f }, { "corruption", 0.20f }, { "agc", 0.32f },
        { "wind", 1.0f }, { "windLevel", 1.0f }, { "dropMode", 0.0f },
        { "bits", 12.0f }, { "rate", 22000.0f }, { "hiss", 0.14f }, { "boxDb", 4.2f }
    } },
};

float clamp01(float x)
{
    return juce::jlimit(0.0f, 1.0f, x);
}
} // namespace

CamcorderEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff95e9ff));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff96dfff));
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4f95b8));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2f343a));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff41474d));
    addAndMakeVisible(slider);

    attachment = std::make_unique<APVTS::SliderAttachment>(state, paramID, slider);
}

void CamcorderEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(20));
    slider.setBounds(area.reduced(2));
}

CamcorderEngineAudioProcessorEditor::Switch::Switch(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    button.setButtonText(text);
    button.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd6dde5));
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff96dfff));
    addAndMakeVisible(button);
    attachment = std::make_unique<APVTS::ButtonAttachment>(state, paramID, button);
}

void CamcorderEngineAudioProcessorEditor::Switch::resized()
{
    button.setBounds(getLocalBounds());
}

CamcorderEngineAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff95e9ff));
    addAndMakeVisible(label);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(paramID)))
        for (int i = 0; i < p->choices.size(); ++i)
            combo.addItem(p->choices[i], i + 1);

    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff20262d));
    combo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff434b54));
    addAndMakeVisible(combo);
    attachment = std::make_unique<APVTS::ComboBoxAttachment>(state, paramID, combo);
}

void CamcorderEngineAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    combo.setBounds(area.reduced(1));
}

CamcorderEngineAudioProcessorEditor::CamcorderEngineAudioProcessorEditor(CamcorderEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.getAPVTS())
{
    title.setText("Camcorder Engine", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(29.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xffe7edf3));
    addAndMakeVisible(title);

    subtitle.setText("Compact Hi8 / MiniDV Console", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff95a3b0));
    addAndMakeVisible(subtitle);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff95e9ff));
    addAndMakeVisible(presetLabel);

    presetBox.addItem("Custom", 1);
    for (int i = 0; i < (int) std::size(kPresets); ++i)
        presetBox.addItem(kPresets[i].name, i + 2);
    presetBox.onChange = [this]()
    {
        const auto id = presetBox.getSelectedId();
        if (id >= 2)
            applyPreset(id - 2);
    };
    presetBox.setSelectedId(1, juce::dontSendNotification);
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xffb4e8ff));
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xff15384a));
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff476d82));
    presetBox.setTooltip("Factory camcorder profiles for quick scene starts.");
    addAndMakeVisible(presetBox);

    tabs.setOutline(0);
    tabs.setTabBarDepth(34);
    tabs.addTab("Macro", juce::Colour(0xff2f3944), &macroPage, true);
    tabs.addTab("Tone", juce::Colour(0xff2f3944), &tonePage, true);
    tabs.addTab("Damage", juce::Colour(0xff2f3944), &damagePage, true);
    tabs.addTab("Motion", juce::Colour(0xff2f3944), &motionPage, true);
    addAndMakeVisible(tabs);

    addKnob(macroPage, "coverage", "Coverage", "How blocked/occluded the capsule sounds. More = narrower, boxed tone.");
    addKnob(macroPage, "movement", "Movement", "Camera handling movement. Raises thumps, wiggles and wind activity.");
    addKnob(macroPage, "corruption", "Corruption", "Digital errors and unstable codec behavior intensity.");
    addKnob(macroPage, "agc", "AGC Drive", "Auto gain/preamp character. More = louder pumping and preamp grit.");
    addSwitch(macroPage, "wind", "Wind Enable", "Enable simulated wind bursts hitting the on-camera mic.");
    addKnob(macroPage, "windLevel", "Wind Level", "Overall strength of wind events when enabled.");

    addKnob(tonePage, "hpHz", "HP", "Cuts low rumble from handling and body coupling.");
    addKnob(tonePage, "lpHz", "LP", "Top-end rolloff from camcorder mic bandwidth limits.");
    addKnob(tonePage, "boxDb", "Box dB", "Nasal mid bump from plastic body/mic cavity resonance.");
    addKnob(tonePage, "boxHz", "Box Hz", "Center frequency of that camcorder body resonance.");
    addKnob(tonePage, "outGain", "Out", "Final output trim after coloration.");

    addKnob(damagePage, "agcAmt", "AGC Amt", "How strongly leveler compression reacts to signal level.");
    addKnob(damagePage, "agcSpeed", "AGC Speed", "Attack/release speed of AGC pumping.");
    addKnob(damagePage, "clip", "Clip", "Harsh preamp clipping when transients hit too hard.");
    addKnob(damagePage, "crush", "Crush", "Bit-depth quantization blend for lo-fi converter texture.");
    addKnob(damagePage, "bits", "Bits", "Target converter resolution.");
    addKnob(damagePage, "rate", "Rate", "Sample-rate reduction for digital roughness.");
    addKnob(damagePage, "hiss", "Hiss", "High-frequency sensor/preamp hiss layer.");
    addKnob(damagePage, "ceiling", "Ceiling", "Limiter ceiling to prevent overs and hard clipping.");

    addKnob(motionPage, "drop", "Drop", "Chance of short packet/ADC style signal dropouts.");
    addKnob(motionPage, "dropMs", "Drop ms", "Length of each dropout.");
    addChoice(motionPage, "dropMode", "Drop Mode", "Hold repeats last good sample. Interp crossfades. Repeat replays earlier audio.");
    addKnob(motionPage, "repeatMs", "Repeat ms", "History offset used by Repeat concealment mode.");
    addKnob(motionPage, "chirp", "Chirp", "Digital chirps and codec squeals.");
    addKnob(motionPage, "handling", "Handling", "Low-frequency bumps from hand contact and body knocks.");
    addKnob(motionPage, "rub", "Rub", "Friction/cloth scraping noises against the mic body.");

    setResizable(false, false);
    setSize(840, 520);

    lastCoverage = getParamValue("coverage");
    lastMovement = getParamValue("movement");
    lastCorruption = getParamValue("corruption");
    lastAgc = getParamValue("agc");

    startTimerHz(18);
}

CamcorderEngineAudioProcessorEditor::~CamcorderEngineAudioProcessorEditor() {}

void CamcorderEngineAudioProcessorEditor::addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Knob>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    knobs.push_back(std::move(c));
}

void CamcorderEngineAudioProcessorEditor::addSwitch(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Switch>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    switches.push_back(std::move(c));
}

void CamcorderEngineAudioProcessorEditor::addChoice(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Choice>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    choices.push_back(std::move(c));
}

void CamcorderEngineAudioProcessorEditor::layoutPage(juce::Component& page, int columns)
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

void CamcorderEngineAudioProcessorEditor::setParamValue(const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
}

float CamcorderEngineAudioProcessorEditor::getParamValue(const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue(id))
        return v->load();
    return 0.0f;
}

void CamcorderEngineAudioProcessorEditor::applyMacroCoverage(float v)
{
    const auto cov = std::pow(clamp01(v), 1.3f);
    setParamValue("lpHz", std::round(15000.0f - cov * 12000.0f));
    setParamValue("hpHz", std::round(45.0f + cov * 70.0f));
    setParamValue("boxDb", std::round((2.2f + cov * 6.5f) * 20.0f) / 20.0f);
    setParamValue("boxHz", std::round(1500.0f + cov * 250.0f));
    setParamValue("hiss", clamp01(0.06f + cov * 0.18f + std::pow(clamp01(getParamValue("corruption")), 1.3f) * 0.08f));
    setParamValue("wind", (getParamValue("movement") > 0.55f && v > 0.45f) ? 1.0f : 0.0f);
}

void CamcorderEngineAudioProcessorEditor::applyMacroMovement(float v)
{
    const auto mov = std::pow(clamp01(v), 1.25f);
    setParamValue("handling", clamp01(0.06f + mov * 0.7f));
    setParamValue("rub", clamp01(0.04f + mov * 0.7f));
    setParamValue("windLevel", (mov > 0.55f) ? 1.05f : 0.95f);
    setParamValue("wind", (mov > 0.55f && getParamValue("coverage") > 0.45f) ? 1.0f : 0.0f);
}

void CamcorderEngineAudioProcessorEditor::applyMacroCorruption(float v)
{
    const auto cor = std::pow(clamp01(v), 1.3f);
    setParamValue("crush", clamp01(0.05f + cor * 0.45f));
    setParamValue("bits", (float) std::lround(14.0f - cor * 6.0f));
    setParamValue("rate", std::round(42000.0f - cor * 26000.0f));
    setParamValue("drop", clamp01(0.05f + cor * 0.8f));
    setParamValue("dropMs", std::round(16.0f + cor * 120.0f));
    setParamValue("dropMode", cor > 0.6f ? 3.0f : (cor > 0.22f ? 0.0f : 2.0f));
    setParamValue("repeatMs", std::round(30.0f + cor * 120.0f));
    setParamValue("chirp", clamp01(cor * 0.6f));
    setParamValue("hiss", clamp01(0.06f + std::pow(clamp01(getParamValue("coverage")), 1.3f) * 0.18f + cor * 0.08f));
}

void CamcorderEngineAudioProcessorEditor::applyMacroAgc(float v)
{
    const auto drv = std::pow(clamp01(v), 1.25f);
    setParamValue("agcAmt", clamp01(0.45f + drv * 0.45f + std::pow(clamp01(getParamValue("coverage")), 1.3f) * 0.15f));
    setParamValue("agcSpeed", clamp01(0.25f + drv * 0.5f));
    setParamValue("clip", clamp01(0.08f + drv * 0.75f));
    setParamValue("ceiling", 0.94f - drv * 0.1f);
    setParamValue("outGain", std::round((0.98f + drv * 0.18f) * 100.0f) / 100.0f);
}

void CamcorderEngineAudioProcessorEditor::applyPreset(int idx)
{
    if (idx < 0 || idx >= (int) std::size(kPresets))
        return;

    suppressMacros = true;
    for (const auto& kv : kPresets[(size_t) idx].values)
        setParamValue(kv.first, kv.second);
    suppressMacros = false;

    lastCoverage = getParamValue("coverage");
    lastMovement = getParamValue("movement");
    lastCorruption = getParamValue("corruption");
    lastAgc = getParamValue("agc");
}

void CamcorderEngineAudioProcessorEditor::timerCallback()
{
    if (suppressMacros)
        return;

    const auto coverage = getParamValue("coverage");
    const auto movement = getParamValue("movement");
    const auto corruption = getParamValue("corruption");
    const auto agc = getParamValue("agc");

    suppressMacros = true;
    if (std::abs(coverage - lastCoverage) > 0.0005f)
    {
        applyMacroCoverage(coverage);
        lastCoverage = coverage;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(movement - lastMovement) > 0.0005f)
    {
        applyMacroMovement(movement);
        lastMovement = movement;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(corruption - lastCorruption) > 0.0005f)
    {
        applyMacroCorruption(corruption);
        lastCorruption = corruption;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(agc - lastAgc) > 0.0005f)
    {
        applyMacroAgc(agc);
        lastAgc = agc;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    suppressMacros = false;
}

void CamcorderEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(juce::Colour(0xff13171c), 0.0f, 0.0f, juce::Colour(0xff27303a), 0.0f, (float) getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();

    auto header = getLocalBounds().removeFromTop(82).reduced(8, 8);
    g.setColour(juce::Colour(0xff1f252d));
    g.fillRoundedRectangle(header.toFloat(), 10.0f);
    g.setColour(juce::Colour(0xff59626b));
    g.drawRoundedRectangle(header.toFloat(), 10.0f, 1.2f);

    auto lcd = juce::Rectangle<float>((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 280.0f, 48.0f);
    g.setColour(juce::Colour(0xffbdeeff));
    g.fillRoundedRectangle(lcd, 8.0f);
    g.setColour(juce::Colour(0xff2d5e70));
    g.drawRoundedRectangle(lcd, 8.0f, 1.1f);

    g.setColour(juce::Colour(0xff0f161b));
    g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    g.drawFittedText("REC 01:23:47", lcd.toNearestInt().reduced(14, 12), juce::Justification::centredLeft, 1);

    g.setColour(juce::Colour(0xff0d1217));
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 11; ++x)
            g.fillEllipse((float) (header.getX() + 420 + x * 10), (float) (header.getY() + 16 + y * 10), 4.0f, 4.0f);
}

void CamcorderEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    auto header = area.removeFromTop(74);

    auto left = header.removeFromLeft(400);
    title.setBounds(left.removeFromTop(42).withTrimmedLeft(18));
    subtitle.setBounds(left.withTrimmedLeft(20));

    auto right = header.withTrimmedLeft(26);
    presetLabel.setBounds(right.removeFromTop(18));
    presetBox.setBounds(right.removeFromTop(30).removeFromLeft(240));

    tabs.setBounds(area);

    layoutPage(macroPage, 3);
    layoutPage(tonePage, 3);
    layoutPage(damagePage, 4);
    layoutPage(motionPage, 4);
}
