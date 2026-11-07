#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace {
constexpr float pi = juce::MathConstants<float>::pi;
constexpr int collapsedHeight = 510;
constexpr int expandedHeight = 724;

void drawText(juce::Graphics& g, const juce::String& value, juce::Rectangle<float> bounds, float size, juce::Colour colour,
    juce::Justification alignment = juce::Justification::centred, bool bold = true) {
    g.setColour(colour);
    g.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    g.drawText(value, bounds, alignment, false);
}

void glow(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour) {
    const juce::Graphics::ScopedSaveState saved(g);
    const auto centre = bounds.getCentre();
    const float radius = bounds.getWidth() * 0.5f;
    g.addTransform(juce::AffineTransform::scale(1, bounds.getHeight() / bounds.getWidth(), centre.x, centre.y));
    g.setGradientFill(juce::ColourGradient(colour, centre, colour.withAlpha(0.0f), centre.translated(radius, 0), true));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2, radius * 2);
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
    setColour(juce::ComboBox::backgroundColourId, look::panel);
    setColour(juce::ComboBox::textColourId, look::paper);
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::arrowColourId, look::mint);
    setColour(juce::PopupMenu::backgroundColourId, look::panel);
    setColour(juce::PopupMenu::textColourId, look::paper);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, look::mint);
    setColour(juce::PopupMenu::highlightedTextColourId, look::ink);
    setColour(juce::TooltipWindow::backgroundColourId, look::panel);
    setColour(juce::TooltipWindow::textColourId, look::paper);
    setColour(juce::AlertWindow::backgroundColourId, look::panel);
    setColour(juce::AlertWindow::textColourId, look::paper);
    setColour(juce::TextButton::buttonColourId, look::panel);
}

void DuckLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) {
    const juce::Rectangle<float> bounds {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
    const float corner = bounds.getHeight() * 0.25f;
    g.setColour(look::panel);
    g.fillRoundedRectangle(bounds, corner);
    g.setColour(look::muted.withAlpha(0.28f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    const float arrowX = bounds.getRight() - bounds.getHeight() * 0.55f;
    const float arrowY = bounds.getCentreY();
    const float arrowSize = bounds.getHeight() * 0.14f;
    juce::Path arrow;
    arrow.startNewSubPath(arrowX - arrowSize, arrowY - arrowSize * 0.5f);
    arrow.lineTo(arrowX, arrowY + arrowSize * 0.6f);
    arrow.lineTo(arrowX + arrowSize, arrowY - arrowSize * 0.5f);
    g.setColour(box.isEnabled() ? look::mint : look::muted.withAlpha(0.4f));
    g.strokePath(arrow, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void DuckLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float value, float start, float end, juce::Slider& slider) {
    if(slider.getComponentID() != "mix") {
        const float s = static_cast<float>(height) / 104.0f;
        const juce::Point<float> centre {static_cast<float>(x) + static_cast<float>(width) * 0.5f, static_cast<float>(y) + 28.0f * s};
        const float radius = 21.5f * s;
        const float angle = start + value * (end - start);
        const float origin = slider.getMinimum() < 0 ? start + (end - start) * static_cast<float>(slider.valueToProportionOfLength(0)) : start;
        juce::Path track, arc;
        track.addCentredArc(centre.x, centre.y, radius, radius, 0, start, end, true);
        arc.addCentredArc(centre.x, centre.y, radius, radius, 0, origin, angle, true);
        g.setColour(look::panel.darker(0.35f));
        g.strokePath(track, juce::PathStrokeType(3.0f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(look::mint);
        g.strokePath(arc, juce::PathStrokeType(3.0f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const juce::Rectangle<float> body {centre.x - 16.5f * s, centre.y - 16.5f * s, 33.0f * s, 33.0f * s};
        g.setGradientFill(juce::ColourGradient(look::panel.brighter(0.25f), body.getTopLeft(), look::background.darker(0.3f), body.getBottomRight(), false));
        g.fillEllipse(body);
        g.setColour(look::muted.withAlpha(0.3f));
        g.drawEllipse(body, 1.0f);
        const auto direction = juce::Point<float>(std::sin(angle), -std::cos(angle));
        g.setColour(look::yellow);
        g.drawLine({centre + direction * (9.5f * s), centre + direction * (15.5f * s)}, 2.0f * s);
        drawText(g, slider.getTextFromValue(slider.getValue()), {static_cast<float>(x), static_cast<float>(y) + 55.0f * s, static_cast<float>(width), 19.0f * s}, 12.5f * s, look::paper, juce::Justification::centred, true);
        drawText(g, slider.getName(), {static_cast<float>(x), static_cast<float>(y) + 75.0f * s, static_cast<float>(width), 17.0f * s}, 11.5f * s, look::muted, juce::Justification::centred, false);
        return;
    }

    const float labelHeight = static_cast<float>(height) * 0.16f;
    const auto knobArea = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height) - labelHeight);
    const float diameter = std::min(knobArea.getWidth(), knobArea.getHeight());
    const auto bounds = juce::Rectangle<float>(diameter, diameter).withCentre(knobArea.getCentre()).reduced(diameter * 0.04f);
    const auto centre = bounds.getCentre();
    const float radius = bounds.getWidth() * 0.5f, thickness = diameter * 0.055f, arcRadius = radius - thickness * 0.5f;
    const float angle = start + value * (end - start);

    // Track background
    juce::Path track, fill;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0, start, end, true);
    fill.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0, start, angle, true);
    g.setColour(look::panel.darker(0.35f));
    g.strokePath(track, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Filled arc with subtle glow
    glow(g, bounds.expanded(thickness * 0.8f), look::mint.withAlpha(0.18f * value));
    g.setColour(look::mint);
    g.strokePath(fill, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Outer ring ticks
    for(int tick = 0; tick <= 24; ++tick) {
        const float tickAngle = start + static_cast<float>(tick) / 24.0f * (end - start);
        const auto dir = juce::Point<float>(std::sin(tickAngle), -std::cos(tickAngle));
        g.setColour(look::muted.withAlpha(tick % 4 == 0 ? 0.5f : 0.2f));
        g.drawLine({centre + dir * (radius + 2), centre + dir * (radius + (tick % 4 == 0 ? 6 : 4))}, 1.0f);
    }

    // Body of the knob
    const auto body = bounds.reduced(thickness * 1.4f);
    g.setGradientFill(juce::ColourGradient(look::panel.brighter(0.2f), body.getTopLeft(), look::background.darker(0.3f), body.getBottomRight(), false));
    g.fillEllipse(body);
    g.setColour(look::muted.withAlpha(0.35f));
    g.drawEllipse(body, 1.0f);
    g.setColour(look::panel.darker(0.5f));
    g.fillEllipse(body.reduced(body.getWidth() * 0.12f));

    // Radial indicator needle
    const auto needleTip = centre + juce::Point<float>(std::sin(angle), -std::cos(angle)) * (radius * 0.68f);
    const auto needleBase = centre + juce::Point<float>(std::sin(angle), -std::cos(angle)) * (radius * 0.38f);
    g.setColour(look::yellow);
    g.drawLine({needleBase, needleTip}, 3.0f);

    // Label below
    const juce::String valText = slider.getTextFromValue(slider.getValue());
    drawText(g, valText, {static_cast<float>(x), knobArea.getBottom() - 4, static_cast<float>(width), labelHeight * 0.65f}, labelHeight * 0.62f, look::paper, juce::Justification::centred, true);
    drawText(g, slider.getName().toUpperCase(), {static_cast<float>(x), knobArea.getBottom() + labelHeight * 0.5f, static_cast<float>(width), labelHeight * 0.45f}, labelHeight * 0.42f, look::muted, juce::Justification::centred, false);
}

void DuckLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float position, float, float,
    juce::Slider::SliderStyle style, juce::Slider& slider) {
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height));
    if(style == juce::Slider::LinearBar) {
        g.setColour(look::panel);
        g.fillRoundedRectangle(bounds, 4);
        g.setColour(look::mint.withAlpha(0.22f));
        g.fillRoundedRectangle(bounds.withRight(std::max(bounds.getX() + 4, position)), 4);
        g.setColour(look::muted.withAlpha(0.3f));
        g.drawRoundedRectangle(bounds, 4, 1);
        drawText(g, slider.getName().toUpperCase() + " " + slider.getTextFromValue(slider.getValue()), bounds, bounds.getHeight() * 0.42f, look::paper, juce::Justification::centred, true);
        return;
    }
    const float trackHeight = bounds.getHeight() * 0.55f;
    const auto track = bounds.withSizeKeepingCentre(bounds.getWidth(), trackHeight);
    g.setColour(look::panel.darker(0.3f));
    g.fillRoundedRectangle(track, trackHeight * 0.5f);
    g.setColour(look::mint);
    g.fillRoundedRectangle(track.withRight(position), trackHeight * 0.5f);
    const float thumb = bounds.getHeight();
    g.setColour(look::panel.brighter(0.3f));
    g.fillEllipse(juce::Rectangle<float>(thumb, thumb).withCentre({position, bounds.getCentreY()}));
    g.setColour(look::yellow);
    g.fillEllipse(juce::Rectangle<float>(thumb * 0.4f, thumb * 0.4f).withCentre({position, bounds.getCentreY()}));
}

void DuckLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) {
    const auto bounds = button.getLocalBounds().toFloat();
    if(button.getToggleState()) {
        g.setColour(look::yellow);
        g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.45f);
    } else {
        g.setColour(over || down ? look::panelHover : look::panel);
        g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.45f);
        g.setColour(look::muted.withAlpha(0.25f));
        g.drawRoundedRectangle(bounds, bounds.getHeight() * 0.45f, 1);
    }
}

void DuckLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    drawText(g, button.getButtonText(), button.getLocalBounds().toFloat(), static_cast<float>(button.getHeight()) * 0.46f,
        button.getToggleState() ? look::ink : look::paper);
}

void GlyphButton::paintButton(juce::Graphics& g, bool over, bool down) {
    const auto bounds = getLocalBounds().toFloat();
    const bool arrow = glyph == Glyph::Previous || glyph == Glyph::Next || glyph == Glyph::PreviousBank || glyph == Glyph::NextBank;
    const bool lit = getToggleState() && !arrow;
    const auto colour = lit ? look::ink : (over || down ? look::paper : look::muted);
    if(arrow) {
        g.setColour(over || down ? look::panelHover : look::panel);
        g.fillRoundedRectangle(bounds, 4);
        g.setColour(look::muted.withAlpha(0.25f));
        g.drawRoundedRectangle(bounds, 4, 1);
    } else if(lit) {
        g.setColour(look::yellow);
        g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.45f);
    } else {
        g.setColour(over || down ? look::panelHover : look::panel);
        g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.45f);
        g.setColour(look::muted.withAlpha(0.25f));
        g.drawRoundedRectangle(bounds, bounds.getHeight() * 0.45f, 1);
    }
    const auto centre = bounds.getCentre();
    const float size = std::min(bounds.getWidth(), bounds.getHeight()) * 0.46f;
    g.setColour(colour);
    juce::Path path;
    switch(glyph) {
        case Glyph::Menu:
            for(int line = -1; line <= 1; ++line) g.fillRect(centre.x - size * 0.85f, centre.y + static_cast<float>(line) * size * 0.55f - 1.2f, size * 1.7f, 2.5f);
            break;
        case Glyph::Previous: case Glyph::Next: {
            const float direction = glyph == Glyph::Next ? 1.0f : -1.0f;
            path.startNewSubPath(centre.x - direction * size * 0.25f, centre.y - size * 0.55f);
            path.lineTo(centre.x + direction * size * 0.25f, centre.y);
            path.lineTo(centre.x - direction * size * 0.25f, centre.y + size * 0.55f);
            g.strokePath(path, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
        case Glyph::PreviousBank: case Glyph::NextBank: {
            const float direction = glyph == Glyph::NextBank ? 1.0f : -1.0f;
            path.addTriangle(centre.x - direction * size * 0.35f, centre.y - size * 0.55f, centre.x + direction * size * 0.45f, centre.y,
                centre.x - direction * size * 0.35f, centre.y + size * 0.55f);
            g.fillPath(path);
            break;
        }
        case Glyph::Loop: {
            const auto loop = juce::Rectangle<float>(size * 1.4f, size * 0.9f).withCentre(centre);
            g.drawRoundedRectangle(loop, size * 0.4f, 2.0f);
            g.setColour(lit ? look::yellow : look::panel);
            g.fillRect(juce::Rectangle<float>(size * 0.5f, 5).withCentre({loop.getRight() - size * 0.28f, loop.getY()}));
            g.fillRect(juce::Rectangle<float>(size * 0.5f, 5).withCentre({loop.getX() + size * 0.28f, loop.getBottom()}));
            g.setColour(colour);
            path.addTriangle(loop.getRight() - size * 0.5f, loop.getY() - size * 0.2f, loop.getRight() - size * 0.5f, loop.getY() + size * 0.2f,
                loop.getRight() - size * 0.12f, loop.getY());
            path.addTriangle(loop.getX() + size * 0.5f, loop.getBottom() - size * 0.2f, loop.getX() + size * 0.5f, loop.getBottom() + size * 0.2f,
                loop.getX() + size * 0.12f, loop.getBottom());
            g.fillPath(path);
            break;
        }
        case Glyph::Headphones: {
            const float radius = size * 0.75f;
            path.addCentredArc(centre.x, centre.y + size * 0.1f, radius, radius, 0, -pi * 0.5f, pi * 0.5f, true);
            g.strokePath(path, juce::PathStrokeType(2.2f));
            for(const float side : {-1.0f, 1.0f})
                g.fillRoundedRectangle(juce::Rectangle<float>(size * 0.38f, size * 0.72f).withCentre({centre.x + side * radius, centre.y + size * 0.5f}), 2);
            break;
        }
    }
}

void CurveButton::paintButton(juce::Graphics& g, bool over, bool down) {
    const auto bounds = getLocalBounds().toFloat();
    const bool selected = getToggleState();
    g.setColour(selected ? look::yellow : (over || down ? look::panelHover : look::panel));
    g.fillRoundedRectangle(bounds, 5);
    g.setColour(selected ? look::yellow.brighter(0.2f) : look::muted.withAlpha(0.2f));
    g.drawRoundedRectangle(bounds, 5, 1);
    g.setColour(selected ? look::ink : look::paper.withAlpha(0.75f));
    g.strokePath(curvePath(curve, bounds.reduced(bounds.getWidth() * 0.18f, bounds.getHeight() * 0.22f), 36),
        juce::PathStrokeType(selected ? 2.6f : 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

CurveDisplay::CurveDisplay(DuckerProcessor& p) : owner(p) {
    setComponentID("display");
    setName("Ducking curve");
    setTooltip("Input (grey) and output (yellow) waveforms over one cycle. Drag the handle to adjust slope length; double-click to reset.");
}

float CurveDisplay::lengthFraction() const { return owner.parameters.getRawParameterValue("length")->load() * 0.01f; }

bool CurveDisplay::overHandle(juce::Point<float> point) const {
    const float edge = lengthFraction() * static_cast<float>(getWidth());
    return std::abs(point.x - edge) < 8.0f || juce::Rectangle<float>(edge - 36, 6, 36, 30).contains(point);
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

    // Background screen
    g.setColour(look::display);
    g.fillRoundedRectangle(bounds, 6);
    g.setColour(look::muted.withAlpha(0.25f));
    g.drawRoundedRectangle(bounds, 6, 1);

    // Subtle beat division grid
    g.setColour(look::muted.withAlpha(0.12f));
    for(int beat = 1; beat < 4; ++beat) {
        const float x = width * static_cast<float>(beat) / 4.0f;
        g.drawVerticalLine(juce::roundToInt(x), bounds.getY() + 4, bounds.getBottom() - 4);
    }
    const float middle = height * 0.5f;
    g.drawHorizontalLine(juce::roundToInt(middle), 4, width - 4);

    // Waveform bars
    const float binWidth = width / static_cast<float>(DuckerProcessor::displayBins);
    for(int bin = 0; bin < DuckerProcessor::displayBins; ++bin) {
        const float x = static_cast<float>(bin) * binWidth;
        const float input = juce::jlimit(0.0f, 1.0f, owner.inputBins[static_cast<size_t>(bin)].load(std::memory_order_relaxed));
        const float output = juce::jlimit(0.0f, 1.0f, owner.outputBins[static_cast<size_t>(bin)].load(std::memory_order_relaxed));

        // Input waveform in muted slate
        g.setColour(look::muted.withAlpha(0.45f));
        g.fillRect(x, middle - input * height * 0.42f, std::max(1.0f, binWidth - 0.5f), std::max(1.0f, input * height * 0.84f));

        // Output waveform in warm yellow/amber
        g.setColour(look::yellow.withAlpha(0.65f));
        g.fillRect(x, middle - output * height * 0.42f, std::max(1.0f, binWidth * 0.55f), std::max(1.0f, output * height * 0.84f));
    }

    // Inactive shaded region past slope length
    const float edge = lengthFraction() * width;
    g.setColour(juce::Colour {0x88000000});
    g.fillRect(edge, 0.0f, width - edge, height);

    // Ducking curve
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

    // Glowing stroke
    glow(g, bounds, look::yellow.withAlpha(0.06f));
    g.setColour(look::yellow);
    g.strokePath(path, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Live playhead cursor
    if(owner.visualRunning.load(std::memory_order_relaxed)) {
        const float playheadX = owner.visualPhase.load(std::memory_order_relaxed) * width;
        g.setColour(look::mint);
        g.fillRect(playheadX, 0.0f, 2.0f, height);
    }

    // Draggable slope handle
    const auto handle = juce::Rectangle<float>(32, 28).withRightX(std::min(width - 2, edge - 2)).withY(6);
    g.setColour(look::panel.brighter(0.15f));
    g.fillRoundedRectangle(handle, 4);
    g.setColour(look::muted.withAlpha(0.4f));
    g.drawRoundedRectangle(handle, 4, 1);
    g.setColour(look::paper);
    for(int line = -1; line <= 1; ++line)
        g.fillRect(handle.getCentreX() + static_cast<float>(line) * 5.0f - 1, handle.getY() + 6, 2.0f, 16.0f);
}

DuckerEditor::DuckerEditor(DuckerProcessor& p) : AudioProcessorEditor(p), owner(p), display(p), effectsPanel(p) {
    setLookAndFeel(&theme);
    mascot = juce::ImageCache::getFromMemory(BinaryData::djduck_png, BinaryData::djduck_pngSize);
    faceplate = juce::ImageCache::getFromMemory(BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize);

    addAndMakeVisible(display);
    addAndMakeVisible(effectsPanel);
    addAndMakeVisible(effectsToggle);
    effectsToggle.setComponentID("effectsToggle");
    effectsToggle.setButtonText("Hide FX");
    effectsToggle.setTooltip("Show or hide the effects controls. Audio processing stays active.");
    effectsToggle.onClick = [this] {
        effectsExpanded = !effectsExpanded;
        effectsPanel.setVisible(effectsExpanded);
        effectsToggle.setButtonText(effectsExpanded ? "Hide FX" : "Show FX");
        const int targetHeight = effectsExpanded ? expandedHeight : collapsedHeight;
        const double ratio = 1000.0 / static_cast<double>(targetHeight);
        setResizeLimits(600, juce::roundToInt(600.0 / ratio), 2000, targetHeight * 2);
        getConstrainer()->setFixedAspectRatio(ratio);
        setSize(getWidth(), juce::roundToInt(static_cast<double>(getWidth()) / ratio));
    };

    mix.setComponentID("mix"); mix.setName("Mix");
    mix.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); mix.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    mix.setRotaryParameters(pi * 1.25f, pi * 2.75f, true); mix.setDoubleClickReturnValue(true, 100);
    mix.setTooltip("Blend between the original signal and the ducked signal. Double-click to reset to 100%.");
    addAndMakeVisible(mix);
    mixAttachment = std::make_unique<SliderAttachment>(p.parameters, "mix", mix);
    mix.textFromValueFunction = [](double value) { return juce::String(juce::roundToInt(value)) + "%"; };

    configureBar(band, "bandFrequency", "Band", "Hz", "Band split frequency. With BAND on, only frequencies below this cutoff are ducked.");
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
    retrigger.setTooltip("Retrigger: when on, incoming triggers restart an active curve immediately.");
    solo.setComponentID("solo"); solo.setClickingTogglesState(true); solo.setTooltip("Listen to the isolated ducked low band.");
    menu.setComponentID("menu"); menu.setTooltip("Trigger smoothing and utility options.");
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
    setResizeLimits(600, 434, 2000, expandedHeight * 2);
    getConstrainer()->setFixedAspectRatio(1000.0 / static_cast<double>(expandedHeight));
    setSize(1000, expandedHeight);
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
    popup.addItem(21, "About openDucker 2");
    popup.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(menu), [this](int result) {
        if(result >= 10 && result < 14) setChoice("smoothing", result - 10);
        if(result == 20) for(const auto* id : {"length", "shift"}) {
            auto* parameter = owner.parameters.getParameter(id);
            parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->getDefaultValue()); parameter->endChangeGesture();
        }
        if(result == 21) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "openDucker 2",
            "openDucker 2\nFind your bounce with DJ Duck.\nAGPL-3.0, github.com/alanmarcero/openDucker");
    });
}

void DuckerEditor::timerCallback() {
    display.repaint();
    effectsPanel.refresh();
    repaint(0, 0, getWidth() * 35 / 100, juce::roundToInt(280.0f * static_cast<float>(getWidth()) / 1000.0f));
}

void DuckerEditor::resized() {
    const float s = static_cast<float>(getWidth()) / 1000.0f;
    const auto area = [s](float x, float y, float w, float h) {
        return juce::Rectangle<float>(x * s, y * s, w * s, h * s).toNearestInt();
    };

    // Left column: DJ Duck is at y=56..240, Mix knob at y=244..424, Band & FX toggle section at y=430..498
    mix.setBounds(area(70, 244, 204, 180));
    bandButton.setBounds(area(22, 434, 76, 30));
    solo.setBounds(area(104, 434, 38, 30));
    effectsToggle.setBounds(area(242, 434, 86, 30));
    band.setBounds(area(22, 472, 306, 26));

    // Right column
    for(size_t i = 0; i < rateButtons.size(); ++i) rateButtons[i].setBounds(area(354 + static_cast<float>(i) * 64, 12, 60, 32));
    for(size_t i = 0; i < triggerButtons.size(); ++i) triggerButtons[i].setBounds(area(636 + static_cast<float>(i) * 82, 12, 78, 32));
    retrigger.setBounds(area(898, 12, 38, 32));
    menu.setBounds(area(944, 12, 40, 32));

    display.setBounds(area(354, 54, 630, 290));

    previousBank.setBounds(area(354, 356, 70, 30));
    previous.setBounds(area(430, 356, 70, 30));
    shift.setBounds(area(508, 356, 156, 30));
    threshold.setBounds(area(672, 356, 156, 30));
    next.setBounds(area(836, 356, 70, 30));
    nextBank.setBounds(area(914, 356, 70, 30));

    for(size_t i = 0; i < curveButtons.size(); ++i)
        curveButtons[i]->setBounds(area(354 + static_cast<float>(i % 8) * 79, i < 8 ? 398 : 450, 74, 44));

    effectsPanel.setBounds(area(22, 516, 962, 198));
}

void DuckerEditor::paintDuckBooth(juce::Graphics& g, juce::Rectangle<float> area, float s) {
    const bool running = owner.visualRunning.load(std::memory_order_relaxed);
    const float phase = owner.visualPhase.load(std::memory_order_relaxed);
    const float pump = running ? std::pow(1.0f - juce::jlimit(0.0f, 1.0f, phase), 2.8f) : 0.0f;

    // Speaker cabinet behind DJ Duck
    const auto cabinet = area.reduced(area.getWidth() * 0.08f, area.getHeight() * 0.03f);
    g.setGradientFill(juce::ColourGradient(juce::Colour {0xff222e34}, cabinet.getTopLeft(), juce::Colour {0xff0e1518}, cabinet.getBottomRight(), false));
    g.fillRoundedRectangle(cabinet, 10 * s);
    g.setColour(look::muted.withAlpha(0.25f));
    g.drawRoundedRectangle(cabinet, 10 * s, 1.0f);

    // Glowing speaker rim & cone
    const auto centre = cabinet.getCentre();
    const float rimRadius = cabinet.getHeight() * 0.42f;
    glow(g, juce::Rectangle<float>(rimRadius * 2, rimRadius * 2).withCentre(centre).expanded(12 * s), look::mint.withAlpha(0.12f + pump * 0.35f));
    g.setColour(juce::Colour {0xff151e22});
    g.fillEllipse(juce::Rectangle<float>(rimRadius * 2, rimRadius * 2).withCentre(centre));
    g.setColour(look::mint.withAlpha(0.25f + pump * 0.5f));
    g.drawEllipse(juce::Rectangle<float>(rimRadius * 2, rimRadius * 2).withCentre(centre).reduced(3 * s), 2.0f);

    // Animated cone that pulses
    const float coneRadius = (rimRadius * 0.72f) + pump * 7.0f * s;
    g.setGradientFill(juce::ColourGradient(juce::Colour {0xff090f12}, centre, juce::Colour {0xff2c3a42}, centre.translated(coneRadius, 0), true));
    g.fillEllipse(juce::Rectangle<float>(coneRadius * 2, coneRadius * 2).withCentre(centre));

    // Concentric grooves
    for(int ring = 1; ring <= 3; ++ring) {
        g.setColour(look::muted.withAlpha(0.15f));
        g.drawEllipse(juce::Rectangle<float>(coneRadius * 2, coneRadius * 2).withCentre(centre).reduced(static_cast<float>(ring) * 5.0f * s), 1.0f);
    }

    // DJ Duck Mascot character bouncing slightly with pump!
    {
        const juce::Graphics::ScopedSaveState duckState(g);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        const auto duckBounds = cabinet.reduced(6 * s).translated(0, -4.0f * pump * s);
        g.setOpacity(1.0f); g.drawImage(mascot, duckBounds, juce::RectanglePlacement::centred);
    }
}

void DuckerEditor::paint(juce::Graphics& g) {
    g.fillAll(look::background);

    // Subtle brushed analog-cyber faceplate texture overlay
    if(faceplate.isValid()) {
        g.setOpacity(0.48f);
        g.drawImage(faceplate, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
    }

    const float s = static_cast<float>(getWidth()) / 1000.0f;

    // Header branding
    drawText(g, "open", {24 * s, 10 * s, 65 * s, 26 * s}, 24 * s, look::paper, juce::Justification::centredLeft, true);
    drawText(g, "Ducker 2", {86 * s, 10 * s, 160 * s, 26 * s}, 24 * s, look::yellow, juce::Justification::centredLeft, true);
    drawText(g, "Find your bounce.", {25 * s, 35 * s, 180 * s, 16 * s}, 12 * s, look::muted, juce::Justification::centredLeft, false);

    // Host / Trigger Sync Status Pill
    const int state = owner.syncState.load(std::memory_order_relaxed);
    const auto mode = static_cast<int>(owner.parameters.getRawParameterValue("trigger")->load());
    const bool isSynced = (mode == 0 && state == 2) || (mode != 0 && owner.visualRunning.load(std::memory_order_relaxed));
    const juce::String status = mode == 1 ? "MIDI Trigger" : (mode == 2 ? "Audio Trigger"
        : (state == 2 ? juce::String(owner.hostBpm.load(std::memory_order_relaxed), 1) + " BPM" : (state == 1 ? "Host Stopped" : "No Sync")));

    const auto statusPill = juce::Rectangle<float>(216 * s, 16 * s, 116 * s, 22 * s);
    g.setColour(look::panel.darker(0.2f));
    g.fillRoundedRectangle(statusPill, statusPill.getHeight() * 0.5f);
    g.setColour(look::muted.withAlpha(0.2f));
    g.drawRoundedRectangle(statusPill, statusPill.getHeight() * 0.5f, 1.0f);

    // Status LED dot
    g.setColour(isSynced ? look::mint : look::muted.withAlpha(0.5f));
    g.fillEllipse(statusPill.getX() + 8 * s, statusPill.getCentreY() - 3.5f * s, 7 * s, 7 * s);
    if(isSynced) glow(g, juce::Rectangle<float>(12 * s, 12 * s).withCentre({statusPill.getX() + 11.5f * s, statusPill.getCentreY()}), look::mint.withAlpha(0.4f));

    drawText(g, status, {statusPill.getX() + 18 * s, statusPill.getY(), statusPill.getWidth() - 22 * s, statusPill.getHeight()}, 11 * s, look::paper, juce::Justification::centred, false);

    // DJ Duck Booth Stage
    paintDuckBooth(g, {22 * s, 54 * s, 310 * s, 182 * s}, s);

    // Subtle divider between left column and right tools
    g.setColour(look::muted.withAlpha(0.18f));
    g.drawVerticalLine(juce::roundToInt(342 * s), 16 * s, 498 * s);
    if(effectsExpanded) g.drawHorizontalLine(juce::roundToInt(508 * s), 22 * s, 984 * s);
}
