#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr auto ink = 0xff171512;
constexpr auto deepInk = 0xff090b0b;
constexpr auto panel = 0xff211f1b;
constexpr auto panelLift = 0xff2b2822;
constexpr auto bone = 0xffe8ddc8;
constexpr auto dimBone = 0xffa89f8f;
constexpr auto amber = 0xffffad32;
constexpr auto hotAmber = 0xffffcf65;
constexpr auto cyan = 0xff55e8ee;
constexpr auto magenta = 0xffff4ecb;
constexpr auto oxblood = 0xff521f25;

struct PresetDef
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;
};

static const PresetDef kPresets[] = {
    { "Fresh Cassette", {
        { "quality", 0.72f }, { "age", 0.18f }, { "wow", 0.14f }, { "glitch", 0.06f },
        { "sfxEnable", 1.0f }, { "sfxBank", 0.0f }, { "sfxMode", 0.0f }, { "sfxLevel", 0.12f }
    } },
    { "Worn Cassette", {
        { "quality", 0.32f }, { "age", 0.62f }, { "wow", 0.55f }, { "glitch", 0.35f },
        { "sfxEnable", 1.0f }, { "sfxBank", 0.0f }, { "sfxMode", 1.0f }, { "sfxLevel", 0.20f }
    } },
    { "VHS HiFi", {
        { "quality", 0.78f }, { "age", 0.24f }, { "wow", 0.12f }, { "glitch", 0.08f },
        { "sfxEnable", 1.0f }, { "sfxBank", 1.0f }, { "sfxMode", 0.0f }, { "sfxLevel", 0.11f }
    } },
    { "VHS Linear", {
        { "quality", 0.42f }, { "age", 0.48f }, { "wow", 0.30f }, { "glitch", 0.22f },
        { "sfxEnable", 1.0f }, { "sfxBank", 1.0f }, { "sfxMode", 2.0f }, { "sfxLevel", 0.17f }
    } },
    { "Rewind Melt", {
        { "quality", 0.08f }, { "age", 0.82f }, { "wow", 0.92f }, { "glitch", 0.62f },
        { "speed", 0.93f }, { "sfxEnable", 1.0f }, { "sfxBank", 0.0f }, { "sfxMode", 2.0f }, { "sfxLevel", 0.24f }
    } },
    { "PLAY - Guitar Oxide", { { "quality", .68f }, { "age", .34f }, { "wow", .08f }, { "glitch", .01f }, { "drive", .48f }, { "comp", .32f }, { "hiss", .025f }, { "hum", .008f }, { "sfxEnable", 0.0f }, { "mix", .82f }, { "ceiling", .90f }, { "outGain", .96f } } },
    { "PLAY - Drum Glue Cassette", { { "quality", .62f }, { "age", .42f }, { "wow", .06f }, { "glitch", .03f }, { "drive", .38f }, { "comp", .52f }, { "hiss", .018f }, { "sfxEnable", 0.0f }, { "mix", .68f }, { "ceiling", .88f }, { "outGain", .94f } } },
    { "PLAY - Synth VHS Bloom", { { "quality", .54f }, { "age", .38f }, { "wow", .28f }, { "glitch", .04f }, { "sfxEnable", 1.0f }, { "sfxBank", 1.0f }, { "sfxMode", 0.0f }, { "sfxLevel", .10f }, { "wowTempoSync", 1.0f }, { "wowDivision", 1.0f }, { "mix", .74f }, { "ceiling", .90f }, { "outGain", .94f } } },
    { "PLAY - Clocked Tape Catch", { { "quality", .48f }, { "age", .46f }, { "wow", .12f }, { "glitch", .16f }, { "dropoutTempoSync", 1.0f }, { "dropoutDivision", 3.0f }, { "dropoutProbability", .28f }, { "dropoutStrength", .46f }, { "dropoutLengthSync", 1.0f }, { "dropoutLengthDivision", 5.0f }, { "sfxEnable", 0.0f }, { "mix", .80f }, { "ceiling", .88f }, { "outGain", .92f } } },
};

juce::Font labelFont(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}
}

