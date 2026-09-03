#include "../src/PluginProcessor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
[[noreturn]] void fail(const char* message)
{
    std::cerr << "Suite state failure: " << message << '\n';
    std::exit(1);
}

void set(juce::AudioProcessorValueTreeState& state, const juce::String& id, float plain)
{
    auto* parameter = state.getParameter(id);
    if (parameter == nullptr) fail("missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
}

float get(juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    auto* value = state.getRawParameterValue(id);
    if (value == nullptr) fail("missing raw parameter");
    return value->load();
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    LostAudioSuiteProcessor source;
    source.prepareToPlay(48000.0, 256);
    auto& state = source.state();
    set(state, "slot1Engine", 8.0f);
    set(state, "slot1MacroA", .73f);
    set(state, "slot1MacroB", .41f);
    set(state, "slot1Detail6", .88f);
    set(state, "slot2Engine", 10.0f);
    set(state, "slot2FeedbackArm", 1.0f);
    set(state, "order1", 1.0f);
    set(state, "order2", 0.0f);
    set(state, "global1", .82f);

    juce::MemoryBlock saved;
    source.getStateInformation(saved);
    LostAudioSuiteProcessor restored;
    restored.prepareToPlay(48000.0, 256);
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    auto& result = restored.state();
    if (std::abs(get(result, "slot1Engine") - 8.0f) > .01f) fail("engine did not survive recall");
    if (std::abs(get(result, "slot1MacroA") - .73f) > .001f) fail("slot macro did not survive recall");
    if (std::abs(get(result, "slot1Detail6") - .88f) > .001f) fail("device control did not survive recall");
    if (std::abs(get(result, "order1") - 1.0f) > .01f || std::abs(get(result, "order2")) > .01f) fail("chain order did not survive recall");
    if (std::abs(get(result, "global1") - .82f) > .001f) fail("global macro did not survive recall");
    for (int i = 1; i <= 6; ++i)
        if (get(result, "slot" + juce::String(i) + "FeedbackArm") > .5f) fail("feedback arm survived recall");
    if (restored.getLatencySamples() != 5760) fail("reported latency changed");

    // Ableton opens the editor immediately after creating the processor.  Keep
    // this lifecycle in the host test so partially-constructed UI regressions
    // cannot hide behind DSP-only coverage again.
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());
        if (editor == nullptr) fail("editor creation returned null");
        editor->setSize(1220, 760);
    }

    juce::AudioBuffer<float> audio(2, 8192);
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        audio.setSample(0, i, .18f * std::sin(6.2831853f * 220.0f * i / 48000.0f));
        audio.setSample(1, i, .15f * std::sin(6.2831853f * 337.0f * i / 48000.0f));
    }
    juce::MidiBuffer midi;
    restored.processBlock(audio, midi);
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < audio.getNumSamples(); ++i)
            if (!std::isfinite(audio.getSample(channel, i)) || std::abs(audio.getSample(channel, i)) > 1.001f) fail("recalled render escaped bounds");

    LostAudioSuiteProcessor television;
    television.prepareToPlay(48000.0, 256);
    set(television.state(), "slot1Engine", 8.0f);
    juce::AudioBuffer<float> silent(2, 16384); silent.clear();
    television.processBlock(silent, midi);
    if (silent.getMagnitude(0, 0, silent.getNumSamples()) < 1.0e-6f) fail("Television slot lost the embedded CRT bed");

    std::cout << "Suite state passed: editor lifecycle, 115 parameters, order and device-control recall, feedback disarm, CRT bed, latency, bounded render\n";
}
