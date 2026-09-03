#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr std::uint32_t ink = 0xff080a0b;
constexpr std::uint32_t deep = 0xff111415;
constexpr std::uint32_t panel = 0xff1d201f;
constexpr std::uint32_t panelLift = 0xff272a28;
constexpr std::uint32_t line = 0xff5f6259;
constexpr std::uint32_t bone = 0xfff0e8d7;
constexpr std::uint32_t dimBone = 0xffaaa79d;
constexpr std::uint32_t cyan = 0xff55e6ed;
constexpr std::uint32_t magenta = 0xffff43c8;
constexpr std::uint32_t amber = 0xffffb342;
constexpr std::uint32_t danger = 0xffff5a55;

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
    { "Clean Player", { { "clarity", 0.96f }, { "damage", 0.01f }, { "tracking", 0.01f }, { "jitterMacro", 0.01f }, { "mode", 2.0f }, { "damageShape", 0.0f }, { "carComp", 0.02f }, { "macroLink", 1.0f } } },
    { "Factory Test Disc", { { "clarity", 0.88f }, { "damage", 0.10f }, { "tracking", 0.04f }, { "jitterMacro", 0.04f }, { "mode", 2.0f }, { "damageShape", 1.0f }, { "macroLink", 1.0f } } },
    { "Dusty Disc", { { "clarity", 0.66f }, { "damage", 0.28f }, { "tracking", 0.12f }, { "jitterMacro", 0.10f }, { "mode", 0.0f }, { "damageShape", 5.0f }, { "macroLink", 1.0f } } },
    { "Light Radial Scratch", { { "clarity", 0.72f }, { "damage", 0.38f }, { "tracking", 0.16f }, { "jitterMacro", 0.12f }, { "mode", 2.0f }, { "damageShape", 0.0f }, { "macroLink", 1.0f } } },
    { "Scratched Hook", { { "clarity", 0.34f }, { "damage", 0.72f }, { "tracking", 0.66f }, { "jitterMacro", 0.28f }, { "mode", 3.0f }, { "damageShape", 0.0f }, { "macroLink", 1.0f } } },
    { "Portable Skip", { { "clarity", 0.48f }, { "damage", 0.30f }, { "tracking", 0.86f }, { "jitterMacro", 0.20f }, { "mode", 3.0f }, { "damageShape", 2.0f }, { "servoNoise", 0.24f }, { "macroLink", 1.0f } } },
    { "Cheap Discman", { { "clarity", 0.42f }, { "damage", 0.34f }, { "tracking", 0.58f }, { "jitterMacro", 0.48f }, { "mode", 4.0f }, { "damageShape", 5.0f }, { "stereoWidth", 0.82f }, { "macroLink", 1.0f } } },
    { "Car CD Player", { { "clarity", 0.62f }, { "damage", 0.18f }, { "tracking", 0.42f }, { "jitterMacro", 0.16f }, { "mode", 2.0f }, { "damageShape", 0.0f }, { "carComp", 0.82f }, { "stereoWidth", 1.08f }, { "macroLink", 1.0f } } },
    { "Stuck Chorus", { { "clarity", 0.28f }, { "damage", 0.58f }, { "tracking", 0.92f }, { "jitterMacro", 0.16f }, { "mode", 3.0f }, { "damageShape", 3.0f }, { "macroLink", 1.0f } } },
    { "Square Sector Loss", { { "clarity", 0.26f }, { "damage", 0.74f }, { "tracking", 0.38f }, { "jitterMacro", 0.34f }, { "mode", 1.0f }, { "damageShape", 3.0f }, { "macroLink", 1.0f } } },
    { "Rotted Rip", { { "clarity", 0.12f }, { "damage", 0.82f }, { "tracking", 0.64f }, { "jitterMacro", 0.74f }, { "mode", 4.0f }, { "damageShape", 5.0f }, { "hfLoss", 0.26f }, { "stereoLink", 0.76f }, { "macroLink", 1.0f } } },
    { "Random Pit Storm", { { "clarity", 0.08f }, { "damage", 0.96f }, { "tracking", 0.80f }, { "jitterMacro", 0.58f }, { "mode", 4.0f }, { "damageShape", 5.0f }, { "servoNoise", 0.44f }, { "softClip", 1.0f }, { "macroLink", 1.0f } } },
    { "Fingerprint Smear", { { "clarity", 0.54f }, { "damage", 0.44f }, { "tracking", 0.24f }, { "jitterMacro", 0.08f }, { "mode", 2.0f }, { "damageShape", 1.0f }, { "stereoLink", 0.92f }, { "macroLink", 1.0f } } },
    { "Loose Spindle", { { "clarity", 0.58f }, { "damage", 0.18f }, { "tracking", 0.46f }, { "jitterMacro", 0.88f }, { "mode", 0.0f }, { "damageShape", 2.0f }, { "stereoWidth", 1.12f }, { "macroLink", 1.0f } } },
    { "Audio CD-R", { { "clarity", 0.76f }, { "damage", 0.22f }, { "tracking", 0.18f }, { "jitterMacro", 0.12f }, { "mode", 4.0f }, { "damageShape", 4.0f }, { "carComp", 0.12f }, { "macroLink", 1.0f } } },
    { "Unreadable Edge", { { "clarity", 0.04f }, { "damage", 0.92f }, { "tracking", 0.96f }, { "jitterMacro", 0.42f }, { "mode", 3.0f }, { "damageShape", 0.0f }, { "stereoLink", 0.68f }, { "softClip", 1.0f }, { "macroLink", 1.0f } } },
    { "PLAY - Guitar Hook Skip", { { "clarity", .72f }, { "damage", .16f }, { "tracking", .18f }, { "jitterMacro", .06f }, { "mode", 3 }, { "macroLink", 1 }, { "tempoSync", 1 }, { "syncDivision", 2 }, { "syncTarget", 1 }, { "syncStrength", .52f }, { "syncProbability", .26f }, { "skipSliceDivision", 5 }, { "skipLengthDivision", 3 }, { "skipRetrigger", 0 }, { "mix", .82f }, { "ceiling", .90f }, { "outGain", .94f } } },
    { "PLAY - Drum Fill Stutter", { { "clarity", .78f }, { "damage", .10f }, { "tracking", .12f }, { "jitterMacro", .04f }, { "mode", 3 }, { "macroLink", 1 }, { "tempoSync", 1 }, { "syncDivision", 3 }, { "syncTarget", 1 }, { "syncStrength", .44f }, { "syncProbability", .22f }, { "skipSliceDivision", 5 }, { "skipLengthDivision", 4 }, { "skipRetrigger", 0 }, { "mix", .76f }, { "ceiling", .90f }, { "outGain", .96f } } },
    { "PLAY - Synth Disc Pulse", { { "clarity", .68f }, { "damage", .20f }, { "tracking", .16f }, { "jitterMacro", .08f }, { "mode", 2 }, { "damageShape", 0 }, { "macroLink", 1 }, { "tempoSync", 1 }, { "syncDivision", 3 }, { "syncTarget", 0 }, { "syncStrength", .38f }, { "syncProbability", .30f }, { "mix", .72f }, { "ceiling", .90f }, { "outGain", .94f } } },
    { "PLAY - Sparse Disc Errors", { { "clarity", .82f }, { "damage", .12f }, { "tracking", .08f }, { "jitterMacro", .04f }, { "mode", 4 }, { "damageShape", 5 }, { "macroLink", 1 }, { "tempoSync", 1 }, { "syncDivision", 2 }, { "syncTarget", 2 }, { "syncStrength", .34f }, { "syncProbability", .16f }, { "skipSliceDivision", 5 }, { "skipLengthDivision", 3 }, { "mix", .84f }, { "ceiling", .92f }, { "outGain", .96f } } },
};
}

