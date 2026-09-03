#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <memory>

class LostAudioSequencerEditor final : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit LostAudioSequencerEditor(LostAudioSequencerProcessor&);
    ~LostAudioSequencerEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void selectStep(int step);

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    class SequencerLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        SequencerLookAndFeel();
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float,
                              float, float, juce::Slider&) override;
        void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                                  bool, bool) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
        void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int,
                          juce::ComboBox&) override;
    };

    class StepPad final : public juce::Button
    {
    public:
        StepPad(LostAudioSequencerEditor&, LostAudioSequencerProcessor&, int);
        void paintButton(juce::Graphics&, bool, bool) override;
        void mouseDoubleClick(const juce::MouseEvent&) override;

    private:
        LostAudioSequencerEditor& owner;
        LostAudioSequencerProcessor& processor;
        int step = 0;
    };

    void timerCallback() override;
    void configureKnob(juce::Slider&, juce::Label&, const juce::String&, const juce::String& suffix = {});
    void configureChoice(juce::ComboBox&);
    void rebuildStepAttachments();
    void setActualValue(const juce::String& id, float value);
    float value(const juce::String& id) const;

    LostAudioSequencerProcessor& processor;
    SequencerLookAndFeel lookAndFeel;
    int selectedStep = 0;

    juce::Label brandLabel, titleLabel, subtitleLabel, stepTitleLabel, hintLabel;
    juce::ComboBox presetBox, divisionBox, engineBox;
    juce::TextButton loadPresetButton { "LOAD" }, randomizeButton { "SAFE RANDOM" }, clearButton { "CLEAR" };
    juce::ToggleButton enabledButton { "SEQUENCER" }, auditionButton { "AUDITION" }, stepEnabledButton { "STEP ARMED" };

    juce::Slider lengthSlider, swingSlider, bpmSlider;
    juce::Slider inputSlider, outputSlider, masterMixSlider, safetySlider, ceilingSlider;
    juce::Slider characterSlider, damageSlider, probabilitySlider, stepMixSlider, modelSlider;
    juce::Label lengthLabel, swingLabel, bpmLabel;
    juce::Label inputLabel, outputLabel, masterMixLabel, safetyLabel, ceilingLabel;
    juce::Label characterLabel, damageLabel, probabilityLabel, stepMixLabel, modelLabel;

    std::array<std::unique_ptr<StepPad>, LostAudioSequencerProcessor::stepCount> pads;

    std::unique_ptr<APVTS::ButtonAttachment> enabledAttachment, auditionAttachment;
    std::unique_ptr<APVTS::ComboBoxAttachment> divisionAttachment;
    std::unique_ptr<APVTS::SliderAttachment> lengthAttachment, swingAttachment, bpmAttachment;
    std::unique_ptr<APVTS::SliderAttachment> inputAttachment, outputAttachment, masterMixAttachment,
                                             safetyAttachment, ceilingAttachment;
    std::unique_ptr<APVTS::ButtonAttachment> stepEnabledAttachment;
    std::unique_ptr<APVTS::ComboBoxAttachment> engineAttachment;
    std::unique_ptr<APVTS::SliderAttachment> characterAttachment, damageAttachment, probabilityAttachment,
                                             stepMixAttachment, modelAttachment;

    juce::Rectangle<int> gridBounds, inspectorBounds, meterBounds;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LostAudioSequencerEditor)
};
