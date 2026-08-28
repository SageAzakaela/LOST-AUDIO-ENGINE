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
    { "Studio Broadcast", { { "bandwidth", 0.78f }, { "drive", 0.10f }, { "badConnection", 0.02f }, { "noiseProfile", 0.03f }, { "macroLink", 1.0f }, { "passes", 1.0f }, { "outGain", 0.98f } } },
    { "Portable AM / Soft", { { "bandwidth", 0.45f }, { "drive", 0.18f }, { "badConnection", 0.06f }, { "noiseProfile", 0.10f }, { "macroLink", 1.0f }, { "passes", 1.0f }, { "outGain", 0.96f } } },
    { "Cheap AM Radio", { { "bandwidth", 0.25f }, { "drive", 0.45f }, { "badConnection", 0.18f }, { "noiseProfile", 0.28f }, { "macroLink", 1.0f }, { "crush", 0.12f }, { "passes", 2.0f }, { "outGain", 0.92f } } },
    { "Police Scanner", { { "bandwidth", 0.35f }, { "drive", 0.30f }, { "badConnection", 0.12f }, { "noiseProfile", 0.18f }, { "macroLink", 1.0f }, { "crush", 0.04f }, { "passes", 1.0f }, { "outGain", 0.92f } } },
    { "Narrow Scanner", { { "bandwidth", 0.12f }, { "drive", 0.22f }, { "badConnection", 0.08f }, { "noiseProfile", 0.12f }, { "macroLink", 1.0f }, { "crush", 0.02f }, { "passes", 1.0f }, { "outGain", 0.96f } } },
    { "Worn Walkie", { { "bandwidth", 0.22f }, { "drive", 0.55f }, { "badConnection", 0.35f }, { "noiseProfile", 0.35f }, { "macroLink", 1.0f }, { "walkieMode", 1.0f }, { "walkieFx", 0.0f }, { "walkieClickLevel", 0.75f }, { "crush", 0.28f }, { "passes", 2.0f }, { "outGain", 0.90f } } },
    { "Dispatch Hot", { { "bandwidth", 0.22f }, { "drive", 0.68f }, { "badConnection", 0.18f }, { "noiseProfile", 0.20f }, { "macroLink", 1.0f }, { "walkieMode", 1.0f }, { "walkieFx", 1.0f }, { "walkieClickMs", 120.0f }, { "walkieClickLevel", 0.75f }, { "crush", 0.22f }, { "passes", 2.0f }, { "outGain", 0.86f } } },
    { "Field Set / 1944", { { "bandwidth", 0.18f }, { "drive", 0.42f }, { "badConnection", 0.22f }, { "noiseProfile", 0.32f }, { "macroLink", 1.0f }, { "crush", 0.12f }, { "passes", 2.0f }, { "outGain", 0.92f } } },
    { "Weak Signal", { { "bandwidth", 0.26f }, { "drive", 0.32f }, { "badConnection", 0.60f }, { "noiseProfile", 0.38f }, { "macroLink", 1.0f }, { "tuningEnable", 1.0f }, { "tuningMode", 0.0f }, { "tuningAmount", 0.35f }, { "crush", 0.14f }, { "passes", 2.0f }, { "outGain", 0.90f } } },
    { "Storm Front", { { "bandwidth", 0.30f }, { "drive", 0.36f }, { "badConnection", 0.55f }, { "noiseProfile", 0.55f }, { "macroLink", 1.0f }, { "crush", 0.25f }, { "passes", 3.0f }, { "outGain", 0.88f } } },
    { "Squelch Hunt", { { "bandwidth", 0.16f }, { "drive", 0.34f }, { "badConnection", 0.32f }, { "noiseProfile", 0.28f }, { "macroLink", 1.0f }, { "tuningEnable", 1.0f }, { "tuningMode", 1.0f }, { "tuningSource", 7.0f }, { "tuningAmount", 0.55f }, { "tuningCutDepth", 0.78f }, { "passes", 2.0f }, { "outGain", 0.90f } } },
    { "Gentle Tuning Edges", { { "bandwidth", 0.34f }, { "drive", 0.16f }, { "badConnection", 0.10f }, { "noiseProfile", 0.12f }, { "macroLink", 1.0f }, { "tuningEnable", 1.0f }, { "tuningMode", 0.0f }, { "tuningAmount", 0.18f }, { "tuningCutDepth", 0.35f }, { "passes", 1.0f }, { "outGain", 0.96f } } },
    { "Pocket Receiver Crush", { { "bandwidth", 0.08f }, { "drive", 0.55f }, { "badConnection", 0.14f }, { "noiseProfile", 0.22f }, { "macroLink", 1.0f }, { "crush", 0.42f }, { "passes", 3.0f }, { "outGain", 0.85f } } },
    { "Analog Horror", { { "bandwidth", 0.20f }, { "drive", 0.70f }, { "badConnection", 0.70f }, { "noiseProfile", 0.65f }, { "macroLink", 1.0f }, { "crush", 0.48f }, { "tuningEnable", 1.0f }, { "tuningMode", 1.0f }, { "tuningAmount", 0.75f }, { "tuningCutDepth", 0.88f }, { "passes", 4.0f }, { "outGain", 0.78f } } },
    { "Numbers Station", { { "bandwidth", 0.28f }, { "drive", 0.25f }, { "badConnection", 0.28f }, { "noiseProfile", 0.22f }, { "macroLink", 1.0f }, { "tuningEnable", 1.0f }, { "tuningMode", 1.0f }, { "tuningSource", 1.0f }, { "tuningAmount", 0.32f }, { "tuningSnippetMs", 280.0f }, { "tuningCutDepth", 0.64f }, { "passes", 2.0f }, { "outGain", 0.90f } } },
    { "Ghost Carrier", { { "bandwidth", 0.38f }, { "drive", 0.30f }, { "badConnection", 0.82f }, { "noiseProfile", 0.48f }, { "macroLink", 1.0f }, { "tuningEnable", 1.0f }, { "tuningMode", 1.0f }, { "tuningSource", 0.0f }, { "tuningAmount", 0.64f }, { "tuningSnippetMs", 360.0f }, { "tuningCutDepth", 0.92f }, { "passes", 3.0f }, { "outGain", 0.82f } } },
};
}

