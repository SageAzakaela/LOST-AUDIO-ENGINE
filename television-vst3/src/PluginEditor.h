#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <vector>

#include "PluginProcessor.h"

class TelevisionEngineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TelevisionEngineAudioProcessorEditor(TelevisionEngineAudioProcessor&);
    ~TelevisionEngineAudioProcessorEditor() override;

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
    void addSwitch(juce::Component& page, const juce::String& id, const juce::String& text, const juce::String& hint);
    void layoutPage(juce::Component& page, int columns);

    void setParamValue(const juce::String& id, float plain);
    float getParamValue(const juce::String& id) const;
    void applyMacroVibe(float v);
    void applyMacroSpeaker(float s);
    void applyMacroAgc(float a);
    void applyMacroStatic(float st);
    void applyPreset(int idx);
    void timerCallback() override;

    TelevisionEngineAudioProcessor& processor;
    APVTS& apvts;

    juce::Label title;
    juce::Label subtitle;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TooltipWindow tooltipWindow { this, 900 };

    juce::Component macroPage;
    juce::Component tonePage;
    juce::Component noisePage;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::unordered_map<juce::Component*, std::vector<juce::Component*>> pageItems;

    bool suppressMacros = false;
    float lastVibe = 0.45f;
    float lastSpeaker = 0.55f;
    float lastAgc = 0.22f;
    float lastStatic = 0.12f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TelevisionEngineAudioProcessorEditor)
};
