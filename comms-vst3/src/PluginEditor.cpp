#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr std::uint32_t ink = 0xff090c0b;
constexpr std::uint32_t deep = 0xff101411;
constexpr std::uint32_t panel = 0xff1c201b;
constexpr std::uint32_t panelLift = 0xff242920;
constexpr std::uint32_t line = 0xff59604e;
constexpr std::uint32_t bone = 0xffeee5cf;
constexpr std::uint32_t dimBone = 0xffaaa895;
constexpr std::uint32_t cyan = 0xff62e9eb;
constexpr std::uint32_t amber = 0xffffb447;
constexpr std::uint32_t signalGreen = 0xffb9df92;
constexpr std::uint32_t danger = 0xffff625d;

juce::Font uiFont(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions("Arial", size, bold ? juce::Font::bold : juce::Font::plain));
}

struct PresetDefinition
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;
};

const PresetDefinition presets[] = {
    { "Landline Clean", { { "mode", 0.0f }, { "bandwidth", 0.62f }, { "drive", 0.14f }, { "glitch", 0.02f }, { "noise", 0.05f }, { "character", 0.42f }, { "distance", 0.02f }, { "macroLink", 1.0f } } },
    { "Warm Bakelite", { { "mode", 0.0f }, { "bandwidth", 0.50f }, { "drive", 0.30f }, { "glitch", 0.04f }, { "noise", 0.12f }, { "character", 0.76f }, { "distance", 0.03f }, { "macroLink", 1.0f } } },
    { "Motel Receiver", { { "mode", 0.0f }, { "bandwidth", 0.34f }, { "drive", 0.52f }, { "glitch", 0.12f }, { "noise", 0.34f }, { "character", 0.88f }, { "distance", 0.04f }, { "macroLink", 1.0f } } },
    { "Payphone Booth", { { "mode", 0.0f }, { "bandwidth", 0.38f }, { "drive", 0.42f }, { "glitch", 0.08f }, { "noise", 0.22f }, { "character", 0.72f }, { "distance", 0.22f }, { "macroLink", 1.0f } } },
    { "Cell Clear", { { "mode", 1.0f }, { "bandwidth", 0.54f }, { "drive", 0.16f }, { "glitch", 0.03f }, { "noise", 0.04f }, { "character", 0.16f }, { "distance", 0.01f }, { "macroLink", 1.0f } } },
    { "Cell Breakup", { { "mode", 1.0f }, { "bandwidth", 0.34f }, { "drive", 0.38f }, { "glitch", 0.72f }, { "noise", 0.10f }, { "character", 0.22f }, { "distance", 0.03f }, { "macroLink", 1.0f } } },
    { "Pocket Speakerphone", { { "mode", 1.0f }, { "bandwidth", 0.42f }, { "drive", 0.28f }, { "glitch", 0.18f }, { "noise", 0.06f }, { "character", 0.48f }, { "distance", 0.28f }, { "macroLink", 1.0f } } },
    { "Office Intercom", { { "mode", 2.0f }, { "bandwidth", 0.42f }, { "drive", 0.24f }, { "glitch", 0.10f }, { "noise", 0.12f }, { "character", 0.62f }, { "distance", 0.20f }, { "macroLink", 1.0f } } },
    { "Security Intercom", { { "mode", 2.0f }, { "bandwidth", 0.26f }, { "drive", 0.52f }, { "glitch", 0.32f }, { "noise", 0.32f }, { "character", 0.90f }, { "distance", 0.38f }, { "macroLink", 1.0f } } },
    { "Warehouse PA", { { "mode", 3.0f }, { "bandwidth", 0.64f }, { "drive", 0.56f }, { "glitch", 0.04f }, { "noise", 0.09f }, { "character", 0.82f }, { "distance", 0.56f }, { "macroLink", 1.0f } } },
    { "Hospital Corridor", { { "mode", 3.0f }, { "bandwidth", 0.58f }, { "drive", 0.38f }, { "glitch", 0.02f }, { "noise", 0.06f }, { "character", 0.68f }, { "distance", 0.76f }, { "macroLink", 1.0f } } },
    { "Subway Platform", { { "mode", 3.0f }, { "bandwidth", 0.36f }, { "drive", 0.68f }, { "glitch", 0.16f }, { "noise", 0.24f }, { "character", 0.94f }, { "distance", 0.88f }, { "macroLink", 1.0f } } },
    { "Alarm Panel", { { "mode", 4.0f }, { "bandwidth", 0.62f }, { "drive", 0.24f }, { "glitch", 0.04f }, { "noise", 0.10f }, { "character", 0.66f }, { "distance", 0.24f }, { "alarmTone", 1.0f }, { "macroLink", 1.0f } } },
    { "Emergency Paging", { { "mode", 4.0f }, { "bandwidth", 0.48f }, { "drive", 0.48f }, { "glitch", 0.12f }, { "noise", 0.22f }, { "character", 0.82f }, { "distance", 0.58f }, { "alarmTone", 1.0f }, { "macroLink", 1.0f } } },
    { "Damaged Copper", { { "mode", 0.0f }, { "bandwidth", 0.20f }, { "drive", 0.72f }, { "glitch", 0.46f }, { "noise", 0.58f }, { "character", 0.86f }, { "distance", 0.08f }, { "macroLink", 1.0f } } },
    { "Diegetic Extreme", { { "mode", 2.0f }, { "bandwidth", 0.18f }, { "drive", 0.78f }, { "glitch", 0.68f }, { "noise", 0.52f }, { "character", 0.98f }, { "distance", 0.74f }, { "macroLink", 1.0f } } },
};
}