TransmissionEngineAudioProcessorEditor::ReceiverLookAndFeel::ReceiverLookAndFeel()
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

void TransmissionEngineAudioProcessorEditor::ReceiverLookAndFeel::drawRotarySlider(
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

void TransmissionEngineAudioProcessorEditor::ReceiverLookAndFeel::drawButtonBackground(
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

void TransmissionEngineAudioProcessorEditor::ReceiverLookAndFeel::drawToggleButton(
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

void TransmissionEngineAudioProcessorEditor::ReceiverLookAndFeel::drawComboBox(
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

juce::Font TransmissionEngineAudioProcessorEditor::ReceiverLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return uiFont(12.0f, true);
}

void TransmissionEngineAudioProcessorEditor::Panel::paint(juce::Graphics& g)
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

juce::Rectangle<int> TransmissionEngineAudioProcessorEditor::Panel::contentBounds() const
{
    return getLocalBounds().withTrimmedTop(28).reduced(8, 5);
}

void TransmissionEngineAudioProcessorEditor::ReceiverDisplay::setState(
    float signal, float newBandwidth, float newDamage, bool squelch, bool searching)
{
    signalLevel += (juce::jlimit(0.0f, 1.0f, signal) - signalLevel) * 0.22f;
    bandwidth = juce::jlimit(0.0f, 1.0f, newBandwidth);
    damage = juce::jlimit(0.0f, 1.0f, newDamage);
    squelchEnabled = squelch;
    searchEnabled = searching;
    repaint();
}

void TransmissionEngineAudioProcessorEditor::ReceiverDisplay::advance()
{
    phase += 0.035f + damage * 0.08f;
    if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
}

void TransmissionEngineAudioProcessorEditor::ReceiverDisplay::paint(juce::Graphics& g)
{
    auto outer = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff34382c));
    g.fillRoundedRectangle(outer, 12.0f);
    g.setColour(juce::Colour(0xff737760));
    g.drawRoundedRectangle(outer.reduced(0.5f), 12.0f, 1.5f);
    auto face = outer.reduced(15.0f);
    g.setColour(juce::Colour(0xff20251d));
    g.fillRoundedRectangle(face, 8.0f);

    auto tuner = face.reduced(14.0f).removeFromTop(face.getHeight() * 0.42f);
    g.setColour(juce::Colour(ink));
    g.fillRoundedRectangle(tuner, 5.0f);
    g.setColour(juce::Colour(0xff627259));
    g.drawRoundedRectangle(tuner, 5.0f, 1.0f);
    g.setFont(uiFont(9.5f, true));
    g.setColour(juce::Colour(signalGreen).withAlpha(0.75f));
    const char* marks[] { "540", "700", "1000", "1400", "1700" };
    for (int i = 0; i < 17; ++i)
    {
        const auto x = tuner.getX() + 18.0f + (tuner.getWidth() - 36.0f) * (float) i / 16.0f;
        const auto major = i % 4 == 0;
        g.drawVerticalLine((int) x, tuner.getBottom() - (major ? 29.0f : 19.0f), tuner.getBottom() - 9.0f);
        if (major) g.drawText(marks[i / 4], (int) x - 18, (int) tuner.getY() + 9, 36, 14, juce::Justification::centred);
    }
    const auto jitter = std::sin(phase * 2.7f) * damage * 0.035f;
    const auto needlePosition = juce::jlimit(0.03f, 0.97f, 0.18f + bandwidth * 0.64f + jitter);
    const auto needleX = tuner.getX() + tuner.getWidth() * needlePosition;
    g.setColour(juce::Colour(danger));
    g.fillRect(needleX - 1.0f, tuner.getY() + 6.0f, 2.0f, tuner.getHeight() - 12.0f);
    g.setColour(juce::Colour(bone));
    g.setFont(uiFont(10.0f, true));
    g.drawText("AM  /  LOST BAND", tuner.toNearestInt().withTrimmedTop((int) tuner.getHeight() - 22), juce::Justification::centred);

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
        { "CARRIER", signalLevel > 0.015f, signalGreen },
        { "SQUELCH", squelchEnabled, cyan },
        { "SEARCH", searchEnabled, amber },
    };
    for (const auto& indicator : indicators)
    {
        auto row = lamps.removeFromTop(23.0f);
        g.setColour(juce::Colour(indicator.on ? indicator.colour : 0xff3d4037));
        g.fillEllipse(row.getX(), row.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setColour(juce::Colour(dimBone));
        g.setFont(uiFont(9.5f, true));
        g.drawText(indicator.text, row.toNearestInt().withTrimmedLeft(16), juce::Justification::centredLeft);
    }

    auto grill = face.withTrimmedTop(face.getHeight() * 0.72f).reduced(18.0f, 6.0f);
    g.setColour(juce::Colour(0xff0b0e0b));
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 22; ++column)
            g.fillEllipse(grill.getX() + column * (grill.getWidth() / 22.0f),
                          grill.getY() + row * 10.0f, 4.0f, 4.0f);
}

