#include "EffectsPanel.h"
#include <cmath>

namespace {
const juce::Colour panel{0xff202a30},mint{0xff8fe2ce},yellow{0xffffcf53},muted{0xff9eadb5};
struct KnobSpec {const char* id;const char* name;const char* tip;};
constexpr std::array<KnobSpec,12> knobSpecs{{
    {"delayLength","Length","Host-synced delay length. Very slow tempos are capped at 8 seconds."},
    {"delayFeedback","Feedback","How much delay returns for another repeat."},
    {"delayBass","Bass","Boost or cut delay bass by up to 12 dB."},
    {"delayTreble","Treble","Boost or cut delay treble by up to 12 dB."},
    {"reverbLength","Length","Reverb decay time, from 0.2 to 25 seconds (approximately a 60 dB decay)."},
    {"reverbWidth","Width","Reverb stereo width. 0% is mono; 100% is full stereo."},
    {"reverbBass","Bass","Boost or cut reverb bass by up to 12 dB."},
    {"reverbTreble","Treble","Boost or cut reverb treble by up to 12 dB."},
    {"sustain","Sustain","Compress peaks and lift quieter effect tails. Higher settings sustain the sound longer."},
    {"destroy","Destroy","Glitch, crush and distort the last five seconds of the estimated tail. New input resets the effect."},
    {"fxMix","FX Mix","Blend original dry audio with the processed effects. 100% is effects only."},
    {"output","Output","Final output level. The peak meter turns yellow above 0 dBFS."}
}};
}

