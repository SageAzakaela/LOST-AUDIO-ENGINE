#pragma once

#include <JuceHeader.h>
#include <random>
#include <vector>

class OpenMicNightAudioProcessor final : public juce::AudioProcessor
{
public:
    OpenMicNightAudioProcessor();
    ~OpenMicNightAudioProcessor() override = default;

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
    struct Tone
    {
        juce::dsp::IIR::Filter<float> hp;
        juce::dsp::IIR::Filter<float> wallLp1;
        juce::dsp::IIR::Filter<float> wallLp2;
        juce::dsp::IIR::Filter<float> wallDip;
        juce::dsp::IIR::Filter<float> fbBand;
        juce::dsp::IIR::Filter<float> fbTone;
    };

    struct CrowdPlayer
    {
        std::vector<float> samples;
        float readPos = 0.0f;
        bool valid() const { return !samples.empty(); }
        float readLoop() noexcept;
    };

    static float clampf(float x, float lo, float hi);
    static float softClip(float x);

    void updateFilters(double sampleRate);
    void loadCrowdAssets(double sampleRate);
    void loadClipFromBinary(const void* data, size_t size, CrowdPlayer& out, double targetSampleRate);
    float nextSigned();

    juce::AudioProcessorValueTreeState apvts;
    Tone tone;

    std::array<CrowdPlayer, 3> crowdBeds;
    CrowdPlayer banterClip;
    CrowdPlayer introClip;
    CrowdPlayer applauseClip;

    std::vector<float> fbDelay;
    int fbWrite = 0;

    juce::Reverb reverb;

    float limEnv = 0.0f;
    float inputEnv = 0.0f;
    bool gateOpen = false;
    int introCooldown = 0;

    int introRemain = 0;
    int introTotal = 0;
    float introGain = 0.0f;
    float introPos = 0.0f;

    int banterRemain = 0;
    int banterTotal = 0;
    float banterGain = 0.0f;
    float banterPos = 0.0f;

    int applauseRemain = 0;
    int applauseTotal = 0;
    float applauseGain = 0.0f;
    float applausePos = 0.0f;
    int applauseCooldown = 0;

    std::minstd_rand rng;
    std::uniform_real_distribution<float> unif { 0.0f, 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMicNightAudioProcessor)
};
