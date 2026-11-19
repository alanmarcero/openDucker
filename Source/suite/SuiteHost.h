#pragma once
#include "PresetLibrary.h"

namespace suite {
/// Operations the shared editor bar asks the plugin to perform.
class SuiteHost {
public:
    virtual ~SuiteHost() = default;
    virtual void undoEdit() = 0;
    virtual void redoEdit() = 0;
    virtual bool canUndo() const = 0;
    virtual bool canRedo() const = 0;
    virtual void selectSlotA() = 0;
    virtual void selectSlotB() = 0;
    virtual bool slotIsA() const = 0;
    virtual void copyAToB() = 0;
    virtual void copyBToA() = 0;
    virtual PresetLibrary& presets() = 0;
    virtual bool loadPreset(const juce::String& key) = 0;
    virtual bool savePreset(const juce::String& name, juce::String& error) = 0;
    virtual bool renamePreset(const juce::String& key, const juce::String& name, juce::String& error) = 0;
    virtual bool deletePreset(const juce::String& key, juce::String& error) = 0;
    virtual void setPresetFavorite(const juce::String& key, bool favorite) = 0;
};
}
