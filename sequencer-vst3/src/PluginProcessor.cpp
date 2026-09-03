#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::NormalisableRange<float> linear(float low, float high, float interval = 0.001f)
{
    return { low, high, interval };
}

const juce::StringArray engineNames {
    "Tape", "Transmission", "Comms", "CD", "Conference", "Camcorder",
    "Cartridge", "Television", "Occlusion", "Open Mic Night"
};
}

juce::String LostAudioSequencerProcessor::stepId(int step, const char* suffix)
{
    return "step" + juce::String(step + 1) + suffix;
}

LostAudioSequencerProcessor::LostAudioSequencerProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMS", createParameterLayout())
{
    parameters.state.setProperty("engineId", "lost-audio-sequencer", nullptr);
    parameters.state.setProperty("schemaVersion", 1, nullptr);
    cacheParameterPointers();
}

juce::AudioProcessorValueTreeState::ParameterLayout LostAudioSequencerProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterBool>("enabled", "Sequencer Enabled", true));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("division", "Step Division",
        juce::StringArray { "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" }, 4));
    p.push_back(std::make_unique<juce::AudioParameterInt>("length", "Pattern Length", 1, stepCount, stepCount));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("swing", "Swing", linear(0.0f, 0.5f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain", linear(-24.0f, 12.0f, 0.1f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outputGain", "Output Gain", linear(-24.0f, 6.0f, 0.1f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Master Mix", linear(0.0f, 1.0f), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("safety", "Output Safety", linear(0.0f, 1.0f), 0.82f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Safety Ceiling", linear(0.25f, 0.99f), 0.90f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("freeRun", "Audition Clock", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("internalBpm", "Audition Tempo", linear(40.0f, 240.0f, 1.0f), 120.0f));
    p.push_back(std::make_unique<juce::AudioParameterInt>("seed", "Probability Seed", 1, 9999, 1337));

    for (int step = 0; step < stepCount; ++step)
    {
        const auto activeByDefault = step % 4 == 0;
        const auto defaultEngine = step == 4 ? 7 : (step == 8 ? 3 : (step == 12 ? 8 : 0));
        const auto prefix = "Step " + juce::String(step + 1) + " ";
        p.push_back(std::make_unique<juce::AudioParameterBool>(stepId(step, "Enabled"), prefix + "Enabled", activeByDefault));
        p.push_back(std::make_unique<juce::AudioParameterChoice>(stepId(step, "Engine"), prefix + "Effect", engineNames, defaultEngine));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(stepId(step, "Character"), prefix + "Character", linear(0.0f, 1.0f), 0.35f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(stepId(step, "Damage"), prefix + "Damage", linear(0.0f, 1.0f), activeByDefault ? 0.32f : 0.20f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(stepId(step, "Probability"), prefix + "Probability", linear(0.0f, 1.0f), 1.0f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(stepId(step, "Mix"), prefix + "Effect Mix", linear(0.0f, 1.0f), 0.82f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(stepId(step, "Model"), prefix + "Device Model", linear(0.0f, 1.0f), 0.0f));
    }
    return { p.begin(), p.end() };
}

void LostAudioSequencerProcessor::cacheParameterPointers()
{
    enabledRef = parameters.getRawParameterValue("enabled");
    divisionRef = parameters.getRawParameterValue("division");
    lengthRef = parameters.getRawParameterValue("length");
    swingRef = parameters.getRawParameterValue("swing");
    inputGainRef = parameters.getRawParameterValue("inputGain");
    outputGainRef = parameters.getRawParameterValue("outputGain");
    mixRef = parameters.getRawParameterValue("mix");
    safetyRef = parameters.getRawParameterValue("safety");
    ceilingRef = parameters.getRawParameterValue("ceiling");
    freeRunRef = parameters.getRawParameterValue("freeRun");
    internalBpmRef = parameters.getRawParameterValue("internalBpm");
    seedRef = parameters.getRawParameterValue("seed");

    for (int step = 0; step < stepCount; ++step)
    {
        auto& refs = stepRefs[static_cast<std::size_t>(step)];
        refs.enabled = parameters.getRawParameterValue(stepId(step, "Enabled"));
        refs.engine = parameters.getRawParameterValue(stepId(step, "Engine"));
        refs.character = parameters.getRawParameterValue(stepId(step, "Character"));
        refs.damage = parameters.getRawParameterValue(stepId(step, "Damage"));
        refs.probability = parameters.getRawParameterValue(stepId(step, "Probability"));
        refs.mix = parameters.getRawParameterValue(stepId(step, "Mix"));
        refs.model = parameters.getRawParameterValue(stepId(step, "Model"));
    }
}

void LostAudioSequencerProcessor::prepareToPlay(double sampleRate, int)
{
    const auto channels = static_cast<std::size_t>(juce::jlimit(1, 2, getTotalNumInputChannels()));
    core.prepare(sampleRate, channels);
    core.reset(0x53455152u);
    setLatencySamples(core.latencySamples());
    crtBed = decodeCrtBed(sampleRate);
    crtPosition = 0.0f;
    freePpq = 0.0;
    activeStep.store(-1);
    stepPhase.store(0.0f);
    stepFired.store(false);
    transportRunning.store(false);
    for (auto& meter : inputMeter) meter.store(0.0f);
    for (auto& meter : outputMeter) meter.store(0.0f);
}

bool LostAudioSequencerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    return input == layouts.getMainOutputChannelSet() &&
           (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

lost_audio::core::SuiteParameters LostAudioSequencerProcessor::parametersForStep(int patternStep,
                                                                                 std::int64_t absoluteStep) const noexcept
{
    lost_audio::core::SuiteParameters p;
    p.inputGain = juce::Decibels::decibelsToGain(inputGainRef->load());
    p.outputGain = juce::Decibels::decibelsToGain(outputGainRef->load());
    p.mix = juce::jlimit(0.0f, 1.0f, mixRef->load());
    p.limiter = juce::jlimit(0.0f, 1.0f, safetyRef->load());
    p.ceiling = juce::jlimit(0.25f, 0.99f, ceilingRef->load());

    const auto& refs = stepRefs[static_cast<std::size_t>(juce::jlimit(0, stepCount - 1, patternStep))];
    const auto enabled = refs.enabled->load() > 0.5f;
    const auto probability = juce::jlimit(0.0f, 1.0f, refs.probability->load());
    const auto seed = static_cast<std::uint32_t>(juce::jlimit(1, 9999, juce::roundToInt(seedRef->load())));
    const auto fired = enabled && lost_audio::core::sequencerProbabilityHit(absoluteStep, seed, probability);

    auto& slot = p.slots[0];
    slot.engine = fired
        ? static_cast<lost_audio::core::SuiteEngine>(juce::jlimit(0, 9, juce::roundToInt(refs.engine->load())) + 1)
        : lost_audio::core::SuiteEngine::empty;
    slot.mix = juce::jlimit(0.0f, 1.0f, refs.mix->load());
    slot.macroA = juce::jlimit(0.0f, 1.0f, refs.character->load());
    slot.macroB = juce::jlimit(0.0f, 1.0f, refs.damage->load());
    slot.model = juce::jlimit(0.0f, 1.0f, refs.model->load());
    slot.feedbackArmed = false;
    return p;
}

float LostAudioSequencerProcessor::readCrtBed() noexcept
{
    if (crtBed.empty()) return 0.0f;
    const auto a = static_cast<std::size_t>(crtPosition);
    const auto b = (a + 1u) % crtBed.size();
    const auto fraction = crtPosition - static_cast<float>(a);
    const auto sample = crtBed[a] + (crtBed[b] - crtBed[a]) * fraction;
    crtPosition += 1.0f;
    if (crtPosition >= static_cast<float>(crtBed.size())) crtPosition -= static_cast<float>(crtBed.size());
    return sample;
}

void LostAudioSequencerProcessor::processSegment(juce::AudioBuffer<float>& buffer, int offset, int samples,
                                                  const lost_audio::core::SuiteParameters& current)
{
    const auto channels = juce::jlimit(0, 2, getTotalNumInputChannels());
    for (int chunkOffset = 0; chunkOffset < samples; chunkOffset += static_cast<int>(crtChunk.size()))
    {
        const auto count = juce::jmin(static_cast<int>(crtChunk.size()), samples - chunkOffset);
        for (int i = 0; i < count; ++i) crtChunk[static_cast<std::size_t>(i)] = readCrtBed() * 0.09f;
        float* data[] {
            buffer.getWritePointer(0, offset + chunkOffset),
            channels > 1 ? buffer.getWritePointer(1, offset + chunkOffset) : nullptr
        };
        core.process(data, static_cast<std::size_t>(channels), static_cast<std::size_t>(count), current,
                     crtBed.empty() ? nullptr : crtChunk.data());
        if (core.safetyEngaged()) safetyMeter.store(true);
    }
}

void LostAudioSequencerProcessor::processInactive(juce::AudioBuffer<float>& buffer)
{
    lost_audio::core::SuiteParameters p;
    p.inputGain = juce::Decibels::decibelsToGain(inputGainRef->load());
    p.outputGain = juce::Decibels::decibelsToGain(outputGainRef->load());
    p.mix = juce::jlimit(0.0f, 1.0f, mixRef->load());
    p.limiter = juce::jlimit(0.0f, 1.0f, safetyRef->load());
    p.ceiling = juce::jlimit(0.25f, 0.99f, ceilingRef->load());
    processSegment(buffer, 0, buffer.getNumSamples(), p);
}

void LostAudioSequencerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto channels = juce::jlimit(0, 2, getTotalNumInputChannels());
    for (int channel = channels; channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
    if (channels == 0) return;

    for (int channel = 0; channel < channels; ++channel)
        inputMeter[static_cast<std::size_t>(channel)].store(std::max(buffer.getMagnitude(channel, 0, buffer.getNumSamples()),
                                                                    inputMeter[static_cast<std::size_t>(channel)].load() * 0.82f));
    safetyMeter.store(false);

    bool hostPlaying = false;
    bool hasPpq = false;
    double bpm = juce::jlimit(40.0, 240.0, static_cast<double>(internalBpmRef->load()));
    double ppq = freePpq;
    double hostPpq = 0.0;
    if (auto* transportHead = getPlayHead())
        if (const auto position = transportHead->getPosition())
        {
            hostPlaying = position->getIsPlaying();
            if (const auto hostBpm = position->getBpm()) bpm = juce::jlimit(20.0, 400.0, *hostBpm);
            if (const auto positionPpq = position->getPpqPosition()) { hostPpq = *positionPpq; hasPpq = true; }
        }

    const auto audition = freeRunRef->load() > 0.5f;
    const auto running = enabledRef->load() > 0.5f && (hostPlaying || audition);
    transportRunning.store(running);
    bpmMeter.store(static_cast<float>(bpm));
    if (hostPlaying && hasPpq) { ppq = hostPpq; freePpq = ppq; }

    if (!running)
    {
        activeStep.store(-1);
        stepPhase.store(0.0f);
        stepFired.store(false);
        processInactive(buffer);
    }
    else
    {
        const auto division = juce::jlimit(0, 10, juce::roundToInt(divisionRef->load()));
        const auto length = juce::jlimit(1, stepCount, juce::roundToInt(lengthRef->load()));
        const auto swing = juce::jlimit(0.0f, 0.5f, swingRef->load());
        const auto ppqPerSample = bpm / (60.0 * juce::jmax(1.0, getSampleRate()));
        int offset = 0;
        while (offset < buffer.getNumSamples())
        {
            const auto position = lost_audio::core::sequencerPosition(ppq, bpm, division, swing, length, getSampleRate());
            const auto count = juce::jmin(buffer.getNumSamples() - offset, position.samplesUntilBoundary);
            const auto current = parametersForStep(position.patternStep, position.absoluteStep);
            const auto fired = current.slots[0].engine != lost_audio::core::SuiteEngine::empty;
            activeStep.store(position.patternStep);
            stepPhase.store(static_cast<float>(position.phase));
            stepFired.store(fired);
            processSegment(buffer, offset, count, current);
            offset += count;
            ppq += static_cast<double>(count) * ppqPerSample;
        }
        if (!hostPlaying || !hasPpq) freePpq = ppq;
    }

    for (int channel = 0; channel < channels; ++channel)
        outputMeter[static_cast<std::size_t>(channel)].store(std::max(buffer.getMagnitude(channel, 0, buffer.getNumSamples()),
                                                                     outputMeter[static_cast<std::size_t>(channel)].load() * 0.82f));
}

std::vector<float> LostAudioSequencerProcessor::decodeCrtBed(double targetRate) const
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto stream = std::make_unique<juce::MemoryInputStream>(BinaryData::crtbed_wav,
                                                            static_cast<std::size_t>(BinaryData::crtbed_wavSize), false);
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(stream)));
    if (!reader || reader->lengthInSamples <= 0) return {};
    const auto length = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> source(static_cast<int>(reader->numChannels), length);
    reader->read(&source, 0, length, 0, true, true);
    const auto ratio = targetRate / reader->sampleRate;
    const auto outputLength = juce::jmax(1, static_cast<int>(std::floor(length * ratio)));
    std::vector<float> output(static_cast<std::size_t>(outputLength));
    for (int i = 0; i < outputLength; ++i)
    {
        const auto position = static_cast<float>(i) / static_cast<float>(ratio);
        const auto a = juce::jlimit(0, length - 1, static_cast<int>(position));
        const auto b = juce::jlimit(0, length - 1, a + 1);
        const auto fraction = position - static_cast<float>(a);
        float sample = 0.0f;
        for (unsigned channel = 0; channel < reader->numChannels; ++channel)
            sample += source.getSample(static_cast<int>(channel), a) +
                      (source.getSample(static_cast<int>(channel), b) - source.getSample(static_cast<int>(channel), a)) * fraction;
        output[static_cast<std::size_t>(i)] = sample / static_cast<float>(reader->numChannels);
    }
    return output;
}

void LostAudioSequencerProcessor::setActualValue(const juce::String& id, float value)
{
    if (auto* parameter = parameters.getParameter(id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}

void LostAudioSequencerProcessor::setStep(int step, bool enabled, int engine, float character, float damage,
                                          float probability, float mix, float model)
{
    setActualValue(stepId(step, "Enabled"), enabled ? 1.0f : 0.0f);
    setActualValue(stepId(step, "Engine"), static_cast<float>(juce::jlimit(0, 9, engine)));
    setActualValue(stepId(step, "Character"), juce::jlimit(0.0f, 1.0f, character));
    setActualValue(stepId(step, "Damage"), juce::jlimit(0.0f, 0.82f, damage));
    setActualValue(stepId(step, "Probability"), juce::jlimit(0.0f, 1.0f, probability));
    setActualValue(stepId(step, "Mix"), juce::jlimit(0.0f, 0.92f, mix));
    setActualValue(stepId(step, "Model"), juce::jlimit(0.0f, 1.0f, model));
}

void LostAudioSequencerProcessor::clearPattern()
{
    for (int step = 0; step < stepCount; ++step)
        setStep(step, false, 0, 0.35f, 0.2f, 1.0f, 0.82f, 0.0f);
}

void LostAudioSequencerProcessor::applyPreset(int presetIndex)
{
    clearPattern();
    setActualValue("length", 16.0f);
    setActualValue("division", 4.0f);
    setActualValue("swing", 0.0f);
    setActualValue("inputGain", 0.0f);
    setActualValue("outputGain", -1.0f);
    setActualValue("mix", .88f);
    setActualValue("safety", .90f);
    setActualValue("ceiling", .88f);
    setActualValue("seed", 1337.0f);
    switch (juce::jlimit(0, 9, presetIndex))
    {
        case 0:
            setStep(0, true, 0, .42f, .28f, 1.0f, .78f, .15f);
            setStep(4, true, 7, .32f, .26f, 1.0f, .76f, .20f);
            setStep(8, true, 0, .58f, .38f, 1.0f, .82f, .45f);
            setStep(12, true, 8, .50f, .32f, 1.0f, .72f, .28f);
            break;
        case 1:
            for (int step = 0; step < stepCount; step += 2)
                setStep(step, true, (step / 2) % 4 == 0 ? 7 : ((step / 2) % 4 == 1 ? 1 : ((step / 2) % 4 == 2 ? 2 : 3)),
                        .38f + .04f * static_cast<float>(step % 4), .34f + .02f * static_cast<float>(step),
                        step % 4 == 0 ? 1.0f : .84f, .78f, static_cast<float>(step) / 15.0f);
            break;
        case 2:
            for (int step : { 3, 7, 11, 15 })
                setStep(step, true, 3, .38f, .45f + .02f * static_cast<float>(step), 1.0f, .88f, static_cast<float>(step) / 15.0f);
            break;
        case 3:
            for (int step = 0; step < stepCount; ++step)
                setStep(step, true, 8, .30f + .025f * static_cast<float>(step), .18f + .018f * static_cast<float>(step),
                        1.0f, .84f, step < 8 ? .20f : .72f);
            setActualValue("swing", .12f);
            break;
        case 4:
            for (int step = 0; step < stepCount; step += 2)
                setStep(step, true, step % 4 == 0 ? 2 : 4, .42f, .48f, step % 8 == 0 ? 1.0f : .76f, .84f, .5f);
            break;
        case 5:
            for (int step = 0; step < stepCount; step += 3)
                setStep(step, true, (step / 3) % 4 == 0 ? 6 : ((step / 3) % 4 == 1 ? 5 : ((step / 3) % 4 == 2 ? 0 : 7)),
                        .48f, .36f + .025f * static_cast<float>(step), .88f, .82f, static_cast<float>(step % 5) / 4.0f);
            setActualValue("swing", .08f);
            break;
        case 6:
            setActualValue("division", 3.0f);
            for (int step : { 0, 6, 8, 14 })
                setStep(step, true, step % 8 == 0 ? 7 : 0, .34f, .20f, .78f, .56f, step < 8 ? .22f : .48f);
            setActualValue("swing", .06f);
            break;
        case 7:
            for (int step : { 6, 7, 14, 15 })
                setStep(step, true, step % 2 == 0 ? 3 : 6, .32f, .28f, 1.0f, .68f, step < 8 ? .24f : .52f);
            break;
        case 8:
            for (int step : { 0, 4, 8, 12 })
                setStep(step, true, step % 8 == 0 ? 7 : 4, .38f, .22f, 1.0f, .54f, static_cast<float>(step) / 15.0f);
            setActualValue("swing", .05f);
            break;
        case 9:
            setActualValue("division", 3.0f);
            setStep(3, true, 3, .28f, .24f, .70f, .58f, .24f);
            setStep(11, true, 8, .31f, .20f, .64f, .52f, .56f);
            break;
    }
}

void LostAudioSequencerProcessor::randomizePattern()
{
    juce::Random random;
    const auto targetActive = 5 + random.nextInt(5);
    clearPattern();
    std::array<bool, stepCount> used {};
    for (int count = 0; count < targetActive; ++count)
    {
        int step = random.nextInt(stepCount);
        while (used[static_cast<std::size_t>(step)]) step = (step + 1) % stepCount;
        used[static_cast<std::size_t>(step)] = true;
        setStep(step, true, random.nextInt(10), .20f + random.nextFloat() * .48f,
                .16f + random.nextFloat() * .50f, .72f + random.nextFloat() * .28f,
                .58f + random.nextFloat() * .30f, random.nextFloat());
    }
    setActualValue("seed", static_cast<float>(1 + random.nextInt(9999)));
}

juce::AudioProcessorEditor* LostAudioSequencerProcessor::createEditor()
{
    return new LostAudioSequencerEditor(*this);
}

void LostAudioSequencerProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.setProperty("engineId", "lost-audio-sequencer", nullptr);
    state.setProperty("schemaVersion", 1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void LostAudioSequencerProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LostAudioSequencerProcessor();
}
