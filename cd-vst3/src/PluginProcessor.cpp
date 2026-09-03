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
    apvts.state.setProperty("schemaVersion", 5, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout CDEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    const auto zeroOne = range(0.0f, 1.0f);
    const auto defaults = lost_audio::core::mapCDMacros(0.65f, 0.25f, 0.22f, 0.18f);

    // Legacy order and IDs stay intact for existing sessions and automation.
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("clarity", "Clarity", zeroOne, 0.65f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("damage", "Damage", zeroOne, 0.25f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("tracking", "Tracking", zeroOne, 0.22f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("jitterMacro", "Jitter", zeroOne, 0.18f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("carComp", "Car Comp", zeroOne, 0.20f));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("mode", "Concealment",
        juce::StringArray { "Hold", "Mute", "Interpolate", "Repeat", "Random" }, 2));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("errorRate", "Error Rate", zeroOne, defaults.errorRate));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("burstMs", "Burst", range(1.0f, 600.0f, 1.0f), defaults.burstMs));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("repeatMs", "Repeat", range(1.0f, 400.0f, 1.0f), defaults.repeatMs));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("scratchRate", "Scratch Rate", zeroOne, defaults.scratchRate));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("scratchAmt", "Scratch Amount", zeroOne, defaults.scratchAmount));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("jitterMs", "Jitter Depth", range(0.0f, 2.0f), defaults.jitterMs));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("jitterRate", "Jitter Rate", range(1.0f, 200.0f, 0.1f), defaults.jitterRateHz));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hfLoss", "HF Loss", zeroOne, defaults.highFrequencyLoss));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("servoNoise", "Servo Noise", zeroOne, defaults.servoNoise));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("softClip", "Soft Clip", false));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", range(0.2f, 1.0f), defaults.ceiling));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output Gain", range(0.0f, 1.5f, 0.01f), defaults.outputGain));

    // Compatibility-only path for existing sessions. New patches author the
    // actual processor parameters and leave legacy macro linking disabled.
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("macroLink", "Legacy Macro Link", false));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("damageShape", "Damage Shape",
        juce::StringArray { "Radial", "Sine", "Triangle", "Square", "Saw", "Random Pits" }, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("correction", "Correction", zeroOne, defaults.correction));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("interpolationMs", "Interpolation", range(0.25f, 30.0f, 0.05f), defaults.interpolationMs));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("rotationHz", "Disc Rotation", range(2.0f, 10.0f, 0.01f), defaults.rotationHz));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("trackingRate", "Tracking Rate", zeroOne, defaults.trackingRate));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("trackingMs", "Tracking Offset", range(10.0f, 1800.0f, 1.0f), defaults.trackingMs));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("servoHunt", "Servo Hunt", zeroOne, defaults.servoHunt));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("stereoLink", "Stereo Link", zeroOne, 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("stereoWidth", "Stereo Width", range(0.0f, 2.0f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain", range(-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", zeroOne, 1.0f));

    // V3 tempo controls are appended to preserve every shipped host index.
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("tempoSync", "Tempo Sync", false));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("syncDivision", "Sync Division",
        juce::StringArray { "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" }, 2));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("syncTarget", "Sync Target",
        juce::StringArray { "Damage", "Skip", "Alternate" }, 1));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("syncStrength", "Sync Strength", zeroOne, 0.58f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("syncProbability", "Sync Probability", zeroOne, .32f));

    // V5 musical-skip controls. The trigger grid decides when an event starts;
    // these independently define the repeated slice and the bounded event.
    const juce::StringArray musicalDivisions { "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("skipSliceDivision", "Skip Slice", musicalDivisions, 5));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("skipLengthDivision", "Skip Hold", musicalDivisions, 3));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("skipRetrigger", "Skip Retrigger",
        juce::StringArray { "Ignore Active", "Restart Active" }, 0));

    return { parameters.begin(), parameters.end() };
}

void CDEngineAudioProcessor::triggerMusicalSkip(float strength, double bpm) noexcept
{
    const auto safeRate = juce::jmax(8000.0, getSampleRate());
    const auto loopSamples = juce::jmax(1, (int) std::lround(
        lost_audio::core::tempoDivisionMilliseconds(bpm, (int) value("skipSliceDivision")) * .001 * safeRate));
    const auto durationSamples = juce::jmax(loopSamples, (int) std::lround(
        lost_audio::core::tempoDivisionMilliseconds(bpm, (int) value("skipLengthDivision")) * .001 * safeRate));
    cd.triggerMusicalSkip(strength, loopSamples, durationSamples, (int) value("skipRetrigger") == 1);
}

void CDEngineAudioProcessor::triggerSkip(float strength) noexcept
{
    if (value("tempoSync") > .5f && transportPlaying.load(std::memory_order_relaxed))
        pendingQuantizedSkip.store(juce::jlimit(0.0f, 1.0f, strength), std::memory_order_release);
    else if (value("tempoSync") > .5f) triggerMusicalSkip(strength, currentBpm.load(std::memory_order_relaxed));
    else cd.triggerSkip(strength);
}