CommsEngineAudioProcessorEditor::ConsoleLookAndFeel::ConsoleLookAndFeel()
{
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(deep));
    setColour(juce::PopupMenu::textColourId, juce::Colour(bone));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff304f48));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(ink));
    setColour(juce::ComboBox::textColourId, juce::Colour(bone));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(line));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(ink));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(amber));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(line));
}

void CommsEngineAudioProcessorEditor::ConsoleLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height, float sliderPosition,
    float start, float end, juce::Slider& slider)
{
    juce::ignoreUnused(slider);
    const auto diameter = (float) juce::jmin(width, height) - 10.0f;
    const auto bounds = juce::Rectangle<float>((float) x + ((float) width - diameter) * 0.5f,
                                                (float) y + 4.0f, diameter, diameter);
    const auto centre = bounds.getCentre();
    const auto radius = bounds.getWidth() * 0.5f;
    const auto angle = start + sliderPosition * (end - start);
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f, start, end, true);
    g.setColour(juce::Colour(0xff4c4b40));
    g.strokePath(track, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path value;
    value.addCentredArc(centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f, start, angle, true);
    g.setColour(juce::Colour(amber));
    g.strokePath(value, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xff24231e));
    g.fillEllipse(bounds.reduced(8.0f));
    g.setColour(juce::Colour(0xff777261));
    g.drawEllipse(bounds.reduced(8.0f), 1.0f);
    juce::Path pointer;
    pointer.addRoundedRectangle(-2.0f, -radius * 0.62f, 4.0f, radius * 0.42f, 2.0f);
    g.setColour(juce::Colour(bone));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void CommsEngineAudioProcessorEditor::ConsoleLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto fill = button.getToggleState() ? juce::Colour(0xff244b49) : juce::Colour(panelLift);
    if (highlighted) fill = fill.brighter(0.08f);
    if (down) fill = fill.darker(0.12f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(button.getToggleState() ? juce::Colour(cyan) : juce::Colour(line));
    g.drawRoundedRectangle(bounds, 4.0f, button.getToggleState() ? 1.5f : 1.0f);
}

void CommsEngineAudioProcessorEditor::ConsoleLookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down)
{
    juce::ignoreUnused(highlighted, down);
    auto bounds = button.getLocalBounds().toFloat();
    const auto toggle = juce::Rectangle<float>(bounds.getX() + 3.0f, bounds.getCentreY() - 8.0f, 34.0f, 16.0f);
    g.setColour(juce::Colour(button.getToggleState() ? 0xff285c55 : 0xff292a25));
    g.fillRoundedRectangle(toggle, 8.0f);
    g.setColour(juce::Colour(button.getToggleState() ? cyan : line));
    g.drawRoundedRectangle(toggle, 8.0f, 1.0f);
    const auto knobX = button.getToggleState() ? toggle.getRight() - 13.0f : toggle.getX() + 3.0f;
    g.setColour(juce::Colour(button.getToggleState() ? cyan : dimBone));
    g.fillEllipse(knobX, toggle.getY() + 3.0f, 10.0f, 10.0f);
    g.setColour(button.findColour(juce::ToggleButton::textColourId));
    g.setFont(uiFont(10.5f, true));
    g.drawText(button.getButtonText(), 45, 0, button.getWidth() - 48, button.getHeight(), juce::Justification::centredLeft);
}

