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
    { "Corner Club", {
        { "hotMic", 0.52f }, { "fbFreq", 1700.0f }, { "ringQ", 14.0f }, { "fbDelayMs", 23.0f }, { "fbTone", 0.58f },
        { "wall", 0.62f }, { "room", 0.50f }, { "limit", 0.62f }, { "outGain", 0.95f }
    } },
    { "Rooftop Night", {
        { "hotMic", 0.48f }, { "fbFreq", 2200.0f }, { "ringQ", 12.0f }, { "fbDelayMs", 19.0f }, { "fbTone", 0.70f },
        { "wall", 0.44f }, { "room", 0.68f }, { "limit", 0.45f }, { "outGain", 1.02f }
    } },
    { "Harsh Monitor", {
        { "hotMic", 0.78f }, { "fbFreq", 2500.0f }, { "ringQ", 22.0f }, { "fbDelayMs", 13.0f }, { "fbTone", 0.74f },
        { "wall", 0.32f }, { "room", 0.30f }, { "limit", 0.78f }, { "outGain", 0.88f }
    } },
    { "Rainy Patio", {
        { "hotMic", 0.42f }, { "fbFreq", 1450.0f }, { "ringQ", 10.0f }, { "fbDelayMs", 31.0f }, { "fbTone", 0.42f },
        { "wall", 0.75f }, { "room", 0.72f }, { "limit", 0.56f }, { "outGain", 1.0f }
    } },
};

float clamp01(float x)
{
    return juce::jlimit(0.0f, 1.0f, x);
}
} // namespace

OpenMicNightAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& id, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff9be8ff));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff8bdfff));
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4f9dbf));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff333941));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff444b54));
    addAndMakeVisible(slider);

    att = std::make_unique<APVTS::SliderAttachment>(state, id, slider);
}

void OpenMicNightAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(20));
    slider.setBounds(area.reduced(2));
}

OpenMicNightAudioProcessorEditor::Switch::Switch(APVTS& state, const juce::String& id, const juce::String& text)
{
    button.setButtonText(text);
    button.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd6dde5));
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff8bdfff));
    addAndMakeVisible(button);
    att = std::make_unique<APVTS::ButtonAttachment>(state, id, button);
}

void OpenMicNightAudioProcessorEditor::Switch::resized()
{
    button.setBounds(getLocalBounds());
}

OpenMicNightAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& id, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff9be8ff));
    addAndMakeVisible(label);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id)))
        for (int i = 0; i < p->choices.size(); ++i)
            combo.addItem(p->choices[i], i + 1);

    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff212830));
    combo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff4a515a));
    addAndMakeVisible(combo);
    att = std::make_unique<APVTS::ComboBoxAttachment>(state, id, combo);
}

void OpenMicNightAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    combo.setBounds(area.reduced(1));
}

OpenMicNightAudioProcessorEditor::OpenMicNightAudioProcessorEditor(OpenMicNightAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.getAPVTS())
{
    title.setText("Open Mic Night", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(29.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xffeef4fa));
    addAndMakeVisible(title);

    subtitle.setText("Compact Stage Spill Console", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff98a6b4));
    addAndMakeVisible(subtitle);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9be8ff));
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
    presetBox.setTooltip("Quick live venue setups.");
    addAndMakeVisible(presetBox);

    tabs.setOutline(0);
    tabs.setTabBarDepth(34);
    tabs.addTab("Macro", juce::Colour(0xff2f3a45), &macroPage, true);
    tabs.addTab("Feedback", juce::Colour(0xff2f3a45), &feedbackPage, true);
    tabs.addTab("Space", juce::Colour(0xff2f3a45), &spacePage, true);
    addAndMakeVisible(tabs);

    addKnob(macroPage, "hotMic", "Intensity", "How aggressively the mic feeds back and rings.");
    addKnob(macroPage, "wall", "Distance", "More wall means more muffled/behind-wall tone.");
    addKnob(macroPage, "room", "Space", "How large and reverberant the venue feels.");
    addKnob(macroPage, "outGain", "Out", "Final output trim.");

    addKnob(feedbackPage, "fbFreq", "Freq", "Center feedback howl frequency.");
    addKnob(feedbackPage, "ringQ", "Q", "How narrow and sharp the ring tone is.");
    addKnob(feedbackPage, "fbDelayMs", "Delay", "Short monitor path delay inside feedback loop.");
    addKnob(feedbackPage, "fbTone", "Tone", "Brightness of feedback path.");

    addKnob(spacePage, "wall", "Wall", "Filters source as if heard through venue structure.");
    addKnob(spacePage, "room", "Room", "Room bloom/reverb amount.");
    addKnob(spacePage, "limit", "Limit", "Limiter blend for taming peaks.");

    setResizable(false, false);
    setSize(820, 520);

    lastIntensity = getParamValue("hotMic");
    lastDistance = getParamValue("wall");

    startTimerHz(18);
}

