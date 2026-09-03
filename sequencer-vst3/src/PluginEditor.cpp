#include "PluginEditor.h"

#include <cmath>

namespace
{
const auto background = juce::Colour::fromRGB(7, 11, 12);
const auto panel = juce::Colour::fromRGB(21, 25, 24);
const auto panelRaised = juce::Colour::fromRGB(31, 33, 29);
const auto cream = juce::Colour::fromRGB(239, 231, 211);
const auto muted = juce::Colour::fromRGB(164, 160, 148);
const auto cyan = juce::Colour::fromRGB(79, 226, 235);
const auto magenta = juce::Colour::fromRGB(244, 66, 198);
const auto amber = juce::Colour::fromRGB(255, 174, 54);
const auto danger = juce::Colour::fromRGB(255, 84, 86);

const juce::StringArray engineNames {
    "TAPE", "TRANSMISSION", "COMMS", "CD", "CONFERENCE", "CAMCORDER",
    "CARTRIDGE", "TELEVISION", "OCCLUSION", "OPEN MIC"
};

juce::String shortEngineName(int index)
{
    const juce::StringArray shortNames { "TAPE", "TX", "COMMS", "CD", "CALL", "CAM", "CART", "TV", "WALL", "MIC" };
    return shortNames[juce::jlimit(0, shortNames.size() - 1, index)];
}

void usePercentDisplay(juce::Slider& slider)
{
    slider.textFromValueFunction = [](double value)
    {
        return juce::String(juce::roundToInt(value * 100.0)) + "%";
    };
    slider.valueFromTextFunction = [](const juce::String& text)
    {
        return text.retainCharacters("0123456789.-").getDoubleValue() / 100.0;
    };
    slider.updateText();
}
}

LostAudioSequencerEditor::SequencerLookAndFeel::SequencerLookAndFeel()
{
    setColour(juce::Label::textColourId, cream);
    setColour(juce::ComboBox::textColourId, cream);
    setColour(juce::ComboBox::backgroundColourId, background);
    setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(82, 83, 75));
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, cream);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, cyan.withAlpha(.22f));
    setColour(juce::Slider::textBoxTextColourId, cream);
    setColour(juce::Slider::textBoxBackgroundColourId, background);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB(75, 76, 69));
    setColour(juce::TextButton::textColourOffId, cream);
    setColour(juce::TextButton::textColourOnId, background);
}

void LostAudioSequencerEditor::SequencerLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                                                      int width, int height, float position,
                                                                      float startAngle, float endAngle,
                                                                      juce::Slider& slider)
{
    const auto radius = static_cast<float>(juce::jmin(width, height)) * .39f;
    const auto centre = juce::Point<float>(static_cast<float>(x + width / 2), static_cast<float>(y + height / 2));
    juce::Rectangle<float> ring(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour(juce::Colour::fromRGB(70, 70, 62));
    g.strokePath(track, juce::PathStrokeType(5.0f));
    const auto angle = startAngle + position * (endAngle - startAngle);
    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId, true));
    g.strokePath(valueArc, juce::PathStrokeType(5.0f));
    g.setColour(panelRaised);
    g.fillEllipse(ring.reduced(7.0f));
    g.setColour(cream);
    const auto pointer = juce::Point<float>(centre.x + std::sin(angle) * (radius - 12.0f),
                                            centre.y - std::cos(angle) * (radius - 12.0f));
    g.drawLine(centre.x, centre.y, pointer.x, pointer.y, 3.0f);
}

void LostAudioSequencerEditor::SequencerLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                                          const juce::Colour&, bool hovered, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(.5f);
    auto fill = button.getToggleState() ? cyan : panelRaised;
    if (down) fill = magenta;
    else if (hovered) fill = fill.brighter(.12f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(button.getToggleState() ? background : juce::Colour::fromRGB(91, 93, 84));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

void LostAudioSequencerEditor::SequencerLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                                                      bool hovered, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    auto fill = button.getToggleState() ? cyan : panelRaised;
    if (down || hovered) fill = fill.brighter(.10f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(button.getToggleState() ? background : cream);
    g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(8, 0), juce::Justification::centred, 1);
}