TransmissionEngineAudioProcessorEditor::Knob::Knob(
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

void TransmissionEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    slider.setBounds(area.reduced(1));
}

void TransmissionEngineAudioProcessorEditor::Knob::setHint(const juce::String& hint)
{
    slider.setTooltip(hint);
    label.setTooltip(hint);
}

TransmissionEngineAudioProcessorEditor::Switch::Switch(
    APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> userChange)
{
    button.setButtonText(text.toUpperCase());
    button.setColour(juce::ToggleButton::textColourId, juce::Colour(bone));
    button.onClick = std::move(userChange);
    addAndMakeVisible(button);
    attachment = std::make_unique<APVTS::ButtonAttachment>(state, id, button);
}

void TransmissionEngineAudioProcessorEditor::Switch::resized() { button.setBounds(getLocalBounds().reduced(3)); }
void TransmissionEngineAudioProcessorEditor::Switch::setHint(const juce::String& hint) { button.setTooltip(hint); }

TransmissionEngineAudioProcessorEditor::Choice::Choice(
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

void TransmissionEngineAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(16));
    combo.setBounds(area.removeFromTop(26));
}

void TransmissionEngineAudioProcessorEditor::Choice::setHint(const juce::String& hint)
{
    combo.setTooltip(hint);
    label.setTooltip(hint);
}

