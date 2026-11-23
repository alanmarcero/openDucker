#include "EffectsPanel.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
struct KnobSpec { const char* id; const char* name; const char* tip; };
constexpr std::array<KnobSpec, 12> knobSpecs {{
    {"delayLength", "Length", "Host-synced delay length. Very slow tempos are capped at 8 seconds. Turn on FX and Delay to use it."},
    {"delayFeedback", "Feedback", "How much delay returns for another repeat. Turn on FX and Delay to use it."},
    {"delayBass", "Bass", "Boost or cut delay bass by up to 12 dB. Turn on FX and Delay to use it."},
    {"delayTreble", "Treble", "Boost or cut delay treble by up to 12 dB. Turn on FX and Delay to use it."},
    {"reverbLength", "Length", "Reverb decay time, from 0.2 to 25 seconds (approximately a 60 dB decay). Turn on FX and Reverb to use it."},
    {"reverbWidth", "Width", "Reverb stereo width. 0% is mono; 100% is full stereo. Turn on FX and Reverb to use it."},
    {"reverbBass", "Bass", "Boost or cut reverb bass by up to 12 dB. Turn on FX and Reverb to use it."},
    {"reverbTreble", "Treble", "Boost or cut reverb treble by up to 12 dB. Turn on FX and Reverb to use it."},
    {"sustain", "Sustain", "Compress peaks and lift quieter effect tails. Higher settings sustain the sound longer. Turn on FX to use it."},
    {"destroy", "Destroy", "Glitch, crush and distort. Always acts during playback; Tail acts during the final five seconds of the estimated FX tail. Turn on FX to use it."},
    {"fxMix", "FX Mix", "Blend original dry audio with the processed effects. 100% is effects only. Turn on FX to use it."},
    {"output", "Output", "Final output level. The peak meter turns yellow above 0 dBFS."}
}};
}

double EffectsPanel::Knob::snapValue(double value, DragMode mode) {
    return zeroDetent && mode != notDragging && std::abs(value) <= 0.25 ? 0 : value;
}

EffectsPanel::EffectsPanel(DuckerProcessor& p) : owner(p) {
    for(size_t i = 0; i < knobs.size(); ++i) {
        auto& knob = knobs[i]; const auto& spec = knobSpecs[i];
        knob.zeroDetent = i == 2 || i == 3 || i == 6 || i == 7;
        knob.setComponentID(spec.id); knob.setName(spec.name); knob.setTitle(spec.name); knob.setTooltip(spec.tip);
        bindParamLock(knob, spec.id);
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        knob.setWantsKeyboardFocus(true); addAndMakeVisible(knob);
        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, spec.id, knob);
        auto* parameter = p.parameters.getParameter(spec.id);
        knob.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
        if(i == 0) {
            const auto labels = parameter->getAllValueStrings();
            knob.textFromValueFunction = [labels](double value) {
                return labels[juce::jlimit(0, labels.size() - 1, juce::roundToInt(value))];
            };
        }
    }
    const std::array<const char*, 6> choiceIds {"duckTarget", "duckPosition", "delayMode", "reverbType", "fxFlow", "destroyMode"};
    const std::array<juce::StringArray, 6> choiceItems {{
        {"Dry", "Wet", "Both"}, {"Before FX", "After FX"}, {"Mono", "Ping-pong"},
        {"Room", "Hall", "Plate"}, {"Serial", "Parallel"}, {"Tail", "Always"}
    }};
    const std::array<const char*, 6> choiceTips {
        "Select which signal ducks: original dry input, effects, or both. Turn on FX to use it.",
        "Before FX ducks the effects input, allowing tails to ring. After FX ducks the complete effects output. Dry-only does not use this switch. Turn on FX to use it.",
        "Mono repeats stay centered. Ping-pong repeats alternate left and right. Turn on FX and Delay to use it.",
        "Choose the reverb character. Length, Width, Bass and Treble apply to every type. Turn on FX and Reverb to use it.",
        "Parallel keeps delay echoes and reverb separate and audible. Serial sends delay repeats into the reverb. Turn on FX to use it.",
        "Always processes continuous audio, even with Delay and Reverb off. Tail waits for the source to stop and needs a delay or reverb tail. Turn on FX and raise Destroy to use it."
    };
    for(size_t i = 0; i < choices.size(); ++i) {
        auto& choice = choices[i];
        const auto choiceName = p.parameters.getParameter(choiceIds[i])->getName(64);
        choice.setComponentID(choiceIds[i]); choice.setName(choiceName); choice.setTitle(choiceName); choice.setTooltip(choiceTips[i]);
        bindParamLock(choice, choiceIds[i]);
        choice.addItemList(choiceItems[i], 1); addAndMakeVisible(choice);
        choiceAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, choiceIds[i], choice);
    }
    const std::array<const char*, 3> switchIds {"fxEnabled", "delayEnabled", "reverbEnabled"};
    for(size_t i = 0; i < switches.size(); ++i) {
        auto& button = switches[i];
        button.setComponentID(switchIds[i]); button.setClickingTogglesState(true); addAndMakeVisible(button);
        bindParamLock(button, switchIds[i]);
        switchAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, switchIds[i], button);
        button.onStateChange = [this] { refresh(); };
    }
    switches[0].setTooltip("Enable the effects chain. Off keeps the original ducking effect.");
    switches[1].setTooltip("Enable or bypass the delay stage. Turn on FX to use it.");
    switches[2].setTooltip("Enable or bypass the reverb stage. Turn on FX to use it.");
    choices[0].onChange = [this] { refresh(); };
    knobs[8].onValueChange = [this] { refresh(); };
    knobs[9].onValueChange = [this] { refresh(); };
    refresh();
}

