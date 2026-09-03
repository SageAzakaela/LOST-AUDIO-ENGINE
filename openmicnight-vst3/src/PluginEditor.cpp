#include "PluginEditor.h"

#include <array>
#include <cmath>

namespace
{
constexpr auto cyan = 0xff51e7ef;
constexpr auto magenta = 0xffff4fb8;
constexpr auto cream = 0xfffff5e7;
constexpr auto ink = 0xff090c0d;
constexpr auto panel = 0xff15191a;
constexpr auto panel2 = 0xff202526;

struct Preset
{
    const char* name;
    std::initializer_list<std::pair<const char*, double>> values;
};

const Preset presets[] {
    { "Corner Club Vocal", {{"micModel",0},{"venueModel",0},{"paModel",0},{"hotMic",.42f},{"wall",.32f},{"room",.44f},{"crowdBed",0},{"crowdLevel",.10f},{"feedbackAmount",.36f},{"feedbackArm",0}} },
    { "Dive Bar Headliner", {{"micModel",0},{"venueModel",1},{"paModel",3},{"hotMic",.64f},{"wall",.28f},{"room",.36f},{"crowdBed",3},{"crowdLevel",.28f},{"crowdMood",.67f},{"feedbackAmount",.52f},{"feedbackArm",0}} },
    { "Cheap Karaoke Glory", {{"micModel",2},{"venueModel",1},{"paModel",3},{"hotMic",.73f},{"wall",.18f},{"room",.31f},{"crowdBed",2},{"crowdLevel",.34f},{"electricalNoise",.045f},{"feedbackAmount",.61f},{"feedbackArm",0}} },
    { "Community Hall Speech", {{"micModel",3},{"venueModel",5},{"paModel",1},{"hotMic",.35f},{"wall",.58f},{"room",.63f},{"crowdBed",0},{"crowdLevel",.06f},{"feedbackAmount",.31f},{"feedbackArm",0}} },
    { "Warehouse Soundcheck", {{"micModel",1},{"venueModel",3},{"paModel",4},{"hotMic",.58f},{"wall",.46f},{"room",.82f},{"crowdLevel",.02f},{"feedbackAmount",.66f},{"fbFreq",2360},{"feedbackArm",0}} },
    { "Rooftop DIY Show", {{"micModel",0},{"venueModel",4},{"paModel",0},{"hotMic",.55f},{"wall",.50f},{"room",.72f},{"crowdBed",1},{"crowdLevel",.19f},{"feedbackAmount",.44f},{"feedbackArm",0}} },
    { "Rehearsal Through Door", {{"micModel",0},{"venueModel",2},{"paModel",4},{"hotMic",.61f},{"wall",.86f},{"room",.43f},{"crowdLevel",0},{"feedbackAmount",.47f},{"feedbackArm",0}} },
    { "Old Ribbon Crooner", {{"micModel",4},{"venueModel",0},{"paModel",0},{"hotMic",.31f},{"wall",.22f},{"room",.52f},{"crowdLevel",.09f},{"feedbackAmount",.28f},{"feedbackArm",0}} },
    { "Paging Horn Panic", {{"micModel",3},{"venueModel",5},{"paModel",2},{"hotMic",.70f},{"wall",.62f},{"room",.58f},{"fbFreq",2860},{"ringQ",23},{"feedbackAmount",.69f},{"feedbackArm",0}} },
    { "Blown Stack Finale", {{"micModel",0},{"venueModel",3},{"paModel",4},{"hotMic",.86f},{"wall",.30f},{"room",.77f},{"crowdBed",3},{"crowdLevel",.31f},{"feedbackAmount",.76f},{"feedbackArm",0}} },
    { "Empty Room Mic Check", {{"micModel",0},{"venueModel",2},{"paModel",0},{"hotMic",.40f},{"wall",.26f},{"room",.48f},{"crowdLevel",0},{"feedbackAmount",.40f},{"feedbackArm",0}} },
    { "Audience From The Lobby", {{"micModel",1},{"venueModel",5},{"paModel",1},{"hotMic",.38f},{"wall",.94f},{"room",.69f},{"crowdLevel",.23f},{"wallAbsorption",.71f},{"feedbackArm",0}} },
    { "Small PA Overdrive", {{"macroLink",0},{"micModel",0},{"venueModel",0},{"paModel",0},{"micDrive",.51f},{"paDrive",.74f},{"monitorLevel",.38f},{"room",.34f},{"mix",1},{"feedbackAmount",.38f},{"feedbackArm",0}} },
    { "Howl Ready: Low Ring", {{"macroLink",0},{"micModel",0},{"venueModel",1},{"paModel",3},{"monitorLevel",.78f},{"fbFreq",620},{"ringQ",27},{"feedbackAmount",.82f},{"feedbackBuildMs",680},{"feedbackArm",0}} },
    { "Howl Ready: Glassy", {{"macroLink",0},{"micModel",1},{"venueModel",3},{"paModel",2},{"monitorLevel",.72f},{"fbFreq",3720},{"ringQ",31},{"feedbackAmount",.77f},{"feedbackBuildMs",510},{"feedbackArm",0}} },
    { "Safe Starting Point", {{"macroLink",1},{"micModel",0},{"venueModel",0},{"paModel",0},{"hotMic",.35f},{"wall",.35f},{"room",.42f},{"crowdLevel",0},{"limit",.82f},{"ceiling",.88f},{"outGain",.92f},{"feedbackAmount",.35f},{"feedbackArm",0}} },
    { "PLAY - Live Guitar Small PA", {{"micModel",0},{"venueModel",0},{"paModel",0},{"hotMic",.46f},{"wall",.24f},{"room",.34f},{"crowdLevel",.035f},{"crowdBed",0},{"feedbackAmount",.28f},{"feedbackArm",0},{"mix",.78f},{"limit",.90f},{"ceiling",.88f},{"outGain",.92f}} },
    { "PLAY - Vocal Club Warmth", {{"micModel",4},{"venueModel",0},{"paModel",0},{"hotMic",.38f},{"wall",.26f},{"room",.46f},{"crowdLevel",.06f},{"crowdBed",1},{"crowdBehavior",1},{"feedbackAmount",.24f},{"feedbackArm",0},{"mix",.82f},{"limit",.90f},{"ceiling",.90f},{"outGain",.94f}} },
    { "PLAY - Drum Room Crowd", {{"micModel",1},{"venueModel",3},{"paModel",4},{"hotMic",.32f},{"wall",.38f},{"room",.58f},{"crowdLevel",.09f},{"crowdBed",2},{"crowdBehavior",0},{"crowdSensitivity",.62f},{"crowdResponse",.38f},{"feedbackAmount",.22f},{"feedbackArm",0},{"mix",.62f},{"limit",.92f},{"ceiling",.88f},{"outGain",.92f}} },
    { "PLAY - Clocked Audience Hits", {{"micModel",0},{"venueModel",1},{"paModel",3},{"hotMic",.42f},{"wall",.28f},{"room",.40f},{"crowdLevel",.07f},{"crowdBed",3},{"crowdBehavior",2},{"crowdSync",1},{"crowdDivision",2},{"crowdProbability",.22f},{"crowdStrength",.42f},{"crowdLengthSync",1},{"crowdLengthDivision",5},{"feedbackAmount",.26f},{"feedbackArm",0},{"mix",.76f},{"limit",.92f},{"ceiling",.88f},{"outGain",.92f}} }
};

float parameterValue(juce::AudioProcessorValueTreeState& state, const char* id)
{
    if (const auto* value = state.getRawParameterValue(id)) return value->load();
    return 0.0f;
}
}

