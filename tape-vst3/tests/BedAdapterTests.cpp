#include "../src/PluginProcessor.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
void set(juce::AudioProcessorValueTreeState& s, const char* id, float v)
{
    auto* p = s.getParameter(id);
    if (!p) std::exit(2);
    p->setValueNotifyingHost(p->convertTo0to1(v));
}

bool near(float a, float b, float tolerance = 0.011f)
{
    return std::abs(a - b) <= tolerance;
}
}

namespace
{
std::vector<float> renderTape(float quality, float age, bool macroLink, float lowPassHz = 11000.0f)
{
    constexpr int sampleCount = 24000;
    constexpr float sampleRate = 48000.0f;
    TapeEngineAudioProcessor processor;
    processor.setRateAndBufferSizeDetails(sampleRate, 256);
    processor.prepareToPlay(sampleRate, 256);
    auto& state = processor.getAPVTS();
    set(state, "quality", quality);
    set(state, "age", age);
    set(state, "wow", 0.0f);
    set(state, "glitch", 0.0f);
    set(state, "macroLink", macroLink ? 1.0f : 0.0f);
    set(state, "lpHz", lowPassHz);
    set(state, "hiss", 0.0f);
    set(state, "hum", 0.0f);
    set(state, "dropout", 0.0f);
    set(state, "sfxEnable", 0.0f);

    juce::AudioBuffer<float> buffer(2, sampleCount);
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const auto time = (float) sample / sampleRate;
        const auto signal = 0.24f * std::sin(juce::MathConstants<float>::twoPi * 440.0f * time)
                          + 0.12f * std::sin(juce::MathConstants<float>::twoPi * 9000.0f * time);
        buffer.setSample(0, sample, signal);
        buffer.setSample(1, sample, signal);
    }
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
    return { buffer.getReadPointer(0), buffer.getReadPointer(0) + sampleCount };
}

