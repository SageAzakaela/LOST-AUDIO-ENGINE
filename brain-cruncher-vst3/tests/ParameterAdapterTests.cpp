#include "../src/PluginProcessor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>

namespace
{
[[noreturn]] void fail(const char* message)
{
    std::cerr << "Brain Cruncher adapter failure: " << message << '\n';
    std::exit(1);
}

void set(juce::AudioProcessorValueTreeState& state, const char* id, float plainValue)
{
    auto* parameter = state.getParameter(id);
    if (parameter == nullptr) fail("missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

std::vector<float> render(std::initializer_list<std::pair<const char*, float>> changes, bool stereoDifference = false)
{
    BrainCruncherAudioProcessor processor;
    processor.setPlayConfigDetails(2, 2, 48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    auto& state = processor.getAPVTS();
    for (const auto& [id, value] : changes) set(state, id, value);

    juce::AudioBuffer<float> audio(2, 48000);
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        const auto value = 0.14f * std::sin(6.2831853f * 261.6256f * sample / 48000.0f)
                         + 0.04f * std::sin(6.2831853f * 1850.0f * sample / 48000.0f);
        audio.setSample(0, sample, value);
        audio.setSample(1, sample, value);
    }
    juce::MidiBuffer midi;
    for (int offset = 0; offset < audio.getNumSamples(); offset += 256)
    {
        const auto count = juce::jmin(256, audio.getNumSamples() - offset);
        juce::AudioBuffer<float> block(audio.getArrayOfWritePointers(), 2, offset, count);
        processor.processBlock(block, midi);
    }

    std::vector<float> result(static_cast<std::size_t>(audio.getNumSamples()));
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        result[static_cast<std::size_t>(sample)] = stereoDifference
            ? audio.getSample(0, sample) - audio.getSample(1, sample)
            : audio.getSample(0, sample);
    return result;
}

float difference(const std::vector<float>& a, const std::vector<float>& b)
{
    double total = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) total += std::abs(a[i] - b[i]);
    return static_cast<float>(total / static_cast<double>(a.size()));
}

float energy(const std::vector<float>& values)
{
    double total = 0.0;
    for (const auto value : values) total += std::abs(value);
    return static_cast<float>(total / static_cast<double>(values.size()));
}
}

int main()
{
    const auto dry = render({ { "mix", 0.0f } });
    const auto crunchLow = render({ { "mix", 1.0f }, { "crunch", 0.05f } });
    const auto crunchHigh = render({ { "mix", 1.0f }, { "crunch", 1.0f } });
    const auto bodyLow = render({ { "body", 0.0f } });
    const auto bodyHigh = render({ { "body", 1.0f } });
    const auto biteLow = render({ { "bite", 0.0f } });
    const auto biteHigh = render({ { "bite", 1.0f } });
    const auto motionOff = render({ { "motion", 0.0f }, { "width", 1.0f } }, true);
    const auto motionOn = render({ { "motion", 1.0f }, { "width", 1.0f } }, true);
    const auto narrow = render({ { "motion", 0.8f }, { "width", 0.0f } }, true);
    const auto wide = render({ { "motion", 0.8f }, { "width", 1.0f } }, true);
    const auto binauralOff = render({ { "motion", 0.0f }, { "binaural", 0.0f }, { "pan", .75f } }, true);
    const auto binauralOn = render({ { "motion", 0.0f }, { "binaural", 1.0f }, { "pan", .75f }, { "headSize", 1.0f } }, true);
    const auto driveLow = render({ { "drive", 0.0f } });
    const auto driveHigh = render({ { "drive", 1.0f } });
    const auto safe = render({ { "crunch", 1.0f }, { "drive", 1.0f }, { "outputGain", 12.0f }, { "ceiling", .7f } });

    if (difference(dry, crunchHigh) < .01f) fail("full-wet engine is not distinct from dry");
    if (difference(crunchLow, crunchHigh) < .004f) fail("crunch is inaudible");
    if (difference(bodyLow, bodyHigh) < .004f) fail("body is inaudible");
    if (difference(biteLow, biteHigh) < .004f) fail("bite is inaudible");
    if (energy(motionOn) <= energy(motionOff) + .0003f) fail("motion does not create stereo divergence");
    if (energy(wide) <= energy(narrow) * 1.2f) fail("width does not change stereo side energy");
    if (energy(binauralOn) <= energy(binauralOff) + .0003f) fail("binaural pan does not create dual-signal stereo divergence");
    if (difference(driveLow, driveHigh) < .003f) fail("drive is inaudible");
    for (const auto value : safe)
        if (!std::isfinite(value) || std::abs(value) > .701f) fail("output escaped the selected ceiling");

    std::cout << "Brain Cruncher adapter passed: wet character, crunch, body, bite, motion, width, binaural pan, drive, bounded output\n";
}
