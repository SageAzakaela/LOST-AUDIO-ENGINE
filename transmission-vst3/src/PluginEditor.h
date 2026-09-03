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

    class ReceiverLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        ReceiverLookAndFeel();
        void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPosition, float rotaryStartAngle, float rotaryEndAngle,
                              juce::Slider&) override;
        void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                                  bool highlighted, bool down) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
        void drawComboBox(juce::Graphics&, int width, int height, bool down,
                          int buttonX, int buttonY, int buttonWidth, int buttonHeight,
                          juce::ComboBox&) override;
        juce::Font getComboBoxFont(juce::ComboBox&) override;
    };

    class Panel final : public juce::Component
    {
    public:
        explicit Panel(juce::String titleText) : title(std::move(titleText)) {}
        void paint(juce::Graphics&) override;
        [[nodiscard]] juce::Rectangle<int> contentBounds() const;
    private:
        juce::String title;
    };

    class ReceiverDisplay final : public juce::Component
    {
    public:
        void setState(std::array<float, 64> waveform, float inputLeft, float inputRight,
                      float outputLeft, float outputRight, float carrierMs,
                      bool dropoutActive, float dropoutProgress, float compression,
                      float noise, float interference, bool squelchClosed,
                      float squelchEvent, bool tuningActive, float tuningProgress,
                      int tuningAsset, float limiter, int generations);
        void paint(juce::Graphics&) override;
    private:
        std::array<float, 64> trace {};
        std::array<float, 2> input {}, output {};
        float carrier = 0.0f, dropout = 0.0f, compression = 0.0f;
        float noise = 0.0f, interference = 0.0f, squelchEvent = 0.0f;
        float tuning = 0.0f, limiter = 0.0f;
        float phase = 0.0f;
        bool dropoutOn = false, squelchClosed = false, tuningOn = false;
        int tuningAsset = 0, generations = 1;
    };

    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS&, const juce::String& parameterId, const juce::String& labelText,
             std::function<void()> userChange);
        void resized() override;
        void setHint(const juce::String& hint);
        void setUserChange(std::function<void()> userChange) { slider.onDragStart = std::move(userChange); }
    private:
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    class Switch final : public juce::Component
    {
    public:
        Switch(APVTS&, const juce::String& parameterId, const juce::String& labelText,
               std::function<void()> userChange);
        void resized() override;
        void setHint(const juce::String& hint);
    private:
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    class Choice final : public juce::Component
    {
    public:
        Choice(APVTS&, const juce::String& parameterId, const juce::String& labelText,
               std::function<void()> userChange);
        void resized() override;
        void setHint(const juce::String& hint);
    private:
        juce::Label label;
        juce::ComboBox combo;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    Knob* addKnob(Panel&, const juce::String& parameterId, const juce::String& text,
                  const juce::String& hint, bool surfaceLinked = false);
    Switch* addSwitch(Panel&, const juce::String& parameterId, const juce::String& text,
                      const juce::String& hint);
    Choice* addChoice(Panel&, const juce::String& parameterId, const juce::String& text,
                      const juce::String& hint);
    void layoutPanel(Panel&, int columns);
    enum class EditorMode { simple, advanced, performer };
    void showMode(EditorMode);
    void applyPreset(int index);
    void setParameter(const juce::String& id, float plainValue);
    [[nodiscard]] float getParameter(const juce::String& id) const;
    void markCustom();
    void timerCallback() override;

    TransmissionEngineAudioProcessor& processor;
    APVTS& apvts;
    ReceiverLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 750 };

    juce::Label brandLabel;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label profileLabel;
    juce::ComboBox presetBox;
    juce::TextButton surfaceButton { "SIMPLE" };
    juce::TextButton advancedButton { "ADVANCED" };
    juce::TextButton performerButton { "PERFORMER" };
    juce::Label statusLabel;

    juce::Component surfacePage, advancedPage, performerPage;
    ReceiverDisplay receiverDisplay;
    Panel characterPanel { "SIGNAL CHARACTER" };
    Panel outputPanel { "RECEIVER OUTPUT" };
    Panel tonePanel { "TUNING BAND" };
    Panel transmitterPanel { "TRANSMITTER" };
    Panel receptionPanel { "RECEPTION DAMAGE" };
    Panel noisePanel { "INTERFERENCE BED" };
    Panel squelchPanel { "SQUELCH GATE" };
    Panel searchPanel { "SEARCH EVENTS" };
    Panel performerCarrierPanel { "CLOCKED CARRIER" };
    Panel performerDropoutPanel { "CARRIER LOSS" };
    Panel performerSearchPanel { "TUNING SEARCH" };
    Panel performerSquelchPanel { "SQUELCH PERFORMANCE" };
    Panel performerOutputPanel { "SAFE RECEIVER OUTPUT" };
    juce::TextButton dropoutButton { "TRIGGER DROPOUT" };
    juce::TextButton searchButton { "TRIGGER SEARCH" };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::vector<std::unique_ptr<Choice>> choices;
    std::unordered_map<Panel*, std::vector<juce::Component*>> panelItems;
    EditorMode currentMode = EditorMode::simple;
    bool suppressPresetChanges = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransmissionEngineAudioProcessorEditor)
};
