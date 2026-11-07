#include "PluginEditor.h"
#include <cmath>

namespace {
constexpr float pi = juce::MathConstants<float>::pi;
void drawText(juce::Graphics& g, const juce::String& value, juce::Rectangle<float> bounds, float size, juce::Colour colour,
    juce::Justification alignment = juce::Justification::centred, bool bold = true) {
    g.setColour(colour);
    g.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    g.drawText(value, bounds, alignment, false);
}
juce::Path curvePath(int curve, juce::Rectangle<float> area, int points) {
    juce::Path path;
    for(int i = 0; i <= points; ++i) {
        const double x = static_cast<double>(i) / points;
        const float px = area.getX() + static_cast<float>(x) * area.getWidth();
        const float py = area.getBottom() - static_cast<float>(duck::curveValue(curve, x)) * area.getHeight();
        if(i == 0) path.startNewSubPath(px, py); else path.lineTo(px, py);
    }
    path.lineTo(area.getRight(), area.getBottom() - static_cast<float>(duck::curveValue(curve, 0)) * area.getHeight());
    return path;
}
}

DuckLookAndFeel::DuckLookAndFeel() {
    setColour(juce::PopupMenu::backgroundColourId, look::panel);
    setColour(juce::PopupMenu::textColourId, juce::Colour {0xfff0f0f0});
    setColour(juce::PopupMenu::highlightedBackgroundColourId, look::yellow);
    setColour(juce::PopupMenu::highlightedTextColourId, look::ink);
    setColour(juce::TooltipWindow::backgroundColourId, look::panel);
    setColour(juce::TooltipWindow::textColourId, juce::Colour {0xfff0f0f0});
    setColour(juce::AlertWindow::backgroundColourId, look::panel);
    setColour(juce::AlertWindow::textColourId, juce::Colour {0xfff0f0f0});
    setColour(juce::TextButton::buttonColourId, look::panel);
}

void DuckLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float value, float start, float end, juce::Slider& slider) {
    const float labelHeight = static_cast<float>(height) * 0.14f;
    const auto knobArea = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height) - labelHeight);
    const float diameter = std::min(knobArea.getWidth(), knobArea.getHeight());
    const auto bounds = juce::Rectangle<float>(diameter, diameter).withCentre(knobArea.getCentre()).reduced(diameter * 0.03f);
    const auto centre = bounds.getCentre();
    const float radius = bounds.getWidth() * 0.5f, thickness = diameter * 0.045f, arcRadius = radius - thickness * 0.5f;
    const float angle = start + value * (end - start);
    juce::Path track, fill;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0, start, end, true);
    fill.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0, start, angle, true);
    g.setColour(juce::Colour {0xff262626});
    g.strokePath(track, juce::PathStrokeType(thickness));
    g.setColour(look::yellow);
    g.strokePath(fill, juce::PathStrokeType(thickness));
    g.setColour(juce::Colour {0xff2a2a2a});
    g.fillEllipse(bounds.reduced(thickness * 1.6f));
    g.setColour(juce::Colour {0xff202020});
    g.fillEllipse(bounds.reduced(thickness * 1.6f + radius * 0.16f));
    g.setColour(juce::Colour {0xff333333});
    g.fillEllipse(bounds.reduced(thickness * 1.6f + radius * 0.28f));
    g.setColour(juce::Colour {0xff121212});
    g.fillEllipse(juce::Rectangle<float>(radius * 0.5f, radius * 0.5f).withCentre(centre));
    const auto dot = centre + juce::Point<float>(std::sin(angle), -std::cos(angle)) * (radius * 0.74f);
    g.setColour(juce::Colour {0xff5a5a5a});
    g.fillEllipse(juce::Rectangle<float>(diameter * 0.045f, diameter * 0.045f).withCentre(dot));
    drawText(g, slider.getName().toUpperCase() + " " + slider.getTextFromValue(slider.getValue()),
        {static_cast<float>(x), knobArea.getBottom(), static_cast<float>(width), labelHeight}, labelHeight * 0.62f, look::text);
}

void DuckLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float position, float, float,
    juce::Slider::SliderStyle style, juce::Slider& slider) {
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height));
    if(style == juce::Slider::LinearBar) {
        g.setColour(look::panel);
        g.fillRoundedRectangle(bounds, 4);
        g.setColour(look::yellow.withAlpha(0.22f));
        g.fillRoundedRectangle(bounds.withRight(std::max(bounds.getX() + 4, position)), 4);
        drawText(g, slider.getName().toUpperCase() + " " + slider.getTextFromValue(slider.getValue()), bounds, bounds.getHeight() * 0.42f, look::text);
        return;
    }
    const float trackHeight = bounds.getHeight() * 0.62f;
    const auto track = bounds.withSizeKeepingCentre(bounds.getWidth(), trackHeight);
    g.setColour(juce::Colour {0xff262626});
    g.fillRoundedRectangle(track, trackHeight * 0.5f);
    g.setColour(look::yellow);
    g.fillRoundedRectangle(track.withRight(position), trackHeight * 0.5f);
    const float thumb = bounds.getHeight();
    g.setColour(juce::Colour {0xff4a4a4a});
    g.fillEllipse(juce::Rectangle<float>(thumb, thumb).withCentre({position, bounds.getCentreY()}));
    g.setColour(juce::Colour {0xff222222});
    g.fillEllipse(juce::Rectangle<float>(thumb * 0.36f, thumb * 0.36f).withCentre({position, bounds.getCentreY()}));
}

void DuckLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) {
    const auto bounds = button.getLocalBounds().toFloat();
    if(button.getToggleState()) g.setColour(look::yellow);
    else g.setColour(over || down ? look::panelHover : juce::Colours::transparentBlack);
    g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);
}

void DuckLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    drawText(g, button.getButtonText(), button.getLocalBounds().toFloat(), static_cast<float>(button.getHeight()) * 0.48f,
        button.getToggleState() ? look::ink : look::text);
}

