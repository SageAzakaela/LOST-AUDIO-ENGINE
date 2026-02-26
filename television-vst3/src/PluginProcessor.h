#pragma once

#include <JuceHeader.h>
#include <array>
#include <random>
#include <vector>

class TelevisionEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    TelevisionEngineAudioProcessor();
    ~TelevisionEngineAudioProcessor() override = default;

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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    struct ChannelState
    {
        float hpState = 0.0f;
        float prevWhite = 0.0f;
        float lpState = 0.0f;
        int crackleHold = 0;
        float crackleAmp = 0.0f;
        float humPhase = 0.0f;
        float whinePhase = 0.0f;
        float compEnv = 0.0f;
        float limEnv = 0.0f;
    };

    struct ToneState
    {
        juce::dsp::IIR::Filter<float> hp1;
        juce::dsp::IIR::Filter<float> hp2;
        juce::dsp::IIR::Filter<float> dip;
        juce::dsp::IIR::Filter<float> hump;
        juce::dsp::IIR::Filter<float> lp1;
        juce::dsp::IIR::Filter<float> lp2;
    };

    struct Runtime
    {
        float vibe = 0.45f;
        float speaker = 0.55f;
        float agc = 0.22f;
        float statik = 0.12f;
        float hum = 0.18f;
        float whine = 0.08f;

        float hpHz = 70.0f;
        float lpHz = 9000.0f;
        float midHumpDb = 1.2f;
        float midFreq = 1800.0f;
        float noiseHiss = 0.55f;
        float noiseCrackle = 0.08f;

        bool bedEnable = false;
        float bedLevel = 0.22f;
        float outGain = 1.0f;
    };

    void updateToneFilters();
    void applyMacro(Runtime& r) const;
    float nextWhite();

    std::vector<float> decodeMp3ToMono(const void* data, size_t bytes, double targetSampleRate) const;
    void initBed(double sampleRate);
    float readBedSample(float pos) const;

    juce::AudioProcessorValueTreeState apvts;
    std::array<ChannelState, 2> chans{};
    std::array<ToneState, 2> tone{};

    std::minstd_rand rng;
    std::uniform_real_distribution<float> unif { 0.0f, 1.0f };

    std::vector<float> bedSample;
    float bedPos = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TelevisionEngineAudioProcessor)
};
