#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr std::uint32_t ink=0xff090b09, deep=0xff151612, panel=0xff202119, line=0xff5b5c46;
constexpr std::uint32_t bone=0xffeee8cf, dim=0xffaaa88e, phosphor=0xffc8ec73, amber=0xffffb84f, red=0xffff5a47;
juce::Font font(float size, bool bold=false) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }
struct Setting { const char* id; float value; };
struct Preset { const char* name; std::initializer_list<Setting> values; };
const Preset presets[] {
 {"Living Room CRT",{{"setModel",1},{"receptionMode",1},{"vibe",.44f},{"speaker",.58f},{"agc",.22f},{"static",.08f},{"hum",.11f},{"whine",.06f},{"bedEnable",1},{"bedLevel",.28f}}},
 {"Late Night Broadcast",{{"setModel",1},{"receptionMode",1},{"vibe",.34f},{"speaker",.68f},{"agc",.18f},{"static",.07f},{"hum",.09f},{"whine",.05f},{"bedEnable",1},{"bedLevel",.25f}}},
 {"Small Kitchen TV",{{"setModel",3},{"receptionMode",1},{"vibe",.58f},{"speaker",.30f},{"agc",.36f},{"static",.13f},{"hum",.14f},{"whine",.08f},{"bedEnable",1},{"bedLevel",.30f}}},
 {"Antenna Snow",{{"setModel",0},{"receptionMode",1},{"vibe",.52f},{"speaker",.42f},{"agc",.24f},{"static",.42f},{"hum",.08f},{"whine",.05f}}},
 {"VHS Playback",{{"setModel",1},{"receptionMode",0},{"vibe",.48f},{"speaker",.56f},{"agc",.26f},{"static",.06f},{"hum",.12f},{"whine",.07f},{"bedEnable",1},{"bedLevel",.24f}}},
 {"Dying Portable",{{"setModel",0},{"receptionMode",3},{"vibe",.86f},{"speaker",.18f},{"agc",.72f},{"static",.52f},{"hum",.42f},{"whine",.34f},{"bedEnable",1},{"bedLevel",.38f},{"limiter",.82f},{"ceiling",.84f},{"outGain",.90f}}},
 {"Lost Broadcast",{{"setModel",4},{"receptionMode",3},{"vibe",.70f},{"speaker",.32f},{"agc",.50f},{"static",.76f},{"hum",.32f},{"whine",.62f},{"bedEnable",1},{"bedLevel",.48f},{"ceiling",.86f}}},
 {"Broadcast Monitor",{{"setModel",2},{"receptionMode",0},{"vibe",.10f},{"speaker",.90f},{"agc",.08f},{"static",.005f},{"hum",.015f},{"whine",.025f}}},
 {"Motel Cable",{{"setModel",4},{"receptionMode",2},{"vibe",.56f},{"speaker",.38f},{"agc",.42f},{"static",.15f},{"hum",.20f},{"whine",.09f},{"bedEnable",1},{"bedLevel",.30f}}},
 {"Wood Console",{{"setModel",1},{"receptionMode",1},{"vibe",.52f},{"speaker",.72f},{"agc",.28f},{"static",.08f},{"hum",.22f},{"whine",.06f},{"bedEnable",1},{"bedLevel",.36f}}},
 {"Detuned Channel",{{"setModel",0},{"receptionMode",3},{"vibe",.64f},{"speaker",.30f},{"agc",.44f},{"static",.82f},{"hum",.16f},{"whine",.18f}}},
 {"Public Access",{{"setModel",3},{"receptionMode",2},{"vibe",.46f},{"speaker",.46f},{"agc",.55f},{"static",.13f},{"hum",.14f},{"whine",.07f}}},
 {"Basement Set",{{"setModel",4},{"receptionMode",1},{"vibe",.72f},{"speaker",.26f},{"agc",.38f},{"static",.28f},{"hum",.40f},{"whine",.16f},{"bedEnable",1},{"bedLevel",.44f}}},
 {"Newsroom Monitor",{{"setModel",2},{"receptionMode",2},{"vibe",.18f},{"speaker",.82f},{"agc",.48f},{"static",.025f},{"hum",.025f},{"whine",.035f}}},
 {"Saturday Morning",{{"setModel",1},{"receptionMode",1},{"vibe",.38f},{"speaker",.60f},{"agc",.32f},{"static",.12f},{"hum",.10f},{"whine",.08f},{"bedEnable",1},{"bedLevel",.27f}}},
 // This preset was already authored outside the legacy macro map. Its former
 // detail defaults are explicit so the V3 canonical defaults cannot alter it.
 {"Power Brownout",{{"setModel",4},{"receptionMode",2},{"static",.30f},{"hum",.58f},{"whine",.24f},{"macroLink",0},{"hpHz",70},{"lpHz",9000},{"midHumpDb",1.2f},{"midFreq",1800},{"noiseHiss",.38f},{"noiseCrackle",.08f},{"drive",.45f},{"comp",.22f},{"tunerDrift",.08f},{"powerSag",.88f},{"syncInstability",.68f},{"cabinet",.55f},{"cabinetRattle",.62f},{"limiter",.88f},{"ceiling",.80f},{"outGain",.92f}}},
 {"PLAY - CRT Guitar Cabinet",{{"setModel",1},{"receptionMode",0},{"vibe",.38f},{"speaker",.76f},{"agc",.10f},{"static",.018f},{"hum",.035f},{"whine",.03f},{"bedEnable",1},{"bedLevel",.18f},{"mix",.82f},{"limiter",.86f},{"ceiling",.90f},{"outGain",.94f}}},
 {"PLAY - Broadcast Drum Room",{{"setModel",2},{"receptionMode",2},{"vibe",.22f},{"speaker",.64f},{"agc",.28f},{"static",.025f},{"hum",.02f},{"whine",.02f},{"bedEnable",0},{"mix",.62f},{"limiter",.90f},{"ceiling",.88f},{"outGain",.94f}}},
 {"PLAY - Phosphor Synth Bed",{{"setModel",1},{"receptionMode",1},{"vibe",.42f},{"speaker",.54f},{"agc",.16f},{"static",.025f},{"hum",.045f},{"whine",.035f},{"bedEnable",1},{"bedLevel",.16f},{"mix",.72f},{"limiter",.86f},{"ceiling",.90f},{"outGain",.94f}}},
 {"PLAY - Clocked Tuner Fault",{{"setModel",0},{"receptionMode",2},{"vibe",.46f},{"speaker",.42f},{"agc",.24f},{"static",.06f},{"hum",.04f},{"whine",.03f},{"faultTempoSync",1},{"faultDivision",3},{"faultProbability",.28f},{"faultStrength",.42f},{"faultDurationSync",1},{"faultLengthDivision",5},{"mix",.76f},{"limiter",.90f},{"ceiling",.88f},{"outGain",.92f}}}
};
}

