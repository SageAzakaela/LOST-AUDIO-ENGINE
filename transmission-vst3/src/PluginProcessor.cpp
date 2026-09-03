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
    apvts.state.setProperty("engineId", "transmission", nullptr);
    apvts.state.setProperty("schemaVersion", 4, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout TransmissionEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    const auto norm = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("bandwidth", "Bandwidth", norm, 0.45f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", norm, 0.35f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("badConnection", "Reception Damage", norm, 0.25f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("noiseProfile", "Noise Profile", norm, 0.20f));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("macroLink", "Legacy Macro Link", false));

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "High-pass", juce::NormalisableRange<float>(40.0f, 1200.0f, 1.0f), 354.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "Low-pass", juce::NormalisableRange<float>(1200.0f, 16000.0f, 1.0f), 6254.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("midGainDb", "Mid Gain", juce::NormalisableRange<float>(-6.0f, 10.0f, 0.1f), 3.1f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("midFreq", "Mid Frequency", juce::NormalisableRange<float>(600.0f, 3500.0f, 1.0f), 1608.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("midQ", "Mid Q", juce::NormalisableRange<float>(0.4f, 5.0f, 0.01f), 1.70f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("boxDipDb", "Cabinet Dip", juce::NormalisableRange<float>(0.0f, 8.0f, 0.1f), 1.2f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("comp", "Compression", norm, 0.408f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("asym", "Asymmetry", norm, 0.21f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("crush", "Converter Loss", norm, 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("wowDepth", "Carrier Drift", norm, 0.25f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("dropRate", "Drop Rate", norm, 0.25f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("dropDepth", "Drop Depth", norm, 0.35f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("crackle", "Interference", norm, 0.25f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("lfoRate", "Drift Rate", juce::NormalisableRange<float>(0.1f, 6.0f, 0.01f), 0.85f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("noiseColor", "Noise Color", norm, 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hiss", "Hiss", norm, 0.19f));
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
    // Appended V3 timing controls preserve all existing automation indices.
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("tuningSync", "Search Tempo Sync", false));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("tuningDivision", "Search Division",
        juce::StringArray { "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" }, 3));
    const juce::StringArray divisions { "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("tuningProbability", "Search Probability", norm, 0.75f));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("tuningLengthSync", "Sync Search Length", false));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("tuningLengthDivision", "Search Length", divisions, 4));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("dropoutTempoSync", "Clock Sync Dropouts", false));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("dropoutDivision", "Dropout Grid", divisions, 2));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("dropoutProbability", "Dropout Probability", norm, 0.35f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("dropoutStrength", "Dropout Strength", norm, 0.68f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("dropoutDurationMs", "Dropout Duration", juce::NormalisableRange<float>(5.0f, 600.0f, 1.0f), 80.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("dropoutLengthSync", "Sync Dropout Length", false));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("dropoutLengthDivision", "Dropout Length", divisions, 4));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("carrierTempoSync", "Clock Sync Carrier", false));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("carrierDivision", "Carrier Cycle", divisions, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Safety Ceiling", juce::NormalisableRange<float>(0.2f, 1.0f, 0.001f), 0.95f));
    return { parameters.begin(), parameters.end() };
}

float TransmissionEngineAudioProcessor::value(const char* id) const noexcept
{
    if (const auto* raw = apvts.getRawParameterValue(id)) return raw->load(std::memory_order_relaxed);
    return 0.0f;
}

bool TransmissionEngineAudioProcessor::legacyMacrosActive() const noexcept { return value("macroLink") > 0.5f; }

void TransmissionEngineAudioProcessor::materialiseLegacyMacros()
{
    if (!legacyMacrosActive()) return;
    const auto targets = lost_audio::core::mapTransmissionMacros(value("bandwidth"), value("drive"), value("badConnection"), value("noiseProfile"));
    const auto set = [this] (const char* id, float plain)
    {
        if (auto* parameter = apvts.getParameter(id)) parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
    };
    set("hpHz", targets.highPassHz); set("lpHz", targets.lowPassHz); set("midGainDb", targets.midGainDb);
    set("midFreq", targets.midFrequencyHz); set("midQ", targets.midQ); set("boxDipDb", targets.boxDipDb);
    set("asym", targets.asymmetry); set("comp", targets.compression); set("wowDepth", targets.wowDepth);
    set("dropRate", targets.dropoutRate); set("dropDepth", targets.dropoutDepth); set("crackle", targets.crackle);
    set("lfoRate", targets.lfoRateHz); set("noiseColor", targets.noiseColor); set("hiss", targets.hiss);
    set("macroLink", 0.0f);
}

float TransmissionEngineAudioProcessor::inputPeak(int channel) const noexcept
{
    return inputPeaks[(size_t) juce::jlimit(0, 1, channel)].load(std::memory_order_relaxed);
}

float TransmissionEngineAudioProcessor::outputPeakForChannel(int channel) const noexcept
{
    return outputPeaks[(size_t) juce::jlimit(0, 1, channel)].load(std::memory_order_relaxed);
}

std::array<float, 64> TransmissionEngineAudioProcessor::outputTrace() const noexcept
{
    std::array<float, 64> result {};
    for (size_t index = 0; index < result.size(); ++index) result[index] = trace[index].load(std::memory_order_relaxed);
    return result;
}

float TransmissionEngineAudioProcessor::dropoutDurationSeconds(double bpm) const noexcept
{
    if (value("dropoutLengthSync") > 0.5f)
        return lost_audio::core::tempoDivisionMilliseconds(bpm, (int) value("dropoutLengthDivision")) * 0.001f;
    return value("dropoutDurationMs") * 0.001f;
}

void TransmissionEngineAudioProcessor::triggerDropout() noexcept
{
    transmissionCore.triggerDropout(value("dropoutStrength"), dropoutDurationSeconds(currentBpm));
}

lost_audio::core::TransmissionParameters TransmissionEngineAudioProcessor::readParameters(double bpm) const noexcept
{
    lost_audio::core::TransmissionParameters p;
    const auto legacy = legacyMacrosActive();
    const auto targets = lost_audio::core::mapTransmissionMacros(value("bandwidth"), value("drive"), value("badConnection"), value("noiseProfile"));
    p.highPassHz = legacy ? targets.highPassHz : value("hpHz"); p.lowPassHz = legacy ? targets.lowPassHz : value("lpHz");
    p.midGainDb = legacy ? targets.midGainDb : value("midGainDb"); p.midFrequencyHz = legacy ? targets.midFrequencyHz : value("midFreq");
    p.midQ = legacy ? targets.midQ : value("midQ"); p.boxDipDb = legacy ? targets.boxDipDb : value("boxDipDb");
    p.drive = clamp01(value("drive")); p.asymmetry = legacy ? targets.asymmetry : value("asym");
    p.compression = legacy ? targets.compression : value("comp"); p.crush = value("crush");
    p.wowDepth = legacy ? targets.wowDepth : value("wowDepth"); p.dropoutRate = legacy ? targets.dropoutRate : value("dropRate");
    p.dropoutDepth = legacy ? targets.dropoutDepth : value("dropDepth"); p.crackle = legacy ? targets.crackle : value("crackle");
    p.lfoRateHz = legacy ? targets.lfoRateHz : value("lfoRate"); p.noise = value("noiseProfile");
    p.noiseColor = legacy ? targets.noiseColor : value("noiseColor"); p.hiss = legacy ? targets.hiss : value("hiss");
    p.passes = juce::jlimit(1, 6, (int) value("passes")); p.walkieEnabled = value("walkieMode") > 0.5f;
    p.walkieThresholdDb = value("walkieThresholdDb"); p.walkieMinSilenceMs = value("walkieMinSilenceMs");
    p.walkieClickMs = value("walkieClickMs"); p.walkieClickLevel = value("walkieClickLevel"); p.walkieDispatchMode = value("walkieFx") > 0.5f;
    p.inputGain = juce::Decibels::decibelsToGain(value("inputGain")); p.mix = value("mix"); p.outputGain = value("outGain"); p.ceiling = value("ceiling");
    if (value("dropoutTempoSync") > 0.5f) p.dropoutRate = 0.0f;
    if (value("carrierTempoSync") > 0.5f) p.lfoRateHz = lost_audio::core::tempoDivisionRateHz(bpm, (int) value("carrierDivision"));
    return p;
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
    lastTuningTempoStep = -1;
    fallbackTuningTempoStep = 0;
    lastTuningDivision = -1;
    tuningTempoFallbackSamples = 0;
    lastHostPpq = 0.0;
    currentBpm = 120.0;
    hostTuningWasPlaying = false;
    lastDropoutTempoStep = -1;
    fallbackDropoutTempoStep = 0;
    lastDropoutDivision = -1;
    dropoutTempoFallbackSamples = 0;
    lastDropoutHostPpq = 0.0;
    hostDropoutWasPlaying = false;
    pendingTuningTrigger.store(false);
    outputPeak.store(0.0f, std::memory_order_relaxed);
    for (auto& meter : inputPeaks) meter.store(0.0f);
    for (auto& meter : outputPeaks) meter.store(0.0f);
    for (auto& point : trace) point.store(0.0f);
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
    if (tuningRemaining <= 0 && p.mode == 1 && isPlaying && !p.tempoSync && tuningCooldownRemaining <= 0)
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

    for (int channel = 0; channel < inputChannels; ++channel)
        inputPeaks[(size_t) channel].store(buffer.getMagnitude(channel, 0, buffer.getNumSamples()), std::memory_order_relaxed);
    if (inputChannels == 1) inputPeaks[1].store(inputPeaks[0].load(std::memory_order_relaxed), std::memory_order_relaxed);

    bool isPlaying = false;
    bool hasPpq = false;
    double bpm = currentBpm;
    double ppq = 0.0;
    if (auto* transportHead = getPlayHead())
        if (const auto position = transportHead->getPosition())
        {
            isPlaying = position->getIsPlaying();
            if (const auto hostBpm = position->getBpm()) bpm = *hostBpm;
            if (const auto hostPpq = position->getPpqPosition()) { ppq = *hostPpq; hasPpq = true; }
        }
    currentBpm = juce::jlimit(20.0, 400.0, bpm);
    const auto transportEdge = isPlaying != transportWasPlaying;
    transportWasPlaying = isPlaying;

    TuningParameters tuning;
    tuning.enabled = value("tuningEnable") > 0.5f;
    tuning.mode = juce::jlimit(0, 1, (int) value("tuningMode"));
    tuning.source = juce::jlimit(0, 7, (int) value("tuningSource"));
    tuning.amount = clamp01(value("tuningAmount"));
    tuning.snippetMs = value("tuningLengthSync") > 0.5f
        ? lost_audio::core::tempoDivisionMilliseconds(currentBpm, (int) value("tuningLengthDivision"))
        : value("tuningSnippetMs");
    tuning.cutDepth = clamp01(value("tuningCutDepth"));
    tuning.tempoSync = value("tuningSync") > 0.5f;
    tuning.probability = value("tuningProbability");
    if (!tuning.enabled) tuningRemaining = tuningCooldownRemaining = 0;
    else if (tuning.mode == 0 && transportEdge) triggerTuningEvent((float) getSampleRate(), tuning);

    const auto makeSchedule = [&] (bool sync, int division, int& fallbackSamples,
                                   std::int64_t& fallbackStep, std::int64_t& lastStep,
                                   int& lastDivision, double& previousPpq, bool& wasPlaying)
    {
        lost_audio::core::TempoEventSchedule schedule;
        if (!sync || !isPlaying)
        {
            lastStep = -1; lastDivision = -1; fallbackSamples = 0; fallbackStep = 0; wasPlaying = false;
            return schedule;
        }
        if (hasPpq)
        {
            if (!wasPlaying || ppq < previousPpq - 1.0e-7) lastStep = -1;
            if (division != lastDivision) { lastStep = -1; lastDivision = division; }
            schedule = lost_audio::core::tempoEventsInBlock(ppq, currentBpm, division, getSampleRate(), buffer.getNumSamples());
            previousPpq = ppq; wasPlaying = true;
            return schedule;
        }
        const auto interval = juce::jmax(1, (int) std::lround(
            lost_audio::core::tempoDivisionMilliseconds(currentBpm, division) * 0.001 * getSampleRate()));
        auto offset = fallbackSamples;
        while (offset < buffer.getNumSamples() && schedule.size < lost_audio::core::TempoEventSchedule::capacity)
        {
            if (offset >= 0) schedule.events[schedule.size++] = { offset, fallbackStep++ };
            offset += interval;
        }
        fallbackSamples = offset - buffer.getNumSamples(); lastStep = -1; lastDivision = division; wasPlaying = true;
        return schedule;
    };

    const auto tuningDivision = juce::jlimit(0, 10, (int) value("tuningDivision"));
    auto tuningEvents = makeSchedule(tuning.enabled && tuning.mode == 1 && tuning.tempoSync,
        tuningDivision, tuningTempoFallbackSamples, fallbackTuningTempoStep,
        lastTuningTempoStep, lastTuningDivision, lastHostPpq, hostTuningWasPlaying);
    const auto dropoutDivision = juce::jlimit(0, 10, (int) value("dropoutDivision"));
    auto dropoutEvents = makeSchedule(value("dropoutTempoSync") > 0.5f, dropoutDivision,
        dropoutTempoFallbackSamples, fallbackDropoutTempoStep, lastDropoutTempoStep,
        lastDropoutDivision, lastDropoutHostPpq, hostDropoutWasPlaying);

    if (pendingTuningTrigger.exchange(false, std::memory_order_acq_rel) && tuning.enabled && tuningRemaining <= 0)
        triggerTuningEvent((float) getSampleRate(), tuning);

    std::array<float*, 2> writePointers { nullptr, nullptr };
    for (int channel = 0; channel < inputChannels; ++channel) writePointers[(std::size_t) channel] = buffer.getWritePointer(channel);
    size_t tuningEventIndex = 0;
    auto measuredTuning = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        while (tuningEventIndex < tuningEvents.size && tuningEvents.events[tuningEventIndex].sampleOffset == sample)
        {
            const auto event = tuningEvents.events[tuningEventIndex++];
            if ((!hasPpq || event.stepIndex != lastTuningTempoStep) && tuningRemaining <= 0
                && lost_audio::core::tempoEventDecision(event.stepIndex, tuning.probability, 0x74756e65ull))
            {
                triggerTuningEvent((float) getSampleRate(), tuning);
            }
            if (hasPpq) lastTuningTempoStep = event.stepIndex;
        }
        float duck = 1.0f;
        const auto artifact = nextTuningSample((float) getSampleRate(), tuning, isPlaying, duck);
        measuredTuning = juce::jmax(measuredTuning, std::abs(artifact));
        for (int channel = 0; channel < inputChannels; ++channel)
            writePointers[(std::size_t) channel][sample] = juce::jlimit(-1.0f, 1.0f, writePointers[(std::size_t) channel][sample] * duck + artifact);
    }

    const auto parameters = readParameters(currentBpm);
    const auto processRange = [&] (int start, int length)
    {
        if (length <= 0) return;
        std::array<float*, 2> rangePointers { nullptr, nullptr };
        for (int channel = 0; channel < inputChannels; ++channel) rangePointers[(size_t) channel] = writePointers[(size_t) channel] + start;
        transmissionCore.process(rangePointers.data(), (size_t) inputChannels, (size_t) length, parameters);
    };
    auto cursor = 0;
    for (size_t index = 0; index < dropoutEvents.size; ++index)
    {
        const auto event = dropoutEvents.events[index];
        if (hasPpq && event.stepIndex == lastDropoutTempoStep) continue;
        processRange(cursor, event.sampleOffset - cursor); cursor = event.sampleOffset;
        if (!transmissionCore.dropoutActive()
            && lost_audio::core::tempoEventDecision(event.stepIndex, value("dropoutProbability"), 0x7472616eull))
            transmissionCore.triggerDropout(value("dropoutStrength"), dropoutDurationSeconds(currentBpm));
        if (hasPpq) lastDropoutTempoStep = event.stepIndex;
    }
    processRange(cursor, buffer.getNumSamples() - cursor);

    carrierTelemetry.store(transmissionCore.carrierDisplacementMs(), std::memory_order_relaxed);
    dropoutState.store(transmissionCore.dropoutActive(), std::memory_order_relaxed);
    dropoutTelemetry.store(transmissionCore.dropoutProgress(), std::memory_order_relaxed);
    compressionTelemetry.store(transmissionCore.compressionReduction(), std::memory_order_relaxed);
    noiseTelemetry.store(transmissionCore.noiseActivity(), std::memory_order_relaxed);
    interferenceTelemetry.store(transmissionCore.crackleActivity(), std::memory_order_relaxed);
    squelchState.store(transmissionCore.squelchClosed(), std::memory_order_relaxed);
    squelchTelemetry.store(transmissionCore.squelchEventActivity(), std::memory_order_relaxed);
    limiterTelemetry.store(transmissionCore.limiterActivity(), std::memory_order_relaxed);
    tuningState.store(tuningRemaining > 0, std::memory_order_relaxed);
    tuningTelemetry.store(tuningRemaining > 0 && tuningTotal > 0 ? 1.0f - (float) tuningRemaining / (float) tuningTotal : measuredTuning, std::memory_order_relaxed);
    tuningAsset.store(tuning.source == 0 ? 0 : tuningSampleIndex + 1, std::memory_order_relaxed);

    float peak = 0.0f;
    for (int channel = 0; channel < inputChannels; ++channel)
    {
        const auto channelPeak = buffer.getMagnitude(channel, 0, buffer.getNumSamples());
        outputPeaks[(size_t) channel].store(channelPeak, std::memory_order_relaxed);
        peak = juce::jmax(peak, channelPeak);
    }
    if (inputChannels == 1) outputPeaks[1].store(outputPeaks[0].load(std::memory_order_relaxed), std::memory_order_relaxed);
    outputPeak.store(juce::jlimit(0.0f, 1.0f, peak), std::memory_order_relaxed);
    for (size_t point = 0; point < trace.size(); ++point)
    {
        const auto sample = juce::jlimit(0, buffer.getNumSamples() - 1, (int) std::floor((double) point * buffer.getNumSamples() / trace.size()));
        auto sampleValue = 0.0f;
        for (int channel = 0; channel < inputChannels; ++channel) sampleValue += buffer.getSample(channel, sample);
        trace[point].store(sampleValue / (float) inputChannels, std::memory_order_relaxed);
    }
}

bool TransmissionEngineAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* TransmissionEngineAudioProcessor::createEditor() { return new TransmissionEngineAudioProcessorEditor(*this); }

void TransmissionEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = apvts.copyState();
    state.setProperty("engineId", "transmission", nullptr);
    state.setProperty("schemaVersion", 4, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void TransmissionEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            apvts.state.setProperty("engineId", "transmission", nullptr);
            apvts.state.setProperty("schemaVersion", 4, nullptr);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TransmissionEngineAudioProcessor(); }
