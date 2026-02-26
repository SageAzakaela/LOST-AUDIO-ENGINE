#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <vector>

#include "PluginProcessor.h"

class CDEngineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CDEngineAudioProcessorEditor(CDEngineAudioProcessor&);
    ~CDEngineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS& state, const juce::String& paramID, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& hint) { slider.setTooltip(hint); label.setTooltip(hint); }

    private:
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    class Switch final : public juce::Component
    {
    public:
        Switch(APVTS& state, const juce::String& paramID, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& hint) { button.setTooltip(hint); }

    private:
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    class Choice final : public juce::Component
    {
    public:
        Choice(APVTS& state, const juce::String& paramID, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& hint) { combo.setTooltip(hint); label.setTooltip(hint); }

    private:
        juce::Label label;
        juce::ComboBox combo;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    void addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint);
    void addSwitch(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint);
    void addChoice(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint);
    void layoutPage(juce::Component& page, int columns);
    void applyPreset(int idx);
    void applyMacroClarity(float clarity);
    void applyMacroDamage(float damage);
    void applyMacroTracking(float tracking);
    void applyMacroJitter(float jitter);
    void applyMacroTargets(float clarity, float damage, float tracking, float jitter);
    void setParamValue(const juce::String& id, float plainValue);
    float getParamValue(const juce::String& id) const;
    void timerCallback() override;

    CDEngineAudioProcessor& processor;
    APVTS& apvts;

    juce::Label title;
    juce::Label subtitle;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TooltipWindow tooltipWindow { this, 900 };

    juce::Component macroPage;
    juce::Component errorsPage;
    juce::Component mechanicsPage;
    juce::Component outputPage;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::vector<std::unique_ptr<Choice>> choices;
    std::unordered_map<juce::Component*, std::vector<juce::Component*>> pageItems;

    bool suppressMacros = false;
    float lastClarity = 0.65f;
    float lastDamage = 0.25f;
    float lastTracking = 0.22f;
    float lastJitter = 0.18f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CDEngineAudioProcessorEditor)
};
