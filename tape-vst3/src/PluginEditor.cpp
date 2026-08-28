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
        { "sfxEnable", 1.0f }, { "sfxBank", 0.0f }, { "sfxMode", 0.0f }, { "sfxLevel", 0.18f }
    } },
    { "Worn Cassette", {
        { "quality", 0.32f }, { "age", 0.62f }, { "wow", 0.55f }, { "glitch", 0.35f },
        { "sfxEnable", 1.0f }, { "sfxBank", 0.0f }, { "sfxMode", 1.0f }, { "sfxLevel", 0.30f }
    } },
    { "VHS HiFi", {
        { "quality", 0.78f }, { "age", 0.24f }, { "wow", 0.12f }, { "glitch", 0.08f },
        { "sfxEnable", 1.0f }, { "sfxBank", 1.0f }, { "sfxMode", 0.0f }, { "sfxLevel", 0.16f }
    } },
    { "VHS Linear", {
        { "quality", 0.42f }, { "age", 0.48f }, { "wow", 0.30f }, { "glitch", 0.22f },
        { "sfxEnable", 1.0f }, { "sfxBank", 1.0f }, { "sfxMode", 2.0f }, { "sfxLevel", 0.25f }
    } },
    { "Rewind Melt", {
        { "quality", 0.08f }, { "age", 0.82f }, { "wow", 0.92f }, { "glitch", 0.62f },
        { "speed", 0.93f }, { "sfxEnable", 1.0f }, { "sfxBank", 0.0f }, { "sfxMode", 2.0f }, { "sfxLevel", 0.35f }
    } },
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
    g.setColour(bone);
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
    auto bounds = button.getLocalBounds().toFloat();
    const auto switchArea = bounds.removeFromLeft(42.0f).reduced(2.0f, 7.0f);
    g.setColour(button.getToggleState() ? juce::Colour(cyan).withAlpha(0.34f)
                                        : juce::Colour(highlighted ? panelLift : deepInk));
    g.fillRoundedRectangle(switchArea, switchArea.getHeight() * 0.5f);
    g.setColour(button.getToggleState() ? juce::Colour(cyan) : juce::Colour(0xff6b655b));
    g.drawRoundedRectangle(switchArea, switchArea.getHeight() * 0.5f, 1.0f);
    const auto dot = switchArea.getHeight() - 6.0f;
    const auto dotX = button.getToggleState() ? switchArea.getRight() - dot - 3.0f : switchArea.getX() + 3.0f;
    g.setColour(button.getToggleState() ? juce::Colour(cyan) : juce::Colour(dimBone));
    g.fillEllipse(dotX, switchArea.getY() + 3.0f, dot, dot);
    g.setColour(bone);
    g.setFont(labelFont(12.0f, true));
    g.drawFittedText(button.getButtonText(), bounds.toNearestInt().withTrimmedLeft(8),
                     juce::Justification::centredLeft, 1);
}

void TapeEngineAudioProcessorEditor::TapeLookAndFeel::drawComboBox(
    juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
    g.setColour(deepInk);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(juce::Colour(0xff625b4e));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    juce::Path arrow;
    const auto cx = (float) width - 15.0f;
    const auto cy = (float) height * 0.52f;
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour(cyan);
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
    g.setColour(cyan);
    g.fillRect(juce::Rectangle<float>(14.0f, 13.0f, 20.0f, 2.0f));
    g.setColour(dimBone);
    g.setFont(labelFont(11.0f, true));
    g.drawText(title, 42, 5, getWidth() - 54, 22, juce::Justification::centredLeft);
}

juce::Rectangle<int> TapeEngineAudioProcessorEditor::Panel::contentBounds() const
{
    return getLocalBounds().withTrimmedTop(31).reduced(8, 6);
}

void TapeEngineAudioProcessorEditor::DeckDisplay::setMotion(float newMotion)
{
    motion = juce::jlimit(0.0f, 1.0f, newMotion);
    phase = std::fmod(phase + 0.018f + motion * 0.055f, juce::MathConstants<float>::twoPi);
    repaint();
}