void TapeEngineAudioProcessorEditor::TapeLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height, float position,
    float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(7.0f);
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + position * (endAngle - startAngle);
    const auto lineWidth = juce::jmax(2.0f, radius * 0.105f);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius - lineWidth, radius - lineWidth, 0.0f,
                        startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff504b42));
    g.strokePath(track, juce::PathStrokeType(lineWidth, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, radius - lineWidth, radius - lineWidth, 0.0f,
                        startAngle, angle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
    g.strokePath(value, juce::PathStrokeType(lineWidth, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    const auto capRadius = radius * 0.66f;
    juce::ColourGradient cap(juce::Colour(0xff403c35), centre.x - capRadius, centre.y - capRadius,
                             juce::Colour(0xff151514), centre.x + capRadius, centre.y + capRadius, false);
    g.setGradientFill(cap);
    g.fillEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);
    g.setColour(juce::Colour(0xff6d6659));
    g.drawEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

    juce::Path pointer;
    const auto pointerLength = capRadius * 0.74f;
    const auto pointerWidth = juce::jmax(2.0f, radius * 0.07f);
    pointer.addRoundedRectangle(-pointerWidth * 0.5f, -pointerLength, pointerWidth, pointerLength, pointerWidth * 0.5f);
    g.setColour(juce::Colour(bone));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void TapeEngineAudioProcessorEditor::TapeLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const auto active = button.getToggleState();
    g.setColour(active ? juce::Colour(cyan).withAlpha(down ? 0.62f : 0.30f)
                       : juce::Colour(highlighted ? panelLift : panel));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(active ? juce::Colour(cyan) : juce::Colour(0xff5b554a));
    g.drawRoundedRectangle(bounds, 4.0f, active ? 1.5f : 1.0f);
}

void TapeEngineAudioProcessorEditor::TapeLookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const auto switchArea = juce::Rectangle<float>(bounds.getX() + 4.0f,
                                                    bounds.getCentreY() - 10.0f,
                                                    38.0f, 20.0f);
    g.setColour(button.getToggleState() ? juce::Colour(cyan).withAlpha(0.34f)
                                        : juce::Colour(highlighted ? panelLift : deepInk));
    g.fillRoundedRectangle(switchArea, switchArea.getHeight() * 0.5f);
    g.setColour(button.getToggleState() ? juce::Colour(cyan) : juce::Colour(0xff6b655b));
    g.drawRoundedRectangle(switchArea, switchArea.getHeight() * 0.5f, 1.0f);
    const auto dot = 14.0f;
    const auto dotX = button.getToggleState() ? switchArea.getRight() - dot - 3.0f : switchArea.getX() + 3.0f;
    g.setColour(button.getToggleState() ? juce::Colour(cyan) : juce::Colour(dimBone));
    g.fillEllipse(dotX, switchArea.getCentreY() - dot * 0.5f, dot, dot);
    g.setColour(juce::Colour(bone));
    g.setFont(labelFont(12.0f, true));
    g.drawFittedText(button.getButtonText(), bounds.toNearestInt().withTrimmedLeft(50),
                     juce::Justification::centredLeft, 1);
}

void TapeEngineAudioProcessorEditor::TapeLookAndFeel::drawComboBox(
    juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
    g.setColour(juce::Colour(deepInk));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(juce::Colour(0xff625b4e));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    juce::Path arrow;
    const auto cx = (float) width - 15.0f;
    const auto cy = (float) height * 0.52f;
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour(juce::Colour(cyan));
    g.fillPath(arrow);
}

juce::Font TapeEngineAudioProcessorEditor::TapeLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return labelFont(13.0f, true);
}

void TapeEngineAudioProcessorEditor::Panel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(panel).withAlpha(0.96f));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(0xff524c42));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    g.setColour(juce::Colour(cyan));
    g.fillRect(juce::Rectangle<float>(14.0f, 13.0f, 20.0f, 2.0f));
    g.setColour(juce::Colour(dimBone));
    g.setFont(labelFont(11.0f, true));
    g.drawText(title, 42, 5, getWidth() - 54, 22, juce::Justification::centredLeft);
}

juce::Rectangle<int> TapeEngineAudioProcessorEditor::Panel::contentBounds() const
{
    return getLocalBounds().withTrimmedTop(31).reduced(8, 6);
}

void TapeEngineAudioProcessorEditor::DeckDisplay::setState(
    std::array<float, 64> waveform, float inputLeft, float inputRight,
    float outputLeft, float outputRight, float modulationMs, float dropoutProgress,
    bool dropoutActive, float compressionValue, float saturationValue, float noiseValue,
    float mechanismValue, float limiterValue, int machineValue)
{
    trace = waveform;
    input = { inputLeft, inputRight };
    output = { outputLeft, outputRight };
    modulation = modulationMs;
    dropout = dropoutProgress;
    dropoutOn = dropoutActive;
    compression = compressionValue;
    saturation = saturationValue;
    noise = noiseValue;
    mechanism = mechanismValue;
    limiter = limiterValue;
    machine = machineValue;
    phase = std::fmod(phase + 0.025f + juce::jmin(0.09f, std::abs(modulationMs) * 0.018f),
                      juce::MathConstants<float>::twoPi);
    repaint();
}

