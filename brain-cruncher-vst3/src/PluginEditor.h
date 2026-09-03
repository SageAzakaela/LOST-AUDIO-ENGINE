#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "PluginProcessor.h"

class BrainCruncherAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BrainCruncherAudioProcessorEditor(BrainCruncherAudioProcessor&);
    ~BrainCruncherAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    class Panel final : public juce::Component
    {
    public:
        explicit Panel(juce::String title) : name(std::move(title)) {}
        void paint(juce::Graphics&) override;
        juce::Rectangle<int> contentBounds() const { return getLocalBounds().reduced(11).withTrimmedTop(24); }
    private:
        juce::String name;
    };

    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS&, const juce::String& id, const juce::String& title, std::function<void()> changed);
        void resized() override;
        void setHint(const juce::String& text);
    private:
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    class NeuralScope final : public juce::Component
    {
    public:
        void update(float input, float output, float motion, float crunch, float width, bool rattle);
        void paint(juce::Graphics&) override;
    private:
        float in = 0.0f, out = 0.0f, movement = 0.0f, amount = 0.86f, stereo = 0.78f, phase = 0.0f;
        bool hardware = false;
    };

    Knob* addKnob(Panel&, const char* id, const char* title, const char* hint);
    void layout(Panel&, int columns);
    void showAdvanced(bool);
    void applyPreset(int index);
    void setParameter(const char* id, float plainValue);
    float getParameter(const char* id) const;
    void markCustom();
    void timerCallback() override;

    BrainCruncherAudioProcessor& processor;
    APVTS& apvts;
    juce::Label brand, title, subtitle, profile, status;
    juce::ComboBox presets;
    juce::TextButton surfaceButton { "PLAYGROUND" }, advancedButton { "CIRCUIT" };
    juce::Component surfacePage, advancedPage;
    NeuralScope scope;
    Panel character { "CRUNCH CHARACTER" }, dimension { "STEREO DIMENSION" }, output { "SIGNAL SAFETY" };
    Panel metal { "METAL EXCITATION" }, phasePanel { "COMB + PHASE" }, utility { "INPUT / OUTPUT" };
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unordered_map<Panel*, std::vector<juce::Component*>> panelItems;
    juce::TooltipWindow tooltips { this, 650 };
    bool advancedVisible = false;
    bool suppressPresetChange = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrainCruncherAudioProcessorEditor)
};
