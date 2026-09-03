#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr std::uint32_t ink = 0xff080b10, deep = 0xff10151d, panel = 0xff171d27, line = 0xff354151;
constexpr std::uint32_t bone = 0xfff1eee6, dim = 0xffaaaeb3, cyan = 0xff55e8f4, blue = 0xff5997ff;
constexpr std::uint32_t violet = 0xffb578ff, magenta = 0xffff4dc4, danger = 0xffff5b6e, amber = 0xffffb454;
juce::Font font(float size, bool bold = false) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }

struct PresetDef { const char* name; std::initializer_list<std::pair<const char*, float>> values; };
const PresetDef presets[] {
    { "Clean Call", { { "mode", 1.0f }, { "bandwidth", .84f }, { "codec", .08f }, { "dropouts", .01f }, { "jitter", .03f }, { "robot", 0.0f }, { "noise", .025f } } },
    { "Teams Clean", { { "mode", 1.0f }, { "bandwidth", .72f }, { "codec", .14f }, { "dropouts", .035f }, { "jitter", .05f }, { "robot", .015f }, { "noise", .04f } } },
    { "Discord Studio", { { "mode", 0.0f }, { "bandwidth", .90f }, { "codec", .06f }, { "dropouts", .02f }, { "jitter", .03f }, { "robot", .01f }, { "noise", .02f } } },
    { "Podcast Remote", { { "mode", 0.0f }, { "bandwidth", .78f }, { "codec", .18f }, { "dropouts", .04f }, { "jitter", .06f }, { "robot", .02f }, { "noise", .045f } } },
    { "Zoom Soft WiFi", { { "mode", 1.0f }, { "bandwidth", .62f }, { "codec", .28f }, { "dropouts", .18f }, { "jitter", .20f }, { "robot", .13f }, { "noise", .08f } } },
    { "Skype Faint", { { "mode", 2.0f }, { "bandwidth", .48f }, { "codec", .36f }, { "dropouts", .12f }, { "jitter", .14f }, { "robot", .08f }, { "noise", .07f }, { "outGain", .88f } } },
    { "Cell Call Stable", { { "mode", 3.0f }, { "bandwidth", .52f }, { "codec", .32f }, { "dropouts", .09f }, { "jitter", .12f }, { "robot", .07f }, { "noise", .08f } } },
    { "Office Speakerphone", { { "mode", 1.0f }, { "bandwidth", .42f }, { "codec", .30f }, { "dropouts", .05f }, { "jitter", .06f }, { "robot", .02f }, { "noise", .08f } } },
    { "Laptop Mic Meeting", { { "mode", 1.0f }, { "bandwidth", .57f }, { "codec", .25f }, { "dropouts", .08f }, { "jitter", .08f }, { "robot", .04f }, { "noise", .09f } } },
    { "Bluetooth Conference", { { "mode", 3.0f }, { "bandwidth", .38f }, { "codec", .52f }, { "dropouts", .12f }, { "jitter", .16f }, { "robot", .10f }, { "noise", .12f } } },
    { "Discord Packet Spiral", { { "mode", 0.0f }, { "bandwidth", .50f }, { "codec", .52f }, { "dropouts", .58f }, { "jitter", .46f }, { "robot", .36f }, { "noise", .13f }, { "concealMode", 3.0f } } },
    { "Zoom Robot Voice", { { "mode", 1.0f }, { "bandwidth", .52f }, { "codec", .48f }, { "dropouts", .34f }, { "jitter", .32f }, { "robot", .72f }, { "noise", .10f } } },
    { "Skype 2008", { { "mode", 2.0f }, { "bandwidth", .40f }, { "codec", .58f }, { "dropouts", .25f }, { "jitter", .20f }, { "robot", .17f }, { "noise", .11f } } },
    { "Bad Hotspot", { { "mode", 3.0f }, { "bandwidth", .29f }, { "codec", .65f }, { "dropouts", .68f }, { "jitter", .60f }, { "robot", .38f }, { "noise", .18f }, { "concealMode", 3.0f } } },
    { "Packet Storm", { { "mode", 0.0f }, { "bandwidth", .44f }, { "codec", .48f }, { "dropouts", .86f }, { "jitter", .74f }, { "robot", .46f }, { "noise", .16f }, { "concealMode", 2.0f } } },
    { "Conference Meltdown", { { "mode", 0.0f }, { "bandwidth", .20f }, { "codec", .90f }, { "dropouts", .92f }, { "jitter", .88f }, { "robot", .78f }, { "noise", .32f }, { "concealMode", 3.0f }, { "ceiling", .86f } } },
    { "PLAY - Discord Guitar Robot", { { "mode", 0 }, { "bandwidth", .76f }, { "codec", .28f }, { "dropouts", .04f }, { "jitter", .06f }, { "robot", .04f }, { "noise", .018f }, { "macroLink", 1 }, { "robotTempoSync", 1 }, { "robotDivision", 3 }, { "robotProbability", .24f }, { "robotStrength", .48f }, { "robotLengthSync", 1 }, { "robotLengthDivision", 5 }, { "robotGrainMs", 18 }, { "mix", .78f }, { "ceiling", .90f }, { "outGain", .94f } } },
    { "PLAY - Zoom Drum Stutter", { { "mode", 1 }, { "bandwidth", .66f }, { "codec", .34f }, { "dropouts", .03f }, { "jitter", .05f }, { "robot", .02f }, { "noise", .015f }, { "macroLink", 1 }, { "packetTempoSync", 1 }, { "packetDivision", 3 }, { "packetProbability", .26f }, { "packetDepth", .46f }, { "packetLengthSync", 1 }, { "packetLengthDivision", 5 }, { "mix", .72f }, { "ceiling", .88f }, { "outGain", .94f } } },
    { "PLAY - Codec Synth Pulse", { { "mode", 3 }, { "bandwidth", .54f }, { "codec", .42f }, { "dropouts", .02f }, { "jitter", .04f }, { "robot", .01f }, { "noise", .012f }, { "macroLink", 1 }, { "packetTempoSync", 1 }, { "packetDivision", 4 }, { "packetProbability", .20f }, { "packetDepth", .34f }, { "packetLengthSync", 1 }, { "packetLengthDivision", 5 }, { "mix", .68f }, { "ceiling", .90f }, { "outGain", .94f } } },
    { "PLAY - Clean Remote Parallel", { { "mode", 0 }, { "bandwidth", .88f }, { "codec", .10f }, { "dropouts", .005f }, { "jitter", .01f }, { "robot", 0 }, { "noise", .008f }, { "macroLink", 1 }, { "mix", .48f }, { "ceiling", .92f }, { "outGain", .98f } } },
};
}