void TapeEngineAudioProcessorEditor::DeckDisplay::paint(juce::Graphics& g)
{
    auto outer = getLocalBounds().toFloat().reduced(1.0f);
    juce::ColourGradient shell(juce::Colour(0xffd8ccb6), outer.getX(), outer.getY(),
                               juce::Colour(0xff9f9584), outer.getRight(), outer.getBottom(), false);
    g.setGradientFill(shell);
    g.fillRoundedRectangle(outer, 10.0f);
    g.setColour(juce::Colour(ink));
    g.drawRoundedRectangle(outer, 10.0f, 2.0f);

    auto plate = outer.reduced(15.0f, 14.0f);
    g.setColour(juce::Colour(0xffbdb29f));
    for (float x = plate.getX(); x < plate.getRight(); x += 5.0f)
        g.drawVerticalLine((int) x, plate.getY(), plate.getBottom());

    g.setColour(juce::Colour(ink));
    g.setFont(labelFont(12.0f, true));
    g.drawText(machine == 0 ? "B&E DIGITAL // CASSETTE TYPE II" : "B&E DIGITAL // VHS LINEAR",
               plate.removeFromTop(24.0f), juce::Justification::centredLeft);

    auto cassette = juce::Rectangle<float>(outer.getX() + outer.getWidth() * 0.12f,
                                            outer.getY() + outer.getHeight() * 0.25f,
                                            outer.getWidth() * 0.76f,
                                            outer.getHeight() * 0.43f);
    g.setColour(dropoutOn ? juce::Colour(0xff7b2c22) : juce::Colour(oxblood));
    g.fillRoundedRectangle(cassette, 8.0f);
    g.setColour(juce::Colour(ink));
    g.drawRoundedRectangle(cassette, 8.0f, 2.0f);

    auto window = cassette.reduced(cassette.getWidth() * 0.12f, cassette.getHeight() * 0.18f);
    g.setColour(juce::Colour(deepInk));
    g.fillRoundedRectangle(window, 5.0f);
    g.setColour(juce::Colour(0xff69434a));
    g.drawRoundedRectangle(window, 5.0f, 1.0f);

    juce::Path waveform;
    for (size_t index = 0; index < trace.size(); ++index)
    {
        const auto x = window.getX() + window.getWidth() * (float) index / (float) (trace.size() - 1);
        const auto y = window.getCentreY() - juce::jlimit(-1.0f, 1.0f, trace[index]) * window.getHeight() * 0.38f;
        if (index == 0) waveform.startNewSubPath(x, y); else waveform.lineTo(x, y);
    }
    g.setColour(juce::Colour(dropoutOn ? hotAmber : cyan).withAlpha(0.48f));
    g.strokePath(waveform, juce::PathStrokeType(dropoutOn ? 2.2f : 1.2f));

    const auto reelRadius = juce::jmin(window.getHeight() * 0.34f, window.getWidth() * 0.11f);
    const auto left = juce::Point<float>(window.getX() + window.getWidth() * 0.25f, window.getCentreY());
    const auto right = juce::Point<float>(window.getRight() - window.getWidth() * 0.25f, window.getCentreY());
    g.setColour(juce::Colour(0xff25211e));
    g.drawLine(left.x, left.y + reelRadius, right.x, right.y + reelRadius, 3.0f);

    const auto drawReel = [&g, this, reelRadius](juce::Point<float> centre, float offset) {
        g.setColour(juce::Colour(bone));
        g.fillEllipse(centre.x - reelRadius, centre.y - reelRadius, reelRadius * 2.0f, reelRadius * 2.0f);
        g.setColour(juce::Colour(ink));
        g.drawEllipse(centre.x - reelRadius, centre.y - reelRadius, reelRadius * 2.0f, reelRadius * 2.0f, 2.0f);
        for (int spoke = 0; spoke < 6; ++spoke)
        {
            const auto a = phase + offset + (float) spoke * juce::MathConstants<float>::twoPi / 6.0f;
            const auto p1 = centre + juce::Point<float>(std::cos(a), std::sin(a)) * (reelRadius * 0.34f);
            const auto p2 = centre + juce::Point<float>(std::cos(a), std::sin(a)) * (reelRadius * 0.78f);
            g.drawLine(p1.x, p1.y, p2.x, p2.y, 2.0f);
        }
        g.fillEllipse(centre.x - 3.0f, centre.y - 3.0f, 6.0f, 6.0f);
    };
    drawReel(left, 0.0f);
    drawReel(right, 0.8f);

    auto meter = juce::Rectangle<float>(cassette.getX() + 14.0f, cassette.getBottom() + 9.0f,
                                        cassette.getWidth() - 28.0f, 8.0f);
    g.setColour(juce::Colour(ink));
    g.fillRoundedRectangle(meter, 4.0f);
    auto lit = meter.reduced(1.0f);
    lit.setWidth(lit.getWidth() * juce::jlimit(0.0f, 1.0f, std::sqrt(0.5f * (output[0] + output[1]))));
    juce::ColourGradient levelGradient(juce::Colour(cyan), lit.getX(), lit.getY(),
                                       juce::Colour(magenta), meter.getRight(), meter.getY(), false);
    g.setGradientFill(levelGradient);
    g.fillRoundedRectangle(lit, 3.0f);

    g.setColour(juce::Colour(ink).withAlpha(0.74f));
    g.setFont(labelFont(10.0f, true));
    g.drawText(dropoutOn ? "DROPOUT / RECOVERING" : "CAPSTAN LOCK",
               (int) cassette.getX(), (int) meter.getBottom() + 2,
               (int) cassette.getWidth(), 16, juce::Justification::centredRight);

    auto telemetry = juce::Rectangle<float>(cassette.getX(), meter.getBottom() + 25.0f,
                                             cassette.getWidth(), outer.getBottom() - meter.getBottom() - 38.0f);
    const char* names[] { "IN", "OUT", "MOD", "DROP", "LEVEL", "SAT", "NOISE", "DECK", "LIMIT" };
    const float values[] {
        juce::jlimit(0.0f, 1.0f, (input[0] + input[1]) * 1.5f),
        juce::jlimit(0.0f, 1.0f, (output[0] + output[1]) * 1.5f),
        juce::jlimit(0.0f, 1.0f, std::abs(modulation) / 12.0f),
        dropoutOn ? juce::jmax(0.08f, dropout) : 0.0f,
        juce::jlimit(0.0f, 1.0f, compression * 2.5f),
        juce::jlimit(0.0f, 1.0f, saturation * 12.0f),
        juce::jlimit(0.0f, 1.0f, noise * 22.0f),
        juce::jlimit(0.0f, 1.0f, mechanism * 4.0f),
        juce::jlimit(0.0f, 1.0f, limiter * 3.0f)
    };
    const auto rowHeight = telemetry.getHeight() / 9.0f;
    for (int index = 0; index < 9; ++index)
    {
        auto row = telemetry.removeFromTop(rowHeight);
        g.setColour(juce::Colour(dimBone));
        g.setFont(labelFont(8.5f, true));
        g.drawText(names[index], row.removeFromLeft(46.0f), juce::Justification::centredLeft);
        auto bar = row.reduced(2.0f, juce::jmax(1.0f, rowHeight * 0.28f));
        g.setColour(juce::Colour(ink));
        g.fillRect(bar);
        g.setColour(juce::Colour(index == 3 ? hotAmber : (index >= 7 ? magenta : cyan)));
        g.fillRect(bar.withWidth(bar.getWidth() * values[index]));
    }
}

TapeEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    label.setText(text.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(labelFont(11.5f, true));
    label.setColour(juce::Label::textColourId, juce::Colour(bone));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.22f,
                               juce::MathConstants<float>::pi * 2.78f, true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(amber));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(deepInk));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(hotAmber));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff5a544a));
    addAndMakeVisible(slider);
    attachment = std::make_unique<APVTS::SliderAttachment>(state, paramID, slider);
}

void TapeEngineAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(21));
    slider.setBounds(area.reduced(2, 0));
}

void TapeEngineAudioProcessorEditor::Knob::setHint(const juce::String& hint)
{
    slider.setTooltip(hint);
    label.setTooltip(hint);
}

TapeEngineAudioProcessorEditor::Switch::Switch(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    button.setButtonText(text.toUpperCase());
    addAndMakeVisible(button);
    attachment = std::make_unique<APVTS::ButtonAttachment>(state, paramID, button);
}

void TapeEngineAudioProcessorEditor::Switch::resized() { button.setBounds(getLocalBounds().reduced(4)); }
void TapeEngineAudioProcessorEditor::Switch::setHint(const juce::String& hint) { button.setTooltip(hint); }

TapeEngineAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& paramID, const juce::String& text)
{
    label.setText(text.toUpperCase(), juce::dontSendNotification);
    label.setFont(labelFont(11.0f, true));
    label.setColour(juce::Label::textColourId, juce::Colour(bone));
    addAndMakeVisible(label);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(paramID)))
        for (int i = 0; i < p->choices.size(); ++i)
            combo.addItem(p->choices[i], i + 1);
    combo.setColour(juce::ComboBox::textColourId, juce::Colour(bone));
    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(deepInk));
    addAndMakeVisible(combo);
    attachment = std::make_unique<APVTS::ComboBoxAttachment>(state, paramID, combo);
}

void TapeEngineAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds().reduced(3);
    label.setBounds(area.removeFromTop(20));
    combo.setBounds(area.removeFromTop(30));
}

void TapeEngineAudioProcessorEditor::Choice::setHint(const juce::String& hint)
{
    combo.setTooltip(hint);
    label.setTooltip(hint);
}

