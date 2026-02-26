#pragma once

#include <JuceHeader.h>
#include <random>
#include <vector>

class CartridgeEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    CartridgeEngineAudioProcessor();
    ~CartridgeEngineAudioProcessor() override = default;

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
        juce::dsp::IIR::Filter<float> lpPre;
        juce::dsp::IIR::Filter<float> hpPost;
        juce::dsp::IIR::Filter<float> dip;
        juce::dsp::IIR::Filter<float> hump;
        juce::dsp::IIR::Filter<float> spLp1;
        juce::dsp::IIR::Filter<float> spLp2;
    };

    struct Bleep
    {
        bool active = false;
        int remain = 0;
        int total = 0;
        float phase = 0.0f;
        float vibPhase = 0.0f;
        float freq = 440.0f;
        float vibRate = 6.0f;
        float vibDepth = 0.0f;
        int wave = 1;
        float duty = 0.5f;
        float amp = 0.25f;
    };

    struct State
    {
        float hold = 0.0f;
        int holdCount = 0;
        int holdPeriod = 1;

        float blockHold = 0.0f;
        int blockRemain = 0;
        int blockTotal = 0;

        float preEmphZ = 0.0f;
        float nsErr = 0.0f;
        float env = 0.0f;
        float dc = 0.0f;
        float humPhase = 0.0f;
        float whinePhase = 0.0f;
        float limEnv = 0.0f;

        std::vector<float> delay;
        int delayIndex = 0;

        std::vector<float> c1;
        std::vector<float> c2;
        std::vector<float> c3;
        int ci1 = 0;
        int ci2 = 0;
        int ci3 = 0;
        float c1lp = 0.0f;
        float c2lp = 0.0f;
        float c3lp = 0.0f;

        std::vector<float> ap1;
        std::vector<float> ap2;
        int api1 = 0;
        int api2 = 0;

        int samplesToNextBleep = 0;
        Bleep bleep;
    };

    static float clampf(float x, float lo, float hi);
    static float softClip(float x);
    static float mulawEncode(float x, float mu = 255.0f);
    static float mulawDecode(float y, float mu = 255.0f);

    void resetState(double sampleRate);
    void updateToneFilters(double sampleRate);
    float nextWhite();
    float nextSigned();

    float bleepEnv(float t) const;
    float bleepOsc(float phase, int wave, float duty) const;
    void triggerBleep(float pitch, float vibrato, int waveSel, double sr);
    float processBleep(double sr, bool enable, float mix, float rate, int waveSel, float vibrato, float pitch);

    float combProcess(std::vector<float>& buf, int& idx, float input, int delaySamps, float fb, float damp, float& lpState);
    float allpassProcess(std::vector<float>& buf, int& idx, float input, int delaySamps, float g);

    juce::AudioProcessorValueTreeState apvts;
    Tone tone;
    State st;

    std::minstd_rand rng;
    std::uniform_real_distribution<float> unif { 0.0f, 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CartridgeEngineAudioProcessor)
};
