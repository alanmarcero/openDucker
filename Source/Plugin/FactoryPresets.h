#pragma once
#include "PresetLibrary.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace duck {
std::vector<suite::FactoryPreset> makeFactoryPresets(juce::AudioProcessorValueTreeState&);
}