OpenMicNightAudioProcessorEditor::NightLookAndFeel::NightLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(panel2));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff606767));
    setColour(juce::ComboBox::textColourId, juce::Colour(cream));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(panel2));
    setColour(juce::PopupMenu::textColourId, juce::Colour(cream));
    setColour(juce::TextButton::buttonColourId, juce::Colour(panel2));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff17494d));
    setColour(juce::TextButton::textColourOffId, juce::Colour(cream));
    setColour(juce::TextButton::textColourOnId, juce::Colour(cyan));
}

void OpenMicNightAudioProcessorEditor::NightLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                                                         float position, float start, float end, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)).reduced(7.0f);
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = start + position * (end - start);
    g.setColour(juce::Colour(0xff343a39)); g.fillEllipse(bounds.withSizeKeepingCentre(radius * 2.0f, radius * 2.0f));
    juce::Path arc; arc.addCentredArc(centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.0f, start, angle, true);
    g.setColour(juce::Colour(cyan)); g.strokePath(arc, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path pointer; pointer.addRoundedRectangle(-2.0f, -radius + 8.0f, 4.0f, radius * 0.54f, 2.0f);
    g.setColour(juce::Colour(cream)); g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void OpenMicNightAudioProcessorEditor::NightLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const auto armed = b.getToggleState();
    g.setColour(armed ? juce::Colour(0xff5e153e) : juce::Colour(panel2)); g.fillRoundedRectangle(r, 5.0f);
    g.setColour(armed ? juce::Colour(magenta) : juce::Colour(0xff67706f)); g.drawRoundedRectangle(r, 5.0f, 1.2f);
    g.setColour(armed ? juce::Colour(cream) : juce::Colour(0xffb8bfbd)); g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(8), juce::Justification::centred, 1);
}

