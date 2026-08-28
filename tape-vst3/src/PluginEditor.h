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
        void setMotion(float newMotion);
        void setOutputLevel(float newLevel);

    private:
        float phase = 0.0f;
        float motion = 0.25f;
        float outputLevel = 0.0f;
    };

    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS& state, const juce::String& paramID, const juce::String& text);
        void resized() override;
        void setHint(const juce::String& hint);

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

    private:
        juce::Label label;
        juce::ComboBox combo;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    void addKnob(Panel&, const juce::String& id, const juce::String& text, const juce::String& hint);
    void addSwitch(Panel&, const juce::String& id, const juce::String& text, const juce::String& hint);
    void addChoice(Panel&, const juce::String& id, const juce::String& text, const juce::String& hint);
    void layoutPanel(Panel&, int columns);
    void showAdvanced(bool shouldShowAdvanced);

    void setParamValue(const juce::String& id, float plainValue);
    float getParamValue(const juce::String& id) const;
    void applyMacroQuality(float quality);
    void applyMacroAge(float age);
    void applyMacroWow(float wow);
    void applyMacroGlitch(float glitch);
    void applyPreset(int idx);
    void timerCallback() override;

    TapeEngineAudioProcessor& processor;
    APVTS& apvts;
    TapeLookAndFeel lookAndFeel;

    juce::Label brand;
    juce::Label title;
    juce::Label subtitle;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TextButton surfaceButton { "SURFACE" };
    juce::TextButton advancedButton { "ADVANCED" };
    juce::Label statusLabel;
    juce::TooltipWindow tooltipWindow { this, 850 };

    juce::Component surfacePage;
    juce::Component advancedPage;
    DeckDisplay deckDisplay;
    Panel macroPanel { "CHARACTER" };
    Panel surfaceOutputPanel { "DECK OUTPUT" };
    Panel tonePanel { "01 / HEAD + BANDWIDTH" };
    Panel transportPanel { "02 / TRANSPORT" };
    Panel texturePanel { "03 / TAPE BODY" };
    Panel deckPanel { "04 / MECHANISM" };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::vector<std::unique_ptr<Choice>> choices;
    std::unordered_map<Panel*, std::vector<juce::Component*>> panelItems;

    bool suppressMacros = false;
    bool showingAdvanced = false;
    float displayLevel = 0.0f;
    float lastQuality = 0.55f;
    float lastAge = 0.35f;
    float lastWow = 0.25f;
    float lastGlitch = 0.18f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeEngineAudioProcessorEditor)
};
