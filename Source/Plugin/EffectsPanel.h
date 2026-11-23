#pragma once
#include "PluginProcessor.h"

/// Suppresses normal click/drag/release on right-click so only the parameter lock menu opens.
template <typename Base>
class LockableControl : public Base {
public:
    using Base::Base;
    void mouseDown(const juce::MouseEvent& e) override {
        if(!e.mods.isPopupMenu()) Base::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(!e.mods.isPopupMenu()) Base::mouseDrag(e);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        if(!e.mods.isPopupMenu()) Base::mouseUp(e);
    }
};

class EffectsPanel final : public juce::Component {
public:
    explicit EffectsPanel(DuckerProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;
    void refresh();
private:
    class Knob final : public LockableControl<juce::Slider> {
    public:
        bool zeroDetent = false;
        double snapValue(double, DragMode) override;
    };
    DuckerProcessor& owner;
    std::array<Knob, 12> knobs;
    std::array<LockableControl<juce::ComboBox>, 6> choices;
    std::array<LockableControl<juce::TextButton>, 3> switches;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 12> knobAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 6> choiceAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 3> switchAttachments;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsPanel)
};