void ConferenceEngineAudioProcessorEditor::Panel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(juce::Colour(panel)); g.fillRoundedRectangle(bounds, 7.0f);
    g.setColour(juce::Colour(line)); g.drawRoundedRectangle(bounds, 7.0f, 1.0f);
    g.setColour(juce::Colour(cyan)); g.fillRect(bounds.getX() + 12.0f, bounds.getY() + 12.0f, 24.0f, 1.5f);
    g.setColour(juce::Colour(dim)); g.setFont(font(9.0f, true));
    g.drawText(name, getLocalBounds().removeFromTop(32).withTrimmedLeft(44), juce::Justification::centredLeft);
}

ConferenceEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    label.setText(text, juce::dontSendNotification); label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(bone)); label.setFont(font(9.0f, true)); addAndMakeVisible(label);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(cyan)); slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff3b4654));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(bone)); slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(bone));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(ink)); slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(line));
    slider.onDragStart = std::move(changed); addAndMakeVisible(slider); attachment = std::make_unique<APVTS::SliderAttachment>(state, id, slider);
}
void ConferenceEngineAudioProcessorEditor::Knob::resized() { auto area = getLocalBounds(); label.setBounds(area.removeFromTop(17)); slider.setBounds(area.reduced(1)); }

ConferenceEngineAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    label.setText(text, juce::dontSendNotification); label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(bone)); label.setFont(font(9.0f, true)); addAndMakeVisible(label);
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id)))
        for (int i = 0; i < parameter->choices.size(); ++i) combo.addItem(parameter->choices[i], i + 1);
    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(ink)); combo.setColour(juce::ComboBox::textColourId, juce::Colour(bone));
    combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(line)); combo.onChange = std::move(changed); addAndMakeVisible(combo);
    attachment = std::make_unique<APVTS::ComboBoxAttachment>(state, id, combo);
}
void ConferenceEngineAudioProcessorEditor::Choice::resized() { auto area = getLocalBounds(); label.setBounds(area.removeFromTop(16)); combo.setBounds(area.removeFromTop(26).reduced(1)); }

