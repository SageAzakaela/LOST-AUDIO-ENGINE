#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <lost_audio/core/TempoSync.h>

#include <algorithm>
#include <cmath>

namespace
{
juce::NormalisableRange<float> linear(float minimum, float maximum, float interval = 0.001f)
{
    return { minimum, maximum, interval };
}
}

OpenMicNightAudioProcessor::OpenMicNightAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMS", createParameterLayout())
{
    parameters.state.setProperty("engineId", "open-mic-night", nullptr);
    parameters.state.setProperty("schemaVersion", 4, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout OpenMicNightAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    const auto n01 = linear(0.0f, 1.0f);
    // V1 order and IDs are immutable for session compatibility.
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hotMic", "Hot Mic", n01, 0.55f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("fbFreq", "Feedback Frequency", linear(200.0f, 5000.0f, 1.0f), 1800.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ringQ", "Feedback Q", linear(1.0f, 40.0f, 0.1f), 16.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("fbDelayMs", "Feedback Delay", linear(8.0f, 70.0f, 0.1f), 24.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("fbTone", "Feedback Tone", n01, 0.55f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("wall", "Mic Distance", n01, 0.65f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("room", "Venue Size", n01, 0.504f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("limit", "Safety", n01, 0.72f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output", linear(0.0f, 1.5f, 0.01f), 0.95f));
    // V2 parameters are appended only.
    p.push_back(std::make_unique<juce::AudioParameterBool>("macroLink", "Legacy Macro Link", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("micModel", "Microphone", juce::StringArray { "Dynamic Handheld", "Vocal Condenser", "Cheap Karaoke", "Podium", "Vintage Ribbon" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("venueModel", "Venue", juce::StringArray { "Corner Club", "Dive Bar", "Rehearsal Room", "Warehouse", "Rooftop", "Community Hall" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("paModel", "PA System", juce::StringArray { "Compact PA", "Column Array", "Paging Horn", "Tired Combo", "Blown Stack" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("proximity", "Proximity", n01, 0.351f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("micDrive", "Mic Preamp", n01, 0.421f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("paDrive", "PA Drive", n01, 0.331f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("monitorLevel", "Stage Monitor", n01, 0.481f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("feedbackArm", "Arm Feedback", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("feedbackAmount", "Feedback Amount", n01, 0.45f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("feedbackBuildMs", "Feedback Build", linear(40.0f, 1800.0f, 1.0f), 420.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("feedbackReleaseMs", "Feedback Release", linear(30.0f, 1200.0f, 1.0f), 180.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("stageBleed", "Stage Bleed", n01, 0.235f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("crowdLevel", "Audience", n01, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("crowdMood", "Audience Energy", n01, 0.45f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("electricalNoise", "Electrical Noise", linear(0.0f, 0.12f), 0.017f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("wallAbsorption", "Audience Absorption", n01, 0.267f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("stereoWidth", "Venue Width", n01, 0.82f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input", linear(0.0f, 2.0f, 0.01f), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", n01, 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Safety Ceiling", linear(0.25f, 0.99f), 0.90f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("venueBedEnable", "Venue Bed Enable", true));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("venueBedLevel", "Venue Bed Level", n01, 0.42f));
    const juce::StringArray divisions { "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
    p.push_back(std::make_unique<juce::AudioParameterChoice>("crowdEventType", "Audience Event", juce::StringArray { "Cheer", "Applause", "Chatter", "Heckle" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterBool>("crowdSync", "Clock Audience Events", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("crowdDivision", "Audience Grid", divisions, 1));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("crowdProbability", "Audience Probability", n01, .35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("crowdStrength", "Audience Event Strength", n01, .62f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("crowdDurationMs", "Audience Event Length", linear(80.0f, 5000.0f, 1.0f), 1200.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("crowdLengthSync", "Clock Audience Length", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("crowdLengthDivision", "Audience Length Grid", divisions, 0));
    p.push_back(std::make_unique<juce::AudioParameterBool>("feedbackConducted", "Conduct Feedback", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("feedbackSync", "Clock Feedback Events", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("feedbackDivision", "Feedback Grid", divisions, 2));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("feedbackProbability", "Feedback Probability", n01, .28f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("feedbackDurationMs", "Feedback Event Length", linear(40.0f, 4000.0f, 1.0f), 640.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("feedbackLengthSync", "Clock Feedback Length", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("feedbackLengthDivision", "Feedback Length Grid", divisions, 2));
    // V4 audience behavior is appended for session compatibility.
    p.push_back(std::make_unique<juce::AudioParameterChoice>("crowdBed", "Audience Bed", juce::StringArray { "Quiet Tables", "Bar Banter", "Busy Room", "Rowdy Bar" }, 1));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("crowdBehavior", "Audience Behavior", juce::StringArray { "Reactive", "Steady Ambience", "Manual / Clocked" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("crowdSensitivity", "Audience Sensitivity", n01, .58f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("crowdResponse", "Audience Response", n01, .68f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("crowdCooldownMs", "Audience Recovery", linear(1000.0f, 12000.0f, 10.0f), 4500.0f));
    return { p.begin(), p.end() };
}

float OpenMicNightAudioProcessor::value(const char* id) const noexcept { return parameters.getRawParameterValue(id)->load(); }
bool OpenMicNightAudioProcessor::legacyMacrosActive() const noexcept { return value("macroLink") > .5f; }
void OpenMicNightAudioProcessor::materialiseLegacyMacros()
{
    if (!legacyMacrosActive()) return;
    using namespace lost_audio::core;
    const auto mapped=mapOpenMicMacros((OpenMicModel)juce::jlimit(0,4,(int)value("micModel")),(OpenMicVenue)juce::jlimit(0,5,(int)value("venueModel")),(OpenMicPA)juce::jlimit(0,4,(int)value("paModel")),value("hotMic"),value("wall"),value("room"));
    const auto set=[this](const char*id,float plain){if(auto*p=parameters.getParameter(id))p->setValueNotifyingHost(p->convertTo0to1(plain));};
    set("proximity",mapped.proximity);set("micDrive",mapped.micDrive);set("paDrive",mapped.paDrive);set("monitorLevel",mapped.monitorLevel);set("stageBleed",mapped.stageBleed);set("room",mapped.roomAmount);set("wallAbsorption",mapped.wallAbsorption);set("electricalNoise",mapped.electricalNoise);set("macroLink",0);
}
std::array<float,64> OpenMicNightAudioProcessor::outputTrace() const noexcept { std::array<float,64> result{};for(std::size_t i=0;i<result.size();++i)result[i]=trace[i].load(std::memory_order_relaxed);return result; }
bool OpenMicNightAudioProcessor::embeddedCrowdReady() const noexcept
{
    return std::all_of(crowdBeds.begin(), crowdBeds.end(), [](const auto& asset) { return asset.ready(); })
        && std::all_of(reactionAssets.begin(), reactionAssets.end(), [](const auto& asset) { return asset.ready(); });
}

OpenMicNightAudioProcessor::StereoAsset OpenMicNightAudioProcessor::decodeAsset(const void* data, std::size_t bytes, double targetRate) const
{
    StereoAsset result;
    if (data == nullptr || bytes == 0 || targetRate <= 0.0) return result;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto stream = std::make_unique<juce::MemoryInputStream>(data, bytes, false);
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(stream)));
    if (!reader || reader->lengthInSamples <= 0 || reader->numChannels == 0) return result;

    const auto sourceLength = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> source(static_cast<int>(reader->numChannels), sourceLength);
    reader->read(&source, 0, sourceLength, 0, true, true);
    const auto ratio = targetRate / reader->sampleRate;
    const auto outputLength = juce::jmax(1, static_cast<int>(std::floor(sourceLength * ratio)));
    for (auto& channel : result.channels) channel.resize(static_cast<std::size_t>(outputLength));
    for (int i = 0; i < outputLength; ++i)
    {
        const auto position = static_cast<float>(i) / static_cast<float>(ratio);
        const auto a = juce::jlimit(0, sourceLength - 1, static_cast<int>(position));
        const auto b = juce::jlimit(0, sourceLength - 1, a + 1);
        const auto fraction = position - static_cast<float>(a);
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto sourceChannel = juce::jmin(channel, static_cast<int>(reader->numChannels) - 1);
            const auto first = source.getSample(sourceChannel, a);
            result.channels[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] = first + (source.getSample(sourceChannel, b) - first) * fraction;
        }
    }
    double energy = 0.0;
    std::size_t energySamples = 0;
    for (const auto& channel : result.channels)
        for (const auto sample : channel) { energy += static_cast<double>(sample) * sample; ++energySamples; }
    const auto rms = energySamples > 0 ? static_cast<float>(std::sqrt(energy / static_cast<double>(energySamples))) : 0.0f;
    result.normalizationGain = rms > 1.0e-5f ? juce::jlimit(0.25f, 3.0f, 0.14f / rms) : 1.0f;
    return result;
}

float OpenMicNightAudioProcessor::readLooped(const StereoAsset& asset, int channel, double position) noexcept
{
    const auto& samples = asset.channels[static_cast<std::size_t>(juce::jlimit(0, 1, channel))];
    if (samples.empty()) return 0.0f;
    const auto size = samples.size();
    auto wrapped = std::fmod(position, static_cast<double>(size));
    if (wrapped < 0.0) wrapped += static_cast<double>(size);
    const auto sampleAt = [&samples, size](double at)
    {
        auto p = std::fmod(at, static_cast<double>(size));
        if (p < 0.0) p += static_cast<double>(size);
        const auto a = static_cast<std::size_t>(p) % size;
        const auto b = (a + 1u) % size;
        const auto fraction = static_cast<float>(p - static_cast<double>(a));
        return samples[a] + (samples[b] - samples[a]) * fraction;
    };
    const auto fadeSamples = static_cast<double>(std::min<std::size_t>(2048u, size / 8u));
    if (fadeSamples > 1.0 && wrapped >= static_cast<double>(size) - fadeSamples)
    {
        const auto alpha = static_cast<float>((wrapped - (static_cast<double>(size) - fadeSamples)) / fadeSamples);
        return (sampleAt(wrapped) * (1.0f - alpha) + sampleAt(wrapped - (static_cast<double>(size) - fadeSamples)) * alpha) * asset.normalizationGain;
    }
    return sampleAt(wrapped) * asset.normalizationGain;
}

void OpenMicNightAudioProcessor::updateAudienceEnergy(const juce::AudioBuffer<float>& buffer, int channels, int samples) noexcept
{
    double energy = 0.0;
    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < samples; ++i)
        {
            const auto sample = buffer.getSample(channel, i);
            energy += static_cast<double>(sample) * sample;
        }
    const auto rms = static_cast<float>(std::sqrt(energy / static_cast<double>(juce::jmax(1, channels * samples))));
    const auto sensitivity = juce::jlimit(0.0f, 1.0f, value("crowdSensitivity"));
    const auto floor = 0.006f + (1.0f - sensitivity) * 0.075f;
    const auto normalized = juce::jlimit(0.0f, 1.0f, (rms - floor) / juce::jmax(0.04f, 0.32f - floor));
    const auto seconds = normalized > inputEnergy ? 0.035 : 0.42;
    const auto coefficient = 1.0f - static_cast<float>(std::exp(-static_cast<double>(samples) / (juce::jmax(1.0, getSampleRate()) * seconds)));
    inputEnergy += (normalized - inputEnergy) * coefficient;
    inputEnergyMeter.store(inputEnergy, std::memory_order_relaxed);

    reactionCooldownRemaining = juce::jmax(0, reactionCooldownRemaining - samples);
    if (reactionDelayRemaining > 0)
    {
        reactionDelayRemaining -= samples;
        if (reactionDelayRemaining <= 0) reactiveReactionReady = true;
    }
    const auto audienceAmount = juce::jlimit(0.0f, 1.0f, value("crowdLevel"));
    const auto reactive = static_cast<int>(value("crowdBehavior")) == 0 && value("crowdSync") < .5f && audienceAmount > .001f;
    if (!reactive) { performancePhraseArmed = false; inputEnergyPeak = 0.0f; return; }

    const auto trigger = 0.20f + (1.0f - sensitivity) * 0.42f;
    if (inputEnergy >= trigger)
    {
        performancePhraseArmed = true;
        inputEnergyPeak = juce::jmax(inputEnergyPeak, inputEnergy);
    }
    else if (performancePhraseArmed && inputEnergy <= trigger * 0.42f && reactionCooldownRemaining <= 0 && reactionDelayRemaining <= 0 && !reactiveReactionReady)
    {
        audienceRandom ^= audienceRandom << 13u; audienceRandom ^= audienceRandom >> 17u; audienceRandom ^= audienceRandom << 5u;
        const auto random = static_cast<float>(static_cast<double>(audienceRandom) / 4294967295.0);
        reactionDelayRemaining = static_cast<int>(getSampleRate() * (0.11 + random * 0.46));
        pendingReactionStrength = juce::jlimit(0.02f, 1.0f, value("crowdStrength") * std::sqrt(audienceAmount) * (0.42f + inputEnergyPeak * 0.72f));
        pendingReactionType = inputEnergyPeak > (0.58f - value("crowdMood") * 0.14f) ? 0 : 1;
        pendingReactionDurationMs = 1200.0f + inputEnergyPeak * 2300.0f + random * 700.0f;
        reactionCooldownRemaining = static_cast<int>(getSampleRate() * value("crowdCooldownMs") * .001f);
        performancePhraseArmed = false;
        inputEnergyPeak = 0.0f;
    }
}

void OpenMicNightAudioProcessor::renderAudience(int offset, int count, int outputChannels) noexcept
{
    const auto ambientLevel = juce::jlimit(0.0f, 1.0f, value("crowdLevel"));
    const auto requestedBed = juce::jlimit(0, 3, static_cast<int>(value("crowdBed")));
    if (requestedBed != activeBedIndex)
    {
        previousBedIndex = activeBedIndex;
        activeBedIndex = requestedBed;
        bedCrossfade = 0.0f;
    }
    const auto behavior = juce::jlimit(0, 2, static_cast<int>(value("crowdBehavior")));
    const auto mood = juce::jlimit(0.0f, 1.0f, value("crowdMood"));
    const auto response = juce::jlimit(0.0f, 1.0f, value("crowdResponse"));
    const auto reactiveMotion = behavior == 0 ? inputEnergy * response : 0.0f;
    const auto ambientMotion = juce::jlimit(0.30f, 1.35f, 0.72f + mood * 0.18f + reactiveMotion * (0.28f + mood * 0.24f));
    audienceResponseMeter.store(juce::jlimit(0.0f, 1.0f, reactiveMotion * std::sqrt(ambientLevel) + (crowdAssetEventRemaining > 0 ? activeCrowdEventStrength : 0.0f)), std::memory_order_relaxed);
    const auto eventAttack = juce::jmax(1.0, getSampleRate() * 0.012);
    const auto eventRelease = juce::jmax(1.0, getSampleRate() * 0.080);
    const auto bedFadeStep = static_cast<float>(1.0 / juce::jmax(1.0, getSampleRate() * 0.24));
    for (int i = 0; i < count; ++i)
    {
        const auto eventActive = crowdAssetEventRemaining > 0;
        const auto eventProgressSamples = crowdAssetEventTotal - crowdAssetEventRemaining;
        const auto eventEnvelope = eventActive
            ? static_cast<float>(juce::jmin(1.0, eventProgressSamples / eventAttack)
                               * juce::jmin(1.0, crowdAssetEventRemaining / eventRelease))
            : 0.0f;
        const auto& eventAsset = activeCrowdEventType == 0 ? reactionAssets[1]
                               : activeCrowdEventType == 1 ? reactionAssets[0]
                               : crowdBeds[static_cast<std::size_t>(activeBedIndex)];
        for (int channel = 0; channel < outputChannels; ++channel)
        {
            const auto current = readLooped(crowdBeds[static_cast<std::size_t>(activeBedIndex)], channel, crowdBedPositions[static_cast<std::size_t>(activeBedIndex)]);
            const auto previous = readLooped(crowdBeds[static_cast<std::size_t>(previousBedIndex)], channel, crowdBedPositions[static_cast<std::size_t>(previousBedIndex)]);
            const auto ambient = (previous + (current - previous) * bedCrossfade) * ambientLevel * ambientMotion * 0.42f;
            const auto event = eventActive ? readLooped(eventAsset, channel, crowdEventPosition) * activeCrowdEventStrength * eventEnvelope * 0.58f : 0.0f;
            audienceScratch.setSample(channel, offset + i, ambient + event);
        }
        for (auto& position : crowdBedPositions) position += 1.0;
        bedCrossfade = juce::jmin(1.0f, bedCrossfade + bedFadeStep);
        if (eventActive)
        {
            crowdEventPosition += 1.0;
            --crowdAssetEventRemaining;
        }
    }
}

lost_audio::core::OpenMicParameters OpenMicNightAudioProcessor::makeCoreParameters() const noexcept
{
    using namespace lost_audio::core;
    OpenMicParameters p;
    p.mic = static_cast<OpenMicModel>(juce::roundToInt(value("micModel")));
    p.venue = static_cast<OpenMicVenue>(juce::roundToInt(value("venueModel")));
    p.pa = static_cast<OpenMicPA>(juce::roundToInt(value("paModel")));
    if (value("macroLink") > 0.5f)
    {
        const auto mapped = mapOpenMicMacros(p.mic, p.venue, p.pa, value("hotMic"), value("wall"), value("room"));
        p.proximity = mapped.proximity; p.micDrive = mapped.micDrive; p.paDrive = mapped.paDrive;
        p.monitorLevel = mapped.monitorLevel; p.stageBleed = mapped.stageBleed; p.roomAmount = mapped.roomAmount;
        p.wallAbsorption = mapped.wallAbsorption; p.electricalNoise = mapped.electricalNoise;
    }
    else
    {
        p.proximity = value("proximity"); p.micDrive = value("micDrive"); p.paDrive = value("paDrive");
        p.monitorLevel = value("monitorLevel"); p.stageBleed = value("stageBleed"); p.roomAmount = value("room");
        p.wallAbsorption = value("wallAbsorption"); p.electricalNoise = value("electricalNoise");
    }
    const auto armed=value("feedbackArm") > 0.5f;const auto conducted=value("feedbackConducted")>.5f||value("feedbackSync")>.5f;
    p.feedbackArmed = armed && (!conducted || feedbackEventRemaining > 0);
    p.feedbackAmount = value("feedbackAmount"); p.feedbackFrequency = value("fbFreq");
    p.feedbackQ = value("ringQ"); p.feedbackDelayMs = value("fbDelayMs"); p.feedbackTone = value("fbTone");
    p.feedbackBuildMs = value("feedbackBuildMs"); p.feedbackReleaseMs = value("feedbackReleaseMs");
    p.crowdLevel = value("crowdLevel"); p.crowdMood = value("crowdMood"); p.stereoWidth = value("stereoWidth");
    p.venueBedEnabled = value("venueBedEnable") > 0.5f; p.venueBedLevel = value("venueBedLevel");
    p.inputGain = value("inputGain"); p.mix = value("mix"); p.limiterAmount = value("limit");
    p.ceiling = value("ceiling"); p.outputGain = value("outGain");
    return p;
}

void OpenMicNightAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    core.prepare(sampleRate, static_cast<std::size_t>(juce::jmax(1, getTotalNumOutputChannels())));
    crowdBeds[0] = decodeAsset(BinaryData::ambientquiet_mp3, static_cast<std::size_t>(BinaryData::ambientquiet_mp3Size), sampleRate);
    crowdBeds[1] = decodeAsset(BinaryData::ambientbanter_mp3, static_cast<std::size_t>(BinaryData::ambientbanter_mp3Size), sampleRate);
    crowdBeds[2] = decodeAsset(BinaryData::ambientbusy_mp3, static_cast<std::size_t>(BinaryData::ambientbusy_mp3Size), sampleRate);
    crowdBeds[3] = decodeAsset(BinaryData::ambientrowdy_mp3, static_cast<std::size_t>(BinaryData::ambientrowdy_mp3Size), sampleRate);
    reactionAssets[0] = decodeAsset(BinaryData::applause_mp3, static_cast<std::size_t>(BinaryData::applause_mp3Size), sampleRate);
    reactionAssets[1] = decodeAsset(BinaryData::reactioncheer_mp3, static_cast<std::size_t>(BinaryData::reactioncheer_mp3Size), sampleRate);
    audienceScratch.setSize(2, juce::jmax(65536, samplesPerBlock), false, true, false);
    audienceScratch.clear();
    for (std::size_t i = 0; i < crowdBeds.size(); ++i) crowdBedPositions[i] = crowdBeds[i].channels[0].empty() ? 0.0 : static_cast<double>(crowdBeds[i].channels[0].size()) * (0.13 + 0.19 * static_cast<double>(i));
    crowdEventPosition=0;crowdAssetEventRemaining=0;crowdAssetEventTotal=1;activeCrowdEventType=0;activeCrowdEventStrength=0;
    activeBedIndex=previousBedIndex=juce::jlimit(0,3,static_cast<int>(value("crowdBed")));bedCrossfade=1;inputEnergy=inputEnergyPeak=0;performancePhraseArmed=false;reactionDelayRemaining=reactionCooldownRemaining=0;reactiveReactionReady=false;pendingReactionStrength=0;pendingReactionType=1;pendingReactionDurationMs=1600;audienceRandom=0x41554449u;inputEnergyMeter.store(0);audienceResponseMeter.store(0);
    feedbackEventRemaining=0;feedbackEventTotal=1;currentBpm=120.0;pendingFeedback.store(false);pendingCrowd.store(false);for(auto&point:trace)point.store(0);
}

bool OpenMicNightAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    return input == layouts.getMainOutputChannelSet()
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void OpenMicNightAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
    const auto channels = juce::jmin(2, buffer.getNumChannels());
    const auto samples=buffer.getNumSamples();if(channels<=0||samples<=0||getSampleRate()<=0)return;
    if(samples>audienceScratch.getNumSamples())return;
    audienceScratch.clear(0,samples);
    updateAudienceEnergy(buffer, channels, samples);
    bool playing=false,hasPpq=false;double ppq=0;if(auto*head=getPlayHead())if(const auto position=head->getPosition()){playing=position->getIsPlaying();if(const auto bpm=position->getBpm())currentBpm=*bpm;if(const auto hostPpq=position->getPpqPosition()){ppq=*hostPpq;hasPpq=true;}}
    lost_audio::core::TempoEventSchedule crowdSchedule,feedbackSchedule;if(playing&&hasPpq&&value("crowdSync")>.5f)crowdSchedule=lost_audio::core::tempoEventsInBlock(ppq,currentBpm,(int)value("crowdDivision"),getSampleRate(),samples);if(playing&&hasPpq&&value("feedbackSync")>.5f)feedbackSchedule=lost_audio::core::tempoEventsInBlock(ppq,currentBpm,(int)value("feedbackDivision"),getSampleRate(),samples);
    std::vector<int> boundaries{0,samples};for(std::size_t i=0;i<crowdSchedule.size;++i)boundaries.push_back(crowdSchedule.events[i].sampleOffset);for(std::size_t i=0;i<feedbackSchedule.size;++i)boundaries.push_back(feedbackSchedule.events[i].sampleOffset);std::sort(boundaries.begin(),boundaries.end());boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());
    auto fireCrowd=pendingCrowd.exchange(false),fireFeedback=pendingFeedback.exchange(false);auto fireReactive=!fireCrowd&&reactiveReactionReady;const auto crowdMs=value("crowdLengthSync")>.5f?lost_audio::core::tempoDivisionMilliseconds(currentBpm,(int)value("crowdLengthDivision")):value("crowdDurationMs");const auto feedbackMs=value("feedbackLengthSync")>.5f?lost_audio::core::tempoDivisionMilliseconds(currentBpm,(int)value("feedbackLengthDivision")):value("feedbackDurationMs");
    for(std::size_t segment=0;segment+1<boundaries.size();++segment){const auto offset=boundaries[segment],count=boundaries[segment+1]-offset;for(std::size_t i=0;i<crowdSchedule.size;++i)if(crowdSchedule.events[i].sampleOffset==offset&&lost_audio::core::tempoEventDecision(crowdSchedule.events[i].stepIndex,value("crowdProbability"),0x43524f57u))fireCrowd=true;for(std::size_t i=0;i<feedbackSchedule.size;++i)if(feedbackSchedule.events[i].sampleOffset==offset&&lost_audio::core::tempoEventDecision(feedbackSchedule.events[i].stepIndex,value("feedbackProbability"),0x484f574cu))fireFeedback=true;
        if((fireCrowd||fireReactive)&&!core.crowdEventActive()){activeCrowdEventType=fireReactive?pendingReactionType:juce::jlimit(0,3,(int)value("crowdEventType"));activeCrowdEventStrength=fireReactive?pendingReactionStrength:value("crowdStrength");const auto eventMs=fireReactive?pendingReactionDurationMs:crowdMs;crowdAssetEventTotal=crowdAssetEventRemaining=std::max(1,(int)std::lround(eventMs*.001*getSampleRate()));crowdEventPosition=0.0;core.triggerCrowdEvent((lost_audio::core::OpenMicCrowdEvent)activeCrowdEventType,activeCrowdEventStrength,crowdAssetEventTotal);if(fireReactive){reactiveReactionReady=false;fireReactive=false;}fireCrowd=false;}if(fireFeedback&&feedbackEventRemaining<=0&&value("feedbackArm")>.5f){feedbackEventTotal=feedbackEventRemaining=std::max(1,(int)std::lround(feedbackMs*.001*getSampleRate()));fireFeedback=false;}renderAudience(offset,count,channels);std::array<float*,2>pointers{buffer.getWritePointer(0,offset),channels>1?buffer.getWritePointer(1,offset):nullptr};std::array<const float*,2>audience{audienceScratch.getReadPointer(0,offset),channels>1?audienceScratch.getReadPointer(1,offset):nullptr};core.process(pointers.data(),(std::size_t)channels,(std::size_t)count,makeCoreParameters(),audience.data());feedbackEventRemaining=std::max(0,feedbackEventRemaining-count);}
    for (auto channel = 0; channel < channels; ++channel)
    {
        inputMeter[static_cast<std::size_t>(channel)].store(core.inputPeak(static_cast<std::size_t>(channel)));
        outputMeter[static_cast<std::size_t>(channel)].store(core.outputPeak(static_cast<std::size_t>(channel)));
    }
    if(channels==1){inputMeter[1].store(inputMeter[0].load());outputMeter[1].store(outputMeter[0].load());}for(std::size_t i=0;i<trace.size();++i){const auto at=juce::jlimit(0,samples-1,(int)std::lround((double)i*(samples-1)/(trace.size()-1)));trace[i].store(buffer.getSample(0,at));}
    feedbackMeter.store(core.feedbackActivity());crowdMeter.store(core.crowdActivity());roomMeter.store(core.roomActivity());limiterMeter.store(core.limiterActivity());safetyMeter.store(core.safetyEngaged());crowdEventState.store(core.crowdEventActive());crowdProgressMeter.store(core.crowdEventProgress());feedbackEventState.store(feedbackEventRemaining>0);feedbackProgressMeter.store(feedbackEventTotal>0?(float)feedbackEventRemaining/(float)feedbackEventTotal:0);
}

juce::AudioProcessorEditor* OpenMicNightAudioProcessor::createEditor() { return new OpenMicNightAudioProcessorEditor(*this); }

void OpenMicNightAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto stateCopy = parameters.copyState();
    stateCopy.setProperty("engineId", "open-mic-night", nullptr);
    stateCopy.setProperty("schemaVersion", 4, nullptr);
    if (auto xml = stateCopy.createXml()) copyXmlToBinary(*xml, destination);
}

void OpenMicNightAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType()))
        {
            auto restored = juce::ValueTree::fromXml(*xml);
            if (!restored.hasProperty("schemaVersion")) restored.setProperty("schemaVersion", 1, nullptr);
            parameters.replaceState(restored);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OpenMicNightAudioProcessor(); }
