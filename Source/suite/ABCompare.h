#pragma once
#include "PresetLibrary.h"

namespace suite {
/// Two full parameter slots. The audible slot tracks the live parameters.
/// Each slot also keeps its preset key and edited flag.
class ABCompare {
public:
    ABCompare(juce::AudioProcessorValueTreeState&, PresetLibrary&);
    bool isA() const noexcept { return activeIsA; }
    void captureActive();
    bool selectA();
    bool selectB();
    void copyAToB();
    void copyBToA();
    juce::ValueTree toTree() const;
    void readTree(const juce::ValueTree&);
    void resetBothFromCurrent();
private:
    struct Slot {
        juce::ValueTree state;
        juce::String presetKey;
        bool presetEdited = false;
    };
    void load(const Slot&);
    juce::AudioProcessorValueTreeState& state;
    PresetLibrary& presets;
    Slot slotA, slotB;
    bool activeIsA = true;
};
}