float CDEngineAudioProcessor::value(const char* parameterID) const noexcept
{
    if (const auto* raw = apvts.getRawParameterValue(parameterID)) return raw->load();
    return 0.0f;
}

bool CDEngineAudioProcessor::legacyMacrosActive() const noexcept
{
    return value("macroLink") > 0.5f;
}

void CDEngineAudioProcessor::materialiseLegacyMacros()
{
    if (!legacyMacrosActive()) return;
    const auto target = lost_audio::core::mapCDMacros(value("clarity"), value("damage"),
                                                       value("tracking"), value("jitterMacro"));
    const auto setPlain = [this] (const char* id, float plainValue)
    {
        if (auto* parameter = apvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
    };
    setPlain("errorRate", target.errorRate);
    setPlain("burstMs", target.burstMs);
    setPlain("repeatMs", target.repeatMs);
    setPlain("scratchRate", target.scratchRate);
    setPlain("scratchAmt", target.scratchAmount);
    setPlain("correction", target.correction);
    setPlain("interpolationMs", target.interpolationMs);
    setPlain("rotationHz", target.rotationHz);
    setPlain("trackingRate", target.trackingRate);
    setPlain("trackingMs", target.trackingMs);
    setPlain("servoHunt", target.servoHunt);
    setPlain("jitterMs", target.jitterMs);
    setPlain("jitterRate", target.jitterRateHz);
    setPlain("hfLoss", target.highFrequencyLoss);
    setPlain("servoNoise", target.servoNoise);
    setPlain("ceiling", target.ceiling);
    setPlain("outGain", target.outputGain * (value("outGain") / 0.98f));
    setPlain("macroLink", 0.0f);
}

void CDEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    cd.prepare(sampleRate, (std::size_t) juce::jlimit(1, 2, getTotalNumInputChannels()));
    cd.reset(0x4344454eu);
    setLatencySamples(cd.latencySamples());
    for (auto& peak : inputPeaks) peak.store(0.0f, std::memory_order_relaxed);
    for (auto& peak : outputPeaks) peak.store(0.0f, std::memory_order_relaxed);
    discPhaseMeter.store(cd.discPhase(), std::memory_order_relaxed);
    damageProgressMeter.store(0.0f, std::memory_order_relaxed);
    skipProgressMeter.store(0.0f, std::memory_order_relaxed);
    servoActivityMeter.store(0.0f, std::memory_order_relaxed);
    lastTempoStep = -1;
    lastTempoDivision = -1;
    tempoFallbackSamples = 0;
    alternateTempoTarget = 0;
    fallbackTempoStep = 0;
    lastHostPpq = 0.0;
    hostTempoWasPlaying = false;
    currentBpm.store(120.0, std::memory_order_relaxed);
    transportPlaying.store(false, std::memory_order_relaxed);
    pendingQuantizedSkip.store(0.0f, std::memory_order_relaxed);
    nextSkipEligibleStep = std::numeric_limits<std::int64_t>::min();
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

    const auto tempoSync = value("tempoSync") > 0.5f;
    bool isPlaying = false;
    bool hasPpq = false;
    double bpm = 120.0;
    double ppq = 0.0;
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
        {
            isPlaying = position->getIsPlaying();
            if (const auto hostBpm = position->getBpm()) bpm = *hostBpm;
            if (const auto hostPpq = position->getPpqPosition()) { ppq = *hostPpq; hasPpq = true; }
        }
    currentBpm.store(juce::jlimit(20.0, 400.0, bpm), std::memory_order_relaxed);
    transportPlaying.store(isPlaying, std::memory_order_relaxed);

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

    const auto division = juce::jlimit(0, 10, (int) value("syncDivision"));
    lost_audio::core::TempoEventSchedule events;
    const auto usingHostSchedule = tempoSync && isPlaying && hasPpq;
    if (!tempoSync || !isPlaying)
    {
        lastTempoStep = -1;
        lastTempoDivision = -1;
        tempoFallbackSamples = 0;
        hostTempoWasPlaying = false;
        fallbackTempoStep = 0;
        nextSkipEligibleStep = std::numeric_limits<std::int64_t>::min();
    }
    else if (usingHostSchedule)
    {
        if (!hostTempoWasPlaying || ppq < lastHostPpq - 1.0e-7)
        {
            lastTempoStep = -1;
            nextSkipEligibleStep = std::numeric_limits<std::int64_t>::min();
        }
        if (division != lastTempoDivision)
        {
            lastTempoStep = -1;
            lastTempoDivision = division;
            nextSkipEligibleStep = std::numeric_limits<std::int64_t>::min();
        }
        events = lost_audio::core::tempoEventsInBlock(ppq, bpm, division, getSampleRate(), buffer.getNumSamples());
        lastHostPpq = ppq;
        hostTempoWasPlaying = true;
    }
    else
    {
        const auto interval = juce::jmax(1, (int) std::lround(
            lost_audio::core::tempoDivisionMilliseconds(bpm, division) * 0.001 * getSampleRate()));
        auto offset = tempoFallbackSamples;
        while (offset < buffer.getNumSamples() && events.size < lost_audio::core::TempoEventSchedule::capacity)
        {
            if (offset >= 0) events.events[events.size++] = { offset, fallbackTempoStep++ };
            offset += interval;
        }
        tempoFallbackSamples = offset - buffer.getNumSamples();
        lastTempoStep = -1;
        lastTempoDivision = division;
        hostTempoWasPlaying = true;
    }

    const auto processRange = [&] (int start, int length)
    {
        if (length <= 0) return;
        std::array<float*, 2> pointers {};
        for (auto channel = 0; channel < channels; ++channel)
            pointers[(std::size_t) channel] = buffer.getWritePointer(channel, start);
        cd.process(pointers.data(), (std::size_t) channels, (std::size_t) length, parameters);
    };
    const auto triggerTempoEvent = [&] (std::int64_t stepIndex)
    {
        const auto target = juce::jlimit(0, 2, (int) value("syncTarget"));
        const auto strength = juce::jlimit(0.0f, 1.0f, value("syncStrength"));
        const auto resolved = target == 2 ? alternateTempoTarget % 2 : target;
        const auto restartSkip = (int) value("skipRetrigger") == 1;
        // Clock events are intentionally monophonic unless the user explicitly
        // chooses restart. A busy deck never accumulates invisible work.
        if ((cd.damageActive() || cd.skipActive()) && !(resolved == 1 && restartSkip)) return;
        if (resolved == 1 && !restartSkip && stepIndex < nextSkipEligibleStep) return;
        if (target == 2) ++alternateTempoTarget;
        if (resolved == 0) cd.triggerDamage(strength);
        else
        {
            triggerMusicalSkip(strength, bpm);
            if (!restartSkip)
            {
                const auto triggerBeats = lost_audio::core::tempoDivisionInBeats(division);
                const auto holdBeats = lost_audio::core::tempoDivisionInBeats((int) value("skipLengthDivision"));
                const auto occupiedSteps = std::max<std::int64_t>(1, (std::int64_t) std::ceil(holdBeats / triggerBeats - 1.0e-6f));
                nextSkipEligibleStep = stepIndex + occupiedSteps + 1; // one clean grid cell after release
            }
        }
    };

    auto cursor = 0;
    const auto eventProbability = juce::jlimit(0.0f, 1.0f, value("syncProbability"));
    for (std::size_t index = 0; index < events.size; ++index)
    {
        const auto& event = events.events[index];
        if (usingHostSchedule && event.stepIndex == lastTempoStep) continue;
        if (const auto requested = pendingQuantizedSkip.exchange(0.0f, std::memory_order_acq_rel); requested > 0.0f)
        {
            processRange(cursor, event.sampleOffset - cursor);
            cursor = event.sampleOffset;
            if (!cd.damageActive() && (!cd.skipActive() || (int) value("skipRetrigger") == 1))
                triggerMusicalSkip(requested, bpm);
        }
        if (lost_audio::core::tempoEventDecision(event.stepIndex, eventProbability))
        {
            processRange(cursor, event.sampleOffset - cursor);
            cursor = event.sampleOffset;
            triggerTempoEvent(event.stepIndex);
        }
        if (usingHostSchedule) lastTempoStep = event.stepIndex;
    }
    processRange(cursor, buffer.getNumSamples() - cursor);

    for (auto channel = 0; channel < channels; ++channel)
        outputPeaks[(std::size_t) channel].store(buffer.getMagnitude(channel, 0, buffer.getNumSamples()), std::memory_order_relaxed);
    if (channels == 1)
    {
        inputPeaks[1].store(inputPeaks[0].load(std::memory_order_relaxed), std::memory_order_relaxed);
        outputPeaks[1].store(outputPeaks[0].load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    damageActiveFlag.store(cd.damageActive(), std::memory_order_relaxed);
    skipActiveFlag.store(cd.skipActive(), std::memory_order_relaxed);
    discPhaseMeter.store(cd.discPhase(), std::memory_order_relaxed);
    damageProgressMeter.store(cd.damageProgress(), std::memory_order_relaxed);
    skipProgressMeter.store(cd.skipProgress(), std::memory_order_relaxed);
    servoActivityMeter.store(cd.servoActivity(), std::memory_order_relaxed);
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
    state.setProperty("schemaVersion", 5, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void CDEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CDEngineAudioProcessor(); }
