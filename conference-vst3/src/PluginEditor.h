#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <vector>

#include "PluginProcessor.h"

class ConferenceEngineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ConferenceEngineAudioProcessorEditor(ConferenceEngineAudioProcessor&);
    ~ConferenceEngineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS& state, const juce::String& id, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& hint) { slider.setTooltip(hint); label.setTooltip(hint); }

    private:
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    class Choice final : public juce::Component
    {
    public:
        Choice(APVTS& state, const juce::String& id, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& hint) { combo.setTooltip(hint); label.setTooltip(hint); }

    private:
        juce::Label label;
        juce::ComboBox combo;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    void addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint);
    void addChoice(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint);
    void layoutPage(juce::Component& page, int columns);
    void applyPreset(int idx);
    void applyMacroTargets(int mode, float bandwidth, float codec, float dropouts, float jitter, float robot, float noise);
    void setParamValue(const juce::String& id, float plainValue);
    float getParamValue(const juce::String& id) const;
    void timerCallback() override;

    ConferenceEngineAudioProcessor& processor;
    APVTS& apvts;

    juce::Label title;
    juce::Label subtitle;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TooltipWindow tooltipWindow { this, 900 };

    juce::Component macroPage;
    juce::Component corePage;
    juce::Component outputPage;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::unordered_map<juce::Component*, std::vector<juce::Component*>> pageItems;

    bool suppressMacros = false;
    int lastMode = 0;
    float lastBandwidth = 0.45f;
    float lastCodec = 0.35f;
    float lastDropouts = 0.25f;
    float lastJitter = 0.2f;
    float lastRobot = 0.12f;
    float lastNoise = 0.12f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConferenceEngineAudioProcessorEditor)
};
