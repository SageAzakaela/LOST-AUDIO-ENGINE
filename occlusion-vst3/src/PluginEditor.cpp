#include "PluginEditor.h"

namespace
{
float clampf(float x, float lo, float hi)
{
    return juce::jlimit(lo, hi, x);
}

struct MaterialDefaults
{
    float lpMin;
    float dipHz;
    float dipDb;
    float bumpHz;
    float bumpDb;
    float damp;
    float leakBias;
};

MaterialDefaults getMaterialDefaults(int material)
{
    switch (material)
    {
        case 1: return { 1100.0f, 1650.0f, -3.0f, 420.0f, 1.8f, 0.75f, -0.05f }; // brick
        case 2: return { 1500.0f, 1450.0f, -2.2f, 520.0f, 1.6f, 0.65f, 0.02f }; // wood
        case 3: return { 2200.0f, 1800.0f, -1.2f, 360.0f, 0.9f, 0.55f, 0.18f }; // curtain
        case 4: return { 1350.0f, 1550.0f, -2.6f, 380.0f, 1.4f, 0.72f, 0.08f }; // door
        case 5: return { 2600.0f, 1250.0f, -1.0f, 900.0f, 1.2f, 0.45f, 0.25f }; // glass
        default: return { 1800.0f, 1600.0f, -2.0f, 420.0f, 1.2f, 0.68f, 0.05f }; // drywall
    }
}

struct PresetDef
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;
};

static const PresetDef kPresets[] = {
    { "Subtle Room", { { "distance", 0.20f }, { "wall", 0.20f }, { "material", 0.0f }, { "sourceRoom", 0.25f }, { "listenerRoom", 0.35f }, { "leak", 0.12f }, { "roomMix", 0.14f }, { "predelayMs", 10.0f }, { "outGain", 1.0f } } },
    { "Next Room", { { "distance", 0.45f }, { "wall", 0.55f }, { "material", 0.0f }, { "sourceRoom", 0.35f }, { "listenerRoom", 0.55f }, { "leak", 0.08f }, { "roomMix", 0.22f }, { "predelayMs", 12.0f }, { "outGain", 1.0f } } },
    { "Behind Door", { { "distance", 0.40f }, { "wall", 0.60f }, { "material", 4.0f }, { "sourceRoom", 0.35f }, { "listenerRoom", 0.45f }, { "leak", 0.12f }, { "roomMix", 0.18f }, { "predelayMs", 9.0f }, { "outGain", 1.0f } } },
    { "Brick Muffle", { { "distance", 0.55f }, { "wall", 0.75f }, { "material", 1.0f }, { "sourceRoom", 0.35f }, { "listenerRoom", 0.50f }, { "leak", 0.05f }, { "roomMix", 0.26f }, { "predelayMs", 14.0f }, { "outGain", 1.02f } } },
    { "Curtain Leak", { { "distance", 0.35f }, { "wall", 0.25f }, { "material", 3.0f }, { "sourceRoom", 0.30f }, { "listenerRoom", 0.55f }, { "leak", 0.22f }, { "roomMix", 0.24f }, { "predelayMs", 16.0f }, { "outGain", 1.0f } } },
};
}

OcclusionEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff9bd9ff));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff94d2ff));
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff5aa0c9));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2b3238));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff3a3f45));
    addAndMakeVisible(slider);

    attachment = std::make_unique<APVTS::SliderAttachment>(state, paramID, slider);
}

void OcclusionEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(20));
    slider.setBounds(area.reduced(2));
}

OcclusionEngineAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff9bd9ff));
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

void OcclusionEngineAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    combo.setBounds(area.reduced(1));
}