OpenMicNightAudioProcessorEditor::Knob::Knob(APVTS& state, const juce::String& parameterId, const juce::String& title, const juce::String& suffix, std::function<void()> changed)
    : id(parameterId)
{
    label.setText(title, juce::dontSendNotification); label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffc5cbc8));
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 20);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(cream)); slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(ink));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff505655)); slider.setTextValueSuffix(suffix);
    slider.onDragStart = std::move(changed);
    addAndMakeVisible(label); addAndMakeVisible(slider); attachment = std::make_unique<APVTS::SliderAttachment>(state, parameterId, slider);
}

void OpenMicNightAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds(); label.setBounds(area.removeFromTop(20)); slider.setBounds(area.reduced(2));
}

OpenMicNightAudioProcessorEditor::Choice::Choice(APVTS& state, const juce::String& parameterId, const juce::String& title, std::function<void()> changed)
{
    label.setText(title, juce::dontSendNotification); label.setColour(juce::Label::textColourId, juce::Colour(0xffaeb5b2));
    if (const auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId)))
        for (int i = 0; i < parameter->choices.size(); ++i) box.addItem(parameter->choices[i], i + 1);
    box.onChange=std::move(changed);addAndMakeVisible(label); addAndMakeVisible(box); attachment = std::make_unique<APVTS::ComboBoxAttachment>(state, parameterId, box);
}

void OpenMicNightAudioProcessorEditor::Choice::resized()
{
    auto area = getLocalBounds(); label.setBounds(area.removeFromTop(18)); box.setBounds(area.removeFromTop(32));
}

OpenMicNightAudioProcessorEditor::StageView::StageView(OpenMicNightAudioProcessor& p, APVTS& s) : processor(p), state(s) {}