CDEngineAudioProcessorEditor::DeckLookAndFeel::DeckLookAndFeel()
{
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(deep));
    setColour(juce::PopupMenu::textColourId, juce::Colour(bone));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff463045));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(ink));
    setColour(juce::ComboBox::textColourId, juce::Colour(bone));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(line));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(ink));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(amber));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(line));
}

void CDEngineAudioProcessorEditor::DeckLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height, float position, float start, float end, juce::Slider&)
{
    const auto diameter = (float) juce::jmin(width, height) - 11.0f;
    const auto bounds = juce::Rectangle<float>((float) x + ((float) width - diameter) * 0.5f,
                                                (float) y + 4.0f, diameter, diameter);
    const auto centre = bounds.getCentre();
    const auto radius = bounds.getWidth() * 0.5f;
    const auto angle = start + position * (end - start);
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f, start, end, true);
    g.setColour(juce::Colour(0xff4b4c47));
    g.strokePath(track, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path valuePath;
    valuePath.addCentredArc(centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f, start, angle, true);
    g.setColour(juce::Colour(amber));
    g.strokePath(valuePath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xff252624));
    g.fillEllipse(bounds.reduced(8.0f));
    g.setColour(juce::Colour(0xff77746a));
    g.drawEllipse(bounds.reduced(8.0f), 1.0f);
    juce::Path pointer;
    pointer.addRoundedRectangle(-2.0f, -radius * 0.62f, 4.0f, radius * 0.42f, 2.0f);
    g.setColour(juce::Colour(bone));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void CDEngineAudioProcessorEditor::DeckLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto fill = button.getToggleState() ? juce::Colour(0xff3d2b3a) : juce::Colour(panelLift);
    if (highlighted) fill = fill.brighter(0.08f);
    if (down) fill = fill.darker(0.15f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(button.getToggleState() ? juce::Colour(magenta) : juce::Colour(line));
    g.drawRoundedRectangle(bounds, 4.0f, button.getToggleState() ? 1.5f : 1.0f);
}

void CDEngineAudioProcessorEditor::DeckLookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& button, bool, bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    const auto toggle = juce::Rectangle<float>(bounds.getX() + 3.0f, bounds.getCentreY() - 8.0f, 34.0f, 16.0f);
    g.setColour(juce::Colour(button.getToggleState() ? 0xff275458 : 0xff292a27));
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

void CDEngineAudioProcessorEditor::DeckLookAndFeel::drawComboBox(
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

juce::Font CDEngineAudioProcessorEditor::DeckLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return uiFont(12.0f, true);
}

void CDEngineAudioProcessorEditor::Panel::paint(juce::Graphics& g)
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

