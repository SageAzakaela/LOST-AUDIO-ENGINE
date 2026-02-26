#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <vector>

#include "PluginProcessor.h"

class CommsEngineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CommsEngineAudioProcessorEditor(CommsEngineAudioProcessor&);
    ~CommsEngineAudioProcessorEditor() override;

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

    class Switch final : public juce::Component
    {
    public:
        Switch(APVTS& state, const juce::String& id, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& hint) { button.setTooltip(hint); }

    private:
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    void addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint);
    void addChoice(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint);
    void addSwitch(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint);
    void layoutPage(juce::Component& page, int columns);
    void applyPreset(int idx);
    void applyMacroTargets(int mode, float bandwidth, float drive, float glitch, float noise);
    void setParamValue(const juce::String& id, float plainValue);
    float getParamValue(const juce::String& id) const;
    void timerCallback() override;

    CommsEngineAudioProcessor& processor;
    APVTS& apvts;

    juce::Label title;
    juce::Label subtitle;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TooltipWindow tooltipWindow { this, 900 };

    juce::Component macroPage;
    juce::Component corePage;
    juce::Component fxPage;
    juce::Component outputPage;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<Switch>> switches;
    std::unordered_map<juce::Component*, std::vector<juce::Component*>> pageItems;

    bool suppressMacros = false;
    int lastMode = 0;
    float lastBandwidth = 0.4f;
    float lastDrive = 0.35f;
    float lastGlitch = 0.2f;
    float lastNoise = 0.18f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommsEngineAudioProcessorEditor)
};