ConferenceEngineAudioProcessorEditor::Switch::Switch(APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    button.setButtonText(text); button.setColour(juce::ToggleButton::textColourId, juce::Colour(bone)); button.onClick = std::move(changed);
    addAndMakeVisible(button); attachment = std::make_unique<APVTS::ButtonAttachment>(state, id, button);
}
void ConferenceEngineAudioProcessorEditor::Switch::resized() { button.setBounds(getLocalBounds().reduced(3)); }

void ConferenceEngineAudioProcessorEditor::CallDisplay::setState(const std::array<float, 64>& trace,
    float inL, float inR, float outL, float outR, bool loss, bool bot, bool bufferSlip, bool bandwidth,
    float packetPhase, float botPhase, float jitterValue, float suppress, float agcValue, float comfortValue,
    float limiterValue, int mode)
{
    waveform = trace; input = { inL, inR }; output = { outL, outR }; lost = loss; robot = bot; slip = bufferSlip;
    narrow = bandwidth; lossProgress = packetPhase; robotProgress = botPhase; jitter = jitterValue;
    suppression = suppress; agc = agcValue; comfort = comfortValue; limiter = limiterValue; platform = mode; repaint();
}

void ConferenceEngineAudioProcessorEditor::CallDisplay::paint(juce::Graphics& g)
{
    auto outer = getLocalBounds().toFloat().reduced(1.0f); g.setColour(juce::Colour(0xff0c1219)); g.fillRoundedRectangle(outer, 8.0f);
    g.setColour(juce::Colour(line)); g.drawRoundedRectangle(outer, 8.0f, 1.0f);
    auto top = outer.reduced(14.0f).removeFromTop(30.0f); const char* names[] { "DISCORD", "ZOOM", "SKYPE", "CELLULAR" };
    g.setFont(font(9.5f, true)); g.setColour(juce::Colour(cyan)); g.drawText("REMOTE SESSION / LIVE DECODE", top.removeFromLeft(top.getWidth() * .68f), juce::Justification::centredLeft);
    g.setColour(juce::Colour(dim)); g.drawText(names[juce::jlimit(0, 3, platform)], top, juce::Justification::centredRight);
    auto screen = outer.reduced(14.0f).withTrimmedTop(42.0f).withTrimmedBottom(188.0f);
    g.setColour(juce::Colour(0xff111b26)); g.fillRoundedRectangle(screen, 5.0f); g.setColour(juce::Colour(0xff26384a));
    for (int i = 1; i < 5; ++i) g.drawHorizontalLine((int) (screen.getY() + i * screen.getHeight() / 5.0f), screen.getX(), screen.getRight());
    juce::Path path;
    for (int i = 0; i < (int) waveform.size(); ++i)
    {
        const auto x = screen.getX() + (float) i * screen.getWidth() / (float) (waveform.size() - 1);
        const auto y = screen.getCentreY() - juce::jlimit(0.0f, 1.0f, waveform[(std::size_t) i] * 5.0f) * screen.getHeight() * 0.42f;
        if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
    }
    g.setColour(juce::Colour(lost ? magenta : robot ? violet : cyan)); g.strokePath(path, juce::PathStrokeType(1.7f));
    if (lost || robot)
    {
        const auto progress = lost ? lossProgress : robotProgress;
        g.setColour(juce::Colour(lost ? magenta : violet).withAlpha(0.18f));
        g.fillRect(screen.withWidth(screen.getWidth() * juce::jlimit(0.0f, 1.0f, progress)));
    }

    auto lower = outer.reduced(14.0f).removeFromBottom(172.0f); auto levels = lower.removeFromTop(48.0f);
    const float io[] { input[0], input[1], output[0], output[1] }; const char* ioNames[] { "IN L", "IN R", "OUT L", "OUT R" };
    for (int i = 0; i < 4; ++i)
    {
        auto row = levels.removeFromTop(12.0f); g.setColour(juce::Colour(dim)); g.setFont(font(7.5f, true));
        g.drawText(ioNames[i], row.removeFromLeft(34.0f), juce::Justification::centredLeft); auto bar = row.reduced(1.0f, 3.0f);
        g.setColour(juce::Colour(0xff303b48)); g.fillRect(bar); g.setColour(juce::Colour(i < 2 ? blue : cyan));
        g.fillRect(bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, io[i] * 2.4f)));
    }
    lower.removeFromTop(8.0f);
    const float activity[] { lossProgress, robotProgress, jitter, suppression, agc, comfort, limiter };
    const char* activityNames[] { "PACKET", "ROBOT", "JITTER", "SUPPRESS", "AGC", "COMFORT", "LIMIT" };
    const std::uint32_t activityColours[] { magenta, violet, blue, cyan, amber, violet, danger };
    for (int i = 0; i < 7; ++i)
    {
        auto row = lower.removeFromTop(14.0f); g.setColour(juce::Colour(dim)); g.setFont(font(7.4f, true));
        g.drawText(activityNames[i], row.removeFromLeft(52.0f), juce::Justification::centredLeft); auto bar = row.reduced(1.0f, 4.0f);
        g.setColour(juce::Colour(0xff303b48)); g.fillRect(bar); g.setColour(juce::Colour(activityColours[i]));
        g.fillRect(bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, activity[i])));
    }
    auto lamps = outer.reduced(14.0f).removeFromBottom(22.0f); const struct { const char* name; bool on; } states[] {
        { "LOSS", lost }, { "ROBOT", robot }, { "SLIP", slip }, { "BW", narrow }
    };
    for (const auto& state : states)
    {
        auto cell = lamps.removeFromLeft(lamps.getWidth() / 4.0f); g.setColour(juce::Colour(state.on ? magenta : 0xff535c67));
        g.fillEllipse(cell.getX(), cell.getCentreY() - 3.0f, 6.0f, 6.0f); g.setColour(juce::Colour(bone)); g.setFont(font(7.4f, true));
        g.drawText(state.name, cell.withTrimmedLeft(10.0f), juce::Justification::centredLeft);
    }
}