OcclusionEngineAudioProcessorEditor::OcclusionEngineAudioProcessorEditor(OcclusionEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.getAPVTS())
{
    title.setText("Occlusion Engine", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xffdff3ff));
    addAndMakeVisible(title);

    subtitle.setText("Wall / Distance Simulator", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff8aa4b5));
    addAndMakeVisible(subtitle);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9bd9ff));
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
    presetBox.setTooltip("Occlusion scene presets for fast setup.");
    addAndMakeVisible(presetBox);

    tabs.setOutline(0);
    tabs.setTabBarDepth(34);
    tabs.addTab("Macro", juce::Colour(0xff2c3640), &macroPage, true);
    tabs.addTab("Tone", juce::Colour(0xff2c3640), &tonePage, true);
    tabs.addTab("Room", juce::Colour(0xff2c3640), &roomPage, true);
    tabs.addTab("Mix", juce::Colour(0xff2c3640), &mixPage, true);
    addAndMakeVisible(tabs);

    addKnob(macroPage, "distance", "Distance", "How far source is from listener through occluding space.");
    addKnob(macroPage, "wall", "Wall", "How dense/thick the barrier is.");
    addChoice(macroPage, "material", "Material", "Barrier type that shifts leak, tonal dip and damping.");
    addKnob(macroPage, "sourceRoom", "Source Rm", "Room contribution on source side.");
    addKnob(macroPage, "listenerRoom", "Listener Rm", "Room contribution on listener side.");

    addKnob(tonePage, "hpHz", "HP", "Low cut caused by wall coupling and distance.");
    addKnob(tonePage, "lpHz", "LP", "High-frequency rolloff from occlusion.");
    addKnob(tonePage, "dipHz", "Dip Hz", "Presence notch center from material absorption.");
    addKnob(tonePage, "dipDb", "Dip dB", "Depth of the occlusion notch.");
    addKnob(tonePage, "dipQ", "Dip Q", "Width of the presence notch.");
    addKnob(tonePage, "bumpHz", "Bump Hz", "Low-mid wall resonance center.");
    addKnob(tonePage, "bumpDb", "Bump dB", "Amount of wall resonance bloom.");
    addKnob(tonePage, "bumpQ", "Bump Q", "Width of resonance bump.");

    addKnob(roomPage, "roomMix", "Room Mix", "Blend of filtered direct vs room return.");
    addKnob(roomPage, "predelayMs", "Predelay", "Arrival delay before room reflections.");
    addKnob(roomPage, "roomSize", "Room Size", "Approximate room volume/decay behavior.");
    addKnob(roomPage, "damp", "Damp", "HF damping in reflections.");

    addKnob(mixPage, "leak", "Leak", "Unoccluded bleed around/through material.");
    addKnob(mixPage, "outGain", "Out Gain", "Final output trim.");

    setResizable(false, false);
    setSize(860, 540);

    lastDistance = getParamValue("distance");
    lastWall = getParamValue("wall");
    lastMaterial = (int) getParamValue("material");
    lastSourceRoom = getParamValue("sourceRoom");
    lastListenerRoom = getParamValue("listenerRoom");

    startTimerHz(18);
}

OcclusionEngineAudioProcessorEditor::~OcclusionEngineAudioProcessorEditor() {}

void OcclusionEngineAudioProcessorEditor::addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Knob>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    knobs.push_back(std::move(c));
}

void OcclusionEngineAudioProcessorEditor::addChoice(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto c = std::make_unique<Choice>(apvts, id, text);
    c->setHint(hint);
    page.addAndMakeVisible(*c);
    pageItems[&page].push_back(c.get());
    choices.push_back(std::move(c));
}

void OcclusionEngineAudioProcessorEditor::layoutPage(juce::Component& page, int columns)
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

void OcclusionEngineAudioProcessorEditor::setParamValue(const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
}

float OcclusionEngineAudioProcessorEditor::getParamValue(const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue(id))
        return v->load();
    return 0.0f;
}

