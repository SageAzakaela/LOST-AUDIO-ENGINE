#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr auto ink = 0xff080b0c;
constexpr auto panel = 0xff131718;
constexpr auto panel2 = 0xff202526;
constexpr auto cream = 0xfffff4e5;
constexpr auto cyan = 0xff52e7ee;
constexpr auto magenta = 0xffff4fb7;
constexpr auto amber = 0xffffb23f;

const juce::StringArray engines { "Empty", "Tape", "Transmission", "Comms", "CD", "Conference", "Camcorder", "Cartridge", "Television", "Occlusion", "Open Mic Night" };

struct SlotSetting { int engine; float a, b, model, mix; };
struct ChainPreset { const char* name; std::array<SlotSetting, 6> slots; float global1, global2; };
constexpr SlotSetting empty { 0, .35f, .2f, 0, 1 };
const std::array<ChainPreset,18> chainPresets {{
    { "Subtle Memory", std::array<SlotSetting,6>{SlotSetting{1,.22f,.10f,.18f,.72f},empty,empty,empty,empty,empty}, .5f,.5f },
    { "Clean Antique Voice", std::array<SlotSetting,6>{SlotSetting{3,.32f,.08f,.02f,.82f},SlotSetting{1,.26f,.12f,.26f,.58f},empty,empty,empty,empty}, .5f,.5f },
    { "Found Footage", std::array<SlotSetting,6>{SlotSetting{6,.48f,.31f,.12f,.90f},SlotSetting{1,.42f,.26f,.38f,.62f},SlotSetting{9,.34f,.25f,.14f,.46f},empty,empty,empty}, .5f,.5f },
    { "Dead Mall PA", std::array<SlotSetting,6>{SlotSetting{3,.61f,.22f,.76f,.91f},SlotSetting{9,.73f,.38f,.58f,.72f},SlotSetting{1,.33f,.15f,.44f,.48f},empty,empty,empty}, .5f,.5f },
    { "Haunted Television", std::array<SlotSetting,6>{SlotSetting{8,.67f,.51f,.78f,.92f},SlotSetting{1,.52f,.35f,.71f,.65f},SlotSetting{2,.34f,.29f,.62f,.44f},empty,empty,empty}, .5f,.5f },
    { "Bad Video Call Evidence", std::array<SlotSetting,6>{SlotSetting{5,.62f,.72f,.04f,.94f},SlotSetting{6,.38f,.35f,.62f,.57f},SlotSetting{2,.42f,.48f,.38f,.52f},empty,empty,empty}, .5f,.5f },
    { "Cartridge Through CRT", std::array<SlotSetting,6>{SlotSetting{7,.68f,.22f,.61f,.95f},SlotSetting{8,.53f,.31f,.29f,.81f},SlotSetting{1,.37f,.19f,.52f,.43f},empty,empty,empty}, .5f,.5f },
    { "CD From The Next Room", std::array<SlotSetting,6>{SlotSetting{4,.30f,.61f,.73f,.94f},SlotSetting{9,.81f,.44f,.18f,.88f},empty,empty,empty,empty}, .5f,.5f },
    { "Rooftop Open Mic Broadcast", std::array<SlotSetting,6>{SlotSetting{10,.44f,.36f,.74f,.88f},SlotSetting{2,.37f,.25f,.22f,.67f},SlotSetting{1,.29f,.14f,.48f,.42f},empty,empty,empty}, .5f,.5f },
    { "Lobby Security Monitor", std::array<SlotSetting,6>{SlotSetting{6,.58f,.28f,.68f,.86f},SlotSetting{3,.49f,.18f,.72f,.63f},SlotSetting{9,.67f,.29f,.07f,.74f},SlotSetting{8,.39f,.17f,.21f,.38f},empty,empty}, .5f,.5f },
    { "Abandoned Rehearsal", std::array<SlotSetting,6>{SlotSetting{10,.53f,.22f,.38f,.72f},SlotSetting{9,.72f,.51f,.42f,.81f},SlotSetting{1,.47f,.28f,.63f,.51f},empty,empty,empty}, .5f,.5f },
    { "Maximum Media Failure", std::array<SlotSetting,6>{SlotSetting{4,.71f,.77f,.94f,.86f},SlotSetting{7,.78f,.45f,.77f,.79f},SlotSetting{8,.74f,.68f,.92f,.71f},SlotSetting{2,.59f,.71f,.83f,.62f},SlotSetting{1,.66f,.58f,.89f,.55f},SlotSetting{9,.58f,.42f,.64f,.47f}}, .5f,.5f },
    { "PLAY - Guitar Found Amp", std::array<SlotSetting,6>{SlotSetting{8,.43f,.18f,.24f,.62f},SlotSetting{1,.34f,.16f,.31f,.48f},empty,empty,empty,empty}, .42f,.30f },
    { "PLAY - Drum Damage Bus", std::array<SlotSetting,6>{SlotSetting{1,.29f,.12f,.20f,.42f},SlotSetting{7,.37f,.16f,.34f,.34f},SlotSetting{4,.22f,.13f,.18f,.26f},empty,empty,empty}, .34f,.28f },
    { "PLAY - Synth Haunted Chain", std::array<SlotSetting,6>{SlotSetting{8,.48f,.22f,.66f,.52f},SlotSetting{9,.36f,.31f,.57f,.38f},SlotSetting{1,.27f,.14f,.46f,.30f},empty,empty,empty}, .39f,.33f },
    { "PLAY - Vocal Diegetic Stack", std::array<SlotSetting,6>{SlotSetting{3,.39f,.11f,.08f,.58f},SlotSetting{2,.28f,.12f,.16f,.34f},SlotSetting{1,.20f,.08f,.22f,.24f},empty,empty,empty}, .31f,.22f },
    { "PLAY - Bass CRT Cartridge", std::array<SlotSetting,6>{SlotSetting{8,.36f,.14f,.29f,.48f},SlotSetting{7,.31f,.13f,.42f,.31f},SlotSetting{1,.23f,.10f,.35f,.22f},empty,empty,empty}, .35f,.24f },
    { "PLAY - Sparse Performance Rig", std::array<SlotSetting,6>{SlotSetting{4,.18f,.10f,.21f,.24f},SlotSetting{5,.24f,.12f,.17f,.22f},SlotSetting{9,.27f,.15f,.33f,.20f},empty,empty,empty}, .28f,.20f }
}};