void TelevisionEngineAudioProcessorEditor::Panel::paint(juce::Graphics& g)
{
    auto bounds=getLocalBounds().toFloat().reduced(1); g.setColour(juce::Colour(panel)); g.fillRoundedRectangle(bounds,7);
    g.setColour(juce::Colour(line)); g.drawRoundedRectangle(bounds,7,1); g.setColour(juce::Colour(phosphor));
    g.fillRect(bounds.getX()+12,bounds.getY()+12,24.0f,1.5f); g.setColour(juce::Colour(dim)); g.setFont(font(9.5f,true));
    g.drawText(name,getLocalBounds().removeFromTop(34).withTrimmedLeft(44),juce::Justification::centredLeft);
}

TelevisionEngineAudioProcessorEditor::Knob::Knob(APVTS& state,const juce::String& id,const juce::String& title,std::function<void()> changed)
{
    label.setText(title,juce::dontSendNotification); label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId,juce::Colour(bone)); label.setFont(font(9.5f,true)); addAndMakeVisible(label);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,72,18);
    slider.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(phosphor)); slider.setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour(0xff4d4e3e));
    slider.setColour(juce::Slider::thumbColourId,juce::Colour(bone)); slider.setColour(juce::Slider::textBoxTextColourId,juce::Colour(bone));
    slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(ink)); slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(line));
    slider.onDragStart=std::move(changed); addAndMakeVisible(slider); attachment=std::make_unique<APVTS::SliderAttachment>(state,id,slider);
}
void TelevisionEngineAudioProcessorEditor::Knob::resized() { auto area=getLocalBounds(); label.setBounds(area.removeFromTop(18)); slider.setBounds(area.reduced(1)); }

