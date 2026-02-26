#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <vector>

#include "PluginProcessor.h"

class OpenMicNightAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OpenMicNightAudioProcessorEditor(OpenMicNightAudioProcessor&);
    ~OpenMicNightAudioProcessorEditor() override;

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
        std::unique_ptr<APVTS::SliderAttachment> att;
    };

    class Switch final : public juce::Component
    {
    public:
        Switch(APVTS& state, const juce::String& id, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& hint) { button.setTooltip(hint); }

    private:
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> att;
    };

    class Choice final : public juce::Component
    {
    public:
        Choice(APVTS& state, const juce::String& id, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& hint) { label.setTooltip(hint); combo.setTooltip(hint); }

    private:
        juce::Label label;
        juce::ComboBox combo;
        std::unique_ptr<APVTS::ComboBoxAttachment> att;
    };

    void addKnob(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint);
    void addSwitch(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint);
    void addChoice(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint);
    void layoutPage(juce::Component& page, int columns);

    void setParamValue(const juce::String& id, float plainValue);
    float getParamValue(const juce::String& id) const;
    void applyPreset(int idx);
    void applyMacroIntensity(float v);
    void applyMacroDistance(float v);
    void timerCallback() override;

    OpenMicNightAudioProcessor& processor;
    APVTS& apvts;

    juce::Label title;
    juce::Label subtitle;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TooltipWindow tooltips { this, 900 };

    juce::Component macroPage;
    juce::Component feedbackPage;
    juce::Component spacePage;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::vector<std::unique_ptr<Choice>> choices;
    std::unordered_map<juce::Component*, std::vector<juce::Component*>> pageItems;

    bool suppressMacros = false;
    float lastIntensity = 0.55f;
    float lastDistance = 0.65f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMicNightAudioProcessorEditor)
};