juce::Rectangle<int> CDEngineAudioProcessorEditor::Panel::contentBounds() const
{
    return getLocalBounds().withTrimmedTop(28).reduced(8, 5);
}

void CDEngineAudioProcessorEditor::DiscDisplay::setState(
    float leftIn, float rightIn, float leftOut, float rightOut, float resolvedDiscPhase,
    float damage, float stereoLink, bool damaged, bool skipping,
    float resolvedDamageProgress, float resolvedSkipProgress, float servoActivity)
{
    const std::array<float, 2> nextInput { leftIn, rightIn };
    const std::array<float, 2> nextOutput { leftOut, rightOut };
    for (int channel = 0; channel < 2; ++channel)
    {
        input[(std::size_t) channel] += (nextInput[(std::size_t) channel] - input[(std::size_t) channel]) * 0.24f;
        output[(std::size_t) channel] += (nextOutput[(std::size_t) channel] - output[(std::size_t) channel]) * 0.24f;
    }
    phase = resolvedDiscPhase;
    damageAmount = damage;
    linkAmount = stereoLink;
    damageActive = damaged;
    skipActive = skipping;
    damageEventProgress = resolvedDamageProgress;
    skipEventProgress = resolvedSkipProgress;
    servo = servoActivity;
    repaint();
}

void CDEngineAudioProcessorEditor::DiscDisplay::paint(juce::Graphics& g)
{
    auto outer = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xffc9c1ae));
    g.fillRoundedRectangle(outer, 10.0f);
    g.setColour(juce::Colour(0xff716c61));
    g.drawRoundedRectangle(outer.reduced(0.5f), 10.0f, 1.2f);
    g.setColour(juce::Colour(ink));
    g.setFont(uiFont(11.0f, true));
    g.drawText("B&E DIGITAL // OPTICAL TRANSPORT", outer.toNearestInt().reduced(16).removeFromTop(20), juce::Justification::centredLeft);

    auto work = outer.reduced(18.0f).withTrimmedTop(24.0f).withTrimmedBottom(94.0f);
    const auto side = juce::jmin(work.getWidth(), work.getHeight()) * 0.86f;
    auto disc = juce::Rectangle<float>(work.getX() + 8.0f, work.getCentreY() - side * 0.5f, side, side);
    juce::ColourGradient rainbow(juce::Colour(0xffe8eadf), disc.getX(), disc.getY(),
                                 juce::Colour(0xff8fc8c8), disc.getRight(), disc.getBottom(), false);
    rainbow.addColour(0.42, juce::Colour(0xffd59acb));
    rainbow.addColour(0.68, juce::Colour(0xffe9d27f));
    g.setGradientFill(rainbow);
    g.fillEllipse(disc);
    g.setColour(juce::Colour(0x55393a37));
    for (int ring = 1; ring < 15; ++ring) g.drawEllipse(disc.reduced((float) ring * side * 0.027f), 0.6f);
    g.setColour(juce::Colour(0xffb9b4a7));
    g.fillEllipse(disc.withSizeKeepingCentre(side * 0.24f, side * 0.24f));
    g.setColour(juce::Colour(ink));
    g.fillEllipse(disc.withSizeKeepingCentre(side * 0.085f, side * 0.085f));
    const auto angle = phase * juce::MathConstants<float>::twoPi;
    const auto scratchAlpha = 0.22f + damageAmount * 0.68f;
    g.setColour(juce::Colour(damageActive ? danger : magenta).withAlpha(scratchAlpha));
    for (int mark = 0; mark < 4; ++mark)
    {
        const auto markAngle = angle + (float) mark * 1.39f;
        const auto radius = side * (0.27f + 0.055f * mark);
        g.drawLine(disc.getCentreX() + std::cos(markAngle) * radius,
                   disc.getCentreY() + std::sin(markAngle) * radius,
                   disc.getCentreX() + std::cos(markAngle + 0.20f) * (radius + side * 0.09f),
                   disc.getCentreY() + std::sin(markAngle + 0.20f) * (radius + side * 0.09f), 1.4f);
    }

    auto armBase = juce::Rectangle<float>(outer.getRight() - 92.0f, work.getY() + 30.0f, 54.0f, 54.0f);
    g.setColour(juce::Colour(0xff343632));
    g.fillEllipse(armBase);
    g.setColour(juce::Colour(0xff6f716a));
    g.drawEllipse(armBase, 1.2f);
    const auto target = disc.getCentre() + juce::Point<float>(side * 0.30f, side * 0.08f);
    g.setColour(juce::Colour(0xff555851));
    g.drawLine(armBase.getCentreX(), armBase.getCentreY(), target.x, target.y, 8.0f);
    g.setColour(juce::Colour(0xffb9b5a9));
    g.drawLine(armBase.getCentreX(), armBase.getCentreY(), target.x, target.y, 3.0f);
    g.setColour(juce::Colour(skipActive ? danger : cyan));
    g.fillEllipse(target.x - 5.0f, target.y - 5.0f, 10.0f, 10.0f);

    auto footer = outer.reduced(16.0f).removeFromBottom(78.0f);
    auto meters = footer.removeFromLeft(footer.getWidth() * 0.60f);
    const char* names[] { "IN L", "IN R", "OUT L", "OUT R" };
    const float values[] { input[0], input[1], output[0], output[1] };
    for (int index = 0; index < 4; ++index)
    {
        auto row = meters.removeFromTop(17.0f);
        g.setColour(juce::Colour(ink));
        g.setFont(uiFont(8.5f, true));
        g.drawText(names[index], row.removeFromLeft(38).toNearestInt(), juce::Justification::centredLeft);
        auto bar = row.reduced(2.0f, 4.0f);
        g.setColour(juce::Colour(0xff484a45));
        g.fillRect(bar);
        g.setColour(juce::Colour(index < 2 ? cyan : amber));
        g.fillRect(bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, values[index] * 2.4f)));
    }
    auto lamps = footer.withTrimmedLeft(15.0f);
    const struct { const char* name; bool on; std::uint32_t colour; } states[] {
        { "READ", input[0] + input[1] > 0.006f, cyan },
        { "CORRECT", !damageActive && damageAmount > 0.04f, amber },
        { "CONCEAL", damageActive, magenta },
        { "SKIP", skipActive, danger },
        { "SERVO", servo > 0.01f, cyan },
    };
    for (const auto& state : states)
    {
        auto row = lamps.removeFromTop(14.0f);
        g.setColour(juce::Colour(state.on ? state.colour : 0xff706d65));
        g.fillEllipse(row.getX(), row.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour(juce::Colour(ink));
        g.setFont(uiFont(8.5f, true));
        g.drawText(state.name, row.toNearestInt().withTrimmedLeft(12), juce::Justification::centredLeft);
    }
    auto eventBars = juce::Rectangle<float>(outer.getX() + 18.0f, footer.getY() - 20.0f,
                                             outer.getWidth() - 36.0f, 16.0f);
    const struct { const char* name; float value; std::uint32_t colour; } events[] {
        { "DAMAGE", damageEventProgress, magenta }, { "SKIP", skipEventProgress, danger }
    };
    for (const auto& event : events)
    {
        auto barArea = eventBars.removeFromLeft(eventBars.getWidth() / 2).reduced(4, 2);
        g.setColour(juce::Colour(ink));
        g.fillRect(barArea);
        g.setColour(juce::Colour(event.colour));
        g.fillRect(barArea.withWidth(barArea.getWidth() * juce::jlimit(0.0f, 1.0f, event.value)));
        g.setColour(juce::Colour(bone));
        g.setFont(uiFont(7.5f, true));
        g.drawText(event.name, barArea.toNearestInt(), juce::Justification::centred);
    }
}

