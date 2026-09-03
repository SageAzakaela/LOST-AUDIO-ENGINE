#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr std::uint32_t ink = 0xff090a0a, deep = 0xff161717, panel = 0xff222321, line = 0xff4a4a45;
constexpr std::uint32_t bone = 0xffeee9dc, dim = 0xffaaa79d, lcd = 0xffb6dfb1, rec = 0xffff4e47, amber = 0xffffb23d, cyan = 0xff53dce7;
juce::Font font(float size, bool bold = false) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }
struct PresetDef { const char* name; std::initializer_list<std::pair<const char*, float>> values; };
bool isCamcorderMacroInput(const juce::String& id)
{
    return id == "format" || id == "micModel" || id == "coverage" || id == "movement"
        || id == "corruption" || id == "agc";
}
const PresetDef presets[] {
    { "MiniDV Clean", { { "format", 2.0f }, { "micModel", 0.0f }, { "coverage", .18f }, { "movement", .08f }, { "corruption", .04f }, { "agc", .28f }, { "wind", 0.0f } } },
    { "MiniDV Family Tape", { { "format", 2.0f }, { "micModel", 0.0f }, { "coverage", .32f }, { "movement", .22f }, { "corruption", .18f }, { "agc", .38f }, { "wind", 0.0f }, { "camBedEnable", 1.0f }, { "camBedLevel", .18f } } },
    { "Hi8 Home Movie", { { "format", 1.0f }, { "micModel", 0.0f }, { "coverage", .38f }, { "movement", .28f }, { "corruption", .20f }, { "agc", .42f }, { "wind", 0.0f }, { "camBedEnable", 1.0f }, { "camBedLevel", .26f } } },
    { "VHS-C Birthday", { { "format", 0.0f }, { "micModel", 1.0f }, { "coverage", .48f }, { "movement", .18f }, { "corruption", .26f }, { "agc", .48f }, { "wind", 0.0f }, { "camBedEnable", 1.0f }, { "camBedLevel", .28f } } },
    { "Pocket Digicam", { { "format", 3.0f }, { "micModel", 1.0f }, { "coverage", .42f }, { "movement", .20f }, { "corruption", .24f }, { "agc", .34f }, { "wind", 0.0f }, { "camBedEnable", 1.0f }, { "camBedLevel", .18f } } },
    { "DSLR Auto Level", { { "format", 3.0f }, { "micModel", 2.0f }, { "coverage", .20f }, { "movement", .10f }, { "corruption", .06f }, { "agc", .56f }, { "wind", 0.0f } } },
    { "Shotgun Documentary", { { "format", 2.0f }, { "micModel", 4.0f }, { "coverage", .12f }, { "movement", .12f }, { "corruption", .08f }, { "agc", .30f }, { "wind", 0.0f } } },
    { "Waterproof Housing", { { "format", 4.0f }, { "micModel", 3.0f }, { "coverage", .68f }, { "movement", .34f }, { "corruption", .10f }, { "agc", .52f }, { "wind", 0.0f } } },
    { "Action Cam Helmet", { { "format", 4.0f }, { "micModel", 3.0f }, { "coverage", .35f }, { "movement", .70f }, { "corruption", .16f }, { "agc", .64f }, { "wind", 1.0f }, { "windLevel", .32f } } },
    { "Wind Across Capsule", { { "format", 1.0f }, { "micModel", 0.0f }, { "coverage", .40f }, { "movement", .72f }, { "corruption", .10f }, { "agc", .46f }, { "wind", 1.0f }, { "windLevel", .38f } } },
    { "Pocket Scrape", { { "format", 3.0f }, { "micModel", 1.0f }, { "coverage", .74f }, { "movement", .82f }, { "corruption", .12f }, { "agc", .52f }, { "wind", 0.0f } } },
    { "Bad Tracking", { { "format", 0.0f }, { "micModel", 1.0f }, { "coverage", .52f }, { "movement", .40f }, { "corruption", .62f }, { "agc", .44f }, { "dropMode", 0.0f }, { "wind", 0.0f } } },
    { "Digital Frame Loss", { { "format", 2.0f }, { "micModel", 0.0f }, { "coverage", .34f }, { "movement", .20f }, { "corruption", .70f }, { "agc", .38f }, { "dropMode", 2.0f }, { "wind", 0.0f } } },
    { "Repeat The Moment", { { "format", 2.0f }, { "micModel", 0.0f }, { "coverage", .42f }, { "movement", .26f }, { "corruption", .78f }, { "agc", .40f }, { "dropMode", 3.0f }, { "wind", 0.0f } } },
    { "Found Footage", { { "format", 0.0f }, { "micModel", 1.0f }, { "coverage", .76f }, { "movement", .56f }, { "corruption", .78f }, { "agc", .58f }, { "dropMode", 3.0f }, { "wind", 0.0f }, { "camBedEnable", 1.0f }, { "camBedLevel", .32f }, { "ceiling", .86f } } },
    { "Panic Run", { { "format", 4.0f }, { "micModel", 0.0f }, { "coverage", .46f }, { "movement", .94f }, { "corruption", .58f }, { "agc", .82f }, { "dropMode", 0.0f }, { "wind", 1.0f }, { "windLevel", .36f }, { "camBedEnable", 1.0f }, { "camBedLevel", .22f }, { "ceiling", .84f } } },
    { "PLAY - Guitar Home Video", { { "format", 1 }, { "micModel", 1 }, { "coverage", .34f }, { "movement", .12f }, { "corruption", .08f }, { "agc", .26f }, { "wind", 0 }, { "camBedEnable", 1 }, { "camBedLevel", .10f }, { "mix", .78f }, { "ceiling", .90f }, { "outGain", .94f } } },
    { "PLAY - Drum Handheld Punch", { { "format", 2 }, { "micModel", 0 }, { "coverage", .28f }, { "movement", .18f }, { "corruption", .10f }, { "agc", .48f }, { "wind", 0 }, { "camBedEnable", 0 }, { "mix", .64f }, { "ceiling", .88f }, { "outGain", .94f } } },
    { "PLAY - Synth Frame Repeat", { { "format", 2 }, { "micModel", 0 }, { "coverage", .32f }, { "movement", .14f }, { "corruption", .18f }, { "agc", .30f }, { "dropMode", 3 }, { "wind", 0 }, { "dropTempoSync", 1 }, { "dropDivision", 3 }, { "dropProbability", .24f }, { "dropLengthSync", 1 }, { "dropLengthDivision", 5 }, { "mix", .76f }, { "ceiling", .88f }, { "outGain", .92f } } },
    { "PLAY - Clocked Handling Hits", { { "format", 3 }, { "micModel", 1 }, { "coverage", .38f }, { "movement", .20f }, { "corruption", .08f }, { "agc", .34f }, { "wind", 0 }, { "handlingTempoSync", 1 }, { "handlingDivision", 3 }, { "handlingProbability", .28f }, { "handlingStrength", .38f }, { "mix", .72f }, { "ceiling", .88f }, { "outGain", .92f } } },
};
}

