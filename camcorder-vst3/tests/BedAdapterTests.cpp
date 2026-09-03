#include "../src/PluginEditor.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace { void set(juce::AudioProcessorValueTreeState& s, const char* id, float v) { auto* p=s.getParameter(id); if(!p) std::exit(2); p->setValueNotifyingHost(p->convertTo0to1(v)); } }

namespace
{
juce::ComboBox* findPresetBox(juce::Component& component)
{
    if (auto* box = dynamic_cast<juce::ComboBox*>(&component); box != nullptr && box->getNumItems() > 10) return box;
    for (auto* child : component.getChildren()) if (auto* result = findPresetBox(*child)) return result;
    return nullptr;
}
juce::TextButton* findButton(juce::Component& component, const juce::String& text)
{
    if (auto* button = dynamic_cast<juce::TextButton*>(&component); button != nullptr && button->getButtonText() == text) return button;
    for (auto* child : component.getChildren()) if (auto* result = findButton(*child, text)) return result;
    return nullptr;
}
juce::Slider* findVisibleSlider(juce::Component& component, const juce::String& label)
{
    if (component.isVisible())
        if (auto* text = dynamic_cast<juce::Label*>(&component); text != nullptr && text->getText() == label)
            if (auto* owner = text->getParentComponent())
                for (auto* child : owner->getChildren()) if (auto* slider = dynamic_cast<juce::Slider*>(child)) return slider;
    for (auto* child : component.getChildren()) if (auto* result = findVisibleSlider(*child, label)) return result;
    return nullptr;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init; CamcorderEngineAudioProcessor p; auto& s=p.getAPVTS();
    if(p.legacyMacrosActive() || !s.getParameter("dropTempoSync") || !s.getParameter("faultTempoSync") || !s.getParameter("handlingTempoSync")){std::cerr<<"Camcorder V3 contract missing\n";return 1;}
    const auto initial=lost_audio::core::mapCamcorderMacros(lost_audio::core::CamcorderFormat::miniDV,lost_audio::core::CameraMic::electret,.35f,.25f,.18f,.35f);
    if(std::abs(s.getRawParameterValue("hpHz")->load()-initial.highPassHz)>1.1f || std::abs(s.getRawParameterValue("lpHz")->load()-initial.lowPassHz)>1.1f || std::abs(s.getRawParameterValue("agcAmt")->load()-initial.agcAmount)>.011f || std::abs(s.getRawParameterValue("drop")->load()-initial.dropout)>.011f){std::cerr<<"Camcorder canonical defaults drifted from legacy sound\n";return 1;}
    set(s,"format",0); set(s,"micModel",1); set(s,"coverage",.64f); set(s,"movement",.52f); set(s,"corruption",.48f); set(s,"agc",.55f); set(s,"macroLink",1);
    const auto target=lost_audio::core::mapCamcorderMacros(lost_audio::core::CamcorderFormat::vhsc,lost_audio::core::CameraMic::cheapMono,.64f,.52f,.48f,.55f); p.materialiseLegacyMacros();
    if(p.legacyMacrosActive() || std::abs(s.getRawParameterValue("hpHz")->load()-target.highPassHz)>1.1f || std::abs(s.getRawParameterValue("drop")->load()-target.dropout)>.011f){std::cerr<<"Camcorder legacy migration failed\n";return 1;}
    {
        CamcorderEngineAudioProcessor uiProcessor; std::unique_ptr<juce::AudioProcessorEditor> editor(uiProcessor.createEditor());
        auto* preset = findPresetBox(*editor); if (!preset) { std::cerr<<"Camcorder real preset selector missing\n";return 1; }
        preset->setSelectedId(5, juce::sendNotificationSync); auto& ui=uiProcessor.getAPVTS();
        const auto vhs=lost_audio::core::mapCamcorderMacros(lost_audio::core::CamcorderFormat::vhsc,lost_audio::core::CameraMic::cheapMono,.48f,.18f,.26f,.48f);
        if(ui.state.getProperty("factoryPresetName").toString()!="VHS-C Birthday"||uiProcessor.legacyMacrosActive()||std::abs(ui.getRawParameterValue("lpHz")->load()-vhs.lowPassHz)>1.1f||ui.getRawParameterValue("camBedEnable")->load()<.5f){std::cerr<<"Camcorder dropdown did not commit the complete preset immediately: name="<<ui.state.getProperty("factoryPresetName").toString()<<" linked="<<uiProcessor.legacyMacrosActive()<<" format="<<ui.getRawParameterValue("format")->load()<<" mic="<<ui.getRawParameterValue("micModel")->load()<<" coverage="<<ui.getRawParameterValue("coverage")->load()<<" movement="<<ui.getRawParameterValue("movement")->load()<<" corruption="<<ui.getRawParameterValue("corruption")->load()<<" agc="<<ui.getRawParameterValue("agc")->load()<<" lp="<<ui.getRawParameterValue("lpHz")->load()<<" expected="<<vhs.lowPassHz<<" bed="<<ui.getRawParameterValue("camBedEnable")->load()<<"\n";return 1;}
        auto* coverage=findVisibleSlider(*editor,"COVERAGE");if(!coverage||!coverage->onDragStart){std::cerr<<"Camcorder surface coverage control missing\n";return 1;}coverage->onDragStart();const auto linkedAfterStart=uiProcessor.legacyMacrosActive();coverage->setValue(.90,juce::sendNotificationSync);if(!uiProcessor.legacyMacrosActive()){std::cerr<<"Camcorder surface macro did not re-arm its physical mapping: afterStart="<<linkedAfterStart<<" afterValue="<<uiProcessor.legacyMacrosActive()<<"\n";return 1;}
        auto* advanced=findButton(*editor,"ADVANCED");if(!advanced||!advanced->onClick){std::cerr<<"Camcorder Advanced view missing\n";return 1;}advanced->onClick();if(!uiProcessor.legacyMacrosActive()){std::cerr<<"Changing Camcorder view altered DSP state\n";return 1;}
        auto* highPass=findVisibleSlider(*editor,"HIGH-PASS");if(!highPass||!highPass->onDragStart){std::cerr<<"Camcorder direct high-pass control missing\n";return 1;}highPass->onDragStart();highPass->setValue(310.0,juce::sendNotificationSync);if(uiProcessor.legacyMacrosActive()||std::abs(ui.getRawParameterValue("hpHz")->load()-310.0f)>1.1f){std::cerr<<"Camcorder direct edit did not commit the current surface sound first\n";return 1;}
    }
    p.setRateAndBufferSizeDetails(48000.0,256); p.prepareToPlay(48000.0,256);
    set(s,"macroLink",0); set(s,"movement",0); set(s,"corruption",0); set(s,"agc",0); set(s,"agcAmt",0); set(s,"agcPump",0); set(s,"clip",0); set(s,"crush",0); set(s,"wind",0); set(s,"handling",0); set(s,"rub",0); set(s,"hiss",0); set(s,"motorBleed",0); set(s,"drop",0); set(s,"chirp",0); set(s,"camBedLevel",1); set(s,"camBedEnable",0);
    juce::MidiBuffer midi; juce::AudioBuffer<float> off(2,48000); off.clear(); p.processBlock(off,midi); const auto offMag=off.getMagnitude(0,0,off.getNumSamples());
    set(s,"camBedEnable",1); juce::AudioBuffer<float> on(2,48000); on.clear(); p.processBlock(on,midi); const auto onMag=on.getMagnitude(0,0,on.getNumSamples());
    if(offMag>1.0e-6f||onMag<1.0e-4f){std::cerr<<"Camcorder bed adapter failure: off="<<offMag<<" on="<<onMag<<'\n';return 1;}
    set(s,"camBedEnable",0); set(s,"dropMs",100); set(s,"faultDurationMs",140); p.triggerDropout(); p.triggerCodecFault(); p.triggerHandling(); juce::AudioBuffer<float> events(2,1024); for(int i=0;i<1024;++i){events.setSample(0,i,.2f);events.setSample(1,i,-.15f);} p.processBlock(events,midi);
    if(!p.dropoutActive()||p.dropoutProgress()<=0||!p.corruptionActive()||p.corruptionProgress()<=0||!p.handlingActive()||p.handlingProgress()<=0){std::cerr<<"Camcorder conducted events or telemetry failed\n";return 1;}
    std::cout<<"Camcorder adapter passed: canonical migration, isolated beds, conducted events, and telemetry\n";
}
