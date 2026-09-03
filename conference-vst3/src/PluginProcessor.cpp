#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <lost_audio/core/TempoSync.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
float clamp01(float value) noexcept { return juce::jlimit(0.0f, 1.0f, value); }
const juce::StringArray divisions { "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
}

ConferenceEngineAudioProcessor::ConferenceEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    apvts.state.setProperty("engineId", "conference", nullptr);
    apvts.state.setProperty("schemaVersion", 3, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout ConferenceEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    const auto n01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);
    // V1 and V2 IDs remain in their original order for host automation and project recall.
    p.push_back(std::make_unique<juce::AudioParameterChoice>("mode", "Mode", juce::StringArray { "Discord", "Zoom", "Skype", "Cell" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bandwidth", "Bandwidth", n01, 0.45f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("codec", "Codec", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dropouts", "Dropouts", n01, 0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("jitter", "Jitter Macro", n01, 0.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("robot", "Robot", n01, 0.02f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("noise", "Noise", n01, 0.04f));
    // These defaults resolve the original default macro recipe into explicit canonical values.
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "High-pass", juce::NormalisableRange<float>(40.0f, 1200.0f, 1.0f), 248.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "Low-pass", juce::NormalisableRange<float>(800.0f, 16000.0f, 1.0f), 5277.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("midHumpDb", "Mid Hump", juce::NormalisableRange<float>(0.0f, 14.0f, 0.1f), 3.8f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("midFreq", "Mid Frequency", juce::NormalisableRange<float>(600.0f, 5000.0f, 1.0f), 2194.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("concealMode", "Conceal", juce::StringArray { "Hold", "Mute", "Interpolate", "Repeat" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("packetLoss", "Packet Loss", n01, 0.012f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("packetMs", "Packet Length", juce::NormalisableRange<float>(4.0f, 240.0f, 1.0f), 19.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("repeatMs", "Repeat Length", juce::NormalisableRange<float>(1.0f, 300.0f, 1.0f), 24.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("jitterMs", "Jitter Depth", juce::NormalisableRange<float>(0.0f, 8.0f, 0.001f), 0.817f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("jitterRate", "Jitter Rate", juce::NormalisableRange<float>(1.0f, 220.0f, 0.1f), 10.4f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("gate", "Gate", n01, 0.124f));
    p.push_back(std::make_unique<juce::AudioParameterInt>("bits", "Codec Bits", 4, 16, 12));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("rate", "Codec Rate", juce::NormalisableRange<float>(6000.0f, 48000.0f, 1.0f), 35980.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", juce::NormalisableRange<float>(0.2f, 1.0f, 0.001f), 0.92f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.98f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("macroLink", "Legacy Macro Link", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("burstiness", "Burst Memory", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("suppression", "Noise Suppression", n01, 0.600f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("agc", "Automatic Gain", n01, 0.646f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bufferSlip", "Buffer Slip", n01, 0.02f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bandwidthSwitch", "Bandwidth Collapse", n01, 0.03f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("comfortNoise", "Comfort Noise", n01, 0.10f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", n01, 1.0f));
    // V3 performer parameters are append-only.
    p.push_back(std::make_unique<juce::AudioParameterBool>("packetTempoSync", "Clock Packet Loss", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("packetDivision", "Packet Trigger Grid", divisions, 2));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("packetProbability", "Packet Trigger Probability", n01, 0.45f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("packetDepth", "Packet Failure Depth", n01, 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("packetDurationMs", "Packet Failure Length", juce::NormalisableRange<float>(4.0f, 1200.0f, 1.0f), 80.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("packetLengthSync", "Clock Packet Length", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("packetLengthDivision", "Packet Failure Length Grid", divisions, 4));
    p.push_back(std::make_unique<juce::AudioParameterBool>("robotTempoSync", "Clock Robot Grain", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("robotDivision", "Robot Trigger Grid", divisions, 3));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("robotProbability", "Robot Trigger Probability", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("robotStrength", "Robot Event Strength", n01, 0.88f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("robotDurationMs", "Robot Event Length", juce::NormalisableRange<float>(8.0f, 2000.0f, 1.0f), 180.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("robotLengthSync", "Clock Robot Length", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("robotLengthDivision", "Robot Event Length Grid", divisions, 3));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("robotGrainMs", "Robot Grain Length", juce::NormalisableRange<float>(2.0f, 90.0f, 0.1f), 24.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("jitterTempoSync", "Clock Jitter Updates", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("jitterDivision", "Jitter Update Grid", divisions, 4));
    return { p.begin(), p.end() };
}

float ConferenceEngineAudioProcessor::value(const char* id) const noexcept
{
    if (const auto* raw = apvts.getRawParameterValue(id)) return raw->load(std::memory_order_relaxed);
    return 0.0f;
}

bool ConferenceEngineAudioProcessor::legacyMacrosActive() const noexcept { return value("macroLink") > 0.5f; }

void ConferenceEngineAudioProcessor::materialiseLegacyMacros()
{
    if (!legacyMacrosActive()) return;
    const auto mode = (lost_audio::core::ConferenceMode) juce::jlimit(0, 3, (int) std::lround(value("mode")));
    const auto t = lost_audio::core::mapConferenceMacros(mode, value("bandwidth"), value("codec"), value("dropouts"), value("jitter"), value("robot"), value("noise"));
    const auto set = [this] (const char* id, float plain)
    {
        if (auto* parameter = apvts.getParameter(id)) parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
    };
    set("hpHz", t.highPassHz); set("lpHz", t.lowPassHz); set("midHumpDb", t.midHumpDb); set("midFreq", t.midFrequencyHz);
    set("packetLoss", t.packetLoss); set("packetMs", t.packetMs); set("repeatMs", t.repeatMs); set("jitterMs", t.jitterMs);
    set("jitterRate", t.jitterRate); set("gate", t.gate); set("bits", (float) t.bits); set("rate", t.converterRateHz);
    set("robot", t.robot); set("noise", t.noise); set("burstiness", t.burstiness); set("suppression", t.suppression);
    set("agc", t.agc); set("bufferSlip", t.bufferSlip); set("bandwidthSwitch", t.bandwidthSwitch);
    set("comfortNoise", t.comfortNoise); set("outGain", t.outputGain); set("ceiling", t.ceiling); set("macroLink", 0.0f);
}

std::array<float, 64> ConferenceEngineAudioProcessor::outputTrace() const noexcept
{
    std::array<float, 64> result {};
    for (std::size_t i = 0; i < result.size(); ++i) result[i] = trace[i].load(std::memory_order_relaxed);
    return result;
}

float ConferenceEngineAudioProcessor::packetDurationSeconds(double bpm) const noexcept
{
    if (value("packetLengthSync") > 0.5f)
        return lost_audio::core::tempoDivisionMilliseconds(bpm, (int) value("packetLengthDivision")) * 0.001f;
    return value("packetDurationMs") * 0.001f;
}

float ConferenceEngineAudioProcessor::robotDurationSeconds(double bpm) const noexcept
{
    if (value("robotLengthSync") > 0.5f)
        return lost_audio::core::tempoDivisionMilliseconds(bpm, (int) value("robotLengthDivision")) * 0.001f;
    return value("robotDurationMs") * 0.001f;
}

lost_audio::core::ConferenceParameters ConferenceEngineAudioProcessor::readParameters(double bpm) const noexcept
{
    lost_audio::core::ConferenceParameters p;
    p.mode = (lost_audio::core::ConferenceMode) juce::jlimit(0, 3, (int) std::lround(value("mode")));
    p.concealment = (lost_audio::core::ConferenceConcealment) juce::jlimit(0, 3, (int) std::lround(value("concealMode")));
    p.inputGain = juce::Decibels::decibelsToGain(value("inputGain")); p.mix = clamp01(value("mix"));
    const auto legacy = legacyMacrosActive();
    const auto t = lost_audio::core::mapConferenceMacros(p.mode, value("bandwidth"), value("codec"), value("dropouts"), value("jitter"), value("robot"), value("noise"));
    p.highPassHz = legacy ? t.highPassHz : value("hpHz"); p.lowPassHz = legacy ? t.lowPassHz : value("lpHz");
    p.midHumpDb = legacy ? t.midHumpDb : value("midHumpDb"); p.midFrequencyHz = legacy ? t.midFrequencyHz : value("midFreq");
    p.packetLoss = legacy ? t.packetLoss : value("packetLoss"); p.packetMs = legacy ? t.packetMs : value("packetMs");
    p.repeatMs = legacy ? t.repeatMs : value("repeatMs"); p.jitterMs = legacy ? t.jitterMs : value("jitterMs");
    p.jitterRate = legacy ? t.jitterRate : value("jitterRate"); p.gate = legacy ? t.gate : value("gate");
    p.bits = legacy ? t.bits : (int) std::lround(value("bits")); p.converterRateHz = legacy ? t.converterRateHz : value("rate");
    p.robot = legacy ? t.robot : value("robot"); p.noise = legacy ? t.noise : value("noise");
    p.burstiness = legacy ? t.burstiness : value("burstiness"); p.suppression = legacy ? t.suppression : value("suppression");
    p.agc = legacy ? t.agc : value("agc"); p.bufferSlip = legacy ? t.bufferSlip : value("bufferSlip");
    p.bandwidthSwitch = legacy ? t.bandwidthSwitch : value("bandwidthSwitch"); p.comfortNoise = legacy ? t.comfortNoise : value("comfortNoise");
    p.outputGain = legacy ? juce::jlimit(0.0f, 1.5f, t.outputGain * value("outGain") / 0.98f) : value("outGain");
    p.ceiling = legacy ? t.ceiling : value("ceiling");
    if (value("packetTempoSync") > 0.5f) p.packetLoss = 0.0f;
    if (value("robotTempoSync") > 0.5f) p.robot = 0.0f;
    if (value("jitterTempoSync") > 0.5f) p.jitterRate = lost_audio::core::tempoDivisionRateHz(bpm, (int) value("jitterDivision"));
    return p;
}

const juce::String ConferenceEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool ConferenceEngineAudioProcessor::acceptsMidi() const { return false; }
bool ConferenceEngineAudioProcessor::producesMidi() const { return false; }
bool ConferenceEngineAudioProcessor::isMidiEffect() const { return false; }
double ConferenceEngineAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int ConferenceEngineAudioProcessor::getNumPrograms() { return 1; }
int ConferenceEngineAudioProcessor::getCurrentProgram() { return 0; }
void ConferenceEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String ConferenceEngineAudioProcessor::getProgramName(int) { return {}; }
void ConferenceEngineAudioProcessor::changeProgramName(int, const juce::String&) {}

void ConferenceEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    conferenceCore.prepare(sampleRate, (std::size_t) juce::jlimit(1, 2, getTotalNumInputChannels()));
    conferenceCore.reset(0x636f6e66u); setLatencySamples(conferenceCore.latencySamples()); currentBpm = 120.0;
    pendingPacketTrigger.store(false); pendingRobotTrigger.store(false);
    for (auto& peak : inputPeaks) peak.store(0.0f); for (auto& peak : outputPeaks) peak.store(0.0f);
    for (auto& point : trace) point.store(0.0f);
}

void ConferenceEngineAudioProcessor::releaseResources() {}

bool ConferenceEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    return input == layouts.getMainOutputChannelSet()
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void ConferenceEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi); juce::ScopedNoDenormals noDenormals;
    const auto channels = juce::jlimit(0, 2, getTotalNumInputChannels());
    for (int channel = channels; channel < getTotalNumOutputChannels(); ++channel) buffer.clear(channel, 0, buffer.getNumSamples());
    if (channels == 0 || getSampleRate() <= 0.0 || buffer.getNumSamples() <= 0) return;
    for (int channel = 0; channel < channels; ++channel)
        inputPeaks[(std::size_t) channel].store(buffer.getMagnitude(channel, 0, buffer.getNumSamples()), std::memory_order_relaxed);
    if (channels == 1) inputPeaks[1].store(inputPeaks[0].load(std::memory_order_relaxed), std::memory_order_relaxed);

    bool playing = false, hasPpq = false; double bpm = currentBpm, ppq = 0.0;
    if (auto* head = getPlayHead()) if (const auto position = head->getPosition())
    {
        playing = position->getIsPlaying();
        if (const auto hostBpm = position->getBpm()) bpm = *hostBpm;
        if (const auto hostPpq = position->getPpqPosition()) { ppq = *hostPpq; hasPpq = true; }
    }
    currentBpm = juce::jlimit(20.0, 400.0, bpm);
    const auto sampleCount = buffer.getNumSamples();
    lost_audio::core::TempoEventSchedule packetSchedule, robotSchedule;
    if (playing && hasPpq && value("packetTempoSync") > 0.5f)
        packetSchedule = lost_audio::core::tempoEventsInBlock(ppq, currentBpm, (int) value("packetDivision"), getSampleRate(), sampleCount);
    if (playing && hasPpq && value("robotTempoSync") > 0.5f)
        robotSchedule = lost_audio::core::tempoEventsInBlock(ppq, currentBpm, (int) value("robotDivision"), getSampleRate(), sampleCount);

    std::array<int, 68> boundaries {}; int boundaryCount = 0;
    boundaries[(std::size_t) boundaryCount++] = 0; boundaries[(std::size_t) boundaryCount++] = sampleCount;
    for (std::size_t i = 0; i < packetSchedule.size; ++i) boundaries[(std::size_t) boundaryCount++] = packetSchedule.events[i].sampleOffset;
    for (std::size_t i = 0; i < robotSchedule.size; ++i) boundaries[(std::size_t) boundaryCount++] = robotSchedule.events[i].sampleOffset;
    std::sort(boundaries.begin(), boundaries.begin() + boundaryCount);
    boundaryCount = (int) std::distance(boundaries.begin(), std::unique(boundaries.begin(), boundaries.begin() + boundaryCount));

    auto firePacket = pendingPacketTrigger.exchange(false, std::memory_order_acq_rel);
    auto fireRobot = pendingRobotTrigger.exchange(false, std::memory_order_acq_rel);
    auto maxJitter = 0.0f, maxSuppression = 0.0f, maxAgc = 0.0f, maxComfort = 0.0f, maxLimiter = 0.0f;
    const auto parameters = readParameters(currentBpm);
    for (int boundary = 0; boundary < boundaryCount - 1; ++boundary)
    {
        const auto offset = boundaries[(std::size_t) boundary];
        for (std::size_t i = 0; i < packetSchedule.size; ++i)
            if (packetSchedule.events[i].sampleOffset == offset
                && lost_audio::core::tempoEventDecision(packetSchedule.events[i].stepIndex, value("packetProbability"), 0x636f6e66504b54ull))
                firePacket = true;
        for (std::size_t i = 0; i < robotSchedule.size; ++i)
            if (robotSchedule.events[i].sampleOffset == offset
                && lost_audio::core::tempoEventDecision(robotSchedule.events[i].stepIndex, value("robotProbability"), 0x636f6e66524254ull))
                fireRobot = true;
        if (firePacket && !conferenceCore.packetLost()) conferenceCore.triggerPacketLoss(value("packetDepth"), packetDurationSeconds(currentBpm));
        if (fireRobot && !conferenceCore.robotActive()) conferenceCore.triggerRobot(value("robotStrength"), robotDurationSeconds(currentBpm), value("robotGrainMs"));
        firePacket = fireRobot = false;

        const auto count = boundaries[(std::size_t) boundary + 1] - offset;
        if (count <= 0) continue;
        float* pointers[] { buffer.getWritePointer(0) + offset, channels > 1 ? buffer.getWritePointer(1) + offset : nullptr };
        conferenceCore.process(pointers, (std::size_t) channels, (std::size_t) count, parameters);
        maxJitter = std::max(maxJitter, conferenceCore.jitterActivity()); maxSuppression = std::max(maxSuppression, conferenceCore.suppressionActivity());
        maxAgc = std::max(maxAgc, conferenceCore.agcActivity()); maxComfort = std::max(maxComfort, conferenceCore.comfortNoiseActivity());
        maxLimiter = std::max(maxLimiter, conferenceCore.limiterActivity());
    }

    for (int channel = 0; channel < channels; ++channel)
        outputPeaks[(std::size_t) channel].store(buffer.getMagnitude(channel, 0, sampleCount), std::memory_order_relaxed);
    if (channels == 1) outputPeaks[1].store(outputPeaks[0].load(std::memory_order_relaxed), std::memory_order_relaxed);
    for (int i = 0; i < (int) trace.size(); ++i)
    {
        const auto first = i * sampleCount / (int) trace.size(), last = juce::jmax(first + 1, (i + 1) * sampleCount / (int) trace.size());
        trace[(std::size_t) i].store(buffer.getRMSLevel(0, first, juce::jmin(sampleCount, last) - first), std::memory_order_relaxed);
    }
    lossActive.store(conferenceCore.packetLost()); robotState.store(conferenceCore.robotActive());
    slipState.store(conferenceCore.bufferSlipActive()); bandwidthState.store(conferenceCore.bandwidthCollapsed());
    packetProgressMeter.store(conferenceCore.packetLossProgress()); robotProgressMeter.store(conferenceCore.robotProgress());
    jitterMeter.store(maxJitter); suppressionMeter.store(maxSuppression); agcMeter.store(maxAgc);
    comfortMeter.store(maxComfort); limiterMeter.store(maxLimiter);
}

bool ConferenceEngineAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* ConferenceEngineAudioProcessor::createEditor() { return new ConferenceEngineAudioProcessorEditor(*this); }
void ConferenceEngineAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState(); state.setProperty("engineId", "conference", nullptr); state.setProperty("schemaVersion", 3, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}
void ConferenceEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size)) if (xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        apvts.state.setProperty("engineId", "conference", nullptr); apvts.state.setProperty("schemaVersion", 3, nullptr);
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ConferenceEngineAudioProcessor(); }