TransmissionEngineAudioProcessorEditor::TransmissionEngineAudioProcessorEditor(TransmissionEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.getAPVTS())
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);
    brandLabel.setText("B&E DIGITAL / LOST AUDIO", juce::dontSendNotification);
    brandLabel.setFont(uiFont(10.5f, true));
    brandLabel.setColour(juce::Label::textColourId, juce::Colour(cyan));
    addAndMakeVisible(brandLabel);
    titleLabel.setText("TRANSMISSION ENGINE", juce::dontSendNotification);
    titleLabel.setFont(uiFont(27.0f, true));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(bone));
    addAndMakeVisible(titleLabel);
    subtitleLabel.setText("BROADCAST SIGNAL WEATHERING / V2", juce::dontSendNotification);
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
    presetBox.setTooltip("Safe factory receiver profiles from clean broadcast through stylized carrier failure.");
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
    surfacePage.addAndMakeVisible(receiverDisplay);
    surfacePage.addAndMakeVisible(characterPanel);
    surfacePage.addAndMakeVisible(outputPanel);
    for (auto* panelComponent : { &tonePanel, &transmitterPanel, &receptionPanel, &noisePanel, &squelchPanel, &searchPanel })
        advancedPage.addAndMakeVisible(*panelComponent);

    addKnob(characterPanel, "bandwidth", "Bandwidth", "Receiver passband from full broadcast to narrow communications audio.", true);
    addKnob(characterPanel, "drive", "Transmitter", "Level-matched transmitter saturation and compression character.", true);
    addKnob(characterPanel, "badConnection", "Reception", "Carrier drift, dropout and transient interference intensity.", true);
    addKnob(characterPanel, "noiseProfile", "Air Noise", "Protected RF/static bed with linked hiss and color.", true);
    addKnob(outputPanel, "inputGain", "Input", "Trim before transmitter and receiver processing.");
    addKnob(outputPanel, "mix", "Mix", "Latency-aligned dry/wet balance.");
    addKnob(outputPanel, "outGain", "Output", "Final protected receiver output.");
    addKnob(outputPanel, "passes", "Generations", "Repeat the complete transmission stage up to six times.");

    addSwitch(tonePanel, "macroLink", "Surface Link", "On: the four Surface controls drive a protected receiver model. Off: direct circuit controls are active.");
    for (auto* linked : {
            addKnob(tonePanel, "hpHz", "High-pass", "Direct receiver low-frequency cutoff."),
            addKnob(tonePanel, "lpHz", "Low-pass", "Direct receiver high-frequency cutoff."),
            addKnob(tonePanel, "midGainDb", "Presence", "Direct communications-band emphasis."),
            addKnob(tonePanel, "midFreq", "Presence Hz", "Center frequency for vocal presence."),
            addKnob(tonePanel, "midQ", "Presence Q", "Width of the presence resonance."),
            addKnob(tonePanel, "boxDipDb", "Cabinet Dip", "Cardboard receiver-cabinet notch.") }) linkedAdvancedControls.push_back(linked);

    for (auto* linked : {
            addKnob(transmitterPanel, "comp", "Compression", "Direct transmitter leveler amount."),
            addKnob(transmitterPanel, "asym", "Asymmetry", "Direct nonlinear bias." ) }) linkedAdvancedControls.push_back(linked);
    addKnob(transmitterPanel, "crush", "Converter Loss", "Quantization and sample-hold loss after saturation.");
    addKnob(transmitterPanel, "passes", "Generations", "Repeat the receiver chain.");
    addKnob(transmitterPanel, "inputGain", "Input", "Input trim before all processing.");
    addKnob(transmitterPanel, "mix", "Mix", "Latency-aligned dry/wet blend.");
    addKnob(transmitterPanel, "outGain", "Output", "Final output trim.");

    for (auto* linked : {
            addKnob(receptionPanel, "wowDepth", "Carrier Drift", "Causal time displacement, not tremolo."),
            addKnob(receptionPanel, "dropRate", "Drop Rate", "Frequency of carrier losses."),
            addKnob(receptionPanel, "dropDepth", "Drop Depth", "Depth of carrier losses."),
            addKnob(receptionPanel, "crackle", "Interference", "Short RF transient events."),
            addKnob(receptionPanel, "lfoRate", "Drift Rate", "Carrier drift speed.") }) linkedAdvancedControls.push_back(linked);

    for (auto* linked : {
            addKnob(noisePanel, "noiseColor", "Noise Color", "White-to-pink RF bed color."),
            addKnob(noisePanel, "hiss", "Hiss", "High-frequency emphasis in the RF bed.") }) linkedAdvancedControls.push_back(linked);
    addKnob(noisePanel, "noiseProfile", "Bed Level", "Overall protected RF/noise level.");
    addKnob(noisePanel, "crush", "Digital Loss", "Cheap converter degradation.");

    addSwitch(squelchPanel, "walkieMode", "Squelch Gate", "Generate causal open/close events from input silence.");
    addChoice(squelchPanel, "walkieFx", "Event", "Short receiver click or dispatch two-tone event.");
    addKnob(squelchPanel, "walkieThresholdDb", "Threshold", "Input level used to detect gate changes.");
    addKnob(squelchPanel, "walkieMinSilenceMs", "Hold", "Silence required before closing the squelch.");
    addKnob(squelchPanel, "walkieClickMs", "Length", "Length of the click or dispatch event.");
    addKnob(squelchPanel, "walkieClickLevel", "Level", "Protected squelch-event level.");

    addSwitch(searchPanel, "tuningEnable", "Search Events", "Enable edge or probabilistic tuning events.");
    addChoice(searchPanel, "tuningMode", "Mode", "Edges reacts to transport boundaries; Search generates events while playing.");
    addChoice(searchPanel, "tuningSource", "Source", "Synthesized carrier sweep or embedded tuning recording.");
    addKnob(searchPanel, "tuningAmount", "Activity", "Search strength and probability.");
    addKnob(searchPanel, "tuningSnippetMs", "Length", "Duration of each tuning event.");
    addKnob(searchPanel, "tuningCutDepth", "Signal Cut", "How deeply search events replace the program signal.");

    setResizable(true, true);
    setResizeLimits(900, 620, 1600, 1000);
    setSize(1100, 720);
    showAdvanced(false);
    startTimerHz(24);
}

