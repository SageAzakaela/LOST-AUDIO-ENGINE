#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/TempoSync.h>
#include <lost_audio/core/TransmissionProcessor.h>

#include <atomic>
#include <array>
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
    void triggerTuningSearch() noexcept { pendingTuningTrigger.store(true, std::memory_order_release); }
    void triggerDropout() noexcept;
    void materialiseLegacyMacros();
    [[nodiscard]] bool legacyMacrosActive() const noexcept;
    [[nodiscard]] float inputPeak(int channel) const noexcept;
    [[nodiscard]] float outputPeakForChannel(int channel) const noexcept;
    [[nodiscard]] std::array<float, 64> outputTrace() const noexcept;
    [[nodiscard]] float carrierMeter() const noexcept { return carrierTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] bool dropoutIsActive() const noexcept { return dropoutState.load(std::memory_order_relaxed); }
    [[nodiscard]] float dropoutProgressMeter() const noexcept { return dropoutTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] float compressionMeter() const noexcept { return compressionTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] float noiseMeter() const noexcept { return noiseTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] float interferenceMeter() const noexcept { return interferenceTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] bool squelchIsClosed() const noexcept { return squelchState.load(std::memory_order_relaxed); }
    [[nodiscard]] float squelchEventMeter() const noexcept { return squelchTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] bool tuningIsActive() const noexcept { return tuningState.load(std::memory_order_relaxed); }
    [[nodiscard]] float tuningProgressMeter() const noexcept { return tuningTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] int tuningAssetMeter() const noexcept { return tuningAsset.load(std::memory_order_relaxed); }
    [[nodiscard]] float limiterMeter() const noexcept { return limiterTelemetry.load(std::memory_order_relaxed); }
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
        bool tempoSync = false;
        float probability = 1.0f;
    };

    static std::vector<float> decodeMp3ToMono(const void* data, std::size_t bytes, double targetSampleRate);
    void initializeTuningSamples(double sampleRate);
    void triggerTuningEvent(float sampleRate, const TuningParameters& parameters);
    [[nodiscard]] float nextTuningSample(float sampleRate, const TuningParameters& parameters,
                                         bool isPlaying, float& duckOut) noexcept;
    [[nodiscard]] float sampleEmbeddedTuning(int sampleIndex, float position) const noexcept;
    [[nodiscard]] float nextRandom() noexcept;
    [[nodiscard]] float nextSigned() noexcept { return nextRandom() * 2.0f - 1.0f; }
    [[nodiscard]] float value(const char* id) const noexcept;
    [[nodiscard]] lost_audio::core::TransmissionParameters readParameters(double bpm) const noexcept;
    [[nodiscard]] float dropoutDurationSeconds(double bpm) const noexcept;

    juce::AudioProcessorValueTreeState apvts;
    lost_audio::core::TransmissionProcessor transmissionCore;
    std::atomic<float> outputPeak { 0.0f };
    std::array<std::atomic<float>, 2> inputPeaks {}, outputPeaks {};
    std::array<std::atomic<float>, 64> trace {};
    std::atomic<float> carrierTelemetry { 0.0f }, dropoutTelemetry { 0.0f };
    std::atomic<float> compressionTelemetry { 0.0f }, noiseTelemetry { 0.0f };
    std::atomic<float> interferenceTelemetry { 0.0f }, squelchTelemetry { 0.0f };
    std::atomic<float> tuningTelemetry { 0.0f }, limiterTelemetry { 0.0f };
    std::atomic<bool> dropoutState { false }, squelchState { false }, tuningState { false };
    std::atomic<int> tuningAsset { 0 };
    std::atomic<bool> pendingTuningTrigger { false };

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
    std::int64_t lastTuningTempoStep = -1;
    std::int64_t fallbackTuningTempoStep = 0;
    int lastTuningDivision = -1;
    int tuningTempoFallbackSamples = 0;
    double lastHostPpq = 0.0, currentBpm = 120.0;
    bool hostTuningWasPlaying = false;
    std::int64_t lastDropoutTempoStep = -1, fallbackDropoutTempoStep = 0;
    int lastDropoutDivision = -1, dropoutTempoFallbackSamples = 0;
    double lastDropoutHostPpq = 0.0;
    bool hostDropoutWasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransmissionEngineAudioProcessor)
};