void OpenMicNightAudioProcessorEditor::StageView::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0b0e0e)); g.fillRoundedRectangle(r, 9.0f);
    g.setColour(juce::Colour(0xff4b5250)); g.drawRoundedRectangle(r.reduced(.5f), 9.0f, 1.0f);
    const auto inputEnergy = processor.inputEnergyActivity(); const auto response = processor.audienceResponseActivity();
    const auto activity = parameterValue(state, "feedbackArm") > .5f ? processor.feedbackActivity() : inputEnergy; const auto armed = parameterValue(state, "feedbackArm") > .5f;
    const auto room = parameterValue(state, "room"); const auto crowd = parameterValue(state, "crowdLevel");
    const auto stageY = r.getBottom() - 70.0f;
    g.setColour(juce::Colour(0xff222827)); g.fillRect(r.getX() + 12.0f, stageY, r.getWidth() - 24.0f, 48.0f);
    for (int i = 0; i < 4; ++i)
    {
        auto echo = r.reduced(25.0f + i * 18.0f, 24.0f + i * 13.0f);
        g.setColour(juce::Colour(cyan).withAlpha((0.04f + room * 0.07f) * (4 - i))); g.drawRoundedRectangle(echo, 18.0f, 1.2f);
    }
    const auto micX = r.getX() + r.getWidth() * 0.38f;
    g.setColour(juce::Colour(0xffbfc5bf)); g.fillRoundedRectangle(micX, stageY - 116.0f, 17.0f, 42.0f, 8.0f);
    g.setColour(juce::Colour(0xff606764)); g.fillRect(micX + 7.0f, stageY - 74.0f, 3.0f, 76.0f);
    g.drawLine(micX + 8.0f, stageY, micX - 14.0f, stageY + 18.0f, 3.0f); g.drawLine(micX + 8.0f, stageY, micX + 30.0f, stageY + 18.0f, 3.0f);
    auto monitor = juce::Rectangle<float>(micX + 38.0f, stageY - 17.0f, 64.0f, 32.0f);
    g.setColour(juce::Colour(0xff333837)); g.fillRoundedRectangle(monitor, 4.0f); g.setColour(juce::Colour(cyan).withAlpha(.35f)); g.drawRoundedRectangle(monitor, 4.0f, 1.0f);
    for (int side = 0; side < 2; ++side)
    {
        auto speaker = juce::Rectangle<float>(side == 0 ? r.getX() + 30.0f : r.getRight() - 76.0f, stageY - 104.0f, 46.0f, 92.0f);
        g.setColour(juce::Colour(0xff292e2d)); g.fillRoundedRectangle(speaker, 4.0f); g.setColour(juce::Colour(0xff555c59)); g.drawRoundedRectangle(speaker, 4.0f, 1.0f);
        g.setColour(juce::Colour(0xff111414)); g.fillEllipse(speaker.withSizeKeepingCentre(31.0f, 31.0f).translated(0, 18));
    }
    const int people = juce::jmax(0, juce::roundToInt(4 + crowd * 10 + response * 5));
    for (int i = 0; i < people; ++i)
    {
        const auto x = r.getX() + 22.0f + std::fmod(static_cast<float>(i * 47), r.getWidth() - 44.0f);
        const auto y = r.getBottom() - 19.0f - static_cast<float>((i * 13) % 19);
        g.setColour(juce::Colour(0xff5a615e).withAlpha(.45f + crowd * .35f)); g.fillEllipse(x, y - 10.0f, 8.0f, 8.0f); g.fillRect(x + 1.0f, y - 2.0f, 6.0f, 12.0f);
    }
    if (armed)
    {
        juce::Path loop; loop.startNewSubPath(micX + 8.0f, stageY - 110.0f); loop.cubicTo(r.getRight() - 38.0f, r.getY() + 35.0f, r.getRight() - 20.0f, stageY - 55.0f, micX + 83.0f, stageY - 20.0f);
        g.setColour(juce::Colour(magenta).withAlpha(.28f + activity * .72f)); g.strokePath(loop, juce::PathStrokeType(2.0f + activity * 5.0f));
    }
    g.setColour(armed ? juce::Colour(magenta) : juce::Colour(cyan)); g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawText(armed ? "FEEDBACK ARMED / SAFETY ONLINE" : "FEEDBACK DISARMED / VENUE LIVE", r.reduced(16.0f).removeFromTop(24.0f), juce::Justification::centredLeft);
    const auto meter = juce::Rectangle<float>(r.getX() + 16.0f, r.getY() + 43.0f, r.getWidth() - 32.0f, 7.0f);
    g.setColour(juce::Colour(0xff242928)); g.fillRoundedRectangle(meter, 3.5f); g.setColour(juce::Colour(armed ? magenta : cyan)); g.fillRoundedRectangle(meter.withWidth(meter.getWidth() * activity), 3.5f);
    auto waveArea=juce::Rectangle<float>(r.getX()+16.0f,r.getY()+61.0f,r.getWidth()-32.0f,46.0f);g.setColour(juce::Colour(0xff080a0a).withAlpha(.76f));g.fillRoundedRectangle(waveArea,4);juce::Path waveform;const auto trace=processor.outputTrace();for(std::size_t i=0;i<trace.size();++i){const auto x=waveArea.getX()+waveArea.getWidth()*(float)i/(float)(trace.size()-1);const auto y=waveArea.getCentreY()-juce::jlimit(-1.0f,1.0f,trace[i])*waveArea.getHeight()*.42f;if(i==0)waveform.startNewSubPath(x,y);else waveform.lineTo(x,y);}g.setColour(juce::Colour(cyan).withAlpha(.82f));g.strokePath(waveform,juce::PathStrokeType(1.4f));
    const char* telemetryNames[]{"INPUT","REACT","CROWD","LIMIT"};const float telemetry[]{inputEnergy,response,processor.crowdActivity(),processor.limiterActivity()};auto telemetryArea=juce::Rectangle<float>(r.getX()+16.0f,r.getBottom()-77.0f,r.getWidth()-32.0f,56.0f);for(int i=0;i<4;++i){auto row=telemetryArea.removeFromTop(14.0f);g.setFont(juce::Font(juce::FontOptions(8.0f,juce::Font::bold)));g.setColour(juce::Colour(0xffb5bdba));g.drawText(telemetryNames[i],row.removeFromLeft(43.0f).toNearestInt(),juce::Justification::centredLeft);auto bar=row.reduced(2.0f,4.0f);g.setColour(juce::Colour(0xff303635));g.fillRect(bar);g.setColour(juce::Colour(i==3?magenta:cyan));g.fillRect(bar.withWidth(bar.getWidth()*juce::jlimit(0.0f,1.0f,telemetry[i])));}
    if(processor.crowdEventActive()||processor.feedbackEventActive()){auto badge=r.reduced(16.0f).removeFromTop(24.0f).removeFromRight(152.0f);g.setColour(juce::Colour(magenta).withAlpha(.18f));g.fillRoundedRectangle(badge,4);g.setColour(juce::Colour(cream));g.setFont(juce::Font(juce::FontOptions(9.0f,juce::Font::bold)));g.drawText(processor.crowdEventActive()?"AUDIENCE EVENT":"CONDUCTED HOWL",badge.toNearestInt(),juce::Justification::centred);}
}