std::pair<juce::String, juce::String> macroNames(int engine)
{
    switch(engine)
    {
        case 1:return{"AGE","INSTABILITY"};case 2:return{"SIGNAL LOSS","CONNECTION DAMAGE"};case 3:return{"TRANSDUCER","LINE FAILURE"};
        case 4:return{"DISC WEAR","DAMAGE"};case 5:return{"CODEC","DROPOUTS"};case 6:return{"CAMERA AGE","CORRUPTION"};
        case 7:return{"CODEC GRIT","HARDWARE NOISE"};case 8:return{"SET CHARACTER","RECEPTION DAMAGE"};case 9:return{"BOUNDARY","ROOM BODY"};
        case 10:return{"HOT MIC","VENUE ENERGY"};default:return{"CHARACTER","DAMAGE"};
    }
}

std::array<juce::String, 6> detailNames(int engine)
{
    switch(engine)
    {
        case 1:return{"SPEED","WOW","FLUTTER","HISS","HUM","DROPOUT"};
        case 2:return{"BANDWIDTH","DRIVE","CRUSH","NOISE","DROPOUT","PASSES"};
        case 3:return{"BANDWIDTH","DRIVE","DISTANCE","ROOM","LINE NOISE","RATTLE"};
        case 4:return{"ERROR RATE","BURST","REPEAT","SCRATCH","TRACKING","SERVO"};
        case 5:return{"BANDWIDTH","PACKET LOSS","JITTER","ROBOT","NOISE GATE","COMFORT NOISE"};
        case 6:return{"COVERAGE","MOVEMENT","WIND","HANDLING","MOTOR","HISS"};
        case 7:return{"BIT DEPTH","SAMPLE RATE","JITTER","SPEAKER","ROOM","NOISE"};
        case 8:return{"SPEAKER","STATIC","MAINS HUM","FLYBACK","CABINET","CRT BED"};
        case 9:return{"BOUNDARY","DISTANCE","SOURCE ROOM","LISTENER ROOM","RESONANCE","EDGE LEAK"};
        case 10:return{"HOT MIC","VENUE SIZE","AUDIENCE BED","STAGE BLEED","ELECTRICAL","HOWL AMOUNT"};
        default:return{"CONTROL 1","CONTROL 2","CONTROL 3","CONTROL 4","CONTROL 5","CONTROL 6"};
    }
}
}

