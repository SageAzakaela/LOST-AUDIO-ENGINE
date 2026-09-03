#include "../src/PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
void set(juce::AudioProcessorValueTreeState& state, const char* id, float value)
{
    auto* parameter = state.getParameter(id);
    if (parameter == nullptr) std::exit(2);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
bool near(float a, float b, float tolerance = 0.011f) { return std::abs(a - b) <= tolerance; }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    ConferenceEngineAudioProcessor processor;
    auto& state = processor.getAPVTS();
    if (processor.legacyMacrosActive() || state.getParameter("packetTempoSync") == nullptr
        || state.getParameter("robotTempoSync") == nullptr || state.getParameter("jitterTempoSync") == nullptr)
    {
        std::cerr << "Conference V3 canonical/performer contract is missing\n";
        return 1;
    }

    set(state, "mode", 3.0f); set(state, "bandwidth", 0.31f); set(state, "codec", 0.62f);
    set(state, "dropouts", 0.48f); set(state, "jitter", 0.43f); set(state, "robot", 0.35f);
    set(state, "noise", 0.22f); set(state, "macroLink", 1.0f);
    const auto target = lost_audio::core::mapConferenceMacros(lost_audio::core::ConferenceMode::cellular, 0.31f, 0.62f, 0.48f, 0.43f, 0.35f, 0.22f);
    processor.materialiseLegacyMacros();
    if (processor.legacyMacrosActive() || !near(state.getRawParameterValue("hpHz")->load(), target.highPassHz, 1.1f)
        || !near(state.getRawParameterValue("packetLoss")->load(), target.packetLoss)
        || !near(state.getRawParameterValue("jitterMs")->load(), target.jitterMs)
        || !near(state.getRawParameterValue("comfortNoise")->load(), target.comfortNoise))
    {
        std::cerr << "Conference legacy profile did not materialise to canonical state\n";
        return 1;
    }

    processor.setRateAndBufferSizeDetails(48000.0, 1024); processor.prepareToPlay(48000.0, 1024);
    if (processor.getLatencySamples() != 96)
    {
        std::cerr << "Conference adapter must report the 2 ms live latency\n";
        return 1;
    }
    set(state, "packetLoss", 0.0f); set(state, "robot", 0.0f); set(state, "packetDurationMs", 120.0f);
    set(state, "robotDurationMs", 240.0f); set(state, "packetDepth", 0.8f); set(state, "robotStrength", 0.85f);
    set(state, "ceiling", 0.5f); set(state, "outGain", 1.4f);
    juce::AudioBuffer<float> buffer(2, 1024);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto x = 0.72f * std::sin((float) sample * 0.05f);
        buffer.setSample(0, sample, x); buffer.setSample(1, sample, -x * 0.8f);
    }
    juce::MidiBuffer midi; processor.triggerPacketLoss(); processor.triggerRobot(); processor.processBlock(buffer, midi);
    const auto trace = processor.outputTrace();
    if (!processor.packetLost() || processor.packetProgress() <= 0.0f || !processor.robotActive() || processor.robotProgress() <= 0.0f
        || buffer.getMagnitude(0, 0, buffer.getNumSamples()) > 0.501f
        || std::all_of(trace.begin(), trace.end(), [] (float point) { return point <= 0.0f; }))
    {
        std::cerr << "Conference manual packet/robot events, telemetry, trace, or ceiling failed\n";
        return 1;
    }
    std::cout << "Conference adapter passed: canonical migration, manual packet/robot events, telemetry, and safety ceiling\n";
}
