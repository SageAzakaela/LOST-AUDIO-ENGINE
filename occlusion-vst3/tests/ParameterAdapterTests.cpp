#include "../src/PluginEditor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
[[noreturn]] void fail(const char* message){std::cerr<<"Occlusion adapter failure: "<<message<<'\n';std::exit(1);}
void set(juce::AudioProcessorValueTreeState& state,const char* id,float plain){auto* p=state.getParameter(id);if(!p)fail("missing parameter");p->setValueNotifyingHost(p->convertTo0to1(plain));}
std::vector<float> render(std::initializer_list<std::pair<const char*,float>> changes)
{
    OcclusionEngineAudioProcessor p;p.setPlayConfigDetails(2,2,48000.0,256);p.prepareToPlay(48000.0,256);auto& state=p.getAPVTS();
    set(state,"material",0);set(state,"construction",0);set(state,"rattle",0);set(state,"mix",1);for(const auto& [id,value]:changes)set(state,id,value);
    juce::AudioBuffer<float> audio(2,48000);for(int i=0;i<audio.getNumSamples();++i){const auto x=.15f*std::sin(6.2831853f*220.0f*i/48000.0f)+.08f*std::sin(6.2831853f*6200.0f*i/48000.0f);audio.setSample(0,i,x);audio.setSample(1,i,x*.83f);}juce::MidiBuffer midi;
    for(int offset=0;offset<audio.getNumSamples();offset+=256){const auto n=juce::jmin(256,audio.getNumSamples()-offset);juce::AudioBuffer<float> block(audio.getArrayOfWritePointers(),2,offset,n);p.processBlock(block,midi);}
    return {audio.getReadPointer(0),audio.getReadPointer(0)+audio.getNumSamples()};
}
float difference(const std::vector<float>& a,const std::vector<float>& b){double total=0;for(std::size_t i=0;i<a.size();++i)total+=std::abs(a[i]-b[i]);return (float)(total/a.size());}
juce::ComboBox* findPresetBox(juce::Component& c){if(auto*box=dynamic_cast<juce::ComboBox*>(&c);box&&box->getNumItems()>10)return box;for(auto*child:c.getChildren())if(auto*result=findPresetBox(*child))return result;return nullptr;}
juce::TextButton* findButton(juce::Component&c,const juce::String&text){if(auto*button=dynamic_cast<juce::TextButton*>(&c);button&&button->getButtonText()==text)return button;for(auto*child:c.getChildren())if(auto*result=findButton(*child,text))return result;return nullptr;}
juce::Slider* findVisibleSlider(juce::Component&c,const juce::String&label){if(c.isVisible())if(auto*text=dynamic_cast<juce::Label*>(&c);text&&text->getText()==label)if(auto*owner=text->getParentComponent())for(auto*child:owner->getChildren())if(auto*slider=dynamic_cast<juce::Slider*>(child))return slider;for(auto*child:c.getChildren())if(auto*result=findVisibleSlider(*child,label))return result;return nullptr;}
}

