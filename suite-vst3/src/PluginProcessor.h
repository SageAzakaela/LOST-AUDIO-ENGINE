#pragma once

#include <JuceHeader.h>
#include <lost_audio/core/SuiteProcessor.h>

#include <array>
#include <atomic>
#include <vector>

class LostAudioSuiteProcessor final : public juce::AudioProcessor
{
public:
    LostAudioSuiteProcessor();
    ~LostAudioSuiteProcessor() override = default;
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
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState& state() noexcept { return parameters; }
    float inputPeak(int channel) const noexcept { return inputMeter[static_cast<std::size_t>(juce::jlimit(0, 1, channel))].load(); }
    float outputPeak(int channel) const noexcept { return outputMeter[static_cast<std::size_t>(juce::jlimit(0, 1, channel))].load(); }
    float cpuLoad() const noexcept { return cpuMeter.load(); }
    float topologyGain() const noexcept { return topologyMeter.load(); }
    bool safetyEngaged() const noexcept { return safetyMeter.load(); }

private:
    struct SlotRefs
    {
        std::atomic<float>* engine = nullptr; std::atomic<float>* bypass = nullptr; std::atomic<float>* mix = nullptr;
        std::atomic<float>* macroA = nullptr; std::atomic<float>* macroB = nullptr; std::atomic<float>* model = nullptr;
        std::array<std::atomic<float>*, 6> detail {};
        std::atomic<float>* g1a = nullptr; std::atomic<float>* g1b = nullptr; std::atomic<float>* g2a = nullptr; std::atomic<float>* g2b = nullptr;
        std::atomic<float>* feedbackArm = nullptr;
    };

    static juce::String slotId(int slot, const char* suffix);
    void cacheParameterPointers();
    lost_audio::core::SuiteParameters readParameters() const noexcept;
    std::vector<float> decodeCrtBed(double sampleRate) const;
    float readCrtBed() noexcept;

    juce::AudioProcessorValueTreeState parameters;
    lost_audio::core::SuiteProcessor core;
    std::array<SlotRefs, lost_audio::core::SuiteParameters::slotCount> slotRefs {};
    std::array<std::atomic<float>*, lost_audio::core::SuiteParameters::slotCount> orderRefs {};
    std::atomic<float>* inputGainRef = nullptr; std::atomic<float>* outputGainRef = nullptr; std::atomic<float>* mixRef = nullptr;
    std::atomic<float>* limiterRef = nullptr; std::atomic<float>* ceilingRef = nullptr; std::atomic<float>* global1Ref = nullptr; std::atomic<float>* global2Ref = nullptr;
    std::array<std::atomic<float>, 2> inputMeter {}, outputMeter {};
    std::atomic<float> cpuMeter { 0.0f }, topologyMeter { 1.0f };
    std::atomic<bool> safetyMeter { false };
    std::vector<float> crtBed;
    float crtPosition = 0.0f;
    std::array<float, 1024> crtChunk {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LostAudioSuiteProcessor)
};