OpenMicNightAudioProcessorEditor::OpenMicNightAudioProcessorEditor(OpenMicNightAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), apvts(p.state()), stage(p, apvts)
{
    setLookAndFeel(&look);
    brand.setText("B&E DIGITAL", juce::dontSendNotification); brand.setColour(juce::Label::textColourId, juce::Colour(cyan));
    brand.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    title.setText("OPEN MIC NIGHT", juce::dontSendNotification); title.setColour(juce::Label::textColourId, juce::Colour(cream));
    title.setFont(juce::Font(juce::FontOptions(27.0f, juce::Font::bold)));
    subtitle.setText("MIC / MONITOR / PA / ROOM / AUDIENCE", juce::dontSendNotification); subtitle.setColour(juce::Label::textColourId, juce::Colour(0xffa8afac));
    profileLabel.setText("PROFILE", juce::dontSendNotification); profileLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa8afac));
    statusLabel.setText("FEEDBACK SAFE / CROWD ROUTED", juce::dontSendNotification); statusLabel.setJustificationType(juce::Justification::centredRight);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa8afac));
    const std::array<juce::Component*, 18> visibleComponents {
        &brand, &title, &subtitle, &profileLabel, &statusLabel, &presetBox,
        &surfaceButton, &advancedButton, &performerButton, &feedbackArm, &venueBed, &conductedFeedback,
        &feedbackSync, &feedbackLengthSync, &crowdSync, &crowdLengthSync, &feedbackTrigger, &crowdTrigger
    };
    for (auto* component : visibleComponents) addAndMakeVisible(component);
    addAndMakeVisible(stage);
    presetBox.addItem("Custom", 1); for (int i = 0; i < static_cast<int>(std::size(presets)); ++i) presetBox.addItem(presets[i].name, i + 2);
    const auto restoredPreset=apvts.state.getProperty("factoryPresetName","Custom").toString();auto restoredPresetId=1;for(int i=0;i<(int)std::size(presets);++i)if(restoredPreset==presets[i].name)restoredPresetId=i+2;presetBox.setSelectedId(restoredPresetId, juce::dontSendNotification); presetBox.onChange = [this] { if (presetBox.getSelectedId() > 1) applyPreset(presetBox.getSelectedId() - 2); };
    for(auto*b:{&surfaceButton,&advancedButton,&performerButton}){b->setClickingTogglesState(true);b->setRadioGroupId(19);}surfaceButton.onClick=[this]{showMode(EditorMode::simple);};advancedButton.onClick=[this]{showMode(EditorMode::advanced);};performerButton.onClick=[this]{showMode(EditorMode::performer);};surfaceButton.setToggleState(true,juce::dontSendNotification);
    feedbackArmAttachment = std::make_unique<APVTS::ButtonAttachment>(apvts, "feedbackArm", feedbackArm);
    venueBedAttachment = std::make_unique<APVTS::ButtonAttachment>(apvts, "venueBedEnable", venueBed);
    conductedFeedbackAttachment=std::make_unique<APVTS::ButtonAttachment>(apvts,"feedbackConducted",conductedFeedback);feedbackSyncAttachment=std::make_unique<APVTS::ButtonAttachment>(apvts,"feedbackSync",feedbackSync);feedbackLengthSyncAttachment=std::make_unique<APVTS::ButtonAttachment>(apvts,"feedbackLengthSync",feedbackLengthSync);crowdSyncAttachment=std::make_unique<APVTS::ButtonAttachment>(apvts,"crowdSync",crowdSync);crowdLengthSyncAttachment=std::make_unique<APVTS::ButtonAttachment>(apvts,"crowdLengthSync",crowdLengthSync);
    for(auto*b:{&feedbackArm,&venueBed,&conductedFeedback,&feedbackSync,&feedbackLengthSync,&crowdSync,&crowdLengthSync})b->onClick=[this]{markCustom();};
    const auto physicalModel=[this]{if(!suppressPresetChanges)setParameter("macroLink",1);markCustom();};const auto edited=[this]{if(processor.legacyMacrosActive())processor.materialiseLegacyMacros();markCustom();};micChoice=std::make_unique<Choice>(apvts,"micModel","MICROPHONE",physicalModel);venueChoice=std::make_unique<Choice>(apvts,"venueModel","VENUE",physicalModel);paChoice=std::make_unique<Choice>(apvts,"paModel","PA SYSTEM",physicalModel);crowdBedChoice=std::make_unique<Choice>(apvts,"crowdBed","AUDIENCE BED",edited);crowdBehaviorChoice=std::make_unique<Choice>(apvts,"crowdBehavior","AUDIENCE BEHAVIOR",edited);crowdEventChoice=std::make_unique<Choice>(apvts,"crowdEventType","AUDIENCE EVENT",edited);feedbackDivisionChoice=std::make_unique<Choice>(apvts,"feedbackDivision","HOWL GRID",edited);feedbackLengthChoice=std::make_unique<Choice>(apvts,"feedbackLengthDivision","HOWL LENGTH",edited);crowdDivisionChoice=std::make_unique<Choice>(apvts,"crowdDivision","CROWD GRID",edited);crowdLengthChoice=std::make_unique<Choice>(apvts,"crowdLengthDivision","CROWD LENGTH",edited);for(auto*c:{micChoice.get(),venueChoice.get(),paChoice.get(),crowdBedChoice.get(),crowdBehaviorChoice.get(),crowdEventChoice.get(),feedbackDivisionChoice.get(),feedbackLengthChoice.get(),crowdDivisionChoice.get(),crowdLengthChoice.get()})addAndMakeVisible(*c);
    addKnob("proximity","PROXIMITY","",EditorMode::simple);addKnob("micDrive","MIC PREAMP","",EditorMode::simple);addKnob("paDrive","PA DRIVE","",EditorMode::simple);addKnob("monitorLevel","MONITOR","",EditorMode::simple);addKnob("room","VENUE SIZE","",EditorMode::simple);addKnob("crowdLevel","AUDIENCE","",EditorMode::simple);addKnob("crowdMood","BASE ENERGY","",EditorMode::simple);addKnob("crowdResponse","REACTION","",EditorMode::simple);addKnob("venueBedLevel","VENUE HUM","",EditorMode::simple);addKnob("mix","MIX","",EditorMode::simple);addKnob("outGain","OUTPUT","",EditorMode::simple);
    addKnob("proximity","PROXIMITY","",EditorMode::advanced);addKnob("micDrive","MIC PREAMP","",EditorMode::advanced);addKnob("paDrive","PA DRIVE","",EditorMode::advanced);addKnob("monitorLevel","MONITOR","",EditorMode::advanced);addKnob("feedbackAmount","HOWL AMOUNT","",EditorMode::advanced);addKnob("fbFreq","HOWL FREQ"," Hz",EditorMode::advanced);addKnob("ringQ","HOWL Q","",EditorMode::advanced);addKnob("fbDelayMs","LOOP DELAY"," ms",EditorMode::advanced);addKnob("fbTone","HOWL TONE","",EditorMode::advanced);addKnob("feedbackBuildMs","HOWL BUILD"," ms",EditorMode::advanced);addKnob("feedbackReleaseMs","HOWL RELEASE"," ms",EditorMode::advanced);addKnob("stageBleed","STAGE BLEED","",EditorMode::advanced);addKnob("crowdLevel","AUDIENCE","",EditorMode::advanced);addKnob("crowdMood","BASE ENERGY","",EditorMode::advanced);addKnob("crowdSensitivity","LISTENING","",EditorMode::advanced);addKnob("crowdResponse","REACTION","",EditorMode::advanced);addKnob("crowdCooldownMs","RECOVERY"," ms",EditorMode::advanced);addKnob("venueBedLevel","VENUE HUM","",EditorMode::advanced);addKnob("electricalNoise","ELECTRICAL","",EditorMode::advanced);addKnob("room","VENUE SIZE","",EditorMode::advanced);addKnob("wallAbsorption","ABSORPTION","",EditorMode::advanced);addKnob("stereoWidth","VENUE WIDTH","",EditorMode::advanced);addKnob("inputGain","INPUT","",EditorMode::advanced);addKnob("mix","MIX","",EditorMode::advanced);addKnob("limit","SAFETY","",EditorMode::advanced);addKnob("ceiling","CEILING","",EditorMode::advanced);addKnob("outGain","OUTPUT","",EditorMode::advanced);
    addKnob("feedbackAmount","HOWL AMOUNT","",EditorMode::performer);addKnob("fbFreq","HOWL FREQ"," Hz",EditorMode::performer);addKnob("ringQ","HOWL Q","",EditorMode::performer);addKnob("feedbackDurationMs","HOWL EVENT"," ms",EditorMode::performer);addKnob("crowdStrength","REACTION LEVEL","",EditorMode::performer);addKnob("crowdDurationMs","MANUAL LENGTH"," ms",EditorMode::performer);addKnob("crowdLevel","AMBIENT CROWD","",EditorMode::performer);addKnob("crowdMood","BASE ENERGY","",EditorMode::performer);addKnob("crowdSensitivity","LISTENING","",EditorMode::performer);addKnob("crowdResponse","REACTION","",EditorMode::performer);addKnob("crowdCooldownMs","RECOVERY"," ms",EditorMode::performer);addKnob("inputGain","INPUT","",EditorMode::performer);addKnob("mix","MIX","",EditorMode::performer);addKnob("limit","SAFETY","",EditorMode::performer);addKnob("ceiling","CEILING","",EditorMode::performer);addKnob("outGain","OUTPUT","",EditorMode::performer);
    feedbackTrigger.onClick=[this]{setParameter("feedbackConducted",1);processor.triggerFeedback();markCustom();};crowdTrigger.onClick=[this]{processor.triggerCrowd();markCustom();};setResizable(true,true);setResizeLimits(980,660,1600,1100);setSize(1120,720);showMode(EditorMode::simple);startTimerHz(24);
}

