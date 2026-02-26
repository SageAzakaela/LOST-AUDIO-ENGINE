#include "PluginEditor.h"

namespace
{
struct PresetDef
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;
};

static const PresetDef kPresets[] = {
    { "Cheap AM Radio", {
        { "bandwidth", 0.30f }, { "drive", 0.28f }, { "badConnection", 0.18f }, { "noiseProfile", 0.24f },
        { "hpHz", 430.0f }, { "lpHz", 3600.0f }, { "midGainDb", 2.5f },
        { "comp", 0.32f }, { "crush", 0.08f }, { "outGain", 0.92f }, { "passes", 1.0f }
    } },
    { "Police Scanner", {
        { "bandwidth", 0.22f }, { "drive", 0.45f }, { "badConnection", 0.30f }, { "noiseProfile", 0.35f },
        { "hpHz", 520.0f }, { "lpHz", 3100.0f }, { "midGainDb", 3.8f }, { "midQ", 1.6f },
        { "comp", 0.44f }, { "crush", 0.10f }, { "passes", 2.0f }
    } },
    { "Storm Transmission", {
        { "bandwidth", 0.36f }, { "drive", 0.38f }, { "badConnection", 0.70f }, { "noiseProfile", 0.64f },
        { "wowDepth", 0.70f }, { "dropRate", 0.75f }, { "dropDepth", 0.65f }, { "crackle", 0.55f },
        { "noiseColor", 0.80f }, { "hiss", 0.62f }, { "passes", 2.0f }
    } },
    { "Narrowband Crunch", {
        { "bandwidth", 0.18f }, { "drive", 0.62f }, { "badConnection", 0.42f }, { "noiseProfile", 0.40f },
        { "hpHz", 600.0f }, { "lpHz", 2800.0f }, { "crush", 0.22f }, { "passes", 2.0f }
    } },
    { "Squelch Hunt", {
        { "bandwidth", 0.28f }, { "drive", 0.33f }, { "badConnection", 0.40f }, { "noiseProfile", 0.45f },
        { "tuningEnable", 1.0f }, { "tuningMode", 1.0f }, { "tuningSource", 7.0f }, { "tuningAmount", 0.52f },
        { "tuningSnippetMs", 160.0f }, { "tuningCutDepth", 0.60f }, { "passes", 1.0f }
    } },
    { "Dispatch Hot", {
        { "bandwidth", 0.26f }, { "drive", 0.50f }, { "badConnection", 0.34f }, { "noiseProfile", 0.28f },
        { "tuningEnable", 1.0f }, { "tuningMode", 0.0f },
        { "tuningSource", 1.0f }, { "tuningAmount", 0.35f }, { "passes", 2.0f }
    } },
};
} // namespace

TransmissionEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff95dfff));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff86d7ff));
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4a9fcd));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2b3238));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff3a3f45));
    addAndMakeVisible(slider);

    attachment = std::make_unique<APVTS::SliderAttachment>(state, paramID, slider);
}

void TransmissionEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(20));
    slider.setBounds(area.reduced(2));
}

TransmissionEngineAudioProcessorEditor::Switch::Switch(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    button.setButtonText(text);
    button.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd2d7dc));
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff86d7ff));
    addAndMakeVisible(button);

    attachment = std::make_unique<APVTS::ButtonAttachment>(state, paramID, button);
}

void TransmissionEngineAudioProcessorEditor::Switch::resized()
{
    button.setBounds(getLocalBounds());
}

TransmissionEngineAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff95dfff));
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

void TransmissionEngineAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    combo.setBounds(area.reduced(1));
}

TransmissionEngineAudioProcessorEditor::BoomboxLookAndFeel::BoomboxLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff12171c));
    setColour(juce::TabbedButtonBar::tabOutlineColourId, juce::Colour(0xff000000));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a323b));
}

