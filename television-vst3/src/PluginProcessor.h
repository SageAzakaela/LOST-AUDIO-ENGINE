#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/TelevisionProcessor.h>
#include <lost_audio/core/TempoSync.h>

#include <array>
#include <atomic>
#include <vector>

class TelevisionEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    TelevisionEngineAudioProcessor();
    ~TelevisionEngineAudioProcessor() override = default;

    void prepareToPlay(double, int) override;
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
    void triggerSyncFault() noexcept;
    void materialiseLegacyMacros();
    [[nodiscard]] bool legacyMacrosActive() const noexcept;
    [[nodiscard]] float inputPeak(int ch) const noexcept;
    [[nodiscard]] float outputPeak(int ch) const noexcept;
    [[nodiscard]] bool staticActive() const noexcept { return staticState.load(std::memory_order_relaxed); }
    [[nodiscard]] bool syncFaultActive() const noexcept { return syncState.load(std::memory_order_relaxed); }
    [[nodiscard]] float bedLevelMeter() const noexcept { return bedMeter.load(std::memory_order_relaxed); }
    [[nodiscard]] float staticLevelMeter() const noexcept { return staticMeter.load(std::memory_order_relaxed); }
    [[nodiscard]] float electricalLevelMeter() const noexcept { return electricalMeter.load(std::memory_order_relaxed); }
    [[nodiscard]] float rattleLevelMeter() const noexcept { return rattleMeter.load(std::memory_order_relaxed); }
    [[nodiscard]] float syncProgressMeter() const noexcept { return syncMeter.load(std::memory_order_relaxed); }
    [[nodiscard]] std::array<float, 64> outputTrace() const noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    [[nodiscard]] float value(const char*) const noexcept;
    [[nodiscard]] lost_audio::core::TelevisionParameters readParameters() const noexcept;
    [[nodiscard]] float faultDurationSeconds(double bpm) const noexcept;
    std::vector<float> decodeMp3ToMono(const void*, std::size_t, double) const;
    float readBedSample(float) const noexcept;

    juce::AudioProcessorValueTreeState apvts;
    lost_audio::core::TelevisionProcessor core;
    std::vector<float> bedSample;
    float bedPosition = 0.0f;
    std::array<float, 4096> bedChunk {};
    std::array<std::atomic<float>, 2> inputPeaks {}, outputPeaks {};
    std::array<std::atomic<float>, 64> trace {};
    std::atomic<bool> staticState { false }, syncState { false };
    std::atomic<float> bedMeter { 0.0f }, staticMeter { 0.0f }, electricalMeter { 0.0f };
    std::atomic<float> rattleMeter { 0.0f }, syncMeter { 0.0f };
    std::int64_t lastTempoStep = -1, fallbackTempoStep = 0;
    int lastTempoDivision = -1, tempoFallbackSamples = 0;
    double lastHostPpq = 0.0, currentBpm = 120.0;
    bool hostTempoWasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TelevisionEngineAudioProcessor)
};
