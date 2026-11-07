#include "EffectsPanel.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
struct KnobSpec { const char* id; const char* name; const char* tip; };
constexpr std::array<KnobSpec, 12> knobSpecs {{
    {"delayLength", "Length", "Host-synced delay length. Very slow tempos are capped at 8 seconds."},
    {"delayFeedback", "Feedback", "How much delay returns for another repeat."},
    {"delayBass", "Bass", "Boost or cut delay bass by up to 12 dB."},
    {"delayTreble", "Treble", "Boost or cut delay treble by up to 12 dB."},
    {"reverbLength", "Length", "Reverb decay time, from 0.2 to 25 seconds (approximately a 60 dB decay)."},
    {"reverbWidth", "Width", "Reverb stereo width. 0% is mono; 100% is full stereo."},
    {"reverbBass", "Bass", "Boost or cut reverb bass by up to 12 dB."},
    {"reverbTreble", "Treble", "Boost or cut reverb treble by up to 12 dB."},
    {"sustain", "Sustain", "Compress peaks and lift quieter effect tails. Higher settings sustain the sound longer."},
    {"destroy", "Destroy", "Glitch, crush and distort. Always acts during playback; Tail acts during the final five seconds of the estimated FX tail."},
    {"fxMix", "FX Mix", "Blend original dry audio with the processed effects. 100% is effects only."},
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
        knob.setComponentID(spec.id); knob.setName(spec.name); knob.setTooltip(spec.tip);
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        knob.setWantsKeyboardFocus(true); addAndMakeVisible(knob);
        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, spec.id, knob);
        auto* parameter = p.parameters.getParameter(spec.id);
        knob.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
        if(i == 0) {
            knob.textFromValueFunction = [](double value) {
                return juce::StringArray {"1/32", "1/16", "1/8", "1/8 dot", "1/4", "1/4 dot", "1/2", "1 bar"}[juce::jlimit(0, 7, juce::roundToInt(value))];
            };
            continue;
        }
        if(i == 4) {
            knob.textFromValueFunction = [](double value) { return juce::String(value, 1) + " s"; };
            continue;
        }
        if(i == 2 || i == 3 || i == 6 || i == 7 || i == 11) {
            knob.textFromValueFunction = [](double value) {
                const double clean = std::abs(value) < 0.05 ? 0.0 : value;
                return (clean > 0 ? "+" : "") + juce::String(clean, 1) + " dB";
            };
            continue;
        }
        knob.textFromValueFunction = [](double value) { return juce::String(juce::roundToInt(value)) + "%"; };
    }
    const std::array<const char*, 6> choiceIds {"duckTarget", "duckPosition", "delayMode", "reverbType", "fxFlow", "destroyMode"};
    const std::array<juce::StringArray, 6> choiceItems {{
        {"Dry", "Wet", "Both"}, {"Before FX", "After FX"}, {"Mono", "Ping-pong"},
        {"Room", "Hall", "Plate"}, {"Serial", "Parallel"}, {"Tail", "Always"}
    }};
    const std::array<const char*, 6> choiceTips {
        "Select which signal ducks: original dry input, effects, or both.",
        "Before FX ducks the effects input, allowing tails to ring. After FX ducks the complete effects output. Dry-only does not use this switch.",
        "Mono repeats stay centered. Ping-pong repeats alternate left and right.",
        "Choose the reverb character. Length, Width, Bass and Treble apply to every type.",
        "Parallel keeps delay echoes and reverb separate and audible. Serial sends delay repeats into the reverb.",
        "Always processes continuous audio, even with Delay and Reverb off. Tail waits for the source to stop and needs a delay or reverb tail."
    };
    for(size_t i = 0; i < choices.size(); ++i) {
        auto& choice = choices[i];
        choice.setComponentID(choiceIds[i]); choice.setName(p.parameters.getParameter(choiceIds[i])->getName(64)); choice.setTooltip(choiceTips[i]);
        choice.addItemList(choiceItems[i], 1); addAndMakeVisible(choice);
        choiceAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, choiceIds[i], choice);
    }
    const std::array<const char*, 3> switchIds {"fxEnabled", "delayEnabled", "reverbEnabled"};
    for(size_t i = 0; i < switches.size(); ++i) {
        auto& button = switches[i];
        button.setComponentID(switchIds[i]); button.setClickingTogglesState(true); addAndMakeVisible(button);
        switchAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, switchIds[i], button);
        button.onStateChange = [this] { refresh(); };
    }
    switches[0].setTooltip("Enable the effects chain. Off keeps the original ducking effect.");
    switches[1].setTooltip("Enable or bypass the delay stage.");
    switches[2].setTooltip("Enable or bypass the reverb stage.");
    choices[0].onChange = [this] { refresh(); };
    refresh();
}

void EffectsPanel::refresh() {
    switches[0].setButtonText(switches[0].getToggleState() ? "FX On" : "FX Off");
    switches[1].setButtonText(switches[1].getToggleState() ? "Delay On" : "Delay Off");
    switches[2].setButtonText(switches[2].getToggleState() ? "Reverb On" : "Reverb Off");
    choices[1].setEnabled(choices[0].getSelectedId() != 1);
    choices[1].setAlpha(choices[1].isEnabled() ? 1.0f : 0.4f);
    for(size_t i = 2; i < 4; ++i) {
        choices[i].setEnabled(switches[i - 1].getToggleState());
        choices[i].setAlpha(choices[i].isEnabled() ? 1.0f : 0.4f);
    }
    for(size_t i = 0; i < 8; ++i) {
        const bool enabled = switches[i < 4 ? 1 : 2].getToggleState();
        knobs[i].setEnabled(enabled);
        knobs[i].setAlpha(enabled ? 1.0f : 0.4f);
    }
    repaint();
}

void EffectsPanel::resized() {
    const float s = static_cast<float>(getWidth()) / 962.0f;
    const auto area = [s](float x, float y, float w, float h) {
        return juce::Rectangle<float>(x * s, y * s, w * s, h * s).toNearestInt();
    };
    switches[0].setBounds(area(0, 4, 86, 28));
    choices[0].setBounds(area(138, 4, 92, 28));
    choices[1].setBounds(area(238, 4, 122, 28));
    choices[4].setBounds(area(412, 4, 110, 28));

    switches[1].setBounds(area(12, 50, 96, 26));
    choices[2].setBounds(area(190, 50, 110, 26));
    switches[2].setBounds(area(337, 50, 102, 26));
    choices[3].setBounds(area(515, 50, 110, 26));
    choices[5].setBounds(area(840, 50, 110, 26));

    for(int i = 0; i < 4; ++i) {
        knobs[static_cast<size_t>(i)].setBounds(area(4 + static_cast<float>(i) * 76, 82, 76, 104));
        knobs[static_cast<size_t>(i + 4)].setBounds(area(329 + static_cast<float>(i) * 76, 82, 76, 104));
        knobs[static_cast<size_t>(i + 8)].setBounds(area(654 + static_cast<float>(i) * 76, 82, 76, 104));
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
    g.drawText("v" JucePlugin_VersionString, juce::Rectangle<float>(890 * s, 16 * s, 72 * s, 18 * s), juce::Justification::centredRight);
}