void EffectsPanel::lookAndFeelChanged() {
    for(auto& knob : knobs)
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
}

void EffectsPanel::refresh() {
    switches[0].setButtonText(switches[0].getToggleState() ? "FX On" : "FX Off");
    switches[1].setButtonText(switches[1].getToggleState() ? "Delay On" : "Delay Off");
    switches[2].setButtonText(switches[2].getToggleState() ? "Reverb On" : "Reverb Off");
    for(auto& button : switches) button.setTitle(button.getButtonText());
    // Controls that have no effect draw dimmed and stay editable. Output always applies.
    const bool fxOn = switches[0].getToggleState();
    const bool delayOn = switches[1].getToggleState();
    const bool reverbOn = switches[2].getToggleState();
    const std::array<bool, 6> choiceActive {fxOn, fxOn && choices[0].getSelectedId() != 1, fxOn && delayOn,
        fxOn && reverbOn, fxOn && delayOn && reverbOn, fxOn && knobs[9].getValue() > 0};
    for(size_t i = 0; i < choices.size(); ++i) choices[i].setAlpha(choiceActive[i] ? 1.0f : look::inactiveAlpha);
    for(size_t i = 0; i < knobs.size(); ++i) {
        const bool active = i == 11 || (fxOn && (i >= 8 || switches[i < 4 ? 1 : 2].getToggleState()));
        knobs[i].setAlpha(active ? 1.0f : look::inactiveAlpha);
    }
    switches[1].setAlpha(fxOn ? 1.0f : look::inactiveAlpha);
    switches[2].setAlpha(fxOn ? 1.0f : look::inactiveAlpha);
    repaint();
}

void EffectsPanel::resized() {
    const float s = static_cast<float>(getWidth()) / 962.0f;
    const auto area = [s](float x, float y, float w, float h) {
        return juce::Rectangle<float>(x * s, y * s, w * s, h * s).toNearestInt();
    };
    switches[0].setBounds(area(0, 2, 86, 40));
    choices[0].setBounds(area(138, 2, 92, 40));
    choices[1].setBounds(area(238, 2, 122, 40));
    choices[4].setBounds(area(412, 2, 110, 40));

    switches[1].setBounds(area(12, 48, 96, 40));
    choices[2].setBounds(area(190, 48, 110, 40));
    switches[2].setBounds(area(337, 48, 102, 40));
    choices[3].setBounds(area(515, 48, 110, 40));
    choices[5].setBounds(area(840, 48, 110, 40));

    for(int i = 0; i < 4; ++i) {
        knobs[static_cast<size_t>(i)].setBounds(area(4 + static_cast<float>(i) * 76, 88, 76, 104));
        knobs[static_cast<size_t>(i + 4)].setBounds(area(329 + static_cast<float>(i) * 76, 88, 76, 104));
        knobs[static_cast<size_t>(i + 8)].setBounds(area(654 + static_cast<float>(i) * 76, 88, 76, 104));
    }
}