TransmissionEngineAudioProcessorEditor::TransmissionEngineAudioProcessorEditor(TransmissionEngineAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      apvts(p.getAPVTS())
{
    setLookAndFeel(&boomboxLnf);

    title.setText("Transmission Engine", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xffdfe8f3));
    addAndMakeVisible(title);

    subtitle.setText("90s Digital Radio Rack", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff9ba8b5));
    addAndMakeVisible(subtitle);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff95dfff));
    addAndMakeVisible(presetLabel);

    presetBox.addItem("Custom", 1);
    for (int i = 0; i < (int) std::size(kPresets); ++i)
        presetBox.addItem(kPresets[i].name, i + 2);
    presetBox.onChange = [this]
    {
        const auto id = presetBox.getSelectedId();
        if (id >= 2)
            applyPreset(id - 2);
    };
    presetBox.setSelectedId(1, juce::dontSendNotification);
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff9feaff));
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xff083542));
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2b5b67));
    presetBox.setTooltip("Factory radio presets mapped for fast scene setup.");
    addAndMakeVisible(presetBox);

    tabs.setOutline(0);
    tabs.setTabBarDepth(34);
    tabs.addTab("Macro", juce::Colour(0xff2c3640), &macroPage, true);
    tabs.addTab("Tone", juce::Colour(0xff2c3640), &tonePage, true);
    tabs.addTab("Damage", juce::Colour(0xff2c3640), &damagePage, true);
    tabs.addTab("Tuning", juce::Colour(0xff2c3640), &tuningPage, true);
    addAndMakeVisible(tabs);

    addKnob(macroPage, "bandwidth", "Bandwidth", "Narrow bandwidth removes lows/highs and boosts radio mid focus.");
    addKnob(macroPage, "drive", "Drive", "Adds nonlinear transmitter saturation and grit.");
    addKnob(macroPage, "badConnection", "Bad Conn", "Controls wow, dropouts and crackle intensity.");
    addKnob(macroPage, "noiseProfile", "Noise", "Global static/noise amount and hiss tendency.");
    addKnob(macroPage, "outGain", "Out Gain", "Final output trim after coloration.");
    addKnob(macroPage, "passes", "Passes", "Stacks the radio stage multiple times for heavier character.");

    addKnob(tonePage, "hpHz", "HP", "Cuts low-end rumble to mimic tiny speakers.");
    addKnob(tonePage, "lpHz", "LP", "Cuts highs for narrow-band transmission tone.");
    addKnob(tonePage, "midGainDb", "Mid Gain", "Boosts the vocal presence region.");
    addKnob(tonePage, "midFreq", "Mid Freq", "Center frequency of the vocal presence bump.");
    addKnob(tonePage, "midQ", "Mid Q", "Bandwidth of the mid boost/cut shape.");
    addKnob(tonePage, "boxDipDb", "Box Dip", "Scoops cardboard resonance before saturation.");

    addKnob(damagePage, "comp", "Comp", "Tightens peaks like limited transmitter headroom.");
    addKnob(damagePage, "asym", "Asym", "Bias distortion for asymmetric clipping character.");
    addKnob(damagePage, "crush", "Crush", "Bit-depth/downsample aliasing for cheap digital radio feel.");
    addKnob(damagePage, "wowDepth", "Wow", "Slow gain wobble simulating unstable circuitry.");
    addKnob(damagePage, "dropRate", "Drop Rate", "How often signal dropouts occur.");
    addKnob(damagePage, "dropDepth", "Drop Depth", "How deep each dropout becomes.");
    addKnob(damagePage, "crackle", "Crackle", "Transient dust/spark events.");
    addKnob(damagePage, "lfoRate", "LFO", "Speed of wow modulation.");
    addKnob(damagePage, "noiseColor", "Noise Color", "White-to-pink tilt of background noise.");
    addKnob(damagePage, "hiss", "Hiss", "High-frequency hiss emphasis.");

    addSwitch(tuningPage, "tuningEnable", "Tuning Enable", "Inject tuning/search artifacts.");
    addChoice(tuningPage, "tuningMode", "Mode", "Edges: event-triggered. Search: random scan activity.");
    addChoice(tuningPage, "tuningSource", "Source", "Synth or built-in SFX snippets.");
    addKnob(tuningPage, "tuningAmount", "Amount", "Strength and event probability of tuning artifacts.");
    addKnob(tuningPage, "tuningSnippetMs", "Snippet", "Length of each tuning artifact.");
    addKnob(tuningPage, "tuningCutDepth", "Cut", "How much base signal ducks during search events.");

    setResizable(false, false);
    setSize(860, 540);

    lastBandwidth = getParamValue("bandwidth");
    lastDrive = getParamValue("drive");
    lastBad = getParamValue("badConnection");
    lastNoise = getParamValue("noiseProfile");

    startTimerHz(18);
}

TransmissionEngineAudioProcessorEditor::~TransmissionEngineAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void TransmissionEngineAudioProcessorEditor::addKnob(juce::Component& page, const juce::String& paramID, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Knob>(apvts, paramID, text);
    c->setHint(hint);
    if (! paramID.isEmpty())
        knobByParam[paramID.toStdString()] = c.get();
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    knobs.push_back(std::move(c));
}

