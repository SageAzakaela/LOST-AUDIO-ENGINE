#include "PluginEditor.h"

#include <array>
#include <cmath>

namespace
{
constexpr std::uint32_t ink = 0xff07090d;
constexpr std::uint32_t deep = 0xff11151d;
constexpr std::uint32_t panel = 0xff1a202a;
constexpr std::uint32_t line = 0xff536170;
constexpr std::uint32_t bone = 0xfff0eee5;
constexpr std::uint32_t dim = 0xffa5acb4;
constexpr std::uint32_t cyan = 0xff64e8f4;
constexpr std::uint32_t magenta = 0xffff4fd8;
constexpr std::uint32_t amber = 0xffffbd62;

juce::Font font(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}

struct Setting { const char* id; float value; };
struct Preset { const char* name; std::initializer_list<Setting> settings; };

const Preset factoryPresets[] {
    { "BRAIN CRUNCHER", { { "crunch", .86f }, { "body", .68f }, { "bite", .62f }, { "space", .46f }, { "smear", .74f }, { "motion", .38f }, { "width", .78f }, { "binaural", .72f }, { "pan", 0.0f }, { "headSize", .55f }, { "drive", .35f }, { "mix", 1.0f }, { "outputGain", 0.0f } } },
    { "Chrome ASMR", { { "crunch", .58f }, { "body", .44f }, { "bite", .76f }, { "space", .62f }, { "smear", .68f }, { "motion", .55f }, { "width", .92f }, { "binaural", .94f }, { "headSize", .72f }, { "drive", .22f }, { "mix", 1.0f } } },
    { "Drone Furnace", { { "crunch", .94f }, { "body", .91f }, { "bite", .40f }, { "space", .58f }, { "smear", .82f }, { "motion", .25f }, { "width", .70f }, { "drive", .54f }, { "mix", 1.0f }, { "outputGain", -2.5f } } },
    { "Hollow Teeth", { { "crunch", .72f }, { "body", .82f }, { "bite", .68f }, { "space", .24f }, { "smear", .48f }, { "motion", .18f }, { "width", .64f }, { "drive", .44f }, { "mix", .92f } } },
    { "Tin Halo", { { "crunch", .64f }, { "body", .36f }, { "bite", .88f }, { "space", .54f }, { "smear", .58f }, { "motion", .72f }, { "width", 1.0f }, { "binaural", 1.0f }, { "pan", .22f }, { "headSize", .85f }, { "drive", .28f }, { "mix", .86f } } },
    { "Neural Static", { { "crunch", 1.0f }, { "body", .54f }, { "bite", .92f }, { "space", .35f }, { "smear", .90f }, { "motion", .82f }, { "width", .96f }, { "drive", .72f }, { "mix", 1.0f }, { "outputGain", -4.0f } } },
    { "Soft Cortex", { { "crunch", .34f }, { "body", .52f }, { "bite", .48f }, { "space", .42f }, { "smear", .35f }, { "motion", .30f }, { "width", .72f }, { "drive", .14f }, { "mix", .58f } } },
    { "Percussion Splinters", { { "crunch", .92f }, { "body", .30f }, { "bite", .82f }, { "space", .16f }, { "smear", .22f }, { "motion", .12f }, { "width", .84f }, { "drive", .62f }, { "mix", .88f }, { "outputGain", -3.0f } } },
    { "PLAY - Guitar Crunch Parallel", { { "crunch", .46f }, { "body", .58f }, { "bite", .54f }, { "space", .18f }, { "smear", .24f }, { "motion", .08f }, { "width", .66f }, { "drive", .30f }, { "mix", .58f }, { "outputGain", -1.5f }, { "ceiling", .90f } } },
    { "PLAY - Drum Splinter Bus", { { "crunch", .68f }, { "body", .34f }, { "bite", .72f }, { "space", .12f }, { "smear", .18f }, { "motion", .06f }, { "width", .78f }, { "drive", .42f }, { "mix", .48f }, { "outputGain", -2.0f }, { "ceiling", .88f } } },
    { "PLAY - Binaural Synth Motion", { { "crunch", .38f }, { "body", .44f }, { "bite", .42f }, { "space", .62f }, { "smear", .48f }, { "motion", .54f }, { "width", .92f }, { "binaural", .86f }, { "headSize", .62f }, { "drive", .18f }, { "mix", .66f }, { "outputGain", -1.0f }, { "ceiling", .90f } } },
    { "PLAY - Bass Cortex Weight", { { "crunch", .32f }, { "body", .74f }, { "bite", .28f }, { "space", .16f }, { "smear", .30f }, { "motion", .04f }, { "width", .48f }, { "drive", .26f }, { "mix", .54f }, { "outputGain", -1.5f }, { "ceiling", .90f } } },
};
}

