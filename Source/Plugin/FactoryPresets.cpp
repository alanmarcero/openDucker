#include "FactoryPresets.h"

namespace duck {
std::vector<suite::FactoryPreset> makeFactoryPresets(juce::AudioProcessorValueTreeState& parameters) {
    std::vector<suite::FactoryPreset> result;
    const auto add = [&](const char* name, std::initializer_list<std::pair<const char*, float>> values) {
        auto tree = parameters.copyState();
        for(const auto& [id, value] : values) {
            auto child = tree.getChildWithProperty("id", id);
            if(child.isValid()) child.setProperty("value", value, nullptr);
        }
        result.push_back({name, std::move(tree)});
    };
    add("Classic Pump", {});
    add("Low-Only Pump", {{"bandOn", 1.0f}, {"bandFrequency", 120.0f}, {"curve", 1.0f}, {"length", 90.0f}});
    add("Trance Gate", {{"curve", 5.0f}, {"rate", 0.0f}, {"length", 40.0f}, {"smoothing", 0.0f}});
    add("Ducked Reverb", {{"fxEnabled", 1.0f}, {"delayEnabled", 0.0f}, {"reverbEnabled", 1.0f}, {"reverbType", 1.0f},
        {"reverbLength", 8.0f}, {"duckTarget", 1.0f}, {"duckPosition", 1.0f}, {"fxMix", 42.0f}});
    add("Ducked Delay", {{"fxEnabled", 1.0f}, {"reverbEnabled", 0.0f}, {"delayEnabled", 1.0f}, {"delayLength", 3.0f},
        {"delayFeedback", 42.0f}, {"delayMode", 1.0f}, {"duckTarget", 1.0f}, {"duckPosition", 1.0f}, {"fxMix", 38.0f}});
    add("Soft Sidechain", {{"trigger", 2.0f}, {"threshold", -18.0f}, {"smoothing", 3.0f}, {"curve", 9.0f},
        {"length", 80.0f}, {"floor", 18.0f}});
    add("Hard 1/4", {{"curve", 11.0f}, {"rate", 1.0f}, {"length", 20.0f}, {"smoothing", 0.0f}, {"tension", -40.0f}});
    add("1/8 Pump", {{"curve", 0.0f}, {"rate", 0.0f}, {"length", 65.0f}});
    add("Destroy Pump", {{"fxEnabled", 1.0f}, {"delayEnabled", 0.0f}, {"reverbEnabled", 0.0f}, {"destroy", 80.0f},
        {"destroyMode", 1.0f}, {"fxMix", 100.0f}, {"duckTarget", 2.0f}});
    add("Offbeat Pump", {{"shift", 50.0f}, {"length", 80.0f}});
    add("Deep Floor", {{"floor", 30.0f}, {"curve", 4.0f}, {"bandOn", 1.0f}, {"bandFrequency", 100.0f}, {"bandSlope", 2.0f}});
    add("MIDI Gate", {{"trigger", 1.0f}, {"curve", 5.0f}, {"length", 30.0f}, {"retrigger", 1.0f}});
    add("Wide Hall Duck", {{"fxEnabled", 1.0f}, {"delayEnabled", 0.0f}, {"reverbEnabled", 1.0f}, {"reverbType", 2.0f},
        {"reverbWidth", 100.0f}, {"reverbLength", 12.0f}, {"duckTarget", 2.0f}, {"duckPosition", 0.0f}, {"fxMix", 50.0f}});
    add("Kick Trigger", {{"trigger", 2.0f}, {"threshold", -20.0f}, {"detectorHpf", 40.0f}, {"detectorLpf", 180.0f},
        {"curve", 12.0f}, {"length", 35.0f}});
    add("Serial Wash", {{"fxEnabled", 1.0f}, {"delayEnabled", 1.0f}, {"reverbEnabled", 1.0f}, {"fxFlow", 0.0f},
        {"delayLength", 2.0f}, {"delayFeedback", 28.0f}, {"reverbType", 0.0f}, {"reverbLength", 4.0f},
        {"duckTarget", 2.0f}, {"duckPosition", 1.0f}, {"fxMix", 45.0f}, {"curve", 10.0f}});
    add("Pump Room", {{"fxEnabled", 1.0f}, {"delayEnabled", 1.0f}, {"reverbEnabled", 1.0f}, {"fxFlow", 1.0f},
        {"delayLength", 4.0f}, {"delayFeedback", 25.0f}, {"reverbType", 0.0f}, {"reverbLength", 5.0f},
        {"duckTarget", 0.0f}, {"fxMix", 30.0f}, {"curve", 3.0f}, {"output", 1.0f}});
    return result;
}
}
