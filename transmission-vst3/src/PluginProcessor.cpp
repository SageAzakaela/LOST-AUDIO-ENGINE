#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <BinaryData.h>

#include <algorithm>
#include <cmath>

namespace
{
float clamp01(float value) noexcept { return juce::jlimit(0.0f, 1.0f, value); }

std::vector<float> resampleLinear(const std::vector<float>& input, double sourceRate, double targetRate)
{
    if (input.empty() || sourceRate <= 1000.0 || targetRate <= 1000.0) return {};
    if (std::abs(sourceRate - targetRate) < 1.0) return input;
    const auto outputLength = juce::jmax(1, (int) std::llround((double) input.size() * targetRate / sourceRate));
    std::vector<float> output((std::size_t) outputLength, 0.0f);
    const auto step = sourceRate / targetRate;
    double position = 0.0;
    for (int i = 0; i < outputLength; ++i)
    {
        const auto first = (int) position;
        const auto second = juce::jmin(first + 1, (int) input.size() - 1);
        const auto fraction = (float) (position - (double) first);
        output[(std::size_t) i] = input[(std::size_t) first]
                               + (input[(std::size_t) second] - input[(std::size_t) first]) * fraction;
        position = juce::jmin(position + step, (double) input.size() - 1.0);
    }
    return output;
}
}