OpenMicNightAudioProcessorEditor::~OpenMicNightAudioProcessorEditor() { stopTimer();setLookAndFeel(nullptr); }

void OpenMicNightAudioProcessorEditor::addKnob(const char* id, const char* text, const char* suffix, EditorMode mode)
{
    auto control = std::make_unique<Knob>(apvts, id, text, suffix,[this]{processor.materialiseLegacyMacros();markCustom();}); auto* raw = control.get(); addAndMakeVisible(*raw);
    if(mode==EditorMode::simple)surfaceKnobs.push_back(raw);else if(mode==EditorMode::advanced)advancedKnobs.push_back(raw);else performerKnobs.push_back(raw);
    knobMap[std::string(id)+":"+std::to_string((int)mode)]=raw;knobs.push_back(std::move(control));
}

OpenMicNightAudioProcessorEditor::Knob* OpenMicNightAudioProcessorEditor::knob(const char* id) const
{
    for (const auto& entry : knobs) if (entry->id == id) return entry.get(); return nullptr;
}

void OpenMicNightAudioProcessorEditor::showMode(EditorMode mode)
{
    currentMode=mode;surfaceButton.setToggleState(mode==EditorMode::simple,juce::dontSendNotification);advancedButton.setToggleState(mode==EditorMode::advanced,juce::dontSendNotification);performerButton.setToggleState(mode==EditorMode::performer,juce::dontSendNotification);stage.setVisible(true);
    for(auto*c:surfaceKnobs)c->setVisible(mode==EditorMode::simple);for(auto*c:advancedKnobs)c->setVisible(mode==EditorMode::advanced);for(auto*c:performerKnobs)c->setVisible(mode==EditorMode::performer);
    const auto performer=mode==EditorMode::performer;feedbackArm.setVisible(true);venueBed.setVisible(!performer);crowdBedChoice->setVisible(true);crowdBehaviorChoice->setVisible(true);conductedFeedback.setVisible(performer);feedbackSync.setVisible(performer);feedbackLengthSync.setVisible(performer);crowdSync.setVisible(performer);crowdLengthSync.setVisible(performer);feedbackTrigger.setVisible(performer);crowdTrigger.setVisible(performer);crowdEventChoice->setVisible(performer);feedbackDivisionChoice->setVisible(performer);feedbackLengthChoice->setVisible(performer);crowdDivisionChoice->setVisible(performer);crowdLengthChoice->setVisible(performer);
    resized(); repaint();
}