void BrainCruncherAudioProcessorEditor::Panel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(juce::Colour(panel));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(line));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    g.setColour(juce::Colour(magenta));
    g.fillRect(bounds.getX() + 12.0f, bounds.getY() + 12.0f, 25.0f, 1.5f);
    g.setColour(juce::Colour(dim));
    g.setFont(font(9.5f, true));
    g.drawText(name, getLocalBounds().removeFromTop(35).withTrimmedLeft(45), juce::Justification::centredLeft);
}

BrainCruncherAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(bone));
    label.setFont(font(10.0f, true));
    addAndMakeVisible(label);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 18);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(magenta));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff414956));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(bone));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(bone));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(ink));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(line));
    slider.onDragStart = std::move(changed);
    addAndMakeVisible(slider);
    attachment = std::make_unique<APVTS::SliderAttachment>(state, id, slider);
}

void BrainCruncherAudioProcessorEditor::Knob::setHint(const juce::String& text)
{
    label.setTooltip(text);
    slider.setTooltip(text);
}

void BrainCruncherAudioProcessorEditor::Knob::resized()
{
    auto bounds = getLocalBounds();
    label.setBounds(bounds.removeFromTop(19));
    slider.setBounds(bounds.reduced(2));
}

void BrainCruncherAudioProcessorEditor::NeuralScope::update(float input, float output, float motion, float crunch, float width, bool rattle)
{
    in = input;
    out = output;
    movement = motion;
    amount = crunch;
    stereo = width;
    hardware = rattle;
    phase += 0.035f + movement * 0.08f;
    repaint();
}