void TapeEngineAudioProcessorEditor::DeckDisplay::setOutputLevel(float newLevel)
{
    outputLevel = juce::jlimit(0.0f, 1.0f, newLevel);
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

    g.setColour(ink);
    g.setFont(labelFont(12.0f, true));
    g.drawText("LOST AUDIO // TYPE II", plate.removeFromTop(24.0f), juce::Justification::centredLeft);

    auto cassette = juce::Rectangle<float>(outer.getX() + outer.getWidth() * 0.12f,
                                            outer.getY() + outer.getHeight() * 0.25f,
                                            outer.getWidth() * 0.76f,
                                            outer.getHeight() * 0.53f);
    g.setColour(juce::Colour(oxblood));
    g.fillRoundedRectangle(cassette, 8.0f);
    g.setColour(juce::Colour(ink));
    g.drawRoundedRectangle(cassette, 8.0f, 2.0f);

    auto window = cassette.reduced(cassette.getWidth() * 0.12f, cassette.getHeight() * 0.18f);
    g.setColour(deepInk);
    g.fillRoundedRectangle(window, 5.0f);
    g.setColour(juce::Colour(0xff69434a));
    g.drawRoundedRectangle(window, 5.0f, 1.0f);

    const auto reelRadius = juce::jmin(window.getHeight() * 0.34f, window.getWidth() * 0.11f);
    const auto left = juce::Point<float>(window.getX() + window.getWidth() * 0.25f, window.getCentreY());
    const auto right = juce::Point<float>(window.getRight() - window.getWidth() * 0.25f, window.getCentreY());
    g.setColour(juce::Colour(0xff25211e));
    g.drawLine(left.x, left.y + reelRadius, right.x, right.y + reelRadius, 3.0f);

    const auto drawReel = [&g, this, reelRadius](juce::Point<float> centre, float offset) {
        g.setColour(bone);
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
    lit.setWidth(lit.getWidth() * outputLevel);
    juce::ColourGradient levelGradient(juce::Colour(cyan), lit.getX(), lit.getY(),
                                       juce::Colour(magenta), meter.getRight(), meter.getY(), false);
    g.setGradientFill(levelGradient);
    g.fillRoundedRectangle(lit, 3.0f);

    g.setColour(juce::Colour(ink).withAlpha(0.74f));
    g.setFont(labelFont(10.0f, true));
    g.drawText("CAPSTAN LOCK", (int) cassette.getX(), (int) meter.getBottom() + 2,
               (int) cassette.getWidth(), 16, juce::Justification::centredRight);
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

    brand.setText("B&E DIGITAL / LOST AUDIO", juce::dontSendNotification);
    brand.setFont(labelFont(11.0f, true));
    brand.setColour(juce::Label::textColourId, juce::Colour(cyan));
    addAndMakeVisible(brand);

    title.setText("TAPE ENGINE", juce::dontSendNotification);
    title.setFont(labelFont(28.0f, true));
    title.setColour(juce::Label::textColourId, juce::Colour(bone));
    addAndMakeVisible(title);

    subtitle.setText("MAGNETIC SIGNAL WEATHERING / V2", juce::dontSendNotification);
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
    presetBox.setSelectedId(1, juce::dontSendNotification);
    presetBox.setTooltip("Factory tape profiles; character-macro edits return the profile to Custom.");
    addAndMakeVisible(presetBox);

    constexpr int viewGroup = 0x4c4145;
    for (auto* button : { &surfaceButton, &advancedButton })
    {
        button->setClickingTogglesState(true);
        button->setRadioGroupId(viewGroup);
        button->setColour(juce::TextButton::textColourOnId, juce::Colour(deepInk));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour(dimBone));
        addAndMakeVisible(*button);
    }
    surfaceButton.onClick = [this]() { showAdvanced(false); };
    advancedButton.onClick = [this]() { showAdvanced(true); };
    surfaceButton.setToggleState(true, juce::dontSendNotification);

    statusLabel.setText("12 MS ANALOG PATH  /  SAFE OUTPUT", juce::dontSendNotification);
    statusLabel.setFont(labelFont(10.0f, true));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(dimBone));
    statusLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(statusLabel);

    addAndMakeVisible(surfacePage);
    addChildComponent(advancedPage);
    surfacePage.addAndMakeVisible(deckDisplay);
    surfacePage.addAndMakeVisible(macroPanel);
    surfacePage.addAndMakeVisible(surfaceOutputPanel);

    addKnob(macroPanel, "quality", "Fidelity", "Preserves bandwidth while reducing hiss and hum.");
    addKnob(macroPanel, "age", "Oxide Age", "Adds head bump, compression, saturation and age-related dropout.");
    addKnob(macroPanel, "wow", "Transport", "Sets the combined wow, flutter and speed instability.");
    addKnob(macroPanel, "glitch", "Damage", "Controls physical dropout frequency and duration.");
    addKnob(surfaceOutputPanel, "outGain", "Output", "Final output level after tape coloration.");
    addKnob(surfaceOutputPanel, "sfxLevel", "Mechanism", "Level of cassette or VHS mechanism recordings.");
    addSwitch(surfaceOutputPanel, "sfxEnable", "Deck sound", "Enable the recorded mechanism layer.");

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
    addKnob(transportPanel, "wowDepthMs", "Wow Depth", "Slow capstan modulation depth in milliseconds.");
    addKnob(transportPanel, "flutterDepthMs", "Flutter", "Fast transport modulation depth.");
    addKnob(transportPanel, "dropout", "Dropout", "Frequency and depth of physical signal losses.");
    addKnob(transportPanel, "dropoutMs", "Drop Length", "Duration of each dropout event.");

    addKnob(texturePanel, "drive", "Oxide Drive", "Gain-compensated tape saturation.");
    addKnob(texturePanel, "comp", "Leveler", "Tape-style downward compression and density.");
    addKnob(texturePanel, "hiss", "Hiss", "Wideband magnetic tape hiss.");
    addKnob(texturePanel, "hum", "Motor Hum", "Power and transport tone leakage.");
    addKnob(texturePanel, "ceiling", "Safety", "Protected output ceiling before head filtering.");

    addSwitch(deckPanel, "sfxEnable", "Deck sound", "Enable authentic cassette or VHS transport recordings.");
    addChoice(deckPanel, "sfxBank", "Machine", "Choose cassette or VHS mechanism recordings.");
    addChoice(deckPanel, "sfxMode", "Behaviour", "Bed loops continuously; Edges reacts to audio; Sequence triggers sparse actions.");
    addKnob(deckPanel, "sfxLevel", "Mechanism", "Level of the recorded mechanism layer.");

    lastQuality = getParamValue("quality");
    lastAge = getParamValue("age");
    lastWow = getParamValue("wow");
    lastGlitch = getParamValue("glitch");

    setResizable(true, true);
    setResizeLimits(820, 560, 1600, 1000);
    setSize(1040, 680);
    startTimerHz(30);
}