void CamcorderEngineAudioProcessorEditor::Panel::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1); g.setColour(juce::Colour(panel)); g.fillRoundedRectangle(b, 7); g.setColour(juce::Colour(line)); g.drawRoundedRectangle(b, 7, 1);
    g.setColour(juce::Colour(amber)); g.fillRect(b.getX() + 12, b.getY() + 12, 24.0f, 1.5f); g.setColour(juce::Colour(dim)); g.setFont(font(9.5f, true));
    g.drawText(name, getLocalBounds().removeFromTop(34).withTrimmedLeft(44), juce::Justification::centredLeft);
}
CamcorderEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    label.setText(text, juce::dontSendNotification); label.setJustificationType(juce::Justification::centred); label.setColour(juce::Label::textColourId, juce::Colour(bone)); label.setFont(font(10, true)); addAndMakeVisible(label);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(amber)); slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff4a4942));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(bone)); slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(bone)); slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(ink));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(line)); slider.onDragStart = std::move(changed); addAndMakeVisible(slider); attachment = std::make_unique<APVTS::SliderAttachment>(state, id, slider);
}
void CamcorderEngineAudioProcessorEditor::Knob::resized() { auto a = getLocalBounds(); label.setBounds(a.removeFromTop(18)); slider.setBounds(a.reduced(1)); }
CamcorderEngineAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    label.setText(text, juce::dontSendNotification); label.setColour(juce::Label::textColourId, juce::Colour(bone)); label.setFont(font(10, true)); addAndMakeVisible(label);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id))) for (int i = 0; i < p->choices.size(); ++i) combo.addItem(p->choices[i], i + 1);
    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(ink)); combo.setColour(juce::ComboBox::textColourId, juce::Colour(bone)); combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(line));
    combo.onChange = std::move(changed); addAndMakeVisible(combo); attachment = std::make_unique<APVTS::ComboBoxAttachment>(state, id, combo);
}
void CamcorderEngineAudioProcessorEditor::Choice::resized() { auto a = getLocalBounds(); label.setBounds(a.removeFromTop(17)); combo.setBounds(a.removeFromTop(32).reduced(1)); }
CamcorderEngineAudioProcessorEditor::Switch::Switch(APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    button.setButtonText(text); button.setColour(juce::ToggleButton::textColourId, juce::Colour(bone)); button.setColour(juce::ToggleButton::tickColourId, juce::Colour(rec)); button.onClick = std::move(changed);
    addAndMakeVisible(button); attachment = std::make_unique<APVTS::ButtonAttachment>(state, id, button);
}

