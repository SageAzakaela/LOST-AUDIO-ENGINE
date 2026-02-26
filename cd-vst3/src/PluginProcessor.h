#pragma once

#include <JuceHeader.h>
#include <array>
#include <random>
#include <vector>

class CDEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    CDEngineAudioProcessor();
    ~CDEngineAudioProcessor() override = default;

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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct CoreState
    {
        std::vector<float> delay;
        int di = 0;

        std::vector<float> ring;
        int ri = 0;

        float jPhase = 0.0f;
        float jNoise = 0.0f;

        int errRemain = 0;
        int errTotal = 0;
        float lastGood = 0.0f;
        float errStart = 0.0f;
        float errEnd = 0.0f;

        int clickRemain = 0;
        int clickTotal = 0;
        float clickAmp = 0.0f;
        float clickSign = 1.0f;

        float servoPhaseA = 0.0f;
        float servoPhaseB = 0.0f;

        float hfZ = 0.0f;
        float limEnv = 0.0f;
    };

    float nextWhite();
    float readDelay(float delaySamps) const;

    juce::AudioProcessorValueTreeState apvts;

    CoreState core;
    std::minstd_rand rng;
    std::uniform_real_distribution<float> unif { 0.0f, 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CDEngineAudioProcessor)
};