void TransmissionEngineAudioProcessorEditor::addSwitch(juce::Component& page, const juce::String& paramID, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Switch>(apvts, paramID, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    switches.push_back(std::move(c));
}

void TransmissionEngineAudioProcessorEditor::addChoice(juce::Component& page, const juce::String& paramID, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Choice>(apvts, paramID, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    choices.push_back(std::move(c));
}

void TransmissionEngineAudioProcessorEditor::layoutPage(juce::Component& page, int columns)
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

void TransmissionEngineAudioProcessorEditor::setParamValue(const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
}

float TransmissionEngineAudioProcessorEditor::getParamValue(const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue(id))
        return v->load();
    return 0.0f;
}

void TransmissionEngineAudioProcessorEditor::applyMacroBandwidth(float bw)
{
    const auto hp = 600.0f - bw * 400.0f;
    const auto lp = 2500.0f + bw * 3500.0f;
    const auto midGain = (1.0f - bw) * 5.2f;
    const auto midQ = 0.9f + (1.0f - bw) * 1.2f;
    setParamValue("hpHz", hp);
    setParamValue("lpHz", lp);
    setParamValue("midGainDb", midGain);
    setParamValue("midQ", midQ);
    setParamValue("midFreq", 1550.0f);
    setParamValue("boxDipDb", (1.0f - bw) * 2.2f);
}

void TransmissionEngineAudioProcessorEditor::applyMacroDrive(float drive)
{
    setParamValue("asym", drive * 0.6f);
    setParamValue("comp", 0.18f + drive * 0.65f);
}

void TransmissionEngineAudioProcessorEditor::applyMacroBad(float bad)
{
    setParamValue("wowDepth", bad);
    setParamValue("dropRate", bad);
    setParamValue("dropDepth", bad);
    setParamValue("crackle", bad);
    setParamValue("lfoRate", 0.45f + bad * 1.6f);
}

void TransmissionEngineAudioProcessorEditor::applyMacroNoise(float noise)
{
    setParamValue("hiss", noise * 0.95f);
    setParamValue("noiseColor", juce::jmax(0.0f, (noise - 0.55f) * 2.0f));
}

void TransmissionEngineAudioProcessorEditor::applyPreset(int idx)
{
    if (idx < 0 || idx >= (int) std::size(kPresets))
        return;

    suppressMacros = true;
    for (const auto& kv : kPresets[(size_t) idx].values)
        setParamValue(kv.first, kv.second);
    suppressMacros = false;

    lastBandwidth = getParamValue("bandwidth");
    lastDrive = getParamValue("drive");
    lastBad = getParamValue("badConnection");
    lastNoise = getParamValue("noiseProfile");
}

void TransmissionEngineAudioProcessorEditor::timerCallback()
{
    if (suppressMacros)
        return;

    const auto bw = getParamValue("bandwidth");
    const auto dr = getParamValue("drive");
    const auto bad = getParamValue("badConnection");
    const auto ns = getParamValue("noiseProfile");

    suppressMacros = true;
    if (std::abs(bw - lastBandwidth) > 0.0005f)
    {
        applyMacroBandwidth(bw);
        lastBandwidth = bw;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(dr - lastDrive) > 0.0005f)
    {
        applyMacroDrive(dr);
        lastDrive = dr;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(bad - lastBad) > 0.0005f)
    {
        applyMacroBad(bad);
        lastBad = bad;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    if (std::abs(ns - lastNoise) > 0.0005f)
    {
        applyMacroNoise(ns);
        lastNoise = ns;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
    suppressMacros = false;
}

void TransmissionEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(juce::Colour(0xff151b21), 0.0f, 0.0f, juce::Colour(0xff242d36), 0.0f, (float) getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();

    auto header = getLocalBounds().removeFromTop(82).reduced(8, 8);
    g.setColour(juce::Colour(0xff202831));
    g.fillRoundedRectangle(header.toFloat(), 10.0f);
    g.setColour(juce::Colour(0xff4a5a68));
    g.drawRoundedRectangle(header.toFloat(), 10.0f, 1.2f);

    g.setColour(juce::Colour(0xff7fcfe6));
    g.fillRoundedRectangle((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 278.0f, 48.0f, 8.0f);
    g.setColour(juce::Colour(0xff193740));
    g.drawRoundedRectangle(juce::Rectangle<float>((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 278.0f, 48.0f), 8.0f, 1.0f);

    const int grillRadius = 5;
    g.setColour(juce::Colour(0xff0f1418));
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 12; ++x)
            g.fillEllipse((float) (header.getX() + 430 + x * 10), (float) (header.getY() + 16 + y * 10), (float) grillRadius, (float) grillRadius);
}

void TransmissionEngineAudioProcessorEditor::resized()
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
    layoutPage(tonePage, 3);
    layoutPage(damagePage, 5);
    layoutPage(tuningPage, 3);
}
