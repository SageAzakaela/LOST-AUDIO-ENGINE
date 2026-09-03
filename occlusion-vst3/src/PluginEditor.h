#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <functional>
#include <unordered_map>
#include <vector>

class OcclusionEngineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OcclusionEngineAudioProcessorEditor(OcclusionEngineAudioProcessor&);
    ~OcclusionEngineAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS=juce::AudioProcessorValueTreeState;
    class Panel final : public juce::Component
    {
    public: explicit Panel(juce::String n):name(std::move(n)){} void paint(juce::Graphics&)override;
        juce::Rectangle<int> contentBounds()const{return getLocalBounds().reduced(9).withTrimmedTop(23);}
    private: juce::String name;
    };
    class Knob final : public juce::Component
    {
    public: Knob(APVTS&,const juce::String&,const juce::String&,std::function<void()>);void resized()override;void hint(const juce::String&h){label.setTooltip(h);slider.setTooltip(h);}
    private: juce::Label label;juce::Slider slider;std::unique_ptr<APVTS::SliderAttachment> attachment;
    };
    class Choice final : public juce::Component
    {
    public: Choice(APVTS&,const juce::String&,const juce::String&,std::function<void()>);void resized()override;void hint(const juce::String&h){label.setTooltip(h);combo.setTooltip(h);}
    private: juce::Label label;juce::ComboBox combo;std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };
    class Toggle final : public juce::ToggleButton
    {
    public: Toggle(APVTS&,const juce::String&,const juce::String&,std::function<void()>);
    private: std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };
    class Boundary final : public juce::Component
    {
    public: void state(float,float,float,float,float,float,float,float,const std::array<float,64>&,int,int);void paint(juce::Graphics&)override;
    private: float in=0,out=0,body=0,room=0,leak=0,rattle=0,limit=0,excite=0,phase=0;std::array<float,64>trace{};int material=0,construction=1;
    };

    Knob* knob(Panel&,const char*,const char*,const char*);
    Choice* choice(Panel&,const char*,const char*,const char*);
    Toggle* toggle(Panel&,const char*,const char*,const char*);
    void layout(Panel&,int);
    void showMode(int);
    void preset(int);
    void reset();
    void set(const char*,float);
    float get(const char*)const;
    void custom();
    void timerCallback()override;

    OcclusionEngineAudioProcessor& processor;APVTS& apvts;
    juce::Label brand,title,subtitle,profile,status;juce::ComboBox presets;
    juce::TextButton simple{"SIMPLE"},advanced{"ADVANCED"},performer{"PERFORMER"};
    Boundary boundary;juce::Component pages[3];
    Panel simpleBody{"BOUNDARY CHARACTER"},simpleSpace{"TWO-SIDED SPACE"},simpleOutput{"LISTENER OUTPUT"};
    Panel tone{"TRANSMISSION BAND"},body{"RESONANT BODY"},space{"SOURCE / LISTENER"},leak{"LEAK / MULTIPATH"},detail{"FILTER DETAIL"},safety{"OUTPUT SAFETY"};
    Panel conducted{"CONDUCTED EXCITATION"},hardware{"HARDWARE STRIKES"},motion{"STEREO MOTION"},performanceOutput{"PERFORMANCE OUTPUT"};
    juce::TextButton exciteButton{"EXCITE BOUNDARY"},strikeButton{"STRIKE HARDWARE"};
    std::vector<std::unique_ptr<Knob>> knobs;std::vector<std::unique_ptr<Choice>> choices;std::vector<std::unique_ptr<Toggle>> toggles;
    std::unordered_map<Panel*,std::vector<juce::Component*>> items;
    juce::TooltipWindow tips{this,700};int mode=0;bool suppress=false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OcclusionEngineAudioProcessorEditor)
};