void CommsEngineAudioProcessorEditor::ConsoleLookAndFeel::drawComboBox(
    juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    juce::Path arrow;
    arrow.addTriangle((float) width - 18.0f, (float) height * 0.42f,
                      (float) width - 10.0f, (float) height * 0.42f,
                      (float) width - 14.0f, (float) height * 0.62f);
    g.setColour(juce::Colour(cyan));
    g.fillPath(arrow);
}

juce::Font CommsEngineAudioProcessorEditor::ConsoleLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return uiFont(12.0f, true);
}

void CommsEngineAudioProcessorEditor::Panel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(panel));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(line));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    g.setColour(juce::Colour(cyan));
    g.fillRect(14.0f, 13.0f, 24.0f, 2.0f);
    g.setColour(juce::Colour(dimBone));
    g.setFont(uiFont(10.0f, true));
    g.drawText(title, 46, 5, getWidth() - 58, 19, juce::Justification::centredLeft);
}

juce::Rectangle<int> CommsEngineAudioProcessorEditor::Panel::contentBounds() const
{
    return getLocalBounds().withTrimmedTop(28).reduced(8, 5);
}

void CommsEngineAudioProcessorEditor::ConsoleDisplay::setState(
    float signal, int mode, float newFailure, float newDistance, bool alarm)
{
    signalLevel += (juce::jlimit(0.0f, 1.0f, signal) - signalLevel) * 0.22f;
    activeMode = juce::jlimit(0, 4, mode);
    failure = juce::jlimit(0.0f, 1.0f, newFailure);
    distance = juce::jlimit(0.0f, 1.0f, newDistance);
    alarmEnabled = alarm;
    repaint();
}

void CommsEngineAudioProcessorEditor::ConsoleDisplay::advance()
{
    phase += 0.035f + failure * 0.08f;
    if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
}

