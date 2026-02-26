#pragma once

#include <JuceHeader.h>
#include <array>
#include <random>
#include <vector>

class OcclusionEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    OcclusionEngineAudioProcessor();
    ~OcclusionEngineAudioProcessor() override = default;

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
        juce::dsp::IIR::Filter<float> bump;
        juce::dsp::IIR::Filter<float> dip;
        juce::dsp::IIR::Filter<float> lp1;
        juce::dsp::IIR::Filter<float> lp2;
    };

    struct ChannelState
    {
        std::vector<float> predelay;
        int writePos = 0;
    };

    void updateFilters();
    float readPredelay(const ChannelState& st, float delaySamps) const;

    juce::AudioProcessorValueTreeState apvts;

    std::array<FilterChain, 2> filters {};
    std::array<ChannelState, 2> channels {};
    juce::Reverb reverb;

    std::vector<float> filteredL;
    std::vector<float> filteredR;
    std::vector<float> wetL;
    std::vector<float> wetR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OcclusionEngineAudioProcessor)
};
