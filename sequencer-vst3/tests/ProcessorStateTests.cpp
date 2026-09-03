#include "PluginProcessor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void setPlain(LostAudioSequencerProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.state().getParameter(id);
    require(parameter != nullptr, "A requested test parameter is missing.");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

std::vector<float> renderEngine(int engine, bool stepEnabled, float damage)
{
    LostAudioSequencerProcessor processor;
    setPlain(processor, "freeRun", 1.0f);
    setPlain(processor, "length", 1.0f);
    setPlain(processor, "mix", 1.0f);
    setPlain(processor, "safety", 1.0f);
    setPlain(processor, "step1Enabled", stepEnabled ? 1.0f : 0.0f);
    setPlain(processor, "step1Engine", static_cast<float>(engine));
    setPlain(processor, "step1Character", 0.72f);
    setPlain(processor, "step1Damage", damage);
    setPlain(processor, "step1Probability", 1.0f);
    setPlain(processor, "step1Mix", 0.90f);
    setPlain(processor, "step1Model", 0.55f);
    processor.prepareToPlay(48000.0, 512);

    constexpr int totalSamples = 24576;
    std::vector<float> output(static_cast<std::size_t>(totalSamples));
    juce::MidiBuffer midi;
    for (int offset = 0; offset < totalSamples; offset += 512)
    {
        const auto count = juce::jmin(512, totalSamples - offset);
        juce::AudioBuffer<float> block(2, count);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto position = offset + sample;
            const auto input = 0.18f * std::sin(juce::MathConstants<float>::twoPi * 237.0f * static_cast<float>(position) / 48000.0f)
                             + 0.07f * std::sin(juce::MathConstants<float>::twoPi * 911.0f * static_cast<float>(position) / 48000.0f);
            block.setSample(0, sample, input);
            block.setSample(1, sample, input * 0.91f);
        }
        processor.processBlock(block, midi);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto value = block.getSample(0, sample);
            require(std::isfinite(value) && std::abs(value) <= 1.001f, "A sequenced engine escaped finite safety bounds.");
            output[static_cast<std::size_t>(offset + sample)] = value;
        }
    }
    return output;
}

double meanDifference(const std::vector<float>& a, const std::vector<float>& b)
{
    require(a.size() == b.size(), "Render lengths must match.");
    double difference = 0.0;
    constexpr std::size_t latencyGuard = 7000;
    for (std::size_t index = latencyGuard; index < a.size(); ++index)
        difference += std::abs(static_cast<double>(a[index] - b[index]));
    return difference / static_cast<double>(a.size() - latencyGuard);
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    LostAudioSequencerProcessor processor;
    require(processor.state().getParameter("division") != nullptr, "The host-sync division must be exposed.");
    require(processor.state().getParameter("step16Damage") != nullptr, "All sixteen step surfaces must be routable.");
    require(processor.state().getParameter("step1Probability") != nullptr, "Step probability must be automatable.");

    processor.applyPreset(4);
    require(processor.state().getRawParameterValue("step1Enabled")->load() > .5f, "Failed Call must arm its first step.");
    require(juce::roundToInt(processor.state().getRawParameterValue("step1Engine")->load()) == 2,
            "Failed Call must begin with the Comms engine.");

    processor.applyPreset(9);
    require(processor.state().getRawParameterValue("step4Enabled")->load() > .5f,
            "Sparse Hook Mutator must arm its first musical event.");
    require(processor.state().getRawParameterValue("step12Enabled")->load() > .5f,
            "Sparse Hook Mutator must arm its second musical event.");
    require(processor.state().getRawParameterValue("step5Enabled")->load() < .5f,
            "Sparse Hook Mutator must remain sparse.");
    require(processor.state().getRawParameterValue("safety")->load() >= .89f,
            "Factory patterns must recall output protection.");

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    LostAudioSequencerProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    require(restored.state().getRawParameterValue("step4Enabled")->load() > .5f, "Pattern state must survive a project reload.");
    require(juce::roundToInt(restored.state().getRawParameterValue("step4Engine")->load()) == 3,
            "Effect assignments must survive a project reload.");

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());
        require(editor != nullptr, "The sequencer editor must survive the host creation lifecycle.");
        editor->setSize(960, 580);
        editor->setSize(1120, 650);
    }

    processor.prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> audio(2, 512);
    audio.clear();
    juce::MidiBuffer midi;
    processor.processBlock(audio, midi);
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            require(std::isfinite(audio.getSample(channel, sample)), "The stopped transport path must remain finite.");

    const auto rest = renderEngine(0, false, 0.2f);
    for (int engine = 0; engine < 10; ++engine)
    {
        const auto effected = renderEngine(engine, true, 0.68f);
        require(meanDifference(rest, effected) > 1.0e-4, "A sequenced device engine is not audibly routed.");
    }
    const auto lowDamage = renderEngine(0, true, 0.08f);
    const auto highDamage = renderEngine(0, true, 0.74f);
    require(meanDifference(lowDamage, highDamage) > 1.0e-4, "Step Damage is not influencing the sequenced DSP.");

    return 0;
}
