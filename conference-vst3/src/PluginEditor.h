#pragma once

#include <JuceHeader.h>

#include <array>
#include <functional>
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
    enum class EditorMode { simple, advanced, performer };

    class Panel final : public juce::Component
    {
    public:
        explicit Panel(juce::String title) : name(std::move(title)) {}
        void paint(juce::Graphics&) override;
        juce::Rectangle<int> contentBounds() const { return getLocalBounds().reduced(9).withTrimmedTop(22); }
    private: juce::String name;
    };
    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS&, const juce::String&, const juce::String&, std::function<void()>);
        void resized() override;
        void setHint(const juce::String& hint) { label.setTooltip(hint); slider.setTooltip(hint); }
    private: juce::Label label; juce::Slider slider; std::unique_ptr<APVTS::SliderAttachment> attachment;
    };
    class Choice final : public juce::Component
    {
    public:
        Choice(APVTS&, const juce::String&, const juce::String&, std::function<void()>);
        void resized() override;
        void setHint(const juce::String& hint) { label.setTooltip(hint); combo.setTooltip(hint); }
    private: juce::Label label; juce::ComboBox combo; std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };
    class Switch final : public juce::Component
    {
    public:
        Switch(APVTS&, const juce::String&, const juce::String&, std::function<void()>);
        void resized() override;
        void setHint(const juce::String& hint) { button.setTooltip(hint); }
    private: juce::ToggleButton button; std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };
    class CallDisplay final : public juce::Component
    {
    public:
        void setState(const std::array<float, 64>&, float, float, float, float, bool, bool, bool, bool,
                      float, float, float, float, float, float, float, int);
        void paint(juce::Graphics&) override;
    private:
        std::array<float, 64> waveform {};
        std::array<float, 2> input {}, output {};
        bool lost = false, robot = false, slip = false, narrow = false;
        float lossProgress = 0.0f, robotProgress = 0.0f, jitter = 0.0f, suppression = 0.0f;
        float agc = 0.0f, comfort = 0.0f, limiter = 0.0f;
        int platform = 0;
    };

    Knob* addKnob(Panel&, const juce::String&, const juce::String&, const juce::String&, bool canonical = true);
    Choice* addChoice(Panel&, const juce::String&, const juce::String&, const juce::String&, bool canonical = true);
    Switch* addSwitch(Panel&, const juce::String&, const juce::String&, const juce::String&);
    void layoutPanel(Panel&, int columns);
    void showMode(EditorMode);
    void applyPreset(int);
    void resetParameters();
    void setParameter(const juce::String&, float);
    float getParameter(const juce::String&) const;
    void markCustom(bool canonical);
    void timerCallback() override;

    ConferenceEngineAudioProcessor& processor;
    APVTS& apvts;
    juce::Label brandLabel, titleLabel, subtitleLabel, profileLabel, statusLabel;
    juce::ComboBox presetBox;
    juce::TextButton simpleButton { "SIMPLE" }, advancedButton { "ADVANCED" }, performerButton { "PERFORMER" };
    juce::Component simplePage, advancedPage, performerPage;
    CallDisplay callDisplay;
    Panel simpleCallPanel { "CALL MODEL" }, simpleOutputPanel { "RETURN PATH" };
    Panel tonePanel { "VOICE BAND" }, packetPanel { "PACKET CONCEALMENT" }, bufferPanel { "JITTER BUFFER" };
    Panel cleanupPanel { "VOICE CLEANUP" }, codecPanel { "CODEC" }, safetyPanel { "OUTPUT SAFETY" };
    Panel performerPacketPanel { "CLOCKED PACKET FAILURE" }, performerRobotPanel { "ROBOTIC GRAIN CAPTURE" };
    Panel performerClockPanel { "JITTER CLOCK" }, performerCleanupPanel { "CALL CLEANUP" }, performerOutputPanel { "SAFE RETURN" };
    juce::TextButton packetTriggerButton { "TRIGGER LOSS" }, robotTriggerButton { "CAPTURE ROBOT" };
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<Switch>> switches;
    std::unordered_map<Panel*, std::vector<juce::Component*>> panelItems;
    juce::TooltipWindow tooltipWindow { this, 700 };
    EditorMode currentMode = EditorMode::simple;
    bool suppressPresetChanges = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConferenceEngineAudioProcessorEditor)
};
