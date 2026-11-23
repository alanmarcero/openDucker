#pragma once
#include "PluginProcessor.h"
#include "EffectsPanel.h"
#include "SuiteBar.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace look {
const juce::Colour background {0xff0d1317}, panel {0xff152026}, panelHover {0xff1e2c33}, display {0xff0a1216},
    mint {0xff48f0c8}, brightMint {0xff80f5dc}, yellow {0xffffbe3b}, darkMint {0xff23483f},
    paper {0xffe6f0f5}, muted {0xffa2b5c0}, ink {0xff0d1317};
// Bar text height as a share of the bar height, and the text inset at each end.
constexpr float barTextScale = 0.37f, barPadding = 9.0f;
// A control whose setting has no effect in the current state draws at this alpha.
constexpr float inactiveAlpha = 0.58f;
}

inline void bindParamLock(juce::Component& control, const char* paramId) {
    control.getProperties().set("paramLockId", juce::String(paramId));
}

class ParamLockOverlay final : public juce::Component {
public:
    explicit ParamLockOverlay(DuckerProcessor& p) : owner(p) { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
private:
    void paintLocksIn(juce::Graphics&, juce::Component&);
    DuckerProcessor& owner;
};

class DuckLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    explicit DuckLookAndFeel(DuckerProcessor&);
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;
private:
    DuckerProcessor& owner;
};

class GlyphButton final : public LockableControl<juce::Button> {
public:
    enum class Glyph { Loop, Menu, Headphones, Previous, Next, PreviousBank, NextBank };
    explicit GlyphButton(Glyph g) : LockableControl<juce::Button>(juce::String()), glyph(g) {}
    void paintButton(juce::Graphics&, bool over, bool down) override;
private:
    Glyph glyph;
};

class CurveButton final : public LockableControl<juce::Button> {
public:
    explicit CurveButton(int index) : LockableControl<juce::Button>(duck::curveNames[static_cast<size_t>(index)]), curve(index) {}
    void paintButton(juce::Graphics&, bool over, bool down) override;
private:
    int curve;
};

/// Input and output waveform over one cycle with the editable ducking curve on top.
class CurveDisplay final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit CurveDisplay(DuckerProcessor&);
    // Reads trigger and detector activity once per editor refresh.
    void tick();
    bool isFlashing() const noexcept { return flashFrames > 0; }
    bool isClipped() const noexcept { return clipped; }
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
private:
    float lengthFraction() const;
    float shiftFraction() const;
    float handleX() const;
    juce::Rectangle<float> handleBounds() const;
    bool overHandle(juce::Point<float>) const;
    void setLength(float fraction);
    void setLengthFromDrag(float x);
    DuckerProcessor& owner;
    bool dragging = false;
    float dragStartX = 0.0f, dragStartLength = 1.0f;
    int seenTriggers = 0, flashFrames = 0, holdFrames = 0;
    float detectorLevel = 0, detectorHold = 0;
    bool clipped = false;
};

class DuckerEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit DuckerEditor(DuckerProcessor&);
    ~DuckerEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    void timerCallback() override;
    void paintDuckBooth(juce::Graphics& g, juce::Rectangle<float> area, float s);
    void setChoice(const char* id, int index);
    void stepCurve(int delta);
    void showMenu();
    void refreshActivity();
    void configureBar(juce::Slider&, const char* id, const juce::String& name, const char* tip);
    DuckerProcessor& owner;
    DuckLookAndFeel theme;
    juce::Component content;
    CurveDisplay display;
    EffectsPanel effectsPanel;
    suite::SuiteBar suiteBar;
    juce::TextButton effectsToggle;
    juce::TextButton pushItButton {"Test Impact"};
    bool effectsExpanded = true;
    LockableControl<juce::Slider> mix, band, shift, slope, threshold, lookahead, smoothing, detectorHpf, detectorLpf, tension, floor;
    LockableControl<juce::ComboBox> bandSlope;
    LockableControl<juce::TextButton> detectorMonitorButton {"Listen Detector"};
    std::array<LockableControl<juce::TextButton>, 4> rateButtons;
    std::array<LockableControl<juce::TextButton>, 3> triggerButtons;
    LockableControl<juce::TextButton> bandButton;
    GlyphButton retrigger {GlyphButton::Glyph::Loop}, menu {GlyphButton::Glyph::Menu}, solo {GlyphButton::Glyph::Headphones},
        previous {GlyphButton::Glyph::Previous}, next {GlyphButton::Glyph::Next},
        previousBank {GlyphButton::Glyph::PreviousBank}, nextBank {GlyphButton::Glyph::NextBank};
    std::vector<std::unique_ptr<CurveButton>> curveButtons;
    ParamLockOverlay lockOverlay;
    std::unique_ptr<juce::Drawable> boothSpeaker;
    juce::Image mascot, faceplate;
    juce::TooltipWindow tooltips {this, 600};
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> barAttachments;
    std::unique_ptr<ComboBoxAttachment> bandSlopeAttachment;
    std::unique_ptr<ButtonAttachment> retriggerAttachment, soloAttachment, bandButtonAttachment, detectorMonitorAttachment;
    std::unique_ptr<juce::ParameterAttachment> rateAttachment, triggerAttachment, curveAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckerEditor)
};