TapeEngineAudioProcessorEditor::TapeEngineAudioProcessorEditor(TapeEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.getAPVTS())
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);

    brand.setText("B&E DIGITAL", juce::dontSendNotification);
    brand.setFont(labelFont(11.0f, true));
    brand.setColour(juce::Label::textColourId, juce::Colour(cyan));
    addAndMakeVisible(brand);

    title.setText("TAPE ENGINE", juce::dontSendNotification);
    title.setFont(labelFont(28.0f, true));
    title.setColour(juce::Label::textColourId, juce::Colour(bone));
    addAndMakeVisible(title);

    subtitle.setText("MAGNETIC SIGNAL WEATHERING / V5 STEREO", juce::dontSendNotification);
    subtitle.setFont(labelFont(10.5f, true));
    subtitle.setColour(juce::Label::textColourId, juce::Colour(dimBone));
    addAndMakeVisible(subtitle);

    presetLabel.setText("PROFILE", juce::dontSendNotification);
    presetLabel.setFont(labelFont(10.5f, true));
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(dimBone));
    addAndMakeVisible(presetLabel);

    presetBox.addItem("Custom", 1);
    for (int i = 0; i < (int) std::size(kPresets); ++i)
        presetBox.addItem(kPresets[i].name, i + 2);
    presetBox.onChange = [this]() {
        if (const auto id = presetBox.getSelectedId(); id >= 2)
            applyPreset(id - 2);
    };
    const auto restoredPreset = apvts.state.getProperty("factoryPresetName", "Custom").toString();
    auto restoredPresetId = 1;
    for (int i = 0; i < (int) std::size(kPresets); ++i) if (restoredPreset == kPresets[i].name) restoredPresetId = i + 2;
    presetBox.setSelectedId(restoredPresetId, juce::dontSendNotification);
    presetBox.setTooltip("Factory tape profiles resolve into the same editable controls used by every view.");
    addAndMakeVisible(presetBox);

    constexpr int viewGroup = 0x4c4145;
    for (auto* button : { &surfaceButton, &advancedButton, &performerButton })
    {
        button->setClickingTogglesState(true);
        button->setRadioGroupId(viewGroup);
        button->setColour(juce::TextButton::textColourOnId, juce::Colour(deepInk));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour(dimBone));
        addAndMakeVisible(*button);
    }
    surfaceButton.onClick = [this]() { showMode(EditorMode::simple); };
    advancedButton.onClick = [this]() { showMode(EditorMode::advanced); };
    performerButton.onClick = [this]() { showMode(EditorMode::performer); };

    statusLabel.setText("12 MS ANALOG PATH  /  SAFE OUTPUT", juce::dontSendNotification);
    statusLabel.setFont(labelFont(10.0f, true));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(dimBone));
    statusLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(statusLabel);

    addAndMakeVisible(deckDisplay);
    addAndMakeVisible(surfacePage);
    addAndMakeVisible(advancedPage);
    addAndMakeVisible(performerPage);
    surfacePage.addAndMakeVisible(simpleCharacterPanel);
    surfacePage.addAndMakeVisible(simpleMotionPanel);

    addKnob(simpleCharacterPanel, "hpHz", "Rumble Cut", "Low-frequency deck and cabinet cutoff.");
    addKnob(simpleCharacterPanel, "lpHz", "Head Limit", "Tape-head high-frequency bandwidth.");
    addKnob(simpleCharacterPanel, "headBumpDb", "Head Bump", "Magnetic-head low-frequency bloom.");
    addKnob(simpleCharacterPanel, "drive", "Oxide Drive", "Gain-compensated magnetic saturation.");
    addKnob(simpleCharacterPanel, "comp", "Tape Leveler", "Tape density and downward compression.");
    addKnob(simpleCharacterPanel, "hiss", "Tape Hiss", "Magnetic tape noise rather than a generic static layer.");

    addKnob(simpleMotionPanel, "speed", "Tape Speed", "Playback speed and transport tilt.");
    addKnob(simpleMotionPanel, "transportDrift", "Transport Drift", "Amount of slow capstan wander and instability.");
    addKnob(simpleMotionPanel, "wowDepthMs", "Wow Depth", "Slow transport displacement in milliseconds.");
    addKnob(simpleMotionPanel, "flutterDepthMs", "Flutter", "Fast transport displacement in milliseconds.");
    addKnob(simpleMotionPanel, "dropout", "Free Dropouts", "Unclocked physical tape losses.");
    addKnob(simpleMotionPanel, "dropoutMs", "Drop Length", "Free and manual dropout duration.");
    addSwitch(simpleMotionPanel, "sfxEnable", "Deck Sound", "Enable the captured cassette or VHS mechanism layer.");
    addKnob(simpleMotionPanel, "sfxLevel", "Mechanism", "Captured deck layer only.");
    addKnob(simpleMotionPanel, "mix", "Mix", "Latency-aligned dry and tape balance.");
    addKnob(simpleMotionPanel, "outGain", "Output", "Final tape output trim.");

    advancedPage.addAndMakeVisible(tonePanel);
    advancedPage.addAndMakeVisible(transportPanel);
    advancedPage.addAndMakeVisible(texturePanel);
    advancedPage.addAndMakeVisible(deckPanel);

    addKnob(tonePanel, "hpHz", "Rumble Cut", "Cuts low-frequency motor and mechanism build-up.");
    addKnob(tonePanel, "lpHz", "Head Limit", "Sets the tape head high-frequency bandwidth.");
    addKnob(tonePanel, "headBumpDb", "Head Bump", "Low-frequency bloom from tape head coupling.");
    addKnob(tonePanel, "headBumpHz", "Bump Tune", "Tunes the head-bump resonant frequency.");
    addKnob(tonePanel, "outGain", "Output", "Final output level after tape coloration.");

    addKnob(transportPanel, "speed", "Tape Speed", "Varispeed-style playback tilt.");
    addKnob(transportPanel, "transportDrift", "Transport Drift", "Free-running capstan wander.");
    addKnob(transportPanel, "wowDepthMs", "Wow Depth", "Slow capstan modulation depth in milliseconds.");
    addKnob(transportPanel, "flutterDepthMs", "Flutter", "Fast transport modulation depth.");
    addKnob(transportPanel, "dropout", "Dropout", "Frequency of physical signal losses.");
    addKnob(transportPanel, "dropoutMs", "Drop Length", "Duration of free and manual dropout events.");

    addKnob(texturePanel, "drive", "Oxide Drive", "Gain-compensated tape saturation.");
    addKnob(texturePanel, "comp", "Leveler", "Tape-style downward compression and density.");
    addKnob(texturePanel, "hiss", "Hiss", "Wideband magnetic tape hiss.");
    addKnob(texturePanel, "hum", "Motor Hum", "Power and transport tone leakage.");
    addKnob(texturePanel, "ceiling", "Safety", "Protected output ceiling before head filtering.");

    addSwitch(deckPanel, "sfxEnable", "Deck sound", "Enable authentic cassette or VHS transport recordings.");
    addChoice(deckPanel, "sfxBank", "Machine", "Choose cassette or VHS mechanism recordings.");
    addChoice(deckPanel, "sfxMode", "Behaviour", "Bed loops continuously; Edges reacts to audio; Sequence triggers sparse actions.");
    addKnob(deckPanel, "sfxLevel", "Mechanism", "Level of the recorded mechanism layer.");
    addKnob(deckPanel, "mix", "Mix", "Latency-aligned dry/wet balance; fully dry excludes the mechanism layer.");

    performerPage.addAndMakeVisible(performerMotionPanel);
    performerPage.addAndMakeVisible(performerDamagePanel);
    performerPage.addAndMakeVisible(performerDeckPanel);
    performerPage.addAndMakeVisible(performerOutputPanel);

    addKnob(performerMotionPanel, "speed", "Tape Speed", "Playable transport speed.");
    addKnob(performerMotionPanel, "transportDrift", "Free Drift", "Organic transport drift remains free-running.");
    addKnob(performerMotionPanel, "wowDepthMs", "Wow Depth", "Slow pitch-motion depth.");
    addSwitch(performerMotionPanel, "wowTempoSync", "Sync Wow", "Lock the wow cycle to host tempo.");
    addChoice(performerMotionPanel, "wowDivision", "Wow Cycle", "Musical length of one wow cycle.");
    addKnob(performerMotionPanel, "flutterDepthMs", "Flutter", "Fast pitch-motion depth.");
    addSwitch(performerMotionPanel, "flutterTempoSync", "Sync Flutter", "Lock the flutter cycle to host tempo.");
    addChoice(performerMotionPanel, "flutterDivision", "Flutter Cycle", "Musical length of one flutter cycle.");

    addKnob(performerDamagePanel, "dropout", "Free Dropouts", "Physical random losses used when clock sync is off.");
    addKnob(performerDamagePanel, "dropoutMs", "Length ms", "Manual/free dropout length when sync length is off.");
    addSwitch(performerDamagePanel, "dropoutTempoSync", "Clock Sync", "Replace random dropouts with host-grid events.");
    addChoice(performerDamagePanel, "dropoutDivision", "Trigger Grid", "When a dropout may begin.");
    addKnob(performerDamagePanel, "dropoutProbability", "Probability", "Stable chance per grid step; no hidden queue.");
    addKnob(performerDamagePanel, "dropoutStrength", "Drop Depth", "How far the tape level falls.");
    addSwitch(performerDamagePanel, "dropoutLengthSync", "Sync Length", "Quantize the dropout envelope length.");
    addChoice(performerDamagePanel, "dropoutLengthDivision", "Drop Length", "Musical duration of each dropout.");
    dropoutButton.setColour(juce::TextButton::buttonColourId, juce::Colour(oxblood));
    dropoutButton.setColour(juce::TextButton::textColourOffId, juce::Colour(bone));
    dropoutButton.setTooltip("Trigger one bounded stereo-linked dropout immediately.");
    dropoutButton.onClick = [this] { processor.triggerDropout(); };
    performerDamagePanel.addAndMakeVisible(dropoutButton);
    panelItems[&performerDamagePanel].push_back(&dropoutButton);

    addSwitch(performerDeckPanel, "sfxEnable", "Deck Sound", "Enable captured machine recordings.");
    addChoice(performerDeckPanel, "sfxBank", "Machine", "Cassette or VHS mechanism.");
    addChoice(performerDeckPanel, "sfxMode", "Behaviour", "Continuous bed, audio edges, or sparse sequence.");
    addKnob(performerDeckPanel, "sfxLevel", "Mechanism", "Captured mechanism level only.");
    mechanismButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff243f42));
    mechanismButton.setColour(juce::TextButton::textColourOffId, juce::Colour(bone));
    mechanismButton.setTooltip("Fire one authentic cassette or VHS mechanism action.");
    mechanismButton.onClick = [this] { processor.triggerMechanism(); };
    performerDeckPanel.addAndMakeVisible(mechanismButton);
    panelItems[&performerDeckPanel].push_back(&mechanismButton);

    addKnob(performerOutputPanel, "mix", "Mix", "Latency-aligned dry and wet balance.");
    addKnob(performerOutputPanel, "outGain", "Output", "Final output trim.");
    addKnob(performerOutputPanel, "ceiling", "Ceiling", "Hard safety ceiling without changing the creative controls.");

    setResizable(true, true);
    setResizeLimits(960, 640, 1600, 1000);
    setSize(1180, 740);
    showMode(EditorMode::simple);
    startTimerHz(30);
}

