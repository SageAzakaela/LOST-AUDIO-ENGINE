#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <memory>
#include <vector>

class LostAudioSuiteEditor final : public juce::AudioProcessorEditor,
                                   public juce::DragAndDropContainer,
                                   public juce::DragAndDropTarget,
                                   private juce::Timer
{
public:
    explicit LostAudioSuiteEditor(LostAudioSuiteProcessor&);
    ~LostAudioSuiteEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInDragSource(const SourceDetails&) override { return true; }
    void itemDropped(const SourceDetails&) override;
    void selectSlot(int physicalSlot);
    void moveSlot(int physicalSlot, int direction);
    void disarmSlot(int physicalSlot);

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    class SuiteLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        SuiteLookAndFeel();
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    };

    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS&, const juce::String& id, const juce::String& title, const juce::String& suffix = {});
        void resized() override;
        void setTitle(const juce::String& text) { label.setText(text, juce::dontSendNotification); }
    private:
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    class SlotCard final : public juce::Component
    {
    public:
        SlotCard(LostAudioSuiteEditor&, APVTS&, int physicalSlot);
        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        int slot() const noexcept { return physical; }
        void setPositionNumber(int position);
        void setSelected(bool selectedState) { selected = selectedState; repaint(); }
    private:
        LostAudioSuiteEditor& owner;
        int physical = 0;
        bool selected = false, dragStarted = false;
        juce::Label number;
        juce::ComboBox engine;
        juce::ToggleButton bypass { "BYP" };
        juce::Slider mix;
        juce::TextButton edit { "EDIT" };
        juce::TextButton up { "^" }, down { "v" };
        std::unique_ptr<APVTS::ComboBoxAttachment> engineAttachment;
        std::unique_ptr<APVTS::ButtonAttachment> bypassAttachment;
        std::unique_ptr<APVTS::SliderAttachment> mixAttachment;
    };

    static juce::String slotId(int slot, const char* suffix);
    std::array<int, 6> readOrder() const;
    int engineForSlot(int physicalSlot) const;
    void setParameter(const juce::String& id, float plainValue);
    void setOrder(const std::array<int, 6>&);
    void handleSlotEngineChange(int physicalSlot);
    void rebuildInspector();
    void updateInspectorLabels();
    void applySlotProfile(int index);
    void applyChainPreset(int index);
    void timerCallback() override;

    LostAudioSuiteProcessor& processor;
    APVTS& apvts;
    SuiteLookAndFeel look;
    juce::Label brand, title, subtitle, chainLabel, inspectorTitle, inspectorHint, masterLabel, cpuLabel;
    juce::ComboBox chainPreset, slotProfile;
    std::array<std::unique_ptr<SlotCard>, 6> slotCards;
    std::vector<std::unique_ptr<Knob>> inspectorKnobs;
    std::array<std::unique_ptr<Knob>, 7> masterKnobs;
    juce::ToggleButton feedbackArm { "ARM OPEN MIC FEEDBACK" };
    std::unique_ptr<APVTS::ButtonAttachment> feedbackAttachment;
    int selectedSlot = 0, lastInspectorEngine = -1;
    bool applyingChainPreset = false;
    juce::Rectangle<int> chainBounds, inspectorBounds, masterBounds;
    juce::TooltipWindow tooltips { this, 700 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LostAudioSuiteEditor)
};
