#pragma once
#include "PluginProcessor.h"

class EffectsPanel final : public juce::Component {
public:
    explicit EffectsPanel(DuckerProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    DuckerProcessor& owner;
    std::array<juce::Slider,12> knobs;
    std::array<juce::ComboBox,4> choices;
    std::array<juce::TextButton,3> switches;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,12> knobAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>,4> choiceAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>,3> switchAttachments;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsPanel)
};
