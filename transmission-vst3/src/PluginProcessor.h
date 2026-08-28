#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/TransmissionProcessor.h>

#include <atomic>
#include <vector>

class TransmissionEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    TransmissionEngineAudioProcessor();
    ~TransmissionEngineAudioProcessor() override = default;

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
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct TuningParameters
    {
        bool enabled = false;
        int mode = 0;
        int source = 0;
        float amount = 0.35f;
        float snippetMs = 140.0f;
        float cutDepth = 0.55f;
    };

    static std::vector<float> decodeMp3ToMono(const void* data, std::size_t bytes, double targetSampleRate);
    void initializeTuningSamples(double sampleRate);
    void triggerTuningEvent(float sampleRate, const TuningParameters& parameters);
    [[nodiscard]] float nextTuningSample(float sampleRate, const TuningParameters& parameters,
                                         bool isPlaying, float& duckOut) noexcept;
    [[nodiscard]] float sampleEmbeddedTuning(int sampleIndex, float position) const noexcept;
    [[nodiscard]] float nextRandom() noexcept;
    [[nodiscard]] float nextSigned() noexcept { return nextRandom() * 2.0f - 1.0f; }

    juce::AudioProcessorValueTreeState apvts;
    lost_audio::core::TransmissionProcessor transmissionCore;
    std::atomic<float> outputPeak { 0.0f };

    std::vector<std::vector<float>> embeddedTuningSamples;
    std::uint32_t tuningRandomState = 0x71c19e51u;
    int tuningRemaining = 0;
    int tuningTotal = 0;
    int tuningCooldownRemaining = 0;
    float tuningF0 = 1200.0f;
    float tuningF1 = 3200.0f;
    float tuningQ = 6.0f;
    float tuningPhase = 0.0f;
    float tuningPlayPosition = 0.0f;
    float tuningPlayStep = 1.0f;
    int tuningSampleIndex = 0;
    float tuningFilterIc1 = 0.0f;
    float tuningFilterIc2 = 0.0f;
    bool transportWasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransmissionEngineAudioProcessor)
};