void EffectsPanel::paint(juce::Graphics& g) {
    const float s = static_cast<float>(getWidth()) / 962.0f;
    for(const float cardX : {0.0f, 325.0f, 650.0f}) {
        const juce::Rectangle<float> card {cardX * s, 42 * s, 312 * s, 152 * s};
        g.setColour(look::panel.withAlpha(0.6f));
        g.fillRoundedRectangle(card, 8 * s);
        g.setColour(look::muted.withAlpha(0.2f));
        g.drawRoundedRectangle(card, 8 * s, 1.0f);
    }
    g.setColour(look::muted);
    g.setFont(juce::FontOptions(11.5f * s, juce::Font::bold));
    g.drawText("DUCK", juce::Rectangle<float>(92 * s, 4 * s, 42 * s, 28 * s), juce::Justification::centred);
    g.drawText("FLOW", juce::Rectangle<float>(366 * s, 4 * s, 42 * s, 28 * s), juce::Justification::centred);

    const bool enabled = switches[0].getToggleState()
        && (switches[1].getToggleState() || switches[2].getToggleState() || knobs[8].getValue() > 0 || knobs[9].getValue() > 0);
    const int target = choices[0].getSelectedId();
    const bool bypassed = owner.parameters.getRawParameterValue("bypass")->load() > 0.5f;
    const juce::String dry = bypassed ? "clean" : !enabled || target != 2 ? "ducked" : "clean";
    const juce::String wet = bypassed ? "bypassed" : !enabled ? "off" : target == 1 ? "clean" : choices[1].getSelectedId() == 1 ? "ducked input" : "ducked output";
    g.setFont(juce::FontOptions(11.5f * s));
    g.drawText("Dry: " + dry + "   /   FX: " + wet, juce::Rectangle<float>(534 * s, 4 * s, 248 * s, 28 * s), juce::Justification::centredLeft);

    // Stage Status LEDs & Badges
    const bool delayOn = enabled && switches[1].getToggleState();
    g.setColour(delayOn ? look::mint : look::muted.withAlpha(0.35f));
    g.fillEllipse(114 * s, 59.5f * s, 7 * s, 7 * s);
    g.setFont(juce::FontOptions(10.5f * s, juce::Font::bold));
    g.setColour(delayOn ? look::mint.withAlpha(0.9f) : look::muted.withAlpha(0.5f));
    g.drawText("DELAY", juce::Rectangle<float>(125 * s, 50 * s, 62 * s, 26 * s), juce::Justification::centredLeft);

    const bool reverbOn = enabled && switches[2].getToggleState();
    g.setColour(reverbOn ? look::yellow : look::muted.withAlpha(0.35f));
    g.fillEllipse(444 * s, 59.5f * s, 7 * s, 7 * s);
    g.setColour(reverbOn ? look::yellow.withAlpha(0.9f) : look::muted.withAlpha(0.5f));
    g.drawText("REVERB", juce::Rectangle<float>(455 * s, 50 * s, 58 * s, 26 * s), juce::Justification::centredLeft);

    const float activity = owner.destroyLevel.load(std::memory_order_relaxed);
    g.setColour(activity > 0.01f ? look::yellow : look::muted.withAlpha(0.35f));
    g.fillEllipse(664 * s, 59.5f * s, 7 * s, 7 * s);
    const bool armed = enabled && knobs[9].getValue() > 0;
    const juce::String destroyText = activity > 0.01f
        ? (choices[5].getSelectedId() == 2 ? "Destroy active" : "Destroying tail")
        : (armed ? "Tail armed" : "Destroy off");
    g.setColour(look::muted);
    g.drawText(destroyText, juce::Rectangle<float>(678 * s, 50 * s, 154 * s, 26 * s), juce::Justification::centredLeft);

    const float peak = owner.outputPeak.load(std::memory_order_relaxed);
    const float db = peak > 0 ? 20 * std::log10(peak) : -60;
    const float meter = juce::jlimit(0.0f, 1.0f, (db + 60) / 60);
    g.setColour(look::panel);
    g.fillRoundedRectangle(794 * s, 7 * s, 168 * s, 6 * s, 3 * s);
    g.setColour(peak > 1 ? look::yellow : look::mint);
    g.fillRoundedRectangle(794 * s, 7 * s, 168 * s * meter, 6 * s, 3 * s);
    g.setColour(look::muted);
    g.setFont(juce::FontOptions(11.0f * s));
    g.drawText(peak > 0.001f ? juce::String(db, 1) + " dBFS" : "Silent", juce::Rectangle<float>(794 * s, 16 * s, 96 * s, 18 * s), juce::Justification::centredLeft);
}