TapeEngineAudioProcessorEditor::~TapeEngineAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

TapeEngineAudioProcessorEditor::Knob* TapeEngineAudioProcessorEditor::addKnob(
    Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto component = std::make_unique<Knob>(apvts, id, text);
    component->setHint(hint);
    component->setUserChange([this] {
        if (processor.legacyMacrosActive())
            processor.materialiseLegacyMacros();
        markCustom();
    });
    owner.addAndMakeVisible(*component);
    auto* result = component.get();
    panelItems[&owner].push_back(result);
    knobs.push_back(std::move(component));
    return result;
}

void TapeEngineAudioProcessorEditor::addSwitch(
    Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto component = std::make_unique<Switch>(apvts, id, text);
    component->setHint(hint);
    component->setUserChange([this] { markCustom(); });
    owner.addAndMakeVisible(*component);
    panelItems[&owner].push_back(component.get());
    switches.push_back(std::move(component));
}

void TapeEngineAudioProcessorEditor::addChoice(
    Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto component = std::make_unique<Choice>(apvts, id, text);
    component->setHint(hint);
    component->setUserChange([this] { markCustom(); });
    owner.addAndMakeVisible(*component);
    panelItems[&owner].push_back(component.get());
    choices.push_back(std::move(component));
}

void TapeEngineAudioProcessorEditor::layoutPanel(Panel& owner, int columns)
{
    auto area = owner.contentBounds();
    auto& items = panelItems[&owner];
    if (items.empty())
        return;
    const auto cols = juce::jmax(1, columns);
    const auto rows = juce::jmax(1, ((int) items.size() + cols - 1) / cols);
    const auto cellWidth = area.getWidth() / cols;
    const auto cellHeight = area.getHeight() / rows;
    for (int i = 0; i < (int) items.size(); ++i)
    {
        const auto column = i % cols;
        const auto row = i / cols;
        items[(size_t) i]->setBounds(area.getX() + column * cellWidth,
                                     area.getY() + row * cellHeight,
                                     cellWidth, cellHeight);
    }
}