juce::String LostAudioSuiteEditor::slotId(int slot, const char* suffix){return "slot"+juce::String(slot+1)+suffix;}

LostAudioSuiteEditor::SuiteLookAndFeel::SuiteLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId,juce::Colour(panel2));setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff555d5b));setColour(juce::ComboBox::textColourId,juce::Colour(cream));
    setColour(juce::PopupMenu::backgroundColourId,juce::Colour(panel2));setColour(juce::PopupMenu::textColourId,juce::Colour(cream));setColour(juce::TextButton::buttonColourId,juce::Colour(panel2));
    setColour(juce::TextButton::textColourOffId,juce::Colour(cream));setColour(juce::Slider::textBoxTextColourId,juce::Colour(cream));setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(ink));setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff4d5553));
}

void LostAudioSuiteEditor::SuiteLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,juce::Slider&)
{
    auto bounds=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(7);const auto radius=juce::jmin(bounds.getWidth(),bounds.getHeight())*.5f;const auto centre=bounds.getCentre();const auto angle=start+position*(end-start);
    g.setColour(juce::Colour(0xff353b39));g.fillEllipse(bounds.withSizeKeepingCentre(radius*2,radius*2));juce::Path arc;arc.addCentredArc(centre.x,centre.y,radius-2,radius-2,0,start,angle,true);
    g.setColour(juce::Colour(cyan));g.strokePath(arc,juce::PathStrokeType(4,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));juce::Path pointer;pointer.addRoundedRectangle(-2,-radius+8,4,radius*.55f,2);
    g.setColour(juce::Colour(cream));g.fillPath(pointer,juce::AffineTransform::rotation(angle).translated(centre.x,centre.y));
}

void LostAudioSuiteEditor::SuiteLookAndFeel::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool,bool)
{
    auto r=b.getLocalBounds().toFloat().reduced(1);const auto on=b.getToggleState();g.setColour(on?juce::Colour(0xff5c173d):juce::Colour(panel2));g.fillRoundedRectangle(r,4);
    g.setColour(on?juce::Colour(magenta):juce::Colour(0xff5c6462));g.drawRoundedRectangle(r,4,1);g.setColour(on?juce::Colour(cream):juce::Colour(0xffaeb5b2));g.setFont(juce::Font(juce::FontOptions(11,juce::Font::bold)));g.drawFittedText(b.getButtonText(),b.getLocalBounds().reduced(5),juce::Justification::centred,1);
}

LostAudioSuiteEditor::Knob::Knob(APVTS& state,const juce::String& id,const juce::String& text,const juce::String& suffix)
{
    label.setText(text,juce::dontSendNotification);label.setJustificationType(juce::Justification::centred);label.setColour(juce::Label::textColourId,juce::Colour(0xffc2c9c6));
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,72,18);slider.setTextValueSuffix(suffix);addAndMakeVisible(label);addAndMakeVisible(slider);
    attachment=std::make_unique<APVTS::SliderAttachment>(state,id,slider);
}
void LostAudioSuiteEditor::Knob::resized(){auto a=getLocalBounds();label.setBounds(a.removeFromTop(19));slider.setBounds(a.reduced(2));}

