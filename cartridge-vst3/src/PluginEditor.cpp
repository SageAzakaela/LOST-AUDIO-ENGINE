#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr std::uint32_t ink = 0xff0b100d, deep = 0xff141b16, panel = 0xff1c241d, line = 0xff4d5b49;
constexpr std::uint32_t cream = 0xffeee2bd, dim = 0xffa49e86, lime = 0xffa9da73, amber = 0xffffb94d, magenta = 0xffff4db8;
juce::Font font(float size, bool bold = false) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }

struct Setting { const char* id; float value; };
struct Preset { const char* name; std::initializer_list<Setting> values; };
const Preset presets[] {
    { "Game Boy Handheld", { { "codecMode", 1 }, { "speakerModel", 1 }, { "quality", .20f }, { "codec", .32f }, { "grit", .22f }, { "noise", .08f } } },
    { "NES DPCM", { { "codecMode", 1 }, { "speakerModel", 2 }, { "quality", .15f }, { "codec", .42f }, { "grit", .20f }, { "noise", .05f } } },
    { "SNES BRR", { { "codecMode", 3 }, { "speakerModel", 0 }, { "quality", .52f }, { "codec", .35f }, { "grit", .12f }, { "noise", .035f }, { "noiseShaping", 1 } } },
    { "PS1 ADPCM", { { "codecMode", 2 }, { "speakerModel", 2 }, { "quality", .38f }, { "codec", .62f }, { "grit", .18f }, { "noise", .04f } } },
    { "GBA Pocket", { { "codecMode", 1 }, { "speakerModel", 1 }, { "quality", .30f }, { "codec", .48f }, { "grit", .28f }, { "noise", .07f } } },
    { "Genesis Television", { { "codecMode", 0 }, { "speakerModel", 2 }, { "quality", .32f }, { "codec", .18f }, { "grit", .40f }, { "noise", .08f } } },
    { "PC Speaker", { { "codecMode", 0 }, { "speakerModel", 4 }, { "quality", .10f }, { "codec", .12f }, { "grit", .36f }, { "noise", .04f } } },
    { "Tracker Direct", { { "codecMode", 0 }, { "speakerModel", 0 }, { "quality", .58f }, { "codec", .20f }, { "grit", .08f }, { "noise", .015f } } },
    { "Arcade Cabinet", { { "codecMode", 1 }, { "speakerModel", 3 }, { "quality", .28f }, { "codec", .32f }, { "grit", .62f }, { "noise", .06f } } },
    { "Crystal Chip", { { "codecMode", 3 }, { "speakerModel", 0 }, { "quality", .68f }, { "codec", .22f }, { "grit", .08f }, { "noise", .01f }, { "noiseShaping", 1 } } },
    { "Cutscene ADPCM", { { "codecMode", 2 }, { "speakerModel", 2 }, { "quality", .40f }, { "codec", .72f }, { "grit", .18f }, { "noise", .06f }, { "verb", .12f } } },
    { "Overclocked Cart", { { "codecMode", 3 }, { "speakerModel", 3 }, { "quality", .76f }, { "codec", .52f }, { "grit", .72f }, { "noise", .07f } } },
    { "Handheld Clean", { { "codecMode", 0 }, { "speakerModel", 1 }, { "quality", .78f }, { "codec", .08f }, { "grit", .08f }, { "noise", .015f } } },
    { "Cartridge Room", { { "codecMode", 2 }, { "speakerModel", 3 }, { "quality", .45f }, { "codec", .38f }, { "grit", .32f }, { "noise", .06f }, { "macroLink", 0 }, { "verb", .35f }, { "verbMs", 72 }, { "microDelayMix", .22f } } },
    { "Chip Voice Demo", { { "codecMode", 1 }, { "speakerModel", 1 }, { "quality", .24f }, { "codec", .34f }, { "grit", .24f }, { "noise", .04f }, { "bleepsEnable", 1 }, { "bleepsMix", .20f }, { "bleepsRate", 4.5f }, { "bleepsWave", 0 }, { "bleepsTrigger", 2 }, { "bleepsScale", 0 } } },
    { "Damaged Save Memory", { { "codecMode", 3 }, { "speakerModel", 2 }, { "quality", .12f }, { "codec", .88f }, { "grit", .76f }, { "noise", .12f }, { "limiter", .88f }, { "ceiling", .82f }, { "outGain", .90f } } },
    { "PLAY - Tracker Guitar", { { "codecMode", 0 }, { "speakerModel", 2 }, { "quality", .46f }, { "codec", .32f }, { "grit", .40f }, { "noise", .015f }, { "dither", 0 }, { "bits", 10 }, { "rate", 18000 }, { "hpHz", 80 }, { "lpHz", 7600 }, { "sat", .40f }, { "edge", .38f }, { "speaker", .70f }, { "microDelayMix", .09f }, { "wet", .78f }, { "limiter", .88f }, { "ceiling", .90f }, { "outGain", .94f } } },
    { "PLAY - Drum ROM Crunch", { { "codecMode", 1 }, { "speakerModel", 3 }, { "quality", .38f }, { "codec", .42f }, { "grit", .54f }, { "noise", .018f }, { "dither", 0 }, { "sat", .42f }, { "wet", .62f }, { "limiter", .92f }, { "ceiling", .88f }, { "outGain", .92f } } },
    { "PLAY - Chip Bass Cabinet", { { "codecMode", 3 }, { "speakerModel", 3 }, { "quality", .58f }, { "codec", .30f }, { "grit", .36f }, { "noise", .012f }, { "dither", 0 }, { "speaker", .72f }, { "verb", .08f }, { "wet", .78f }, { "limiter", .90f }, { "ceiling", .90f }, { "outGain", .94f } } },
    { "PLAY - Clocked Save Glitch", { { "codecMode", 2 }, { "speakerModel", 1 }, { "quality", .46f }, { "codec", .38f }, { "grit", .28f }, { "noise", .012f }, { "dither", 0 }, { "stallSync", 1 }, { "stallDivision", 3 }, { "stallProbability", .28f }, { "stallStrength", .48f }, { "stallLengthSync", 1 }, { "stallLengthDivision", 5 }, { "stallRepeatSync", 1 }, { "stallRepeatDivision", 5 }, { "wet", .82f }, { "limiter", .92f }, { "ceiling", .88f }, { "outGain", .92f } } },
};
}

