#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/SequencerClock.h>
#include <lost_audio/core/SuiteProcessor.h>

#include <array>
#include <atomic>
#include <vector>

class LostAudioSequencerProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int stepCount = 16;

    LostAudioSequencerProcessor();
    ~LostAudioSequencerProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String stepId(int step, const char* suffix);
    juce::AudioProcessorValueTreeState& state() noexcept { return parameters; }
    int currentStep() const noexcept { return activeStep.load(); }
    float currentPhase() const noexcept { return stepPhase.load(); }
    bool currentStepFired() const noexcept { return stepFired.load(); }
    bool transportActive() const noexcept { return transportRunning.load(); }
    float currentBpm() const noexcept { return bpmMeter.load(); }
    float inputPeak(int channel) const noexcept { return inputMeter[static_cast<std::size_t>(juce::jlimit(0, 1, channel))].load(); }
    float outputPeak(int channel) const noexcept { return outputMeter[static_cast<std::size_t>(juce::jlimit(0, 1, channel))].load(); }
    bool safetyEngaged() const noexcept { return safetyMeter.load(); }

    void applyPreset(int presetIndex);
    void randomizePattern();
    void clearPattern();

private:
    struct StepRefs
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* engine = nullptr;
        std::atomic<float>* character = nullptr;
        std::atomic<float>* damage = nullptr;
        std::atomic<float>* probability = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* model = nullptr;
    };

    void cacheParameterPointers();
    lost_audio::core::SuiteParameters parametersForStep(int patternStep, std::int64_t absoluteStep) const noexcept;
    void processSegment(juce::AudioBuffer<float>& buffer, int offset, int samples,
                        const lost_audio::core::SuiteParameters& current);
    void processInactive(juce::AudioBuffer<float>& buffer);
    std::vector<float> decodeCrtBed(double sampleRate) const;
    float readCrtBed() noexcept;
    void setActualValue(const juce::String& id, float value);
    void setStep(int step, bool enabled, int engine, float character, float damage,
                 float probability, float mix, float model);

    juce::AudioProcessorValueTreeState parameters;
    lost_audio::core::SuiteProcessor core;
    std::array<StepRefs, stepCount> stepRefs {};
    std::atomic<float>* enabledRef = nullptr;
    std::atomic<float>* divisionRef = nullptr;
    std::atomic<float>* lengthRef = nullptr;
    std::atomic<float>* swingRef = nullptr;
    std::atomic<float>* inputGainRef = nullptr;
    std::atomic<float>* outputGainRef = nullptr;
    std::atomic<float>* mixRef = nullptr;
    std::atomic<float>* safetyRef = nullptr;
    std::atomic<float>* ceilingRef = nullptr;
    std::atomic<float>* freeRunRef = nullptr;
    std::atomic<float>* internalBpmRef = nullptr;
    std::atomic<float>* seedRef = nullptr;
    std::array<std::atomic<float>, 2> inputMeter {}, outputMeter {};
    std::atomic<int> activeStep { -1 };
    std::atomic<float> stepPhase { 0.0f };
    std::atomic<bool> stepFired { false }, transportRunning { false }, safetyMeter { false };
    std::atomic<float> bpmMeter { 120.0f };
    std::vector<float> crtBed;
    float crtPosition = 0.0f;
    std::array<float, 1024> crtChunk {};
    double freePpq = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LostAudioSequencerProcessor)
};
