#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace {
const juce::Colour background{0xff151b20},panel{0xff202a30},mint{0xff8fe2ce},yellow{0xffffcf53},paper{0xfff2f1e8},muted{0xff9eadb5};
void text(juce::Graphics& g,const juce::String& value,juce::Rectangle<int> bounds,float size,juce::Colour colour,int alignment=juce::Justification::centredLeft,bool bold=false) {
    g.setColour(colour);g.setFont(juce::FontOptions(size,bold ? juce::Font::bold : juce::Font::plain));
    g.drawText(value,bounds,alignment);
}
}
DuckLookAndFeel::DuckLookAndFeel() {
    setColour(juce::TextButton::textColourOffId,muted);setColour(juce::TextButton::textColourOnId,background);
    setColour(juce::ComboBox::backgroundColourId,panel);setColour(juce::ComboBox::textColourId,paper);
    setColour(juce::ComboBox::outlineColourId,juce::Colours::transparentBlack);
    setColour(juce::ComboBox::arrowColourId,mint);setColour(juce::PopupMenu::backgroundColourId,panel);
    setColour(juce::PopupMenu::textColourId,paper);setColour(juce::PopupMenu::highlightedBackgroundColourId,mint);
    setColour(juce::PopupMenu::highlightedTextColourId,background);
    setColour(juce::TooltipWindow::backgroundColourId,panel);setColour(juce::TooltipWindow::textColourId,paper);
}
void DuckLookAndFeel::drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down) {
    g.setColour(b.getToggleState() ? mint : (over || down ? panel.brighter(0.15f) : panel));
    g.fillRoundedRectangle(b.getLocalBounds().toFloat(),8);
    if(b.hasKeyboardFocus(true)) {g.setColour(yellow);g.drawRoundedRectangle(b.getLocalBounds().toFloat().reduced(1),8,2);}
}
void DuckLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float value,float start,float end,juce::Slider& slider) {
    const auto bounds=juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),static_cast<float>(width),static_cast<float>(height)).reduced(15);
    const auto centre=bounds.getCentre();const float radius=std::min(bounds.getWidth(),bounds.getHeight())*0.5f;
    juce::Path track,arc;
    track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
    arc.addCentredArc(centre.x,centre.y,radius,radius,0,start,start+value*(end-start),true);
    g.setColour(panel.brighter(0.12f));g.strokePath(track,juce::PathStrokeType(6,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.setColour(mint);g.strokePath(arc,juce::PathStrokeType(6,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    const auto body=bounds.reduced(13);
    g.setGradientFill(juce::ColourGradient(panel.brighter(0.12f),body.getTopLeft(),background,body.getBottomRight(),false));g.fillEllipse(body);
    g.setColour(panel.brighter(0.35f));g.drawEllipse(body,1);
    const float angle=start+value*(end-start);
    const auto a=centre+juce::Point<float>(std::sin(angle),-std::cos(angle))*(radius-22);
    const auto b=centre+juce::Point<float>(std::sin(angle),-std::cos(angle))*(radius-34);
    g.setColour(yellow);g.drawLine({a,b},4);
    text(g,juce::String(juce::roundToInt(slider.getValue()))+"%",{x,y+height/2-24,width,44},36,paper,juce::Justification::centred,true);
    text(g,"Amount",{x,y+height/2+21,width,22},14,muted,juce::Justification::centred);
    if(slider.hasKeyboardFocus(true)) {g.setColour(yellow);g.drawEllipse(bounds.expanded(6),1);}
}
DuckerEditor::DuckerEditor(DuckerProcessor& p):AudioProcessorEditor(p),owner(p) {
    setLookAndFeel(&theme);
    mascot=juce::ImageCache::getFromMemory(BinaryData::djduck_png,BinaryData::djduck_pngSize);
    amount.setComponentID("amount");amount.setName("Ducking amount");
    amount.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);amount.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    amount.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
    amount.setDoubleClickReturnValue(true,65);amount.setWantsKeyboardFocus(true);
    amount.setTooltip("Ducking depth. 0% is dry; 100% reaches silence on each beat. Double-click resets to 65%.");
    addAndMakeVisible(amount);
    amountAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,"amount",amount);
    const std::array<juce::String,4> names{"Smooth","Punch","Snap","Swell"};
    for(size_t i=0;i<slopes.size();++i) {
        auto& button=slopes[i];button.setButtonText(names[i]);button.setComponentID("shape"+juce::String(static_cast<int>(i)));
        button.setTooltip("Select the "+names[i].toLowerCase()+" recovery slope.");
        button.onClick=[this,i] {auto* parameter=owner.parameters.getParameter("shape");parameter->beginChangeGesture();parameter->setValueNotifyingHost(static_cast<float>(i)/3.0f);parameter->endChangeGesture();timerCallback();};
        addAndMakeVisible(button);
    }
    shapeAttachment=std::make_unique<juce::ParameterAttachment>(*p.parameters.getParameter("shape"),[this](float value) {
        for(size_t i=0;i<slopes.size();++i) slopes[i].setToggleState(static_cast<int>(i)==juce::roundToInt(value),juce::dontSendNotification);
        repaint();
    },nullptr);
    shapeAttachment->sendInitialUpdate();
    rate.setComponentID("rate");rate.setName("Ducking rate");rate.setTooltip("Host-synced note length. Triplet and dotted rates use quarter-note host timing.");
    rate.addItemList({"1/2","1/4","1/8","1/16","1/4 triplet","1/8 dotted"},1);addAndMakeVisible(rate);
    rateAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,"rate",rate);
    bypass.setComponentID("bypass");bypass.setName("Bypass");bypass.setClickingTogglesState(true);bypass.setTooltip("Bypass the ducking effect.");addAndMakeVisible(bypass);
    bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"bypass",bypass);
    bypass.onStateChange=[this] {bypass.setButtonText(bypass.getToggleState() ? "Bypassed" : "On");};
    setSize(720,460);timerCallback();startTimerHz(30);
}
DuckerEditor::~DuckerEditor() {stopTimer();setLookAndFeel(nullptr);}
void DuckerEditor::timerCallback() {
    bypass.setButtonText(bypass.getToggleState() ? "Bypassed" : "On");
    repaint();
}
void DuckerEditor::resized() {
    amount.setBounds(65,76,230,215);
    for(size_t i=0;i<slopes.size();++i) slopes[i].setBounds(34+static_cast<int>(i)*84,306,76,34);
    rate.setBounds(560,306,126,34);bypass.setBounds(606,24,80,30);
}
void DuckerEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    text(g,"openDucker",{34,22,265,35},29,paper,juce::Justification::centredLeft,true);
    text(g,"Find your bounce.",{35,59,260,23},14,muted);
    const int state=owner.syncState.load(std::memory_order_relaxed);
    const float bpm=owner.hostBpm.load(std::memory_order_relaxed);
    g.setColour(state==2 ? mint : muted);g.fillEllipse(399,35,6,6);
    const juce::String status=state==2 ? juce::String(bpm,1)+" BPM / Synced" : (state==1 ? "Host stopped" : "No host sync");
    text(g,status,{413,24,186,28},13,muted);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImageWithin(mascot,385,79,278,218,juce::RectanglePlacement::centred);
    text(g,"Dry",{66,267,52,20},12,muted);
    text(g,"Deep",{247,267,52,20},12,muted,juce::Justification::centredRight);
    text(g,"Rate",{512,306,44,34},13,muted);
    const juce::Rectangle<float> graph{34,356,652,62};
    g.setColour(panel);g.fillRoundedRectangle(graph,10);
    const auto inner=graph.reduced(12,9);
    g.setColour(muted.withAlpha(0.16f));
    for(int i=1;i<4;++i) {const auto x=inner.getX()+inner.getWidth()*static_cast<float>(i)/4;g.drawVerticalLine(juce::roundToInt(x),inner.getY(),inner.getBottom());}
    const double depth=owner.parameters.getRawParameterValue("amount")->load()*0.01;
    const int slope=static_cast<int>(owner.parameters.getRawParameterValue("shape")->load());
    const int speed=juce::jlimit(0,5,static_cast<int>(owner.parameters.getRawParameterValue("rate")->load()));
    const double edge=std::min(0.08,0.003*(bpm>0 ? bpm : 120)/(60*duck::cycleBeats[static_cast<size_t>(speed)]));
    const bool bypassed=owner.parameters.getRawParameterValue("bypass")->load()>0.5f;
    juce::Path curve;
    for(int i=0;i<=652;++i) {
        const auto phase=static_cast<double>(i)/652;
        const auto gain=bypassed ? 1.0 : 1-depth*(1-duck::envelope(phase,slope,edge));
        const float x=inner.getX()+static_cast<float>(phase)*inner.getWidth();
        const float y=inner.getBottom()-static_cast<float>(gain)*inner.getHeight();
        if(i==0) curve.startNewSubPath(x,y);else curve.lineTo(x,y);
    }
    g.setColour(bypassed ? muted : mint);g.strokePath(curve,juce::PathStrokeType(2,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    if(state==2 && !bypassed) {
        const float x=inner.getX()+owner.visualPhase.load(std::memory_order_relaxed)*inner.getWidth();
        g.setColour(yellow.withAlpha(0.85f));g.drawVerticalLine(juce::roundToInt(x),inner.getY()-3,inner.getBottom()+3);
    }
    text(g,"Ducking envelope",{34,426,220,20},12,muted);
    text(g,"DJ Duck / openDucker 1.0",{450,426,236,20},12,muted,juce::Justification::centredRight);
}
