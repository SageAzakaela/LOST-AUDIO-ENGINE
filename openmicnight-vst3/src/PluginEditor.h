#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

class OpenMicNightAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit OpenMicNightAudioProcessorEditor(OpenMicNightAudioProcessor&);
    ~OpenMicNightAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    enum class EditorMode { simple, advanced, performer };

    class NightLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        NightLookAndFeel();
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    };

    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS&, const juce::String& parameterId, const juce::String& title, const juce::String& suffix, std::function<void()> changed);
        void resized() override;
        juce::String id;
    private:
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    class Choice final : public juce::Component
    {
    public:
        Choice(APVTS&, const juce::String& parameterId, const juce::String& title, std::function<void()> changed = {});
        void resized() override;
    private:
        juce::Label label;
        juce::ComboBox box;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    class StageView final : public juce::Component
    {
    public:
        StageView(OpenMicNightAudioProcessor&, APVTS&);
        void paint(juce::Graphics&) override;
    private:
        OpenMicNightAudioProcessor& processor;
        APVTS& state;
    };

    void addKnob(const char* id, const char* title, const char* suffix, EditorMode);
    Knob* knob(const char* id) const;
    void showMode(EditorMode);
    void setParameter(const char* id, float plainValue);
    void applyPreset(int index);
    void markCustom();
    void timerCallback() override;

    OpenMicNightAudioProcessor& processor;
    APVTS& apvts;
    NightLookAndFeel look;
    StageView stage;

    juce::Label brand, title, subtitle, profileLabel, statusLabel;
    juce::ComboBox presetBox;
    juce::TextButton surfaceButton { "SIMPLE" }, advancedButton { "ADVANCED" }, performerButton { "PERFORMER" };
    juce::ToggleButton feedbackArm { "ARM FEEDBACK" }, venueBed { "VENUE HUM" }, conductedFeedback { "CONDUCT HOWL" }, feedbackSync { "SYNC HOWL" }, feedbackLengthSync { "SYNC HOWL LENGTH" }, crowdSync { "SYNC CROWD" }, crowdLengthSync { "SYNC CROWD LENGTH" };
    juce::TextButton feedbackTrigger { "TRIGGER HOWL" }, crowdTrigger { "TRIGGER CROWD" };
    std::unique_ptr<APVTS::ButtonAttachment> feedbackArmAttachment, venueBedAttachment, conductedFeedbackAttachment, feedbackSyncAttachment, feedbackLengthSyncAttachment, crowdSyncAttachment, crowdLengthSyncAttachment;
    std::unique_ptr<Choice> micChoice, venueChoice, paChoice, crowdBedChoice, crowdBehaviorChoice, crowdEventChoice, feedbackDivisionChoice, feedbackLengthChoice, crowdDivisionChoice, crowdLengthChoice;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unordered_map<std::string, Knob*> knobMap;
    std::vector<Knob*> surfaceKnobs, advancedKnobs, performerKnobs;
    EditorMode currentMode = EditorMode::simple;
    bool suppressPresetChanges = false;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMicNightAudioProcessorEditor)
};