ConferenceEngineAudioProcessorEditor::ConferenceEngineAudioProcessorEditor(ConferenceEngineAudioProcessor& owner)
    : AudioProcessorEditor(&owner), processor(owner), apvts(owner.getAPVTS())
{
    setOpaque(true);
    brandLabel.setText("B&E DIGITAL", juce::dontSendNotification); brandLabel.setColour(juce::Label::textColourId, juce::Colour(cyan)); brandLabel.setFont(font(10, true)); addAndMakeVisible(brandLabel);
    titleLabel.setText("CONFERENCE ENGINE", juce::dontSendNotification); titleLabel.setColour(juce::Label::textColourId, juce::Colour(bone)); titleLabel.setFont(font(25, true)); addAndMakeVisible(titleLabel);
    subtitleLabel.setText("REAL-TIME CODEC FAILURE / V3 STEREO", juce::dontSendNotification); subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(dim)); subtitleLabel.setFont(font(9.5f, true)); addAndMakeVisible(subtitleLabel);
    profileLabel.setText("PROFILE", juce::dontSendNotification); profileLabel.setColour(juce::Label::textColourId, juce::Colour(dim)); profileLabel.setFont(font(9, true)); addAndMakeVisible(profileLabel);
    presetBox.addItem("Custom", 1); for (int i = 0; i < (int) std::size(presets); ++i) presetBox.addItem(presets[i].name, i + 2);
    const auto restoredPreset = apvts.state.getProperty("factoryPresetName", "Custom").toString(); auto restoredPresetId = 1; for (int i = 0; i < (int) std::size(presets); ++i) if (restoredPreset == presets[i].name) restoredPresetId = i + 2;
    presetBox.setSelectedId(restoredPresetId, juce::dontSendNotification); presetBox.onChange = [this] { if (presetBox.getSelectedId() >= 2) applyPreset(presetBox.getSelectedId() - 2); }; addAndMakeVisible(presetBox);
    for (auto* button : { &simpleButton, &advancedButton, &performerButton }) { button->setColour(juce::TextButton::textColourOffId, juce::Colour(bone)); addAndMakeVisible(*button); }
    simpleButton.onClick = [this] { showMode(EditorMode::simple); }; advancedButton.onClick = [this] { showMode(EditorMode::advanced); };
    performerButton.onClick = [this] { showMode(EditorMode::performer); };
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(dim)); statusLabel.setFont(font(8.5f, true)); statusLabel.setJustificationType(juce::Justification::centredRight); addAndMakeVisible(statusLabel);
    addAndMakeVisible(callDisplay); addAndMakeVisible(simplePage); addAndMakeVisible(advancedPage); addAndMakeVisible(performerPage);
    for (auto* p : { &simpleCallPanel, &simpleOutputPanel }) simplePage.addAndMakeVisible(*p);
    for (auto* p : { &tonePanel, &packetPanel, &bufferPanel, &cleanupPanel, &codecPanel, &safetyPanel }) advancedPage.addAndMakeVisible(*p);
    for (auto* p : { &performerPacketPanel, &performerRobotPanel, &performerClockPanel, &performerCleanupPanel, &performerOutputPanel }) performerPage.addAndMakeVisible(*p);

    addChoice(simpleCallPanel, "mode", "PLATFORM", "Select codec family and transport character.");
    addChoice(simpleCallPanel, "concealMode", "MISSING AUDIO", "How lost frames are reconstructed.");
    addKnob(simpleCallPanel, "hpHz", "LOW CUT", "Voice-band low edge."); addKnob(simpleCallPanel, "lpHz", "HIGH CUT", "Voice-band high edge.");
    addKnob(simpleCallPanel, "bits", "CODEC BITS", "Quantizer resolution."); addKnob(simpleCallPanel, "rate", "CODEC RATE", "Decoded sample-and-hold rate.");
    addKnob(simpleCallPanel, "packetLoss", "FREE LOSS", "Free-running packet failure probability."); addKnob(simpleCallPanel, "robot", "FREE ROBOT", "Free-running repeated speech grain probability.");
    addKnob(simpleCallPanel, "noise", "CALL BED", "Coded background and comfort-noise bed.");
    addKnob(simpleOutputPanel, "inputGain", "INPUT", "Trim before encoding."); addKnob(simpleOutputPanel, "mix", "MIX", "Latency-aligned dry/wet blend.");
    addKnob(simpleOutputPanel, "outGain", "OUTPUT", "Final return level."); addKnob(simpleOutputPanel, "ceiling", "CEILING", "Protected output ceiling.");

    addKnob(tonePanel, "hpHz", "HIGH-PASS", "Low speech cutoff."); addKnob(tonePanel, "lpHz", "LOW-PASS", "High speech cutoff.");
    addKnob(tonePanel, "midHumpDb", "PRESENCE", "Speech presence emphasis."); addKnob(tonePanel, "midFreq", "CENTER", "Presence center frequency.");
    addChoice(packetPanel, "concealMode", "CONCEALMENT", "Hold, mute, interpolate, or repeat missing frames."); addKnob(packetPanel, "packetLoss", "FREE LOSS", "Per-frame free loss probability.");
    addKnob(packetPanel, "packetMs", "FRAME ms", "Codec frame duration."); addKnob(packetPanel, "repeatMs", "REPEAT ms", "PLC history loop length."); addKnob(packetPanel, "burstiness", "BURST MEMORY", "Chance a bad frame continues.");
    addKnob(bufferPanel, "jitterMs", "WANDER ms", "Buffered speech timing wander."); addKnob(bufferPanel, "jitterRate", "FREE RATE", "Unclocked buffer target update rate.");
    addKnob(bufferPanel, "bufferSlip", "BUFFER SLIP", "Duplicate or skip speech chunks."); addKnob(bufferPanel, "bandwidthSwitch", "BW COLLAPSE", "Temporary narrow-band fallback.");
    addKnob(cleanupPanel, "gate", "VOICE GATE", "Voice activity threshold."); addKnob(cleanupPanel, "suppression", "SUPPRESSION", "Decoded noise suppression depth.");
    addKnob(cleanupPanel, "agc", "AUTO GAIN", "Conference loudness normalization."); addKnob(cleanupPanel, "comfortNoise", "COMFORT", "Synthesized noise under gates and loss.");
    addKnob(codecPanel, "bits", "BITS", "Codec quantizer resolution."); addKnob(codecPanel, "rate", "RATE", "Decoded sample-and-hold rate.");
    addKnob(codecPanel, "robot", "FREE ROBOT", "Free repeated-grain probability."); addKnob(codecPanel, "noise", "NOISE BED", "Codec and comfort-noise amount.");
    addKnob(safetyPanel, "inputGain", "INPUT", "Input trim before encoding."); addKnob(safetyPanel, "mix", "MIX", "Latency-aligned blend.");
    addKnob(safetyPanel, "outGain", "OUTPUT", "Final return gain."); addKnob(safetyPanel, "ceiling", "CEILING", "Output protection ceiling.");

    addSwitch(performerPacketPanel, "packetTempoSync", "SYNC LOSS", "Replace random packet loss with host-grid events.");
    addChoice(performerPacketPanel, "packetDivision", "TRIGGER GRID", "Where packet failures may begin."); addKnob(performerPacketPanel, "packetProbability", "PROBABILITY", "Stable chance per grid step.");
    addKnob(performerPacketPanel, "packetDepth", "FAIL DEPTH", "Blend between healthy and concealed frame."); addKnob(performerPacketPanel, "packetDurationMs", "LENGTH ms", "Free/manual failure length.");
    addSwitch(performerPacketPanel, "packetLengthSync", "SYNC LENGTH", "Use musical packet-failure length."); addChoice(performerPacketPanel, "packetLengthDivision", "FAIL LENGTH", "Musical failure duration.");
    packetTriggerButton.onClick = [this] { processor.triggerPacketLoss(); markCustom(true); }; packetTriggerButton.setTooltip("Trigger one protected packet failure now.");
    performerPacketPanel.addAndMakeVisible(packetTriggerButton); panelItems[&performerPacketPanel].push_back(&packetTriggerButton);

    addSwitch(performerRobotPanel, "robotTempoSync", "SYNC ROBOT", "Replace random robot grains with host-grid captures.");
    addChoice(performerRobotPanel, "robotDivision", "TRIGGER GRID", "Where robotic capture may begin."); addKnob(performerRobotPanel, "robotProbability", "PROBABILITY", "Stable chance per grid step.");
    addKnob(performerRobotPanel, "robotStrength", "STRENGTH", "Blend of captured loop and live decoder."); addKnob(performerRobotPanel, "robotGrainMs", "GRAIN ms", "Length of captured speech fragment.");
    addKnob(performerRobotPanel, "robotDurationMs", "LENGTH ms", "Free/manual event length."); addSwitch(performerRobotPanel, "robotLengthSync", "SYNC LENGTH", "Use musical robot duration.");
    addChoice(performerRobotPanel, "robotLengthDivision", "ROBOT LENGTH", "Musical robot event duration.");
    robotTriggerButton.onClick = [this] { processor.triggerRobot(); markCustom(true); }; robotTriggerButton.setTooltip("Capture and repeat the current decoder grain.");
    performerRobotPanel.addAndMakeVisible(robotTriggerButton); panelItems[&performerRobotPanel].push_back(&robotTriggerButton);

    addKnob(performerClockPanel, "jitterMs", "WANDER ms", "Jitter-buffer motion depth."); addKnob(performerClockPanel, "jitterRate", "FREE RATE", "Unclocked update rate.");
    addSwitch(performerClockPanel, "jitterTempoSync", "SYNC JITTER", "Clock buffer updates to the host."); addChoice(performerClockPanel, "jitterDivision", "UPDATE GRID", "Clocked buffer update cycle.");
    addKnob(performerClockPanel, "bufferSlip", "SLIP", "Duplicate or skip chunks."); addKnob(performerClockPanel, "bandwidthSwitch", "NARROW", "Temporary bandwidth fallback.");
    addChoice(performerCleanupPanel, "concealMode", "CONCEAL", "Missing-frame reconstruction style."); addKnob(performerCleanupPanel, "gate", "GATE", "Voice activity threshold.");
    addKnob(performerCleanupPanel, "suppression", "SUPPRESS", "Noise suppression depth."); addKnob(performerCleanupPanel, "agc", "AUTO GAIN", "Automatic gain strength.");
    addKnob(performerCleanupPanel, "comfortNoise", "COMFORT", "Generated coded noise level.");
    addKnob(performerOutputPanel, "inputGain", "INPUT", "Input trim."); addKnob(performerOutputPanel, "mix", "MIX", "Latency-aligned dry/wet.");
    addKnob(performerOutputPanel, "outGain", "OUTPUT", "Return gain."); addKnob(performerOutputPanel, "ceiling", "CEILING", "Hard safety ceiling.");

    setResizable(true, true); setResizeLimits(980, 660, 1600, 1000); setSize(1240, 760); showMode(EditorMode::simple); startTimerHz(30);
}

