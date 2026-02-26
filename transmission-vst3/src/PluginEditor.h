#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <vector>

#include "PluginProcessor.h"

class TransmissionEngineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TransmissionEngineAudioProcessorEditor(TransmissionEngineAudioProcessor&);
    ~TransmissionEngineAudioProcessorEditor() override;

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
        juce::Slider& getSlider() { return slider; }

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

    class BoomboxLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        BoomboxLookAndFeel();
    };

    void addKnob(juce::Component& page, const juce::String& paramID, const juce::String& text, const juce::String& hint);
    void addSwitch(juce::Component& page, const juce::String& paramID, const juce::String& text, const juce::String& hint);
    void addChoice(juce::Component& page, const juce::String& paramID, const juce::String& text, const juce::String& hint);
    void layoutPage(juce::Component& page, int columns);
    void applyPreset(int idx);
    void applyMacroBandwidth(float bw);
    void applyMacroDrive(float drive);
    void applyMacroBad(float bad);
    void applyMacroNoise(float noise);
    void setParamValue(const juce::String& id, float plainValue);
    float getParamValue(const juce::String& id) const;
    void timerCallback() override;

    TransmissionEngineAudioProcessor& processor;
    APVTS& apvts;

    BoomboxLookAndFeel boomboxLnf;
    juce::Label title;
    juce::Label subtitle;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TooltipWindow tooltipWindow { this, 900 };

    juce::Component macroPage;
    juce::Component tonePage;
    juce::Component damagePage;
    juce::Component tuningPage;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::vector<std::unique_ptr<Choice>> choices;
    std::unordered_map<juce::Component*, std::vector<juce::Component*>> pageItems;
    std::unordered_map<std::string, Knob*> knobByParam;

    bool suppressMacros = false;
    float lastBandwidth = 0.45f;
    float lastDrive = 0.35f;
    float lastBad = 0.25f;
    float lastNoise = 0.2f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransmissionEngineAudioProcessorEditor)
};
