#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/CDProcessor.h>

#include <array>
#include <atomic>

class CDEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    CDEngineAudioProcessor();
    ~CDEngineAudioProcessor() override = default;

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
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override;

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    void triggerDamage(float strength = 1.0f) noexcept { cd.triggerDamage(strength); }
    void triggerSkip(float strength = 1.0f) noexcept { cd.triggerSkip(strength); }
    [[nodiscard]] float inputPeak(int channel) const noexcept;
    [[nodiscard]] float outputPeak(int channel) const noexcept;
    [[nodiscard]] bool damageActive() const noexcept { return damageActiveFlag.load(std::memory_order_relaxed); }
    [[nodiscard]] bool skipActive() const noexcept { return skipActiveFlag.load(std::memory_order_relaxed); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    [[nodiscard]] float value(const char* parameterID) const noexcept;

    juce::AudioProcessorValueTreeState apvts;
    lost_audio::core::CDProcessor cd;
    std::array<std::atomic<float>, 2> inputPeaks {};
    std::array<std::atomic<float>, 2> outputPeaks {};
    std::atomic<bool> damageActiveFlag { false };
    std::atomic<bool> skipActiveFlag { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CDEngineAudioProcessor)
};