void TapeEngineAudioProcessorEditor::showMode(EditorMode mode)
{
    currentMode = mode;
    surfaceButton.setToggleState(mode == EditorMode::simple, juce::dontSendNotification);
    advancedButton.setToggleState(mode == EditorMode::advanced, juce::dontSendNotification);
    performerButton.setToggleState(mode == EditorMode::performer, juce::dontSendNotification);
    surfacePage.setVisible(mode == EditorMode::simple);
    advancedPage.setVisible(mode == EditorMode::advanced);
    performerPage.setVisible(mode == EditorMode::performer);
    resized();
}

void TapeEngineAudioProcessorEditor::setParamValue(const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
}

float TapeEngineAudioProcessorEditor::getParamValue(const juce::String& id) const
{
    if (auto* value = apvts.getRawParameterValue(id))
        return value->load();
    return 0.0f;
}

void TapeEngineAudioProcessorEditor::resetParameters()
{
    for (auto* parameter : processor.getParameters())
        parameter->setValueNotifyingHost(parameter->getDefaultValue());
}

void TapeEngineAudioProcessorEditor::applyPreset(int idx)
{
    if (idx < 0 || idx >= (int) std::size(kPresets))
        return;
    resetParameters();
    setParamValue("outGain", 0.98f);
    for (const auto& item : kPresets[(size_t) idx].values)
        if (juce::String(item.first) != "macroLink") setParamValue(item.first, item.second);
    setParamValue("macroLink", 1.0f);
    processor.materialiseLegacyMacros();
    for (const auto& item : kPresets[(size_t) idx].values)
        if (juce::String(item.first) != "quality" && juce::String(item.first) != "age"
            && juce::String(item.first) != "wow" && juce::String(item.first) != "glitch")
            setParamValue(item.first, item.second);
    setParamValue("macroLink", 0.0f);
    apvts.state.setProperty("factoryPresetName", kPresets[(size_t) idx].name, nullptr);
}

void TapeEngineAudioProcessorEditor::markCustom()
{
    presetBox.setSelectedId(1, juce::dontSendNotification);
    apvts.state.setProperty("factoryPresetName", "Custom", nullptr);
}