void GlyphButton::paintButton(juce::Graphics& g, bool over, bool down) {
    const auto bounds = getLocalBounds().toFloat();
    const bool arrow = glyph == Glyph::Previous || glyph == Glyph::Next || glyph == Glyph::PreviousBank || glyph == Glyph::NextBank;
    const bool lit = getToggleState() && !arrow;
    const auto colour = lit ? look::yellow : (over || down ? juce::Colour {0xffe0e0e0} : look::text);
    if(arrow) {
        g.setColour(over || down ? look::panelHover : look::panel);
        g.fillRoundedRectangle(bounds, 4);
    }
    const auto centre = bounds.getCentre();
    const float size = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    g.setColour(colour);
    juce::Path path;
    switch(glyph) {
        case Glyph::Menu:
            for(int line = -1; line <= 1; ++line) g.fillRect(centre.x - size * 0.9f, centre.y + static_cast<float>(line) * size * 0.55f - 1.5f, size * 1.8f, 3.0f);
            break;
        case Glyph::Previous: case Glyph::Next: {
            const float direction = glyph == Glyph::Next ? 1.0f : -1.0f;
            path.startNewSubPath(centre.x - direction * size * 0.25f, centre.y - size * 0.6f);
            path.lineTo(centre.x + direction * size * 0.25f, centre.y);
            path.lineTo(centre.x - direction * size * 0.25f, centre.y + size * 0.6f);
            g.strokePath(path, juce::PathStrokeType(3, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
        case Glyph::PreviousBank: case Glyph::NextBank: {
            const float direction = glyph == Glyph::NextBank ? 1.0f : -1.0f;
            path.addTriangle(centre.x - direction * size * 0.4f, centre.y - size * 0.6f, centre.x + direction * size * 0.5f, centre.y,
                centre.x - direction * size * 0.4f, centre.y + size * 0.6f);
            g.fillPath(path);
            break;
        }
        case Glyph::Loop: {
            const auto loop = juce::Rectangle<float>(size * 1.5f, size * 0.95f).withCentre(centre);
            g.drawRoundedRectangle(loop, size * 0.45f, 2.2f);
            g.setColour(look::background);
            g.fillRect(juce::Rectangle<float>(size * 0.5f, 6).withCentre({loop.getRight() - size * 0.3f, loop.getY()}));
            g.fillRect(juce::Rectangle<float>(size * 0.5f, 6).withCentre({loop.getX() + size * 0.3f, loop.getBottom()}));
            g.setColour(colour);
            path.addTriangle(loop.getRight() - size * 0.55f, loop.getY() - size * 0.22f, loop.getRight() - size * 0.55f, loop.getY() + size * 0.22f,
                loop.getRight() - size * 0.15f, loop.getY());
            path.addTriangle(loop.getX() + size * 0.55f, loop.getBottom() - size * 0.22f, loop.getX() + size * 0.55f, loop.getBottom() + size * 0.22f,
                loop.getX() + size * 0.15f, loop.getBottom());
            g.fillPath(path);
            break;
        }
        case Glyph::Headphones: {
            const float radius = size * 0.8f;
            path.addCentredArc(centre.x, centre.y + size * 0.1f, radius, radius, 0, -pi * 0.5f, pi * 0.5f, true);
            g.strokePath(path, juce::PathStrokeType(2.4f));
            for(const float side : {-1.0f, 1.0f})
                g.fillRoundedRectangle(juce::Rectangle<float>(size * 0.42f, size * 0.8f).withCentre({centre.x + side * radius, centre.y + size * 0.55f}), 2);
            break;
        }
    }
}

void CurveButton::paintButton(juce::Graphics& g, bool over, bool down) {
    const auto bounds = getLocalBounds().toFloat();
    const bool selected = getToggleState();
    g.setColour(selected ? look::yellow : (over || down ? look::panelHover : look::panel));
    g.fillRoundedRectangle(bounds, 4);
    g.setColour(selected ? look::ink : juce::Colour {0xff8c8c8c});
    g.strokePath(curvePath(curve, bounds.reduced(bounds.getWidth() * 0.2f, bounds.getHeight() * 0.24f), 40),
        juce::PathStrokeType(selected ? 2.6f : 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

CurveDisplay::CurveDisplay(DuckerProcessor& p) : owner(p) {
    setComponentID("display");
    setName("Ducking curve");
    setTooltip("Input (grey) and output (yellow) over one cycle. Drag the handle to change the slope length; double-click to reset.");
}

float CurveDisplay::lengthFraction() const { return owner.parameters.getRawParameterValue("length")->load() * 0.01f; }

bool CurveDisplay::overHandle(juce::Point<float> point) const {
    const float edge = lengthFraction() * static_cast<float>(getWidth());
    return std::abs(point.x - edge) < 7.0f || juce::Rectangle<float>(edge - 36, 6, 36, 30).contains(point);
}

void CurveDisplay::setLength(float fraction) {
    auto* parameter = owner.parameters.getParameter("length");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(juce::jlimit(5.0f, 100.0f, fraction * 100.0f)));
}

void CurveDisplay::mouseMove(const juce::MouseEvent& e) {
    setMouseCursor(overHandle(e.position) ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}
void CurveDisplay::mouseDown(const juce::MouseEvent& e) {
    dragging = overHandle(e.position);
    if(dragging) owner.parameters.getParameter("length")->beginChangeGesture();
}
void CurveDisplay::mouseDrag(const juce::MouseEvent& e) {
    if(dragging) setLength(e.position.x / static_cast<float>(getWidth()));
}
void CurveDisplay::mouseUp(const juce::MouseEvent&) {
    if(!dragging) return;
    dragging = false;
    owner.parameters.getParameter("length")->endChangeGesture();
}
void CurveDisplay::mouseDoubleClick(const juce::MouseEvent& e) {
    if(!overHandle(e.position)) return;
    auto* parameter = owner.parameters.getParameter("length");
    parameter->beginChangeGesture(); parameter->setValueNotifyingHost(1.0f); parameter->endChangeGesture();
}

void CurveDisplay::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    const float width = bounds.getWidth(), height = bounds.getHeight();
    g.setColour(look::display);
    g.fillRoundedRectangle(bounds, 4);
    const float binWidth = width / DuckerProcessor::displayBins, middle = height * 0.5f;
    for(int bin = 0; bin < DuckerProcessor::displayBins; ++bin) {
        const float x = static_cast<float>(bin) * binWidth;
        const float input = juce::jlimit(0.0f, 1.0f, owner.inputBins[static_cast<size_t>(bin)].load(std::memory_order_relaxed));
        const float output = juce::jlimit(0.0f, 1.0f, owner.outputBins[static_cast<size_t>(bin)].load(std::memory_order_relaxed));
        g.setColour(juce::Colour {0xff8d8a92}.withAlpha(0.85f));
        g.fillRect(x, middle - input * height * 0.42f, std::max(1.0f, binWidth - 0.5f), std::max(1.0f, input * height * 0.84f));
        g.setColour(look::darkYellow);
        g.fillRect(x, middle - output * height * 0.42f, std::max(1.0f, binWidth * 0.5f), std::max(1.0f, output * height * 0.84f));
    }
    g.setColour(look::darkYellow.withAlpha(0.8f));
    g.drawHorizontalLine(juce::roundToInt(middle), 0, width);
    const float edge = lengthFraction() * width;
    g.setColour(juce::Colour {0xaa000000});
    g.fillRect(edge, 0.0f, width - edge, height);
    const auto curveArea = juce::Rectangle<float>(0, height * 0.06f, width, height * 0.88f);
    const int curve = static_cast<int>(owner.parameters.getRawParameterValue("curve")->load());
    juce::Path path;
    const float length = lengthFraction();
    for(int px = 0; px <= juce::roundToInt(width); ++px) {
        const float x = static_cast<float>(px);
        const float y = curveArea.getBottom() - static_cast<float>(duck::cycleGain(curve, x / width, length)) * curveArea.getHeight();
        if(px == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
    }
    path.lineTo(width - 1, curveArea.getBottom() - static_cast<float>(duck::cycleGain(curve, 0, length)) * curveArea.getHeight());
    g.setColour(look::yellow);
    g.strokePath(path, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if(owner.visualRunning.load(std::memory_order_relaxed)) {
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillRect(owner.visualPhase.load(std::memory_order_relaxed) * width, 0.0f, 1.5f, height);
    }
    const auto handle = juce::Rectangle<float>(34, 30).withRightX(std::min(width - 2, edge - 2)).withY(6);
    g.setColour(juce::Colour {0xff343434});
    g.fillRoundedRectangle(handle, 3);
    g.setColour(juce::Colour {0xffb0b0b0});
    for(int line = -1; line <= 1; ++line) g.fillRect(handle.getCentreX() + static_cast<float>(line) * 6.0f - 1, handle.getY() + 7, 2.0f, 16.0f);
}

DuckerEditor::DuckerEditor(DuckerProcessor& p) : AudioProcessorEditor(p), owner(p), display(p) {
    setLookAndFeel(&theme);
    addAndMakeVisible(display);
    mix.setComponentID("mix"); mix.setName("Mix");
    mix.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); mix.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    mix.setRotaryParameters(pi * 1.25f, pi * 2.75f, true); mix.setDoubleClickReturnValue(true, 100);
    mix.setTooltip("Blend between the original signal and the ducked signal. Double-click to reset to 100%.");
    addAndMakeVisible(mix);
    mixAttachment = std::make_unique<SliderAttachment>(p.parameters, "mix", mix);
    mix.textFromValueFunction = [](double value) { return juce::String(juce::roundToInt(value)) + "%"; };
    configureBar(band, "bandFrequency", "Band", "Hz", "Band split frequency. With BAND on, only the low band below this frequency is ducked.");
    band.setSliderStyle(juce::Slider::LinearHorizontal); band.setDoubleClickReturnValue(true, 150);
    configureBar(shift, "shift", "Shift", "%", "Offset the curve against the beat. In MIDI and Audio modes this delays the curve after each trigger.");
    configureBar(threshold, "threshold", "Threshold", "dB", "Audio trigger level. Uses the sidechain input when connected, otherwise the main input.");
    threshold.setDoubleClickReturnValue(true, -24);
    const juce::StringArray rateNames {"1/8", "1/4", "1/2", "1/1"};
    for(size_t i = 0; i < rateButtons.size(); ++i) {
        rateButtons[i].setButtonText(rateNames[static_cast<int>(i)]);
        rateButtons[i].setComponentID("rate" + juce::String(static_cast<int>(i)));
        rateButtons[i].setTooltip("Length of one ducking cycle, synced to the host tempo.");
        rateButtons[i].onClick = [this, i] { setChoice("rate", static_cast<int>(i)); };
        addAndMakeVisible(rateButtons[i]);
    }
    const juce::StringArray triggerNames {"SYNC", "MIDI", "AUDIO"};
    const juce::StringArray triggerTips {"Loop the curve with the host transport.", "Restart the curve on every incoming MIDI note.",
        "Restart the curve when the sidechain (or main input) crosses the threshold."};
    for(size_t i = 0; i < triggerButtons.size(); ++i) {
        triggerButtons[i].setButtonText(triggerNames[static_cast<int>(i)]);
        triggerButtons[i].setComponentID("trigger" + juce::String(static_cast<int>(i)));
        triggerButtons[i].setTooltip(triggerTips[static_cast<int>(i)]);
        triggerButtons[i].onClick = [this, i] { setChoice("trigger", static_cast<int>(i)); };
        addAndMakeVisible(triggerButtons[i]);
    }
    bandButton.setButtonText("BAND"); bandButton.setComponentID("band"); bandButton.setClickingTogglesState(true);
    bandButton.setTooltip("Duck only the low band below the band frequency.");
    addAndMakeVisible(bandButton);
    retrigger.setComponentID("retrigger"); retrigger.setClickingTogglesState(true);
    retrigger.setTooltip("Retrigger: a new trigger restarts the curve even while it is running.");
    solo.setComponentID("solo"); solo.setClickingTogglesState(true); solo.setTooltip("Listen to the ducked band only.");
    menu.setComponentID("menu"); menu.setTooltip("Smoothing and reset options.");
    menu.onClick = [this] { showMenu(); };
    previous.setComponentID("previous"); previous.setTooltip("Previous curve."); previous.onClick = [this] { stepCurve(-1); };
    next.setComponentID("next"); next.setTooltip("Next curve."); next.onClick = [this] { stepCurve(1); };
    previousBank.setComponentID("previousBank"); previousBank.setTooltip("Previous row of curves."); previousBank.onClick = [this] { stepCurve(-8); };
    nextBank.setComponentID("nextBank"); nextBank.setTooltip("Next row of curves."); nextBank.onClick = [this] { stepCurve(8); };
    for(auto* button : {&retrigger, &solo, &menu, &previous, &next, &previousBank, &nextBank}) addAndMakeVisible(*button);
    retriggerAttachment = std::make_unique<ButtonAttachment>(p.parameters, "retrigger", retrigger);
    soloAttachment = std::make_unique<ButtonAttachment>(p.parameters, "bandSolo", solo);
    bandButtonAttachment = std::make_unique<ButtonAttachment>(p.parameters, "bandOn", bandButton);
    for(int i = 0; i < duck::curveCount; ++i) {
        auto button = std::make_unique<CurveButton>(i);
        button->setComponentID("curve" + juce::String(i));
        button->setTooltip(juce::String(duck::curveNames[static_cast<size_t>(i)]) + " curve.");
        button->onClick = [this, i] { setChoice("curve", i); };
        addAndMakeVisible(*button);
        curveButtons.push_back(std::move(button));
    }
    const auto selectInGroup = [](auto& buttons, float value) {
        for(size_t i = 0; i < buttons.size(); ++i) buttons[i].setToggleState(static_cast<int>(i) == juce::roundToInt(value), juce::dontSendNotification);
    };
    rateAttachment = std::make_unique<juce::ParameterAttachment>(*p.parameters.getParameter("rate"),
        [this, selectInGroup](float value) { selectInGroup(rateButtons, value); }, nullptr);
    triggerAttachment = std::make_unique<juce::ParameterAttachment>(*p.parameters.getParameter("trigger"),
        [this, selectInGroup](float value) { selectInGroup(triggerButtons, value); repaint(); }, nullptr);
    curveAttachment = std::make_unique<juce::ParameterAttachment>(*p.parameters.getParameter("curve"),
        [this](float value) {
            for(size_t i = 0; i < curveButtons.size(); ++i) curveButtons[i]->setToggleState(static_cast<int>(i) == juce::roundToInt(value), juce::dontSendNotification);
            display.repaint();
        }, nullptr);
    rateAttachment->sendInitialUpdate(); triggerAttachment->sendInitialUpdate(); curveAttachment->sendInitialUpdate();
    setResizable(true, true);
    setResizeLimits(600, 306, 2000, 1020);
    getConstrainer()->setFixedAspectRatio(1000.0 / 510.0);
    setSize(1000, 510);
    startTimerHz(30);
}

DuckerEditor::~DuckerEditor() { stopTimer(); setLookAndFeel(nullptr); }

void DuckerEditor::configureBar(juce::Slider& slider, const char* id, const juce::String& name, const juce::String& unit, const char* tip) {
    slider.setComponentID(id); slider.setName(name); slider.setTooltip(tip);
    slider.setSliderStyle(juce::Slider::LinearBar); slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(slider);
    auto attachment = std::make_unique<SliderAttachment>(owner.parameters, id, slider);
    if(juce::String(id) == "bandFrequency") bandAttachment = std::move(attachment);
    else if(juce::String(id) == "shift") shiftAttachment = std::move(attachment);
    else thresholdAttachment = std::move(attachment);
    slider.textFromValueFunction = [unit](double value) {
        return (unit == "%" ? juce::String(juce::roundToInt(value)) : juce::String(value, unit == "dB" ? 1 : 0)) + (unit == "%" ? "%" : " " + unit);
    };
}

void DuckerEditor::setChoice(const char* id, int index) {
    auto* parameter = owner.parameters.getParameter(id);
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(index)));
    parameter->endChangeGesture();
}

void DuckerEditor::stepCurve(int delta) {
    const int current = static_cast<int>(owner.parameters.getRawParameterValue("curve")->load());
    setChoice("curve", ((current + delta) % duck::curveCount + duck::curveCount) % duck::curveCount);
}

void DuckerEditor::showMenu() {
    juce::PopupMenu popup;
    const int smoothing = static_cast<int>(owner.parameters.getRawParameterValue("smoothing")->load());
    popup.addSectionHeader("Trigger smoothing");
    const juce::StringArray names {"Off", "Low", "Medium", "High"};
    for(int i = 0; i < names.size(); ++i) popup.addItem(10 + i, names[i], true, i == smoothing);
    popup.addSeparator();
    popup.addItem(20, "Reset slope and shift");
    popup.addItem(21, "About openDucker");
    popup.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(menu), [this](int result) {
        if(result >= 10 && result < 14) setChoice("smoothing", result - 10);
        if(result == 20) for(const auto* id : {"length", "shift"}) {
            auto* parameter = owner.parameters.getParameter(id);
            parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->getDefaultValue()); parameter->endChangeGesture();
        }
        if(result == 21) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "openDucker",
            "openDucker v" JucePlugin_VersionString "\nAn independent open-source volume-shaping sidechain ducker.\nAGPL-3.0, github.com/alanmarcero/openDucker");
    });
}

void DuckerEditor::timerCallback() {
    display.repaint();
    repaint(0, 0, getWidth() * 36 / 100, getHeight() * 24 / 100);
}

void DuckerEditor::resized() {
    const float s = static_cast<float>(getWidth()) / 1000.0f;
    const auto area = [s](float x, float y, float w, float h) {
        return juce::Rectangle<float>(x * s, y * s, w * s, h * s).toNearestInt();
    };
    mix.setBounds(area(30, 118, 300, 310));
    bandButton.setBounds(area(18, 436, 82, 32)); solo.setBounds(area(108, 436, 36, 32)); band.setBounds(area(18, 478, 316, 22));
    for(size_t i = 0; i < rateButtons.size(); ++i) rateButtons[i].setBounds(area(357 + static_cast<float>(i) * 64, 12, 62, 33));
    for(size_t i = 0; i < triggerButtons.size(); ++i) triggerButtons[i].setBounds(area(640 + static_cast<float>(i) * 83 + (i == 2 ? 3 : 0), 12, i == 2 ? 94 : 80, 33));
    retrigger.setBounds(area(904, 12, 36, 33)); menu.setBounds(area(952, 12, 38, 33));
    display.setBounds(area(357, 55, 631, 295));
    previousBank.setBounds(area(357, 360, 74, 33)); previous.setBounds(area(437, 360, 74, 33));
    shift.setBounds(area(522, 362, 146, 29)); threshold.setBounds(area(676, 362, 146, 29));
    next.setBounds(area(834, 360, 74, 33)); nextBank.setBounds(area(914, 360, 74, 33));
    for(size_t i = 0; i < curveButtons.size(); ++i)
        curveButtons[i]->setBounds(area(357 + static_cast<float>(i % 8) * 79, i < 8 ? 405 : 453, 74, 43));
}

void DuckerEditor::paint(juce::Graphics& g) {
    g.fillAll(look::background);
    const float s = static_cast<float>(getWidth()) / 1000.0f;
    drawText(g, "open", {44 * s, 24 * s, 270 * s, 34 * s}, 34 * s, juce::Colour {0xff4a4a4a}, juce::Justification::centredLeft);
    drawText(g, "DUCKER 2", {42 * s, 52 * s, 290 * s, 48 * s}, 44 * s, look::yellow, juce::Justification::centredLeft);
    const int state = owner.syncState.load(std::memory_order_relaxed);
    const auto mode = static_cast<int>(owner.parameters.getRawParameterValue("trigger")->load());
    const juce::String status = mode == 1 ? "MIDI triggered" : (mode == 2 ? "Audio triggered"
        : (state == 2 ? juce::String(owner.hostBpm.load(std::memory_order_relaxed), 1) + " BPM / synced" : (state == 1 ? "Host stopped" : "No host sync")));
    drawText(g, status, {46 * s, 98 * s, 280 * s, 18 * s}, 12 * s, juce::Colour {0xff707070}, juce::Justification::centredLeft, false);
}