void CamcorderEngineAudioProcessorEditor::Viewfinder::setState(const std::array<float, 64>& trace, float inL, float inR, float outL, float outR,
    bool w, bool h, bool d, bool c, bool windOn, float dropPhase, float faultPhase, float handlingPhase, float windPhaseValue,
    float agcValue, float flutterValue, float limiterValue, float cameraBedValue, float windBedValue, int f)
{ waveform = trace; input = { inL, inR }; output = { outL, outR }; wind = w; handling = h; dropout = d; corrupt = c; enabledWind = windOn;
  dropProgress = dropPhase; faultProgress = faultPhase; handlingProgress = handlingPhase; windProgress = windPhaseValue; agc = agcValue;
  flutter = flutterValue; limiter = limiterValue; cameraBed = cameraBedValue; windBed = windBedValue; format = f; repaint(); }
void CamcorderEngineAudioProcessorEditor::Viewfinder::paint(juce::Graphics& g)
{
    auto outer = getLocalBounds().toFloat().reduced(1); g.setColour(juce::Colour(0xff252622)); g.fillRoundedRectangle(outer, 11); g.setColour(juce::Colour(0xff57584f)); g.drawRoundedRectangle(outer, 11, 1.4f);
    auto screen = outer.reduced(18).withTrimmedBottom(100); g.setColour(juce::Colour(0xff07100d)); g.fillRoundedRectangle(screen, 5); g.setColour(juce::Colour(0xff274039)); g.drawRoundedRectangle(screen, 5, 1);
    g.setColour(juce::Colour(0xff173027)); for (int i = 1; i < 7; ++i) g.drawHorizontalLine((int) (screen.getY() + i * screen.getHeight() / 7), screen.getX(), screen.getRight());
    g.setFont(font(10, true)); g.setColour(juce::Colour(rec)); g.fillEllipse(screen.getX() + 13, screen.getY() + 13, 8, 8); g.drawText("REC", (int) screen.getX() + 28, (int) screen.getY() + 7, 50, 20, juce::Justification::centredLeft);
    const char* formats[] { "VHS-C", "VIDEO8", "MINIDV", "DIGICAM", "ACTION" }; g.setColour(juce::Colour(lcd)); g.drawText(formats[juce::jlimit(0, 4, format)], screen.toNearestInt().reduced(13, 7), juce::Justification::topRight);
    juce::Path tracePath; for (int i = 0; i < (int) waveform.size(); ++i) { const auto x = screen.getX() + (float) i * screen.getWidth() / (float) (waveform.size() - 1);
        const auto y = screen.getCentreY() - juce::jlimit(0.0f, 1.0f, waveform[(std::size_t) i] * 5.0f) * screen.getHeight() * .42f; if (i == 0) tracePath.startNewSubPath(x, y); else tracePath.lineTo(x, y); }
    g.setColour(juce::Colour(dropout ? rec : corrupt ? cyan : lcd)); g.strokePath(tracePath, juce::PathStrokeType(1.6f));
    auto footer = outer.reduced(18).removeFromBottom(80); auto meters = footer.removeFromLeft(footer.getWidth() * .60f); const char* names[] { "IN L", "IN R", "OUT L", "OUT R" }; const float values[] { input[0], input[1], output[0], output[1] };
    for (int i = 0; i < 4; ++i) { auto row = meters.removeFromTop(18); g.setColour(juce::Colour(dim)); g.setFont(font(8.5f, true)); g.drawText(names[i], row.removeFromLeft(38), juce::Justification::centredLeft); auto bar = row.reduced(2, 5); g.setColour(juce::Colour(0xff44443f)); g.fillRect(bar); g.setColour(juce::Colour(i < 2 ? amber : lcd)); g.fillRect(bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, values[i] * 2.4f))); }
    auto activity = footer.withTrimmedLeft(12); const char* activityNames[] { "DROP", "FAULT", "HIT", "WIND", "AGC", "FLUTTER", "LIMIT", "CAM BED", "WIND BED" };
    const float activityValues[] { dropProgress, faultProgress, handlingProgress, windProgress, agc, flutter, limiter, cameraBed, windBed };
    for (int i = 0; i < 9; ++i) { auto row = activity.removeFromTop(8); g.setColour(juce::Colour(dim)); g.setFont(font(6.8f, true));
        g.drawText(activityNames[i], row.removeFromLeft(43), juce::Justification::centredLeft); auto bar = row.reduced(1, 2); g.setColour(juce::Colour(0xff44443f)); g.fillRect(bar);
        g.setColour(juce::Colour(i == 0 || i == 2 ? rec : i == 1 ? cyan : amber)); g.fillRect(bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, activityValues[i]))); }
    g.setColour(juce::Colour(enabledWind ? amber : 0xff5b5a53)); g.fillEllipse(outer.getRight() - 22, outer.getY() + 16, 6, 6);
}