TransmissionEngineAudioProcessor::TransmissionEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout TransmissionEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    const auto norm = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("bandwidth", "Bandwidth", norm, 0.45f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", norm, 0.35f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("badConnection", "Reception Damage", norm, 0.25f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("noiseProfile", "Noise Profile", norm, 0.20f));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("macroLink", "Surface Link", true));

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "High-pass", juce::NormalisableRange<float>(40.0f, 1200.0f, 1.0f), 380.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "Low-pass", juce::NormalisableRange<float>(1200.0f, 16000.0f, 1.0f), 5200.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("midGainDb", "Mid Gain", juce::NormalisableRange<float>(-6.0f, 10.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("midFreq", "Mid Frequency", juce::NormalisableRange<float>(600.0f, 3500.0f, 1.0f), 1550.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("midQ", "Mid Q", juce::NormalisableRange<float>(0.4f, 5.0f, 0.01f), 1.2f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("boxDipDb", "Cabinet Dip", juce::NormalisableRange<float>(0.0f, 8.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("comp", "Compression", norm, 0.25f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("asym", "Asymmetry", norm, 0.10f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("crush", "Converter Loss", norm, 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("wowDepth", "Carrier Drift", norm, 0.25f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("dropRate", "Drop Rate", norm, 0.25f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("dropDepth", "Drop Depth", norm, 0.35f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("crackle", "Interference", norm, 0.25f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("lfoRate", "Drift Rate", juce::NormalisableRange<float>(0.1f, 6.0f, 0.01f), 0.7f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("noiseColor", "Noise Color", norm, 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hiss", "Hiss", norm, 0.2f));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("passes", "Generations", 1, 6, 1));

    parameters.push_back(std::make_unique<juce::AudioParameterBool>("walkieMode", "Squelch Gate", false));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("walkieThresholdDb", "Squelch Threshold", juce::NormalisableRange<float>(-80.0f, -20.0f, 0.1f), -45.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("walkieMinSilenceMs", "Squelch Hold", juce::NormalisableRange<float>(80.0f, 600.0f, 1.0f), 220.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("walkieClickMs", "Squelch Length", juce::NormalisableRange<float>(5.0f, 200.0f, 1.0f), 12.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("walkieClickLevel", "Squelch Level", norm, 0.65f));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("walkieFx", "Squelch Event", juce::StringArray { "Click", "Dispatch" }, 0));

    parameters.push_back(std::make_unique<juce::AudioParameterBool>("tuningEnable", "Tuning Enable", false));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("tuningMode", "Tuning Mode", juce::StringArray { "Edges", "Search" }, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("tuningSource", "Tuning Source", juce::StringArray { "Synth", "Dispatch", "Tuning 1", "Tuning 2", "Tuning 3", "Tuning 4", "Tuning 5", "Random Tuning" }, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("tuningAmount", "Tuning Amount", norm, 0.35f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("tuningSnippetMs", "Tuning Snippet", juce::NormalisableRange<float>(40.0f, 600.0f, 1.0f), 140.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("tuningCutDepth", "Tuning Cut", norm, 0.55f));

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", norm, 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output Gain", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.92f));
    return { parameters.begin(), parameters.end() };
}

const juce::String TransmissionEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool TransmissionEngineAudioProcessor::acceptsMidi() const { return false; }
bool TransmissionEngineAudioProcessor::producesMidi() const { return false; }
bool TransmissionEngineAudioProcessor::isMidiEffect() const { return false; }
double TransmissionEngineAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int TransmissionEngineAudioProcessor::getNumPrograms() { return 1; }
int TransmissionEngineAudioProcessor::getCurrentProgram() { return 0; }
void TransmissionEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String TransmissionEngineAudioProcessor::getProgramName(int) { return {}; }
void TransmissionEngineAudioProcessor::changeProgramName(int, const juce::String&) {}

std::vector<float> TransmissionEngineAudioProcessor::decodeMp3ToMono(const void* data, std::size_t bytes, double targetSampleRate)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto stream = std::make_unique<juce::MemoryInputStream>(data, bytes, false);
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(stream)));
    if (reader == nullptr || reader->lengthInSamples <= 0) return {};
    const auto length = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> decoded((int) reader->numChannels, length);
    reader->read(&decoded, 0, length, 0, true, true);
    std::vector<float> mono((std::size_t) length, 0.0f);
    for (int i = 0; i < length; ++i)
    {
        auto sum = 0.0f;
        for (int channel = 0; channel < (int) reader->numChannels; ++channel) sum += decoded.getSample(channel, i);
        mono[(std::size_t) i] = sum / (float) juce::jmax(1, (int) reader->numChannels);
    }
    return resampleLinear(mono, reader->sampleRate, targetSampleRate);
}

void TransmissionEngineAudioProcessor::initializeTuningSamples(double sampleRate)
{
    embeddedTuningSamples.clear();
    embeddedTuningSamples.reserve(6);
    embeddedTuningSamples.push_back(decodeMp3ToMono(BinaryData::dispatch_mp3, (std::size_t) BinaryData::dispatch_mp3Size, sampleRate));
    embeddedTuningSamples.push_back(decodeMp3ToMono(BinaryData::tuning1_mp3, (std::size_t) BinaryData::tuning1_mp3Size, sampleRate));
    embeddedTuningSamples.push_back(decodeMp3ToMono(BinaryData::tuning2_mp3, (std::size_t) BinaryData::tuning2_mp3Size, sampleRate));
    embeddedTuningSamples.push_back(decodeMp3ToMono(BinaryData::tuning3_mp3, (std::size_t) BinaryData::tuning3_mp3Size, sampleRate));
    embeddedTuningSamples.push_back(decodeMp3ToMono(BinaryData::tuning4_mp3, (std::size_t) BinaryData::tuning4_mp3Size, sampleRate));
    embeddedTuningSamples.push_back(decodeMp3ToMono(BinaryData::tuning5_mp3, (std::size_t) BinaryData::tuning5_mp3Size, sampleRate));
}

void TransmissionEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    transmissionCore.prepare(sampleRate, (std::size_t) juce::jlimit(1, 2, getTotalNumInputChannels()));
    transmissionCore.reset(0x7472616eu);
    setLatencySamples(transmissionCore.latencySamples());
    initializeTuningSamples(sampleRate);
    tuningRandomState = 0x71c19e51u;
    tuningRemaining = tuningTotal = tuningCooldownRemaining = 0;
    tuningF0 = 1200.0f;
    tuningF1 = 3200.0f;
    tuningQ = 6.0f;
    tuningPhase = tuningPlayPosition = 0.0f;
    tuningPlayStep = 1.0f;
    tuningSampleIndex = 0;
    tuningFilterIc1 = tuningFilterIc2 = 0.0f;
    transportWasPlaying = false;
    outputPeak.store(0.0f, std::memory_order_relaxed);
}

void TransmissionEngineAudioProcessor::releaseResources() {}

bool TransmissionEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    return input == layouts.getMainOutputChannelSet()
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

float TransmissionEngineAudioProcessor::nextRandom() noexcept
{
    auto x = tuningRandomState == 0 ? 0x12345678u : tuningRandomState;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    tuningRandomState = x;
    return (float) ((double) x / 4294967295.0);
}

void TransmissionEngineAudioProcessor::triggerTuningEvent(float sampleRate, const TuningParameters& p)
{
    tuningTotal = juce::jmax(8, (int) ((p.snippetMs / 1000.0f) * sampleRate));
    tuningRemaining = tuningTotal;
    tuningF0 = 250.0f + nextRandom() * 3500.0f;
    tuningF1 = 600.0f + nextRandom() * 7000.0f;
    if (nextRandom() < 0.5f) std::swap(tuningF0, tuningF1);
    tuningQ = 4.0f + nextRandom() * 8.0f;
    tuningPhase = nextRandom() * juce::MathConstants<float>::twoPi;
    tuningFilterIc1 = tuningFilterIc2 = 0.0f;
    if (p.source == 7 && embeddedTuningSamples.size() >= 6) tuningSampleIndex = 1 + (int) std::floor(nextRandom() * 5.0f);
    else if (p.source >= 1 && p.source <= 6) tuningSampleIndex = p.source - 1;
    else tuningSampleIndex = 0;
    const auto* selected = tuningSampleIndex >= 0 && tuningSampleIndex < (int) embeddedTuningSamples.size()
        ? &embeddedTuningSamples[(std::size_t) tuningSampleIndex] : nullptr;
    tuningPlayPosition = selected != nullptr && !selected->empty() ? nextRandom() * (float) selected->size() : 0.0f;
    tuningPlayStep = 0.92f + 0.16f * nextRandom();
}

float TransmissionEngineAudioProcessor::sampleEmbeddedTuning(int sampleIndex, float position) const noexcept
{
    if (sampleIndex < 0 || sampleIndex >= (int) embeddedTuningSamples.size()) return 0.0f;
    const auto& samples = embeddedTuningSamples[(std::size_t) sampleIndex];
    if (samples.size() < 4) return 0.0f;
    auto wrapped = std::fmod(position, (float) samples.size());
    if (wrapped < 0.0f) wrapped += (float) samples.size();
    const auto first = (int) wrapped;
    const auto second = (first + 1) % (int) samples.size();
    const auto fraction = wrapped - (float) first;
    return samples[(std::size_t) first] + (samples[(std::size_t) second] - samples[(std::size_t) first]) * fraction;
}

float TransmissionEngineAudioProcessor::nextTuningSample(float sampleRate, const TuningParameters& p,
                                                         bool isPlaying, float& duckOut) noexcept
{
    duckOut = 1.0f;
    if (!p.enabled)
    {
        tuningRemaining = tuningCooldownRemaining = 0;
        return 0.0f;
    }
    if (tuningRemaining <= 0 && p.mode == 1 && isPlaying && tuningCooldownRemaining <= 0)
    {
        const auto ratePerSecond = 0.08f + 5.5f * p.amount * p.amount;
        if (nextRandom() < ratePerSecond / sampleRate) triggerTuningEvent(sampleRate, p);
    }
    if (tuningRemaining <= 0)
    {
        if (tuningCooldownRemaining > 0) --tuningCooldownRemaining;
        return 0.0f;
    }
    const auto t = 1.0f - (float) tuningRemaining / (float) tuningTotal;
    const auto envelope = std::sin(juce::jlimit(0.0f, 1.0f, t / 0.08f) * juce::MathConstants<float>::halfPi)
                        * std::sin(juce::jlimit(0.0f, 1.0f, (1.0f - t) / 0.12f) * juce::MathConstants<float>::halfPi);
    float artifact = 0.0f;
    if (p.source == 0)
    {
        const auto frequency = juce::jlimit(40.0f, sampleRate * 0.45f, tuningF0 + (tuningF1 - tuningF0) * t);
        const auto g = std::tan(juce::MathConstants<float>::pi * frequency / sampleRate);
        const auto k = 1.0f / juce::jmax(0.08f, tuningQ);
        const auto source = nextSigned();
        const auto v0 = source - tuningFilterIc2;
        const auto v1 = (g * v0 + tuningFilterIc1) / (1.0f + g * (g + k));
        const auto v2 = tuningFilterIc2 + g * v1;
        tuningFilterIc1 = 2.0f * v1 - tuningFilterIc1;
        tuningFilterIc2 = 2.0f * v2 - tuningFilterIc2;
        const auto oscillator = std::sin(tuningPhase) * 0.12f;
        tuningPhase += juce::MathConstants<float>::twoPi * (40.0f + 70.0f * p.amount) / sampleRate;
        artifact = std::tanh((v1 * 2.2f + oscillator) * (0.6f + 1.6f * p.amount)) * envelope;
    }
    else
    {
        const auto source = sampleEmbeddedTuning(tuningSampleIndex, tuningPlayPosition);
        tuningPlayPosition += tuningPlayStep;
        artifact = std::tanh((source * (0.9f + 1.8f * p.amount) + nextSigned() * (0.02f + 0.06f * p.amount)) * 1.1f) * envelope;
    }
    --tuningRemaining;
    tuningCooldownRemaining = juce::jmax(tuningCooldownRemaining, juce::jmax(1, (int) (0.04f * sampleRate)));
    if (tuningRemaining <= 0) tuningCooldownRemaining = juce::jmax(tuningCooldownRemaining, juce::jmax(1, (int) (0.22f * sampleRate)));
    if (p.mode == 1)
    {
        duckOut = 1.0f - p.cutDepth * envelope;
        return artifact * (p.source == 0 ? (0.55f + 0.65f * p.amount) : (0.75f + 0.65f * p.amount));
    }
    return artifact * (p.source == 0 ? (0.45f + 0.75f * p.amount) : (0.55f + 0.75f * p.amount));
}

void TransmissionEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;
    const auto inputChannels = juce::jlimit(0, 2, getTotalNumInputChannels());
    const auto outputChannels = getTotalNumOutputChannels();
    for (int channel = inputChannels; channel < outputChannels; ++channel) buffer.clear(channel, 0, buffer.getNumSamples());
    if (inputChannels == 0 || getSampleRate() <= 0.0) return;

    bool isPlaying = false;
    if (auto* transportHead = getPlayHead())
        if (const auto position = transportHead->getPosition()) isPlaying = position->getIsPlaying();
    const auto transportEdge = isPlaying != transportWasPlaying;
    transportWasPlaying = isPlaying;

    TuningParameters tuning;
    tuning.enabled = apvts.getRawParameterValue("tuningEnable")->load() > 0.5f;
    tuning.mode = juce::jlimit(0, 1, (int) apvts.getRawParameterValue("tuningMode")->load());
    tuning.source = juce::jlimit(0, 7, (int) apvts.getRawParameterValue("tuningSource")->load());
    tuning.amount = clamp01(apvts.getRawParameterValue("tuningAmount")->load());
    tuning.snippetMs = apvts.getRawParameterValue("tuningSnippetMs")->load();
    tuning.cutDepth = clamp01(apvts.getRawParameterValue("tuningCutDepth")->load());
    if (!tuning.enabled) tuningRemaining = tuningCooldownRemaining = 0;
    else if (tuning.mode == 0 && transportEdge) triggerTuningEvent((float) getSampleRate(), tuning);

    std::array<float*, 2> writePointers { nullptr, nullptr };
    for (int channel = 0; channel < inputChannels; ++channel) writePointers[(std::size_t) channel] = buffer.getWritePointer(channel);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float duck = 1.0f;
        const auto artifact = nextTuningSample((float) getSampleRate(), tuning, isPlaying, duck);
        for (int channel = 0; channel < inputChannels; ++channel)
            writePointers[(std::size_t) channel][sample] = juce::jlimit(-1.0f, 1.0f, writePointers[(std::size_t) channel][sample] * duck + artifact);
    }

    lost_audio::core::TransmissionParameters parameters;
    const auto macroTargets = lost_audio::core::mapTransmissionMacros(
        apvts.getRawParameterValue("bandwidth")->load(),
        apvts.getRawParameterValue("drive")->load(),
        apvts.getRawParameterValue("badConnection")->load(),
        apvts.getRawParameterValue("noiseProfile")->load());
    const auto surfaceLinked = apvts.getRawParameterValue("macroLink")->load() > 0.5f;
    parameters.highPassHz = surfaceLinked ? macroTargets.highPassHz : apvts.getRawParameterValue("hpHz")->load();
    parameters.lowPassHz = surfaceLinked ? macroTargets.lowPassHz : apvts.getRawParameterValue("lpHz")->load();
    parameters.midGainDb = surfaceLinked ? macroTargets.midGainDb : apvts.getRawParameterValue("midGainDb")->load();
    parameters.midFrequencyHz = surfaceLinked ? macroTargets.midFrequencyHz : apvts.getRawParameterValue("midFreq")->load();
    parameters.midQ = surfaceLinked ? macroTargets.midQ : apvts.getRawParameterValue("midQ")->load();
    parameters.boxDipDb = surfaceLinked ? macroTargets.boxDipDb : apvts.getRawParameterValue("boxDipDb")->load();
    parameters.drive = clamp01(apvts.getRawParameterValue("drive")->load());
    parameters.asymmetry = surfaceLinked ? macroTargets.asymmetry : apvts.getRawParameterValue("asym")->load();
    parameters.compression = surfaceLinked ? macroTargets.compression : apvts.getRawParameterValue("comp")->load();
    parameters.crush = apvts.getRawParameterValue("crush")->load();
    parameters.wowDepth = surfaceLinked ? macroTargets.wowDepth : apvts.getRawParameterValue("wowDepth")->load();
    parameters.dropoutRate = surfaceLinked ? macroTargets.dropoutRate : apvts.getRawParameterValue("dropRate")->load();
    parameters.dropoutDepth = surfaceLinked ? macroTargets.dropoutDepth : apvts.getRawParameterValue("dropDepth")->load();
    parameters.crackle = surfaceLinked ? macroTargets.crackle : apvts.getRawParameterValue("crackle")->load();
    parameters.lfoRateHz = surfaceLinked ? macroTargets.lfoRateHz : apvts.getRawParameterValue("lfoRate")->load();
    parameters.noise = apvts.getRawParameterValue("noiseProfile")->load();
    parameters.noiseColor = surfaceLinked ? macroTargets.noiseColor : apvts.getRawParameterValue("noiseColor")->load();
    parameters.hiss = surfaceLinked ? macroTargets.hiss : apvts.getRawParameterValue("hiss")->load();
    parameters.passes = juce::jlimit(1, 6, (int) apvts.getRawParameterValue("passes")->load());
    parameters.walkieEnabled = apvts.getRawParameterValue("walkieMode")->load() > 0.5f;
    parameters.walkieThresholdDb = apvts.getRawParameterValue("walkieThresholdDb")->load();
    parameters.walkieMinSilenceMs = apvts.getRawParameterValue("walkieMinSilenceMs")->load();
    parameters.walkieClickMs = apvts.getRawParameterValue("walkieClickMs")->load();
    parameters.walkieClickLevel = apvts.getRawParameterValue("walkieClickLevel")->load();
    parameters.walkieDispatchMode = apvts.getRawParameterValue("walkieFx")->load() > 0.5f;
    parameters.inputGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("inputGain")->load());
    parameters.mix = apvts.getRawParameterValue("mix")->load();
    parameters.outputGain = apvts.getRawParameterValue("outGain")->load();
    transmissionCore.process(writePointers.data(), (std::size_t) inputChannels, (std::size_t) buffer.getNumSamples(), parameters);

    float peak = 0.0f;
    for (int channel = 0; channel < inputChannels; ++channel)
        peak = juce::jmax(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
    outputPeak.store(juce::jlimit(0.0f, 1.0f, peak), std::memory_order_relaxed);
}

bool TransmissionEngineAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* TransmissionEngineAudioProcessor::createEditor() { return new TransmissionEngineAudioProcessorEditor(*this); }

void TransmissionEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = apvts.copyState();
    state.setProperty("engineId", "transmission", nullptr);
    state.setProperty("schemaVersion", 2, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void TransmissionEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TransmissionEngineAudioProcessor(); }
