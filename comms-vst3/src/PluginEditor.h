#pragma once

#include <JuceHeader.h>

#include <unordered_map>
#include <vector>

#include "PluginProcessor.h"

class CommsEngineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CommsEngineAudioProcessorEditor(CommsEngineAudioProcessor&);
    ~CommsEngineAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    class ConsoleLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        ConsoleLookAndFeel();
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

    class ConsoleDisplay final : public juce::Component
    {
    public:
        void setState(float signal, int mode, float failure, float distance, bool alarm);
        void advance();
        void paint(juce::Graphics&) override;
    private:
        float signalLevel = 0.0f;
        int activeMode = 0;
        float failure = 0.2f;
        float distance = 0.15f;
        float phase = 0.0f;
        bool alarmEnabled = false;
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
    void showAdvanced(bool shouldShowAdvanced);
    void applyPreset(int index);
    void setParameter(const juce::String& id, float plainValue);
    [[nodiscard]] float getParameter(const juce::String& id) const;
    void markCustom();
    void timerCallback() override;

    CommsEngineAudioProcessor& processor;
    APVTS& apvts;
    ConsoleLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 750 };

    juce::Label brandLabel;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label profileLabel;
    juce::ComboBox presetBox;
    juce::TextButton surfaceButton { "SURFACE" };
    juce::TextButton advancedButton { "ADVANCED" };
    juce::Label statusLabel;

    juce::Component surfacePage;
    juce::Component advancedPage;
    ConsoleDisplay consoleDisplay;
    Panel characterPanel { "CHANNEL CHARACTER" };
    Panel outputPanel { "CONSOLE OUTPUT" };
    Panel tonePanel { "VOICE BAND" };
    Panel transmitterPanel { "GAIN & CODING" };
    Panel receptionPanel { "LINE FAILURE" };
    Panel noisePanel { "TRANSDUCER" };
    Panel squelchPanel { "SPACE & ECHO" };
    Panel searchPanel { "ROOM & PROTECTION" };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::vector<std::unique_ptr<Choice>> choices;
    std::unordered_map<Panel*, std::vector<juce::Component*>> panelItems;
    std::vector<juce::Component*> linkedAdvancedControls;
    bool showingAdvanced = false;
    bool suppressPresetChanges = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommsEngineAudioProcessorEditor)
};