void CartridgeEngineAudioProcessorEditor::Panel::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(juce::Colour(panel)); g.fillRoundedRectangle(b, 7.0f);
    g.setColour(juce::Colour(line)); g.drawRoundedRectangle(b, 7.0f, 1.0f);
    g.setColour(juce::Colour(lime)); g.fillRect(b.getX() + 12.0f, b.getY() + 12.0f, 24.0f, 1.5f);
    g.setColour(juce::Colour(dim)); g.setFont(font(9.5f, true));
    g.drawText(name, getLocalBounds().removeFromTop(34).withTrimmedLeft(44), juce::Justification::centredLeft);
}

CartridgeEngineAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    label.setText(text, juce::dontSendNotification); label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(cream)); label.setFont(font(10.0f, true)); addAndMakeVisible(label);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(lime)); slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff485246));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(cream)); slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(cream));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(ink)); slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(line));
    slider.onDragStart = std::move(changed); addAndMakeVisible(slider); attachment = std::make_unique<APVTS::SliderAttachment>(state, id, slider);
}
void CartridgeEngineAudioProcessorEditor::Knob::resized() { auto a = getLocalBounds(); label.setBounds(a.removeFromTop(18)); slider.setBounds(a.reduced(1)); }

CartridgeEngineAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    label.setText(text, juce::dontSendNotification); label.setColour(juce::Label::textColourId, juce::Colour(cream)); label.setFont(font(10.0f, true)); addAndMakeVisible(label);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id))) for (int i = 0; i < p->choices.size(); ++i) combo.addItem(p->choices[i], i + 1);
    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(ink)); combo.setColour(juce::ComboBox::textColourId, juce::Colour(cream)); combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(line));
    combo.onChange = std::move(changed); addAndMakeVisible(combo); attachment = std::make_unique<APVTS::ComboBoxAttachment>(state, id, combo);
}
void CartridgeEngineAudioProcessorEditor::Choice::resized() { auto a = getLocalBounds(); label.setBounds(a.removeFromTop(17)); combo.setBounds(a.removeFromTop(32).reduced(1)); }

CartridgeEngineAudioProcessorEditor::Switch::Switch(APVTS& state, const juce::String& id, const juce::String& text, std::function<void()> changed)
{
    button.setButtonText(text); button.setColour(juce::ToggleButton::textColourId, juce::Colour(cream)); button.setColour(juce::ToggleButton::tickColourId, juce::Colour(magenta));
    button.onClick = std::move(changed); addAndMakeVisible(button); attachment = std::make_unique<APVTS::ButtonAttachment>(state, id, button);
}

