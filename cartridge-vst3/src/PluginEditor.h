#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
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
    enum class EditorMode { simple, advanced, performer };

    class Panel final : public juce::Component
    {
    public:
        explicit Panel(juce::String title) : name(std::move(title)) {}
        void paint(juce::Graphics&) override;
        juce::Rectangle<int> contentBounds() const { return getLocalBounds().reduced(10).withTrimmedTop(23); }
    private:
        juce::String name;
    };

    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS&, const juce::String&, const juce::String&, std::function<void()>);
        void resized() override;
        void setHint(const juce::String& h) { label.setTooltip(h); slider.setTooltip(h); }
    private:
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    class Choice final : public juce::Component
    {
    public:
        Choice(APVTS&, const juce::String&, const juce::String&, std::function<void()>);
        void resized() override;
        void setHint(const juce::String& h) { label.setTooltip(h); combo.setTooltip(h); }
    private:
        juce::Label label;
        juce::ComboBox combo;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    class Switch final : public juce::Component
    {
    public:
        Switch(APVTS&, const juce::String&, const juce::String&, std::function<void()>);
        void resized() override { button.setBounds(getLocalBounds()); }
        void setHint(const juce::String& h) { button.setTooltip(h); }
    private:
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    class CartridgeDeck final : public juce::Component
    {
    public:
        void setState(float, float, float, float, bool, bool, bool, float, float, int, int, const std::array<float,64>&);
        void paint(juce::Graphics&) override;
    private:
        std::array<float, 2> input {}, output {};
        bool chipActive = false, stallActive = false, bankActive = false;
        float stallProgress = 0, bankProgress = 0;
        std::array<float,64> trace {};
        int codec = 2, speaker = 1;
        float phase = 0.0f;
    };

    Knob* addKnob(Panel&, const juce::String&, const juce::String&, const juce::String&, bool linked = false);
    Choice* addChoice(Panel&, const juce::String&, const juce::String&, const juce::String&, bool linked = false);
    Switch* addSwitch(Panel&, const juce::String&, const juce::String&, const juce::String&, bool linked = false);
    void layoutPanel(Panel&, int);
    void showMode(EditorMode);
    void applyPreset(int);
    void resetParameters();
    void setParameter(const juce::String&, float);
    float getParameter(const juce::String&) const;
    void markCustom();
    void timerCallback() override;

    CartridgeEngineAudioProcessor& processor;
    APVTS& apvts;
    juce::Label brandLabel, titleLabel, subtitleLabel, profileLabel, statusLabel;
    juce::ComboBox presetBox;
    juce::TextButton surfaceButton { "SIMPLE" }, advancedButton { "ADVANCED" }, performerButton { "PERFORMER" };
    juce::Component surfacePage, advancedPage, performerPage;
    CartridgeDeck deck;
    Panel memoryPanel { "MEMORY / DAC" }, playbackPanel { "PLAYBACK HARDWARE" };
    Panel conversionPanel { "MEMORY CONVERSION" }, clockPanel { "CLOCK & BANDWIDTH" }, hardwarePanel { "PLAYBACK BODY" };
    Panel texturePanel { "DAC TEXTURE" }, chipPanel { "CHIP VOICE" }, outputPanel { "SPACE & SAFETY" };
    Panel stallPanel { "ROM STALL / REPEAT" }, bankPanel { "BANK-SWITCH DAMAGE" }, performanceChipPanel { "CHIP VOICE TRIGGER" }, performanceOutputPanel { "SAFE PERFORMANCE" };
    juce::TextButton stallTriggerButton { "TRIGGER ROM STALL" }, bankTriggerButton { "TRIGGER BANK FAULT" }, chipTriggerButton { "FIRE CHIP VOICE" };
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<Switch>> switches;
    std::unordered_map<Panel*, std::vector<juce::Component*>> panelItems;
    std::vector<juce::Component*> linkedControls;
    juce::TooltipWindow tooltipWindow { this, 700 };
    EditorMode currentMode = EditorMode::simple; bool suppressPresetChanges = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CartridgeEngineAudioProcessorEditor)
};
