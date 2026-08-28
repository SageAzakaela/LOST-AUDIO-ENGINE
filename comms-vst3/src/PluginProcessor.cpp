#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
float clamp01(float value) noexcept { return juce::jlimit(0.0f, 1.0f, value); }
}

CommsEngineAudioProcessor::CommsEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CommsEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    const auto norm = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);

    // Existing identifiers remain in their original order for session and automation compatibility.
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("mode", "Mode", juce::StringArray { "Landline", "Cellular", "Intercom", "PA", "Alarm Panel" }, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("bandwidth", "Bandwidth", norm, 0.4f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", norm, 0.35f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("glitch", "Line Failure", norm, 0.2f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("noise", "Line Noise", norm, 0.18f));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("alarmTone", "Alarm Tone", false));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "High-pass", juce::NormalisableRange<float>(40.0f, 1200.0f, 1.0f), 280.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "Low-pass", juce::NormalisableRange<float>(800.0f, 14000.0f, 1.0f), 3400.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("midHumpDb", "Mid Hump", juce::NormalisableRange<float>(0.0f, 14.0f, 0.1f), 3.5f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("midFreq", "Mid Frequency", juce::NormalisableRange<float>(600.0f, 5000.0f, 1.0f), 1850.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("comp", "AGC", norm, 0.45f));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("bits", "Codec Bits", 4, 16, 12));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("rate", "Converter Rate", juce::NormalisableRange<float>(6000.0f, 48000.0f, 1.0f), 24000.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("packet", "Packet Loss", norm, 0.2f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("packetMs", "Packet Length", juce::NormalisableRange<float>(8.0f, 160.0f, 1.0f), 28.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hum", "Hum", norm, 0.25f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hiss", "Hiss", norm, 0.22f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("toneMix", "Tone Mix", norm, 0.35f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", juce::NormalisableRange<float>(0.2f, 1.0f, 0.001f), 0.92f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.95f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("echoMix", "Echo Mix", norm, 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("echoMs", "Echo Time", juce::NormalisableRange<float>(10.0f, 2500.0f, 1.0f), 180.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("echoFb", "Echo Feedback", juce::NormalisableRange<float>(0.0f, 0.92f, 0.001f), 0.28f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("echoTone", "Echo Tone", norm, 0.55f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("verbMix", "Room Mix", norm, 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("verbMs", "Room Decay", juce::NormalisableRange<float>(35.0f, 2500.0f, 1.0f), 240.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("verbDamp", "Room Damping", norm, 0.45f));

    // V2 additions are appended so old hosts retain the original parameter indices.
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("macroLink", "Surface Link", true));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("character", "Hardware Character", norm, 0.45f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("distance", "Listening Distance", norm, 0.15f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("transducer", "Transducer", norm, 0.45f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("lineAge", "Line Age", norm, 0.2f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("duplex", "Half Duplex", norm, 0.08f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("speakerRattle", "Speaker Rattle", norm, 0.12f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", norm, 1.0f));
    return { parameters.begin(), parameters.end() };
}

const juce::String CommsEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool CommsEngineAudioProcessor::acceptsMidi() const { return false; }
bool CommsEngineAudioProcessor::producesMidi() const { return false; }
bool CommsEngineAudioProcessor::isMidiEffect() const { return false; }
double CommsEngineAudioProcessor::getTailLengthSeconds() const { return 2.5; }
int CommsEngineAudioProcessor::getNumPrograms() { return 1; }
int CommsEngineAudioProcessor::getCurrentProgram() { return 0; }
void CommsEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String CommsEngineAudioProcessor::getProgramName(int) { return {}; }
void CommsEngineAudioProcessor::changeProgramName(int, const juce::String&) {}

void CommsEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    commsCore.prepare(sampleRate, (std::size_t) juce::jlimit(1, 2, getTotalNumInputChannels()));
    commsCore.reset(0x636f6d6du);
    setLatencySamples(0);
    outputPeak.store(0.0f, std::memory_order_relaxed);
}

void CommsEngineAudioProcessor::releaseResources() {}

bool CommsEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    return input == layouts.getMainOutputChannelSet()
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

lost_audio::core::CommsParameters CommsEngineAudioProcessor::readParameters() const noexcept
{
    const auto value = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    lost_audio::core::CommsParameters p;
    p.mode = (lost_audio::core::CommsMode) juce::jlimit(0, 4, (int) std::lround(value("mode")));
    p.drive = clamp01(value("drive"));
    p.alarmTone = value("alarmTone") > 0.5f;
    p.inputGain = juce::Decibels::decibelsToGain(value("inputGain"));
    p.mix = clamp01(value("mix"));

    if (value("macroLink") > 0.5f)
    {
        const auto targets = lost_audio::core::mapCommsMacros(p.mode, value("bandwidth"), p.drive,
                                                               value("glitch"), value("noise"),
                                                               value("character"), value("distance"));
        p.highPassHz = targets.highPassHz; p.lowPassHz = targets.lowPassHz;
        p.midHumpDb = targets.midHumpDb; p.midFrequencyHz = targets.midFrequencyHz;
        p.compression = targets.compression; p.bits = targets.bits; p.converterRateHz = targets.converterRateHz;
        p.packetLoss = targets.packetLoss; p.packetLengthMs = targets.packetLengthMs;
        p.hum = targets.hum; p.hiss = targets.hiss; p.toneMix = targets.toneMix;
        p.transducer = targets.transducer; p.lineAge = targets.lineAge; p.duplex = targets.duplex;
        p.speakerRattle = targets.speakerRattle; p.distance = targets.distance;
        p.echoMix = targets.echoMix; p.echoMs = targets.echoMs; p.echoFeedback = targets.echoFeedback;
        p.echoTone = targets.echoTone; p.roomMix = targets.roomMix; p.roomMs = targets.roomMs;
        p.roomDamping = targets.roomDamping;
        p.outputGain = juce::jlimit(0.0f, 1.5f, targets.outputGain * value("outGain") / 0.95f);
        p.ceiling = targets.ceiling;
    }
    else
    {
        p.highPassHz = value("hpHz"); p.lowPassHz = value("lpHz");
        p.midHumpDb = value("midHumpDb"); p.midFrequencyHz = value("midFreq");
        p.compression = value("comp"); p.bits = (int) std::lround(value("bits")); p.converterRateHz = value("rate");
        p.packetLoss = value("packet"); p.packetLengthMs = value("packetMs");
        p.hum = value("hum"); p.hiss = value("hiss"); p.toneMix = value("toneMix");
        p.transducer = value("transducer"); p.lineAge = value("lineAge"); p.duplex = value("duplex");
        p.speakerRattle = value("speakerRattle"); p.distance = value("distance");
        p.echoMix = value("echoMix"); p.echoMs = value("echoMs"); p.echoFeedback = value("echoFb");
        p.echoTone = value("echoTone"); p.roomMix = value("verbMix"); p.roomMs = value("verbMs");
        p.roomDamping = value("verbDamp"); p.outputGain = value("outGain"); p.ceiling = value("ceiling");
    }
    return p;
}

void CommsEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;
    const auto inputChannels = juce::jlimit(0, 2, getTotalNumInputChannels());
    const auto outputChannels = getTotalNumOutputChannels();
    for (int channel = inputChannels; channel < outputChannels; ++channel) buffer.clear(channel, 0, buffer.getNumSamples());
    if (inputChannels == 0 || getSampleRate() <= 0.0) return;
    float* channelPointers[] { buffer.getWritePointer(0), inputChannels > 1 ? buffer.getWritePointer(1) : nullptr };
    const auto parameters = readParameters();
    commsCore.process(channelPointers, (std::size_t) inputChannels, (std::size_t) buffer.getNumSamples(), parameters);
    const auto current = commsCore.outputPeak();
    const auto previous = outputPeak.load(std::memory_order_relaxed);
    outputPeak.store(std::max(current, previous * 0.86f), std::memory_order_relaxed);
}

bool CommsEngineAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* CommsEngineAudioProcessor::createEditor() { return new CommsEngineAudioProcessorEditor(*this); }

void CommsEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("engineId", "comms", nullptr);
    state.setProperty("schemaVersion", 2, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void CommsEngineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CommsEngineAudioProcessor(); }