int main()
{
    {
        OcclusionEngineAudioProcessor p;auto& state=p.getAPVTS();
        if(state.getRawParameterValue("macroLink")->load()>.5f)fail("new sessions still start under legacy macro control");
        const auto mapped=lost_audio::core::mapOcclusionMacros(lost_audio::core::OcclusionMaterial::drywall,lost_audio::core::OcclusionConstruction::stud,.35f,.45f,.35f,.45f);
        if(std::abs(state.getRawParameterValue("lpHz")->load()-mapped.lpHz)>1.1f||std::abs(state.getRawParameterValue("resonance")->load()-mapped.resonance)>.002f||std::abs(state.getRawParameterValue("roomMix")->load()-mapped.roomMix)>.002f)fail("direct defaults do not preserve the legacy default sound");
        set(state,"wall",.82f);set(state,"distance",.61f);set(state,"macroLink",1);const auto expected=lost_audio::core::mapOcclusionMacros(lost_audio::core::OcclusionMaterial::drywall,lost_audio::core::OcclusionConstruction::stud,.61f,.82f,.35f,.45f);p.materialiseLegacyMacros();if(state.getRawParameterValue("macroLink")->load()>.5f||std::abs(state.getRawParameterValue("lpHz")->load()-expected.lpHz)>1.1f||std::abs(state.getRawParameterValue("cavity")->load()-expected.cavity)>.002f)fail("legacy materialisation changed the sound or remained linked");
    }
    {
        OcclusionEngineAudioProcessor p;std::unique_ptr<juce::AudioProcessorEditor>editor(p.createEditor());auto*selector=findPresetBox(*editor);if(!selector)fail("real preset selector missing");selector->setSelectedId(16,juce::sendNotificationSync);auto&state=p.getAPVTS();if(state.state.getProperty("factoryPresetName").toString()!="Rattling Sheet Metal"||p.legacyMacrosActive()||std::abs(state.getRawParameterValue("rattle")->load()-.86f)>.002f)fail("dropdown did not commit Rattling Sheet Metal immediately");auto*thickness=findVisibleSlider(*editor,"THICKNESS");if(!thickness||!thickness->onDragStart)fail("surface thickness control missing");thickness->onDragStart();thickness->setValue(.91,juce::sendNotificationSync);if(!p.legacyMacrosActive())fail("surface thickness did not activate physical mapping");auto*advanced=findButton(*editor,"ADVANCED");if(!advanced||!advanced->onClick)fail("Advanced view missing");advanced->onClick();if(!p.legacyMacrosActive())fail("changing view altered the DSP state");auto*lowPass=findVisibleSlider(*editor,"LOW-PASS");if(!lowPass||!lowPass->onDragStart)fail("direct low-pass control missing");lowPass->onDragStart();lowPass->setValue(920.0,juce::sendNotificationSync);if(p.legacyMacrosActive()||std::abs(state.getRawParameterValue("lpHz")->load()-920.0f)>1.1f)fail("direct low-pass did not commit surface mapping before edit");
    }
    const auto open=render({{"macroLink",1},{"wall",0},{"distance",.35f},{"roomMix",.22f}});
    const auto closed=render({{"macroLink",1},{"wall",1},{"distance",.35f},{"roomMix",.22f}});
    const auto source=render({{"macroLink",1},{"sourceRoom",1},{"listenerRoom",0},{"roomMix",.7f}});
    const auto listener=render({{"macroLink",1},{"sourceRoom",0},{"listenerRoom",1},{"roomMix",.7f}});
    const auto roomOff=render({{"macroLink",1},{"sourceRoom",1},{"listenerRoom",1},{"roomMix",0}});
    const auto roomOn=render({{"macroLink",1},{"sourceRoom",1},{"listenerRoom",1},{"roomMix",1}});
    const auto lpOpen=render({{"macroLink",0},{"lpHz",18000}});
    const auto lpClosed=render({{"macroLink",0},{"lpHz",600}});
    const auto thin=render({{"macroLink",0},{"wall",0},{"distance",0}});
    const auto thick=render({{"macroLink",0},{"wall",1},{"distance",1}});
    if(difference(open,closed)<.012f)fail("surface boundary does not close bandwidth audibly");
    if(difference(source,listener)<.002f)fail("source and listener rooms are not distinct");
    if(difference(roomOff,roomOn)<.003f)fail("room sound control is inaudible");
    if(difference(lpOpen,lpClosed)<.012f)fail("advanced low-pass is inaudible");
    const auto physicalDifference=difference(thin,thick);
    if(physicalDifference<.006f){std::cerr<<"physical difference="<<physicalDifference<<'\n';fail("direct thickness and distance controls are inaudible");}
    for(const auto& value:roomOn)if(!std::isfinite(value)||std::abs(value)>1.001f)fail("output escaped bounds");
    {
        OcclusionEngineAudioProcessor p;p.setPlayConfigDetails(2,2,48000,256);p.prepareToPlay(48000,256);auto&state=p.getAPVTS();set(state,"material",6);set(state,"construction",4);set(state,"resonance",.9f);set(state,"cavity",.8f);set(state,"rattle",.8f);juce::AudioBuffer<float> audio(2,4096);audio.clear();juce::MidiBuffer midi;p.triggerBoundary();p.triggerHardware();for(int offset=0;offset<audio.getNumSamples();offset+=256){juce::AudioBuffer<float> block(audio.getArrayOfWritePointers(),2,offset,256);p.processBlock(block,midi);}double energy=0;for(int i=0;i<audio.getNumSamples();++i)energy+=std::abs(audio.getSample(0,i));if(energy<.01||p.bodyActivity()<.0001f||p.rattleActivity()<.0001f)fail("manual conducted events are silent or decorative");
    }
    std::cout<<"Occlusion adapter passed: direct defaults, legacy migration, boundary bandwidth, distinct rooms, low-pass, conducted events, telemetry, bounded output\n";
}
