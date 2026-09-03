#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/OpenMicProcessor.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

class OpenMicNightAudioProcessor final : public juce::AudioProcessor
{
public:
    OpenMicNightAudioProcessor();
    ~OpenMicNightAudioProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return parameters; }
    void materialiseLegacyMacros(); [[nodiscard]] bool legacyMacrosActive() const noexcept;
    void triggerFeedback() noexcept { pendingFeedback.store(true,std::memory_order_release); }
    void triggerCrowd() noexcept { pendingCrowd.store(true,std::memory_order_release); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float inputPeak(int channel) const noexcept { return inputMeter[static_cast<std::size_t>(juce::jlimit(0, 1, channel))].load(); }
    float outputPeak(int channel) const noexcept { return outputMeter[static_cast<std::size_t>(juce::jlimit(0, 1, channel))].load(); }
    float feedbackActivity() const noexcept { return feedbackMeter.load(); }
    float crowdActivity() const noexcept { return crowdMeter.load(); }
    float roomActivity() const noexcept { return roomMeter.load(); }
    float limiterActivity() const noexcept { return limiterMeter.load(); }
    bool crowdEventActive() const noexcept { return crowdEventState.load(); }
    bool feedbackEventActive() const noexcept { return feedbackEventState.load(); }
    float crowdEventProgress() const noexcept { return crowdProgressMeter.load(); }
    float feedbackEventProgress() const noexcept { return feedbackProgressMeter.load(); }
    std::array<float,64> outputTrace() const noexcept;
    bool safetyEngaged() const noexcept { return safetyMeter.load(); }
    bool embeddedCrowdReady() const noexcept;
    float inputEnergyActivity() const noexcept { return inputEnergyMeter.load(); }
    float audienceResponseActivity() const noexcept { return audienceResponseMeter.load(); }

private:
    struct StereoAsset
    {
        std::array<std::vector<float>, 2> channels;
        float normalizationGain = 1.0f;
        [[nodiscard]] bool ready() const noexcept { return !channels[0].empty() && !channels[1].empty(); }
    };
    float value(const char* id) const noexcept;
    lost_audio::core::OpenMicParameters makeCoreParameters() const noexcept;
    [[nodiscard]] StereoAsset decodeAsset(const void* data, std::size_t bytes, double targetRate) const;
    [[nodiscard]] static float readLooped(const StereoAsset& asset, int channel, double position) noexcept;
    void updateAudienceEnergy(const juce::AudioBuffer<float>& buffer, int channels, int samples) noexcept;
    void renderAudience(int offset, int count, int outputChannels) noexcept;
    juce::AudioProcessorValueTreeState parameters;
    lost_audio::core::OpenMicProcessor core;
    std::array<std::atomic<float>, 2> inputMeter {};
    std::array<std::atomic<float>, 2> outputMeter {};
    std::atomic<float> feedbackMeter { 0.0f };
    std::atomic<float> crowdMeter { 0.0f }, roomMeter { 0.0f }, limiterMeter { 0.0f };
    std::atomic<bool> safetyMeter { false };
    std::atomic<bool> crowdEventState { false }, feedbackEventState { false }, pendingFeedback { false }, pendingCrowd { false };
    std::atomic<float> crowdProgressMeter { 0 }, feedbackProgressMeter { 0 };
    std::atomic<float> inputEnergyMeter { 0 }, audienceResponseMeter { 0 };
    std::array<std::atomic<float>,64> trace {};
    int feedbackEventRemaining=0, feedbackEventTotal=1; double currentBpm=120.0;
    std::array<StereoAsset, 4> crowdBeds;
    std::array<StereoAsset, 2> reactionAssets;
    juce::AudioBuffer<float> audienceScratch;
    std::array<double, 4> crowdBedPositions {};
    double crowdEventPosition = 0.0;
    int crowdAssetEventRemaining = 0, crowdAssetEventTotal = 1, activeCrowdEventType = 0;
    float activeCrowdEventStrength = 0.0f;
    int activeBedIndex = 1, previousBedIndex = 1;
    float bedCrossfade = 1.0f, inputEnergy = 0.0f, inputEnergyPeak = 0.0f;
    bool performancePhraseArmed = false;
    int reactionDelayRemaining = 0, reactionCooldownRemaining = 0;
    bool reactiveReactionReady = false;
    int pendingReactionType = 1;
    float pendingReactionStrength = 0.0f, pendingReactionDurationMs = 1600.0f;
    std::uint32_t audienceRandom = 0x41554449u;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMicNightAudioProcessor)
};
