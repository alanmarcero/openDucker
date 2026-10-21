#pragma once
#include "PluginProcessor.h"
#include "EffectsPanel.h"
#include <juce_gui_basics/juce_gui_basics.h>

class DuckLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    DuckLookAndFeel();
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
};
class DuckerEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit DuckerEditor(DuckerProcessor&);
    ~DuckerEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void paintStage(juce::Graphics&,float amountPercent);
    DuckerProcessor& owner;
    DuckLookAndFeel theme;
    juce::Slider amount;
    std::array<juce::TextButton,4> slopes;
    juce::ComboBox rate;
    juce::TextButton bypass;
    EffectsPanel effectsPanel;
    juce::TextButton effectsToggle;
    bool effectsExpanded=true;
    juce::Image mascot, faceplate;
    juce::TooltipWindow tooltips{this,600};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::ParameterAttachment> shapeAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckerEditor)
};
