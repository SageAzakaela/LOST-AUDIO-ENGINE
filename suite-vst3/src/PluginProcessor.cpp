#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <chrono>

namespace
{
juce::NormalisableRange<float> linear(float low, float high, float interval = .001f) { return { low, high, interval }; }
const juce::StringArray engineNames { "Empty", "Tape", "Transmission", "Comms", "CD", "Conference", "Camcorder", "Cartridge", "Television", "Occlusion", "Open Mic Night" };
}

juce::String LostAudioSuiteProcessor::slotId(int slot, const char* suffix)
{
    return "slot" + juce::String(slot + 1) + suffix;
}

LostAudioSuiteProcessor::LostAudioSuiteProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMS", createParameterLayout())
{
    parameters.state.setProperty("engineId", "lost-audio-suite", nullptr);
    parameters.state.setProperty("schemaVersion", 1, nullptr);
    cacheParameterPointers();
}

juce::AudioProcessorValueTreeState::ParameterLayout LostAudioSuiteProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain", linear(-24.0f, 12.0f, .1f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outputGain", "Output Gain", linear(-24.0f, 12.0f, .1f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Suite Mix", linear(0.0f, 1.0f), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("limiter", "Master Safety", linear(0.0f, 1.0f), .72f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Safety Ceiling", linear(.25f, .99f), .92f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("global1", "Global Macro One", linear(0.0f, 1.0f), .5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("global2", "Global Macro Two", linear(0.0f, 1.0f), .5f));
    const juce::StringArray orderNames { "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Slot 6" };
    for (int i = 0; i < 6; ++i)
        p.push_back(std::make_unique<juce::AudioParameterChoice>("order" + juce::String(i + 1), "Chain Position " + juce::String(i + 1), orderNames, i));
    for (int i = 0; i < 6; ++i)
    {
        const auto prefix = "Slot " + juce::String(i + 1) + " ";
        p.push_back(std::make_unique<juce::AudioParameterChoice>(slotId(i, "Engine"), prefix + "Engine", engineNames, 0));
        p.push_back(std::make_unique<juce::AudioParameterBool>(slotId(i, "Bypass"), prefix + "Bypass", false));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(slotId(i, "Mix"), prefix + "Mix", linear(0.0f, 1.0f), 1.0f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(slotId(i, "MacroA"), prefix + "Character", linear(0.0f, 1.0f), .35f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(slotId(i, "MacroB"), prefix + "Damage", linear(0.0f, 1.0f), .20f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(slotId(i, "Model"), prefix + "Model", linear(0.0f, 1.0f), 0.0f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(slotId(i, "G1A"), prefix + "Global 1 to Character", linear(-1.0f, 1.0f), 0.0f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(slotId(i, "G1B"), prefix + "Global 1 to Damage", linear(-1.0f, 1.0f), 0.0f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(slotId(i, "G2A"), prefix + "Global 2 to Character", linear(-1.0f, 1.0f), 0.0f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(slotId(i, "G2B"), prefix + "Global 2 to Damage", linear(-1.0f, 1.0f), 0.0f));
        p.push_back(std::make_unique<juce::AudioParameterBool>(slotId(i, "FeedbackArm"), prefix + "Feedback Arm", false));
    }
    // Append new controls after the complete legacy parameter list so saved
    // host automation indices for every existing slot remain stable.
    for (int i = 0; i < 6; ++i)
        for (int detail = 0; detail < 6; ++detail)
        {
            const auto suffix = "Detail" + juce::String(detail + 1);
            p.push_back(std::make_unique<juce::AudioParameterFloat>(slotId(i, suffix.toRawUTF8()), "Device " + juce::String(i + 1) + " Control " + juce::String(detail + 1), linear(0.0f, 1.0f), .5f));
        }
    return { p.begin(), p.end() };
}

void LostAudioSuiteProcessor::cacheParameterPointers()
{
    inputGainRef=parameters.getRawParameterValue("inputGain");outputGainRef=parameters.getRawParameterValue("outputGain");mixRef=parameters.getRawParameterValue("mix");
    limiterRef=parameters.getRawParameterValue("limiter");ceilingRef=parameters.getRawParameterValue("ceiling");global1Ref=parameters.getRawParameterValue("global1");global2Ref=parameters.getRawParameterValue("global2");
    for(int i=0;i<6;++i)
    {
        orderRefs[static_cast<std::size_t>(i)]=parameters.getRawParameterValue("order"+juce::String(i+1));auto& r=slotRefs[static_cast<std::size_t>(i)];
        r.engine=parameters.getRawParameterValue(slotId(i,"Engine"));r.bypass=parameters.getRawParameterValue(slotId(i,"Bypass"));r.mix=parameters.getRawParameterValue(slotId(i,"Mix"));
        r.macroA=parameters.getRawParameterValue(slotId(i,"MacroA"));r.macroB=parameters.getRawParameterValue(slotId(i,"MacroB"));r.model=parameters.getRawParameterValue(slotId(i,"Model"));
        r.g1a=parameters.getRawParameterValue(slotId(i,"G1A"));r.g1b=parameters.getRawParameterValue(slotId(i,"G1B"));r.g2a=parameters.getRawParameterValue(slotId(i,"G2A"));r.g2b=parameters.getRawParameterValue(slotId(i,"G2B"));
        r.feedbackArm=parameters.getRawParameterValue(slotId(i,"FeedbackArm"));
        for(int detail=0;detail<6;++detail){const auto suffix="Detail"+juce::String(detail+1);r.detail[(std::size_t)detail]=parameters.getRawParameterValue(slotId(i,suffix.toRawUTF8()));}
    }
}

lost_audio::core::SuiteParameters LostAudioSuiteProcessor::readParameters() const noexcept
{
    lost_audio::core::SuiteParameters p;
    p.inputGain=juce::Decibels::decibelsToGain(inputGainRef->load());p.outputGain=juce::Decibels::decibelsToGain(outputGainRef->load());p.mix=mixRef->load();
    p.limiter=limiterRef->load();p.ceiling=ceilingRef->load();p.globalMacro1=global1Ref->load();p.globalMacro2=global2Ref->load();
    for(std::size_t i=0;i<6;++i)
    {
        p.order[i]=juce::jlimit(0,5,juce::roundToInt(orderRefs[i]->load()));const auto& r=slotRefs[i];auto& s=p.slots[i];
        s.engine=static_cast<lost_audio::core::SuiteEngine>(juce::jlimit(0,10,juce::roundToInt(r.engine->load())));s.bypass=r.bypass->load()>.5f;s.mix=r.mix->load();
        s.macroA=r.macroA->load();s.macroB=r.macroB->load();s.model=r.model->load();s.global1ToA=r.g1a->load();s.global1ToB=r.g1b->load();s.global2ToA=r.g2a->load();s.global2ToB=r.g2b->load();
        s.feedbackArmed=r.feedbackArm->load()>.5f;
        for(std::size_t detail=0;detail<s.detail.size();++detail)s.detail[detail]=r.detail[detail]->load();
    }
    return p;
}

void LostAudioSuiteProcessor::prepareToPlay(double sampleRate, int)
{
    core.prepare(sampleRate, static_cast<std::size_t>(juce::jlimit(1,2,getTotalNumInputChannels())));core.reset();setLatencySamples(core.latencySamples());crtBed=decodeCrtBed(sampleRate);crtPosition=0.0f;
    for(auto& meter:inputMeter)meter.store(0);for(auto& meter:outputMeter)meter.store(0);cpuMeter.store(0);topologyMeter.store(1);safetyMeter.store(false);
}

bool LostAudioSuiteProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input=layouts.getMainInputChannelSet();return input==layouts.getMainOutputChannelSet()&&(input==juce::AudioChannelSet::mono()||input==juce::AudioChannelSet::stereo());
}

void LostAudioSuiteProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;const auto channels=juce::jlimit(0,2,getTotalNumInputChannels());for(int c=channels;c<getTotalNumOutputChannels();++c)buffer.clear(c,0,buffer.getNumSamples());if(channels==0)return;
    const auto start=std::chrono::steady_clock::now();const auto current=readParameters();for(int offset=0;offset<buffer.getNumSamples();offset+=(int)crtChunk.size()){const auto count=juce::jmin((int)crtChunk.size(),buffer.getNumSamples()-offset);for(int i=0;i<count;++i)crtChunk[(std::size_t)i]=readCrtBed()*.09f;float* data[]{buffer.getWritePointer(0,offset),channels>1?buffer.getWritePointer(1,offset):nullptr};core.process(data,static_cast<std::size_t>(channels),static_cast<std::size_t>(count),current,crtBed.empty()?nullptr:crtChunk.data());}
    const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();const auto audioSeconds=buffer.getNumSamples()/getSampleRate();const auto load=audioSeconds>0?static_cast<float>(elapsed/audioSeconds):0.0f;
    cpuMeter.store(cpuMeter.load()*.88f+load*.12f);topologyMeter.store(core.topologyActivity());safetyMeter.store(core.safetyEngaged());
    for(int c=0;c<channels;++c){inputMeter[static_cast<std::size_t>(c)].store(std::max(core.inputPeak(static_cast<std::size_t>(c)),inputMeter[static_cast<std::size_t>(c)].load()*.82f));outputMeter[static_cast<std::size_t>(c)].store(std::max(core.outputPeak(static_cast<std::size_t>(c)),outputMeter[static_cast<std::size_t>(c)].load()*.82f));}
}

std::vector<float> LostAudioSuiteProcessor::decodeCrtBed(double targetRate) const
{
    juce::AudioFormatManager formats;formats.registerBasicFormats();auto stream=std::make_unique<juce::MemoryInputStream>(BinaryData::crtbed_wav,(std::size_t)BinaryData::crtbed_wavSize,false);std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(stream)));if(!reader||reader->lengthInSamples<=0)return{};
    const auto length=(int)reader->lengthInSamples;juce::AudioBuffer<float> source((int)reader->numChannels,length);reader->read(&source,0,length,0,true,true);const auto ratio=targetRate/reader->sampleRate;const auto outputLength=juce::jmax(1,(int)std::floor(length*ratio));std::vector<float> output((std::size_t)outputLength);
    for(int i=0;i<outputLength;++i){const auto position=(float)i/(float)ratio;const auto a=juce::jlimit(0,length-1,(int)position);const auto b=juce::jlimit(0,length-1,a+1);const auto fraction=position-(float)a;float sample=0;for(unsigned channel=0;channel<reader->numChannels;++channel)sample+=source.getSample((int)channel,a)+(source.getSample((int)channel,b)-source.getSample((int)channel,a))*fraction;output[(std::size_t)i]=sample/(float)reader->numChannels;}return output;
}

float LostAudioSuiteProcessor::readCrtBed() noexcept
{
    if(crtBed.empty())return 0.0f;const auto a=(std::size_t)crtPosition;const auto b=(a+1u)%crtBed.size();const auto fraction=crtPosition-(float)a;const auto sample=crtBed[a]+(crtBed[b]-crtBed[a])*fraction;crtPosition+=1.0f;if(crtPosition>=(float)crtBed.size())crtPosition-=(float)crtBed.size();return sample;
}

juce::AudioProcessorEditor* LostAudioSuiteProcessor::createEditor(){return new LostAudioSuiteEditor(*this);}

void LostAudioSuiteProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state=parameters.copyState();state.setProperty("engineId","lost-audio-suite",nullptr);state.setProperty("schemaVersion",1,nullptr);if(auto xml=state.createXml())copyXmlToBinary(*xml,destination);
}

void LostAudioSuiteProcessor::setStateInformation(const void* data,int size)
{
    if(auto xml=getXmlFromBinary(data,size))if(xml->hasTagName(parameters.state.getType()))
    {
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
        for(int i=0;i<6;++i)if(auto* arm=parameters.getParameter(slotId(i,"FeedbackArm")))arm->setValueNotifyingHost(0.0f);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new LostAudioSuiteProcessor();}