ConferenceEngineAudioProcessorEditor::~ConferenceEngineAudioProcessorEditor() { stopTimer(); }

ConferenceEngineAudioProcessorEditor::Knob* ConferenceEngineAudioProcessorEditor::addKnob(Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint, bool canonical)
{
    auto control = std::make_unique<Knob>(apvts, id, text, [this, canonical] { markCustom(canonical); }); control->setHint(hint);
    auto* result = control.get(); owner.addAndMakeVisible(*result); panelItems[&owner].push_back(result); knobs.push_back(std::move(control)); return result;
}
ConferenceEngineAudioProcessorEditor::Choice* ConferenceEngineAudioProcessorEditor::addChoice(Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint, bool canonical)
{
    auto control = std::make_unique<Choice>(apvts, id, text, [this, canonical] { markCustom(canonical); }); control->setHint(hint);
    auto* result = control.get(); owner.addAndMakeVisible(*result); panelItems[&owner].push_back(result); choices.push_back(std::move(control)); return result;
}
ConferenceEngineAudioProcessorEditor::Switch* ConferenceEngineAudioProcessorEditor::addSwitch(Panel& owner, const juce::String& id, const juce::String& text, const juce::String& hint)
{
    auto control = std::make_unique<Switch>(apvts, id, text, [this] { markCustom(true); }); control->setHint(hint);
    auto* result = control.get(); owner.addAndMakeVisible(*result); panelItems[&owner].push_back(result); switches.push_back(std::move(control)); return result;
}

