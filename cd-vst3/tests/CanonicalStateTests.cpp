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

float get(const juce::AudioProcessorValueTreeState& state, const char* id)
{
    const auto* value = state.getRawParameterValue(id);
    if (value == nullptr) std::exit(2);
    return value->load();
}

bool near(float actual, float expected, float tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

float renderDefaultMaxImpulse()
{
    constexpr int sampleRate = 44100, blockSize = 512, totalSamples = sampleRate * 5, warmup = sampleRate;
    CDEngineAudioProcessor processor;
    processor.setRateAndBufferSizeDetails(sampleRate, blockSize);
    processor.prepareToPlay(sampleRate, blockSize);
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;
    float previous2 = 0.0f, previous1 = 0.0f, maxImpulse = 0.0f;
    for (int offset = 0; offset < totalSamples; offset += blockSize)
    {
        const auto count = std::min(blockSize, totalSamples - offset);
        buffer.setSize(2, count, false, false, true);
        for (int index = 0; index < count; ++index)
        {
            const auto sample = .14f * std::sin(juce::MathConstants<float>::twoPi * 997.0f * (float) (offset + index) / (float) sampleRate);
            buffer.setSample(0, index, sample);
            buffer.setSample(1, index, sample);
        }
        processor.processBlock(buffer, midi);
        for (int index = 0; index < count; ++index)
        {
            const auto current = buffer.getSample(0, index);
            if (offset + index >= warmup) maxImpulse = std::max(maxImpulse, std::abs(current - 2.0f * previous1 + previous2));
            previous2 = previous1;
            previous1 = current;
        }
    }
    return maxImpulse;
}

float renderPresetTransitionMaxImpulse(int isolation = 0)
{
    constexpr int sampleRate = 44100, blockSize = 512, totalSamples = sampleRate * 5, warmup = sampleRate;
    CDEngineAudioProcessor processor;
    processor.setRateAndBufferSizeDetails(sampleRate, blockSize);
    processor.prepareToPlay(sampleRate, blockSize);
    auto& state = processor.getAPVTS();
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;
    float previous2 = 0.0f, previous1 = 0.0f, maxImpulse = 0.0f;
    bool changed = false;
    int maxIndex = -1, changeIndex = -1;
    for (int offset = 0; offset < totalSamples; offset += blockSize)
    {
        if (!changed && offset >= sampleRate * 2)
        {
            set(state, "clarity", .34f); set(state, "damage", .72f); set(state, "tracking", .66f);
            set(state, "jitterMacro", .28f); set(state, "mode", 3.0f); set(state, "damageShape", 0.0f);
            set(state, "macroLink", 1.0f); processor.materialiseLegacyMacros(); changed = true; changeIndex = offset;
            if (isolation == 1) set(state, "trackingRate", 0.0f);
            if (isolation == 2) { set(state, "errorRate", 0.0f); set(state, "scratchAmt", 0.0f); }
            if (isolation == 3) set(state, "servoNoise", 0.0f);
        }
        const auto count = std::min(blockSize, totalSamples - offset);
        buffer.setSize(2, count, false, false, true);
        for (int index = 0; index < count; ++index)
        {
            const auto sample = .14f * std::sin(juce::MathConstants<float>::twoPi * 997.0f * (float) (offset + index) / (float) sampleRate);
            buffer.setSample(0, index, sample); buffer.setSample(1, index, sample);
        }
        processor.processBlock(buffer, midi);
        for (int index = 0; index < count; ++index)
        {
            const auto current = buffer.getSample(0, index);
            if (offset + index >= warmup)
            {
                const auto impulse = std::abs(current - 2.0f * previous1 + previous2);
                if (impulse > maxImpulse) { maxImpulse = impulse; maxIndex = offset + index; }
            }
            previous2 = previous1; previous1 = current;
        }
    }
    std::cout << "CD transition peak offset: " << (maxIndex - changeIndex) << " samples\n";
    return maxImpulse;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    CDEngineAudioProcessor processor;
    auto& state = processor.getAPVTS();

    const auto defaults = lost_audio::core::mapCDMacros(0.65f, 0.25f, 0.22f, 0.18f);
    if (processor.legacyMacrosActive()
        || !near(get(state, "skipSliceDivision"), 5.0f, 0.01f)
        || !near(get(state, "skipLengthDivision"), 3.0f, 0.01f)
        || !near(get(state, "skipRetrigger"), 0.0f, 0.01f)
        || !near(get(state, "syncDivision"), 2.0f, 0.01f)
        || !near(get(state, "syncProbability"), .32f, 0.0011f)
        || !near(get(state, "errorRate"), defaults.errorRate, 0.0011f)
        || !near(get(state, "repeatMs"), defaults.repeatMs, 1.1f)
        || !near(get(state, "trackingRate"), defaults.trackingRate, 0.0011f)
        || !near(get(state, "jitterMs"), defaults.jitterMs, 0.0011f))
    {
        std::cerr << "New CD state is not canonical\n";
        return 1;
    }

    set(state, "clarity", 0.28f);
    set(state, "damage", 0.72f);
    set(state, "tracking", 0.66f);
    set(state, "jitterMacro", 0.28f);
    set(state, "outGain", 0.98f);
    set(state, "macroLink", 1.0f);
    const auto expected = lost_audio::core::mapCDMacros(0.28f, 0.72f, 0.66f, 0.28f);
    processor.materialiseLegacyMacros();

    if (processor.legacyMacrosActive()
        || !near(get(state, "errorRate"), expected.errorRate, 0.0011f)
        || !near(get(state, "burstMs"), expected.burstMs, 1.1f)
        || !near(get(state, "repeatMs"), expected.repeatMs, 1.1f)
        || !near(get(state, "scratchAmt"), expected.scratchAmount, 0.0011f)
        || !near(get(state, "correction"), expected.correction, 0.0011f)
        || !near(get(state, "trackingMs"), expected.trackingMs, 1.1f)
        || !near(get(state, "servoNoise"), expected.servoNoise, 0.0011f)
        || !near(get(state, "outGain"), expected.outputGain, 0.011f))
    {
        std::cerr << "Legacy CD macro state did not materialise faithfully\n";
        return 1;
    }

    const auto defaultImpulse = renderDefaultMaxImpulse();
    std::cout << "CD fresh-instance max impulse at 44.1k/512: " << defaultImpulse << "\n";
    if (defaultImpulse > .08f)
    {
        std::cerr << "CD fresh instance creates click-sized discontinuities\n";
        return 1;
    }
    const auto transitionImpulse = renderPresetTransitionMaxImpulse();
    std::cout << "CD Scratched Hook transition max impulse: " << transitionImpulse << "\n";
    std::cout << "  without tracking: " << renderPresetTransitionMaxImpulse(1) << "\n";
    std::cout << "  without damage: " << renderPresetTransitionMaxImpulse(2) << "\n";
    std::cout << "  without servo: " << renderPresetTransitionMaxImpulse(3) << "\n";
    if (transitionImpulse > .08f)
    {
        std::cerr << "CD preset transition creates a click-sized discontinuity\n";
        return 1;
    }

    std::cout << "CD canonical defaults and legacy macro materialisation passed\n";
}