LostAudioSuiteEditor::SlotCard::SlotCard(LostAudioSuiteEditor& o,APVTS& state,int p):owner(o),physical(p)
{
    number.setText("1",juce::dontSendNotification);number.setJustificationType(juce::Justification::centred);number.setColour(juce::Label::textColourId,juce::Colour(cyan));number.setFont(juce::Font(juce::FontOptions(17,juce::Font::bold)));
    for(int i=0;i<engines.size();++i)engine.addItem(engines[i],i+1);mix.setSliderStyle(juce::Slider::LinearHorizontal);mix.setTextBoxStyle(juce::Slider::TextBoxRight,false,44,18);mix.setNumDecimalPlacesToDisplay(2);
    up.setTooltip("Move this device earlier in the signal chain");down.setTooltip("Move this device later in the signal chain");
    up.setTitle("Move earlier");down.setTitle("Move later");
    up.onClick=[this]{owner.moveSlot(physical,-1);};down.onClick=[this]{owner.moveSlot(physical,1);};edit.onClick=[this]{owner.selectSlot(physical);};
    const std::array<juce::Component*,7> components{&number,&engine,&bypass,&mix,&edit,&up,&down};
    for(auto* c:components)addAndMakeVisible(c);
    engineAttachment=std::make_unique<APVTS::ComboBoxAttachment>(state,slotId(physical,"Engine"),engine);bypassAttachment=std::make_unique<APVTS::ButtonAttachment>(state,slotId(physical,"Bypass"),bypass);mixAttachment=std::make_unique<APVTS::SliderAttachment>(state,slotId(physical,"Mix"),mix);
    // ComboBoxAttachment synchronises the control during construction.  Install
    // our callback afterwards so that synchronisation cannot re-enter the owner
    // while its six-card rack is only partially constructed.
    engine.onChange=[this]{owner.handleSlotEngineChange(physical);};
}
void LostAudioSuiteEditor::SlotCard::setPositionNumber(int position){number.setText(juce::String(position+1),juce::dontSendNotification);}
void LostAudioSuiteEditor::SlotCard::paint(juce::Graphics& g){auto r=getLocalBounds().toFloat().reduced(1);g.setColour(juce::Colour(selected?0xff213438:panel2));g.fillRoundedRectangle(r,6);g.setColour(juce::Colour(selected?cyan:0xff474f4d));g.drawRoundedRectangle(r,6,selected?1.7f:1.0f);g.setColour(juce::Colour(selected?cyan:0xff69716e));g.fillRect(0.0f,8.0f,3.0f,r.getHeight()-16.0f);}
void LostAudioSuiteEditor::SlotCard::resized(){auto a=getLocalBounds().reduced(6,5);number.setBounds(a.removeFromLeft(27));auto arrows=a.removeFromRight(26);up.setBounds(arrows.removeFromTop(arrows.getHeight()/2).reduced(1));down.setBounds(arrows.reduced(1));bypass.setBounds(a.removeFromRight(42).reduced(2));edit.setBounds(a.removeFromRight(48).reduced(2));auto bottom=a.removeFromBottom(24);mix.setBounds(bottom);engine.setBounds(a.reduced(2,1));}
void LostAudioSuiteEditor::SlotCard::mouseDown(const juce::MouseEvent&){dragStarted=false;owner.selectSlot(physical);}
void LostAudioSuiteEditor::SlotCard::mouseDrag(const juce::MouseEvent& e){if(!dragStarted&&e.getDistanceFromDragStart()>6)if(auto* container=juce::DragAndDropContainer::findParentDragContainerFor(this)){dragStarted=true;container->startDragging(juce::var(physical),this);}}

