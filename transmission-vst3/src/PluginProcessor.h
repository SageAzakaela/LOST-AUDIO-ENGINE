#pragma once

#include <JuceHeader.h>
#include <array>
#include <random>
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
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    static constexpr int kMaxPasses = 6;

    struct PinkNoiseState
    {
        float p0 = 0.0f;
        float p1 = 0.0f;
        float p2 = 0.0f;
        float p3 = 0.0f;
        float p4 = 0.0f;
        float p5 = 0.0f;
        float p6 = 0.0f;
    };

    struct ChannelState
    {
        float env = 0.0f;
        float crushHold = 0.0f;
        int crushPhase = 0;
        float prevNoise = 0.0f;
        PinkNoiseState pink;
    };

    struct FilterChain
    {
        juce::dsp::IIR::Filter<float> hp1;
        juce::dsp::IIR::Filter<float> hp2;
        juce::dsp::IIR::Filter<float> lp1;
        juce::dsp::IIR::Filter<float> lp2;
        juce::dsp::IIR::Filter<float> dip;
        juce::dsp::IIR::Filter<float> mid;
    };

    void updateFilters(double sampleRate);
    struct RuntimeParams
    {
        float drive = 0.35f;
        float asym = 0.1f;
        float comp = 0.25f;
        float crush = 0.0f;
        float wowDepth = 0.25f;
        float dropRate = 0.25f;
        float dropDepth = 0.35f;
        float crackle = 0.25f;
        float lfoRate = 0.7f;
        float noiseProfile = 0.2f;
        float noiseColor = 0.0f;
        float hiss = 0.2f;
        float outGain = 0.92f;
        bool tuningEnable = false;
        int tuningMode = 0;
        int tuningSource = 0;
        float tuningAmount = 0.35f;
        float tuningSnippetMs = 140.0f;
        float tuningCutDepth = 0.55f;
        int passes = 1;
    };

    float processChannelSample(int passIndex, int channel, float x, double sampleRate, const RuntimeParams& params, bool includeEvents);
    float nextWhite();
    float pinkFromWhite(PinkNoiseState& state, float white) const;
    void maybeTriggerDropout(float sampleRate, float dropRate, float dropDepth);
    void maybeTriggerCrackle(float sampleRate, float crackleAmount);
    void triggerTuningEvent(float sampleRate, float snippetMs, int sourceMode);
    float sampleEmbeddedTuning(int sampleIndex, float position) const;
    void initEmbeddedTuningSamples(double sampleRate);
    float nextTuningSample(double sampleRate, const RuntimeParams& params, bool isPlaying, float& duckOut);

    juce::AudioProcessorValueTreeState apvts;
    std::array<std::array<ChannelState, 2>, kMaxPasses> channelStates{};
    std::array<std::array<FilterChain, 2>, kMaxPasses> filters{};

    std::minstd_rand rng;
    std::uniform_real_distribution<float> unif { 0.0f, 1.0f };

    float lfoPhase = 0.0f;

    int dropoutRemaining = 0;
    int dropoutTotal = 0;
    float dropoutDepth = 1.0f;

    int crackleRemaining = 0;
    int crackleTotal = 0;

    int tuningRemaining = 0;
    int tuningTotal = 0;
    float tuningF0 = 1200.0f;
    float tuningF1 = 3200.0f;
    float tuningPhase = 0.0f;
    float tuningPlayPos = 0.0f;
    float tuningPlayStep = 1.0f;
    int tuningSampleIndex = 0;
    int tuningCooldownRemaining = 0;
    bool transportWasPlaying = false;
    std::vector<std::vector<float>> embeddedTuningSamples;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransmissionEngineAudioProcessor)
};
