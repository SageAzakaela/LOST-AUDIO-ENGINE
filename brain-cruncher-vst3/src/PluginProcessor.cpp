#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float twoPi = 6.2831853071795864769f;
float clamp01(float value) noexcept { return juce::jlimit(0.0f, 1.0f, value); }
}

BrainCruncherAudioProcessor::BrainCruncherAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BrainCruncherAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    const auto unit = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("crunch", "Crunch", unit, 0.86f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("body", "Body", unit, 0.68f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("bite", "Bite", unit, 0.62f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("space", "Space", unit, 0.46f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("smear", "Comb Smear", unit, 0.74f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("motion", "Motion", unit, 0.38f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("width", "Stereo Width", unit, 0.78f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("binaural", "Binaural Depth", unit, 0.72f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("pan", "Source Pan", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("headSize", "Head Spacing", unit, 0.55f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Exciter Drive", unit, 0.35f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain", juce::NormalisableRange<float>(-24.0f, 18.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", unit, 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("outputGain", "Output Gain", juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Safety Ceiling", juce::NormalisableRange<float>(0.25f, 0.98f, 0.001f), 0.92f));
    return { parameters.begin(), parameters.end() };
}

void BrainCruncherAudioProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    currentSampleRate = std::max(8000.0, sampleRate);
    const auto channels = static_cast<std::size_t>(juce::jlimit(1, 2, getTotalNumOutputChannels()));
    core.prepare(currentSampleRate, channels);
    core.reset(0x42524149u);
    dryBuffer.setSize(2, juce::jmax(1, maximumExpectedSamplesPerBlock), false, false, true);
    const auto delayLength = static_cast<std::size_t>(currentSampleRate * 0.04) + 64;
    for (auto& delay : motionDelay) delay.assign(delayLength, 0.0f);
    motionWrite = 0;
    lfoPhase = 0.0f;
    setLatencySamples(0);
}

void BrainCruncherAudioProcessor::releaseResources() {}

bool BrainCruncherAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto input = layout.getMainInputChannelSet();
    const auto output = layout.getMainOutputChannelSet();
    const auto supportedInput = input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
    const auto supportedOutput = output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
    return supportedInput && supportedOutput && output.size() >= input.size();
}

BrainCruncherAudioProcessor::Controls BrainCruncherAudioProcessor::readControls() const noexcept
{
    const auto value = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    Controls controls;
    controls.crunch = value("crunch");
    controls.body = value("body");
    controls.bite = value("bite");
    controls.space = value("space");
    controls.smear = value("smear");
    controls.motion = value("motion");
    controls.width = value("width");
    controls.binaural = value("binaural");
    controls.pan = value("pan");
    controls.headSize = value("headSize");
    controls.drive = value("drive");
    controls.inputGain = juce::Decibels::decibelsToGain(value("inputGain"));
    controls.mix = value("mix");
    controls.outputGain = juce::Decibels::decibelsToGain(value("outputGain"));
    controls.ceiling = value("ceiling");
    return controls;
}

lost_audio::core::OcclusionParameters BrainCruncherAudioProcessor::makeCoreParameters(const Controls& c) const noexcept
{
    lost_audio::core::OcclusionParameters p;
    p.material = lost_audio::core::OcclusionMaterial::metal;
    p.construction = lost_audio::core::OcclusionConstruction::loose;
    p.hpHz = 24.0f + c.body * 82.0f;
    p.lpHz = 950.0f + std::pow(c.bite, 1.35f) * 13200.0f;
    p.dipHz = 1180.0f + c.bite * 1280.0f;
    p.dipDb = -1.0f - c.crunch * 5.0f;
    p.dipQ = 0.8f + c.body * 2.8f;
    p.bumpHz = 260.0f + c.body * 610.0f;
    p.bumpDb = 1.0f + c.body * 7.0f;
    p.bumpQ = 1.2f + c.crunch * 3.0f;
    p.resonance = clamp01(0.18f + c.crunch * 0.80f);
    p.cavity = clamp01(0.10f + c.body * 0.90f);
    p.rattle = clamp01(0.06f + c.crunch * 0.94f);
    p.looseness = clamp01(0.38f + c.crunch * 0.60f);
    p.smear = clamp01(0.08f + c.smear * 0.92f);
    p.leak = 0.012f + c.space * 0.055f;
    p.leakTone = 0.64f + c.bite * 0.30f;
    p.sourceRoom = 0.10f + c.space * 0.38f;
    p.listenerRoom = 0.20f + c.space * 0.62f;
    p.roomMix = 0.06f + c.space * 0.72f;
    p.predelayMs = 2.0f + c.space * 21.0f;
    p.roomSize = 0.20f + c.space * 0.76f;
    p.damp = clamp01(0.66f - c.bite * 0.43f);
    p.inputGain = c.inputGain;
    p.mix = 1.0f;
    p.limiter = 0.82f;
    p.ceiling = 0.98f;
    p.outputGain = 1.0f;
    return p;
}

float BrainCruncherAudioProcessor::readMotionDelay(std::size_t channel, float delaySamples) const noexcept
{
    const auto& delay = motionDelay[channel];
    if (delay.empty()) return 0.0f;
    auto position = static_cast<float>(motionWrite) - delaySamples;
    while (position < 0.0f) position += static_cast<float>(delay.size());
    const auto a = static_cast<std::size_t>(position) % delay.size();
    const auto b = (a + 1) % delay.size();
    const auto blend = position - std::floor(position);
    return delay[a] + (delay[b] - delay[a]) * blend;
}

void BrainCruncherAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;
    const auto inputChannels = juce::jlimit(0, 2, getTotalNumInputChannels());
    const auto channels = juce::jlimit(0, 2, getTotalNumOutputChannels());
    const auto samples = buffer.getNumSamples();
    if (inputChannels == 0 || channels == 0 || samples == 0) return;

    if (inputChannels == 1 && channels == 2) buffer.copyFrom(1, 0, buffer, 0, 0, samples);

    if (dryBuffer.getNumSamples() < samples) dryBuffer.setSize(2, samples, false, false, true);
    for (int channel = 0; channel < channels; ++channel) dryBuffer.copyFrom(channel, 0, buffer, channel, 0, samples);

    const auto controls = readControls();
    float* data[] { buffer.getWritePointer(0), channels > 1 ? buffer.getWritePointer(1) : nullptr };
    core.process(data, static_cast<std::size_t>(channels), static_cast<std::size_t>(samples), makeCoreParameters(controls));

    const auto driveGain = 1.0f + controls.drive * 7.0f;
    const auto driveNorm = 1.0f / std::tanh(driveGain);
    const auto lfoIncrement = twoPi * (0.035f + controls.motion * 0.74f) / static_cast<float>(currentSampleRate);
    const auto panAngle = (controls.pan + 1.0f) * 0.7853981634f;
    const auto leftPan = 1.41421356f * std::cos(panAngle);
    const auto rightPan = 1.41421356f * std::sin(panAngle);
    const auto maximumItdMs = 0.18f + controls.headSize * 0.72f;
    float accumulatedMotion = 0.0f;

    for (int sample = 0; sample < samples; ++sample)
    {
        std::array<float, 2> wet { 0.0f, 0.0f };
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto excited = std::tanh(data[channel][sample] * driveGain) * driveNorm;
            motionDelay[static_cast<std::size_t>(channel)][motionWrite] = excited;
            const auto polarity = channel == 0 ? 1.0f : -1.0f;
            const auto wave = std::sin(lfoPhase + polarity * 1.5707963f);
            const auto farEar = channel == 0 ? juce::jmax(0.0f, controls.pan) : juce::jmax(0.0f, -controls.pan);
            const auto interauralDelay = controls.binaural * farEar * maximumItdMs;
            const auto delayMs = 0.35f + controls.motion * (2.2f + (wave * 0.5f + 0.5f) * 8.5f) + interauralDelay;
            const auto delayed = readMotionDelay(static_cast<std::size_t>(channel), delayMs * 0.001f * static_cast<float>(currentSampleRate));
            const auto motionBlend = controls.motion * 0.58f;
            const auto panGain = channel == 0 ? leftPan : rightPan;
            const auto spatialGain = 1.0f + (panGain - 1.0f) * controls.binaural;
            wet[static_cast<std::size_t>(channel)] = (excited + (delayed - excited) * motionBlend) * spatialGain;
            accumulatedMotion += std::abs(delayed - excited);
        }

        if (channels > 1)
        {
            const auto mid = (wet[0] + wet[1]) * 0.5f;
            auto side = (wet[0] - wet[1]) * 0.5f;
            side *= 0.25f + controls.width * 1.75f;
            wet[0] = mid + side;
            wet[1] = mid - side;
        }

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto dry = dryBuffer.getSample(channel, sample);
            inputPeaks[static_cast<std::size_t>(channel)].store(std::max(std::abs(dry), inputPeaks[static_cast<std::size_t>(channel)].load() * 0.92f));
            auto output = (dry + (wet[static_cast<std::size_t>(channel)] - dry) * controls.mix) * controls.outputGain;
            output = std::tanh(output / controls.ceiling) * controls.ceiling;
            if (!std::isfinite(output)) output = 0.0f;
            data[channel][sample] = output;
            outputPeaks[static_cast<std::size_t>(channel)].store(std::max(std::abs(output), outputPeaks[static_cast<std::size_t>(channel)].load() * 0.92f));
        }

        motionWrite = (motionWrite + 1) % motionDelay[0].size();
        lfoPhase += lfoIncrement;
        if (lfoPhase >= twoPi) lfoPhase -= twoPi;
    }

    motionMeter.store(juce::jlimit(0.0f, 1.0f, accumulatedMotion / static_cast<float>(samples * channels) * 5.0f));
}

float BrainCruncherAudioProcessor::inputPeak(int channel) const noexcept
{
    return inputPeaks[static_cast<std::size_t>(juce::jlimit(0, 1, channel))].load();
}

float BrainCruncherAudioProcessor::outputPeak(int channel) const noexcept
{
    return outputPeaks[static_cast<std::size_t>(juce::jlimit(0, 1, channel))].load();
}

const juce::String BrainCruncherAudioProcessor::getName() const { return JucePlugin_Name; }
bool BrainCruncherAudioProcessor::acceptsMidi() const { return false; }
bool BrainCruncherAudioProcessor::producesMidi() const { return false; }
bool BrainCruncherAudioProcessor::isMidiEffect() const { return false; }
double BrainCruncherAudioProcessor::getTailLengthSeconds() const { return 0.25; }
int BrainCruncherAudioProcessor::getNumPrograms() { return 1; }
int BrainCruncherAudioProcessor::getCurrentProgram() { return 0; }
void BrainCruncherAudioProcessor::setCurrentProgram(int) {}
const juce::String BrainCruncherAudioProcessor::getProgramName(int) { return {}; }
void BrainCruncherAudioProcessor::changeProgramName(int, const juce::String&) {}
bool BrainCruncherAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* BrainCruncherAudioProcessor::createEditor() { return new BrainCruncherAudioProcessorEditor(*this); }

void BrainCruncherAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = apvts.copyState();
    state.setProperty("engineId", "brain-cruncher", nullptr);
    state.setProperty("schemaVersion", 1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void BrainCruncherAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BrainCruncherAudioProcessor(); }