void OpenMicNightAudioProcessorEditor::setParameter(const char* id, float value)
{
    if (auto* parameter = apvts.getParameter(id)) parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void OpenMicNightAudioProcessorEditor::applyPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(std::size(presets))) return;
    suppressPresetChanges=true;
    for(auto*p:processor.getParameters())p->setValueNotifyingHost(p->getDefaultValue());bool direct=false;for(const auto&[id,v]:presets[index].values)if(juce::String(id)=="macroLink"&&v<.5)direct=true;
    if(direct){for(const auto&[id,v]:presets[index].values)if(juce::String(id)!="macroLink")setParameter(id,(float)v);setParameter("macroLink",0);}else{for(const auto&[id,v]:presets[index].values)if(juce::String(id)!="macroLink")setParameter(id,(float)v);setParameter("macroLink",1);processor.materialiseLegacyMacros();for(const auto&[id,v]:presets[index].values){const auto name=juce::String(id);if(name!="hotMic"&&name!="wall"&&name!="room"&&name!="macroLink")setParameter(id,(float)v);}setParameter("macroLink",0);}
    setParameter("feedbackArm",0);
    apvts.state.setProperty("factoryPresetName",presets[index].name,nullptr);suppressPresetChanges=false;
}

void OpenMicNightAudioProcessorEditor::markCustom()
{
    if(!suppressPresetChanges){presetBox.setSelectedId(1,juce::dontSendNotification);apvts.state.setProperty("factoryPresetName","Custom",nullptr);}
}

void OpenMicNightAudioProcessorEditor::timerCallback()
{
    stage.repaint();
    statusLabel.setText(processor.safetyEngaged()?"SAFETY REDUCTION ACTIVE":processor.crowdEventActive()?"AUDIENCE REACTING":processor.feedbackEventActive()?"CONDUCTED HOWL ACTIVE":processor.inputEnergyActivity()>.12f?"AUDIENCE LISTENING":"CROWD / ROOM / FEEDBACK ROUTED",juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, processor.safetyEngaged() ? juce::Colour(magenta) : juce::Colour(0xffa8afac));
}

void OpenMicNightAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ink));
    auto header = getLocalBounds().removeFromTop(126).toFloat(); g.setColour(juce::Colour(0xff121617)); g.fillRect(header);
    g.setColour(juce::Colour(cyan)); g.fillRect(0.0f, 0.0f, getWidth() * .32f, 2.0f); g.setColour(juce::Colour(magenta)); g.fillRect(getWidth() * .78f, 0.0f, getWidth() * .22f, 2.0f);
    auto body = getLocalBounds().withTrimmedTop(126).reduced(12).toFloat(); g.setColour(juce::Colour(panel)); g.fillRoundedRectangle(body, 9.0f); g.setColour(juce::Colour(0xff464d4b)); g.drawRoundedRectangle(body, 9.0f, 1.0f);
}