void ConferenceEngineAudioProcessorEditor::layoutPanel(Panel& owner, int columns)
{
    auto area = owner.contentBounds(); auto& items = panelItems[&owner]; if (items.empty()) return;
    const auto cols = juce::jmax(1, columns), rows = juce::jmax(1, ((int) items.size() + cols - 1) / cols);
    const auto width = area.getWidth() / cols, height = area.getHeight() / rows;
    for (int i = 0; i < (int) items.size(); ++i)
        items[(std::size_t) i]->setBounds(area.getX() + (i % cols) * width, area.getY() + (i / cols) * height, width, height);
}

void ConferenceEngineAudioProcessorEditor::showMode(EditorMode mode)
{
    currentMode = mode; simplePage.setVisible(mode == EditorMode::simple); advancedPage.setVisible(mode == EditorMode::advanced); performerPage.setVisible(mode == EditorMode::performer);
    simpleButton.setToggleState(mode == EditorMode::simple, juce::dontSendNotification); advancedButton.setToggleState(mode == EditorMode::advanced, juce::dontSendNotification);
    performerButton.setToggleState(mode == EditorMode::performer, juce::dontSendNotification); resized();
}
void ConferenceEngineAudioProcessorEditor::setParameter(const juce::String& id, float value)
{ if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value)); }
float ConferenceEngineAudioProcessorEditor::getParameter(const juce::String& id) const
{ if (const auto* p = apvts.getRawParameterValue(id)) return p->load(); return 0.0f; }
void ConferenceEngineAudioProcessorEditor::resetParameters()
{ for (auto* parameter : processor.getParameters()) parameter->setValueNotifyingHost(parameter->getDefaultValue()); }
void ConferenceEngineAudioProcessorEditor::applyPreset(int index)
{
    if (index < 0 || index >= (int) std::size(presets)) return;
    suppressPresetChanges = true; resetParameters();
    for (const auto& setting : presets[index].values) if (juce::String(setting.first) != "macroLink") setParameter(setting.first, setting.second);
    setParameter("macroLink", 1.0f);
    processor.materialiseLegacyMacros();
    for (const auto& setting : presets[index].values)
    {
        const auto id = juce::String(setting.first);
        if (id != "bandwidth" && id != "codec" && id != "dropouts" && id != "jitter" && id != "robot" && id != "noise" && id != "macroLink")
            setParameter(id, setting.second);
    }
    setParameter("macroLink", 0.0f);
    apvts.state.setProperty("factoryPresetName", presets[index].name, nullptr);
    suppressPresetChanges = false;
}
void ConferenceEngineAudioProcessorEditor::markCustom(bool canonical)
{
    if (canonical) processor.materialiseLegacyMacros();
    if (!suppressPresetChanges) { presetBox.setSelectedId(1, juce::dontSendNotification); apvts.state.setProperty("factoryPresetName", "Custom", nullptr); }
}

