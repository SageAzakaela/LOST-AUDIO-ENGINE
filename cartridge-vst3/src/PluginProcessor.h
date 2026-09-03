#pragma once
#include <JuceHeader.h>
#include <lost_audio/core/CartridgeProcessor.h>
#include <lost_audio/core/TempoSync.h>
#include <array>
#include <atomic>
#include <vector>

class CartridgeEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    CartridgeEngineAudioProcessor(); ~CartridgeEngineAudioProcessor() override=default;
    void prepareToPlay(double,int) override; void releaseResources() override; bool isBusesLayoutSupported(const BusesLayout&) const override; void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override; bool hasEditor() const override; const juce::String getName() const override; bool acceptsMidi() const override; bool producesMidi() const override; bool isMidiEffect() const override; double getTailLengthSeconds() const override;
    int getNumPrograms() override; int getCurrentProgram() override; void setCurrentProgram(int) override; const juce::String getProgramName(int) override; void changeProgramName(int,const juce::String&) override;
    void getStateInformation(juce::MemoryBlock&) override; void setStateInformation(const void*,int) override;
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept{return apvts;}
    void materialiseLegacyMacros(); [[nodiscard]] bool legacyMacrosActive()const noexcept;
    void triggerRomStall()noexcept{pendingStall.store(true,std::memory_order_release);}void triggerBankFault()noexcept{pendingBank.store(true,std::memory_order_release);}void triggerChipVoice()noexcept{pendingBleep.store(true,std::memory_order_release);}
    float inputPeak(int ch)const noexcept{return inputPeaks[(std::size_t)juce::jlimit(0,1,ch)].load();}float outputPeak(int ch)const noexcept{return outputPeaks[(std::size_t)juce::jlimit(0,1,ch)].load();}bool bleepActive()const noexcept{return bleepState.load();}bool stallActive()const noexcept{return stallState.load();}bool bankFaultActive()const noexcept{return bankState.load();}
    float stallProgress()const noexcept{return stallProgressMeter.load();}float bankFaultProgress()const noexcept{return bankProgressMeter.load();}std::array<float,64> outputTrace()const noexcept;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
private:
    lost_audio::core::CartridgeParameters readParameters() const noexcept;
    float value(const char*)const noexcept;float syncedMilliseconds(const char*,const char*,double)const noexcept;
    juce::AudioProcessorValueTreeState apvts; lost_audio::core::CartridgeProcessor core;
    std::array<std::atomic<float>,2> inputPeaks{0.0f,0.0f},outputPeaks{0.0f,0.0f};std::array<std::atomic<float>,64> trace{};std::atomic<bool> bleepState{false},stallState{false},bankState{false};std::atomic<float> stallProgressMeter{0},bankProgressMeter{0};
    std::atomic<bool> pendingStall{false},pendingBank{false},pendingBleep{false};double currentBpm=120.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CartridgeEngineAudioProcessor)
};
