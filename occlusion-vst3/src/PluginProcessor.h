#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/OcclusionProcessor.h>

#include <array>
#include <atomic>

class OcclusionEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    OcclusionEngineAudioProcessor();
    ~OcclusionEngineAudioProcessor() override = default;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void materialiseLegacyMacros();
    bool legacyMacrosActive() const noexcept;
    void triggerBoundary() noexcept { pendingBoundary.store(true, std::memory_order_release); }
    void triggerHardware() noexcept { pendingHardware.store(true, std::memory_order_release); }
    float inputPeak(int c) const noexcept { return inputPeaks[(std::size_t)juce::jlimit(0,1,c)].load(); }
    float outputPeak(int c) const noexcept { return outputPeaks[(std::size_t)juce::jlimit(0,1,c)].load(); }
    float bodyActivity() const noexcept { return bodyMeter.load(); }
    float roomActivity() const noexcept { return roomMeter.load(); }
    float leakActivity() const noexcept { return leakMeter.load(); }
    float rattleActivity() const noexcept { return rattleMeter.load(); }
    float limiterActivity() const noexcept { return limiterMeter.load(); }
    bool excitationActive() const noexcept { return excitationState.load(); }
    float excitationProgress() const noexcept { return excitationProgressMeter.load(); }
    std::array<float,64> outputTrace() const noexcept;

private:
    float value(const char*) const noexcept;
    lost_audio::core::OcclusionParameters readParameters() const noexcept;
    juce::AudioProcessorValueTreeState apvts;
    lost_audio::core::OcclusionProcessor core;
    std::array<std::atomic<float>,2> inputPeaks {0,0}, outputPeaks {0,0};
    std::atomic<float> bodyMeter {0}, roomMeter {0}, leakMeter {0}, rattleMeter {0}, limiterMeter {0};
    std::atomic<bool> excitationState {false}, pendingBoundary {false}, pendingHardware {false};
    std::atomic<float> excitationProgressMeter {0};
    std::array<std::atomic<float>,64> trace {};
    double currentBpm = 120.0;
    float motionPhase = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OcclusionEngineAudioProcessor)
};