void OcclusionEngineAudioProcessorEditor::applyMacroTargets(float distance, float wall, int material, float sourceRoom, float listenerRoom)
{
    const auto dist = clampf(distance, 0.0f, 1.0f);
    const auto wal = clampf(wall, 0.0f, 1.0f);
    const auto srcRoom = clampf(sourceRoom, 0.0f, 1.0f);
    const auto lisRoom = clampf(listenerRoom, 0.0f, 1.0f);

    const auto m = getMaterialDefaults(juce::jlimit(0, 5, material));
    const auto d = std::pow(dist, 1.15f);
    const auto w = std::pow(wal, 1.2f);
    const auto room = clampf(0.45f * srcRoom + 0.55f * lisRoom, 0.0f, 1.0f);

    const auto hpHz = 35.0f + d * 75.0f + w * 45.0f;
    const auto lpHzRaw = 16000.0f - (d * 5500.0f + w * 9500.0f);
    const auto lpMin = m.lpMin + w * 250.0f;
    const auto lpHz = juce::jmax(lpMin, lpHzRaw);

    const auto dipDb = m.dipDb - w * 1.8f;
    const auto bumpDb = m.bumpDb + w * 2.2f;
    const auto dipHz = m.dipHz + (0.5f - room) * 140.0f;
    const auto bumpHz = m.bumpHz + (0.5f - room) * 120.0f;

    const auto leak = clampf(0.03f + (1.0f - w) * 0.18f + m.leakBias, 0.0f, 1.0f);
    const auto roomMix = clampf(0.08f + room * 0.32f + d * 0.18f, 0.0f, 1.0f);
    const auto predelayMs = std::round(6.0f + room * 26.0f + d * 10.0f);
    const auto damp = clampf(m.damp + room * 0.12f, 0.0f, 1.0f);
    const auto roomSize = clampf(room, 0.0f, 1.0f);
    const auto outGain = std::round((1.0f - d * 0.18f) * 100.0f) / 100.0f;

    setParamValue("hpHz", hpHz);
    setParamValue("lpHz", lpHz);
    setParamValue("dipHz", dipHz);
    setParamValue("dipDb", dipDb);
    setParamValue("bumpHz", bumpHz);
    setParamValue("bumpDb", bumpDb);
    setParamValue("leak", leak);
    setParamValue("roomMix", roomMix);
    setParamValue("predelayMs", predelayMs);
    setParamValue("damp", damp);
    setParamValue("roomSize", roomSize);
    setParamValue("outGain", outGain);
}

void OcclusionEngineAudioProcessorEditor::applyPreset(int idx)
{
    if (idx < 0 || idx >= (int) std::size(kPresets))
        return;

    suppressMacros = true;
    for (const auto& kv : kPresets[(size_t) idx].values)
        setParamValue(kv.first, kv.second);
    suppressMacros = false;

    lastDistance = getParamValue("distance");
    lastWall = getParamValue("wall");
    lastMaterial = (int) getParamValue("material");
    lastSourceRoom = getParamValue("sourceRoom");
    lastListenerRoom = getParamValue("listenerRoom");
}

void OcclusionEngineAudioProcessorEditor::timerCallback()
{
    if (suppressMacros)
        return;

    const auto dist = getParamValue("distance");
    const auto wall = getParamValue("wall");
    const auto material = (int) getParamValue("material");
    const auto srcRoom = getParamValue("sourceRoom");
    const auto lisRoom = getParamValue("listenerRoom");

    if (std::abs(dist - lastDistance) > 0.0005f
        || std::abs(wall - lastWall) > 0.0005f
        || material != lastMaterial
        || std::abs(srcRoom - lastSourceRoom) > 0.0005f
        || std::abs(lisRoom - lastListenerRoom) > 0.0005f)
    {
        suppressMacros = true;
        applyMacroTargets(dist, wall, material, srcRoom, lisRoom);
        suppressMacros = false;

        lastDistance = dist;
        lastWall = wall;
        lastMaterial = material;
        lastSourceRoom = srcRoom;
        lastListenerRoom = lisRoom;
        presetBox.setSelectedId(1, juce::dontSendNotification);
    }
}

void OcclusionEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(juce::Colour(0xff131920), 0.0f, 0.0f, juce::Colour(0xff1f2b34), 0.0f, (float) getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();

    auto header = getLocalBounds().removeFromTop(82).reduced(8, 8);
    g.setColour(juce::Colour(0xff1f2832));
    g.fillRoundedRectangle(header.toFloat(), 10.0f);
    g.setColour(juce::Colour(0xff556676));
    g.drawRoundedRectangle(header.toFloat(), 10.0f, 1.2f);

    g.setColour(juce::Colour(0xffa5e3ff));
    g.fillRoundedRectangle((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 278.0f, 48.0f, 8.0f);
    g.setColour(juce::Colour(0xff275565));
    g.drawRoundedRectangle(juce::Rectangle<float>((float) header.getRight() - 300.0f, (float) header.getY() + 10.0f, 278.0f, 48.0f), 8.0f, 1.0f);
}

void OcclusionEngineAudioProcessorEditor::resized()
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
    layoutPage(tonePage, 4);
    layoutPage(roomPage, 4);
    layoutPage(mixPage, 3);
}
