#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/TapeProcessor.h>
#include <lost_audio/core/TempoSync.h>
#include <array>
#include <atomic>
#include <random>
#include <string>
#include <vector>

class TapeEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    using juce::AudioProcessor::processBlock;

    TapeEngineAudioProcessor();
    ~TapeEngineAudioProcessor() override = default;

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
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    float getOutputPeak() const noexcept { return outputPeak.load(std::memory_order_relaxed); }
    void triggerDropout() noexcept;
    void triggerMechanism() noexcept { pendingMechanismTrigger.store(true, std::memory_order_release); }
    void materialiseLegacyMacros();
    [[nodiscard]] bool legacyMacrosActive() const noexcept;
    [[nodiscard]] float inputPeak(int channel) const noexcept;
    [[nodiscard]] float outputPeakForChannel(int channel) const noexcept;
    [[nodiscard]] float modulationMeter() const noexcept { return modulationTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] bool dropoutIsActive() const noexcept { return dropoutState.load(std::memory_order_relaxed); }
    [[nodiscard]] float dropoutProgressMeter() const noexcept { return dropoutTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] float compressionMeter() const noexcept { return compressionTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] float saturationMeter() const noexcept { return saturationTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] float noiseMeter() const noexcept { return noiseTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] float mechanismMeter() const noexcept { return mechanismTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] float limiterMeter() const noexcept { return limiterTelemetry.load(std::memory_order_relaxed); }
    [[nodiscard]] std::array<float, 64> outputTrace() const noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    enum class SfxMode : int
    {
        bed = 0,
        edges = 1,
        sequence = 2
    };

    struct SfxVoice
    {
        int sampleIndex = -1;
        float pos = 0.0f;
        float step = 1.0f;
        float gain = 0.0f;
        bool active = false;
    };

    struct ToneState
    {
        juce::dsp::IIR::Filter<float> hp;
        juce::dsp::IIR::Filter<float> bump;
        juce::dsp::IIR::Filter<float> lp1;
        juce::dsp::IIR::Filter<float> lp2;
    };

    void updateToneFilters();
    [[nodiscard]] float value(const char* id) const noexcept;
    [[nodiscard]] lost_audio::core::TapeParameters readParameters(double bpm) const noexcept;
    [[nodiscard]] float dropoutDurationSeconds(double bpm) const noexcept;
    std::vector<float> decodeWavToMono(const void* data, size_t bytes, double targetSampleRate) const;
    void initSfx(double sampleRate);
    void startSfxVoice(int sampleIndex, float gain = 1.0f);
    float processSfxSample(float signalAbs, float sampleRate, float glitch, bool enabled, int bank, SfxMode mode, float level);
    float readEmbeddedSample(int sampleIndex, float pos) const;

    juce::AudioProcessorValueTreeState apvts;

    std::array<ToneState, 2> tone{};
    lost_audio::core::TapeProcessor tapeCore;
    juce::AudioBuffer<float> alignedDry;
    std::array<std::vector<float>, 2> dryDelay{};
    std::size_t dryWriteIndex = 0;
    std::atomic<float> outputPeak { 0.0f };
    std::array<std::atomic<float>, 2> inputPeaks {}, outputPeaks {};
    std::array<std::atomic<float>, 64> trace {};
    std::atomic<float> modulationTelemetry { 0.0f }, dropoutTelemetry { 0.0f };
    std::atomic<float> compressionTelemetry { 0.0f }, saturationTelemetry { 0.0f };
    std::atomic<float> noiseTelemetry { 0.0f }, mechanismTelemetry { 0.0f }, limiterTelemetry { 0.0f };
    std::atomic<bool> dropoutState { false }, pendingMechanismTrigger { false };

    std::int64_t lastTempoStep = -1, fallbackTempoStep = 0;
    int lastTempoDivision = -1, tempoFallbackSamples = 0;
    double lastHostPpq = 0.0, currentBpm = 120.0;
    bool hostTempoWasPlaying = false;

    std::minstd_rand rng;
    std::uniform_real_distribution<float> unif { 0.0f, 1.0f };

    std::vector<std::vector<float>> sfxSamples;
    std::array<int, 2> bedSampleByBank { 0, 7 };
    std::array<std::vector<int>, 2> startByBank { std::vector<int> { 1, 2, 3 }, std::vector<int> { 8, 9 } };
    std::array<std::vector<int>, 2> endByBank { std::vector<int> { 4, 5, 6 }, std::vector<int> { 4, 10 } };
    std::array<float, 2> bedPos { 0.0f, 0.0f };
    std::array<SfxVoice, 4> sfxVoices {};
    float sfxGateEnv = 0.0f;
    int sfxBelowCount = 0;
    bool sfxInSilence = false;
    int sfxSeqCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeEngineAudioProcessor)
};
