#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/CamcorderProcessor.h>

#include <array>
#include <atomic>
#include <vector>

class CamcorderEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    CamcorderEngineAudioProcessor();
    ~CamcorderEngineAudioProcessor() override = default;
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
    void materialiseLegacyMacros();
    [[nodiscard]] bool legacyMacrosActive() const noexcept;
    void triggerDropout() noexcept { pendingDropTrigger.store(true, std::memory_order_release); }
    void triggerCodecFault() noexcept { pendingFaultTrigger.store(true, std::memory_order_release); }
    void triggerHandling() noexcept { pendingHandlingTrigger.store(true, std::memory_order_release); }
    [[nodiscard]] float inputPeak(int channel) const noexcept { return inputPeaks[(std::size_t) juce::jlimit(0, 1, channel)].load(); }
    [[nodiscard]] float outputPeak(int channel) const noexcept { return outputPeaks[(std::size_t) juce::jlimit(0, 1, channel)].load(); }
    [[nodiscard]] bool windActive() const noexcept { return windState.load(); }
    [[nodiscard]] bool handlingActive() const noexcept { return handlingState.load(); }
    [[nodiscard]] bool dropoutActive() const noexcept { return dropoutState.load(); }
    [[nodiscard]] bool corruptionActive() const noexcept { return corruptionState.load(); }
    [[nodiscard]] float dropoutProgress() const noexcept { return dropoutProgressMeter.load(); }
    [[nodiscard]] float corruptionProgress() const noexcept { return corruptionProgressMeter.load(); }
    [[nodiscard]] float handlingProgress() const noexcept { return handlingProgressMeter.load(); }
    [[nodiscard]] float windProgress() const noexcept { return windProgressMeter.load(); }
    [[nodiscard]] float agcActivity() const noexcept { return agcMeter.load(); }
    [[nodiscard]] float flutterActivity() const noexcept { return flutterMeter.load(); }
    [[nodiscard]] float limiterActivity() const noexcept { return limiterMeter.load(); }
    [[nodiscard]] float cameraBedActivity() const noexcept { return cameraBedMeter.load(); }
    [[nodiscard]] float windBedActivity() const noexcept { return windBedMeter.load(); }
    [[nodiscard]] std::array<float, 64> outputTrace() const noexcept;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    [[nodiscard]] float value(const char*) const noexcept;
    [[nodiscard]] lost_audio::core::CamcorderParameters readParameters(double bpm) const noexcept;
    [[nodiscard]] float syncedDuration(const char* syncId, const char* divisionId, const char* millisecondsId, double bpm) const noexcept;
    [[nodiscard]] std::vector<float> decodeBed(const void*, std::size_t, double) const;
    [[nodiscard]] float readBed(const std::vector<float>&, float&) const noexcept;
    juce::AudioProcessorValueTreeState apvts;
    lost_audio::core::CamcorderProcessor camcorderCore;
    std::array<std::atomic<float>, 2> inputPeaks { 0.0f, 0.0f }, outputPeaks { 0.0f, 0.0f };
    std::array<std::atomic<float>, 64> trace {};
    std::atomic<bool> windState { false }, handlingState { false }, dropoutState { false }, corruptionState { false };
    std::atomic<bool> pendingDropTrigger { false }, pendingFaultTrigger { false }, pendingHandlingTrigger { false };
    std::atomic<float> dropoutProgressMeter { 0 }, corruptionProgressMeter { 0 }, handlingProgressMeter { 0 }, windProgressMeter { 0 };
    std::atomic<float> agcMeter { 0 }, flutterMeter { 0 }, limiterMeter { 0 }, cameraBedMeter { 0 }, windBedMeter { 0 };
    std::array<std::vector<float>, 4> cameraBeds, windBeds;
    std::array<float, 4> cameraBedPositions {}, windBedPositions {};
    std::vector<float> bedChunk;
    double currentBpm = 120.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CamcorderEngineAudioProcessor)
};