void CommsEngineAudioProcessorEditor::ConsoleDisplay::paint(juce::Graphics& g)
{
    auto outer = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff34382c));
    g.fillRoundedRectangle(outer, 12.0f);
    g.setColour(juce::Colour(0xff737760));
    g.drawRoundedRectangle(outer.reduced(0.5f), 12.0f, 1.5f);
    auto face = outer.reduced(15.0f);
    g.setColour(juce::Colour(0xff20251d));
    g.fillRoundedRectangle(face, 8.0f);

    auto upper = face.reduced(14.0f).removeFromTop(face.getHeight() * 0.44f);
    g.setColour(juce::Colour(ink));
    g.fillRoundedRectangle(upper, 5.0f);
    g.setColour(juce::Colour(0xff627259));
    g.drawRoundedRectangle(upper, 5.0f, 1.0f);
    auto modeRow = upper.reduced(12.0f).removeFromTop(42.0f);
    const char* modes[] { "LINE", "CELL", "ICOM", "P.A.", "ALRM" };
    const auto modeWidth = modeRow.getWidth() / 5.0f;
    for (int index = 0; index < 5; ++index)
    {
        auto cell = juce::Rectangle<float>(modeRow.getX() + modeWidth * index, modeRow.getY(), modeWidth - 4.0f, modeRow.getHeight());
        const auto active = index == activeMode;
        g.setColour(juce::Colour(active ? 0xff284e47 : 0xff171b17));
        g.fillRoundedRectangle(cell, 3.0f);
        g.setColour(juce::Colour(active ? cyan : 0xff495047));
        g.drawRoundedRectangle(cell, 3.0f, active ? 1.5f : 1.0f);
        if (active) g.fillEllipse(cell.getCentreX() - 3.0f, cell.getY() + 6.0f, 6.0f, 6.0f);
        g.setColour(juce::Colour(active ? bone : dimBone));
        g.setFont(uiFont(9.0f, true));
        g.drawText(modes[index], cell.toNearestInt().withTrimmedTop(15), juce::Justification::centred);
    }

    auto trace = upper.reduced(12.0f).withTrimmedTop(50.0f).withTrimmedBottom(10.0f);
    g.setColour(juce::Colour(0xff17201c));
    g.fillRect(trace);
    juce::Path waveform;
    for (int pixel = 0; pixel < (int) trace.getWidth(); ++pixel)
    {
        const auto t = (float) pixel / juce::jmax(1.0f, trace.getWidth());
        const auto breakup = std::sin(t * 34.0f + phase) * failure * 0.23f;
        const auto voice = std::sin(t * 12.0f + phase * 0.7f) * (0.16f + signalLevel * 0.52f);
        const auto y = trace.getCentreY() - (voice + breakup) * trace.getHeight() * 0.45f;
        if (pixel == 0) waveform.startNewSubPath(trace.getX(), y); else waveform.lineTo(trace.getX() + pixel, y);
    }
    g.setColour(juce::Colour(alarmEnabled ? danger : signalGreen).withAlpha(0.9f));
    g.strokePath(waveform, juce::PathStrokeType(1.5f));

    auto lower = face.reduced(14.0f).withTrimmedTop(face.getHeight() * 0.47f);
    auto meterRow = lower.removeFromTop(70.0f);
    auto meter = meterRow.removeFromLeft(meterRow.getWidth() * 0.58f);
    g.setColour(juce::Colour(0xffe5ddc4));
    g.fillRoundedRectangle(meter, 4.0f);
    g.setColour(juce::Colour(0xff554f42));
    g.drawRoundedRectangle(meter, 4.0f, 1.0f);
    g.setFont(uiFont(9.0f, true));
    g.drawText("SIGNAL", meter.toNearestInt().removeFromTop(18), juce::Justification::centred);
    const auto meterArea = meter.reduced(14.0f, 20.0f).withTrimmedTop(6.0f);
    g.setColour(juce::Colour(0xff36372e));
    g.fillRect(meterArea);
    g.setColour(signalLevel > 0.82f ? juce::Colour(danger) : juce::Colour(0xff72b66a));
    g.fillRect(meterArea.withWidth(meterArea.getWidth() * juce::jlimit(0.0f, 1.0f, signalLevel)));

    auto lamps = meterRow.withTrimmedLeft(10.0f);
    const struct { const char* text; bool on; std::uint32_t colour; } indicators[] {
        { "VOICE", signalLevel > 0.015f, signalGreen },
        { "DUPLEX", activeMode == 2 || activeMode == 1, cyan },
        { "REMOTE", distance > 0.35f, amber },
        { "ALARM", alarmEnabled, danger },
    };
    for (const auto& indicator : indicators)
    {
        auto row = lamps.removeFromTop(17.0f);
        g.setColour(juce::Colour(indicator.on ? indicator.colour : 0xff3d4037));
        g.fillEllipse(row.getX(), row.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setColour(juce::Colour(dimBone));
        g.setFont(uiFont(9.5f, true));
        g.drawText(indicator.text, row.toNearestInt().withTrimmedLeft(16), juce::Justification::centredLeft);
    }

    auto grill = face.withTrimmedTop(face.getHeight() * 0.74f).reduced(18.0f, 6.0f);
    g.setColour(juce::Colour(0xff0b0e0b));
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 22; ++column)
            g.fillEllipse(grill.getX() + column * (grill.getWidth() / 22.0f),
                          grill.getY() + row * 10.0f, 4.0f, 4.0f);
}

CommsEngineAudioProcessorEditor::Knob::Knob(
    APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> userChange)
{
    label.setText(text.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(bone));
    label.setFont(uiFont(10.5f, true));
    addAndMakeVisible(label);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 20);
    slider.setDoubleClickReturnValue(true, state.getParameter(id)->convertFrom0to1(state.getParameter(id)->getDefaultValue()));
    slider.onDragStart = std::move(userChange);
    addAndMakeVisible(slider);
    attachment = std::make_unique<APVTS::SliderAttachment>(state, id, slider);
}

void CommsEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    slider.setBounds(area.reduced(1));
}

void CommsEngineAudioProcessorEditor::Knob::setHint(const juce::String& hint)
{
    slider.setTooltip(hint);
    label.setTooltip(hint);
}

CommsEngineAudioProcessorEditor::Switch::Switch(
    APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> userChange)
{
    button.setButtonText(text.toUpperCase());
    button.setColour(juce::ToggleButton::textColourId, juce::Colour(bone));
    button.onClick = std::move(userChange);
    addAndMakeVisible(button);
    attachment = std::make_unique<APVTS::ButtonAttachment>(state, id, button);
}

void CommsEngineAudioProcessorEditor::Switch::resized() { button.setBounds(getLocalBounds().reduced(3)); }
void CommsEngineAudioProcessorEditor::Switch::setHint(const juce::String& hint) { button.setTooltip(hint); }

CommsEngineAudioProcessorEditor::Choice::Choice(
    APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> userChange)
{
    label.setText(text.toUpperCase(), juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(dimBone));
    label.setFont(uiFont(9.5f, true));
    addAndMakeVisible(label);
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id)))
        for (int i = 0; i < parameter->choices.size(); ++i) combo.addItem(parameter->choices[i], i + 1);
    combo.onChange = std::move(userChange);
    addAndMakeVisible(combo);
    attachment = std::make_unique<APVTS::ComboBoxAttachment>(state, id, combo);
}

void CommsEngineAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(16));
    combo.setBounds(area.removeFromTop(26));
}

void CommsEngineAudioProcessorEditor::Choice::setHint(const juce::String& hint)
{
    combo.setTooltip(hint);
    label.setTooltip(hint);
}