OpenMicNightAudioProcessorEditor::~OpenMicNightAudioProcessorEditor() {}

void OpenMicNightAudioProcessorEditor::addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Knob>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    knobs.push_back(std::move(c));
}

void OpenMicNightAudioProcessorEditor::addSwitch(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Switch>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    switches.push_back(std::move(c));
}

void OpenMicNightAudioProcessorEditor::addChoice(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Choice>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    choices.push_back(std::move(c));
}

void OpenMicNightAudioProcessorEditor::layoutPage(juce::Component& page, int columns)
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

void OpenMicNightAudioProcessorEditor::setParamValue(const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
}

float OpenMicNightAudioProcessorEditor::getParamValue(const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue(id))
        return v->load();
    return 0.0f;
}

void OpenMicNightAudioProcessorEditor::applyMacroIntensity(float v)
{
    const auto x = clamp01(v);
    setParamValue("ringQ", 6.0f + x * 24.0f);
    setParamValue("fbFreq", 900.0f + x * 2600.0f);
    setParamValue("fbDelayMs", 30.0f - x * 16.0f);
    setParamValue("limit", 0.35f + x * 0.45f);
}

void OpenMicNightAudioProcessorEditor::applyMacroDistance(float v)
{
    const auto d = clamp01(v);
    setParamValue("wall", d);
    setParamValue("room", 0.25f + d * 0.65f);
    setParamValue("fbTone", 0.8f - d * 0.45f);
}

void OpenMicNightAudioProcessorEditor::applyPreset(int idx)
{
    if (idx < 0 || idx >= (int) std::size(kPresets))
        return;

    suppressMacros = true;
    for (const auto& kv : kPresets[(size_t) idx].values)
        setParamValue(kv.first, kv.second);
    suppressMacros = false;

    lastIntensity = getParamValue("hotMic");
    lastDistance = getParamValue("wall");
}

void OpenMicNightAudioProcessorEditor::timerCallback()
{
    if (suppressMacros)
        return;

    const auto i = getParamValue("hotMic");
    const auto d = getParamValue("wall");

    suppressMacros = true;
    if (std::abs(i - lastIntensity) > 0.0005f)
    {
        applyMacroIntensity(i);
        lastIntensity = i;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(d - lastDistance) > 0.0005f)
    {
        applyMacroDistance(d);
        lastDistance = d;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    suppressMacros = false;
}

void OpenMicNightAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(juce::Colour(0xff161b22), 0.0f, 0.0f, juce::Colour(0xff252f3a), 0.0f, (float) getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();

    auto header = getLocalBounds().removeFromTop(82).reduced(8, 8);
    g.setColour(juce::Colour(0xff202a34));
    g.fillRoundedRectangle(header.toFloat(), 10.0f);
    g.setColour(juce::Colour(0xff556270));
    g.drawRoundedRectangle(header.toFloat(), 10.0f, 1.2f);

    auto lcd = juce::Rectangle<float>((float) header.getRight() - 280.0f, (float) header.getY() + 10.0f, 260.0f, 48.0f);
    g.setColour(juce::Colour(0xffbcecff));
    g.fillRoundedRectangle(lcd, 8.0f);
    g.setColour(juce::Colour(0xff2f6176));
    g.drawRoundedRectangle(lcd, 8.0f, 1.1f);
    g.setColour(juce::Colour(0xff123240));
    g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    g.drawFittedText("OPEN MIC • LIVE", lcd.toNearestInt().reduced(14, 12), juce::Justification::centredLeft, 1);
}

void OpenMicNightAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    auto header = area.removeFromTop(74);

    auto left = header.removeFromLeft(390);
    title.setBounds(left.removeFromTop(42).withTrimmedLeft(18));
    subtitle.setBounds(left.withTrimmedLeft(20));

    auto right = header.withTrimmedLeft(26);
    presetLabel.setBounds(right.removeFromTop(18));
    presetBox.setBounds(right.removeFromTop(30).removeFromLeft(220));

    tabs.setBounds(area);
    layoutPage(macroPage, 4);
    layoutPage(feedbackPage, 4);
    layoutPage(spacePage, 3);
}