CDEngineAudioProcessorEditor::Knob::Knob(
    APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(bone));
    label.setFont(uiFont(10.0f, true));
    addAndMakeVisible(label);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    slider.onDragStart = std::move(changed);
    addAndMakeVisible(slider);
    attachment = std::make_unique<APVTS::SliderAttachment>(state, id, slider);
}

void CDEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    slider.setBounds(area.reduced(1));
}

void CDEngineAudioProcessorEditor::Knob::setHint(const juce::String& hint)
{
    label.setTooltip(hint);
    slider.setTooltip(hint);
}

CDEngineAudioProcessorEditor::Switch::Switch(
    APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    button.setButtonText(text);
    button.setColour(juce::ToggleButton::textColourId, juce::Colour(bone));
    button.onClick = std::move(changed);
    addAndMakeVisible(button);
    attachment = std::make_unique<APVTS::ButtonAttachment>(state, id, button);
}

void CDEngineAudioProcessorEditor::Switch::resized() { button.setBounds(getLocalBounds()); }
void CDEngineAudioProcessorEditor::Switch::setHint(const juce::String& hint) { button.setTooltip(hint); }

CDEngineAudioProcessorEditor::Choice::Choice(
    APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(bone));
    label.setFont(uiFont(10.0f, true));
    addAndMakeVisible(label);
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id)))
        for (int index = 0; index < parameter->choices.size(); ++index) combo.addItem(parameter->choices[index], index + 1);
    combo.onChange = std::move(changed);
    addAndMakeVisible(combo);
    attachment = std::make_unique<APVTS::ComboBoxAttachment>(state, id, combo);
}

void CDEngineAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(17));
    combo.setBounds(area.removeFromTop(32).reduced(1));
}

void CDEngineAudioProcessorEditor::Choice::setHint(const juce::String& hint)
{
    label.setTooltip(hint);
    combo.setTooltip(hint);
}