void ConferenceEngineAudioProcessorEditor::timerCallback()
{
    callDisplay.setState(processor.outputTrace(), processor.inputPeak(0), processor.inputPeak(1), processor.outputPeak(0), processor.outputPeak(1),
        processor.packetLost(), processor.robotActive(), processor.bufferSlipActive(), processor.bandwidthCollapsed(), processor.packetProgress(), processor.robotProgress(),
        processor.jitterActivity(), processor.suppressionActivity(), processor.agcActivity(), processor.comfortNoiseActivity(), processor.limiterActivity(), (int) getParameter("mode"));
    if (processor.packetLost()) statusLabel.setText("FRAME LOST / PLC CONCEALMENT ACTIVE", juce::dontSendNotification);
    else if (processor.robotActive()) statusLabel.setText("CAPTURED SPEECH GRAIN REPEATING", juce::dontSendNotification);
    else if (processor.bufferSlipActive()) statusLabel.setText("JITTER BUFFER SLIP", juce::dontSendNotification);
    else if (processor.legacyMacrosActive()) statusLabel.setText("LEGACY SESSION / SOUND PRESERVED", juce::dontSendNotification);
    else statusLabel.setText("LIVE 2 MS STEREO CALL PATH", juce::dontSendNotification);
}

void ConferenceEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ink)); auto header = getLocalBounds().removeFromTop(112).toFloat(); g.setColour(juce::Colour(deep)); g.fillRect(header);
    g.setColour(juce::Colour(cyan)); g.fillRect(16.0f, 0.0f, 190.0f, 2.0f); g.setColour(juce::Colour(magenta)); g.fillRect((float) getWidth() - 150.0f, 0.0f, 134.0f, 2.0f);
    g.setColour(juce::Colour(0xff343a31)); g.drawHorizontalLine(111, 0.0f, (float) getWidth()); g.setColour(juce::Colour(0xff171a16).withAlpha(0.62f));
    for (int y = 112; y < getHeight(); y += 4) g.drawHorizontalLine(y, 0.0f, (float) getWidth());
}

void ConferenceEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16); auto header = area.removeFromTop(94); auto profile = header.removeFromRight(270);
    profileLabel.setBounds(profile.removeFromTop(17)); presetBox.setBounds(profile.removeFromTop(32)); auto identity = header;
    brandLabel.setBounds(identity.removeFromTop(18)); titleLabel.setBounds(identity.removeFromTop(35)); subtitleLabel.setBounds(identity.removeFromTop(18));
    auto modeRow = area.removeFromTop(40); simpleButton.setBounds(modeRow.removeFromLeft(108).reduced(0, 4)); modeRow.removeFromLeft(8);
    advancedButton.setBounds(modeRow.removeFromLeft(108).reduced(0, 4)); modeRow.removeFromLeft(8); performerButton.setBounds(modeRow.removeFromLeft(118).reduced(0, 4)); statusLabel.setBounds(modeRow);
    area.removeFromTop(6); auto display = area.removeFromLeft((int) std::round(area.getWidth() * 0.36f)); callDisplay.setBounds(display.reduced(2).withTrimmedRight(6));
    auto pages = area.reduced(2); simplePage.setBounds(pages); advancedPage.setBounds(pages); performerPage.setBounds(pages);

    auto simple = simplePage.getLocalBounds(); simpleCallPanel.setBounds(simple.removeFromTop((int) std::round(simple.getHeight() * .68f)).reduced(2));
    simpleOutputPanel.setBounds(simple.reduced(2)); layoutPanel(simpleCallPanel, 3); layoutPanel(simpleOutputPanel, 4);

    auto advanced = advancedPage.getLocalBounds(); const auto halfW = advanced.getWidth() / 3, halfH = advanced.getHeight() / 2;
    Panel* advancedPanels[] { &tonePanel, &packetPanel, &bufferPanel, &cleanupPanel, &codecPanel, &safetyPanel };
    for (int i = 0; i < 6; ++i) advancedPanels[i]->setBounds((i % 3) * halfW, (i / 3) * halfH, i % 3 == 2 ? advanced.getWidth() - halfW * 2 : halfW, i / 3 == 1 ? advanced.getHeight() - halfH : halfH);
    layoutPanel(tonePanel, 2); layoutPanel(packetPanel, 2); layoutPanel(bufferPanel, 2); layoutPanel(cleanupPanel, 2); layoutPanel(codecPanel, 2); layoutPanel(safetyPanel, 2);

    auto performer = performerPage.getLocalBounds(); auto left = performer.removeFromLeft((int) std::round(performer.getWidth() * .53f));
    performerPacketPanel.setBounds(left.removeFromTop(left.getHeight() / 2).reduced(2)); performerRobotPanel.setBounds(left.reduced(2));
    const auto rightHeight = performer.getHeight(); performerClockPanel.setBounds(performer.removeFromTop((int) std::round(rightHeight * .34f)).reduced(2));
    performerCleanupPanel.setBounds(performer.removeFromTop((int) std::round(rightHeight * .34f)).reduced(2)); performerOutputPanel.setBounds(performer.reduced(2));
    layoutPanel(performerPacketPanel, 3); layoutPanel(performerRobotPanel, 3); layoutPanel(performerClockPanel, 3);
    layoutPanel(performerCleanupPanel, 3); layoutPanel(performerOutputPanel, 4);
}