CommsEngineAudioProcessorEditor::CommsEngineAudioProcessorEditor(CommsEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.getAPVTS())
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);
    brandLabel.setText("B&E DIGITAL / LOST AUDIO", juce::dontSendNotification);
    brandLabel.setFont(uiFont(10.5f, true));
    brandLabel.setColour(juce::Label::textColourId, juce::Colour(cyan));
    addAndMakeVisible(brandLabel);
    titleLabel.setText("COMMS ENGINE", juce::dontSendNotification);
    titleLabel.setFont(uiFont(27.0f, true));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(bone));
    addAndMakeVisible(titleLabel);
    subtitleLabel.setText("DIEGETIC COMMUNICATION HARDWARE / V2", juce::dontSendNotification);
    subtitleLabel.setFont(uiFont(10.0f, true));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(dimBone));
    addAndMakeVisible(subtitleLabel);
    profileLabel.setText("PROFILE", juce::dontSendNotification);
    profileLabel.setFont(uiFont(9.5f, true));
    profileLabel.setColour(juce::Label::textColourId, juce::Colour(dimBone));
    addAndMakeVisible(profileLabel);
    presetBox.addItem("Custom", 1);
    for (int i = 0; i < (int) std::size(presets); ++i) presetBox.addItem(presets[i].name, i + 2);
    presetBox.onChange = [this]
    {
        if (const auto selected = presetBox.getSelectedId(); selected >= 2) applyPreset(selected - 2);
    };
    presetBox.setSelectedId(1, juce::dontSendNotification);
    presetBox.setTooltip("Safe profiles for handsets, intercoms, PA systems, cellular paths and alarm panels.");
    addAndMakeVisible(presetBox);

    for (auto* button : { &surfaceButton, &advancedButton })
    {
        button->setClickingTogglesState(false);
        button->setColour(juce::TextButton::textColourOffId, juce::Colour(bone));
        button->setColour(juce::TextButton::textColourOnId, juce::Colour(ink));
        addAndMakeVisible(*button);
    }
    surfaceButton.onClick = [this] { showAdvanced(false); };
    advancedButton.onClick = [this] { showAdvanced(true); };
    statusLabel.setFont(uiFont(9.5f, true));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(dimBone));
    statusLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(statusLabel);

    addAndMakeVisible(surfacePage);
    addAndMakeVisible(advancedPage);
    surfacePage.addAndMakeVisible(consoleDisplay);
    surfacePage.addAndMakeVisible(characterPanel);
    surfacePage.addAndMakeVisible(outputPanel);
    for (auto* panelComponent : { &tonePanel, &transmitterPanel, &receptionPanel, &noisePanel, &squelchPanel, &searchPanel })
        advancedPage.addAndMakeVisible(*panelComponent);

    addChoice(characterPanel, "mode", "Hardware", "Choose a landline, cellular path, intercom, PA horn or alarm panel.");
    addKnob(characterPanel, "bandwidth", "Bandwidth", "Protected voice-band width for the selected hardware.", true);
    addKnob(characterPanel, "drive", "Drive", "Level-matched carbon, electronics or amplifier drive.", true);
    addKnob(characterPanel, "glitch", "Failure", "Packet loss, converter instability and line interruptions.", true);
    addKnob(characterPanel, "noise", "Line Noise", "Protected hum, hiss and aging-line activity.", true);
    addKnob(characterPanel, "character", "Hardware", "Physical transducer and enclosure character.", true);
    addKnob(characterPanel, "distance", "Distance", "Move the device into its surrounding space.", true);
    addSwitch(characterPanel, "alarmTone", "Alarm Tone", "Enable the device's two-tone warning generator.");
    addKnob(outputPanel, "inputGain", "Input", "Trim before the communications path.");
    addKnob(outputPanel, "mix", "Mix", "Dry/wet balance.");
    addKnob(outputPanel, "outGain", "Output", "Final protected output trim.");

    addSwitch(tonePanel, "macroLink", "Surface Link", "On: Surface controls drive a protected hardware model. Off: direct circuit controls are active.");
    for (auto* linked : {
            addKnob(tonePanel, "hpHz", "High-pass", "Direct voice-band low cutoff."),
            addKnob(tonePanel, "lpHz", "Low-pass", "Direct voice-band high cutoff."),
            addKnob(tonePanel, "midHumpDb", "Presence", "Direct communications-band emphasis."),
            addKnob(tonePanel, "midFreq", "Presence Hz", "Voice presence center frequency.") }) linkedAdvancedControls.push_back(linked);

    for (auto* linked : {
            addKnob(transmitterPanel, "comp", "AGC", "Telecom automatic gain riding."),
            addKnob(transmitterPanel, "bits", "Codec Bits", "Quantizer depth."),
            addKnob(transmitterPanel, "rate", "Rate", "Converter sample-and-hold rate.") }) linkedAdvancedControls.push_back(linked);
    addKnob(transmitterPanel, "drive", "Drive", "Carbon, electronic or amplifier nonlinearity.");
    addKnob(transmitterPanel, "inputGain", "Input", "Input trim before all processing.");
    addKnob(transmitterPanel, "mix", "Mix", "Dry/wet balance.");
    addKnob(transmitterPanel, "outGain", "Output", "Final output trim.");

    for (auto* linked : {
            addKnob(receptionPanel, "packet", "Dropouts", "Probability of a line or codec block dropping."),
            addKnob(receptionPanel, "packetMs", "Block Ms", "Length of each failure block."),
            addKnob(receptionPanel, "duplex", "Half Duplex", "Talk-path clamp and release behavior."),
            addKnob(receptionPanel, "lineAge", "Line Age", "Carbon granules and aging electronics.") }) linkedAdvancedControls.push_back(linked);

    for (auto* linked : {
            addKnob(noisePanel, "hum", "Hum", "Mode-aware mains and line hum."),
            addKnob(noisePanel, "hiss", "Hiss", "Line and transducer hiss."),
            addKnob(noisePanel, "transducer", "Body", "Receiver, intercom box or horn resonances."),
            addKnob(noisePanel, "speakerRattle", "Rattle", "Signal-excited diaphragm and enclosure rattle.") }) linkedAdvancedControls.push_back(linked);

    addKnob(squelchPanel, "distance", "Distance", "Acoustic distance and perspective.");
    for (auto* linked : {
            addKnob(squelchPanel, "echoMix", "Echo Mix", "Early reflection level."),
            addKnob(squelchPanel, "echoMs", "Echo Time", "Early reflection delay."),
            addKnob(squelchPanel, "echoFb", "Feedback", "Repeated paging-system reflections."),
            addKnob(squelchPanel, "echoTone", "Echo Tone", "Bandwidth of repeated reflections.") }) linkedAdvancedControls.push_back(linked);

    for (auto* linked : {
            addKnob(searchPanel, "verbMix", "Room Mix", "Device-space reverberation level."),
            addKnob(searchPanel, "verbMs", "Room Decay", "Room response length."),
            addKnob(searchPanel, "verbDamp", "Damping", "Room high-frequency absorption."),
            addKnob(searchPanel, "ceiling", "Ceiling", "Protected output ceiling.") }) linkedAdvancedControls.push_back(linked);
    addSwitch(searchPanel, "alarmTone", "Alarm Tone", "Enable the panel or paging alarm generator.");
    addKnob(searchPanel, "toneMix", "Alarm Level", "Warning-tone contribution.");

    setResizable(true, true);
    setResizeLimits(900, 620, 1600, 1000);
    setSize(1100, 720);
    showAdvanced(false);
    startTimerHz(24);
}

