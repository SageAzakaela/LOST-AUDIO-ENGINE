#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/CommsProcessor.h>

#include <atomic>

class CommsEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    CommsEngineAudioProcessor();
    ~CommsEngineAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    [[nodiscard]] float getOutputPeak() const noexcept { return outputPeak.load(std::memory_order_relaxed); }
    void materialiseLegacyMacros();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    [[nodiscard]] lost_audio::core::CommsParameters readParameters() const noexcept;

    juce::AudioProcessorValueTreeState apvts;
    lost_audio::core::CommsProcessor commsCore;
    std::atomic<float> outputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommsEngineAudioProcessor)
};
