#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <vector>

#include "PluginProcessor.h"

class CartridgeEngineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CartridgeEngineAudioProcessorEditor(CartridgeEngineAudioProcessor&);
    ~CartridgeEngineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS& state, const juce::String& id, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& h) { slider.setTooltip(h); label.setTooltip(h); }

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
        void setHint(const juce::String& h) { button.setTooltip(h); }

    private:
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> att;
    };

    class Choice final : public juce::Component
    {
    public:
        Choice(APVTS& state, const juce::String& id, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& h) { label.setTooltip(h); combo.setTooltip(h); }

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
    void applyMacroQuality(float quality);
    void applyMacroCodec(float codec);
    void applyMacroGrit(float grit);
    void timerCallback() override;

    CartridgeEngineAudioProcessor& processor;
    APVTS& apvts;

    juce::Label title;
    juce::Label subtitle;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TooltipWindow tooltips { this, 900 };

    juce::Component macroPage;
    juce::Component corePage;
    juce::Component vibePage;
    juce::Component bleepPage;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::vector<std::unique_ptr<Choice>> choices;
    std::unordered_map<juce::Component*, std::vector<juce::Component*>> pageItems;

    bool suppressMacros = false;
    float lastQuality = 0.55f;
    float lastCodec = 0.25f;
    float lastGrit = 0.25f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CartridgeEngineAudioProcessorEditor)
};