CDEngineAudioProcessorEditor::CDEngineAudioProcessorEditor(CDEngineAudioProcessor& owner)
    : AudioProcessorEditor(&owner), processor(owner), apvts(owner.getAPVTS())
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);

    brandLabel.setText("B&E DIGITAL", juce::dontSendNotification);
    brandLabel.setColour(juce::Label::textColourId, juce::Colour(cyan));
    brandLabel.setFont(uiFont(10.0f, true));
    addAndMakeVisible(brandLabel);
    titleLabel.setText("CD ENGINE", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(bone));
    titleLabel.setFont(uiFont(27.0f, true));
    addAndMakeVisible(titleLabel);
    subtitleLabel.setText("OPTICAL READ FAILURE / V2 STEREO", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(dimBone));
    subtitleLabel.setFont(uiFont(10.0f, true));
    addAndMakeVisible(subtitleLabel);
    profileLabel.setText("PROFILE", juce::dontSendNotification);
    profileLabel.setColour(juce::Label::textColourId, juce::Colour(dimBone));
    profileLabel.setFont(uiFont(9.0f, true));
    addAndMakeVisible(profileLabel);
    presetBox.addItem("Custom", 1);
    for (int index = 0; index < (int) std::size(presets); ++index) presetBox.addItem(presets[index].name, index + 2);
    const auto restoredPreset = apvts.state.getProperty("factoryPresetName", "Custom").toString();
    auto restoredPresetId = 1;
    for (int index = 0; index < (int) std::size(presets); ++index) if (restoredPreset == presets[index].name) restoredPresetId = index + 2;
    presetBox.setSelectedId(restoredPresetId, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() >= 2) applyPreset(presetBox.getSelectedId() - 2);
    };
    addAndMakeVisible(presetBox);

    for (auto* button : { &simpleButton, &advancedButton, &performerButton })
    {
        button->setClickingTogglesState(false);
        button->setColour(juce::TextButton::textColourOffId, juce::Colour(bone));
        addAndMakeVisible(*button);
    }
    simpleButton.onClick = [this] { showMode(EditorMode::simple); };
    advancedButton.onClick = [this] { showMode(EditorMode::advanced); };
    performerButton.onClick = [this] { showMode(EditorMode::performer); };
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(dimBone));
    statusLabel.setFont(uiFont(9.0f, true));
    statusLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(statusLabel);

    addAndMakeVisible(discDisplay);
    addAndMakeVisible(simplePage);
    addAndMakeVisible(advancedPage);
    addAndMakeVisible(performerPage);
    for (auto* panelComponent : { &characterPanel, &deckPanel }) simplePage.addAndMakeVisible(*panelComponent);
    for (auto* panelComponent : { &decoderPanel, &burstPanel, &trackingPanel, &mechanicsPanel, &stereoPanel, &protectionPanel })
        advancedPage.addAndMakeVisible(*panelComponent);
    for (auto* panelComponent : { &performanceEventPanel, &performanceClockPanel, &performanceTrackingPanel, &performanceOutputPanel })
        performerPage.addAndMakeVisible(*panelComponent);
    for (auto* button : { &damageButton, &skipButton })
    {
        button->setColour(juce::TextButton::textColourOffId, juce::Colour(bone));
        button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b2427));
        addAndMakeVisible(*button);
    }
    damageButton.onClick = [this] { processor.triggerDamage(getParameter("syncStrength")); };
    skipButton.onClick = [this] { processor.triggerSkip(getParameter("syncStrength")); };
    damageButton.setTooltip("Trigger a sector failure at the current FORCE setting.");
    skipButton.setTooltip("Trigger an authentic skip, or the synchronized slice/hold pattern when HOST CLOCK is enabled.");

    addChoice(characterPanel, "mode", "CONCEALMENT", "Choose how missing sectors are reconstructed.");
    addChoice(characterPanel, "damageShape", "DAMAGE SHAPE", "Radial scratches recur each revolution; other shapes alter the sector-loss geometry.");
    addKnob(characterPanel, "errorRate", "FAULT CHANCE", "Rate of uncorrelated sector read failures.");
    addKnob(characterPanel, "scratchAmt", "FAULT DEPTH", "Severity of caught scratches and pits.");
    addKnob(characterPanel, "trackingRate", "SKIP CHANCE", "Rate of tracking-loss repeats.");
    addKnob(characterPanel, "repeatMs", "LOOP SIZE", "Length of repeated history.");

    addKnob(deckPanel, "carComp", "CAR COMP", "Three-band car-stereo levelling and density.");
    addKnob(deckPanel, "correction", "CORRECTION", "Chance that the decoder recovers damage before concealment.");
    addKnob(deckPanel, "servoNoise", "SERVO", "Audible pickup and spindle search mechanism.");
    addKnob(deckPanel, "inputGain", "INPUT dB", "Input trim before the optical transport.");
    addKnob(deckPanel, "mix", "MIX", "Latency-aligned dry/wet blend.");
    addKnob(deckPanel, "outGain", "OUTPUT", "Final deck output trim.");

    addKnob(decoderPanel, "correction", "CORRECTION", "Chance that the decoder recovers a damaged sector before concealment.", true);
    addKnob(decoderPanel, "interpolationMs", "INTERPOLATE", "Short repair window before a terminal concealment strategy.", true);
    addKnob(burstPanel, "errorRate", "ERROR RATE", "Uncorrelated sector read failure rate.", true);
    addKnob(burstPanel, "burstMs", "BURST ms", "Length of terminal bad-read bursts.", true);
    addKnob(burstPanel, "scratchRate", "SCRATCH RATE", "Probability that the selected damage geometry catches.", true);
    addKnob(burstPanel, "scratchAmt", "SCRATCH AMT", "Severity of caught scratches and pits.", true);
    addKnob(trackingPanel, "trackingRate", "LOSS RATE", "Probability of a tracking-loss repeat.", true);
    addKnob(trackingPanel, "trackingMs", "SEEK BACK", "How far the pickup jumps backward.", true);
    addKnob(trackingPanel, "repeatMs", "LOOP SIZE", "Repeated history loop length.", true);
    addKnob(trackingPanel, "servoHunt", "SERVO HUNT", "Pickup search intensity around failures.", true);
    addKnob(mechanicsPanel, "rotationHz", "ROTATION", "Disc revolution speed controlling radial recurrence.", true);
    addKnob(mechanicsPanel, "jitterMs", "JITTER ms", "Sample-time wander depth.", true);
    addKnob(mechanicsPanel, "jitterRate", "JITTER RATE", "Clock-instability modulation rate.", true);
    addKnob(mechanicsPanel, "servoNoise", "SERVO BED", "Audible pickup and spindle search mechanism.", true);
    addSwitch(mechanicsPanel, "tempoSync", "TEMPO SYNC", "Trigger damage or bounded musical skip patterns from the host grid.");
    addChoice(mechanicsPanel, "syncDivision", "TRIGGER GRID", "Straight, triplet, dotted, or bar-length event spacing.");
    addChoice(mechanicsPanel, "syncTarget", "SYNC EVENT", "Trigger damage, skips, or alternate between both.");
    addKnob(mechanicsPanel, "syncStrength", "SYNC FORCE", "Intensity of each grid-triggered event.");
    addKnob(stereoPanel, "stereoLink", "LINK", "Shared error timing with independent channel samples.");
    addKnob(stereoPanel, "stereoWidth", "WIDTH", "Mid/side width after concealment.");
    addKnob(stereoPanel, "mix", "MIX", "Latency-aligned dry/wet balance.");
    addKnob(protectionPanel, "hfLoss", "HF LOSS", "Read-path blur and top-end loss.", true);
    addSwitch(protectionPanel, "softClip", "SOFT CLIP", "Smooth peaks before the safety limiter.");
    addKnob(protectionPanel, "ceiling", "CEILING", "Shared stereo limiter ceiling.", true);
    addKnob(protectionPanel, "outGain", "OUTPUT", "Final gain trim after macro compensation.");

    addChoice(performanceEventPanel, "mode", "EVENT RESULT", "Concealment result used by triggered damage events.");
    addChoice(performanceEventPanel, "damageShape", "DAMAGE SHAPE", "Geometry used by free-running and triggered damage.");
    addKnob(performanceEventPanel, "scratchAmt", "EVENT DEPTH", "Severity of a caught or triggered defect.");
    addKnob(performanceEventPanel, "burstMs", "DAMAGE LENGTH", "Free-time duration of a damage burst.");
    addSwitch(performanceClockPanel, "tempoSync", "HOST CLOCK", "Trigger monophonic events from exact host-grid boundaries without creating a backlog.");
    addChoice(performanceClockPanel, "syncDivision", "TRIGGER GRID", "When a new event is eligible to begin.");
    addChoice(performanceClockPanel, "syncTarget", "EVENT TYPE", "Trigger damage, musical skips, or alternate between both.");
    addKnob(performanceClockPanel, "syncStrength", "FORCE", "Intensity of each clocked event.");
    addKnob(performanceClockPanel, "syncProbability", "PROBABILITY", "Chance that each eligible clock step fires.");
    addKnob(performanceTrackingPanel, "errorRate", "FREE DAMAGE", "Unclocked sector-failure rate.");
    addKnob(performanceTrackingPanel, "trackingRate", "FREE SKIPS", "Unclocked tracking-loss rate.");
    addKnob(performanceTrackingPanel, "trackingMs", "JUMP BACK", "How far the pickup jumps into history.");
    addChoice(performanceTrackingPanel, "skipSliceDivision", "SKIP SLICE", "Musical length of the immediately preceding audio fragment that repeats.");
    addChoice(performanceTrackingPanel, "skipLengthDivision", "SKIP HOLD", "Total musical duration of the bounded repetition event.");
    addChoice(performanceTrackingPanel, "skipRetrigger", "RETRIGGER", "Ignore active is protected; restart active deliberately interrupts the current loop.");
    addKnob(performanceOutputPanel, "correction", "RECOVERY", "Chance of repair before terminal concealment.");
    addKnob(performanceOutputPanel, "servoNoise", "SERVO BED", "Audible pickup-search mechanism.");
    addKnob(performanceOutputPanel, "mix", "MIX", "Latency-aligned dry/wet balance.");
    addKnob(performanceOutputPanel, "outGain", "OUTPUT", "Final deck trim.");
    addKnob(performanceOutputPanel, "ceiling", "CEILING", "Shared stereo safety ceiling.");

    setResizable(true, true);
    setResizeLimits(900, 620, 1600, 1000);
    setSize(1120, 720);
    showMode(EditorMode::simple);
    startTimerHz(30);
}

