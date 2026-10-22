#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace {
const juce::Colour background{0xff10171b},panel{0xff202a2d},mint{0xff8fd3ce},yellow{0xffd9b878},paper{0xfff2f1e8},muted{0xff9eadb5};
void text(juce::Graphics& g,const juce::String& value,juce::Rectangle<int> bounds,float size,juce::Colour colour,int alignment=juce::Justification::centredLeft,bool bold=false) {
    g.setColour(colour);g.setFont(juce::FontOptions(size,bold ? juce::Font::bold : juce::Font::plain));
    g.drawText(value,bounds,alignment);
}
void glow(juce::Graphics& g,juce::Rectangle<float> bounds,juce::Colour colour) {
    const juce::Graphics::ScopedSaveState saved(g);
    const auto centre=bounds.getCentre();const float radius=bounds.getWidth()*0.5f;
    g.addTransform(juce::AffineTransform::scale(1,bounds.getHeight()/bounds.getWidth(),centre.x,centre.y));
    g.setGradientFill(juce::ColourGradient(colour,centre,colour.withAlpha(0.0f),centre.translated(radius,0),true));
    g.fillEllipse(centre.x-radius,centre.y-radius,radius*2,radius*2);
}
void speaker(juce::Graphics& g,float pump) {
    const juce::Rectangle<float> cabinet{358,91,252,199};
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff36434b},cabinet.getTopLeft(),
        juce::Colour{0xff10171b},cabinet.getBottomRight(),false));
    g.fillRoundedRectangle(cabinet,14);
    g.setColour(muted.withAlpha(0.25f));g.drawRoundedRectangle(cabinet.reduced(1),14,1);
    g.setColour(background);g.fillRoundedRectangle(cabinet.reduced(7),10);
    const juce::Point<float> centre{459,189};
    const juce::Rectangle<float> rim{366,96,186,186};
    glow(g,rim.expanded(10),mint.withAlpha(0.1f+pump*0.32f));
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff46545a},rim.getTopLeft(),
        juce::Colour{0xff080e12},rim.getBottomRight(),false));
    g.fillEllipse(rim);
    g.setColour(mint.withAlpha(0.2f+pump*0.55f));g.drawEllipse(rim.reduced(4),2);
    g.setColour(juce::Colour{0xff080e12});g.fillEllipse(rim.reduced(9));
    const float radius=73+pump*9;
    const juce::Rectangle<float> cone{centre.x-radius,centre.y-radius,radius*2,radius*2};
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff10191f},centre,
        juce::Colour{0xff50616a},centre.translated(radius,0),true));
    g.fillEllipse(cone);
    for(int ring=0;ring<4;++ring) {
        g.setColour(muted.withAlpha(0.18f));
        g.drawEllipse(cone.reduced(static_cast<float>(ring)*4+2),1);
    }
    const float capRadius=28+pump*5;
    const juce::Rectangle<float> cap{centre.x-capRadius,centre.y-capRadius,capRadius*2,capRadius*2};
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff60747c},cap.getTopLeft(),
        juce::Colour{0xff101a20},cap.getBottomRight(),false));
    g.fillEllipse(cap);
    g.setColour(mint.withAlpha(0.15f+pump*0.3f));g.drawEllipse(cap,1);
    for(const auto point:{cabinet.getTopLeft(),cabinet.getTopRight(),cabinet.getBottomLeft(),cabinet.getBottomRight()}) {
        const auto screw=point+(cabinet.getCentre()-point)*0.09f;
        g.setColour(muted.withAlpha(0.45f));g.fillEllipse(screw.x-2,screw.y-2,4,4);
        g.setColour(background);g.drawLine(screw.x-1,screw.y,screw.x+1,screw.y,1);
    }
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
    if(width<150) {
        const juce::Point<float> centre{static_cast<float>(x+width/2),static_cast<float>(y+28)};
        constexpr float radius=22;
        const float angle=start+value*(end-start);
        juce::Path track,arc;
        track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
        const float origin=slider.getMinimum()<0 ? start+(end-start)*static_cast<float>(slider.valueToProportionOfLength(0)) : start;
        arc.addCentredArc(centre.x,centre.y,radius,radius,0,origin,angle,true);
        g.setColour(panel.brighter(0.2f));g.strokePath(track,juce::PathStrokeType(3));
        g.setColour(mint);g.strokePath(arc,juce::PathStrokeType(3));
        const juce::Rectangle<float> body{centre.x-17,centre.y-17,34,34};
        g.setGradientFill(juce::ColourGradient(panel.brighter(0.3f),body.getTopLeft(),background,body.getBottomRight(),false));g.fillEllipse(body);
        const auto direction=juce::Point<float>(std::sin(angle),-std::cos(angle));
        g.setColour(yellow);g.drawLine({centre+direction*10,centre+direction*16},2);
        text(g,slider.getTextFromValue(slider.getValue()),{x,y+55,width,20},13,paper,juce::Justification::centred,true);
        text(g,slider.getName(),{x,y+77,width,18},12,muted,juce::Justification::centred);
        if(slider.hasKeyboardFocus(true)) {g.setColour(yellow);g.drawEllipse(body.expanded(9),1);}
        return;
    }
    const auto bounds=juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),static_cast<float>(width),static_cast<float>(height)).reduced(15);
    const auto centre=bounds.getCentre();const float radius=std::min(bounds.getWidth(),bounds.getHeight())*0.5f;
    juce::Path track,arc;
    track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
    arc.addCentredArc(centre.x,centre.y,radius,radius,0,start,start+value*(end-start),true);
    g.setColour(panel.brighter(0.12f));g.strokePath(track,juce::PathStrokeType(6,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.setColour(mint);g.strokePath(arc,juce::PathStrokeType(6,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    for(int tick=0;tick<=32;++tick) {
        const float angle=start+static_cast<float>(tick)/32*(end-start);
        const juce::Point<float> direction{std::sin(angle),-std::cos(angle)};
        g.setColour(muted.withAlpha(tick%4==0 ? 0.65f : 0.28f));
        g.drawLine({centre+direction*(radius+9),centre+direction*(radius+(tick%4==0 ? 14 : 12))},1);
    }
    const auto body=bounds.reduced(13);
    g.setGradientFill(juce::ColourGradient(panel.brighter(0.12f),body.getTopLeft(),background,body.getBottomRight(),false));g.fillEllipse(body);
    g.setColour(muted.withAlpha(0.65f));g.drawEllipse(body,1);
    g.setColour(yellow.withAlpha(0.22f));g.drawEllipse(body.reduced(4),1);
    const float angle=start+value*(end-start);
    const auto a=centre+juce::Point<float>(std::sin(angle),-std::cos(angle))*(radius-22);
    const auto b=centre+juce::Point<float>(std::sin(angle),-std::cos(angle))*(radius-34);
    g.setColour(yellow);g.drawLine({a,b},4);
    text(g,juce::String(juce::roundToInt(slider.getValue()))+"%",{x,y+height/2-24,width,44},36,paper,juce::Justification::centred,true);
    text(g,"Amount",{x,y+height/2+21,width,22},14,muted,juce::Justification::centred);
    if(slider.hasKeyboardFocus(true)) {g.setColour(yellow);g.drawEllipse(bounds.expanded(6),1);}
}
DuckerEditor::DuckerEditor(DuckerProcessor& p):AudioProcessorEditor(p),owner(p),effectsPanel(p) {
    setLookAndFeel(&theme);
    mascot=juce::ImageCache::getFromMemory(BinaryData::djduck_png,BinaryData::djduck_pngSize);
    faceplate=juce::ImageCache::getFromMemory(BinaryData::analogcyberfaceplate_png,BinaryData::analogcyberfaceplate_pngSize);
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
    bypass.setComponentID("bypass");bypass.setName("Bypass");bypass.setClickingTogglesState(true);bypass.setTooltip("Bypass ducking and the entire effects chain.");addAndMakeVisible(bypass);
    bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"bypass",bypass);
    bypass.onStateChange=[this] {bypass.setButtonText(bypass.getToggleState() ? "Bypassed" : "On");};
    addAndMakeVisible(effectsPanel);addAndMakeVisible(effectsToggle);
    effectsToggle.setComponentID("effectsToggle");effectsToggle.setButtonText("Hide FX");
    effectsToggle.setTooltip("Show or hide the effects controls. Audio processing stays active.");
    effectsToggle.onClick=[this] {
        effectsExpanded=!effectsExpanded;effectsPanel.setVisible(effectsExpanded);
        effectsToggle.setButtonText(effectsExpanded ? "Hide FX" : "Show FX");
        setSize(720,effectsExpanded ? 800 : 460);
    };
    setSize(720,800);timerCallback();startTimerHz(30);
}
DuckerEditor::~DuckerEditor() {stopTimer();setLookAndFeel(nullptr);}
void DuckerEditor::timerCallback() {
    bypass.setButtonText(bypass.getToggleState() ? "Bypassed" : "On");
    effectsPanel.refresh();
    repaint();
}
void DuckerEditor::resized() {
    amount.setBounds(65,76,230,215);
    for(size_t i=0;i<slopes.size();++i) slopes[i].setBounds(34+static_cast<int>(i)*84,306,76,34);
    rate.setBounds(560,306,126,34);bypass.setBounds(606,24,80,30);
    effectsToggle.setBounds(606,426,80,22);effectsPanel.setBounds(22,460,676,326);
}
void DuckerEditor::paintStage(juce::Graphics& g,float amountPercent) {
    const juce::Graphics::ScopedSaveState saved(g);
    g.reduceClipRegion(350,79,336,218);
    const float level=juce::jlimit(0.0f,1.0f,amountPercent*0.01f);
    const float floor=290;
    const double time=juce::Time::getMillisecondCounterHiRes()*0.001;
    const bool synced=owner.syncState.load(std::memory_order_relaxed)==2;
    const float phase=juce::jlimit(0.0f,1.0f,owner.visualPhase.load(std::memory_order_relaxed));
    const float pump=synced ? std::pow(1-phase,3.0f) : 0.25f+0.2f*static_cast<float>(std::sin(time*juce::MathConstants<double>::twoPi));
    speaker(g,pump);
    if(level>0) {
        glow(g,{360,floor-10,326,18},mint.withAlpha(level*0.16f));
        for(int side=0;side<2;++side) {
            const float source=side==0 ? 386.0f : 659.0f;
            const auto light=side==0 ? mint : juce::Colour{0xffb6a2ff};
            const float pulse=0.65f+0.35f*static_cast<float>(std::pow(std::sin(time*5.5+side*2.0),8));
            const float top=floor-204*level;
            for(int beam=0;beam<3;++beam) {
                const float tip=source+(side==0 ? 1.0f : -1.0f)*(static_cast<float>(beam)*58-12);
                juce::Path cone;
                cone.startNewSubPath(source-3,floor);
                cone.lineTo(tip-19,top);cone.lineTo(tip+19,top);
                cone.lineTo(source+3,floor);cone.closeSubPath();
                g.setGradientFill(juce::ColourGradient(light.withAlpha(level*pulse*0.38f),source,floor,
                    light.withAlpha(0.0f),tip,top,false));
                g.fillPath(cone);
                g.setGradientFill(juce::ColourGradient(light.withAlpha(level*pulse*0.5f),source,floor,
                    light.withAlpha(0.0f),tip,top,false));
                g.drawLine(source,floor,tip,top,1);
            }
            g.setColour(light.withAlpha(level*pulse));g.fillEllipse(source-7,floor-3,14,4);
            for(int puff=0;puff<16;++puff) {
                const float index=static_cast<float>(puff);
                const float radius=18+std::fmod(index*11,23.0f);
                const float x=juce::jlimit(392.0f,644.0f,source+std::sin(index*2.3f)*24+(side==0 ? 12.0f : -12.0f));
                const float y=floor+(1-level)*174-static_cast<float>(puff%8)*22-8;
                const float alpha=level*(0.12f+0.06f*std::sin(index));
                juce::ColourGradient mist(light.interpolatedWith(paper,0.5f).withAlpha(alpha),x,y,
                    light.withAlpha(0.0f),x+radius,y,true);
                mist.addColour(0.45,light.withAlpha(alpha*0.6f));
                g.setGradientFill(mist);g.fillEllipse(x-radius,y-radius,radius*2,radius*2);
            }
        }
    }
    {
        const juce::Graphics::ScopedSaveState duckState(g);
        g.addTransform(juce::AffineTransform::translation(16,-4*pump));
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImageWithin(mascot,385,79,278,218,juce::RectanglePlacement::centred);
    }
    if(level>0) {
        for(int puff=0;puff<9;++puff) {
            const float x=388+static_cast<float>(puff)*33;
            const float y=floor+12-level*19;
            glow(g,{x-34,y-12,68,24},paper.withAlpha(level*0.19f));
        }
    }
    if(amountPercent<70) return;
    for(int side=0;side<2;++side) {
        const float source=side==0 ? 386.0f : 659.0f;
        for(int particle=0;particle<48;++particle) {
            const float seed=std::fmod(static_cast<float>(particle)*0.618034f+static_cast<float>(side)*0.173f,1.0f);
            const float age=static_cast<float>(std::fmod(time*(0.75+seed*0.65)+particle*0.137+side*0.3,1.0));
            const float height=(86+seed*75)*level;
            const float spread=(seed-0.5f)*94;
            const float previous=std::max(0.0f,age-0.028f);
            const juce::Point<float> head{source+spread*age,floor-4*height*age*(1-age)};
            const juce::Point<float> tail{source+spread*previous,floor-4*height*previous*(1-previous)};
            const auto colour=yellow.interpolatedWith(juce::Colour{0xfffff1b8},seed).withAlpha(1-age);
            g.setColour(colour.withMultipliedAlpha(0.16f));g.fillEllipse(head.x-3,head.y-3,6,6);
            g.setColour(colour);g.drawLine({tail,head},1.4f);g.fillEllipse(head.x-0.9f,head.y-0.9f,1.8f,1.8f);
        }
        glow(g,{source-15,floor-10,30,18},yellow.withAlpha(0.65f));
    }
}
void DuckerEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    g.setOpacity(0.7f);g.drawImage(faceplate,getLocalBounds().toFloat(),juce::RectanglePlacement::stretchToFit);g.setOpacity(1);
    g.setColour(muted.withAlpha(0.22f));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(7),7,1);
    g.setColour(yellow.withAlpha(0.4f));g.drawHorizontalLine(88,34,324);
    text(g,"openDucker",{34,22,265,35},29,paper,juce::Justification::centredLeft,true);
    text(g,"Find your bounce.",{35,59,260,23},14,muted);
    const int state=owner.syncState.load(std::memory_order_relaxed);
    const float bpm=owner.hostBpm.load(std::memory_order_relaxed);
    g.setColour(state==2 ? mint : muted);g.fillEllipse(399,35,6,6);
    const juce::String status=state==2 ? juce::String(bpm,1)+" BPM / Synced" : (state==1 ? "Host stopped" : "No host sync");
    text(g,status,{413,24,186,28},13,muted);
    const float amountPercent=owner.parameters.getRawParameterValue("amount")->load();
    paintStage(g,amountPercent);
    text(g,"Dry",{66,267,52,20},12,muted);
    text(g,"Deep",{247,267,52,20},12,muted,juce::Justification::centredRight);
    text(g,"Rate",{512,306,44,34},13,muted);
    const juce::Rectangle<float> graph{34,356,652,62};
    g.setGradientFill(juce::ColourGradient(background,graph.getTopLeft(),panel,graph.getBottomRight(),false));g.fillRoundedRectangle(graph,6);
    g.setColour(muted.withAlpha(0.25f));g.drawRoundedRectangle(graph,6,1);
    const auto inner=graph.reduced(12,9);
    g.setColour(muted.withAlpha(0.16f));
    for(int i=1;i<4;++i) {const auto x=inner.getX()+inner.getWidth()*static_cast<float>(i)/4;g.drawVerticalLine(juce::roundToInt(x),inner.getY(),inner.getBottom());}
    const double depth=amountPercent*0.01;
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
    text(g,"DJ Duck / openDucker   /   v1.1.0",{350,426,240,20},12,muted,juce::Justification::centredRight);
}
