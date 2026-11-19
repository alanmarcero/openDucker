#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace suite {
/// Two full parameter slots. The audible slot tracks the live parameters.
class ABCompare {
public:
    explicit ABCompare(juce::AudioProcessorValueTreeState&);
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
    void load(const juce::ValueTree&);
    juce::AudioProcessorValueTreeState& state;
    juce::ValueTree slotA, slotB;
    bool activeIsA = true;
};
}