void CartridgeEngineAudioProcessorEditor::CartridgeDeck::setState(float inL, float inR, float outL, float outR, bool chip, bool stall, bool bank, float stallP, float bankP, int c, int s, const std::array<float,64>& waveform)
{
    input = { inL, inR }; output = { outL, outR }; chipActive = chip; stallActive = stall; bankActive = bank; stallProgress = stallP; bankProgress = bankP; codec = c; speaker = s; trace = waveform; phase += .025f; repaint();
}
void CartridgeEngineAudioProcessorEditor::CartridgeDeck::paint(juce::Graphics& g)
{
    auto outer = getLocalBounds().toFloat().reduced(1.0f); g.setColour(juce::Colour(0xff242c25)); g.fillRoundedRectangle(outer, 12.0f); g.setColour(juce::Colour(0xff66705f)); g.drawRoundedRectangle(outer, 12.0f, 1.4f);
    auto bay = outer.reduced(24.0f).withTrimmedBottom(102.0f); g.setColour(juce::Colour(0xff080d0a)); g.fillRoundedRectangle(bay, 8.0f); g.setColour(juce::Colour(0xff465244)); g.drawRoundedRectangle(bay, 8.0f, 1.0f);
    auto cart = bay.reduced(30.0f, 24.0f); g.setColour(juce::Colour(0xff59634f)); g.fillRoundedRectangle(cart, 9.0f); g.setColour(juce::Colour(0xff818972)); g.drawRoundedRectangle(cart, 9.0f, 1.2f);
    auto label = cart.reduced(22.0f, 18.0f).withTrimmedBottom(cart.getHeight() * .27f); g.setColour(juce::Colour(cream)); g.fillRoundedRectangle(label, 4.0f);
    g.setColour(juce::Colour(0xff1f2a20)); g.setFont(font(12.0f, true)); g.drawText("B&E DIGITAL // MEMORY PROGRAM", label.toNearestInt().reduced(12, 8), juce::Justification::topLeft);
    const char* codecs[] { "PCM", "DPCM", "ADPCM", "BRR", "MU-LAW" }; const char* speakers[] { "DIRECT", "HANDHELD", "TELEVISION", "CABINET", "PC SPEAKER" };
    g.setFont(font(10.0f, true)); g.setColour(juce::Colour(0xff536648)); g.drawText(juce::String(codecs[juce::jlimit(0, 4, codec)]) + " / " + speakers[juce::jlimit(0, 4, speaker)], label.toNearestInt().reduced(12, 8), juce::Justification::bottomLeft);
    auto waveformArea = label.reduced(12.0f, 7.0f).withTrimmedTop(28.0f).withTrimmedBottom(18.0f); juce::Path wave;
    for (std::size_t i = 0; i < this->trace.size(); ++i) { const auto x = waveformArea.getX() + waveformArea.getWidth() * (float) i / (float) (this->trace.size() - 1); const auto y = waveformArea.getCentreY() - juce::jlimit(-1.0f, 1.0f, this->trace[i]) * waveformArea.getHeight() * .43f; if (i == 0) wave.startNewSubPath(x, y); else wave.lineTo(x, y); }
    g.setColour(juce::Colour(0xff526746)); g.strokePath(wave, juce::PathStrokeType(1.5f));
    for (int i = 0; i < 9; ++i) { auto slot = juce::Rectangle<float>(cart.getX() + 22.0f + i * (cart.getWidth() - 44.0f) / 9.0f, cart.getBottom() - 25.0f, 16.0f, 9.0f); g.setColour(juce::Colour(i < 3 + codec ? amber : 0xff30382e)); g.fillRect(slot); }
    auto footer = outer.reduced(20.0f).removeFromBottom(92.0f); auto meters = footer.removeFromTop(38.0f); const char* names[] { "INPUT", "OUTPUT" }; const float values[] { std::max(input[0],input[1]), std::max(output[0],output[1]) };
    for (int i = 0; i < 2; ++i) { auto row = meters.removeFromTop(18.0f); g.setColour(juce::Colour(dim)); g.setFont(font(8.5f, true)); g.drawText(names[i], row.removeFromLeft(48.0f).toNearestInt(), juce::Justification::centredLeft); auto bar = row.reduced(2.0f, 5.0f); g.setColour(juce::Colour(0xff41483f)); g.fillRect(bar); g.setColour(juce::Colour(i == 0 ? amber : lime)); g.fillRect(bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, values[i] * 2.4f))); }
    const char* events[] { "STALL", "BANK", "CHIP" }; const bool states[] { stallActive, bankActive, chipActive }; const float progress[] { stallProgress, bankProgress, chipActive ? 1.0f : 0.0f }; const auto eventWidth=footer.getWidth()/3.0f; for (int i=0;i<3;++i) { auto cell=juce::Rectangle<float>(footer.getX()+i*eventWidth,footer.getY(),eventWidth,footer.getHeight()).reduced(4.0f); g.setColour(juce::Colour(states[i] ? (i==1?amber:magenta) : 0xff454d43)); g.fillEllipse(cell.getX(),cell.getY()+3.0f,9.0f,9.0f); g.setColour(juce::Colour(cream)); g.setFont(font(8.0f,true)); g.drawText(events[i],cell.withTrimmedLeft(14.0f).removeFromTop(16.0f).toNearestInt(),juce::Justification::centredLeft); auto bar=cell.removeFromBottom(5.0f);g.setColour(juce::Colour(0xff3e483d));g.fillRect(bar);g.setColour(juce::Colour(lime));g.fillRect(bar.withWidth(bar.getWidth()*juce::jlimit(0.0f,1.0f,progress[i]))); }
}