void TapeEngineAudioProcessorEditor::timerCallback()
{
    deckDisplay.setState(processor.outputTrace(), processor.inputPeak(0), processor.inputPeak(1),
                         processor.outputPeakForChannel(0), processor.outputPeakForChannel(1),
                         processor.modulationMeter(), processor.dropoutProgressMeter(),
                         processor.dropoutIsActive(), processor.compressionMeter(),
                         processor.saturationMeter(), processor.noiseMeter(),
                         processor.mechanismMeter(), processor.limiterMeter(),
                         (int) getParamValue("sfxBank"));
    if (processor.dropoutIsActive())
        statusLabel.setText("DROPOUT ACTIVE  /  LINKED RECOVERY", juce::dontSendNotification);
    else if (processor.legacyMacrosActive())
        statusLabel.setText("LEGACY SESSION  /  SOUND PRESERVED", juce::dontSendNotification);
    else
        statusLabel.setText("CANONICAL DSP  /  ALL VIEWS SHARE ONE STATE", juce::dontSendNotification);
}

void TapeEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(deepInk));
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient glow(juce::Colour(oxblood).withAlpha(0.28f), bounds.getCentreX(), 0.0f,
                              juce::Colour(deepInk), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRect(bounds);

    auto frame = bounds.reduced(10.0f);
    g.setColour(juce::Colour(0xff34312b));
    g.drawRoundedRectangle(frame, 9.0f, 1.0f);
    g.setColour(juce::Colour(cyan).withAlpha(0.65f));
    g.fillRect(frame.getX(), frame.getY(), juce::jmin(190.0f, frame.getWidth() * 0.22f), 2.0f);
    g.setColour(juce::Colour(magenta).withAlpha(0.65f));
    const auto accentWidth = juce::jmin(130.0f, frame.getWidth() * 0.16f);
    g.fillRect(frame.getRight() - accentWidth, frame.getY(), accentWidth, 2.0f);
}

void TapeEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(22, 18);
    auto header = area.removeFromTop(72);
    auto identity = header.removeFromLeft(juce::jmin(430, header.getWidth() / 2));
    brand.setBounds(identity.removeFromTop(17));
    title.setBounds(identity.removeFromTop(33));
    subtitle.setBounds(identity);

    auto profile = header.removeFromRight(250);
    presetLabel.setBounds(profile.removeFromTop(18));
    presetBox.setBounds(profile.removeFromTop(32));

    auto nav = area.removeFromTop(40);
    surfaceButton.setBounds(nav.removeFromLeft(108).reduced(0, 4));
    nav.removeFromLeft(7);
    advancedButton.setBounds(nav.removeFromLeft(118).reduced(0, 4));
    nav.removeFromLeft(7);
    performerButton.setBounds(nav.removeFromLeft(124).reduced(0, 4));
    statusLabel.setBounds(nav);
    area.removeFromTop(7);

    auto displayArea = area.removeFromLeft((int) std::round(area.getWidth() * 0.38f));
    deckDisplay.setBounds(displayArea.reduced(2).withTrimmedRight(6));
    auto pageArea = area.reduced(2);
    surfacePage.setBounds(pageArea);
    advancedPage.setBounds(pageArea);
    performerPage.setBounds(pageArea);

    auto surface = surfacePage.getLocalBounds();
    simpleCharacterPanel.setBounds(surface.removeFromTop((int) std::round(surface.getHeight() * 0.43f)).reduced(2));
    simpleMotionPanel.setBounds(surface.reduced(2));
    layoutPanel(simpleCharacterPanel, 3);
    layoutPanel(simpleMotionPanel, 5);

    auto advanced = advancedPage.getLocalBounds();
    constexpr int gap = 9;
    auto top = advanced.removeFromTop((advanced.getHeight() - gap) / 2);
    advanced.removeFromTop(gap);
    auto tone = top.removeFromLeft((top.getWidth() - gap) / 2);
    top.removeFromLeft(gap);
    tonePanel.setBounds(tone);
    transportPanel.setBounds(top);
    auto texture = advanced.removeFromLeft((advanced.getWidth() - gap) / 2);
    advanced.removeFromLeft(gap);
    texturePanel.setBounds(texture);
    deckPanel.setBounds(advanced);
    layoutPanel(tonePanel, 3);
    layoutPanel(transportPanel, 3);
    layoutPanel(texturePanel, 3);
    layoutPanel(deckPanel, 3);

    auto performer = performerPage.getLocalBounds();
    const auto motionHeight = (int) std::round(performer.getHeight() * 0.34f);
    const auto damageHeight = (int) std::round(performer.getHeight() * 0.38f);
    performerMotionPanel.setBounds(performer.removeFromTop(motionHeight).reduced(2));
    performerDamagePanel.setBounds(performer.removeFromTop(damageHeight).reduced(2));
    auto deck = performer.removeFromLeft((int) std::round(performer.getWidth() * 0.64f));
    performerDeckPanel.setBounds(deck.reduced(2));
    performerOutputPanel.setBounds(performer.reduced(2));
    layoutPanel(performerMotionPanel, 4);
    layoutPanel(performerDamagePanel, 5);
    layoutPanel(performerDeckPanel, 3);
    layoutPanel(performerOutputPanel, 3);
}
