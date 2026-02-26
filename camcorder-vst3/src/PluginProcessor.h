#pragma once

#include <JuceHeader.h>
#include <random>
#include <vector>

class CamcorderEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    CamcorderEngineAudioProcessor();
    ~CamcorderEngineAudioProcessor() override = default;

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
        juce::dsp::IIR::Filter<float> hp;
        juce::dsp::IIR::Filter<float> box;
        juce::dsp::IIR::Filter<float> dip;
        juce::dsp::IIR::Filter<float> lp1;
        juce::dsp::IIR::Filter<float> lp2;
    };

    struct State
    {
        float env = 0.0f;
        float limEnv = 0.0f;
        float mufZ = 0.0f;

        float hold = 0.0f;
        int holdCount = 0;
        int holdPeriod = 1;

        int dropRemain = 0;
        int dropTotal = 0;
        float lastGood = 0.0f;
        float dropStart = 0.0f;
        float dropEnd = 0.0f;

        std::vector<float> ring;
        int ri = 0;

        int thumpRemain = 0;
        int thumpTotal = 0;
        float thumpPhase = 0.0f;
        float thumpHz = 55.0f;
        float thumpAmp = 0.0f;

        float rubLp90 = 0.0f;
        float rubLp1800 = 0.0f;

        int chirpRemain = 0;
        int chirpTotal = 0;
        float chirpPhase = 0.0f;
        float chirpF0 = 1200.0f;
        float chirpF1 = 6200.0f;
        float chirpAmp = 0.0f;

        int windRemain = 0;
        int windTotal = 0;
        float windPhase = 0.0f;
        float windAmp = 0.0f;

        int wiggleRemain = 0;
        int wiggleTotal = 0;
        float wigglePhase = 0.0f;
        float wiggleHz = 220.0f;
        float wiggleAmp = 0.0f;

        float hissZ = 0.0f;
    };

    void updateFilters(double sampleRate);
    float nextWhite();

    juce::AudioProcessorValueTreeState apvts;
    FilterChain tone;
    State st;

    std::minstd_rand rng;
    std::uniform_real_distribution<float> unif { 0.0f, 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CamcorderEngineAudioProcessor)
};
