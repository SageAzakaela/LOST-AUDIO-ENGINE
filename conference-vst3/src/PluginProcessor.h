#pragma once

#include <JuceHeader.h>
#include <random>
#include <vector>

class ConferenceEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    ConferenceEngineAudioProcessor();
    ~ConferenceEngineAudioProcessor() override = default;

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
    struct FilterChain
    {
        juce::dsp::IIR::Filter<float> hp1;
        juce::dsp::IIR::Filter<float> hp2;
        juce::dsp::IIR::Filter<float> dip;
        juce::dsp::IIR::Filter<float> hump;
        juce::dsp::IIR::Filter<float> lp1;
        juce::dsp::IIR::Filter<float> lp2;
    };

    struct CoreState
    {
        std::vector<float> delay;
        int di = 0;
        float jPhase = 0.0f;
        float jNoise = 0.0f;

        std::vector<float> ring;
        int ri = 0;

        float env = 0.0f;
        float gateGain = 1.0f;

        int packetRemain = 0;
        int packetTotal = 0;
        bool inDrop = false;
        float dropFade = 0.0f;
        float lastGood = 0.0f;
        float dropStart = 0.0f;

        float hold = 0.0f;
        int holdPeriod = 1;
        int rateAcc = 0;

        int robotRemain = 0;
        int robotLen = 0;
        int robotI = 0;
        std::vector<float> robotBuf;
    };

    void updateFilters(double sampleRate);
    float readDelay(float delaySamps) const;
    void startPacket(int packetSamps, float lossProb);
    float nextWhite();

    juce::AudioProcessorValueTreeState apvts;
    FilterChain tone;
    CoreState core;

    std::minstd_rand rng;
    std::uniform_real_distribution<float> unif { 0.0f, 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConferenceEngineAudioProcessor)
};