CartridgeEngineAudioProcessorEditor::CartridgeEngineAudioProcessorEditor(CartridgeEngineAudioProcessor& owner)
    : AudioProcessorEditor(&owner), processor(owner), apvts(owner.getAPVTS())
{
    setOpaque(true); brandLabel.setText("B&E DIGITAL", juce::dontSendNotification); brandLabel.setColour(juce::Label::textColourId, juce::Colour(lime)); brandLabel.setFont(font(10, true)); addAndMakeVisible(brandLabel);
    titleLabel.setText("CARTRIDGE ENGINE", juce::dontSendNotification); titleLabel.setColour(juce::Label::textColourId, juce::Colour(cream)); titleLabel.setFont(font(27, true)); addAndMakeVisible(titleLabel);
    subtitleLabel.setText("SAMPLE MEMORY / ADDRESS BUS / PLAYBACK HARDWARE / V4 STEREO", juce::dontSendNotification); subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(dim)); subtitleLabel.setFont(font(10, true)); addAndMakeVisible(subtitleLabel);
    profileLabel.setText("CARTRIDGE PROFILE", juce::dontSendNotification); profileLabel.setColour(juce::Label::textColourId, juce::Colour(dim)); profileLabel.setFont(font(9, true)); addAndMakeVisible(profileLabel);
    presetBox.addItem("Custom", 1); for (int i = 0; i < (int) std::size(presets); ++i) presetBox.addItem(presets[i].name, i + 2);
    const auto restoredPreset = apvts.state.getProperty("factoryPresetName", "Custom").toString();
    auto restoredPresetId = 1; for (int i = 0; i < (int) std::size(presets); ++i) if (restoredPreset == presets[i].name) restoredPresetId = i + 2;
    presetBox.setSelectedId(restoredPresetId, juce::dontSendNotification);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() >= 2) applyPreset(presetBox.getSelectedId() - 2); }; addAndMakeVisible(presetBox);
    for (auto* b : { &surfaceButton, &advancedButton, &performerButton }) { b->setColour(juce::TextButton::textColourOffId, juce::Colour(cream)); addAndMakeVisible(*b); }
    surfaceButton.onClick = [this] { showMode(EditorMode::simple); }; advancedButton.onClick = [this] { showMode(EditorMode::advanced); }; performerButton.onClick = [this] { showMode(EditorMode::performer); };
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(dim)); statusLabel.setFont(font(9, true)); statusLabel.setJustificationType(juce::Justification::centredRight); addAndMakeVisible(statusLabel);
    addAndMakeVisible(deck); addAndMakeVisible(surfacePage); addAndMakeVisible(advancedPage); addAndMakeVisible(performerPage); surfacePage.addAndMakeVisible(memoryPanel); surfacePage.addAndMakeVisible(playbackPanel);
    for (auto* p : { &conversionPanel, &clockPanel, &hardwarePanel, &texturePanel, &chipPanel, &outputPanel }) advancedPage.addAndMakeVisible(*p);
    for (auto* p : { &stallPanel, &bankPanel, &performanceChipPanel, &performanceOutputPanel }) performerPage.addAndMakeVisible(*p);

    addChoice(memoryPanel, "codecMode", "CODEC MEMORY", "PCM, delta, adaptive, BRR, or mu-law storage behavior.", true); addChoice(memoryPanel, "speakerModel", "PLAYBACK BODY", "Physical device response after conversion.", true);
    addKnob(memoryPanel, "bits", "WORD LENGTH", "Actual converter bit depth.", true); addKnob(memoryPanel, "rate", "MEMORY CLOCK", "Actual sample-memory playback rate.", true); addKnob(memoryPanel, "edge", "DAC EDGE", "Converter transition hardness.", true); addKnob(memoryPanel, "speaker", "BODY", "Strength of playback-device coloration.", true);
    addKnob(playbackPanel, "addressWear", "ADDRESS WEAR", "Sparse continuous ROM read errors."); addKnob(playbackPanel, "noise", "NOISE FLOOR", "Explicit cartridge electronics noise level.", true); addSwitch(playbackPanel, "dither", "DITHER", "Optional converter dither; disable it for a silent idle floor."); addKnob(playbackPanel, "sat", "OUTPUT DRIVE", "DAC and output-stage drive.", true); addKnob(playbackPanel, "inputGain", "INPUT dB", "Trim before memory conversion."); addKnob(playbackPanel, "wet", "MIX", "Dry and cartridge playback balance."); addKnob(playbackPanel, "outGain", "OUTPUT", "Final playback level.");

    addChoice(conversionPanel, "codecMode", "CODEC", "Stored sample codec family.", true); addKnob(conversionPanel, "bits", "BITS", "Converter resolution.", true); addKnob(conversionPanel, "blockMs", "BLOCK ms", "Codec predictor reset block.", true);
    addSwitch(conversionPanel, "dither", "DITHER", "Dither before quantization."); addSwitch(conversionPanel, "noiseShaping", "NOISE SHAPING", "Push quantization error toward the upper band.");
    addKnob(clockPanel, "rate", "CLOCK RATE", "Effective sample memory clock.", true); addKnob(clockPanel, "jitter", "JITTER", "Clock instability.", true); addKnob(clockPanel, "lpHz", "LOW-PASS", "Playback bandwidth ceiling.", true); addKnob(clockPanel, "hpHz", "HIGH-PASS", "Playback coupling cutoff.", true); addKnob(clockPanel, "preEmph", "PRE-EMPH", "Encode-side emphasis.", true); addKnob(clockPanel, "mulaw", "COMPANDING", "Nonlinear codec companding.", true);
    addChoice(hardwarePanel, "speakerModel", "DEVICE", "Direct, handheld, television, cabinet, or PC speaker."); addKnob(hardwarePanel, "speaker", "BODY", "Strength of playback hardware coloration.", true); addKnob(hardwarePanel, "microDelayMs", "BODY DELAY", "Short cabinet conduction path.", true); addKnob(hardwarePanel, "microDelayMix", "BODY MIX", "Conduction-path blend.", true); addKnob(hardwarePanel, "verb", "ROOM", "Short device enclosure reflections.", true); addKnob(hardwarePanel, "verbMs", "ROOM ms", "Enclosure reflection size.", true);
    addKnob(texturePanel, "sat", "SATURATION", "DAC and output-stage saturation.", true); addKnob(texturePanel, "edge", "DAC EDGE", "Stepped converter edge.", true); addKnob(texturePanel, "noise", "NOISE FLOOR", "Continuous electronics noise level.", true); addKnob(texturePanel, "dcDrift", "DC DRIFT", "Coupling and bias wander.", true); addKnob(texturePanel, "hum", "HUM", "Protected power supply hum.", true); addKnob(texturePanel, "whine", "CLOCK WHINE", "Protected oscillator bleed.", true); addKnob(texturePanel, "noiseTrack", "NOISE TRACK", "How strongly noise follows signal.", true);addKnob(texturePanel,"addressWear","ADDRESS WEAR","Sparse continuous ROM misreads.");addKnob(texturePanel,"busDepth","BUS DEPTH","Depth of address-line foldback.");
    addSwitch(chipPanel, "bleepsEnable", "ARM CHIP VOICE", "Chip synthesis is always explicitly user-owned."); addChoice(chipPanel, "bleepsWave", "WAVE", "Alternate, pulse, saw, triangle, or noise voice."); addChoice(chipPanel, "bleepsTrigger", "TRIGGER", "Transient, clock, or hybrid note triggering."); addChoice(chipPanel, "bleepsScale", "SCALE", "Pitch constraint for generated notes."); addKnob(chipPanel, "bleepsMix", "LEVEL", "Bounded chip voice level."); addKnob(chipPanel, "bleepsRate", "RATE", "Free-running note opportunity rate when Tempo Sync is off."); addKnob(chipPanel, "bleepsVibrato", "VIBRATO", "Pitch modulation depth."); addKnob(chipPanel, "bleepsPitch", "REGISTER", "Overall note register.");
    addKnob(outputPanel, "inputGain", "INPUT dB", "Input trim."); addKnob(outputPanel, "wet", "MIX", "Dry/wet balance."); addKnob(outputPanel, "limiter", "LIMITER", "Protected output limiting.", true); addKnob(outputPanel, "ceiling", "CEILING", "Limiter ceiling.", true); addKnob(outputPanel, "outGain", "OUTPUT", "Final trim.");

    addSwitch(stallPanel,"stallSync","SYNC STALLS","Trigger ROM stalls from the host grid.");addChoice(stallPanel,"stallDivision","TRIGGER GRID","Where a stall may begin.");addKnob(stallPanel,"stallProbability","PROBABILITY","Stable chance per musical step.");addKnob(stallPanel,"stallStrength","STRENGTH","Blend between live playback and stalled memory.");addKnob(stallPanel,"stallDurationMs","LENGTH ms","Free/manual stall duration.");addSwitch(stallPanel,"stallLengthSync","SYNC LENGTH","Use a musical stall duration.");addChoice(stallPanel,"stallLengthDivision","STALL LENGTH","Musical stall duration.");addKnob(stallPanel,"stallRepeatMs","REPEAT ms","Loop window inside the stalled ROM.");addSwitch(stallPanel,"stallRepeatSync","SYNC REPEAT","Quantize the repeated phrase itself.");addChoice(stallPanel,"stallRepeatDivision","REPEAT GRID","Musical repeat-window length.");stallTriggerButton.onClick=[this]{processor.triggerRomStall();markCustom();};stallPanel.addAndMakeVisible(stallTriggerButton);panelItems[&stallPanel].push_back(&stallTriggerButton);
    addSwitch(bankPanel,"bankSync","SYNC FAULTS","Trigger address-bank faults from the host grid.");addChoice(bankPanel,"bankDivision","TRIGGER GRID","Where a bank fault may begin.");addKnob(bankPanel,"bankProbability","PROBABILITY","Stable chance per musical step.");addKnob(bankPanel,"bankStrength","STRENGTH","Depth of the address-line corruption.");addKnob(bankPanel,"bankDurationMs","LENGTH ms","Free/manual bank fault length.");addSwitch(bankPanel,"bankLengthSync","SYNC LENGTH","Use a musical bank-fault duration.");addChoice(bankPanel,"bankLengthDivision","FAULT LENGTH","Musical bank-fault duration.");addKnob(bankPanel,"addressWear","ADDRESS WEAR","Continuous sparse ROM misreads.");addKnob(bankPanel,"busDepth","BUS DEPTH","How deeply corrupted addresses fold the sample.");bankTriggerButton.onClick=[this]{processor.triggerBankFault();markCustom();};bankPanel.addAndMakeVisible(bankTriggerButton);panelItems[&bankPanel].push_back(&bankTriggerButton);
    addSwitch(performanceChipPanel,"bleepsEnable","ARM CHIP VOICE","Generated tones are always explicitly owned.");addSwitch(performanceChipPanel,"tempoSync","SYNC VOICE","Place chip notes on the host grid.");addChoice(performanceChipPanel,"syncDivision","TRIGGER GRID","Chip-note opportunity grid.");addKnob(performanceChipPanel,"chipProbability","PROBABILITY","Stable chance per grid step.");addChoice(performanceChipPanel,"bleepsWave","WAVE","Pulse, saw, triangle, noise, or alternate.");addChoice(performanceChipPanel,"bleepsScale","SCALE","Constrain generated pitches.");addKnob(performanceChipPanel,"bleepsPitch","REGISTER","Overall note register.");addKnob(performanceChipPanel,"bleepsMix","LEVEL","Bounded voice level.");chipTriggerButton.onClick=[this]{processor.triggerChipVoice();markCustom();};performanceChipPanel.addAndMakeVisible(chipTriggerButton);panelItems[&performanceChipPanel].push_back(&chipTriggerButton);
    addKnob(performanceOutputPanel,"inputGain","INPUT","Input trim.");addKnob(performanceOutputPanel,"wet","MIX","Dry/wet balance.");addKnob(performanceOutputPanel,"limiter","LIMITER","Output protection.",true);addKnob(performanceOutputPanel,"ceiling","CEILING","Hard safety ceiling.",true);addKnob(performanceOutputPanel,"outGain","OUTPUT","Final trim.");

    setResizable(true, true); setResizeLimits(980, 660, 1600, 1000); setSize(1180, 720); showMode(EditorMode::simple); startTimerHz(30);
}
CartridgeEngineAudioProcessorEditor::~CartridgeEngineAudioProcessorEditor() { stopTimer(); }

