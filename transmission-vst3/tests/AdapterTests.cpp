#include "../src/PluginProcessor.h"

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
    TransmissionEngineAudioProcessor processor;
    auto& state = processor.getAPVTS();
    if (processor.legacyMacrosActive() || state.getParameter("dropoutTempoSync") == nullptr
        || state.getParameter("carrierTempoSync") == nullptr || state.getParameter("tuningProbability") == nullptr)
    {
        std::cerr << "Transmission V4 canonical/performer contract is missing\n";
        return 1;
    }

    set(state, "bandwidth", 0.22f); set(state, "drive", 0.55f);
    set(state, "badConnection", 0.35f); set(state, "noiseProfile", 0.35f); set(state, "macroLink", 1.0f);
    const auto target = lost_audio::core::mapTransmissionMacros(0.22f, 0.55f, 0.35f, 0.35f);
    processor.materialiseLegacyMacros();
    if (processor.legacyMacrosActive() || !near(state.getRawParameterValue("hpHz")->load(), target.highPassHz, 1.1f)
        || !near(state.getRawParameterValue("comp")->load(), target.compression)
        || !near(state.getRawParameterValue("dropRate")->load(), target.dropoutRate)
        || !near(state.getRawParameterValue("hiss")->load(), target.hiss))
    {
        std::cerr << "Transmission legacy profile did not materialise to canonical state\n";
        return 1;
    }

    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    set(state, "macroLink", 0.0f); set(state, "dropRate", 0.0f); set(state, "crackle", 0.0f);
    set(state, "noiseProfile", 0.0f); set(state, "hiss", 0.0f); set(state, "dropoutDurationMs", 80.0f);
    set(state, "dropoutStrength", 0.8f); set(state, "tuningEnable", 1.0f); set(state, "tuningSource", 0.0f);
    juce::AudioBuffer<float> buffer(2, 1024);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        buffer.setSample(0, sample, 0.2f);
        buffer.setSample(1, sample, -0.2f);
    }
    juce::MidiBuffer midi;
    processor.triggerDropout(); processor.triggerTuningSearch(); processor.processBlock(buffer, midi);
    if (!processor.dropoutIsActive() || processor.dropoutProgressMeter() <= 0.0f
        || !processor.tuningIsActive() || processor.tuningProgressMeter() <= 0.0f
        || buffer.getMagnitude(0, 0, buffer.getNumSamples()) > 0.951f)
    {
        std::cerr << "Transmission manual events, telemetry, or ceiling failed\n";
        return 1;
    }
    std::cout << "Transmission adapter passed: canonical migration, manual tuning/loss, telemetry, and safety ceiling\n";
}
