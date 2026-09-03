#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <unordered_map>
#include <vector>
#include "PluginProcessor.h"

class TelevisionEngineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TelevisionEngineAudioProcessorEditor(TelevisionEngineAudioProcessor&);
    ~TelevisionEngineAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    enum class View { simple, advanced, performer };

    class Panel final : public juce::Component
    {
    public:
        explicit Panel(juce::String title) : name(std::move(title)) {}
        void paint(juce::Graphics&) override;
        juce::Rectangle<int> contentBounds() const { return getLocalBounds().reduced(10).withTrimmedTop(23); }
    private:
        juce::String name;
    };
    class Knob final : public juce::Component
    {
    public:
        Knob(APVTS&, const juce::String&, const juce::String&, std::function<void()>);
        void resized() override;
        void setHint(const juce::String& hint) { label.setTooltip(hint); slider.setTooltip(hint); }
    private:
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };
    class Choice final : public juce::Component
    {
    public:
        Choice(APVTS&, const juce::String&, const juce::String&, std::function<void()>);
        void resized() override;
        void setHint(const juce::String& hint) { label.setTooltip(hint); combo.setTooltip(hint); }
    private:
        juce::Label label;
        juce::ComboBox combo;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };
    class Switch final : public juce::Component
    {
    public:
        Switch(APVTS&, const juce::String&, const juce::String&, std::function<void()>);
        void resized() override { button.setBounds(getLocalBounds()); }
        void setHint(const juce::String& hint) { button.setTooltip(hint); }
    private:
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };
    class CrtDisplay final : public juce::Component
    {
    public:
        void setState(std::array<float, 64>, float, float, float, float, float, float, float, float, float, int, int);
        void paint(juce::Graphics&) override;
    private:
        std::array<float, 64> trace {};
        std::array<float, 2> input {}, output {};
        float bed = 0, snow = 0, electrical = 0, rattle = 0, sync = 0, phase = 0;
        int model = 1, reception = 1;
    };

    Knob* addKnob(Panel&, const juce::String&, const juce::String&, const juce::String&);
    Choice* addChoice(Panel&, const juce::String&, const juce::String&, const juce::String&);
    Switch* addSwitch(Panel&, const juce::String&, const juce::String&, const juce::String&);
    void layoutPanel(Panel&, int columns);
    void setView(View);
    void applyPreset(int);
    void resetParameters();
    void setParameter(const juce::String&, float);
    float getParameter(const juce::String&) const;
    void markCustom();
    void timerCallback() override;

    TelevisionEngineAudioProcessor& processor;
    APVTS& apvts;
    juce::Label brandLabel, titleLabel, subtitleLabel, profileLabel, statusLabel;
    juce::ComboBox presetBox;
    juce::TextButton simpleButton { "SIMPLE" }, advancedButton { "ADVANCED" }, performerButton { "PERFORMER" };
    juce::TextButton triggerButton { "TRIGGER SYNC FAULT" };
    juce::Component simplePage, advancedPage, performerPage;
    CrtDisplay display;

    Panel simpleCharacter { "SET CHARACTER" }, simpleLayers { "PLAYBACK LAYERS & OUTPUT" };
    Panel tonePanel { "RECEIVER TONE" }, broadcastPanel { "BROADCAST PATH" }, cabinetPanel { "CABINET & POWER" };
    Panel noisePanel { "ELECTRICAL TEXTURE" }, outputPanel { "OUTPUT SAFETY" };
    Panel performerTone { "PLAYABLE SET" }, performerLayers { "INDEPENDENT LAYERS" };
    Panel performerFault { "CLOCKED SYNC FAULT" }, performerOutput { "PERFORMANCE OUTPUT" };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<Switch>> switches;
    std::unordered_map<Panel*, std::vector<juce::Component*>> panelItems;
    juce::TooltipWindow tooltipWindow { this, 700 };
    View currentView = View::simple;
    bool suppressPresetChanges = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TelevisionEngineAudioProcessorEditor)
};
