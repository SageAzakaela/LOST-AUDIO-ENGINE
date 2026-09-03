#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <BinaryData.h>
#include <algorithm>
#include <cmath>

namespace
{
juce::NormalisableRange<float> range(float low, float high, float step = 0.001f)
{
    return { low, high, step };
}

std::vector<float> resample(const std::vector<float>& input, double sourceRate, double targetRate)
{
    if (input.empty() || sourceRate <= 1000.0 || targetRate <= 1000.0) return {};
    if (std::abs(sourceRate - targetRate) < 1.0) return input;
    const auto size = juce::jmax(1, (int) std::llround((double) input.size() * targetRate / sourceRate));
    std::vector<float> output((std::size_t) size);
    auto position = 0.0;
    const auto step = sourceRate / targetRate;
    for (auto i = 0; i < size; ++i)
    {
        const auto a = juce::jlimit(0, (int) input.size() - 1, (int) position);
        const auto b = juce::jmin((int) input.size() - 1, a + 1);
        const auto blend = (float) (position - a);
        output[(std::size_t) i] = input[(std::size_t) a]
            + (input[(std::size_t) b] - input[(std::size_t) a]) * blend;
        position = std::min(position + step, (double) input.size() - 1.0);
    }
    return output;
}
}

TelevisionEngineAudioProcessor::TelevisionEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    apvts.state.setProperty("engineId", "television", nullptr);
    apvts.state.setProperty("schemaVersion", 3, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout TelevisionEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    const auto n = range(0.0f, 1.0f);

    // V1/V2 order is immutable. Legacy macros remain loadable but new states
    // author the resolved DSP parameters directly.
    p.push_back(std::make_unique<juce::AudioParameterFloat>("vibe", "Legacy Set Age", n, .45f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("speaker", "Legacy Speaker", n, .55f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("agc", "Legacy AGC", n, .22f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("static", "Tuner Snow", n, .12f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hum", "Mains Hum", n, .18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("whine", "Flyback Whine", n, .08f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "High-pass", range(20.0f, 1200.0f, 1.0f), 127.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "Low-pass", range(800.0f, 18000.0f, 1.0f), 9528.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("midHumpDb", "Mid Hump", range(-6.0f, 10.0f, .1f), 3.4f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("midFreq", "Mid Freq", range(600.0f, 5000.0f, 1.0f), 1839.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("noiseHiss", "Snow Tone", n, .369f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("noiseCrackle", "Tuner Crackle", n, .077f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("bedEnable", "CRT Bed Enable", true));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bedLevel", "CRT Bed Level", n, .38f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output Gain", range(0.0f, 1.5f, .01f), .99f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("macroLink", "Legacy Macro Link", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("setModel", "Television Set", juce::StringArray { "Portable", "Console", "Broadcast Monitor", "Kitchen", "Motel" }, 1));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("receptionMode", "Reception", juce::StringArray { "Baseband", "Antenna", "Cable", "Detuned" }, 1));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Amplifier Drive", n, .333f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("comp", "Broadcast Compression", n, .168f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("tunerDrift", "Tuner Drift", n, .200f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("syncInstability", "Free Sync Faults", n, .113f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("powerSag", "Power Sag", n, .133f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("cabinet", "Cabinet", n, .759f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("cabinetRattle", "Cabinet Rattle", n, .125f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain", range(-24.0f, 24.0f, .1f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", n, 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("limiter", "Limiter", n, .409f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", range(.2f, 1.0f), .928f));

    // V3 performer parameters are append-only for host automation compatibility.
    const juce::StringArray divisions { "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
    p.push_back(std::make_unique<juce::AudioParameterBool>("faultTempoSync", "Clock Sync Faults", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("faultDivision", "Fault Division", divisions, 3));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("faultProbability", "Fault Probability", n, 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("faultStrength", "Fault Strength", n, .72f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("faultDurationMs", "Fault Duration", range(5.0f, 500.0f, 1.0f), 40.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("faultDurationSync", "Sync Fault Length", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("faultLengthDivision", "Fault Length", divisions, 5));
    return { p.begin(), p.end() };
}

float TelevisionEngineAudioProcessor::value(const char* id) const noexcept
{
    if (const auto* raw = apvts.getRawParameterValue(id)) return raw->load(std::memory_order_relaxed);
    return 0.0f;
}

bool TelevisionEngineAudioProcessor::legacyMacrosActive() const noexcept { return value("macroLink") > .5f; }

void TelevisionEngineAudioProcessor::materialiseLegacyMacros()
{
    if (!legacyMacrosActive()) return;
    const auto model = static_cast<lost_audio::core::TelevisionModel>(juce::jlimit(0, 4, (int) std::lround(value("setModel"))));
    const auto reception = static_cast<lost_audio::core::TelevisionReception>(juce::jlimit(0, 3, (int) std::lround(value("receptionMode"))));
    const auto t = lost_audio::core::mapTelevisionMacros(model, reception, value("vibe"), value("speaker"), value("agc"), value("static"));
    const auto set = [this] (const char* id, float v)
    {
        if (auto* parameter = apvts.getParameter(id)) parameter->setValueNotifyingHost(parameter->convertTo0to1(v));
    };
    set("hpHz", t.highPassHz); set("lpHz", t.lowPassHz); set("midHumpDb", t.midHumpDb); set("midFreq", t.midFrequencyHz);
    set("noiseHiss", t.noiseHiss); set("noiseCrackle", t.noiseCrackle); set("drive", t.drive); set("comp", t.compression);
    set("tunerDrift", t.tunerDrift); set("syncInstability", t.syncInstability); set("powerSag", t.powerSag);
    set("cabinet", t.cabinet); set("cabinetRattle", t.cabinetRattle); set("limiter", t.limiter); set("ceiling", t.ceiling);
    set("outGain", juce::jlimit(0.0f, 1.5f, t.outputGain * value("outGain")));
    set("macroLink", 0.0f);
}

lost_audio::core::TelevisionParameters TelevisionEngineAudioProcessor::readParameters() const noexcept
{
    lost_audio::core::TelevisionParameters p;
    p.model = static_cast<lost_audio::core::TelevisionModel>(juce::jlimit(0, 4, (int) std::lround(value("setModel"))));
    p.reception = static_cast<lost_audio::core::TelevisionReception>(juce::jlimit(0, 3, (int) std::lround(value("receptionMode"))));
    p.staticAmount = value("static"); p.hum = value("hum"); p.whine = value("whine");
    p.inputGain = juce::Decibels::decibelsToGain(value("inputGain")); p.mix = value("mix");
    if (legacyMacrosActive())
    {
        const auto t = lost_audio::core::mapTelevisionMacros(p.model, p.reception, value("vibe"), value("speaker"), value("agc"), value("static"));
        p.highPassHz=t.highPassHz; p.lowPassHz=t.lowPassHz; p.midHumpDb=t.midHumpDb; p.midFrequencyHz=t.midFrequencyHz;
        p.drive=t.drive; p.compression=t.compression; p.noiseHiss=t.noiseHiss; p.noiseCrackle=t.noiseCrackle;
        p.tunerDrift=t.tunerDrift; p.syncInstability=t.syncInstability; p.powerSag=t.powerSag; p.cabinet=t.cabinet;
        p.cabinetRattle=t.cabinetRattle; p.limiter=t.limiter; p.ceiling=t.ceiling;
        p.outputGain=juce::jlimit(0.0f,1.5f,t.outputGain*value("outGain"));
    }
    else
    {
        p.highPassHz=value("hpHz"); p.lowPassHz=value("lpHz"); p.midHumpDb=value("midHumpDb"); p.midFrequencyHz=value("midFreq");
        p.drive=value("drive"); p.compression=value("comp"); p.noiseHiss=value("noiseHiss"); p.noiseCrackle=value("noiseCrackle");
        p.tunerDrift=value("tunerDrift"); p.syncInstability=value("syncInstability"); p.powerSag=value("powerSag");
        p.cabinet=value("cabinet"); p.cabinetRattle=value("cabinetRattle"); p.limiter=value("limiter"); p.ceiling=value("ceiling"); p.outputGain=value("outGain");
    }
    if (value("faultTempoSync") > .5f) p.syncInstability = 0.0f;
    return p;
}

float TelevisionEngineAudioProcessor::faultDurationSeconds(double bpm) const noexcept
{
    if (value("faultDurationSync") > .5f)
        return lost_audio::core::tempoDivisionMilliseconds(bpm, (int) value("faultLengthDivision")) * .001f;
    return value("faultDurationMs") * .001f;
}

void TelevisionEngineAudioProcessor::triggerSyncFault() noexcept
{
    core.triggerSyncFault(value("faultStrength"), faultDurationSeconds(currentBpm));
}

std::vector<float> TelevisionEngineAudioProcessor::decodeMp3ToMono(const void* data, std::size_t bytes, double targetRate) const
{
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto stream = std::make_unique<juce::MemoryInputStream>(data, bytes, false);
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(stream)));
    if (!reader || reader->lengthInSamples <= 0) return {};
    const auto length = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> buffer((int) reader->numChannels, length);
    reader->read(&buffer, 0, length, 0, true, true);
    std::vector<float> mono((std::size_t) length);
    for (auto i = 0; i < length; ++i)
    {
        auto sample = 0.0f;
        for (auto ch = 0u; ch < reader->numChannels; ++ch) sample += buffer.getSample((int) ch, i);
        mono[(std::size_t) i] = sample / (float) reader->numChannels;
    }
    return resample(mono, reader->sampleRate, targetRate);
}

float TelevisionEngineAudioProcessor::readBedSample(float position) const noexcept
{
    if (bedSample.empty()) return 0.0f;
    auto p = std::fmod(position, (float) bedSample.size());
    if (p < 0.0f) p += (float) bedSample.size();
    const auto a = (std::size_t) p, b = (a + 1) % bedSample.size();
    return bedSample[a] + (bedSample[b] - bedSample[a]) * (p - (float) a);
}

void TelevisionEngineAudioProcessor::prepareToPlay(double sr, int block)
{
    juce::ignoreUnused(block);
    core.prepare(sr, (std::size_t) juce::jlimit(1, 2, getTotalNumInputChannels()));
    core.reset(0xdecafbadu);
    bedSample = decodeMp3ToMono(BinaryData::crtbed_wav, (std::size_t) BinaryData::crtbed_wavSize, sr);
    bedPosition = 0.0f; currentBpm = 120.0; lastTempoStep = -1; lastTempoDivision = -1;
    tempoFallbackSamples = 0; fallbackTempoStep = 0; hostTempoWasPlaying = false;
    setLatencySamples(0);
    for (auto& meter : inputPeaks) meter.store(0); for (auto& meter : outputPeaks) meter.store(0);
    for (auto& point : trace) point.store(0);
}

void TelevisionEngineAudioProcessor::releaseResources() {}
bool TelevisionEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    return input == layouts.getMainOutputChannelSet() && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void TelevisionEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi); juce::ScopedNoDenormals noDenormals;
    const auto channels = juce::jlimit(0, 2, getTotalNumInputChannels());
    for (auto ch = channels; ch < getTotalNumOutputChannels(); ++ch) buffer.clear(ch, 0, buffer.getNumSamples());
    if (channels == 0 || buffer.getNumSamples() == 0) return;
    for (auto ch = 0; ch < channels; ++ch) inputPeaks[(std::size_t) ch].store(buffer.getMagnitude(ch, 0, buffer.getNumSamples()), std::memory_order_relaxed);

    bool isPlaying = false, hasPpq = false;
    auto bpm = currentBpm, ppq = 0.0;
    if (auto* hostPlayHead = getPlayHead()) if (const auto position = hostPlayHead->getPosition())
    {
        isPlaying = position->getIsPlaying();
        if (const auto hostBpm = position->getBpm()) bpm = *hostBpm;
        if (const auto hostPpq = position->getPpqPosition()) { ppq = *hostPpq; hasPpq = true; }
    }
    currentBpm = juce::jlimit(20.0, 400.0, bpm);
    const auto tempoSync = value("faultTempoSync") > .5f;
    const auto division = juce::jlimit(0, 10, (int) value("faultDivision"));
    const auto usingHostSchedule = tempoSync && isPlaying && hasPpq;
    lost_audio::core::TempoEventSchedule events;
    if (!tempoSync || !isPlaying)
    {
        lastTempoStep = -1; lastTempoDivision = -1; tempoFallbackSamples = 0;
        fallbackTempoStep = 0; hostTempoWasPlaying = false;
    }
    else if (usingHostSchedule)
    {
        if (!hostTempoWasPlaying || ppq < lastHostPpq - 1.0e-7) lastTempoStep = -1;
        if (division != lastTempoDivision) { lastTempoStep = -1; lastTempoDivision = division; }
        events = lost_audio::core::tempoEventsInBlock(ppq, currentBpm, division, getSampleRate(), buffer.getNumSamples());
        lastHostPpq = ppq; hostTempoWasPlaying = true;
    }
    else
    {
        const auto interval = juce::jmax(1, (int) std::lround(lost_audio::core::tempoDivisionMilliseconds(currentBpm, division) * .001 * getSampleRate()));
        auto offset = tempoFallbackSamples;
        while (offset < buffer.getNumSamples() && events.size < lost_audio::core::TempoEventSchedule::capacity)
        {
            if (offset >= 0) events.events[events.size++] = { offset, fallbackTempoStep++ };
            offset += interval;
        }
        tempoFallbackSamples = offset - buffer.getNumSamples(); lastTempoStep = -1; lastTempoDivision = division; hostTempoWasPlaying = true;
    }

    const auto parameters = readParameters();
    const auto bedOn = value("bedEnable") > .5f && !bedSample.empty();
    const auto bedGain = juce::jlimit(0.0f, 1.0f, value("bedLevel")) * .86f;
    auto measuredBed = 0.0f;
    const auto processRange = [&] (int start, int length)
    {
        for (auto blockStart = start; blockStart < start + length; blockStart += (int) bedChunk.size())
        {
            const auto amount = juce::jmin((int) bedChunk.size(), start + length - blockStart);
            const float* auxiliary = nullptr;
            if (bedOn)
            {
                for (auto i = 0; i < amount; ++i)
                {
                    const auto sample = readBedSample(bedPosition) * bedGain;
                    bedChunk[(std::size_t) i] = sample; measuredBed = std::max(measuredBed, std::abs(sample));
                    bedPosition += 1.0f; if (bedPosition >= (float) bedSample.size()) bedPosition -= (float) bedSample.size();
                }
                auxiliary = bedChunk.data();
            }
            float* data[] { buffer.getWritePointer(0, blockStart), channels > 1 ? buffer.getWritePointer(1, blockStart) : nullptr };
            core.process(data, (std::size_t) channels, (std::size_t) amount, parameters, auxiliary);
        }
    };

    auto cursor = 0;
    const auto probability = juce::jlimit(0.0f, 1.0f, value("faultProbability"));
    for (std::size_t i = 0; i < events.size; ++i)
    {
        const auto& event = events.events[i];
        if (usingHostSchedule && event.stepIndex == lastTempoStep) continue;
        if (lost_audio::core::tempoEventDecision(event.stepIndex, probability, 0x54564c45ull))
        {
            processRange(cursor, event.sampleOffset - cursor); cursor = event.sampleOffset;
            core.triggerSyncFault(value("faultStrength"), faultDurationSeconds(currentBpm));
        }
        if (usingHostSchedule) lastTempoStep = event.stepIndex;
    }
    processRange(cursor, buffer.getNumSamples() - cursor);

    for (auto ch = 0; ch < channels; ++ch) outputPeaks[(std::size_t) ch].store(buffer.getMagnitude(ch, 0, buffer.getNumSamples()), std::memory_order_relaxed);
    if (channels == 1) { inputPeaks[1].store(inputPeaks[0].load()); outputPeaks[1].store(outputPeaks[0].load()); }
    bedMeter.store(std::max(measuredBed, bedMeter.load() * .84f)); staticMeter.store(core.staticLevel());
    electricalMeter.store(core.electricalLevel()); rattleMeter.store(core.rattleLevel()); syncMeter.store(core.syncProgress());
    staticState.store(core.staticActive()); syncState.store(core.syncFaultActive());
    for (std::size_t i = 0; i < trace.size(); ++i)
    {
        const auto sample = juce::jlimit(0, buffer.getNumSamples() - 1, (int) ((i * (std::size_t) buffer.getNumSamples()) / trace.size()));
        auto point = buffer.getSample(0, sample);
        if (channels > 1) point = .5f * (point + buffer.getSample(1, sample));
        trace[i].store(point, std::memory_order_relaxed);
    }
}

std::array<float, 64> TelevisionEngineAudioProcessor::outputTrace() const noexcept
{
    std::array<float, 64> result {};
    for (std::size_t i = 0; i < result.size(); ++i) result[i] = trace[i].load(std::memory_order_relaxed);
    return result;
}
float TelevisionEngineAudioProcessor::inputPeak(int ch) const noexcept { return inputPeaks[(std::size_t) juce::jlimit(0, 1, ch)].load(); }
float TelevisionEngineAudioProcessor::outputPeak(int ch) const noexcept { return outputPeaks[(std::size_t) juce::jlimit(0, 1, ch)].load(); }
const juce::String TelevisionEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool TelevisionEngineAudioProcessor::acceptsMidi() const { return false; }
bool TelevisionEngineAudioProcessor::producesMidi() const { return false; }
bool TelevisionEngineAudioProcessor::isMidiEffect() const { return false; }
double TelevisionEngineAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int TelevisionEngineAudioProcessor::getNumPrograms() { return 1; }
int TelevisionEngineAudioProcessor::getCurrentProgram() { return 0; }
void TelevisionEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String TelevisionEngineAudioProcessor::getProgramName(int) { return {}; }
void TelevisionEngineAudioProcessor::changeProgramName(int, const juce::String&) {}
bool TelevisionEngineAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* TelevisionEngineAudioProcessor::createEditor() { return new TelevisionEngineAudioProcessorEditor(*this); }
void TelevisionEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = apvts.copyState(); state.setProperty("engineId", "television", nullptr); state.setProperty("schemaVersion", 3, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}
void TelevisionEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size)) if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TelevisionEngineAudioProcessor(); }