void BrainCruncherAudioProcessorEditor::NeuralScope::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(juce::Colour(0xff0a0e15));
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(juce::Colour(line));
    g.drawRoundedRectangle(bounds, 10.0f, 1.2f);
    auto brain = bounds.reduced(30.0f).withTrimmedBottom(58.0f);
    const auto centre = brain.getCentre();
    const auto radius = juce::jmin(brain.getWidth(), brain.getHeight()) * 0.37f;

    for (int side = -1; side <= 1; side += 2)
    {
        juce::Path path;
        for (int point = 0; point <= 150; ++point)
        {
            const auto t = static_cast<float>(point) / 150.0f;
            const auto angle = -1.45f + t * 2.9f;
            const auto ripple = std::sin(t * (24.0f + amount * 34.0f) + phase * side) * radius * (0.035f + movement * 0.07f);
            const auto x = centre.x + side * (14.0f + std::cos(angle) * (radius + ripple) * (0.72f + stereo * 0.28f));
            const auto y = centre.y + std::sin(angle) * radius;
            if (point == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(juce::Colour(side < 0 ? cyan : magenta).withAlpha(0.62f + out * 0.35f));
        g.strokePath(path, juce::PathStrokeType(2.2f));
    }

    for (int band = 0; band < 7; ++band)
    {
        const auto y = centre.y - radius * .65f + band * radius * .22f;
        const auto swing = std::sin(phase * (1.0f + band * .12f) + band) * (6.0f + movement * 28.0f);
        g.setColour(juce::Colour(hardware ? amber : line).withAlpha(0.22f + amount * 0.08f));
        g.drawLine(centre.x - radius * .82f, y - swing, centre.x + radius * .82f, y + swing, 1.0f);
    }

    auto footer = bounds.reduced(22.0f).removeFromBottom(44.0f);
    const char* labels[] { "EXCITE", "CRUNCH", "MOTION" };
    const float values[] { in * 2.5f, out * 2.5f, movement };
    for (int row = 0; row < 3; ++row)
    {
        auto lineBounds = footer.removeFromTop(14.0f);
        g.setColour(juce::Colour(dim));
        g.setFont(font(8.0f, true));
        g.drawText(labels[row], lineBounds.removeFromLeft(48.0f), juce::Justification::centredLeft);
        auto meter = lineBounds.reduced(2.0f, 4.0f);
        g.setColour(juce::Colour(0xff39414c));
        g.fillRect(meter);
        g.setColour(juce::Colour(row == 0 ? amber : row == 1 ? magenta : cyan));
        g.fillRect(meter.withWidth(meter.getWidth() * juce::jlimit(0.0f, 1.0f, values[row])));
    }
}

BrainCruncherAudioProcessorEditor::BrainCruncherAudioProcessorEditor(BrainCruncherAudioProcessor& owner)
    : AudioProcessorEditor(&owner), processor(owner), apvts(owner.getAPVTS())
{
    setOpaque(true);
    brand.setText("B&E DIGITAL", juce::dontSendNotification);
    brand.setColour(juce::Label::textColourId, juce::Colour(cyan));
    brand.setFont(font(10.0f, true));
    addAndMakeVisible(brand);
    title.setText("BRAIN CRUNCHER", juce::dontSendNotification);
    title.setColour(juce::Label::textColourId, juce::Colour(bone));
    title.setFont(font(29.0f, true));
    addAndMakeVisible(title);
    subtitle.setText("STEREO METAL RESONATOR / DRONE EXCITER", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(dim));
    subtitle.setFont(font(10.0f, true));
    addAndMakeVisible(subtitle);
    profile.setText("NEURAL PROFILE", juce::dontSendNotification);
    profile.setColour(juce::Label::textColourId, juce::Colour(dim));
    profile.setFont(font(9.0f, true));
    addAndMakeVisible(profile);

    presets.addItem("Custom", 1);
    for (int i = 0; i < static_cast<int>(std::size(factoryPresets)); ++i) presets.addItem(factoryPresets[i].name, i + 2);
    presets.setSelectedId(2, juce::dontSendNotification);
    presets.onChange = [this] { if (presets.getSelectedId() >= 2) applyPreset(presets.getSelectedId() - 2); };
    presets.setTooltip("Load a protected factory profile. BRAIN CRUNCHER recreates the original loose-metal discovery.");
    addAndMakeVisible(presets);

    for (auto* button : { &surfaceButton, &advancedButton })
    {
        button->setColour(juce::TextButton::textColourOffId, juce::Colour(bone));
        addAndMakeVisible(*button);
    }
    surfaceButton.onClick = [this] { showAdvanced(false); };
    advancedButton.onClick = [this] { showAdvanced(true); };
    status.setColour(juce::Label::textColourId, juce::Colour(dim));
    status.setFont(font(9.0f, true));
    status.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(status);

    addAndMakeVisible(surfacePage);
    addAndMakeVisible(advancedPage);
    surfacePage.addAndMakeVisible(scope);
    for (auto* p : { &character, &dimension, &output }) surfacePage.addAndMakeVisible(*p);
    for (auto* p : { &metal, &phasePanel, &utility }) advancedPage.addAndMakeVisible(*p);

    addKnob(character, "crunch", "CRUNCH", "Sympathetic metal modes and signal-driven hardware rattle.");
    addKnob(character, "body", "BODY", "Lower resonant cavity weight and comb depth.");
    addKnob(character, "drive", "EXCITE", "Drive into the resonant sheet-metal model.");
    addKnob(dimension, "space", "SPACE", "Asymmetric source and listener reflections.");
    addKnob(dimension, "motion", "MOTION", "Slow opposed micro-delay movement across the stereo field.");
    addKnob(dimension, "width", "WIDTH", "Mid/side width after the moving resonator.");
    addKnob(dimension, "binaural", "BINAURAL", "Create independently delayed left and right signals with interaural level cues.");
    addKnob(dimension, "pan", "SOURCE PAN", "Place the resonant source continuously around the left/right listening axis.");
    addKnob(dimension, "headSize", "HEAD SPACE", "Virtual ear spacing; larger values increase the far-ear delay.");
    addKnob(output, "mix", "MIX", "Dry/wet blend. Full wet reproduces the original discovery.");
    addKnob(output, "outputGain", "OUTPUT dB", "Final level before the protected ceiling.");

    addKnob(metal, "crunch", "METAL MODES", "Strength of tuned sheet-metal modes.");
    addKnob(metal, "body", "CAVITY", "Cavity resonance beneath the metal panel.");
    addKnob(metal, "bite", "BITE", "Upper transmission bandwidth and metallic edge.");
    addKnob(metal, "drive", "EXCITER", "Nonlinear excitation feeding the resonator.");
    addKnob(phasePanel, "smear", "COMB SMEAR", "Short multipath blur around the resonant body.");
    addKnob(phasePanel, "motion", "PHASE MOTION", "Rate and depth of opposed stereo micro-delays.");
    addKnob(phasePanel, "space", "REFLECTIONS", "Room excitation and decaying listener-side reflections.");
    addKnob(phasePanel, "width", "SIDE WIDTH", "Stereo side gain without changing the center.");
    addKnob(phasePanel, "binaural", "DUAL SIGNAL", "Depth of independent left/right timing and level treatment.");
    addKnob(phasePanel, "pan", "SOURCE PAN", "Virtual source position from left to right.");
    addKnob(phasePanel, "headSize", "EAR SPACING", "Maximum interaural time difference.");
    addKnob(utility, "inputGain", "INPUT dB", "Input trim before metal excitation.");
    addKnob(utility, "mix", "MIX", "Dry/wet balance.");
    addKnob(utility, "outputGain", "OUTPUT dB", "Final output trim.");
    addKnob(utility, "ceiling", "CEILING", "Always-active soft output ceiling.");

    setResizable(true, true);
    setResizeLimits(900, 600, 1500, 980);
    setSize(1120, 700);
    showAdvanced(false);
    applyPreset(0);
    startTimerHz(30);
}

BrainCruncherAudioProcessorEditor::~BrainCruncherAudioProcessorEditor() { stopTimer(); }

BrainCruncherAudioProcessorEditor::Knob* BrainCruncherAudioProcessorEditor::addKnob(Panel& panelOwner, const char* id, const char* knobTitle, const char* hint)
{
    auto control = std::make_unique<Knob>(apvts, id, knobTitle, [this] { markCustom(); });
    control->setHint(hint);
    auto* result = control.get();
    panelOwner.addAndMakeVisible(*result);
    panelItems[&panelOwner].push_back(result);
    knobs.push_back(std::move(control));
    return result;
}

void BrainCruncherAudioProcessorEditor::layout(Panel& panelOwner, int columns)
{
    auto bounds = panelOwner.contentBounds();
    auto& items = panelItems[&panelOwner];
    const auto rows = juce::jmax(1, (static_cast<int>(items.size()) + columns - 1) / columns);
    const auto width = bounds.getWidth() / columns;
    const auto height = bounds.getHeight() / rows;
    for (int index = 0; index < static_cast<int>(items.size()); ++index)
        items[static_cast<std::size_t>(index)]->setBounds(bounds.getX() + (index % columns) * width, bounds.getY() + (index / columns) * height, width, height);
}

void BrainCruncherAudioProcessorEditor::showAdvanced(bool show)
{
    advancedVisible = show;
    surfacePage.setVisible(!show);
    advancedPage.setVisible(show);
    surfaceButton.setToggleState(!show, juce::dontSendNotification);
    advancedButton.setToggleState(show, juce::dontSendNotification);
    resized();
}

void BrainCruncherAudioProcessorEditor::setParameter(const char* id, float plainValue)
{
    if (auto* parameter = apvts.getParameter(id)) parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

float BrainCruncherAudioProcessorEditor::getParameter(const char* id) const
{
    if (auto* parameter = apvts.getRawParameterValue(id)) return parameter->load();
    return 0.0f;
}

void BrainCruncherAudioProcessorEditor::applyPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(std::size(factoryPresets))) return;
    suppressPresetChange = true;
    for (auto* parameter : processor.getParameters()) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    for (const auto& setting : factoryPresets[index].settings) setParameter(setting.id, setting.value);
    suppressPresetChange = false;
}

