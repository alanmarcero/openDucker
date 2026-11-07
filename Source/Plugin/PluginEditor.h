#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace look {
const juce::Colour background {0xff1c1c1c}, panel {0xff2c2c2c}, panelHover {0xff383838}, display {0xff0e0e0e},
    yellow {0xffffe033}, darkYellow {0xff8a7a1f}, text {0xffa6a6a6}, ink {0xff1c1c1c};
}

class DuckLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    DuckLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
};

class GlyphButton final : public juce::Button {
public:
    enum class Glyph { Loop, Menu, Headphones, Previous, Next, PreviousBank, NextBank };
    explicit GlyphButton(Glyph g) : juce::Button({}), glyph(g) {}
    void paintButton(juce::Graphics&, bool over, bool down) override;
private:
    Glyph glyph;
};

class CurveButton final : public juce::Button {
public:
    explicit CurveButton(int index) : juce::Button(duck::curveNames[static_cast<size_t>(index)]), curve(index) {}
    void paintButton(juce::Graphics&, bool over, bool down) override;
private:
    int curve;
};

/// Input and output waveform over one cycle with the editable ducking curve on top.
class CurveDisplay final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit CurveDisplay(DuckerProcessor&);
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
private:
    float lengthFraction() const;
    bool overHandle(juce::Point<float>) const;
    void setLength(float fraction);
    DuckerProcessor& owner;
    bool dragging = false;
};

class DuckerEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit DuckerEditor(DuckerProcessor&);
    ~DuckerEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void setChoice(const char* id, int index);
    void stepCurve(int delta);
    void showMenu();
    void configureBar(juce::Slider&, const char* id, const juce::String& name, const juce::String& unit, const char* tip);
    DuckerProcessor& owner;
    DuckLookAndFeel theme;
    CurveDisplay display;
    juce::Slider mix, band, shift, threshold;
    std::array<juce::TextButton, 4> rateButtons;
    std::array<juce::TextButton, 3> triggerButtons;
    juce::TextButton bandButton;
    GlyphButton retrigger {GlyphButton::Glyph::Loop}, menu {GlyphButton::Glyph::Menu}, solo {GlyphButton::Glyph::Headphones},
        previous {GlyphButton::Glyph::Previous}, next {GlyphButton::Glyph::Next},
        previousBank {GlyphButton::Glyph::PreviousBank}, nextBank {GlyphButton::Glyph::NextBank};
    std::vector<std::unique_ptr<CurveButton>> curveButtons;
    juce::TooltipWindow tooltips {this, 600};
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment, bandAttachment, shiftAttachment, thresholdAttachment;
    std::unique_ptr<ButtonAttachment> retriggerAttachment, soloAttachment, bandButtonAttachment;
    std::unique_ptr<juce::ParameterAttachment> rateAttachment, triggerAttachment, curveAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckerEditor)
};