TelevisionEngineAudioProcessorEditor::Choice::Choice(APVTS& state,const juce::String& id,const juce::String& title,std::function<void()> changed)
{
    label.setText(title,juce::dontSendNotification); label.setColour(juce::Label::textColourId,juce::Colour(bone)); label.setFont(font(9.5f,true)); addAndMakeVisible(label);
    if(auto* parameter=dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id))) for(auto i=0;i<parameter->choices.size();++i) combo.addItem(parameter->choices[i],i+1);
    combo.setColour(juce::ComboBox::backgroundColourId,juce::Colour(ink)); combo.setColour(juce::ComboBox::textColourId,juce::Colour(bone)); combo.setColour(juce::ComboBox::outlineColourId,juce::Colour(line));
    combo.onChange=std::move(changed); addAndMakeVisible(combo); attachment=std::make_unique<APVTS::ComboBoxAttachment>(state,id,combo);
}
void TelevisionEngineAudioProcessorEditor::Choice::resized() { auto area=getLocalBounds(); label.setBounds(area.removeFromTop(17)); combo.setBounds(area.removeFromTop(32).reduced(1)); }

TelevisionEngineAudioProcessorEditor::Switch::Switch(APVTS& state,const juce::String& id,const juce::String& title,std::function<void()> changed)
{
    button.setButtonText(title); button.setColour(juce::ToggleButton::textColourId,juce::Colour(bone)); button.setColour(juce::ToggleButton::tickColourId,juce::Colour(amber));
    button.onClick=std::move(changed); addAndMakeVisible(button); attachment=std::make_unique<APVTS::ButtonAttachment>(state,id,button);
}

void TelevisionEngineAudioProcessorEditor::CrtDisplay::setState(std::array<float,64> waveform,float il,float ir,float ol,float oright,float bedValue,float snowValue,float electricalValue,float rattleValue,float syncValue,int modelValue,int receptionValue)
{
    trace=waveform; input={il,ir}; output={ol,oright}; bed=bedValue; snow=snowValue; electrical=electricalValue; rattle=rattleValue; sync=syncValue;
    model=modelValue; reception=receptionValue; phase+=.035f; repaint();
}

