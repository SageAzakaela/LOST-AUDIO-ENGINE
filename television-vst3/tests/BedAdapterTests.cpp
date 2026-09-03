#include "../src/PluginProcessor.h"
#include <cstdlib>
#include <iostream>

namespace { void set(juce::AudioProcessorValueTreeState& s, const char* id, float v) { auto* p=s.getParameter(id); if(!p) std::exit(2); p->setValueNotifyingHost(p->convertTo0to1(v)); } }

int main()
{
    juce::ScopedJuceInitialiser_GUI init; TelevisionEngineAudioProcessor p; p.prepareToPlay(48000.0,256); auto& s=p.getAPVTS();
    set(s,"macroLink",0); set(s,"static",0); set(s,"hum",0); set(s,"whine",0); set(s,"cabinetRattle",0); set(s,"syncInstability",0); set(s,"bedLevel",1); set(s,"bedEnable",0);
    juce::MidiBuffer midi; juce::AudioBuffer<float> off(2,48000); off.clear(); p.processBlock(off,midi); const auto offMag=off.getMagnitude(0,0,off.getNumSamples());
    set(s,"bedEnable",1); juce::AudioBuffer<float> on(2,48000); on.clear(); p.processBlock(on,midi); const auto onMag=on.getMagnitude(0,0,on.getNumSamples());
    if(offMag>1.0e-6f||onMag<1.0e-4f){std::cerr<<"TV bed adapter failure: off="<<offMag<<" on="<<onMag<<'\n';return 1;}
    std::cout<<"Television bed adapter passed: disabled silent, embedded CRT bed audible\n";
}