CommsEngineAudioProcessorEditor::~CommsEngineAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

CommsEngineAudioProcessorEditor::Knob* CommsEngineAudioProcessorEditor::addKnob(
    Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint, bool surfaceMacro)
{
    auto callback = [this, surfaceMacro]
    {
        if (surfaceMacro) setParameter("macroLink", 1.0f);
        markCustom();
    };
    auto control = std::make_unique<Knob>(apvts, id, text, callback);
    control->setHint(hint);
    auto* result = control.get();
    owner.addAndMakeVisible(*control);
    panelItems[&owner].push_back(result);
    knobs.push_back(std::move(control));
    return result;
}

CommsEngineAudioProcessorEditor::Switch* CommsEngineAudioProcessorEditor::addSwitch(
    Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto control = std::make_unique<Switch>(apvts, id, text, [this] { markCustom(); });
    control->setHint(hint);
    auto* result = control.get();
    owner.addAndMakeVisible(*control);
    panelItems[&owner].push_back(result);
    switches.push_back(std::move(control));
    return result;
}

CommsEngineAudioProcessorEditor::Choice* CommsEngineAudioProcessorEditor::addChoice(
    Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto control = std::make_unique<Choice>(apvts, id, text, [this] { markCustom(); });
    control->setHint(hint);
    auto* result = control.get();
    owner.addAndMakeVisible(*control);
    panelItems[&owner].push_back(result);
    choices.push_back(std::move(control));
    return result;
}

void CommsEngineAudioProcessorEditor::layoutPanel(Panel& owner, int columns)
{
    auto area = owner.contentBounds();
    auto& items = panelItems[&owner];
    if (items.empty()) return;
    const auto cols = juce::jmax(1, columns);
    const auto rows = juce::jmax(1, ((int) items.size() + cols - 1) / cols);
    const auto width = area.getWidth() / cols;
    const auto height = area.getHeight() / rows;
    for (int index = 0; index < (int) items.size(); ++index)
        items[(std::size_t) index]->setBounds(area.getX() + (index % cols) * width,
                                             area.getY() + (index / cols) * height,
                                             width, height);
}

void CommsEngineAudioProcessorEditor::showAdvanced(bool shouldShowAdvanced)
{
    showingAdvanced = shouldShowAdvanced;
    surfacePage.setVisible(!showingAdvanced);
    advancedPage.setVisible(showingAdvanced);
    surfaceButton.setToggleState(!showingAdvanced, juce::dontSendNotification);
    advancedButton.setToggleState(showingAdvanced, juce::dontSendNotification);
    resized();
}