CDEngineAudioProcessorEditor::~CDEngineAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

CDEngineAudioProcessorEditor::Knob* CDEngineAudioProcessorEditor::addKnob(
    Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint, bool linked)
{
    auto control = std::make_unique<Knob>(apvts, id, text, [this, linked]
    {
        if (linked) processor.materialiseLegacyMacros();
        markCustom();
    });
    control->setHint(hint);
    auto* raw = control.get();
    owner.addAndMakeVisible(*raw);
    panelItems[&owner].push_back(raw);
    if (linked) linkedControls.push_back(raw);
    knobs.push_back(std::move(control));
    return raw;
}

CDEngineAudioProcessorEditor::Switch* CDEngineAudioProcessorEditor::addSwitch(
    Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint, bool linked)
{
    auto control = std::make_unique<Switch>(apvts, id, text, [this] { markCustom(); });
    control->setHint(hint);
    auto* raw = control.get();
    owner.addAndMakeVisible(*raw);
    panelItems[&owner].push_back(raw);
    if (linked) linkedControls.push_back(raw);
    switches.push_back(std::move(control));
    return raw;
}

CDEngineAudioProcessorEditor::Choice* CDEngineAudioProcessorEditor::addChoice(
    Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint, bool linked)
{
    auto control = std::make_unique<Choice>(apvts, id, text, [this] { markCustom(); });
    control->setHint(hint);
    auto* raw = control.get();
    owner.addAndMakeVisible(*raw);
    panelItems[&owner].push_back(raw);
    if (linked) linkedControls.push_back(raw);
    choices.push_back(std::move(control));
    return raw;
}