CamcorderEngineAudioProcessorEditor::CamcorderEngineAudioProcessorEditor(CamcorderEngineAudioProcessor& owner)
    : AudioProcessorEditor(&owner), processor(owner), apvts(owner.getAPVTS())
{
    setOpaque(true); brandLabel.setText("B&E DIGITAL", juce::dontSendNotification); brandLabel.setColour(juce::Label::textColourId, juce::Colour(amber)); brandLabel.setFont(font(10, true)); addAndMakeVisible(brandLabel);
    titleLabel.setText("CAMCORDER ENGINE", juce::dontSendNotification); titleLabel.setColour(juce::Label::textColourId, juce::Colour(bone)); titleLabel.setFont(font(27, true)); addAndMakeVisible(titleLabel);
    subtitleLabel.setText("ON-CAMERA CAPTURE / V3 STEREO", juce::dontSendNotification); subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(dim)); subtitleLabel.setFont(font(10, true)); addAndMakeVisible(subtitleLabel);
    profileLabel.setText("TAPE / SCENE", juce::dontSendNotification); profileLabel.setColour(juce::Label::textColourId, juce::Colour(dim)); profileLabel.setFont(font(9, true)); addAndMakeVisible(profileLabel);
    presetBox.addItem("Custom", 1); for (int i = 0; i < (int) std::size(presets); ++i) presetBox.addItem(presets[i].name, i + 2); const auto restoredPreset=apvts.state.getProperty("factoryPresetName","Custom").toString();auto restoredPresetId=1;for(int i=0;i<(int)std::size(presets);++i)if(restoredPreset==presets[i].name)restoredPresetId=i+2;presetBox.setSelectedId(restoredPresetId, juce::dontSendNotification);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() >= 2) applyPreset(presetBox.getSelectedId() - 2); }; addAndMakeVisible(presetBox);
    for (auto* b : { &surfaceButton, &advancedButton, &performerButton }) { b->setColour(juce::TextButton::textColourOffId, juce::Colour(bone)); addAndMakeVisible(*b); }
    surfaceButton.onClick = [this] { showMode(EditorMode::simple); }; advancedButton.onClick = [this] { showMode(EditorMode::advanced); }; performerButton.onClick = [this] { showMode(EditorMode::performer); };
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(dim)); statusLabel.setFont(font(9, true)); statusLabel.setJustificationType(juce::Justification::centredRight); addAndMakeVisible(statusLabel);
    addAndMakeVisible(viewfinder); addAndMakeVisible(surfacePage); addAndMakeVisible(advancedPage); addAndMakeVisible(performerPage); surfacePage.addAndMakeVisible(capturePanel); surfacePage.addAndMakeVisible(scenePanel);
    for (auto* p : { &tonePanel, &agcPanel, &converterPanel, &damagePanel, &movementPanel, &outputPanel }) advancedPage.addAndMakeVisible(*p);
    for (auto* p : { &performerDropPanel, &performerFaultPanel, &performerHandlingPanel, &performerBedPanel, &performerOutputPanel }) performerPage.addAndMakeVisible(*p);
    addChoice(capturePanel, "format", "FORMAT", "Recording transport and converter family."); addChoice(capturePanel, "micModel", "MICROPHONE", "Physical on-camera capsule model.");
    addKnob(capturePanel, "coverage", "COVERAGE", "Capsule blockage and body occlusion."); addKnob(capturePanel, "movement", "MOVEMENT", "Camera motion and body contact.");
    addKnob(capturePanel, "corruption", "CORRUPTION", "Transport and converter damage, independent of Wind."); addKnob(capturePanel, "agc", "AUTO LEVEL", "Camera preamp and automatic gain intensity.");
    addSwitch(scenePanel, "wind", "WIND ARMED", "Explicitly enable captured wind and wind hits. No macro changes this switch."); addKnob(scenePanel, "windLevel", "WIND LEVEL", "Captured and synthesized wind strength only while Wind is armed."); addSwitch(scenePanel, "camBedEnable", "CAMERA BED", "Explicitly arm the captured camera mechanism recording."); addKnob(scenePanel, "camBedLevel", "BED LEVEL", "Captured camera mechanism level.");
    addKnob(scenePanel, "inputGain", "INPUT dB", "Trim before camera capture."); addKnob(scenePanel, "mix", "MIX", "Latency-aligned dry/wet blend."); addKnob(scenePanel, "outGain", "OUTPUT", "Final camera playback trim."); addKnob(scenePanel, "ceiling", "CEILING", "Protected playback ceiling.");
    addKnob(tonePanel, "coverage", "COVERAGE", "Capsule blockage and body occlusion."); addKnob(tonePanel, "hpHz", "HIGH-PASS", "Microphone low cutoff.", true); addKnob(tonePanel, "lpHz", "LOW-PASS", "Capsule top-end limit.", true); addKnob(tonePanel, "boxDb", "BODY", "Plastic camera-body resonance.", true); addKnob(tonePanel, "boxHz", "BODY Hz", "Body resonance center.", true);
    addKnob(agcPanel, "agc", "DRIVE", "Camera preamp and auto-level intensity."); addKnob(agcPanel, "agcAmt", "AMOUNT", "Automatic gain range.", true); addKnob(agcPanel, "agcSpeed", "SPEED", "Level detector response.", true); addKnob(agcPanel, "agcPump", "PUMP", "Audible gain breathing.", true); addKnob(agcPanel, "clip", "PREAMP CLIP", "Overloaded camera input stage.", true);
    addChoice(converterPanel, "format", "FORMAT", "Recording transport family."); addChoice(converterPanel, "micModel", "MIC", "Physical camera capsule."); addKnob(converterPanel, "crush", "CONVERTER", "Progressive converter damage.", true); addKnob(converterPanel, "flutter", "FLUTTER", "Analog capstan or digital clock motion.", true); addKnob(converterPanel, "bits", "BITS", "Converter resolution.", true); addKnob(converterPanel, "rate", "RATE", "Converter sample-and-hold rate.", true);
    addKnob(damagePanel, "corruption", "CORRUPTION", "Transport/converter instability."); addChoice(damagePanel, "dropMode", "CONCEALMENT", "User-owned strategy; macros never change it."); addKnob(damagePanel, "drop", "DROPOUTS", "Capture loss probability.", true); addKnob(damagePanel, "dropMs", "DROP ms", "Capture loss duration.", true); addKnob(damagePanel, "repeatMs", "REPEAT ms", "History used by Repeat concealment.", true); addKnob(damagePanel, "chirp", "CODEC FAULT", "Rare, protected codec chirps.", true);
    addKnob(movementPanel, "movement", "MOVEMENT", "Camera motion and contact rate."); addKnob(movementPanel, "handling", "BODY HITS", "Low camera-body impacts.", true); addKnob(movementPanel, "rub", "BODY RUB", "Contact and fabric scrape.", true); addKnob(movementPanel, "motorBleed", "MOTOR", "Synthesized transport mechanism bleed.", true); addKnob(movementPanel, "hiss", "MIC HISS", "Capsule and preamp noise.", true); addSwitch(movementPanel, "wind", "WIND", "Explicit wind ownership."); addKnob(movementPanel, "windLevel", "WIND LEVEL", "Captured and synthesized wind level.");
    addKnob(outputPanel, "inputGain", "INPUT dB", "Input trim."); addKnob(outputPanel, "mix", "MIX", "Aligned dry/wet balance."); addKnob(outputPanel, "ceiling", "CEILING", "Output limiter ceiling.", true); addKnob(outputPanel, "outGain", "OUTPUT", "Final trim."); addSwitch(outputPanel, "camBedEnable", "CAMERA BED", "Arm the authentic captured mechanism layer."); addKnob(outputPanel, "camBedLevel", "BED LEVEL", "Captured mechanism level.");

    addSwitch(performerDropPanel, "dropTempoSync", "SYNC LOSS", "Replace random capture loss with clocked events."); addChoice(performerDropPanel, "dropDivision", "TRIGGER GRID", "Where capture loss may begin.");
    addKnob(performerDropPanel, "dropProbability", "PROBABILITY", "Stable chance per grid step."); addChoice(performerDropPanel, "dropMode", "CONCEAL", "How missing capture is reconstructed.");
    addKnob(performerDropPanel, "dropMs", "LENGTH ms", "Free/manual capture-loss duration."); addSwitch(performerDropPanel, "dropLengthSync", "SYNC LENGTH", "Use a musical capture-loss duration.");
    addChoice(performerDropPanel, "dropLengthDivision", "LOSS LENGTH", "Musical capture-loss duration."); dropTriggerButton.onClick = [this] { processor.triggerDropout(); markCustom(); };
    performerDropPanel.addAndMakeVisible(dropTriggerButton); panelItems[&performerDropPanel].push_back(&dropTriggerButton);

    addSwitch(performerFaultPanel, "faultTempoSync", "SYNC FAULT", "Replace random chirps with clocked codec faults."); addChoice(performerFaultPanel, "faultDivision", "TRIGGER GRID", "Where codec faults may begin.");
    addKnob(performerFaultPanel, "faultProbability", "PROBABILITY", "Stable chance per grid step."); addKnob(performerFaultPanel, "faultStrength", "STRENGTH", "Codec-fault intensity.");
    addKnob(performerFaultPanel, "faultDurationMs", "LENGTH ms", "Free/manual codec-fault duration."); addSwitch(performerFaultPanel, "faultLengthSync", "SYNC LENGTH", "Use musical fault duration.");
    addChoice(performerFaultPanel, "faultLengthDivision", "FAULT LENGTH", "Musical codec-fault duration."); faultTriggerButton.onClick = [this] { processor.triggerCodecFault(); markCustom(); };
    performerFaultPanel.addAndMakeVisible(faultTriggerButton); panelItems[&performerFaultPanel].push_back(&faultTriggerButton);

    addSwitch(performerHandlingPanel, "handlingTempoSync", "SYNC HITS", "Replace random camera hits with clocked impacts."); addChoice(performerHandlingPanel, "handlingDivision", "TRIGGER GRID", "Where camera impacts may occur.");
    addKnob(performerHandlingPanel, "handlingProbability", "PROBABILITY", "Stable chance per grid step."); addKnob(performerHandlingPanel, "handlingStrength", "STRENGTH", "Manual/clocked camera impact strength.");
    addKnob(performerHandlingPanel, "rub", "BODY RUB", "Continuous physical contact texture."); handlingTriggerButton.onClick = [this] { processor.triggerHandling(); markCustom(); };
    performerHandlingPanel.addAndMakeVisible(handlingTriggerButton); panelItems[&performerHandlingPanel].push_back(&handlingTriggerButton);

    addSwitch(performerBedPanel, "wind", "WIND ARMED", "Explicitly enable wind; clocks never alter this switch."); addKnob(performerBedPanel, "windLevel", "WIND LEVEL", "Captured/synthesized wind level.");
    addSwitch(performerBedPanel, "camBedEnable", "CAMERA BED", "Explicitly arm recorded mechanism bed."); addKnob(performerBedPanel, "camBedLevel", "BED LEVEL", "Recorded mechanism bed level.");
    addKnob(performerBedPanel, "motorBleed", "MOTOR", "Synthesized transport bleed."); addKnob(performerBedPanel, "hiss", "MIC HISS", "Capsule/preamp hiss.");
    addSwitch(performerOutputPanel, "flutterTempoSync", "SYNC FLUTTER", "Clock flutter motion to host tempo."); addChoice(performerOutputPanel, "flutterDivision", "FLUTTER CYCLE", "Musical flutter cycle.");
    addKnob(performerOutputPanel, "inputGain", "INPUT", "Input trim."); addKnob(performerOutputPanel, "mix", "MIX", "Aligned dry/wet."); addKnob(performerOutputPanel, "outGain", "OUTPUT", "Final trim."); addKnob(performerOutputPanel, "ceiling", "CEILING", "Hard safety ceiling.");
    setResizable(true, true); setResizeLimits(980, 660, 1600, 1000); setSize(1240, 760); showMode(EditorMode::simple); startTimerHz(30);
}
CamcorderEngineAudioProcessorEditor::~CamcorderEngineAudioProcessorEditor() { stopTimer(); }