LostAudioSuiteEditor::LostAudioSuiteEditor(LostAudioSuiteProcessor& p):AudioProcessorEditor(&p),processor(p),apvts(p.state())
{
    setLookAndFeel(&look);brand.setText("B&E DIGITAL",juce::dontSendNotification);brand.setColour(juce::Label::textColourId,juce::Colour(cyan));brand.setFont(juce::Font(juce::FontOptions(11,juce::Font::bold)));
    title.setText("LOST AUDIO SUITE",juce::dontSendNotification);title.setColour(juce::Label::textColourId,juce::Colour(cream));title.setFont(juce::Font(juce::FontOptions(28,juce::Font::bold)));
    subtitle.setText("BUILD A CHAIN OF DEVICES, DAMAGE, AND SPACES",juce::dontSendNotification);subtitle.setColour(juce::Label::textColourId,juce::Colour(0xff9da5a2));
    chainLabel.setText("DEVICE CHAIN / DRAG OR USE ARROWS",juce::dontSendNotification);inspectorTitle.setColour(juce::Label::textColourId,juce::Colour(cream));inspectorTitle.setFont(juce::Font(juce::FontOptions(20,juce::Font::bold)));
    inspectorHint.setColour(juce::Label::textColourId,juce::Colour(0xff969e9b));masterLabel.setText("CHAIN PRESETS / FINAL OUTPUT",juce::dontSendNotification);cpuLabel.setJustificationType(juce::Justification::centredRight);cpuLabel.setColour(juce::Label::textColourId,juce::Colour(0xffaeb5b2));
    const std::array<juce::Component*,11> visibleComponents{&brand,&title,&subtitle,&chainLabel,&inspectorTitle,&inspectorHint,&masterLabel,&cpuLabel,&chainPreset,&slotProfile,&feedbackArm};
    for(auto* c:visibleComponents)addAndMakeVisible(c);
    chainPreset.addItem("Custom Chain",1);for(int i=0;i<(int)std::size(chainPresets);++i)chainPreset.addItem(chainPresets[i].name,i+2);const auto restoredPreset=apvts.state.getProperty("factoryPresetName","Custom Chain").toString();auto restoredPresetId=1;for(int i=0;i<(int)std::size(chainPresets);++i)if(restoredPreset==chainPresets[i].name)restoredPresetId=i+2;chainPreset.setSelectedId(restoredPresetId,juce::dontSendNotification);chainPreset.onChange=[this]{if(chainPreset.getSelectedId()>1)applyChainPreset(chainPreset.getSelectedId()-2);};
    slotProfile.addItemList({"Custom Device","Subtle","Worn","Broken","Extreme"},1);slotProfile.setSelectedId(1,juce::dontSendNotification);slotProfile.onChange=[this]{if(slotProfile.getSelectedId()>1)applySlotProfile(slotProfile.getSelectedId()-2);};
    for(int i=0;i<6;++i){slotCards[(std::size_t)i]=std::make_unique<SlotCard>(*this,apvts,i);addAndMakeVisible(*slotCards[(std::size_t)i]);}
    const std::array<const char*,7> ids{"global1","global2","inputGain","outputGain","mix","limiter","ceiling"};const std::array<const char*,7> labels{"MASTER CHARACTER","MASTER DAMAGE","INPUT","OUTPUT","SUITE MIX","PROTECTION","CEILING"};
    for(std::size_t i=0;i<ids.size();++i){masterKnobs[i]=std::make_unique<Knob>(apvts,ids[i],labels[i],i==2||i==3?" dB":"");addAndMakeVisible(*masterKnobs[i]);}
    selectSlot(0);setResizable(true,true);setResizeLimits(980,620,1800,1200);setSize(1220,760);startTimerHz(24);
}

LostAudioSuiteEditor::~LostAudioSuiteEditor(){setLookAndFeel(nullptr);}

std::array<int,6> LostAudioSuiteEditor::readOrder()const
{
    std::array<int,6> order{};std::array<bool,6> used{};int position=0;
    for(int i=0;i<6;++i){const auto value=juce::jlimit(0,5,juce::roundToInt(apvts.getRawParameterValue("order"+juce::String(i+1))->load()));if(!used[(std::size_t)value]){order[(std::size_t)position++]=value;used[(std::size_t)value]=true;}}
    for(int value=0;value<6;++value)if(!used[(std::size_t)value])order[(std::size_t)position++]=value;return order;
}
int LostAudioSuiteEditor::engineForSlot(int physicalSlot)const{return juce::jlimit(0,10,juce::roundToInt(apvts.getRawParameterValue(slotId(physicalSlot,"Engine"))->load()));}
void LostAudioSuiteEditor::setParameter(const juce::String& id,float value){if(auto* p=apvts.getParameter(id))p->setValueNotifyingHost(p->convertTo0to1(value));}
void LostAudioSuiteEditor::setOrder(const std::array<int,6>& order){for(int i=0;i<6;++i)setParameter("order"+juce::String(i+1),(float)order[(std::size_t)i]);resized();}

