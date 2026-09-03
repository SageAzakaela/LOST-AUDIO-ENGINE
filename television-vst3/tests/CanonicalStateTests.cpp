#include "../src/PluginProcessor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
void set(juce::AudioProcessorValueTreeState& state, const char* id, float value)
{
    auto* parameter = state.getParameter(id); if (!parameter) std::exit(2);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(const juce::AudioProcessorValueTreeState& state, const char* id)
{
    const auto* value = state.getRawParameterValue(id); if (!value) std::exit(2); return value->load();
}
bool near(float a, float b, float tolerance) { return std::abs(a-b) <= tolerance; }
juce::ComboBox* findPresetBox(juce::Component& component)
{
    if (auto* box=dynamic_cast<juce::ComboBox*>(&component); box&&box->getNumItems()>10) return box;
    for (auto* child:component.getChildren()) if (auto* result=findPresetBox(*child)) return result;
    return nullptr;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init; TelevisionEngineAudioProcessor processor; auto& state=processor.getAPVTS();
    const auto defaults=lost_audio::core::mapTelevisionMacros(lost_audio::core::TelevisionModel::console,lost_audio::core::TelevisionReception::antenna,.45f,.55f,.22f,.12f);
    if(processor.legacyMacrosActive()||!near(get(state,"hpHz"),defaults.highPassHz,1.1f)||!near(get(state,"lpHz"),defaults.lowPassHz,1.1f)||!near(get(state,"cabinet"),defaults.cabinet,.0011f)){std::cerr<<"New Television state is not canonical\n";return 1;}
    set(state,"setModel",4);set(state,"receptionMode",3);set(state,"vibe",.74f);set(state,"speaker",.21f);set(state,"agc",.63f);set(state,"static",.48f);set(state,"outGain",1);set(state,"macroLink",1);
    const auto expected=lost_audio::core::mapTelevisionMacros(lost_audio::core::TelevisionModel::motel,lost_audio::core::TelevisionReception::detuned,.74f,.21f,.63f,.48f);processor.materialiseLegacyMacros();
    if(processor.legacyMacrosActive()||!near(get(state,"hpHz"),expected.highPassHz,1.1f)||!near(get(state,"drive"),expected.drive,.0011f)||!near(get(state,"tunerDrift"),expected.tunerDrift,.0011f)||!near(get(state,"cabinetRattle"),expected.cabinetRattle,.0011f)){std::cerr<<"Legacy Television state did not materialise faithfully\n";return 1;}
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());editor->setSize(960,640);const auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());if(!snapshot.isValid()||snapshot.getWidth()!=960||snapshot.getHeight()!=640){std::cerr<<"Television editor did not render at laptop minimum\n";return 1;}auto*preset=findPresetBox(*editor);if(!preset){std::cerr<<"Television real preset selector missing\n";return 1;}preset->setSelectedId(17,juce::sendNotificationSync);if(state.state.getProperty("factoryPresetName").toString()!="Power Brownout"||processor.legacyMacrosActive()||!near(get(state,"hpHz"),70,1.1f)||!near(get(state,"powerSag"),.88f,.002f)){std::cerr<<"Power Brownout did not commit its direct preset state immediately\n";return 1;}
    std::cout<<"Television canonical defaults, legacy macro materialisation, and laptop UI render passed\n";
}