EffectsPanel::EffectsPanel(DuckerProcessor& p):owner(p) {
    for(size_t i=0;i<knobs.size();++i) {
        auto& knob=knobs[i];const auto& spec=knobSpecs[i];
        knob.setComponentID(spec.id);knob.setName(spec.name);knob.setTooltip(spec.tip);
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);knob.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        knob.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
        knob.setWantsKeyboardFocus(true);addAndMakeVisible(knob);
        knobAttachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,spec.id,knob);
        auto* parameter=p.parameters.getParameter(spec.id);
        knob.setDoubleClickReturnValue(true,parameter->convertFrom0to1(parameter->getDefaultValue()));
        if(i==0) {
            knob.textFromValueFunction=[](double value) {return juce::StringArray{"1/32","1/16","1/8","1/8 dot","1/4","1/4 dot","1/2","1 bar"}[juce::jlimit(0,7,juce::roundToInt(value))];};
        } else if(i==4) {
            knob.textFromValueFunction=[](double value) {return juce::String(value,1)+" s";};
        } else if(i==2 || i==3 || i==6 || i==7 || i==11) {
            knob.textFromValueFunction=[](double value) {if(std::abs(value)<0.05) value=0;return (value>0 ? "+" : "")+juce::String(value,1)+" dB";};
        } else {
            knob.textFromValueFunction=[](double value) {return juce::String(juce::roundToInt(value))+"%";};
        }
    }
    const std::array<const char*,4> choiceIds{"duckTarget","duckPosition","delayMode","reverbType"};
    const std::array<juce::StringArray,4> choiceItems{{{"Dry","Wet","Both"},{"Before FX","After FX"},{"Mono","Ping-pong"},{"Room","Hall","Plate"}}};
    const std::array<const char*,4> choiceTips{"Select which signal ducks: original dry input, effects, or both.",
        "Before FX ducks the effects input, allowing tails to ring. After FX ducks the complete effects output. Dry-only does not use this switch.",
        "Mono repeats stay centered. Ping-pong repeats alternate left and right.","Choose the reverb character. Length, Width, Bass and Treble apply to every type."};
    for(size_t i=0;i<choices.size();++i) {
        auto& choice=choices[i];choice.setComponentID(choiceIds[i]);choice.setName(p.parameters.getParameter(choiceIds[i])->getName(64));choice.setTooltip(choiceTips[i]);
        choice.addItemList(choiceItems[i],1);addAndMakeVisible(choice);
        choiceAttachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,choiceIds[i],choice);
    }
    const std::array<const char*,3> switchIds{"fxEnabled","delayEnabled","reverbEnabled"};
    for(size_t i=0;i<switches.size();++i) {
        auto& button=switches[i];button.setComponentID(switchIds[i]);button.setClickingTogglesState(true);addAndMakeVisible(button);
        switchAttachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,switchIds[i],button);
        button.onStateChange=[this] {refresh();};
    }
    switches[0].setTooltip("Enable the effects chain. Off keeps the original ducking effect.");
    switches[1].setTooltip("Enable or bypass the delay stage.");switches[2].setTooltip("Enable or bypass the reverb stage.");
    choices[0].onChange=[this] {refresh();};
    refresh();
}
void EffectsPanel::refresh() {
    switches[0].setButtonText(switches[0].getToggleState() ? "FX On" : "FX Off");
    switches[1].setButtonText(switches[1].getToggleState() ? "Delay On" : "Delay Off");
    switches[2].setButtonText(switches[2].getToggleState() ? "Reverb On" : "Reverb Off");
    choices[1].setEnabled(choices[0].getSelectedId()!=1);
    choices[1].setAlpha(choices[1].isEnabled() ? 1.0f : 0.4f);
    for(size_t i=2;i<choices.size();++i) {
        choices[i].setEnabled(switches[i-1].getToggleState());choices[i].setAlpha(choices[i].isEnabled() ? 1.0f : 0.4f);
    }
    for(size_t i=0;i<8;++i) {
        const bool enabled=switches[i<4 ? 1 : 2].getToggleState();knobs[i].setEnabled(enabled);knobs[i].setAlpha(enabled ? 1.0f : 0.4f);
    }
    repaint();
}
void EffectsPanel::resized() {
    switches[0].setBounds(10,8,74,28);
    choices[0].setBounds(132,8,82,28);choices[1].setBounds(225,8,118,28);
    switches[1].setBounds(10,58,90,26);switches[2].setBounds(354,58,100,26);
    choices[2].setBounds(211,58,110,26);choices[3].setBounds(555,58,110,26);
    for(int i=0;i<4;++i) {
        knobs[static_cast<size_t>(i)].setBounds(3+i*82,94,82,100);
        knobs[static_cast<size_t>(i+4)].setBounds(347+i*82,94,82,100);
    }
    knobs[8].setBounds(20,219,90,100);knobs[9].setBounds(125,219,90,100);
    knobs[10].setBounds(300,219,90,100);knobs[11].setBounds(405,219,90,100);
}
void EffectsPanel::paint(juce::Graphics& g) {
    g.setColour(panel.withAlpha(0.55f));g.fillRoundedRectangle(0,48,332,150,10);g.fillRoundedRectangle(344,48,332,150,10);
    g.setColour(muted);g.setFont(juce::FontOptions(12));g.drawText("Duck",92,8,36,28,juce::Justification::centred);
    const bool before=choices[1].getSelectedId()==1 && choices[0].getSelectedId()!=1;
    juce::String chain;
    if(before) chain="Duck > ";
    if(switches[1].getToggleState()) chain+="Delay > ";
    if(switches[2].getToggleState()) chain+="Reverb > ";
    chain+="Sustain > Destroy";
    if(!before && choices[0].getSelectedId()!=1) chain+=" > Duck";
    g.drawText(chain,355,8,310,28,juce::Justification::centredRight);
    g.setColour(panel);g.drawHorizontalLine(207,0,676);g.drawVerticalLine(262,220,307);
    const float activity=owner.destroyLevel.load(std::memory_order_relaxed);
    g.setColour(activity>0.01f ? yellow : muted.withAlpha(0.35f));g.fillEllipse(209,230,5,5);
    const bool armed=switches[0].getToggleState() && (switches[1].getToggleState() || switches[2].getToggleState()) && knobs[9].getValue()>0;
    g.setColour(muted);g.drawText(activity>0.01f ? "Destroying tail" : (armed ? "Tail armed" : "Destroy off"),526,233,140,20,juce::Justification::centredLeft);
    const float peak=owner.outputPeak.load(std::memory_order_relaxed);
    const float db=peak>0 ? 20*std::log10(peak) : -60;
    const float meter=juce::jlimit(0.0f,1.0f,(db+60)/60);
    g.setColour(panel);g.fillRoundedRectangle(526,263,140,6,3);
    g.setColour(peak>1 ? yellow : mint);g.fillRoundedRectangle(526,263,140*meter,6,3);
    g.setColour(muted);g.drawText(peak>0.001f ? juce::String(db,1)+" dBFS" : "Silent",526,278,140,20,juce::Justification::centredLeft);
}
