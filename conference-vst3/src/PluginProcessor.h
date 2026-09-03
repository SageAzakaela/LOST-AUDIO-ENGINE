#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/ConferenceProcessor.h>

#include <array>
#include <atomic>
#include <cstdint>

class ConferenceEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    ConferenceEngineAudioProcessor();
    ~ConferenceEngineAudioProcessor() override = default;
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
    void triggerPacketLoss() noexcept { pendingPacketTrigger.store(true, std::memory_order_release); }
    void triggerRobot() noexcept { pendingRobotTrigger.store(true, std::memory_order_release); }
    [[nodiscard]] float inputPeak(int ch) const noexcept { return inputPeaks[(std::size_t) juce::jlimit(0, 1, ch)].load(std::memory_order_relaxed); }
    [[nodiscard]] float outputPeak(int ch) const noexcept { return outputPeaks[(std::size_t) juce::jlimit(0, 1, ch)].load(std::memory_order_relaxed); }
    [[nodiscard]] bool packetLost() const noexcept { return lossActive.load(); }
    [[nodiscard]] bool robotActive() const noexcept { return robotState.load(); }
    [[nodiscard]] bool bufferSlipActive() const noexcept { return slipState.load(); }
    [[nodiscard]] bool bandwidthCollapsed() const noexcept { return bandwidthState.load(); }
    [[nodiscard]] float packetProgress() const noexcept { return packetProgressMeter.load(); }
    [[nodiscard]] float robotProgress() const noexcept { return robotProgressMeter.load(); }
    [[nodiscard]] float jitterActivity() const noexcept { return jitterMeter.load(); }
    [[nodiscard]] float suppressionActivity() const noexcept { return suppressionMeter.load(); }
    [[nodiscard]] float agcActivity() const noexcept { return agcMeter.load(); }
    [[nodiscard]] float comfortNoiseActivity() const noexcept { return comfortMeter.load(); }
    [[nodiscard]] float limiterActivity() const noexcept { return limiterMeter.load(); }
    [[nodiscard]] std::array<float, 64> outputTrace() const noexcept;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    [[nodiscard]] float value(const char*) const noexcept;
    [[nodiscard]] lost_audio::core::ConferenceParameters readParameters(double bpm) const noexcept;
    [[nodiscard]] float packetDurationSeconds(double bpm) const noexcept;
    [[nodiscard]] float robotDurationSeconds(double bpm) const noexcept;
    juce::AudioProcessorValueTreeState apvts;
    lost_audio::core::ConferenceProcessor conferenceCore;
    std::array<std::atomic<float>, 2> inputPeaks { 0.0f, 0.0f }, outputPeaks { 0.0f, 0.0f };
    std::array<std::atomic<float>, 64> trace {};
    std::atomic<bool> lossActive { false }, robotState { false }, slipState { false };
    std::atomic<bool> bandwidthState { false }, pendingPacketTrigger { false }, pendingRobotTrigger { false };
    std::atomic<float> packetProgressMeter { 0.0f }, robotProgressMeter { 0.0f }, jitterMeter { 0.0f };
    std::atomic<float> suppressionMeter { 0.0f }, agcMeter { 0.0f }, comfortMeter { 0.0f }, limiterMeter { 0.0f };
    double currentBpm = 120.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConferenceEngineAudioProcessor)
};