void LostAudioSuiteEditor::selectSlot(int physicalSlot)
{
    selectedSlot=juce::jlimit(0,5,physicalSlot);
    for(int i=0;i<6;++i)
        if(auto* card=slotCards[(std::size_t)i].get())
            card->setSelected(i==selectedSlot);

    // This guard also makes future control synchronisation during construction
    // harmless if another attachment begins sending notifications immediately.
    if(std::all_of(slotCards.begin(),slotCards.end(),[](const auto& card){return card!=nullptr;}))
        rebuildInspector();
}
void LostAudioSuiteEditor::handleSlotEngineChange(int physicalSlot)
{
    if(applyingChainPreset)return;
    disarmSlot(physicalSlot);
    selectSlot(physicalSlot);
}
void LostAudioSuiteEditor::disarmSlot(int physicalSlot){setParameter(slotId(physicalSlot,"FeedbackArm"),0.0f);chainPreset.setSelectedId(1,juce::dontSendNotification);apvts.state.setProperty("factoryPresetName","Custom Chain",nullptr);}
void LostAudioSuiteEditor::moveSlot(int physicalSlot,int direction){auto order=readOrder();auto it=std::find(order.begin(),order.end(),physicalSlot);if(it==order.end())return;const auto pos=(int)std::distance(order.begin(),it);const auto target=juce::jlimit(0,5,pos+direction);if(target!=pos){std::swap(order[(std::size_t)pos],order[(std::size_t)target]);setOrder(order);chainPreset.setSelectedId(1,juce::dontSendNotification);apvts.state.setProperty("factoryPresetName","Custom Chain",nullptr);}}

void LostAudioSuiteEditor::itemDropped(const SourceDetails& details)
{
    const auto dragged=juce::jlimit(0,5,(int)details.description);auto order=readOrder();std::vector<int> values(order.begin(),order.end());values.erase(std::remove(values.begin(),values.end(),dragged),values.end());
    const auto relativeY=details.localPosition.getY()-chainBounds.getY();const auto target=juce::jlimit(0,5,(relativeY*6)/juce::jmax(1,chainBounds.getHeight()));values.insert(values.begin()+target,dragged);for(int i=0;i<6;++i)order[(std::size_t)i]=values[(std::size_t)i];setOrder(order);chainPreset.setSelectedId(1,juce::dontSendNotification);apvts.state.setProperty("factoryPresetName","Custom Chain",nullptr);
}

void LostAudioSuiteEditor::rebuildInspector()
{
    feedbackAttachment.reset();inspectorKnobs.clear();const std::array<const char*,9> suffixes{"Model","MacroA","MacroB","Detail1","Detail2","Detail3","Detail4","Detail5","Detail6"};const std::array<const char*,9> labels{"MODEL","CHARACTER","DAMAGE","CONTROL 1","CONTROL 2","CONTROL 3","CONTROL 4","CONTROL 5","CONTROL 6"};
    for(std::size_t i=0;i<suffixes.size();++i){auto knob=std::make_unique<Knob>(apvts,slotId(selectedSlot,suffixes[i]),labels[i]);addAndMakeVisible(*knob);inspectorKnobs.push_back(std::move(knob));}
    feedbackAttachment=std::make_unique<APVTS::ButtonAttachment>(apvts,slotId(selectedSlot,"FeedbackArm"),feedbackArm);lastInspectorEngine=-1;updateInspectorLabels();resized();
}

