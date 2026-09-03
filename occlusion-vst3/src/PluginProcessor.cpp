#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <lost_audio/core/TempoSync.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
juce::NormalisableRange<float> linear(float a,float b,float step=.001f){return {a,b,step};}
const juce::StringArray divisions { "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
}

OcclusionEngineAudioProcessor::OcclusionEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                      .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      apvts(*this,nullptr,"PARAMS",createParameterLayout())
{
    apvts.state.setProperty("engineId","occlusion",nullptr);
    apvts.state.setProperty("schemaVersion",3,nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout OcclusionEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;const auto n=linear(0,1);
    // V1 and V2 order/IDs stay immutable for existing sessions.
    p.push_back(std::make_unique<juce::AudioParameterFloat>("distance","Distance",n,.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("wall","Wall",n,.45f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("material","Material",juce::StringArray{"Drywall","Brick","Wood","Curtain","Door","Glass","Metal","Concrete"},0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("sourceRoom","Source Room",n,.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("listenerRoom","Listener Room",n,.45f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz","High-pass",linear(10,600,1),64));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz","Low-pass",linear(80,20000,1),8285));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dipHz","Absorption Frequency",linear(120,10000,1),1528));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dipDb","Absorption Depth",linear(-18,6,.1f),-3.8f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dipQ","Absorption Width",linear(.2f,8,.01f),1.1f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bumpHz","Body Frequency",linear(60,4000,1),352));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bumpDb","Body Gain",linear(-12,12,.1f),2.8f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bumpQ","Body Width",linear(.2f,5,.01f),.95f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("leak","Edge Leak",n,.111f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("roomMix","Room Sound",n,.413f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("predelayMs","Listener Predelay",linear(0,120,1),12));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("roomSize","Room Size",n,.412f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("damp","Damping",n,.671f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain","Output",linear(0,1.5f,.01f),.9f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("macroLink","Legacy Surface Link",false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("construction","Construction",juce::StringArray{"Solid","Stud","Hollow","Panel","Loose"},1));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("resonance","Resonance",n,.613f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("cavity","Cavity",n,.443f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("rattle","Hardware Rattle",n,.060f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("looseness","Looseness",n,.34f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("smear","Multipath Smear",n,.396f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("leakTone","Leak Tone",n,.564f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain","Input Gain",linear(-24,24,.1f),0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix","Mix",n,1));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("limiter","Limiter",n,1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling","Ceiling",linear(.2f,1,.001f),.94f));
    // V3 performance and motion controls are append-only.
    p.push_back(std::make_unique<juce::AudioParameterFloat>("stereoMotion","Stereo Motion",n,0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("motionRateHz","Motion Rate",linear(.01f,8,.01f),.18f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("motionSync","Clock Motion",false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("motionDivision","Motion Cycle",divisions,0));
    p.push_back(std::make_unique<juce::AudioParameterBool>("exciteSync","Clock Boundary Excitation",false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("exciteDivision","Excitation Grid",divisions,2));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("exciteProbability","Excitation Probability",n,.28f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("exciteStrength","Excitation Strength",n,.72f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("exciteDurationMs","Excitation Length",linear(30,6000,1),900));
    p.push_back(std::make_unique<juce::AudioParameterBool>("exciteLengthSync","Clock Excitation Length",false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("exciteLengthDivision","Excitation Length Grid",divisions,0));
    p.push_back(std::make_unique<juce::AudioParameterBool>("strikeSync","Clock Hardware Strikes",false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("strikeDivision","Hardware Grid",divisions,3));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("strikeProbability","Hardware Probability",n,.34f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("strikeStrength","Hardware Strength",n,.7f));
    return {p.begin(),p.end()};
}

float OcclusionEngineAudioProcessor::value(const char* id)const noexcept{return apvts.getRawParameterValue(id)->load();}
bool OcclusionEngineAudioProcessor::legacyMacrosActive()const noexcept{return value("macroLink")>.5f;}

void OcclusionEngineAudioProcessor::materialiseLegacyMacros()
{
    if(!legacyMacrosActive())return;
    const auto material=(lost_audio::core::OcclusionMaterial)juce::jlimit(0,7,(int)std::lround(value("material")));
    const auto construction=(lost_audio::core::OcclusionConstruction)juce::jlimit(0,4,(int)std::lround(value("construction")));
    const auto t=lost_audio::core::mapOcclusionMacros(material,construction,value("distance"),value("wall"),value("sourceRoom"),value("listenerRoom"));
    const auto set=[this](const char*id,float plain){if(auto*parameter=apvts.getParameter(id))parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));};
    set("hpHz",t.hpHz);set("lpHz",t.lpHz);set("dipHz",t.dipHz);set("dipDb",t.dipDb);set("bumpHz",t.bumpHz);set("bumpDb",t.bumpDb);
    set("resonance",t.resonance);set("cavity",t.cavity);set("rattle",t.rattle);set("looseness",t.looseness);set("smear",t.smear);
    set("leak",t.leak);set("leakTone",t.leakTone);set("roomMix",t.roomMix);set("predelayMs",t.predelayMs);set("roomSize",t.roomSize);set("damp",t.damp);set("outGain",t.outputGain);set("macroLink",0);
}

lost_audio::core::OcclusionParameters OcclusionEngineAudioProcessor::readParameters()const noexcept
{
    lost_audio::core::OcclusionParameters p;
    p.material=(lost_audio::core::OcclusionMaterial)juce::jlimit(0,7,(int)std::lround(value("material")));p.construction=(lost_audio::core::OcclusionConstruction)juce::jlimit(0,4,(int)std::lround(value("construction")));
    p.distance=value("distance");p.wall=value("wall");p.sourceRoom=value("sourceRoom");p.listenerRoom=value("listenerRoom");p.dipQ=value("dipQ");p.bumpQ=value("bumpQ");p.inputGain=juce::Decibels::decibelsToGain(value("inputGain"));p.mix=value("mix");
    if(legacyMacrosActive()){const auto t=lost_audio::core::mapOcclusionMacros(p.material,p.construction,value("distance"),value("wall"),p.sourceRoom,p.listenerRoom);p.hpHz=t.hpHz;p.lpHz=t.lpHz;p.dipHz=t.dipHz;p.dipDb=t.dipDb;p.bumpHz=t.bumpHz;p.bumpDb=t.bumpDb;p.resonance=t.resonance;p.cavity=t.cavity;p.rattle=t.rattle;p.looseness=t.looseness;p.smear=t.smear;p.leak=t.leak;p.leakTone=t.leakTone;p.roomMix=juce::jlimit(0.0f,1.0f,t.roomMix*(value("roomMix")/.22f));p.predelayMs=t.predelayMs;p.roomSize=t.roomSize;p.damp=t.damp;p.outputGain=t.outputGain;}
    else{p.hpHz=value("hpHz");p.lpHz=value("lpHz");p.dipHz=value("dipHz");p.dipDb=value("dipDb");p.bumpHz=value("bumpHz");p.bumpDb=value("bumpDb");p.resonance=value("resonance");p.cavity=value("cavity");p.rattle=value("rattle");p.looseness=value("looseness");p.smear=value("smear");p.leak=value("leak");p.leakTone=value("leakTone");p.roomMix=value("roomMix");p.predelayMs=value("predelayMs");p.roomSize=value("roomSize");p.damp=value("damp");p.outputGain=value("outGain");}
    p.limiter=value("limiter");p.ceiling=value("ceiling");p.stereoMotion=value("stereoMotion");p.motionPhase=motionPhase;return p;
}

void OcclusionEngineAudioProcessor::prepareToPlay(double sr,int)
{
    core.prepare(sr,(std::size_t)juce::jlimit(1,2,getTotalNumOutputChannels()));core.reset();motionPhase=0;currentBpm=120;pendingBoundary.store(false);pendingHardware.store(false);for(auto&x:trace)x.store(0);setLatencySamples(0);
}

bool OcclusionEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& l)const
{
    const auto in=l.getMainInputChannelSet();return in==l.getMainOutputChannelSet()&&(in==juce::AudioChannelSet::mono()||in==juce::AudioChannelSet::stereo());
}

void OcclusionEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;for(int c=getTotalNumInputChannels();c<getTotalNumOutputChannels();++c)b.clear(c,0,b.getNumSamples());const auto channels=juce::jmin(2,b.getNumChannels()),samples=b.getNumSamples();if(channels<=0||samples<=0||getSampleRate()<=0)return;
    bool playing=false,hasPpq=false;double ppq=0;if(auto*head=getPlayHead())if(const auto pos=head->getPosition()){playing=pos->getIsPlaying();if(const auto bpm=pos->getBpm())currentBpm=*bpm;if(const auto hostPpq=pos->getPpqPosition()){ppq=*hostPpq;hasPpq=true;}}
    lost_audio::core::TempoEventSchedule exciteSchedule,strikeSchedule;if(playing&&hasPpq&&value("exciteSync")>.5f)exciteSchedule=lost_audio::core::tempoEventsInBlock(ppq,currentBpm,(int)value("exciteDivision"),getSampleRate(),samples);if(playing&&hasPpq&&value("strikeSync")>.5f)strikeSchedule=lost_audio::core::tempoEventsInBlock(ppq,currentBpm,(int)value("strikeDivision"),getSampleRate(),samples);
    std::vector<int> boundaries{0,samples};for(std::size_t i=0;i<exciteSchedule.size;++i)boundaries.push_back(exciteSchedule.events[i].sampleOffset);for(std::size_t i=0;i<strikeSchedule.size;++i)boundaries.push_back(strikeSchedule.events[i].sampleOffset);std::sort(boundaries.begin(),boundaries.end());boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());
    auto fireExcite=pendingBoundary.exchange(false),fireStrike=pendingHardware.exchange(false);const auto durationMs=value("exciteLengthSync")>.5f?lost_audio::core::tempoDivisionMilliseconds(currentBpm,(int)value("exciteLengthDivision")):value("exciteDurationMs");
    for(std::size_t segment=0;segment+1<boundaries.size();++segment){const auto offset=boundaries[segment],count=boundaries[segment+1]-offset;for(std::size_t i=0;i<exciteSchedule.size;++i)if(exciteSchedule.events[i].sampleOffset==offset&&lost_audio::core::tempoEventDecision(exciteSchedule.events[i].stepIndex,value("exciteProbability"),0x4f43434cu))fireExcite=true;for(std::size_t i=0;i<strikeSchedule.size;++i)if(strikeSchedule.events[i].sampleOffset==offset&&lost_audio::core::tempoEventDecision(strikeSchedule.events[i].stepIndex,value("strikeProbability"),0x52415454u))fireStrike=true;
        if(fireExcite&&!core.excitationActive()){core.triggerBoundaryExcitation(value("exciteStrength"),std::max(1,(int)std::lround(durationMs*.001*getSampleRate())));fireExcite=false;}if(fireStrike){core.triggerRattleStrike(value("strikeStrength"));fireStrike=false;}
        const auto cycleMs=value("motionSync")>.5f?lost_audio::core::tempoDivisionMilliseconds(currentBpm,(int)value("motionDivision")):1000.0/value("motionRateHz");motionPhase=std::fmod(motionPhase+(float)(count/(getSampleRate()*cycleMs*.001)),1.0f);std::array<float*,2>d{b.getWritePointer(0,offset),channels>1?b.getWritePointer(1,offset):nullptr};core.process(d.data(),(std::size_t)channels,(std::size_t)count,readParameters());}
    for(int c=0;c<channels;++c){inputPeaks[(std::size_t)c].store(core.inputPeak((std::size_t)c));outputPeaks[(std::size_t)c].store(core.outputPeak((std::size_t)c));}if(channels==1){inputPeaks[1].store(inputPeaks[0].load());outputPeaks[1].store(outputPeaks[0].load());}
    for(std::size_t i=0;i<trace.size();++i){const auto at=juce::jlimit(0,samples-1,(int)std::lround((double)i*(samples-1)/(trace.size()-1)));trace[i].store(b.getSample(0,at));}bodyMeter.store(core.bodyActivity());roomMeter.store(core.roomActivity());leakMeter.store(core.leakActivity());rattleMeter.store(core.rattleActivity());limiterMeter.store(core.limiterActivity());excitationState.store(core.excitationActive());excitationProgressMeter.store(core.excitationProgress());
}

std::array<float,64> OcclusionEngineAudioProcessor::outputTrace()const noexcept{std::array<float,64> out{};for(std::size_t i=0;i<out.size();++i)out[i]=trace[i].load();return out;}
juce::AudioProcessorEditor* OcclusionEngineAudioProcessor::createEditor(){return new OcclusionEngineAudioProcessorEditor(*this);}

void OcclusionEngineAudioProcessor::getStateInformation(juce::MemoryBlock& d)
{
    auto state=apvts.copyState();state.setProperty("engineId","occlusion",nullptr);state.setProperty("schemaVersion",3,nullptr);if(auto xml=state.createXml())copyXmlToBinary(*xml,d);
}

void OcclusionEngineAudioProcessor::setStateInformation(const void* d,int size)
{
    if(auto xml=getXmlFromBinary(d,size))if(xml->hasTagName(apvts.state.getType())){auto restored=juce::ValueTree::fromXml(*xml);const auto schema=(int)restored.getProperty("schemaVersion",1);apvts.replaceState(restored);if(schema<3&&legacyMacrosActive())materialiseLegacyMacros();apvts.state.setProperty("engineId","occlusion",nullptr);apvts.state.setProperty("schemaVersion",3,nullptr);}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new OcclusionEngineAudioProcessor();}