void OpenMicNightAudioProcessorEditor::resized()
{
    auto area = getLocalBounds(); auto header = area.removeFromTop(126).reduced(18, 10);
    const auto identityWidth = juce::jlimit(250, 310, (int) std::round(header.getWidth() * .31f));
    const auto profileWidth = juce::jlimit(190, 235, (int) std::round(header.getWidth() * .24f));
    auto left = header.removeFromLeft(identityWidth); brand.setBounds(left.removeFromTop(18)); title.setBounds(left.removeFromTop(38)); subtitle.setBounds(left.removeFromTop(22));
    auto profile = header.removeFromRight(profileWidth); profileLabel.setBounds(profile.removeFromTop(18)); presetBox.setBounds(profile.removeFromTop(30)); statusLabel.setBounds(profile.removeFromBottom(28));
    const auto choiceWidth = header.getWidth() / 3; micChoice->setBounds(header.removeFromLeft(choiceWidth).reduced(4, 0)); venueChoice->setBounds(header.removeFromLeft(choiceWidth).reduced(4, 0)); paChoice->setBounds(header.reduced(4, 0));
    area=area.reduced(18,14);auto mode=area.removeFromTop(40);surfaceButton.setBounds(mode.removeFromLeft(112).reduced(2));advancedButton.setBounds(mode.removeFromLeft(112).reduced(2));performerButton.setBounds(mode.removeFromLeft(124).reduced(2));area.removeFromTop(8);auto visual=area.removeFromLeft((int)std::round(area.getWidth()*.36f));stage.setBounds(visual.reduced(4));auto controls=area.reduced(5);auto audienceRow=controls.removeFromTop(52);crowdBedChoice->setBounds(audienceRow.removeFromLeft(audienceRow.getWidth()/2).reduced(2));crowdBehaviorChoice->setBounds(audienceRow.reduced(2));
    if(currentMode==EditorMode::simple){auto toggles=controls.removeFromTop(40);feedbackArm.setBounds(toggles.removeFromLeft(toggles.getWidth()/2).reduced(3,2));venueBed.setBounds(toggles.reduced(3,2));const auto columns=3,rows=4,cellW=controls.getWidth()/columns,cellH=controls.getHeight()/rows;for(int i=0;i<(int)surfaceKnobs.size();++i)surfaceKnobs[(std::size_t)i]->setBounds(controls.getX()+(i%columns)*cellW,controls.getY()+(i/columns)*cellH,cellW,cellH);}
    else if(currentMode==EditorMode::advanced){auto toggles=controls.removeFromTop(38);feedbackArm.setBounds(toggles.removeFromLeft(toggles.getWidth()/2).reduced(3,2));venueBed.setBounds(toggles.reduced(3,2));const auto columns=6,rows=5,cellW=controls.getWidth()/columns,cellH=controls.getHeight()/rows;for(int i=0;i<(int)advancedKnobs.size();++i)advancedKnobs[(std::size_t)i]->setBounds(controls.getX()+(i%columns)*cellW,controls.getY()+(i/columns)*cellH,cellW,cellH);}
    else{auto row1=controls.removeFromTop(38);const auto r1w=row1.getWidth()/4;feedbackArm.setBounds(row1.removeFromLeft(r1w).reduced(2));conductedFeedback.setBounds(row1.removeFromLeft(r1w).reduced(2));feedbackTrigger.setBounds(row1.removeFromLeft(r1w).reduced(2));crowdTrigger.setBounds(row1.reduced(2));auto row2=controls.removeFromTop(52);const auto r2w=row2.getWidth()/4;feedbackSync.setBounds(row2.removeFromLeft(r2w).reduced(2,7));feedbackDivisionChoice->setBounds(row2.removeFromLeft(r2w).reduced(2));feedbackLengthSync.setBounds(row2.removeFromLeft(r2w).reduced(2,7));feedbackLengthChoice->setBounds(row2.reduced(2));auto row3=controls.removeFromTop(52);const auto r3w=row3.getWidth()/5;crowdSync.setBounds(row3.removeFromLeft(r3w).reduced(2,7));crowdDivisionChoice->setBounds(row3.removeFromLeft(r3w).reduced(2));crowdLengthSync.setBounds(row3.removeFromLeft(r3w).reduced(2,7));crowdLengthChoice->setBounds(row3.removeFromLeft(r3w).reduced(2));crowdEventChoice->setBounds(row3.reduced(2));const auto columns=4,rows=4,cellW=controls.getWidth()/columns,cellH=controls.getHeight()/rows;for(int i=0;i<(int)performerKnobs.size();++i)performerKnobs[(std::size_t)i]->setBounds(controls.getX()+(i%columns)*cellW,controls.getY()+(i/columns)*cellH,cellW,cellH);}
}