void TelevisionEngineAudioProcessorEditor::CrtDisplay::paint(juce::Graphics& g)
{
    auto outer=getLocalBounds().toFloat().reduced(1); g.setColour(juce::Colour(0xff342c20)); g.fillRoundedRectangle(outer,13);
    g.setColour(juce::Colour(0xff75654b)); g.drawRoundedRectangle(outer,13,1.5f); auto bezel=outer.reduced(20).withTrimmedBottom(145);
    g.setColour(juce::Colour(0xff151510)); g.fillRoundedRectangle(bezel,24); auto screen=bezel.reduced(17);
    g.setColour(juce::Colour(0xff07100b)); g.fillRoundedRectangle(screen,19); g.setColour(juce::Colour(0xff263a28)); g.drawRoundedRectangle(screen,19,1);
    for(auto y=(int)screen.getY()+4;y<(int)screen.getBottom();y+=5){g.setColour(juce::Colour(0x1820ff80));g.drawHorizontalLine(y,screen.getX()+8,screen.getRight()-8);}
    const auto snowAmount=juce::jlimit(0.0f,1.0f,std::sqrt(snow*18.0f));
    for(auto i=0;i<(int)(100*snowAmount);++i){const auto x=screen.getX()+std::fmod(i*61.7f+phase*93,screen.getWidth()),y=screen.getY()+std::fmod(i*37.3f+phase*47,screen.getHeight());g.setColour(juce::Colour(i%5?0x386fae72:0x70dfffaa));g.fillRect(x,y,1.4f,1.2f);}
    juce::Path wave; for(std::size_t i=0;i<trace.size();++i){const auto x=screen.getX()+screen.getWidth()*(float)i/(float)(trace.size()-1),y=screen.getCentreY()-juce::jlimit(-1.0f,1.0f,trace[i])*screen.getHeight()*.42f;if(i==0)wave.startNewSubPath(x,y);else wave.lineTo(x,y);}
    g.setColour(juce::Colour(sync>0?red:phosphor)); g.strokePath(wave,juce::PathStrokeType(sync>0?2.7f:1.6f));
    const char* models[]{"PORTABLE","CONSOLE","BROADCAST","KITCHEN","MOTEL"}; const char* modes[]{"BASEBAND","ANTENNA","CABLE","DETUNED"};
    g.setFont(font(9,true));g.setColour(juce::Colour(phosphor));g.drawText(models[juce::jlimit(0,4,model)],screen.toNearestInt().reduced(13,8),juce::Justification::topLeft);g.drawText(modes[juce::jlimit(0,3,reception)],screen.toNearestInt().reduced(13,8),juce::Justification::topRight);
    auto footer=outer.reduced(18).removeFromBottom(126); auto io=footer.removeFromTop(48); const char* ioNames[]{"IN L","IN R","OUT L","OUT R"}; const float ioValues[]{input[0],input[1],output[0],output[1]};
    for(auto i=0;i<4;++i){auto row=io.removeFromTop(12.0f);g.setColour(juce::Colour(dim));g.setFont(font(8,true));g.drawText(ioNames[i],row.removeFromLeft(34),juce::Justification::centredLeft);auto bar=row.reduced(2,3);g.setColour(juce::Colour(0xff474536));g.fillRect(bar);g.setColour(juce::Colour(i<2?amber:phosphor));g.fillRect(bar.withWidth(bar.getWidth()*juce::jlimit(0.0f,1.0f,ioValues[i]*2.4f)));}
    footer.removeFromTop(5); const char* names[]{"CRT BED","TUNER","ELECTRICAL","RATTLE","SYNC"}; const float raw[]{bed,snow,electrical,rattle,sync}; const float scales[]{7,18,90,30,1};
    for(auto i=0;i<5;++i){auto row=footer.removeFromTop(14.0f);g.setColour(juce::Colour(dim));g.setFont(font(8,true));g.drawText(names[i],row.removeFromLeft(66),juce::Justification::centredLeft);auto bar=row.reduced(2,3);g.setColour(juce::Colour(0xff474536));g.fillRect(bar);const auto amount=juce::jlimit(0.0f,1.0f,std::sqrt(std::max(0.0f,raw[i])*scales[i]));g.setColour(juce::Colour(i==4?red:(i==0?amber:phosphor)));g.fillRect(bar.withWidth(bar.getWidth()*amount));}
}

