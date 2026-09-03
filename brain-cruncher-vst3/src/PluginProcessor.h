#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/OcclusionProcessor.h>

#include <array>
#include <atomic>
#include <vector>

class BrainCruncherAudioProcessor final : public juce::AudioProcessor
{
public:
    BrainCruncherAudioProcessor();
    ~BrainCruncherAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
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
    float inputPeak(int channel) const noexcept;
    float outputPeak(int channel) const noexcept;
    float stereoMotion() const noexcept { return motionMeter.load(); }
    bool rattleActive() const noexcept { return core.rattleActive(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct Controls
    {
        float crunch = 0.86f;
        float body = 0.68f;
        float bite = 0.62f;
        float space = 0.46f;
        float smear = 0.74f;
        float motion = 0.38f;
        float width = 0.78f;
        float binaural = 0.72f;
        float pan = 0.0f;
        float headSize = 0.55f;
        float drive = 0.35f;
        float inputGain = 1.0f;
        float mix = 1.0f;
        float outputGain = 1.0f;
        float ceiling = 0.92f;
    };

    Controls readControls() const noexcept;
    lost_audio::core::OcclusionParameters makeCoreParameters(const Controls&) const noexcept;
    float readMotionDelay(std::size_t channel, float delaySamples) const noexcept;

    juce::AudioProcessorValueTreeState apvts;
    lost_audio::core::OcclusionProcessor core;
    juce::AudioBuffer<float> dryBuffer;
    std::array<std::vector<float>, 2> motionDelay;
    std::array<std::atomic<float>, 2> inputPeaks { 0.0f, 0.0f };
    std::array<std::atomic<float>, 2> outputPeaks { 0.0f, 0.0f };
    std::atomic<float> motionMeter { 0.0f };
    std::size_t motionWrite = 0;
    double currentSampleRate = 48000.0;
    float lfoPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrainCruncherAudioProcessor)
};