CartridgeEngineAudioProcessorEditor::Knob* CartridgeEngineAudioProcessorEditor::addKnob(Panel& p, const juce::String& id, const juce::String& text, const juce::String& hint, bool linked)
{ auto c = std::make_unique<Knob>(apvts, id, text, [this, linked] { if (linked) processor.materialiseLegacyMacros(); markCustom(); }); c->setHint(hint); auto* raw = c.get(); p.addAndMakeVisible(*raw); panelItems[&p].push_back(raw); if (linked) linkedControls.push_back(raw); knobs.push_back(std::move(c)); return raw; }
CartridgeEngineAudioProcessorEditor::Choice* CartridgeEngineAudioProcessorEditor::addChoice(Panel& p, const juce::String& id, const juce::String& text, const juce::String& hint, bool linked)
{ auto c = std::make_unique<Choice>(apvts, id, text, [this, linked] { if (linked) processor.materialiseLegacyMacros(); markCustom(); }); c->setHint(hint); auto* raw = c.get(); p.addAndMakeVisible(*raw); panelItems[&p].push_back(raw); if (linked) linkedControls.push_back(raw); choices.push_back(std::move(c)); return raw; }
CartridgeEngineAudioProcessorEditor::Switch* CartridgeEngineAudioProcessorEditor::addSwitch(Panel& p, const juce::String& id, const juce::String& text, const juce::String& hint, bool linked)
{ auto c = std::make_unique<Switch>(apvts, id, text, [this, linked] { if (linked) processor.materialiseLegacyMacros(); markCustom(); }); c->setHint(hint); auto* raw = c.get(); p.addAndMakeVisible(*raw); panelItems[&p].push_back(raw); if (linked) linkedControls.push_back(raw); switches.push_back(std::move(c)); return raw; }
void CartridgeEngineAudioProcessorEditor::layoutPanel(Panel& p, int columns)
{ auto area = p.contentBounds(); auto& items = panelItems[&p]; if (items.empty()) return; const auto cols = juce::jmax(1, columns), rows = juce::jmax(1, ((int) items.size() + cols - 1) / cols), w = area.getWidth() / cols, h = area.getHeight() / rows; for (int i = 0; i < (int) items.size(); ++i) items[(std::size_t) i]->setBounds(area.getX() + (i % cols) * w, area.getY() + (i / cols) * h, w, h); }
void CartridgeEngineAudioProcessorEditor::showMode(EditorMode mode) { currentMode=mode;surfacePage.setVisible(mode==EditorMode::simple);advancedPage.setVisible(mode==EditorMode::advanced);performerPage.setVisible(mode==EditorMode::performer);surfaceButton.setToggleState(mode==EditorMode::simple,juce::dontSendNotification);advancedButton.setToggleState(mode==EditorMode::advanced,juce::dontSendNotification);performerButton.setToggleState(mode==EditorMode::performer,juce::dontSendNotification);resized(); }
void CartridgeEngineAudioProcessorEditor::setParameter(const juce::String& id, float value) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value)); }
float CartridgeEngineAudioProcessorEditor::getParameter(const juce::String& id) const { if (const auto* p = apvts.getRawParameterValue(id)) return p->load(); return 0.0f; }
void CartridgeEngineAudioProcessorEditor::resetParameters() { for (auto* p : processor.getParameters()) p->setValueNotifyingHost(p->getDefaultValue()); }
void CartridgeEngineAudioProcessorEditor::applyPreset(int index) { if (index < 0 || index >= (int) std::size(presets)) return; suppressPresetChanges = true; resetParameters();for(const auto&s:presets[index].values)if(juce::String(s.id)!="macroLink")setParameter(s.id,s.value);setParameter("macroLink",1);processor.materialiseLegacyMacros();for(const auto&s:presets[index].values){const auto id=juce::String(s.id);if(id!="quality"&&id!="codec"&&id!="grit"&&id!="noise"&&id!="macroLink")setParameter(s.id,s.value);}setParameter("macroLink",0);apvts.state.setProperty("factoryPresetName",presets[index].name,nullptr);suppressPresetChanges = false; }
void CartridgeEngineAudioProcessorEditor::markCustom() { if (!suppressPresetChanges) { presetBox.setSelectedId(1, juce::dontSendNotification); apvts.state.setProperty("factoryPresetName","Custom",nullptr); } }
void CartridgeEngineAudioProcessorEditor::timerCallback()
{
    const auto linked = getParameter("macroLink") > .5f; for (auto* c : linkedControls) { c->setEnabled(true); c->setAlpha(1.0f); }
    const auto chipArmed = getParameter("bleepsEnable") > .5f; deck.setState(processor.inputPeak(0), processor.inputPeak(1), processor.outputPeak(0), processor.outputPeak(1), processor.bleepActive(),processor.stallActive(),processor.bankFaultActive(),processor.stallProgress(),processor.bankFaultProgress(), (int) getParameter("codecMode"), (int) getParameter("speakerModel"),processor.outputTrace());
    if (processor.bleepActive()) statusLabel.setText("CHIP VOICE EVENT / MEMORY BUS", juce::dontSendNotification); else if(processor.stallActive())statusLabel.setText("ROM STALL / REPEATING MEMORY WINDOW",juce::dontSendNotification);else if(processor.bankFaultActive())statusLabel.setText("ADDRESS BANK FAULT / BUS FOLDBACK",juce::dontSendNotification);else if (chipArmed && getParameter("tempoSync") > .5f) statusLabel.setText("CHIP VOICE ARMED / HOST GRID", juce::dontSendNotification); else if (chipArmed) statusLabel.setText("CHIP VOICE ARMED / WAITING FOR TRIGGER", juce::dontSendNotification); else statusLabel.setText(linked ? "LEGACY SOUND / TURN A DETAIL TO EDIT" : "V4 STEREO / DIRECT CONTROL", juce::dontSendNotification);
}
void CartridgeEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ink)); auto b = getLocalBounds().toFloat().reduced(7.0f); g.setColour(juce::Colour(deep)); g.fillRoundedRectangle(b, 7.0f); g.setColour(juce::Colour(line)); g.drawRoundedRectangle(b, 7.0f, 1.0f);
    g.setColour(juce::Colour(lime)); g.fillRect(16.0f, 7.0f, 190.0f, 2.0f); g.setColour(juce::Colour(magenta)); g.fillRect((float) getWidth() - 206.0f, 7.0f, 190.0f, 2.0f); g.setColour(juce::Colour(0xff374136)); g.fillRect(15, 91, getWidth() - 30, 1);
}
void CartridgeEngineAudioProcessorEditor::resized()
{
    auto area=getLocalBounds().reduced(16),header=area.removeFromTop(82);const auto profileWidth=juce::jlimit(220,300,(int)std::round(header.getWidth()*.28f));auto profile=header.removeFromRight(profileWidth);header.removeFromRight(12);auto identity=header;brandLabel.setBounds(identity.removeFromTop(17));titleLabel.setBounds(identity.removeFromTop(36));subtitleLabel.setBounds(identity.removeFromTop(18));profileLabel.setBounds(profile.removeFromTop(17));presetBox.setBounds(profile.removeFromTop(32));
    auto nav=area.removeFromTop(42);surfaceButton.setBounds(nav.removeFromLeft(108).reduced(0,4));nav.removeFromLeft(8);advancedButton.setBounds(nav.removeFromLeft(108).reduced(0,4));nav.removeFromLeft(8);performerButton.setBounds(nav.removeFromLeft(118).reduced(0,4));statusLabel.setBounds(nav);
    auto body=area.reduced(2);deck.setBounds(body.removeFromLeft((int)std::round(body.getWidth()*.34f)).reduced(3));surfacePage.setBounds(body);advancedPage.setBounds(body);performerPage.setBounds(body);
    auto simple=surfacePage.getLocalBounds().reduced(3);memoryPanel.setBounds(simple.removeFromTop((int)std::round(simple.getHeight()*.58f)).reduced(2));playbackPanel.setBounds(simple.reduced(2));layoutPanel(memoryPanel,3);layoutPanel(playbackPanel,3);
    auto advanced=advancedPage.getLocalBounds().reduced(3);const auto aw=advanced.getWidth()/2,ah=advanced.getHeight()/3;Panel*panels[]{&conversionPanel,&clockPanel,&hardwarePanel,&texturePanel,&chipPanel,&outputPanel};for(int i=0;i<6;++i)panels[i]->setBounds(advanced.getX()+(i%2)*aw,advanced.getY()+(i/2)*ah,aw,ah);layoutPanel(conversionPanel,3);layoutPanel(clockPanel,3);layoutPanel(hardwarePanel,3);layoutPanel(texturePanel,4);layoutPanel(chipPanel,4);layoutPanel(outputPanel,3);
    auto performer=performerPage.getLocalBounds().reduced(3);const auto pw=performer.getWidth()/2,ph=performer.getHeight()/2;stallPanel.setBounds(performer.getX(),performer.getY(),pw,ph);bankPanel.setBounds(performer.getX()+pw,performer.getY(),pw,ph);performanceChipPanel.setBounds(performer.getX(),performer.getY()+ph,pw,ph);performanceOutputPanel.setBounds(performer.getX()+pw,performer.getY()+ph,pw,ph);layoutPanel(stallPanel,3);layoutPanel(bankPanel,3);layoutPanel(performanceChipPanel,3);layoutPanel(performanceOutputPanel,3);
}
