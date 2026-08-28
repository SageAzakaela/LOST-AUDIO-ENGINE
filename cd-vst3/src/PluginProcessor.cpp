#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::NormalisableRange<float> range(float minimum, float maximum, float interval = 0.001f)
{
    return { minimum, maximum, interval };
}
}

CDEngineAudioProcessor::CDEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    apvts.state.setProperty("engineId", "cd", nullptr);
    apvts.state.setProperty("schemaVersion", 2, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout CDEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    const auto zeroOne = range(0.0f, 1.0f);

    // Legacy order and IDs stay intact for existing sessions and automation.
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("clarity", "Clarity", zeroOne, 0.65f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("damage", "Damage", zeroOne, 0.25f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("tracking", "Tracking", zeroOne, 0.22f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("jitterMacro", "Jitter", zeroOne, 0.18f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("carComp", "Car Comp", zeroOne, 0.20f));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("mode", "Concealment",
        juce::StringArray { "Hold", "Mute", "Interpolate", "Repeat", "Random" }, 2));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("errorRate", "Error Rate", zeroOne, 0.12f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("burstMs", "Burst", range(1.0f, 600.0f, 1.0f), 18.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("repeatMs", "Repeat", range(1.0f, 400.0f, 1.0f), 36.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("scratchRate", "Scratch Rate", zeroOne, 0.14f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("scratchAmt", "Scratch Amount", zeroOne, 0.20f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("jitterMs", "Jitter Depth", range(0.0f, 2.0f), 0.025f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("jitterRate", "Jitter Rate", range(1.0f, 200.0f, 0.1f), 34.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hfLoss", "HF Loss", zeroOne, 0.025f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("servoNoise", "Servo Noise", zeroOne, 0.08f));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("softClip", "Soft Clip", false));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", range(0.2f, 1.0f), 0.94f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output Gain", range(0.0f, 1.5f, 0.01f), 0.98f));

    parameters.push_back(std::make_unique<juce::AudioParameterBool>("macroLink", "Macro Link", true));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("damageShape", "Damage Shape",
        juce::StringArray { "Radial", "Sine", "Triangle", "Square", "Saw", "Random Pits" }, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("correction", "Correction", zeroOne, 0.88f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("interpolationMs", "Interpolation", range(0.25f, 30.0f, 0.05f), 5.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("rotationHz", "Disc Rotation", range(2.0f, 10.0f, 0.01f), 5.2f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("trackingRate", "Tracking Rate", zeroOne, 0.08f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("trackingMs", "Tracking Offset", range(10.0f, 1800.0f, 1.0f), 140.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("servoHunt", "Servo Hunt", zeroOne, 0.18f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("stereoLink", "Stereo Link", zeroOne, 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("stereoWidth", "Stereo Width", range(0.0f, 2.0f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain", range(-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", zeroOne, 1.0f));

    return { parameters.begin(), parameters.end() };
}

float CDEngineAudioProcessor::value(const char* parameterID) const noexcept
{
    if (const auto* raw = apvts.getRawParameterValue(parameterID)) return raw->load();
    return 0.0f;
}

void CDEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    cd.prepare(sampleRate, (std::size_t) juce::jlimit(1, 2, getTotalNumInputChannels()));
    cd.reset(0x4344454eu);
    setLatencySamples(cd.latencySamples());
    for (auto& peak : inputPeaks) peak.store(0.0f, std::memory_order_relaxed);
    for (auto& peak : outputPeaks) peak.store(0.0f, std::memory_order_relaxed);
}

void CDEngineAudioProcessor::releaseResources() {}

bool CDEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void CDEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;
    const auto channels = juce::jlimit(1, 2, getTotalNumInputChannels());
    for (auto channel = channels; channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    for (auto channel = 0; channel < channels; ++channel)
        inputPeaks[(std::size_t) channel].store(buffer.getMagnitude(channel, 0, buffer.getNumSamples()), std::memory_order_relaxed);

    lost_audio::core::CDParameters parameters;
    parameters.mode = static_cast<lost_audio::core::CDConcealment>(juce::jlimit(0, 4, (int) value("mode")));
    parameters.damageShape = static_cast<lost_audio::core::CDDamageShape>(juce::jlimit(0, 5, (int) value("damageShape")));

    const auto macroLink = value("macroLink") > 0.5f;
    if (macroLink)
    {
        const auto target = lost_audio::core::mapCDMacros(value("clarity"), value("damage"), value("tracking"), value("jitterMacro"));
        parameters.errorRate = target.errorRate;
        parameters.burstMs = target.burstMs;
        parameters.repeatMs = target.repeatMs;
        parameters.scratchRate = target.scratchRate;
        parameters.scratchAmount = target.scratchAmount;
        parameters.correction = target.correction;
        parameters.interpolationMs = target.interpolationMs;
        parameters.rotationHz = target.rotationHz;
        parameters.trackingRate = target.trackingRate;
        parameters.trackingMs = target.trackingMs;
        parameters.servoHunt = target.servoHunt;
        parameters.jitterMs = target.jitterMs;
        parameters.jitterRateHz = target.jitterRateHz;
        parameters.highFrequencyLoss = target.highFrequencyLoss;
        parameters.servoNoise = target.servoNoise;
        parameters.ceiling = target.ceiling;
        parameters.outputGain = target.outputGain * (value("outGain") / 0.98f);
    }
    else
    {
        parameters.errorRate = value("errorRate");
        parameters.burstMs = value("burstMs");
        parameters.repeatMs = value("repeatMs");
        parameters.scratchRate = value("scratchRate");
        parameters.scratchAmount = value("scratchAmt");
        parameters.correction = value("correction");
        parameters.interpolationMs = value("interpolationMs");
        parameters.rotationHz = value("rotationHz");
        parameters.trackingRate = value("trackingRate");
        parameters.trackingMs = value("trackingMs");
        parameters.servoHunt = value("servoHunt");
        parameters.jitterMs = value("jitterMs");
        parameters.jitterRateHz = value("jitterRate");
        parameters.highFrequencyLoss = value("hfLoss");
        parameters.servoNoise = value("servoNoise");
        parameters.ceiling = value("ceiling");
        parameters.outputGain = value("outGain");
    }
    parameters.carCompression = value("carComp");
    parameters.stereoLink = value("stereoLink");
    parameters.stereoWidth = value("stereoWidth");
    parameters.inputGain = juce::Decibels::decibelsToGain(value("inputGain"));
    parameters.mix = value("mix");
    parameters.softClip = value("softClip") > 0.5f;

    std::array<float*, 2> pointers {};
    for (auto channel = 0; channel < channels; ++channel) pointers[(std::size_t) channel] = buffer.getWritePointer(channel);
    cd.process(pointers.data(), (std::size_t) channels, (std::size_t) buffer.getNumSamples(), parameters);

    for (auto channel = 0; channel < channels; ++channel)
        outputPeaks[(std::size_t) channel].store(buffer.getMagnitude(channel, 0, buffer.getNumSamples()), std::memory_order_relaxed);
    if (channels == 1)
    {
        inputPeaks[1].store(inputPeaks[0].load(std::memory_order_relaxed), std::memory_order_relaxed);
        outputPeaks[1].store(outputPeaks[0].load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    damageActiveFlag.store(cd.damageActive(), std::memory_order_relaxed);
    skipActiveFlag.store(cd.skipActive(), std::memory_order_relaxed);
}

float CDEngineAudioProcessor::inputPeak(int channel) const noexcept
{
    return inputPeaks[(std::size_t) juce::jlimit(0, 1, channel)].load(std::memory_order_relaxed);
}

float CDEngineAudioProcessor::outputPeak(int channel) const noexcept
{
    return outputPeaks[(std::size_t) juce::jlimit(0, 1, channel)].load(std::memory_order_relaxed);
}

const juce::String CDEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool CDEngineAudioProcessor::acceptsMidi() const { return false; }
bool CDEngineAudioProcessor::producesMidi() const { return false; }
bool CDEngineAudioProcessor::isMidiEffect() const { return false; }
double CDEngineAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int CDEngineAudioProcessor::getNumPrograms() { return 1; }
int CDEngineAudioProcessor::getCurrentProgram() { return 0; }
void CDEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String CDEngineAudioProcessor::getProgramName(int) { return {}; }
void CDEngineAudioProcessor::changeProgramName(int, const juce::String&) {}

bool CDEngineAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* CDEngineAudioProcessor::createEditor() { return new CDEngineAudioProcessorEditor(*this); }

void CDEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = apvts.copyState();
    state.setProperty("engineId", "cd", nullptr);
    state.setProperty("schemaVersion", 2, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void CDEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CDEngineAudioProcessor(); }