float meanDifference(const std::vector<float>& a, const std::vector<float>& b)
{
    double difference = 0.0;
    const auto count = juce::jmin(a.size(), b.size());
    for (std::size_t i = 1024; i < count; ++i)
        difference += std::abs(a[i] - b[i]);
    return (float) (difference / (double) juce::jmax<std::size_t>(1, count - 1024));
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    {
        TapeEngineAudioProcessor canonical;
        auto& state = canonical.getAPVTS();
        if (canonical.legacyMacrosActive()
            || state.getParameter("dropoutTempoSync") == nullptr
            || state.getParameter("wowTempoSync") == nullptr
            || state.getParameter("flutterTempoSync") == nullptr)
        {
            std::cerr << "Tape V5 canonical/performer parameter contract is missing\n";
            return 1;
        }

        set(state, "quality", 0.32f);
        set(state, "age", 0.62f);
        set(state, "wow", 0.55f);
        set(state, "glitch", 0.35f);
        set(state, "outGain", 0.98f);
        set(state, "macroLink", 1.0f);
        const auto target = lost_audio::core::mapTapeMacros(0.32f, 0.62f, 0.55f, 0.35f);
        canonical.materialiseLegacyMacros();
        if (canonical.legacyMacrosActive()
            || !near(state.getRawParameterValue("lpHz")->load(), target.lowPassHz, 1.1f)
            || !near(state.getRawParameterValue("drive")->load(), target.drive)
            || !near(state.getRawParameterValue("dropout")->load(), target.dropout)
            || !near(state.getRawParameterValue("speed")->load(), target.speed))
        {
            std::cerr << "Tape legacy macro materialisation did not produce canonical DSP state\n";
            return 1;
        }
    }

    {
        TapeEngineAudioProcessor dryProcessor;
        dryProcessor.setRateAndBufferSizeDetails(48000.0, 256);
        dryProcessor.prepareToPlay(48000.0, 256);
        auto& dryState = dryProcessor.getAPVTS();
        set(dryState, "mix", 0.0f);
        set(dryState, "outGain", 1.0f);
        set(dryState, "sfxEnable", 1.0f);
        set(dryState, "sfxLevel", 1.0f);
        juce::AudioBuffer<float> impulse(2, 2048);
        impulse.clear();
        impulse.setSample(0, 0, 0.5f);
        impulse.setSample(1, 0, -0.5f);
        juce::MidiBuffer dryMidi;
        dryProcessor.processBlock(impulse, dryMidi);
        const auto latency = dryProcessor.getLatencySamples();
        if (latency <= 0 || std::abs(impulse.getSample(0, latency) - 0.5f) > 1.0e-6f
            || std::abs(impulse.getSample(1, latency) + 0.5f) > 1.0e-6f
            || impulse.getMagnitude(0, 0, latency) > 1.0e-6f)
        {
            std::cerr << "Tape dry mix is not sample-accurately aligned to reported latency\n";
            return 1;
        }
    }

    {
        TapeEngineAudioProcessor triggered;
        triggered.setRateAndBufferSizeDetails(48000.0, 256);
        triggered.prepareToPlay(48000.0, 256);
        auto& state = triggered.getAPVTS();
        set(state, "dropoutStrength", 0.8f);
        set(state, "dropoutMs", 80.0f);
        set(state, "dropoutLengthSync", 0.0f);
        set(state, "dropout", 0.0f);
        set(state, "hiss", 0.0f);
        set(state, "hum", 0.0f);
        set(state, "sfxEnable", 0.0f);
        juce::AudioBuffer<float> buffer(2, 1024);
        buffer.clear();
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(0, sample, 0.25f), buffer.setSample(1, sample, -0.25f);
        triggered.triggerDropout();
        juce::MidiBuffer midi;
        triggered.processBlock(buffer, midi);
        if (!triggered.dropoutIsActive() || triggered.dropoutProgressMeter() <= 0.0f)
        {
            std::cerr << "Tape adapter manual dropout trigger/telemetry failure\n";
            return 1;
        }
    }

    TapeEngineAudioProcessor p; p.setRateAndBufferSizeDetails(48000.0, 256); p.prepareToPlay(48000.0, 256); auto& s=p.getAPVTS();
    set(s,"macroLink",0); set(s,"hiss",0); set(s,"hum",0); set(s,"dropout",0); set(s,"glitch",0); set(s,"sfxMode",0); set(s,"sfxLevel",1); set(s,"sfxEnable",0);
    juce::MidiBuffer midi; juce::AudioBuffer<float> off(2,48000); off.clear(); p.processBlock(off,midi); const auto offMag=off.getMagnitude(0,0,off.getNumSamples());
    set(s,"sfxEnable",1); juce::AudioBuffer<float> on(2,48000); on.clear(); p.processBlock(on,midi); const auto onMag=on.getMagnitude(0,0,on.getNumSamples());
    if(offMag>1.0e-6f||onMag<1.0e-4f){std::cerr<<"Tape bed adapter failure: off="<<offMag<<" on="<<onMag<<'\n';return 1;}

    const auto highFidelity = renderTape(1.0f, 0.35f, true);
    const auto lowFidelity = renderTape(0.0f, 0.35f, true);
    const auto freshTape = renderTape(0.75f, 0.0f, true);
    const auto agedTape = renderTape(0.75f, 1.0f, true);
    const auto directWide = renderTape(0.55f, 0.35f, false, 18000.0f);
    const auto directNarrow = renderTape(0.55f, 0.35f, false, 2200.0f);
    const auto fidelityDelta = meanDifference(highFidelity, lowFidelity);
    const auto ageDelta = meanDifference(freshTape, agedTape);
    const auto directDelta = meanDifference(directWide, directNarrow);
    if (fidelityDelta < 0.002f || ageDelta < 0.002f || directDelta < 0.002f)
    {
        std::cerr << "Tape routing failure: fidelity=" << fidelityDelta
                  << " age=" << ageDelta << " advanced=" << directDelta << '\n';
        return 1;
    }
    std::cout << "Tape adapter passed: canonical migration, manual dropout telemetry, aligned dry mix, mechanism bed, legacy macros, and direct routing\n";
}
