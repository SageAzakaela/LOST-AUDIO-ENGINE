#pragma once

#include <JuceHeader.h>

#include <unordered_map>
#include <vector>

#include "PluginProcessor.h"

class CDEngineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CDEngineAudioProcessorEditor(CDEngineAudioProcessor&);
    ~CDEngineAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    class DeckLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        DeckLookAndFeel();
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
        void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
        void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
        juce::Font getComboBoxFont(juce::ComboBox&) override;
    };

    class Panel final : public juce::Component
    {
    public:
        explicit Panel(juce::String name) : title(std::move(name)) {}
        void paint(juce::Graphics&) override;
        [[nodiscard]] juce::Rectangle<int> contentBounds() const;
    private:
        juce::String title;
    };

    class DiscDisplay final : public juce::Component
    {
    public:
        void setState(float leftIn, float rightIn, float leftOut, float rightOut,
                      float rotation, float damage, float stereoLink, bool damaged, bool skipping);
        void advance();
        void paint(juce::Graphics&) override;
    private:
        std::array<float, 2> input { 0.0f, 0.0f };
        std::array<float, 2> output { 0.0f, 0.0f };
        float phase = 0.0f;
        float speed = 5.2f;
        float damageAmount = 0.2f;
        float linkAmount = 1.0f;
        bool damageActive = false;
        bool skipActive = false;
    };

    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS&, const juce::String&, const juce::String&, std::function<void()>);
        void resized() override;
        void setHint(const juce::String&);
    private:
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    class Switch final : public juce::Component
    {
    public:
        Switch(APVTS&, const juce::String&, const juce::String&, std::function<void()>);
        void resized() override;
        void setHint(const juce::String&);
    private:
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    class Choice final : public juce::Component
    {
    public:
        Choice(APVTS&, const juce::String&, const juce::String&, std::function<void()>);
        void resized() override;
        void setHint(const juce::String&);
    private:
        juce::Label label;
        juce::ComboBox combo;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    Knob* addKnob(Panel&, const juce::String&, const juce::String&, const juce::String&, bool linked = false);
    Switch* addSwitch(Panel&, const juce::String&, const juce::String&, const juce::String&, bool linked = false);
    Choice* addChoice(Panel&, const juce::String&, const juce::String&, const juce::String&, bool linked = false);
    void layoutPanel(Panel&, int columns);
    void showAdvanced(bool);
    void applyPreset(int);
    void resetParameters();
    void setParameter(const juce::String&, float);
    [[nodiscard]] float getParameter(const juce::String&) const;
    void markCustom();
    void timerCallback() override;

    CDEngineAudioProcessor& processor;
    APVTS& apvts;
    DeckLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 700 };

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
    DiscDisplay discDisplay;
    Panel characterPanel { "DISC CHARACTER" };
    Panel deckPanel { "DECK OUTPUT" };
    Panel decoderPanel { "DECODER" };
    Panel burstPanel { "SECTOR DAMAGE" };
    Panel trackingPanel { "TRACKING" };
    Panel mechanicsPanel { "TRANSPORT" };
    Panel stereoPanel { "STEREO OUTPUT" };
    Panel protectionPanel { "PROTECTION" };
    juce::TextButton damageButton { "TRIGGER DAMAGE" };
    juce::TextButton skipButton { "TRIGGER SKIP" };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::vector<std::unique_ptr<Choice>> choices;
    std::unordered_map<Panel*, std::vector<juce::Component*>> panelItems;
    std::vector<juce::Component*> linkedControls;
    bool showingAdvanced = false;
    bool suppressPresetChanges = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CDEngineAudioProcessorEditor)
};