TelevisionEngineAudioProcessorEditor::TelevisionEngineAudioProcessorEditor(TelevisionEngineAudioProcessor& owner)
    : AudioProcessorEditor(&owner), processor(owner), apvts(owner.getAPVTS())
{
    setOpaque(true); brandLabel.setText("B&E DIGITAL",juce::dontSendNotification); brandLabel.setColour(juce::Label::textColourId,juce::Colour(phosphor)); brandLabel.setFont(font(10,true)); addAndMakeVisible(brandLabel);
    titleLabel.setText("TELEVISION ENGINE",juce::dontSendNotification); titleLabel.setColour(juce::Label::textColourId,juce::Colour(bone)); titleLabel.setFont(font(27,true)); addAndMakeVisible(titleLabel);
    subtitleLabel.setText("CRT RECEIVER / CABINET PLAYBACK / V3 STEREO",juce::dontSendNotification); subtitleLabel.setColour(juce::Label::textColourId,juce::Colour(dim)); subtitleLabel.setFont(font(10,true)); addAndMakeVisible(subtitleLabel);
    profileLabel.setText("CHANNEL / SET PROFILE",juce::dontSendNotification); profileLabel.setColour(juce::Label::textColourId,juce::Colour(dim)); profileLabel.setFont(font(9,true)); addAndMakeVisible(profileLabel);
    presetBox.addItem("Custom",1); for(auto i=0;i<(int)std::size(presets);++i)presetBox.addItem(presets[i].name,i+2);const auto restoredPreset=apvts.state.getProperty("factoryPresetName","Custom").toString();auto restoredPresetId=1;for(auto i=0;i<(int)std::size(presets);++i)if(restoredPreset==presets[i].name)restoredPresetId=i+2;presetBox.setSelectedId(restoredPresetId,juce::dontSendNotification);
    presetBox.onChange=[this]{if(presetBox.getSelectedId()>=2)applyPreset(presetBox.getSelectedId()-2);}; addAndMakeVisible(presetBox);
    for(auto* button:{&simpleButton,&advancedButton,&performerButton}){button->setColour(juce::TextButton::textColourOffId,juce::Colour(bone));addAndMakeVisible(*button);}
    simpleButton.onClick=[this]{setView(View::simple);};advancedButton.onClick=[this]{setView(View::advanced);};performerButton.onClick=[this]{setView(View::performer);};
    statusLabel.setColour(juce::Label::textColourId,juce::Colour(dim));statusLabel.setFont(font(9,true));statusLabel.setJustificationType(juce::Justification::centredRight);addAndMakeVisible(statusLabel);
    addAndMakeVisible(display); addAndMakeVisible(simplePage); addAndMakeVisible(advancedPage); addAndMakeVisible(performerPage);
    for(auto* p:{&simpleCharacter,&simpleLayers})simplePage.addAndMakeVisible(*p);
    for(auto* p:{&tonePanel,&broadcastPanel,&cabinetPanel,&noisePanel,&outputPanel})advancedPage.addAndMakeVisible(*p);
    for(auto* p:{&performerTone,&performerLayers,&performerFault,&performerOutput})performerPage.addAndMakeVisible(*p);

    addChoice(simpleCharacter,"setModel","TELEVISION SET","Physical cabinet, driver, and amplifier family.");addChoice(simpleCharacter,"receptionMode","RECEPTION","Baseband, antenna, cable, or detuned input.");addKnob(simpleCharacter,"cabinet","CABINET","Set-body resonance.");addKnob(simpleCharacter,"drive","AMPLIFIER","Television output-stage saturation.");addKnob(simpleCharacter,"comp","AUTO LEVEL","Broadcast levelling and pump.");addKnob(simpleCharacter,"static","TUNER SNOW","Reception noise amount.");
    addSwitch(simpleLayers,"bedEnable","ARM CRT BED","Captured CRT mechanism layer.");addKnob(simpleLayers,"bedLevel","BED LEVEL","Captured CRT bed only.");addKnob(simpleLayers,"hum","MAINS HUM","60 Hz electrical leakage.");addKnob(simpleLayers,"whine","FLYBACK","Line whistle and protected subharmonic.");addKnob(simpleLayers,"cabinetRattle","RATTLE","Signal-excited loose cabinet parts.");addKnob(simpleLayers,"syncInstability","FREE FAULTS","Unclocked receiver sync failures.");addKnob(simpleLayers,"mix","MIX","Dry and television playback balance.");addKnob(simpleLayers,"outGain","OUTPUT","Final set output.");

    addKnob(tonePanel,"hpHz","HIGH-PASS","Small-speaker bass loss.");addKnob(tonePanel,"lpHz","LOW-PASS","Set and transmission bandwidth.");addKnob(tonePanel,"midHumpDb","MID BODY","Speech-band cabinet emphasis.");addKnob(tonePanel,"midFreq","MID Hz","Cabinet presence frequency.");
    addKnob(broadcastPanel,"drive","AMPLIFIER","Output-stage saturation.");addKnob(broadcastPanel,"comp","AUTO LEVEL","Broadcast compression.");addKnob(broadcastPanel,"tunerDrift","TUNER DRIFT","Slow reception wander.");addKnob(broadcastPanel,"syncInstability","FREE FAULTS","Random receiver sync losses.");
    addChoice(cabinetPanel,"setModel","SET MODEL","Physical television family.");addKnob(cabinetPanel,"cabinet","CABINET","Set-body resonance.");addKnob(cabinetPanel,"cabinetRattle","RATTLE","Signal-excited loose parts.");addKnob(cabinetPanel,"powerSag","POWER SAG","Aging power supply modulation.");
    addChoice(noisePanel,"receptionMode","RECEPTION","Incoming signal path.");addKnob(noisePanel,"static","TUNER SNOW","Reception noise level.");addKnob(noisePanel,"noiseHiss","SNOW TONE","Tuner-noise spectral contour.");addKnob(noisePanel,"noiseCrackle","CRACKLE","Sparse tuner impulses.");addKnob(noisePanel,"hum","MAINS HUM","60 Hz leakage.");addKnob(noisePanel,"whine","FLYBACK","Line whistle.");addSwitch(noisePanel,"bedEnable","ARM CRT BED","Captured CRT mechanism.");addKnob(noisePanel,"bedLevel","BED LEVEL","Captured bed level.");
    addKnob(outputPanel,"inputGain","INPUT dB","Input trim.");addKnob(outputPanel,"mix","MIX","Dry/wet balance.");addKnob(outputPanel,"limiter","LIMITER","Output protection.");addKnob(outputPanel,"ceiling","CEILING","Hard output ceiling.");addKnob(outputPanel,"outGain","OUTPUT","Final trim.");

    addChoice(performerTone,"setModel","SET","Physical television family.");addKnob(performerTone,"cabinet","CABINET","Set-body resonance.");addKnob(performerTone,"drive","DRIVE","Amplifier saturation.");addKnob(performerTone,"comp","AUTO LEVEL","Broadcast levelling.");addKnob(performerTone,"hpHz","HIGH-PASS","Speaker bass cutoff.");addKnob(performerTone,"lpHz","LOW-PASS","Speaker treble cutoff.");
    addSwitch(performerLayers,"bedEnable","CRT BED","Enable captured CRT bed.");addKnob(performerLayers,"bedLevel","BED LEVEL","CRT layer only.");addKnob(performerLayers,"static","TUNER","Reception noise only.");addKnob(performerLayers,"hum","HUM","Mains leakage only.");addKnob(performerLayers,"whine","FLYBACK","Line whistle only.");addKnob(performerLayers,"cabinetRattle","RATTLE","Excited cabinet hardware only.");
    addKnob(performerFault,"tunerDrift","TUNER DRIFT","Physical reception wander remains free-running.");addKnob(performerFault,"syncInstability","FREE FAULTS","Random faults used when clock sync is off.");addSwitch(performerFault,"faultTempoSync","CLOCK SYNC","Replace random faults with host-grid triggers.");addChoice(performerFault,"faultDivision","TRIGGER GRID","Musical trigger division.");addKnob(performerFault,"faultProbability","PROBABILITY","Deterministic chance per grid step.");addKnob(performerFault,"faultStrength","STRENGTH","Fault depth.");addSwitch(performerFault,"faultDurationSync","SYNC LENGTH","Use a musical fault duration.");addChoice(performerFault,"faultLengthDivision","FAULT LENGTH","Musical duration when synced.");addKnob(performerFault,"faultDurationMs","LENGTH ms","Free fault duration.");
    triggerButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff5e251f));triggerButton.setColour(juce::TextButton::textColourOffId,juce::Colour(bone));triggerButton.onClick=[this]{processor.triggerSyncFault();};performerFault.addAndMakeVisible(triggerButton);panelItems[&performerFault].push_back(&triggerButton);
    addKnob(performerOutput,"inputGain","INPUT dB","Input trim.");addKnob(performerOutput,"mix","MIX","Dry/wet balance.");addKnob(performerOutput,"outGain","OUTPUT","Final trim.");addKnob(performerOutput,"limiter","LIMITER","Output protection.");addKnob(performerOutput,"ceiling","CEILING","Hard output ceiling.");

    setResizable(true,true);setResizeLimits(960,640,1600,1000);setSize(1180,740);setView(View::simple);startTimerHz(30);
}