void BrainCruncherAudioProcessorEditor::markCustom()
{
    if (!suppressPresetChange) presets.setSelectedId(1, juce::dontSendNotification);
}

void BrainCruncherAudioProcessorEditor::timerCallback()
{
    const auto input = (processor.inputPeak(0) + processor.inputPeak(1)) * 0.5f;
    const auto outputLevel = (processor.outputPeak(0) + processor.outputPeak(1)) * 0.5f;
    scope.update(input, outputLevel, processor.stereoMotion(), getParameter("crunch"), getParameter("width"), processor.rattleActive());
    status.setText(processor.rattleActive() ? "METAL HARDWARE EXCITED" : "FEED IT A SUSTAINED TONE", juce::dontSendNotification);
}

void BrainCruncherAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ink));
    const auto bounds = getLocalBounds().toFloat().reduced(7.0f);
    g.setColour(juce::Colour(deep));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(line));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    g.setColour(juce::Colour(cyan));
    g.fillRect(16.0f, 7.0f, 190.0f, 2.0f);
    g.setColour(juce::Colour(magenta));
    g.fillRect(static_cast<float>(getWidth()) - 206.0f, 7.0f, 190.0f, 2.0f);
    g.setColour(juce::Colour(0xff343d49));
    g.fillRect(15, 93, getWidth() - 30, 1);
}

void BrainCruncherAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(16);
    auto header = bounds.removeFromTop(88);
    auto identity = header.removeFromLeft(juce::jmin(570, header.getWidth() / 2));
    brand.setBounds(identity.removeFromTop(17));
    title.setBounds(identity.removeFromTop(38));
    subtitle.setBounds(identity.removeFromTop(18));
    auto profileArea = header.removeFromRight(juce::jmin(310, header.getWidth()));
    profile.setBounds(profileArea.removeFromTop(17));
    presets.setBounds(profileArea.removeFromTop(32));
    auto navigation = header.removeFromBottom(35);
    surfaceButton.setBounds(navigation.removeFromLeft(118));
    navigation.removeFromLeft(8);
    advancedButton.setBounds(navigation.removeFromLeft(100));
    status.setBounds(header);

    surfacePage.setBounds(bounds);
    advancedPage.setBounds(bounds);
    if (!advancedVisible)
    {
        auto page = surfacePage.getLocalBounds();
        scope.setBounds(page.removeFromLeft(static_cast<int>(page.getWidth() * .42f)).reduced(4));
        auto controls = page.reduced(4);
        character.setBounds(controls.removeFromTop(static_cast<int>(controls.getHeight() * .47f)).reduced(3));
        dimension.setBounds(controls.removeFromTop(static_cast<int>(controls.getHeight() * .58f)).reduced(3));
        output.setBounds(controls.reduced(3));
        layout(character, 3);
        layout(dimension, 6);
        layout(output, 2);
    }
    else
    {
        auto page = advancedPage.getLocalBounds().reduced(3);
        const auto columnWidth = page.getWidth() / 3;
        metal.setBounds(page.removeFromLeft(columnWidth).reduced(3));
        phasePanel.setBounds(page.removeFromLeft(columnWidth).reduced(3));
        utility.setBounds(page.reduced(3));
        layout(metal, 2);
        layout(phasePanel, 2);
        layout(utility, 2);
    }
}
