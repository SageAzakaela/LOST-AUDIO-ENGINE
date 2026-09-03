#pragma once

#include <JuceHeader.h>
#include <functional>
#include <unordered_map>
#include <vector>

#include "PluginProcessor.h"

class CamcorderEngineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CamcorderEngineAudioProcessorEditor(CamcorderEngineAudioProcessor&);
    ~CamcorderEngineAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using APVTS = juce::AudioProcessorValueTreeState;
    enum class EditorMode { simple, advanced, performer };
    class Panel final : public juce::Component
    {
    public: explicit Panel(juce::String title) : name(std::move(title)) {} void paint(juce::Graphics&) override;
        juce::Rectangle<int> contentBounds() const { return getLocalBounds().reduced(10).withTrimmedTop(23); }
    private: juce::String name;
    };
    class Knob final : public juce::Component
    {
    public: Knob(APVTS&, const juce::String&, const juce::String&, std::function<void()>); void resized() override;
        void setHint(const juce::String& h) { label.setTooltip(h); slider.setTooltip(h); }
    private: juce::Label label; juce::Slider slider; std::unique_ptr<APVTS::SliderAttachment> attachment;
    };
    class Choice final : public juce::Component
    {
    public: Choice(APVTS&, const juce::String&, const juce::String&, std::function<void()>); void resized() override;
        void setHint(const juce::String& h) { label.setTooltip(h); combo.setTooltip(h); }
    private: juce::Label label; juce::ComboBox combo; std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };
    class Switch final : public juce::Component
    {
    public: Switch(APVTS&, const juce::String&, const juce::String&, std::function<void()>); void resized() override { button.setBounds(getLocalBounds()); }
        void setHint(const juce::String& h) { button.setTooltip(h); }
    private: juce::ToggleButton button; std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };
    class Viewfinder final : public juce::Component
    {
    public: void setState(const std::array<float, 64>&, float, float, float, float, bool, bool, bool, bool, bool,
                          float, float, float, float, float, float, float, float, float, int); void paint(juce::Graphics&) override;
    private: std::array<float, 64> waveform {}; std::array<float, 2> input {}, output {}; bool wind = false, handling = false, dropout = false, corrupt = false, enabledWind = false;
        float dropProgress = 0, faultProgress = 0, handlingProgress = 0, windProgress = 0, agc = 0, flutter = 0, limiter = 0, cameraBed = 0, windBed = 0; int format = 2;
    };

    Knob* addKnob(Panel&, const juce::String&, const juce::String&, const juce::String&, bool linked = false);
    Choice* addChoice(Panel&, const juce::String&, const juce::String&, const juce::String&, bool linked = false);
    Switch* addSwitch(Panel&, const juce::String&, const juce::String&, const juce::String&, bool linked = false);
    void layoutPanel(Panel&, int); void showMode(EditorMode); void applyPreset(int); void resetParameters();
    void setParameter(const juce::String&, float); float getParameter(const juce::String&) const; void markCustom(bool canonical = true); void timerCallback() override;

    CamcorderEngineAudioProcessor& processor; APVTS& apvts;
    juce::Label brandLabel, titleLabel, subtitleLabel, profileLabel, statusLabel; juce::ComboBox presetBox;
    juce::TextButton surfaceButton { "SIMPLE" }, advancedButton { "ADVANCED" }, performerButton { "PERFORMER" }; juce::Component surfacePage, advancedPage, performerPage; Viewfinder viewfinder;
    Panel capturePanel { "CAPTURE PROFILE" }, scenePanel { "ON-CAMERA SOUND" };
    Panel tonePanel { "CAMERA MICROPHONE" }, agcPanel { "AUTO LEVEL" }, converterPanel { "FORMAT TRANSPORT" };
    Panel damagePanel { "SIGNAL DAMAGE" }, movementPanel { "BODY & MOVEMENT" }, outputPanel { "OUTPUT SAFETY" };
    Panel performerDropPanel { "CLOCKED CAPTURE LOSS" }, performerFaultPanel { "CODEC FAULT" }, performerHandlingPanel { "CAMERA IMPACT" };
    Panel performerBedPanel { "ENVIRONMENT BEDS" }, performerOutputPanel { "SAFE PLAYBACK" };
    juce::TextButton dropTriggerButton { "TRIGGER LOSS" }, faultTriggerButton { "TRIGGER FAULT" }, handlingTriggerButton { "HIT CAMERA" };
    std::vector<std::unique_ptr<Knob>> knobs; std::vector<std::unique_ptr<Choice>> choices; std::vector<std::unique_ptr<Switch>> switches;
    std::unordered_map<Panel*, std::vector<juce::Component*>> panelItems;
    juce::TooltipWindow tooltipWindow { this, 700 }; EditorMode currentMode = EditorMode::simple; bool suppressPresetChanges = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CamcorderEngineAudioProcessorEditor)
};