CamcorderEngineAudioProcessorEditor::Knob* CamcorderEngineAudioProcessorEditor::addKnob(Panel& p, const juce::String& id, const juce::String& text, const juce::String& hint, bool linked)
{ const auto macroInput=isCamcorderMacroInput(id);auto c = std::make_unique<Knob>(apvts, id, text, [this, linked, macroInput] { if(suppressPresetChanges)return;if(macroInput)setParameter("macroLink",1.0f);else if(linked&&processor.legacyMacrosActive())processor.materialiseLegacyMacros();markCustom(false); }); c->setHint(hint); auto* raw = c.get(); p.addAndMakeVisible(*raw); panelItems[&p].push_back(raw); knobs.push_back(std::move(c)); return raw; }
CamcorderEngineAudioProcessorEditor::Choice* CamcorderEngineAudioProcessorEditor::addChoice(Panel& p, const juce::String& id, const juce::String& text, const juce::String& hint, bool linked)
{ const auto macroInput=isCamcorderMacroInput(id);auto c = std::make_unique<Choice>(apvts, id, text, [this, linked, macroInput] { if(suppressPresetChanges)return;if(macroInput)setParameter("macroLink",1.0f);else if(linked&&processor.legacyMacrosActive())processor.materialiseLegacyMacros();markCustom(false); }); c->setHint(hint); auto* raw = c.get(); p.addAndMakeVisible(*raw); panelItems[&p].push_back(raw); choices.push_back(std::move(c)); return raw; }
CamcorderEngineAudioProcessorEditor::Switch* CamcorderEngineAudioProcessorEditor::addSwitch(Panel& p, const juce::String& id, const juce::String& text, const juce::String& hint, bool linked)
{ auto c = std::make_unique<Switch>(apvts, id, text, [this, linked] { if(suppressPresetChanges)return;if(linked&&processor.legacyMacrosActive())processor.materialiseLegacyMacros();markCustom(false); }); c->setHint(hint); auto* raw = c.get(); p.addAndMakeVisible(*raw); panelItems[&p].push_back(raw); switches.push_back(std::move(c)); return raw; }
void CamcorderEngineAudioProcessorEditor::layoutPanel(Panel& p, int columns)
{ auto area = p.contentBounds(); auto& items = panelItems[&p]; if (items.empty()) return; const auto cols = juce::jmax(1, columns), rows = juce::jmax(1, ((int) items.size() + cols - 1) / cols), w = area.getWidth() / cols, h = area.getHeight() / rows; for (int i = 0; i < (int) items.size(); ++i) items[(std::size_t) i]->setBounds(area.getX() + (i % cols) * w, area.getY() + (i / cols) * h, w, h); }
void CamcorderEngineAudioProcessorEditor::showMode(EditorMode mode) { currentMode = mode; surfacePage.setVisible(mode == EditorMode::simple); advancedPage.setVisible(mode == EditorMode::advanced); performerPage.setVisible(mode == EditorMode::performer); surfaceButton.setToggleState(mode == EditorMode::simple, juce::dontSendNotification); advancedButton.setToggleState(mode == EditorMode::advanced, juce::dontSendNotification); performerButton.setToggleState(mode == EditorMode::performer, juce::dontSendNotification); resized(); }
void CamcorderEngineAudioProcessorEditor::setParameter(const juce::String& id, float value) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value)); }
float CamcorderEngineAudioProcessorEditor::getParameter(const juce::String& id) const { if (const auto* p = apvts.getRawParameterValue(id)) return p->load(); return 0; }
void CamcorderEngineAudioProcessorEditor::resetParameters() { for (auto* p : processor.getParameters()) p->setValueNotifyingHost(p->getDefaultValue()); }
void CamcorderEngineAudioProcessorEditor::applyPreset(int index) { if (index < 0 || index >= (int) std::size(presets)) return; suppressPresetChanges = true; resetParameters();for(const auto& s : presets[index].values) if(isCamcorderMacroInput(s.first))setParameter(s.first,s.second);setParameter("macroLink",1);processor.materialiseLegacyMacros();for(const auto& s : presets[index].values)if(!isCamcorderMacroInput(s.first)&&juce::String(s.first)!="macroLink")setParameter(s.first,s.second);setParameter("macroLink",0);apvts.state.setProperty("factoryPresetName",presets[index].name,nullptr);suppressPresetChanges = false; }
void CamcorderEngineAudioProcessorEditor::markCustom(bool canonical) { if (canonical) processor.materialiseLegacyMacros(); if (!suppressPresetChanges) { presetBox.setSelectedId(1, juce::dontSendNotification);apvts.state.setProperty("factoryPresetName","Custom",nullptr); } }
void CamcorderEngineAudioProcessorEditor::timerCallback()
{
    viewfinder.setState(processor.outputTrace(), processor.inputPeak(0), processor.inputPeak(1), processor.outputPeak(0), processor.outputPeak(1), processor.windActive(), processor.handlingActive(), processor.dropoutActive(), processor.corruptionActive(), getParameter("wind") > .5f,
        processor.dropoutProgress(), processor.corruptionProgress(), processor.handlingProgress(), processor.windProgress(), processor.agcActivity(), processor.flutterActivity(), processor.limiterActivity(), processor.cameraBedActivity(), processor.windBedActivity(), (int) getParameter("format"));
    if (processor.dropoutActive()) statusLabel.setText("CAPTURE LOST / CONCEALMENT ACTIVE", juce::dontSendNotification); else if (processor.windActive()) statusLabel.setText("WIND IMPACT / CAPSULE AGC", juce::dontSendNotification);
    else if (processor.handlingActive()) statusLabel.setText("CAMERA BODY CONTACT", juce::dontSendNotification); else if (processor.corruptionActive()) statusLabel.setText("FORMAT ERROR", juce::dontSendNotification);
    else statusLabel.setText(processor.legacyMacrosActive() ? "LEGACY SESSION / SOUND PRESERVED" : "CANONICAL 3 MS STEREO CAPTURE", juce::dontSendNotification);
}
void CamcorderEngineAudioProcessorEditor::paint(juce::Graphics& g)
{ g.fillAll(juce::Colour(ink)); auto b = getLocalBounds().toFloat().reduced(7); g.setColour(juce::Colour(deep)); g.fillRoundedRectangle(b, 7); g.setColour(juce::Colour(line)); g.drawRoundedRectangle(b, 7, 1); g.setColour(juce::Colour(amber)); g.fillRect(16.0f, 7.0f, 190.0f, 2.0f); g.setColour(juce::Colour(rec)); g.fillRect((float) getWidth() - 206.0f, 7.0f, 190.0f, 2.0f); g.setColour(juce::Colour(0xff393934)); g.fillRect(15, 91, getWidth() - 30, 1); }
void CamcorderEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16), header = area.removeFromTop(94); auto profile = header.removeFromRight(270); auto identity = header;
    brandLabel.setBounds(identity.removeFromTop(18)); titleLabel.setBounds(identity.removeFromTop(35)); subtitleLabel.setBounds(identity.removeFromTop(18));
    profileLabel.setBounds(profile.removeFromTop(17)); presetBox.setBounds(profile.removeFromTop(32));
    auto nav = area.removeFromTop(40); surfaceButton.setBounds(nav.removeFromLeft(108).reduced(0, 4)); nav.removeFromLeft(8); advancedButton.setBounds(nav.removeFromLeft(108).reduced(0, 4)); nav.removeFromLeft(8); performerButton.setBounds(nav.removeFromLeft(118).reduced(0, 4)); statusLabel.setBounds(nav);
    area.removeFromTop(6); auto display = area.removeFromLeft((int) std::round(area.getWidth() * .36f)); viewfinder.setBounds(display.reduced(2).withTrimmedRight(6));
    const auto pages = area.reduced(2); surfacePage.setBounds(pages); advancedPage.setBounds(pages); performerPage.setBounds(pages);
    auto simple = surfacePage.getLocalBounds(); capturePanel.setBounds(simple.removeFromTop((int) std::round(simple.getHeight() * .60f)).reduced(2)); scenePanel.setBounds(simple.reduced(2)); layoutPanel(capturePanel, 3); layoutPanel(scenePanel, 4);
    auto advanced = advancedPage.getLocalBounds(); const auto width = advanced.getWidth() / 3, height = advanced.getHeight() / 2; Panel* panels[] { &tonePanel, &agcPanel, &converterPanel, &damagePanel, &movementPanel, &outputPanel };
    for (int i = 0; i < 6; ++i) panels[i]->setBounds((i % 3) * width, (i / 3) * height, i % 3 == 2 ? advanced.getWidth() - width * 2 : width, i / 3 == 1 ? advanced.getHeight() - height : height);
    layoutPanel(tonePanel, 2); layoutPanel(agcPanel, 2); layoutPanel(converterPanel, 2); layoutPanel(damagePanel, 2); layoutPanel(movementPanel, 3); layoutPanel(outputPanel, 3);
    auto performer = performerPage.getLocalBounds(); auto left = performer.removeFromLeft((int) std::round(performer.getWidth() * .54f)); performerDropPanel.setBounds(left.removeFromTop(left.getHeight() / 2).reduced(2)); performerFaultPanel.setBounds(left.reduced(2));
    const auto rightHeight = performer.getHeight(); performerHandlingPanel.setBounds(performer.removeFromTop((int) std::round(rightHeight * .34f)).reduced(2)); performerBedPanel.setBounds(performer.removeFromTop((int) std::round(rightHeight * .34f)).reduced(2)); performerOutputPanel.setBounds(performer.reduced(2));
    layoutPanel(performerDropPanel, 3); layoutPanel(performerFaultPanel, 3); layoutPanel(performerHandlingPanel, 3); layoutPanel(performerBedPanel, 3); layoutPanel(performerOutputPanel, 3);
}