void LostAudioSequencerEditor::SequencerLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                                                  bool, int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(.5f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    juce::Path arrow;
    arrow.addTriangle(static_cast<float>(width - 18), static_cast<float>(height / 2 - 3),
                      static_cast<float>(width - 8), static_cast<float>(height / 2 - 3),
                      static_cast<float>(width - 13), static_cast<float>(height / 2 + 3));
    g.setColour(cyan);
    g.fillPath(arrow);
}

LostAudioSequencerEditor::StepPad::StepPad(LostAudioSequencerEditor& parent,
                                            LostAudioSequencerProcessor& p, int index)
    : juce::Button("Step " + juce::String(index + 1)), owner(parent), processor(p), step(index)
{
    onClick = [this] { owner.selectStep(step); };
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void LostAudioSequencerEditor::StepPad::mouseDoubleClick(const juce::MouseEvent& event)
{
    owner.selectStep(step);
    const auto id = LostAudioSequencerProcessor::stepId(step, "Enabled");
    if (auto* parameter = processor.state().getParameter(id))
    {
        const auto current = processor.state().getRawParameterValue(id)->load() > .5f;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(current ? 0.0f : 1.0f);
        parameter->endChangeGesture();
    }
    juce::Button::mouseDoubleClick(event);
}

void LostAudioSequencerEditor::StepPad::paintButton(juce::Graphics& g, bool hovered, bool down)
{
    const auto active = processor.state().getRawParameterValue(LostAudioSequencerProcessor::stepId(step, "Enabled"))->load() > .5f;
    const auto engine = juce::jlimit(0, 9, juce::roundToInt(processor.state().getRawParameterValue(LostAudioSequencerProcessor::stepId(step, "Engine"))->load()));
    const auto damage = processor.state().getRawParameterValue(LostAudioSequencerProcessor::stepId(step, "Damage"))->load();
    const auto chance = processor.state().getRawParameterValue(LostAudioSequencerProcessor::stepId(step, "Probability"))->load();
    const auto selected = owner.selectedStep == step;
    const auto playing = processor.currentStep() == step && processor.transportActive();
    const auto fired = playing && processor.currentStepFired();

    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    auto fill = active ? panelRaised : panel;
    if (hovered) fill = fill.brighter(.08f);
    if (down) fill = fill.brighter(.16f);
    if (playing) fill = fired ? cyan.withAlpha(.26f) : magenta.withAlpha(.18f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(playing ? (fired ? cyan : magenta) : (selected ? amber : juce::Colour::fromRGB(64, 67, 62)));
    g.drawRoundedRectangle(bounds, 4.0f, selected || playing ? 2.0f : 1.0f);

    auto area = getLocalBounds().reduced(10, 6);
    auto numberArea = area.removeFromLeft(30);
    g.setColour(active ? cyan : muted.withAlpha(.55f));
    g.setFont(juce::Font(juce::FontOptions(19.0f).withStyle("Bold")));
    g.drawText(juce::String(step + 1).paddedLeft('0', 2), numberArea, juce::Justification::centredLeft);
    g.setColour(active ? cream : muted.withAlpha(.55f));
    g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    g.drawFittedText(active ? shortEngineName(engine) : "REST", area.removeFromTop(25), juce::Justification::centredLeft, 1);

    auto damageBar = area.removeFromTop(7).toFloat();
    g.setColour(background);
    g.fillRoundedRectangle(damageBar, 2.0f);
    g.setColour(active ? magenta : muted.withAlpha(.25f));
    g.fillRoundedRectangle(damageBar.withWidth(damageBar.getWidth() * juce::jlimit(0.0f, 1.0f, damage)), 2.0f);
    area.removeFromTop(4);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
    g.drawText(active ? juce::String(juce::roundToInt(chance * 100.0f)) + "% CHANCE" : "DBL-CLICK: ARM",
               area, juce::Justification::centredLeft);
}

LostAudioSequencerEditor::LostAudioSequencerEditor(LostAudioSequencerProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);

    brandLabel.setText("B&E DIGITAL / LOST AUDIO", juce::dontSendNotification);
    brandLabel.setColour(juce::Label::textColourId, cyan);
    brandLabel.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    titleLabel.setText("EFFECT SEQUENCER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(30.0f).withStyle("Bold")));
    subtitleLabel.setText("HOST-SYNCED DEVICE DAMAGE / V1", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, muted);
    subtitleLabel.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    stepTitleLabel.setFont(juce::Font(juce::FontOptions(16.0f).withStyle("Bold")));
    stepTitleLabel.setColour(juce::Label::textColourId, cream);
    hintLabel.setText("CLICK: INSPECT  /  DOUBLE-CLICK: ARM OR REST", juce::dontSendNotification);
    hintLabel.setColour(juce::Label::textColourId, muted);
    hintLabel.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    for (auto* label : { &brandLabel, &titleLabel, &subtitleLabel, &stepTitleLabel, &hintLabel }) addAndMakeVisible(*label);

    presetBox.addItemList({ "Weathered Pulse", "Broken Broadcast", "Disc Panic", "Behind The Wall", "Failed Call", "Machine Haunting",
                            "PLAY - Guitar Gate Scatter", "PLAY - Drum Fill Machine", "PLAY - Synth Pulse Grid", "PLAY - Sparse Hook Mutator" }, 1);
    presetBox.setSelectedId(1, juce::dontSendNotification);
    configureChoice(presetBox);
    configureChoice(divisionBox);
    divisionBox.addItemList({ "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" }, 1);
    configureChoice(engineBox);
    engineBox.addItemList(engineNames, 1);
    const std::array<juce::Component*, 9> controls { &presetBox, &loadPresetButton, &randomizeButton, &clearButton,
                                                     &enabledButton, &auditionButton, &stepEnabledButton, &divisionBox, &engineBox };
    for (auto* component : controls)
        addAndMakeVisible(*component);

    loadPresetButton.onClick = [this] { processor.applyPreset(juce::jmax(0, presetBox.getSelectedItemIndex())); };
    randomizeButton.onClick = [this] { processor.randomizePattern(); };
    clearButton.onClick = [this] { processor.clearPattern(); };
    clearButton.setColour(juce::TextButton::buttonColourId, danger.withAlpha(.22f));

    configureKnob(lengthSlider, lengthLabel, "LENGTH", " steps");
    configureKnob(swingSlider, swingLabel, "SWING");
    configureKnob(bpmSlider, bpmLabel, "AUDITION BPM", "");
    configureKnob(inputSlider, inputLabel, "INPUT", " dB");
    configureKnob(outputSlider, outputLabel, "OUTPUT", " dB");
    configureKnob(masterMixSlider, masterMixLabel, "MASTER MIX");
    configureKnob(safetySlider, safetyLabel, "SAFETY");
    configureKnob(ceilingSlider, ceilingLabel, "CEILING", "");
    configureKnob(characterSlider, characterLabel, "CHARACTER");
    configureKnob(damageSlider, damageLabel, "DAMAGE");
    configureKnob(probabilitySlider, probabilityLabel, "CHANCE");
    configureKnob(stepMixSlider, stepMixLabel, "STEP MIX");
    configureKnob(modelSlider, modelLabel, "DEVICE MODEL");

    lengthSlider.setRange(1.0, 16.0, 1.0);
    swingSlider.setRange(0.0, 0.5, 0.001);
    bpmSlider.setRange(40.0, 240.0, 1.0);
    inputSlider.setRange(-24.0, 12.0, 0.1);
    outputSlider.setRange(-24.0, 6.0, 0.1);
    masterMixSlider.setRange(0.0, 1.0, 0.001);
    safetySlider.setRange(0.0, 1.0, 0.001);
    ceilingSlider.setRange(0.25, 0.99, 0.001);
    characterSlider.setRange(0.0, 1.0, 0.001);
    damageSlider.setRange(0.0, 1.0, 0.001);
    probabilitySlider.setRange(0.0, 1.0, 0.001);
    stepMixSlider.setRange(0.0, 1.0, 0.001);
    modelSlider.setRange(0.0, 1.0, 0.001);
    swingSlider.setDoubleClickReturnValue(true, 0.0);

    auto& state = processor.state();
    enabledAttachment = std::make_unique<APVTS::ButtonAttachment>(state, "enabled", enabledButton);
    auditionAttachment = std::make_unique<APVTS::ButtonAttachment>(state, "freeRun", auditionButton);
    divisionAttachment = std::make_unique<APVTS::ComboBoxAttachment>(state, "division", divisionBox);
    lengthAttachment = std::make_unique<APVTS::SliderAttachment>(state, "length", lengthSlider);
    swingAttachment = std::make_unique<APVTS::SliderAttachment>(state, "swing", swingSlider);
    bpmAttachment = std::make_unique<APVTS::SliderAttachment>(state, "internalBpm", bpmSlider);
    inputAttachment = std::make_unique<APVTS::SliderAttachment>(state, "inputGain", inputSlider);
    outputAttachment = std::make_unique<APVTS::SliderAttachment>(state, "outputGain", outputSlider);
    masterMixAttachment = std::make_unique<APVTS::SliderAttachment>(state, "mix", masterMixSlider);
    safetyAttachment = std::make_unique<APVTS::SliderAttachment>(state, "safety", safetySlider);
    ceilingAttachment = std::make_unique<APVTS::SliderAttachment>(state, "ceiling", ceilingSlider);
    for (auto* slider : { &swingSlider, &masterMixSlider, &safetySlider }) usePercentDisplay(*slider);

    for (int step = 0; step < LostAudioSequencerProcessor::stepCount; ++step)
    {
        pads[static_cast<std::size_t>(step)] = std::make_unique<StepPad>(*this, processor, step);
        addAndMakeVisible(*pads[static_cast<std::size_t>(step)]);
    }

    selectStep(0);
    setResizable(true, true);
    setResizeLimits(960, 580, 1500, 920);
    setSize(1120, 650);
    startTimerHz(30);
}

LostAudioSequencerEditor::~LostAudioSequencerEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void LostAudioSequencerEditor::configureChoice(juce::ComboBox& box)
{
    box.setJustificationType(juce::Justification::centredLeft);
    box.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void LostAudioSequencerEditor::configureKnob(juce::Slider& slider, juce::Label& label,
                                              const juce::String& text, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    slider.setTextValueSuffix(suffix);
    slider.setColour(juce::Slider::rotarySliderFillColourId, amber);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(juce::FontOptions(10.5f).withStyle("Bold")));
    label.setColour(juce::Label::textColourId, muted);
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void LostAudioSequencerEditor::setActualValue(const juce::String& id, float actual)
{
    if (auto* parameter = processor.state().getParameter(id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(actual));
        parameter->endChangeGesture();
    }
}

float LostAudioSequencerEditor::value(const juce::String& id) const
{
    if (const auto* raw = processor.state().getRawParameterValue(id)) return raw->load();
    return 0.0f;
}

void LostAudioSequencerEditor::selectStep(int step)
{
    selectedStep = juce::jlimit(0, LostAudioSequencerProcessor::stepCount - 1, step);
    stepTitleLabel.setText("STEP " + juce::String(selectedStep + 1).paddedLeft('0', 2) + " / DEVICE SURFACE",
                           juce::dontSendNotification);
    rebuildStepAttachments();
    for (auto& pad : pads) if (pad) pad->repaint();
}

void LostAudioSequencerEditor::rebuildStepAttachments()
{
    stepEnabledAttachment.reset();
    engineAttachment.reset();
    characterAttachment.reset();
    damageAttachment.reset();
    probabilityAttachment.reset();
    stepMixAttachment.reset();
    modelAttachment.reset();

    auto& state = processor.state();
    stepEnabledAttachment = std::make_unique<APVTS::ButtonAttachment>(state, LostAudioSequencerProcessor::stepId(selectedStep, "Enabled"), stepEnabledButton);
    engineAttachment = std::make_unique<APVTS::ComboBoxAttachment>(state, LostAudioSequencerProcessor::stepId(selectedStep, "Engine"), engineBox);
    characterAttachment = std::make_unique<APVTS::SliderAttachment>(state, LostAudioSequencerProcessor::stepId(selectedStep, "Character"), characterSlider);
    damageAttachment = std::make_unique<APVTS::SliderAttachment>(state, LostAudioSequencerProcessor::stepId(selectedStep, "Damage"), damageSlider);
    probabilityAttachment = std::make_unique<APVTS::SliderAttachment>(state, LostAudioSequencerProcessor::stepId(selectedStep, "Probability"), probabilitySlider);
    stepMixAttachment = std::make_unique<APVTS::SliderAttachment>(state, LostAudioSequencerProcessor::stepId(selectedStep, "Mix"), stepMixSlider);
    modelAttachment = std::make_unique<APVTS::SliderAttachment>(state, LostAudioSequencerProcessor::stepId(selectedStep, "Model"), modelSlider);
    for (auto* slider : { &characterSlider, &damageSlider, &probabilitySlider, &stepMixSlider, &modelSlider })
        usePercentDisplay(*slider);
}

void LostAudioSequencerEditor::timerCallback()
{
    for (auto& pad : pads) pad->repaint();
    repaint();
}

void LostAudioSequencerEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.setColour(cyan);
    g.fillRect(0, 0, getWidth() / 2, 2);
    g.setColour(magenta);
    g.fillRect(getWidth() / 2, 0, getWidth() - getWidth() / 2, 2);

    g.setColour(panel);
    g.fillRoundedRectangle(gridBounds.toFloat(), 5.0f);
    g.fillRoundedRectangle(inspectorBounds.toFloat(), 5.0f);
    g.fillRoundedRectangle(meterBounds.toFloat(), 5.0f);
    g.setColour(juce::Colour::fromRGB(60, 63, 58));
    g.drawRoundedRectangle(gridBounds.toFloat(), 5.0f, 1.0f);
    g.drawRoundedRectangle(inspectorBounds.toFloat(), 5.0f, 1.0f);
    g.drawRoundedRectangle(meterBounds.toFloat(), 5.0f, 1.0f);

    auto statusArea = juce::Rectangle<int>(getWidth() - 290, 50, 270, 22);
    const auto running = processor.transportActive();
    g.setColour(running ? cyan : muted);
    g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    const auto status = running
        ? "GRID RUNNING  /  " + juce::String(processor.currentBpm(), 1) + " BPM"
        : "WAITING FOR HOST  /  AUDITION AVAILABLE";
    g.drawFittedText(status, statusArea, juce::Justification::centredRight, 1);

    auto meter = meterBounds.reduced(16, 12).removeFromLeft(238);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    g.drawText("SIGNAL / INPUT", meter.removeFromTop(16), juce::Justification::centredLeft);
    auto inputBar = meter.removeFromTop(12).toFloat();
    g.setColour(background); g.fillRoundedRectangle(inputBar, 2.0f);
    const auto inputPeak = juce::jmax(processor.inputPeak(0), processor.inputPeak(1));
    g.setColour(cyan); g.fillRoundedRectangle(inputBar.withWidth(inputBar.getWidth() * juce::jlimit(0.0f, 1.0f, inputPeak)), 2.0f);
    meter.removeFromTop(8);
    g.setColour(muted); g.drawText("SIGNAL / OUTPUT", meter.removeFromTop(16), juce::Justification::centredLeft);
    auto outputBar = meter.removeFromTop(12).toFloat();
    g.setColour(background); g.fillRoundedRectangle(outputBar, 2.0f);
    const auto outputPeak = juce::jmax(processor.outputPeak(0), processor.outputPeak(1));
    g.setColour(processor.safetyEngaged() ? danger : magenta);
    g.fillRoundedRectangle(outputBar.withWidth(outputBar.getWidth() * juce::jlimit(0.0f, 1.0f, outputPeak)), 2.0f);
}

void LostAudioSequencerEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto header = area.removeFromTop(66);
    brandLabel.setBounds(header.removeFromTop(18).removeFromLeft(310));
    auto titleRow = header.removeFromTop(36);
    titleLabel.setBounds(titleRow.removeFromLeft(430));
    subtitleLabel.setBounds(titleRow.removeFromLeft(310));

    auto presetArea = juce::Rectangle<int>(getWidth() - 430, 16, 414, 32);
    clearButton.setBounds(presetArea.removeFromRight(62));
    presetArea.removeFromRight(6);
    randomizeButton.setBounds(presetArea.removeFromRight(110));
    presetArea.removeFromRight(6);
    loadPresetButton.setBounds(presetArea.removeFromRight(56));
    presetArea.removeFromRight(6);
    presetBox.setBounds(presetArea);

    auto transport = area.removeFromTop(88);
    transport.removeFromBottom(8);
    enabledButton.setBounds(transport.removeFromLeft(112).reduced(3, 22));
    transport.removeFromLeft(8);
    divisionBox.setBounds(transport.removeFromLeft(138).reduced(3, 22));
    auto placeTopControl = [&transport](juce::Label& label, juce::Slider& slider, int width)
    {
        auto cell = transport.removeFromLeft(width).reduced(3, 0);
        label.setBounds(cell.removeFromTop(18));
        slider.setBounds(cell);
    };
    placeTopControl(lengthLabel, lengthSlider, 92);
    placeTopControl(swingLabel, swingSlider, 92);
    auditionButton.setBounds(transport.removeFromLeft(104).reduced(3, 22));
    placeTopControl(bpmLabel, bpmSlider, 112);

    meterBounds = area.removeFromBottom(98);
    area.removeFromBottom(10);
    inspectorBounds = area.removeFromRight(juce::jlimit(285, 350, getWidth() / 3));
    area.removeFromRight(10);
    gridBounds = area;

    auto grid = gridBounds.reduced(12);
    hintLabel.setBounds(grid.removeFromTop(22));
    grid.removeFromTop(5);
    const auto padGap = 7;
    const auto padWidth = (grid.getWidth() - padGap * 3) / 4;
    const auto padHeight = (grid.getHeight() - padGap * 3) / 4;
    for (int step = 0; step < LostAudioSequencerProcessor::stepCount; ++step)
    {
        const auto column = step % 4;
        const auto row = step / 4;
        pads[static_cast<std::size_t>(step)]->setBounds(grid.getX() + column * (padWidth + padGap),
                                                        grid.getY() + row * (padHeight + padGap),
                                                        padWidth, padHeight);
    }

    auto inspector = inspectorBounds.reduced(12);
    stepTitleLabel.setBounds(inspector.removeFromTop(26));
    inspector.removeFromTop(4);
    stepEnabledButton.setBounds(inspector.removeFromTop(30));
    inspector.removeFromTop(6);
    engineBox.setBounds(inspector.removeFromTop(34));
    inspector.removeFromTop(7);
    const auto rowHeight = juce::jmax(82, inspector.getHeight() / 2);
    auto firstRow = inspector.removeFromTop(rowHeight);
    auto secondRow = inspector;
    auto placeInspectorKnob = [](juce::Rectangle<int>& row, juce::Label& label, juce::Slider& slider, int remaining)
    {
        auto cell = row.removeFromLeft(row.getWidth() / remaining).reduced(2);
        label.setBounds(cell.removeFromTop(17));
        slider.setBounds(cell);
    };
    placeInspectorKnob(firstRow, characterLabel, characterSlider, 3);
    placeInspectorKnob(firstRow, damageLabel, damageSlider, 2);
    placeInspectorKnob(firstRow, probabilityLabel, probabilitySlider, 1);
    placeInspectorKnob(secondRow, stepMixLabel, stepMixSlider, 2);
    placeInspectorKnob(secondRow, modelLabel, modelSlider, 1);

    auto masters = meterBounds.reduced(14, 7);
    masters.removeFromLeft(260);
    auto placeMaster = [&masters](juce::Label& label, juce::Slider& slider, int remaining)
    {
        auto cell = masters.removeFromLeft(masters.getWidth() / remaining).reduced(2);
        label.setBounds(cell.removeFromTop(17));
        slider.setBounds(cell);
    };
    placeMaster(inputLabel, inputSlider, 5);
    placeMaster(outputLabel, outputSlider, 4);
    placeMaster(masterMixLabel, masterMixSlider, 3);
    placeMaster(safetyLabel, safetySlider, 2);
    placeMaster(ceilingLabel, ceilingSlider, 1);
}
