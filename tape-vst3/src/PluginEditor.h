#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <vector>

#include "PluginProcessor.h"

class TapeEngineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TapeEngineAudioProcessorEditor(TapeEngineAudioProcessor&);
    ~TapeEngineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    class TapeLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float,
                              float, float, juce::Slider&) override;
        void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                                  bool, bool) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
        void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int,
                          juce::ComboBox&) override;
        juce::Font getComboBoxFont(juce::ComboBox&) override;
    };

    class Panel final : public juce::Component
    {
    public:
        explicit Panel(juce::String titleText) : title(std::move(titleText)) {}
        void paint(juce::Graphics&) override;
        juce::Rectangle<int> contentBounds() const;

    private:
        juce::String title;
    };

    class DeckDisplay final : public juce::Component
    {
    public:
        void paint(juce::Graphics&) override;
        void setState(std::array<float, 64> waveform, float inputLeft, float inputRight,
                      float outputLeft, float outputRight, float modulationMs,
                      float dropoutProgress, bool dropoutActive, float compression,
                      float saturation, float noise, float mechanism, float limiter,
                      int machine);

    private:
        float phase = 0.0f;
        std::array<float, 64> trace {};
        std::array<float, 2> input {}, output {};
        float modulation = 0.0f, dropout = 0.0f, compression = 0.0f;
        float saturation = 0.0f, noise = 0.0f, mechanism = 0.0f, limiter = 0.0f;
        bool dropoutOn = false;
        int machine = 0;
    };

    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS& state, const juce::String& paramID, const juce::String& text);
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
        Switch(APVTS& state, const juce::String& paramID, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& hint);
        void setUserChange(std::function<void()> userChange) { button.onClick = std::move(userChange); }

    private:
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    class Choice final : public juce::Component
    {
    public:
        Choice(APVTS& state, const juce::String& paramID, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& hint);
        void setUserChange(std::function<void()> userChange) { combo.onChange = std::move(userChange); }

    private:
        juce::Label label;
        juce::ComboBox combo;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    Knob* addKnob(Panel&, const juce::String& id, const juce::String& text, const juce::String& hint);
    void addSwitch(Panel&, const juce::String& id, const juce::String& text, const juce::String& hint);
    void addChoice(Panel&, const juce::String& id, const juce::String& text, const juce::String& hint);
    void layoutPanel(Panel&, int columns);
    enum class EditorMode { simple, advanced, performer };
    void showMode(EditorMode);

    void setParamValue(const juce::String& id, float plainValue);
    float getParamValue(const juce::String& id) const;
    void resetParameters();
    void applyPreset(int idx);
    void markCustom();
    void timerCallback() override;

    TapeEngineAudioProcessor& processor;
    APVTS& apvts;
    TapeLookAndFeel lookAndFeel;

    juce::Label brand;
    juce::Label title;
    juce::Label subtitle;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TextButton surfaceButton { "SIMPLE" };
    juce::TextButton advancedButton { "ADVANCED" };
    juce::TextButton performerButton { "PERFORMER" };
    juce::Label statusLabel;
    juce::TooltipWindow tooltipWindow { this, 850 };

    juce::Component surfacePage, advancedPage, performerPage;
    DeckDisplay deckDisplay;
    Panel simpleCharacterPanel { "TAPE CHARACTER" };
    Panel simpleMotionPanel { "TRANSPORT + OUTPUT" };
    Panel tonePanel { "01 / HEAD + BANDWIDTH" };
    Panel transportPanel { "02 / TRANSPORT" };
    Panel texturePanel { "03 / TAPE BODY" };
    Panel deckPanel { "04 / MECHANISM" };
    Panel performerMotionPanel { "CLOCKED TRANSPORT" };
    Panel performerDamagePanel { "DROPOUT PERFORMANCE" };
    Panel performerDeckPanel { "MACHINE LAYER" };
    Panel performerOutputPanel { "SAFE OUTPUT" };
    juce::TextButton dropoutButton { "TRIGGER DROPOUT" };
    juce::TextButton mechanismButton { "TRIGGER MECHANISM" };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::vector<std::unique_ptr<Choice>> choices;
    std::unordered_map<Panel*, std::vector<juce::Component*>> panelItems;
    EditorMode currentMode = EditorMode::simple;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeEngineAudioProcessorEditor)
};
