#pragma once

#include <JuceHeader.h>
#include <random>
#include <vector>

class CommsEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    CommsEngineAudioProcessor();
    ~CommsEngineAudioProcessor() override = default;

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
    struct MonoCoreState
    {
        float env = 0.0f;
        float limEnv = 0.0f;

        float hold = 0.0f;
        int holdCount = 0;
        int holdPeriod = 1;

        int dropRemain = 0;
        int dropBlockRemain = 0;
        float dropTarget = 1.0f;
        float dropGain = 1.0f;

        float humPhase = 0.0f;
        float tonePhase = 0.0f;
        float tonePhase2 = 0.0f;
        float warblePhase = 0.0f;
    };

    struct EchoState
    {
        std::vector<float> delay;
        int writePos = 0;
        float fbTone = 0.0f;
    };

    struct FilterChain
    {
        juce::dsp::IIR::Filter<float> hp1;
        juce::dsp::IIR::Filter<float> hp2;
        juce::dsp::IIR::Filter<float> dip;
        juce::dsp::IIR::Filter<float> hump;
        juce::dsp::IIR::Filter<float> lp1;
        juce::dsp::IIR::Filter<float> lp2;
    };

    void updateFilters(double sampleRate);
    float readEcho(float delaySamps) const;
    float nextWhite();

    juce::AudioProcessorValueTreeState apvts;

    FilterChain tone;
    MonoCoreState core;
    EchoState echo;
    juce::Reverb reverb;

    std::vector<float> verbL;
    std::vector<float> verbR;

    std::minstd_rand rng;
    std::uniform_real_distribution<float> unif { 0.0f, 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommsEngineAudioProcessor)
};