TelevisionEngineAudioProcessorEditor::~TelevisionEngineAudioProcessorEditor(){stopTimer();}
TelevisionEngineAudioProcessorEditor::Knob* TelevisionEngineAudioProcessorEditor::addKnob(Panel& p,const juce::String& id,const juce::String& title,const juce::String& hint){auto c=std::make_unique<Knob>(apvts,id,title,[this]{markCustom();});c->setHint(hint);auto* raw=c.get();p.addAndMakeVisible(*raw);panelItems[&p].push_back(raw);knobs.push_back(std::move(c));return raw;}
TelevisionEngineAudioProcessorEditor::Choice* TelevisionEngineAudioProcessorEditor::addChoice(Panel& p,const juce::String& id,const juce::String& title,const juce::String& hint){auto c=std::make_unique<Choice>(apvts,id,title,[this]{markCustom();});c->setHint(hint);auto* raw=c.get();p.addAndMakeVisible(*raw);panelItems[&p].push_back(raw);choices.push_back(std::move(c));return raw;}
TelevisionEngineAudioProcessorEditor::Switch* TelevisionEngineAudioProcessorEditor::addSwitch(Panel& p,const juce::String& id,const juce::String& title,const juce::String& hint){auto c=std::make_unique<Switch>(apvts,id,title,[this]{markCustom();});c->setHint(hint);auto* raw=c.get();p.addAndMakeVisible(*raw);panelItems[&p].push_back(raw);switches.push_back(std::move(c));return raw;}
void TelevisionEngineAudioProcessorEditor::layoutPanel(Panel& p,int columns){auto area=p.contentBounds();auto& items=panelItems[&p];if(items.empty())return;const auto cols=juce::jmax(1,columns),rows=juce::jmax(1,((int)items.size()+cols-1)/cols),w=area.getWidth()/cols,h=area.getHeight()/rows;for(auto i=0;i<(int)items.size();++i)items[(std::size_t)i]->setBounds(area.getX()+(i%cols)*w,area.getY()+(i/cols)*h,w,h);}
void TelevisionEngineAudioProcessorEditor::setView(View view){currentView=view;simplePage.setVisible(view==View::simple);advancedPage.setVisible(view==View::advanced);performerPage.setVisible(view==View::performer);simpleButton.setToggleState(view==View::simple,juce::dontSendNotification);advancedButton.setToggleState(view==View::advanced,juce::dontSendNotification);performerButton.setToggleState(view==View::performer,juce::dontSendNotification);resized();}
void TelevisionEngineAudioProcessorEditor::setParameter(const juce::String& id,float v){if(auto* p=apvts.getParameter(id))p->setValueNotifyingHost(p->convertTo0to1(v));}
float TelevisionEngineAudioProcessorEditor::getParameter(const juce::String& id)const{if(const auto* p=apvts.getRawParameterValue(id))return p->load();return 0;}
void TelevisionEngineAudioProcessorEditor::resetParameters(){for(auto* p:processor.getParameters())p->setValueNotifyingHost(p->getDefaultValue());}
void TelevisionEngineAudioProcessorEditor::applyPreset(int index){if(index<0||index>=(int)std::size(presets))return;suppressPresetChanges=true;resetParameters();setParameter("outGain",1);for(const auto& s:presets[index].values){const auto id=juce::String(s.id);if(id=="setModel"||id=="receptionMode"||id=="vibe"||id=="speaker"||id=="agc"||id=="static")setParameter(s.id,s.value);}setParameter("macroLink",1);processor.materialiseLegacyMacros();for(const auto& s:presets[index].values){const auto id=juce::String(s.id);if(id!="setModel"&&id!="receptionMode"&&id!="vibe"&&id!="speaker"&&id!="agc"&&id!="static"&&id!="macroLink")setParameter(s.id,s.value);}setParameter("macroLink",0);apvts.state.setProperty("factoryPresetName",presets[index].name,nullptr);suppressPresetChanges=false;}
void TelevisionEngineAudioProcessorEditor::markCustom(){if(!suppressPresetChanges){presetBox.setSelectedId(1,juce::dontSendNotification);apvts.state.setProperty("factoryPresetName","Custom",nullptr);}}
void TelevisionEngineAudioProcessorEditor::timerCallback(){display.setState(processor.outputTrace(),processor.inputPeak(0),processor.inputPeak(1),processor.outputPeak(0),processor.outputPeak(1),processor.bedLevelMeter(),processor.staticLevelMeter(),processor.electricalLevelMeter(),processor.rattleLevelMeter(),processor.syncProgressMeter(),(int)getParameter("setModel"),(int)getParameter("receptionMode"));if(processor.syncFaultActive())statusLabel.setText("SYNC FAULT / RECEIVER RECOVERING",juce::dontSendNotification);else if(processor.legacyMacrosActive())statusLabel.setText("LEGACY SESSION / SOUND PRESERVED",juce::dontSendNotification);else statusLabel.setText("CANONICAL DSP / ALL VIEWS SHARE ONE STATE",juce::dontSendNotification);}
void TelevisionEngineAudioProcessorEditor::paint(juce::Graphics& g){g.fillAll(juce::Colour(ink));auto bounds=getLocalBounds().toFloat().reduced(7);g.setColour(juce::Colour(deep));g.fillRoundedRectangle(bounds,7);g.setColour(juce::Colour(line));g.drawRoundedRectangle(bounds,7,1);g.setColour(juce::Colour(phosphor));g.fillRect(16.0f,7.0f,190.0f,2.0f);g.setColour(juce::Colour(amber));g.fillRect((float)getWidth()-206,7.0f,190.0f,2.0f);g.setColour(juce::Colour(0xff3c3d30));g.fillRect(15,91,getWidth()-30,1);}
void TelevisionEngineAudioProcessorEditor::resized()
{
    auto area=getLocalBounds().reduced(16),header=area.removeFromTop(86);const auto profileWidth=juce::jlimit(220,300,(int)std::round(header.getWidth()*.26f));auto profile=header.removeFromRight(profileWidth);header.removeFromRight(10);auto navColumn=header.removeFromRight(315);auto identity=header;
    brandLabel.setBounds(identity.removeFromTop(17));titleLabel.setBounds(identity.removeFromTop(36));subtitleLabel.setBounds(identity.removeFromTop(18));profileLabel.setBounds(profile.removeFromTop(17));presetBox.setBounds(profile.removeFromTop(32));auto nav=navColumn.removeFromBottom(35);simpleButton.setBounds(nav.removeFromLeft(97));nav.removeFromLeft(6);advancedButton.setBounds(nav.removeFromLeft(100));nav.removeFromLeft(6);performerButton.setBounds(nav.removeFromLeft(105));statusLabel.setBounds(navColumn);
    auto displayArea=area.removeFromLeft((int)std::round(area.getWidth()*.36f));display.setBounds(displayArea.reduced(3));auto pageArea=area.reduced(3);simplePage.setBounds(pageArea);advancedPage.setBounds(pageArea);performerPage.setBounds(pageArea);
    {auto page=simplePage.getLocalBounds();simpleCharacter.setBounds(page.removeFromTop((int)(page.getHeight()*.52f)).reduced(2));simpleLayers.setBounds(page.reduced(2));layoutPanel(simpleCharacter,3);layoutPanel(simpleLayers,4);}
    {auto page=advancedPage.getLocalBounds();const auto half=page.getWidth()/2,third=page.getHeight()/3;tonePanel.setBounds(page.getX(),page.getY(),half,third);broadcastPanel.setBounds(page.getX()+half,page.getY(),page.getWidth()-half,third);cabinetPanel.setBounds(page.getX(),page.getY()+third,half,third);noisePanel.setBounds(page.getX()+half,page.getY()+third,page.getWidth()-half,third);outputPanel.setBounds(page.getX(),page.getY()+third*2,page.getWidth(),page.getHeight()-third*2);layoutPanel(tonePanel,4);layoutPanel(broadcastPanel,4);layoutPanel(cabinetPanel,4);layoutPanel(noisePanel,4);layoutPanel(outputPanel,5);}
    {auto page=performerPage.getLocalBounds();const auto toneH=(int)(page.getHeight()*.34f),layersH=(int)(page.getHeight()*.29f);performerTone.setBounds(page.removeFromTop(toneH).reduced(1));performerLayers.setBounds(page.removeFromTop(layersH).reduced(1));const auto faultW=(int)(page.getWidth()*.72f);performerFault.setBounds(page.removeFromLeft(faultW).reduced(1));performerOutput.setBounds(page.reduced(1));layoutPanel(performerTone,6);layoutPanel(performerLayers,6);layoutPanel(performerFault,5);layoutPanel(performerOutput,2);}
}