TransmissionEngineAudioProcessorEditor::~TransmissionEngineAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

TransmissionEngineAudioProcessorEditor::Knob* TransmissionEngineAudioProcessorEditor::addKnob(
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

TransmissionEngineAudioProcessorEditor::Switch* TransmissionEngineAudioProcessorEditor::addSwitch(
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

TransmissionEngineAudioProcessorEditor::Choice* TransmissionEngineAudioProcessorEditor::addChoice(
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

void TransmissionEngineAudioProcessorEditor::layoutPanel(Panel& owner, int columns)
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

void TransmissionEngineAudioProcessorEditor::showAdvanced(bool shouldShowAdvanced)
{
    showingAdvanced = shouldShowAdvanced;
    surfacePage.setVisible(!showingAdvanced);
    advancedPage.setVisible(showingAdvanced);
    surfaceButton.setToggleState(!showingAdvanced, juce::dontSendNotification);
    advancedButton.setToggleState(showingAdvanced, juce::dontSendNotification);
    resized();
}

void TransmissionEngineAudioProcessorEditor::setParameter(const juce::String& id, float plainValue)
{
    if (auto* parameter = apvts.getParameter(id)) parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

float TransmissionEngineAudioProcessorEditor::getParameter(const juce::String& id) const
{
    if (auto* value = apvts.getRawParameterValue(id)) return value->load();
    return 0.0f;
}

void TransmissionEngineAudioProcessorEditor::markCustom()
{
    if (!suppressPresetChanges) presetBox.setSelectedId(1, juce::dontSendNotification);
}

void TransmissionEngineAudioProcessorEditor::applyPreset(int index)
{
    if (index < 0 || index >= (int) std::size(presets)) return;
    suppressPresetChanges = true;
    for (auto* parameter : processor.getParameters()) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    for (const auto& [id, value] : presets[(std::size_t) index].values) setParameter(id, value);
    suppressPresetChanges = false;
}

void TransmissionEngineAudioProcessorEditor::timerCallback()
{
    const auto linked = getParameter("macroLink") > 0.5f;
    for (auto* control : linkedAdvancedControls) control->setEnabled(!linked);
    const auto squelch = getParameter("walkieMode") > 0.5f;
    const auto searching = getParameter("tuningEnable") > 0.5f;
    receiverDisplay.setState(processor.getOutputPeak(), getParameter("bandwidth"), getParameter("badConnection"), squelch, searching);
    receiverDisplay.advance();
    statusLabel.setText(linked ? "SURFACE LINK  /  PROTECTED SIGNAL PATH"
                               : "ADVANCED CIRCUIT  /  DIRECT CONTROL",
                        juce::dontSendNotification);
}

void TransmissionEngineAudioProcessorEditor::paint(juce::Graphics& g)
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

void TransmissionEngineAudioProcessorEditor::resized()
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
        receiverDisplay.setBounds(display.reduced(0, 4).withTrimmedRight(7));
        auto controls = surface.withTrimmedLeft(7);
        characterPanel.setBounds(controls.removeFromTop((int) std::round(controls.getHeight() * 0.62f)).withTrimmedBottom(6));
        outputPanel.setBounds(controls.withTrimmedTop(6));
        layoutPanel(characterPanel, 2);
        layoutPanel(outputPanel, 4);
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
        layoutPanel(receptionPanel, 3);
        layoutPanel(noisePanel, 2);
        layoutPanel(squelchPanel, 3);
        layoutPanel(searchPanel, 3);
    }
}