void LostAudioSuiteEditor::updateInspectorLabels()
{
    const auto engine=engineForSlot(selectedSlot);if(engine==lastInspectorEngine)return;lastInspectorEngine=engine;const auto names=macroNames(engine);const auto details=detailNames(engine);inspectorKnobs[1]->setTitle(names.first);inspectorKnobs[2]->setTitle(names.second);for(int i=0;i<6;++i)inspectorKnobs[(std::size_t)i+3]->setTitle(details[(std::size_t)i]);
    inspectorTitle.setText("DEVICE "+juce::String(selectedSlot+1)+" / "+engines[engine],juce::dontSendNotification);inspectorHint.setText(engine==0?"Choose a device in the chain.":"Shape this device here. Every visible control changes its sound.",juce::dontSendNotification);
    feedbackArm.setVisible(engine==10);repaint();
}

void LostAudioSuiteEditor::applySlotProfile(int index)
{
    constexpr std::array<std::pair<float,float>,4> values{{{.22f,.08f},{.42f,.24f},{.64f,.52f},{.84f,.78f}}};if(index<0||index>=4)return;setParameter(slotId(selectedSlot,"MacroA"),values[(std::size_t)index].first);setParameter(slotId(selectedSlot,"MacroB"),values[(std::size_t)index].second);for(int detail=1;detail<=6;++detail){const auto suffix="Detail"+juce::String(detail);setParameter(slotId(selectedSlot,suffix.toRawUTF8()),.5f);}chainPreset.setSelectedId(1,juce::dontSendNotification);apvts.state.setProperty("factoryPresetName","Custom Chain",nullptr);
}

void LostAudioSuiteEditor::applyChainPreset(int index)
{
    if(index<0||index>=(int)std::size(chainPresets))return;const auto& preset=chainPresets[index];
    {
        const juce::ScopedValueSetter<bool> applyingPresetGuard(applyingChainPreset,true);
        for(int i=0;i<6;++i){setParameter(slotId(i,"FeedbackArm"),0);setParameter(slotId(i,"Engine"),(float)preset.slots[(std::size_t)i].engine);setParameter(slotId(i,"Bypass"),0);setParameter(slotId(i,"MacroA"),preset.slots[(std::size_t)i].a);setParameter(slotId(i,"MacroB"),preset.slots[(std::size_t)i].b);setParameter(slotId(i,"Model"),preset.slots[(std::size_t)i].model);setParameter(slotId(i,"Mix"),preset.slots[(std::size_t)i].mix);for(int detail=1;detail<=6;++detail){const auto suffix="Detail"+juce::String(detail);setParameter(slotId(i,suffix.toRawUTF8()),.5f);}}
        setOrder({0,1,2,3,4,5});setParameter("global1",preset.global1);setParameter("global2",preset.global2);
        setParameter("inputGain",0.0f);setParameter("outputGain",-1.0f);setParameter("mix",1.0f);setParameter("limiter",.86f);setParameter("ceiling",.88f);
    }
    apvts.state.setProperty("factoryPresetName",preset.name,nullptr);chainPreset.setSelectedId(index+2,juce::dontSendNotification);slotProfile.setSelectedId(1,juce::dontSendNotification);rebuildInspector();
}

void LostAudioSuiteEditor::timerCallback(){updateInspectorLabels();cpuLabel.setText("CPU "+juce::String(processor.cpuLoad()*100.0f,1)+"%  /  "+(processor.safetyEngaged()?"SAFETY ACTIVE":"SAFETY READY"),juce::dontSendNotification);cpuLabel.setColour(juce::Label::textColourId,processor.safetyEngaged()?juce::Colour(magenta):juce::Colour(0xffaeb5b2));repaint();}

void LostAudioSuiteEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ink));g.setColour(juce::Colour(cyan));g.fillRect(0,0,(int)(getWidth()*.28f),2);g.setColour(juce::Colour(magenta));g.fillRect((int)(getWidth()*.78f),0,getWidth(),2);
    for(const auto bounds:{chainBounds,inspectorBounds,masterBounds}){g.setColour(juce::Colour(panel));g.fillRoundedRectangle(bounds.toFloat(),8);g.setColour(juce::Colour(0xff454d4a));g.drawRoundedRectangle(bounds.toFloat(),8,1);}
    g.setColour(juce::Colour(0xff38413f));const auto order=readOrder();for(int i=0;i<5;++i){const auto& a=*slotCards[(std::size_t)order[(std::size_t)i]];const auto& b=*slotCards[(std::size_t)order[(std::size_t)i+1]];g.drawLine((float)a.getBounds().getCentreX(),(float)a.getBottom(),(float)b.getBounds().getCentreX(),(float)b.getY(),2);}
    auto meters=masterBounds.reduced(14).removeFromBottom(76);const auto in=juce::jmax(processor.inputPeak(0),processor.inputPeak(1));const auto out=juce::jmax(processor.outputPeak(0),processor.outputPeak(1));
    const std::array<std::pair<const char*,float>,3> values{{{"IN",in},{"OUT",out},{"CHAIN",processor.topologyGain()}}};for(int i=0;i<3;++i){auto row=meters.removeFromTop(22);g.setColour(juce::Colour(0xffaeb5b2));g.setFont(juce::Font(juce::FontOptions(10,juce::Font::bold)));g.drawText(values[(std::size_t)i].first,row.removeFromLeft(43),juce::Justification::centredLeft);auto bar=row.reduced(2,7).toFloat();g.setColour(juce::Colour(0xff2b3130));g.fillRoundedRectangle(bar,3);g.setColour(juce::Colour(i==1&&processor.safetyEngaged()?magenta:(i==2?amber:cyan)));g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*juce::jlimit(0.0f,1.0f,values[(std::size_t)i].second)),3);}
}

void LostAudioSuiteEditor::resized()
{
    auto area=getLocalBounds();auto header=area.removeFromTop(94).reduced(18,8);auto left=header.removeFromLeft(390);brand.setBounds(left.removeFromTop(18));title.setBounds(left.removeFromTop(38));subtitle.setBounds(left.removeFromTop(22));cpuLabel.setBounds(header.removeFromRight(260).removeFromBottom(28));
    area=area.reduced(12,8);chainBounds=area.removeFromLeft(292);area.removeFromLeft(9);masterBounds=area.removeFromRight(286);area.removeFromRight(9);inspectorBounds=area;
    auto chain=chainBounds.reduced(10);chainLabel.setBounds(chain.removeFromTop(25));chain.removeFromTop(3);const auto order=readOrder();const auto cardHeight=chain.getHeight()/6;
    for(int position=0;position<6;++position){const auto physical=order[(std::size_t)position];slotCards[(std::size_t)physical]->setPositionNumber(position);slotCards[(std::size_t)physical]->setBounds(chain.removeFromTop(cardHeight).reduced(0,3));}
    auto inspect=inspectorBounds.reduced(14);auto inspectHeader=inspect.removeFromTop(62);inspectorTitle.setBounds(inspectHeader.removeFromTop(28));inspectorHint.setBounds(inspectHeader.removeFromTop(22));auto profileRow=inspect.removeFromTop(30);slotProfile.setBounds(profileRow.removeFromRight(190));feedbackArm.setBounds(profileRow.removeFromLeft(220));inspect.removeFromTop(8);
    const auto rowHeight=inspect.getHeight()/3;for(int row=0;row<3;++row){auto line=inspect.removeFromTop(row==2?inspect.getHeight():rowHeight);const auto cell=line.getWidth()/3;for(int col=0;col<3;++col)inspectorKnobs[(std::size_t)(row*3+col)]->setBounds(line.removeFromLeft(cell));}
    auto master=masterBounds.reduced(12);masterLabel.setBounds(master.removeFromTop(23));chainPreset.setBounds(master.removeFromTop(31));master.removeFromTop(7);master.removeFromBottom(84);const auto rows=4;const auto cellH=master.getHeight()/rows;for(int i=0;i<7;++i){const auto row=i/2,col=i%2;const auto cellW=master.getWidth()/2;masterKnobs[(std::size_t)i]->setBounds(master.getX()+col*cellW,master.getY()+row*cellH,cellW,cellH);}
}