TapeEngineAudioProcessorEditor::~TapeEngineAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void TapeEngineAudioProcessorEditor::addKnob(
    Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto component = std::make_unique<Knob>(apvts, id, text);
    component->setHint(hint);
    owner.addAndMakeVisible(*component);
    panelItems[&owner].push_back(component.get());
    knobs.push_back(std::move(component));
}

void TapeEngineAudioProcessorEditor::addSwitch(
    Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto component = std::make_unique<Switch>(apvts, id, text);
    component->setHint(hint);
    owner.addAndMakeVisible(*component);
    panelItems[&owner].push_back(component.get());
    switches.push_back(std::move(component));
}

void TapeEngineAudioProcessorEditor::addChoice(
    Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto component = std::make_unique<Choice>(apvts, id, text);
    component->setHint(hint);
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

void TapeEngineAudioProcessorEditor::showAdvanced(bool shouldShowAdvanced)
{
    showingAdvanced = shouldShowAdvanced;
    surfaceButton.setToggleState(!shouldShowAdvanced, juce::dontSendNotification);
    advancedButton.setToggleState(shouldShowAdvanced, juce::dontSendNotification);
    surfacePage.setVisible(!shouldShowAdvanced);
    advancedPage.setVisible(shouldShowAdvanced);
    statusLabel.setText(shouldShowAdvanced ? "FULL SIGNAL PATH  /  NO HIDDEN CONTROLS"
                                           : "12 MS ANALOG PATH  /  SAFE OUTPUT",
                        juce::dontSendNotification);
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

void TapeEngineAudioProcessorEditor::applyMacroQuality(float quality)
{
    const auto targets = lost_audio::core::mapTapeMacros(quality, getParamValue("age"), getParamValue("wow"), getParamValue("glitch"));
    setParamValue("lpHz", targets.lowPassHz);
    setParamValue("hpHz", targets.highPassHz);
    setParamValue("hiss", targets.hiss);
    setParamValue("hum", targets.hum);
}

void TapeEngineAudioProcessorEditor::applyMacroAge(float age)
{
    const auto targets = lost_audio::core::mapTapeMacros(getParamValue("quality"), age, getParamValue("wow"), getParamValue("glitch"));
    setParamValue("drive", targets.drive);
    setParamValue("comp", targets.compression);
    setParamValue("headBumpDb", targets.headBumpDb);
    setParamValue("headBumpHz", targets.headBumpHz);
    setParamValue("outGain", targets.outputGain);
    setParamValue("ceiling", targets.ceiling);
    setParamValue("dropout", targets.dropout);
}

void TapeEngineAudioProcessorEditor::applyMacroWow(float wow)
{
    const auto targets = lost_audio::core::mapTapeMacros(getParamValue("quality"), getParamValue("age"), wow, getParamValue("glitch"));
    setParamValue("wowDepthMs", targets.wowDepthMs);
    setParamValue("flutterDepthMs", targets.flutterDepthMs);
    setParamValue("speed", targets.speed);
}

void TapeEngineAudioProcessorEditor::applyMacroGlitch(float glitch)
{
    const auto targets = lost_audio::core::mapTapeMacros(getParamValue("quality"), getParamValue("age"), getParamValue("wow"), glitch);
    setParamValue("dropout", targets.dropout);
    setParamValue("dropoutMs", targets.dropoutMs);
}

void TapeEngineAudioProcessorEditor::applyPreset(int idx)
{
    if (idx < 0 || idx >= (int) std::size(kPresets))
        return;
    suppressMacros = true;
    for (const auto& item : kPresets[(size_t) idx].values)
        setParamValue(item.first, item.second);
    applyMacroQuality(getParamValue("quality"));
    applyMacroAge(getParamValue("age"));
    applyMacroWow(getParamValue("wow"));
    applyMacroGlitch(getParamValue("glitch"));
    suppressMacros = false;
    lastQuality = getParamValue("quality");
    lastAge = getParamValue("age");
    lastWow = getParamValue("wow");
    lastGlitch = getParamValue("glitch");
}

void TapeEngineAudioProcessorEditor::timerCallback()
{
    const auto q = getParamValue("quality");
    const auto a = getParamValue("age");
    const auto w = getParamValue("wow");
    const auto damage = getParamValue("glitch");

    if (!suppressMacros)
    {
        suppressMacros = true;
        bool changed = false;
        if (std::abs(q - lastQuality) > 0.0005f) { applyMacroQuality(q); lastQuality = q; changed = true; }
        if (std::abs(a - lastAge) > 0.0005f) { applyMacroAge(a); lastAge = a; changed = true; }
        if (std::abs(w - lastWow) > 0.0005f) { applyMacroWow(w); lastWow = w; changed = true; }
        if (std::abs(damage - lastGlitch) > 0.0005f) { applyMacroGlitch(damage); lastGlitch = damage; changed = true; }
        if (changed)
            presetBox.setSelectedId(1, juce::dontSendNotification);
        suppressMacros = false;
    }

    const auto peak = processor.getOutputPeak();
    displayLevel = peak > displayLevel ? peak : displayLevel * 0.91f;
    deckDisplay.setOutputLevel(std::sqrt(juce::jlimit(0.0f, 1.0f, displayLevel)));
    deckDisplay.setMotion(w);
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
    statusLabel.setBounds(nav);
    area.removeFromTop(7);

    surfacePage.setBounds(area);
    advancedPage.setBounds(area);

    auto surface = surfacePage.getLocalBounds();
    if (surface.getWidth() >= 960)
    {
        auto left = surface.removeFromLeft((int) std::round(surface.getWidth() * 0.54f));
        deckDisplay.setBounds(left.reduced(0, 2).withTrimmedRight(8));
        auto right = surface.withTrimmedLeft(4);
        macroPanel.setBounds(right.removeFromTop((int) std::round(right.getHeight() * 0.67f)).withTrimmedBottom(5));
        surfaceOutputPanel.setBounds(right.withTrimmedTop(5));
        layoutPanel(macroPanel, 2);
        layoutPanel(surfaceOutputPanel, 3);
    }
    else
    {
        deckDisplay.setBounds(surface.removeFromTop((int) std::round(surface.getHeight() * 0.39f)).withTrimmedBottom(5));
        macroPanel.setBounds(surface.removeFromTop((int) std::round(surface.getHeight() * 0.61f)).reduced(0, 5));
        surfaceOutputPanel.setBounds(surface.withTrimmedTop(5));
        layoutPanel(macroPanel, 4);
        layoutPanel(surfaceOutputPanel, 3);
    }

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
    layoutPanel(tonePanel, 5);
    layoutPanel(transportPanel, 5);
    layoutPanel(texturePanel, 5);
    layoutPanel(deckPanel, 4);
}