void CommsEngineAudioProcessorEditor::setParameter(const juce::String& id, float plainValue)
{
    if (auto* parameter = apvts.getParameter(id)) parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

float CommsEngineAudioProcessorEditor::getParameter(const juce::String& id) const
{
    if (auto* value = apvts.getRawParameterValue(id)) return value->load();
    return 0.0f;
}

void CommsEngineAudioProcessorEditor::markCustom()
{
    if (!suppressPresetChanges) presetBox.setSelectedId(1, juce::dontSendNotification);
}

void CommsEngineAudioProcessorEditor::applyPreset(int index)
{
    if (index < 0 || index >= (int) std::size(presets)) return;
    suppressPresetChanges = true;
    for (auto* parameter : processor.getParameters()) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    for (const auto& [id, value] : presets[(std::size_t) index].values) setParameter(id, value);
    suppressPresetChanges = false;
}

void CommsEngineAudioProcessorEditor::timerCallback()
{
    const auto linked = getParameter("macroLink") > 0.5f;
    for (auto* control : linkedAdvancedControls) control->setEnabled(!linked);
    consoleDisplay.setState(processor.getOutputPeak(), (int) getParameter("mode"),
                            getParameter("glitch"), getParameter("distance"),
                            getParameter("alarmTone") > 0.5f);
    consoleDisplay.advance();
    statusLabel.setText(linked ? "SURFACE LINK  /  PROTECTED COMMS PATH"
                               : "ADVANCED CIRCUIT  /  DIRECT HARDWARE CONTROL",
                        juce::dontSendNotification);
}

void CommsEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ink));
    auto header = getLocalBounds().removeFromTop(112).toFloat();
    g.setColour(juce::Colour(deep));
    g.fillRect(header);
    g.setColour(juce::Colour(cyan));
    g.fillRect(16.0f, 0.0f, 190.0f, 2.0f);
    g.setColour(juce::Colour(amber));
    g.fillRect((float) getWidth() - 150.0f, 0.0f, 134.0f, 2.0f);
    g.setColour(juce::Colour(0xff343a31));
    g.drawHorizontalLine(111, 0.0f, (float) getWidth());
    g.setColour(juce::Colour(0xff171a16).withAlpha(0.62f));
    for (int y = 112; y < getHeight(); y += 4) g.drawHorizontalLine(y, 0.0f, (float) getWidth());
}

void CommsEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto header = area.removeFromTop(94);
    auto profile = header.removeFromRight(270);
    profileLabel.setBounds(profile.removeFromTop(17));
    presetBox.setBounds(profile.removeFromTop(32));
    auto heading = header;
    brandLabel.setBounds(heading.removeFromTop(18));
    titleLabel.setBounds(heading.removeFromTop(35));
    subtitleLabel.setBounds(heading.removeFromTop(18));
    auto modeRow = area.removeFromTop(40);
    surfaceButton.setBounds(modeRow.removeFromLeft(108).reduced(0, 4));
    modeRow.removeFromLeft(8);
    advancedButton.setBounds(modeRow.removeFromLeft(108).reduced(0, 4));
    statusLabel.setBounds(modeRow);
    area.removeFromTop(6);
    surfacePage.setBounds(area);
    advancedPage.setBounds(area);

    if (!showingAdvanced)
    {
        auto surface = surfacePage.getLocalBounds();
        auto display = surface.removeFromLeft((int) std::round(surface.getWidth() * 0.56f));
        consoleDisplay.setBounds(display.reduced(0, 4).withTrimmedRight(7));
        auto controls = surface.withTrimmedLeft(7);
        characterPanel.setBounds(controls.removeFromTop((int) std::round(controls.getHeight() * 0.62f)).withTrimmedBottom(6));
        outputPanel.setBounds(controls.withTrimmedTop(6));
        layoutPanel(characterPanel, 2);
        layoutPanel(outputPanel, 3);
    }
    else
    {
        auto advanced = advancedPage.getLocalBounds();
        const auto gap = 8;
        const auto columnWidth = (advanced.getWidth() - gap * 2) / 3;
        const auto rowHeight = (advanced.getHeight() - gap) / 2;
        Panel* panels[] { &tonePanel, &transmitterPanel, &receptionPanel, &noisePanel, &squelchPanel, &searchPanel };
        for (int index = 0; index < 6; ++index)
        {
            const auto column = index % 3;
            const auto row = index / 3;
            panels[index]->setBounds(column * (columnWidth + gap), row * (rowHeight + gap), columnWidth, rowHeight);
        }
        layoutPanel(tonePanel, 3);
        layoutPanel(transmitterPanel, 4);
        layoutPanel(receptionPanel, 2);
        layoutPanel(noisePanel, 2);
        layoutPanel(squelchPanel, 3);
        layoutPanel(searchPanel, 3);
    }
}
