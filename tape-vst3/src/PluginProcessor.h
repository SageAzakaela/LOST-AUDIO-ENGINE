#pragma once

#include <JuceHeader.h>
#include <array>
#include <random>
#include <string>
#include <vector>

class TapeEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    TapeEngineAudioProcessor();
    ~TapeEngineAudioProcessor() override = default;

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
    enum class SfxMode : int
    {
        bed = 0,
        edges = 1,
        sequence = 2
    };

    struct SfxVoice
    {
        int sampleIndex = -1;
        float pos = 0.0f;
        float step = 1.0f;
        float gain = 0.0f;
        bool active = false;
    };

    struct ChannelState
    {
        std::vector<float> delay;
        int di = 0;
        float wowPhase = 0.0f;
        float flutterPhase = 0.0f;
        float drift = 0.0f;
        float env = 0.0f;
        float limEnv = 0.0f;
        float humPhase = 0.0f;
        float hissZ = 0.0f;
        int dropRemain = 0;
        int dropBlock = 0;
        float dropGain = 1.0f;
        float dropTarget = 1.0f;
    };

    struct ToneState
    {
        juce::dsp::IIR::Filter<float> hp;
        juce::dsp::IIR::Filter<float> bump;
        juce::dsp::IIR::Filter<float> lp1;
        juce::dsp::IIR::Filter<float> lp2;
    };

    float nextWhite();
    float readDelay(const ChannelState& st, float delaySamps) const;
    void updateToneFilters();
    std::vector<float> decodeWavToMono(const void* data, size_t bytes, double targetSampleRate) const;
    void initSfx(double sampleRate);
    void startSfxVoice(int sampleIndex, float gain = 1.0f);
    float processSfxSample(float signalAbs, float sampleRate, float glitch, bool enabled, int bank, SfxMode mode, float level);
    float readEmbeddedSample(int sampleIndex, float pos) const;

    juce::AudioProcessorValueTreeState apvts;

    std::array<ChannelState, 2> chans{};
    std::array<ToneState, 2> tone{};

    std::minstd_rand rng;
    std::uniform_real_distribution<float> unif { 0.0f, 1.0f };

    std::vector<std::vector<float>> sfxSamples;
    std::array<int, 2> bedSampleByBank { 0, 7 };
    std::array<std::vector<int>, 2> startByBank { std::vector<int> { 1, 2, 3 }, std::vector<int> { 8, 9 } };
    std::array<std::vector<int>, 2> endByBank { std::vector<int> { 4, 5, 6 }, std::vector<int> { 4, 10 } };
    std::array<float, 2> bedPos { 0.0f, 0.0f };
    std::array<SfxVoice, 4> sfxVoices {};
    float sfxGateEnv = 0.0f;
    int sfxBelowCount = 0;
    bool sfxInSilence = false;
    int sfxSeqCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeEngineAudioProcessor)
};