void CDEngineAudioProcessorEditor::layoutPanel(Panel& owner, int columns)
{
    auto area = owner.contentBounds();
    auto& items = panelItems[&owner];
    if (items.empty()) return;
    const auto cols = juce::jmax(1, columns);
    const auto rows = juce::jmax(1, ((int) items.size() + cols - 1) / cols);
    const auto cellWidth = area.getWidth() / cols;
    const auto cellHeight = area.getHeight() / rows;
    for (int index = 0; index < (int) items.size(); ++index)
    {
        const auto row = index / cols;
        const auto column = index % cols;
        items[(std::size_t) index]->setBounds(area.getX() + column * cellWidth, area.getY() + row * cellHeight,
                                             cellWidth, cellHeight);
    }
}

void CDEngineAudioProcessorEditor::showMode(EditorMode mode)
{
    editorMode = mode;
    simplePage.setVisible(editorMode == EditorMode::simple);
    advancedPage.setVisible(editorMode == EditorMode::advanced);
    performerPage.setVisible(editorMode == EditorMode::performer);
    simpleButton.setToggleState(editorMode == EditorMode::simple, juce::dontSendNotification);
    advancedButton.setToggleState(editorMode == EditorMode::advanced, juce::dontSendNotification);
    performerButton.setToggleState(editorMode == EditorMode::performer, juce::dontSendNotification);
    resized();
}

void CDEngineAudioProcessorEditor::setParameter(const juce::String& id, float plainValue)
{
    if (auto* parameter = apvts.getParameter(id)) parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

float CDEngineAudioProcessorEditor::getParameter(const juce::String& id) const
{
    if (const auto* parameter = apvts.getRawParameterValue(id)) return parameter->load();
    return 0.0f;
}

void CDEngineAudioProcessorEditor::resetParameters()
{
    for (auto* parameter : processor.getParameters()) parameter->setValueNotifyingHost(parameter->getDefaultValue());
}

void CDEngineAudioProcessorEditor::applyPreset(int index)
{
    if (index < 0 || index >= (int) std::size(presets)) return;
    suppressPresetChanges = true;
    resetParameters();
    // The legacy macro path treated 0.98 as unity for its output compensation.
    // Resolve old factory recipes from that exact baseline, then store the
    // resulting canonical output value.
    setParameter("outGain", 0.98f);
    for (const auto& setting : presets[index].values) if (juce::String(setting.first) != "macroLink") setParameter(setting.first, setting.second);
    setParameter("macroLink", 1.0f);
    processor.materialiseLegacyMacros();
    for (const auto& setting : presets[index].values)
    {
        const auto id = juce::String(setting.first);
        if (id != "clarity" && id != "damage" && id != "tracking" && id != "jitterMacro" && id != "macroLink")
            setParameter(setting.first, setting.second);
    }
    setParameter("macroLink", 0.0f);
    apvts.state.setProperty("factoryPresetName", presets[index].name, nullptr);
    suppressPresetChanges = false;
}

void CDEngineAudioProcessorEditor::markCustom()
{
    if (!suppressPresetChanges)
    {
        presetBox.setSelectedId(1, juce::dontSendNotification);
        apvts.state.setProperty("factoryPresetName", "Custom", nullptr);
    }
}

void CDEngineAudioProcessorEditor::timerCallback()
{
    for (auto* control : linkedControls)
    {
        control->setEnabled(true);
        control->setAlpha(1.0f);
    }
    discDisplay.setState(processor.inputPeak(0), processor.inputPeak(1), processor.outputPeak(0), processor.outputPeak(1),
                         processor.discPhase(), getParameter("scratchAmt"), getParameter("stereoLink"),
                         processor.damageActive(), processor.skipActive(), processor.damageProgress(),
                         processor.skipProgress(), processor.servoActivity());
    if (processor.skipActive() && getParameter("tempoSync") > 0.5f) statusLabel.setText("MUSICAL SKIP / BOUNDED HISTORY LOOP", juce::dontSendNotification);
    else if (processor.skipActive()) statusLabel.setText("TRACKING LOST / AUTHENTIC HISTORY REPEAT", juce::dontSendNotification);
    else if (processor.damageActive()) statusLabel.setText("SECTOR FAILURE / CONCEALMENT ACTIVE", juce::dontSendNotification);
    else if (getParameter("tempoSync") > 0.5f) statusLabel.setText("TEMPO SYNC ARMED / HOST GRID EVENTS", juce::dontSendNotification);
    else if (processor.legacyMacrosActive()) statusLabel.setText("LEGACY MACRO PATCH / SOUND PRESERVED", juce::dontSendNotification);
    else statusLabel.setText("CANONICAL PARAMETERS / 120 SAMPLE STEREO PATH", juce::dontSendNotification);
}

void CDEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ink));
    auto bounds = getLocalBounds().toFloat().reduced(7.0f);
    g.setColour(juce::Colour(deep));
    g.fillRoundedRectangle(bounds, 7.0f);
    g.setColour(juce::Colour(line));
    g.drawRoundedRectangle(bounds, 7.0f, 1.0f);
    g.setColour(juce::Colour(cyan));
    g.fillRect(16.0f, 7.0f, 190.0f, 2.0f);
    g.setColour(juce::Colour(magenta));
    g.fillRect((float) getWidth() - 206.0f, 7.0f, 190.0f, 2.0f);
    g.setColour(juce::Colour(0xff383a35));
    g.fillRect(15, 91, getWidth() - 30, 1);
}

void CDEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto header = area.removeFromTop(86);
    const auto profileWidth = juce::jlimit(205, 270, (int) std::round(header.getWidth() * 0.24f));
    auto profile = header.removeFromRight(profileWidth);
    header.removeFromRight(12);
    auto navColumn = header.removeFromRight(300);
    auto identity = header;
    brandLabel.setBounds(identity.removeFromTop(17));
    titleLabel.setBounds(identity.removeFromTop(36));
    subtitleLabel.setBounds(identity.removeFromTop(18));
    profileLabel.setBounds(profile.removeFromTop(17));
    presetBox.setBounds(profile.removeFromTop(32));
    auto nav = navColumn.removeFromBottom(35);
    const auto modeWidth = (nav.getWidth() - 12) / 3;
    simpleButton.setBounds(nav.removeFromLeft(modeWidth));
    nav.removeFromLeft(6);
    advancedButton.setBounds(nav.removeFromLeft(modeWidth));
    nav.removeFromLeft(6);
    performerButton.setBounds(nav);
    statusLabel.setBounds(navColumn);

    auto triggerRow = area.removeFromBottom(48);
    damageButton.setBounds(triggerRow.removeFromLeft(triggerRow.getWidth() / 2).reduced(6, 5));
    skipButton.setBounds(triggerRow.reduced(6, 5));
    auto displayArea = area.removeFromLeft((int) std::round(area.getWidth() * 0.35f)).reduced(4, 4);
    discDisplay.setBounds(displayArea);
    auto controls = area.reduced(4, 4);
    simplePage.setBounds(controls);
    advancedPage.setBounds(controls);
    performerPage.setBounds(controls);

    if (editorMode == EditorMode::simple)
    {
        auto page = simplePage.getLocalBounds();
        characterPanel.setBounds(page.removeFromTop(page.getHeight() / 2).reduced(3));
        deckPanel.setBounds(page.reduced(3));
        layoutPanel(characterPanel, 3);
        layoutPanel(deckPanel, 3);
    }
    else if (editorMode == EditorMode::advanced)
    {
        const auto page = advancedPage.getLocalBounds().reduced(3);
        const auto columnWidth = page.getWidth() / 3;
        const auto rowHeight = page.getHeight() / 2;
        Panel* panels[] { &decoderPanel, &burstPanel, &trackingPanel, &mechanicsPanel, &stereoPanel, &protectionPanel };
        for (int index = 0; index < 6; ++index)
            panels[index]->setBounds(juce::Rectangle<int>(page.getX() + (index % 3) * columnWidth,
                                                           page.getY() + (index / 3) * rowHeight,
                                                           columnWidth, rowHeight).reduced(4));
        layoutPanel(decoderPanel, 2);
        layoutPanel(burstPanel, 2);
        layoutPanel(trackingPanel, 2);
        layoutPanel(mechanicsPanel, 2);
        layoutPanel(stereoPanel, 2);
        layoutPanel(protectionPanel, 2);
    }
    else
    {
        const auto page = performerPage.getLocalBounds().reduced(3);
        const auto columnWidth = page.getWidth() / 2;
        const auto rowHeight = page.getHeight() / 2;
        Panel* panels[] { &performanceEventPanel, &performanceClockPanel,
                          &performanceTrackingPanel, &performanceOutputPanel };
        for (int index = 0; index < 4; ++index)
            panels[index]->setBounds(juce::Rectangle<int>(page.getX() + (index % 2) * columnWidth,
                                                           page.getY() + (index / 2) * rowHeight,
                                                           columnWidth, rowHeight).reduced(4));
        layoutPanel(performanceEventPanel, 2);
        layoutPanel(performanceClockPanel, 2);
        layoutPanel(performanceTrackingPanel, 2);
        layoutPanel(performanceOutputPanel, 3);
    }
}
