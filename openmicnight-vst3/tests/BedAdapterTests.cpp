#include "../src/PluginProcessor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
[[noreturn]] void fail(const char* message)
{
    std::cerr << "Open Mic adapter failure: " << message << '\n';
    std::exit(1);
}

void set(juce::AudioProcessorValueTreeState& state, const char* id, float value)
{
    auto* parameter = state.getParameter(id);
    if (!parameter) fail("missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float render(bool venueBed, float crowd)
{
    OpenMicNightAudioProcessor processor;
    processor.setPlayConfigDetails(2, 2, 48000, 256);
    processor.prepareToPlay(48000, 256);
    auto& state = processor.state();
    set(state, "macroLink", 0); set(state, "feedbackArm", 0); set(state, "crowdLevel", crowd);
    set(state, "crowdBehavior", 1); set(state, "crowdMood", .65f); set(state, "electricalNoise", .04f);
    set(state, "stageBleed", 0); set(state, "room", 0); set(state, "venueBedLevel", .72f);
    set(state, "venueBedEnable", venueBed ? 1.0f : 0.0f);
    juce::AudioBuffer<float> audio(2, 48000); audio.clear(); juce::MidiBuffer midi;
    for (int offset = 0; offset < audio.getNumSamples(); offset += 256)
    {
        const auto count = juce::jmin(256, audio.getNumSamples() - offset);
        juce::AudioBuffer<float> block(audio.getArrayOfWritePointers(), 2, offset, count);
        processor.processBlock(block, midi);
    }
    return audio.getMagnitude(0, 0, audio.getNumSamples());
}

std::vector<float> renderBed(int bed)
{
    OpenMicNightAudioProcessor processor;
    processor.setPlayConfigDetails(2, 2, 48000, 256);
    processor.prepareToPlay(48000, 256);
    auto& state = processor.state();
    set(state, "macroLink", 0); set(state, "feedbackArm", 0); set(state, "crowdBed", static_cast<float>(bed));
    set(state, "crowdBehavior", 1); set(state, "crowdLevel", .7f); set(state, "crowdMood", .45f);
    set(state, "electricalNoise", 0); set(state, "stageBleed", 0); set(state, "room", 0); set(state, "venueBedEnable", 0);
    juce::AudioBuffer<float> audio(2, 16384); audio.clear(); juce::MidiBuffer midi;
    for (int offset = 0; offset < audio.getNumSamples(); offset += 256)
    {
        const auto count = juce::jmin(256, audio.getNumSamples() - offset);
        juce::AudioBuffer<float> block(audio.getArrayOfWritePointers(), 2, offset, count);
        processor.processBlock(block, midi);
    }
    std::vector<float> result(static_cast<std::size_t>(audio.getNumSamples()));
    std::copy(audio.getReadPointer(0), audio.getReadPointer(0) + audio.getNumSamples(), result.begin());
    return result;
}

bool audienceReactedAfterPhrase(int behavior, float audienceLevel = .7f)
{
    OpenMicNightAudioProcessor processor;
    processor.setPlayConfigDetails(2, 2, 48000, 256);
    processor.prepareToPlay(48000, 256);
    auto& state = processor.state();
    set(state, "macroLink", 0); set(state, "feedbackArm", 0); set(state, "crowdBehavior", static_cast<float>(behavior));
    set(state, "crowdSync", 0); set(state, "crowdLevel", audienceLevel); set(state, "crowdSensitivity", 1);
    set(state, "crowdStrength", 1); set(state, "crowdCooldownMs", 1000); set(state, "electricalNoise", 0);
    set(state, "venueBedEnable", 0); set(state, "room", 0);
    juce::AudioBuffer<float> block(2, 256); juce::MidiBuffer midi;
    for (int offset = 0; offset < 192000; offset += 256)
    {
        for (int i = 0; i < block.getNumSamples(); ++i)
        {
            const auto absolute = offset + i;
            const auto sample = absolute < 48000 ? .48f * std::sin(static_cast<float>(absolute) * .071f) : 0.0f;
            block.setSample(0, i, sample); block.setSample(1, i, sample * .91f);
        }
        processor.processBlock(block, midi);
        if (offset > 48000 && processor.crowdEventActive()) return true;
    }
    return false;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    OpenMicNightAudioProcessor processor;
    processor.setPlayConfigDetails(2, 2, 48000, 256);
    processor.prepareToPlay(48000, 256);
    if (!processor.embeddedCrowdReady()) fail("embedded real crowd recordings failed to decode");
    auto& state = processor.state();
    if (processor.legacyMacrosActive() || !state.getParameter("crowdSync") || !state.getParameter("feedbackConducted")
        || !state.getParameter("crowdBed") || !state.getParameter("crowdBehavior") || !state.getParameter("crowdSensitivity"))
        fail("V4 contract missing");

    const auto initial = lost_audio::core::mapOpenMicMacros(lost_audio::core::OpenMicModel::dynamicHandheld,
        lost_audio::core::OpenMicVenue::cornerClub, lost_audio::core::OpenMicPA::compact, .55f, .65f, .55f);
    if (std::abs(state.getRawParameterValue("proximity")->load() - initial.proximity) > .011f
        || std::abs(state.getRawParameterValue("monitorLevel")->load() - initial.monitorLevel) > .011f
        || std::abs(state.getRawParameterValue("room")->load() - initial.roomAmount) > .011f)
        fail("canonical defaults drifted from legacy sound");

    set(state, "hotMic", .72f); set(state, "wall", .28f); set(state, "room", .66f);
    set(state, "micModel", 2); set(state, "venueModel", 1); set(state, "paModel", 3); set(state, "macroLink", 1);
    const auto target = lost_audio::core::mapOpenMicMacros(lost_audio::core::OpenMicModel::karaoke,
        lost_audio::core::OpenMicVenue::diveBar, lost_audio::core::OpenMicPA::tiredCombo, .72f, .28f, .66f);
    processor.materialiseLegacyMacros();
    if (processor.legacyMacrosActive() || std::abs(state.getRawParameterValue("micDrive")->load() - target.micDrive) > .011f
        || std::abs(state.getRawParameterValue("stageBleed")->load() - target.stageBleed) > .011f)
        fail("legacy migration failed");

    const auto off = render(false, 0), crowd = render(false, .65f), bed = render(true, 0);
    if (off > 1e-7f) fail("disabled ambience leaked signal");
    if (crowd < 1e-4f) fail("audience remained inaudible without venue bed");
    if (bed < 1e-4f) fail("venue bed is inaudible");

    std::array<std::vector<float>, 4> beds;
    for (int i = 0; i < 4; ++i) beds[static_cast<std::size_t>(i)] = renderBed(i);
    for (int a = 0; a < 4; ++a)
        for (int b = a + 1; b < 4; ++b)
        {
            double difference = 0.0;
            for (std::size_t i = 0; i < beds[static_cast<std::size_t>(a)].size(); ++i)
            {
                const auto delta = beds[static_cast<std::size_t>(a)][i] - beds[static_cast<std::size_t>(b)][i];
                difference += static_cast<double>(delta) * delta;
            }
            if (difference / beds[static_cast<std::size_t>(a)].size() < 1e-7) fail("audience bed choices are not sonically distinct");
        }

    if (!audienceReactedAfterPhrase(0)) fail("reactive audience ignored a high-energy phrase ending");
    if (audienceReactedAfterPhrase(0, 0.0f)) fail("zero audience generated an automatic reaction");
    if (audienceReactedAfterPhrase(1) || audienceReactedAfterPhrase(2)) fail("non-reactive audience generated an automatic event");

    processor.setRateAndBufferSizeDetails(48000, 256); processor.prepareToPlay(48000, 256);
    set(state, "feedbackArm", 1); set(state, "feedbackConducted", 1); set(state, "crowdStrength", .8f);
    set(state, "crowdDurationMs", 1000); set(state, "feedbackDurationMs", 800);
    processor.triggerCrowd(); processor.triggerFeedback();
    juce::AudioBuffer<float> audio(2, 1024);
    for (int i = 0; i < 1024; ++i) { const auto sample = .18f * std::sin(i * .06f); audio.setSample(0, i, sample); audio.setSample(1, i, -sample * .8f); }
    juce::MidiBuffer midi; processor.processBlock(audio, midi);
    if (!processor.crowdEventActive() || processor.crowdEventProgress() <= 0 || !processor.feedbackEventActive() || processor.feedbackEventProgress() <= 0)
        fail("conducted event telemetry failed");

    std::cout << "Open Mic adapter passed: selectable normalized beds, energy-following reactions, explicit steady/manual modes, legacy migration, and telemetry\n";
}
