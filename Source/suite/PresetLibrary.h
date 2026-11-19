#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>
#include <vector>

namespace suite {
struct FactoryPreset {
    juce::String name;
    juce::ValueTree state;
};

struct PresetEntry {
    juce::String name;
    juce::String key;
    bool factory = false;
    bool favorite = false;
    int program = -1;
    juce::ValueTree state;
};

class PresetLibrary {
public:
    PresetLibrary(juce::AudioProcessorValueTreeState&, juce::String pluginName);
    void setFactory(std::vector<FactoryPreset>);
    int numFactory() const;
    juce::String factoryName(int index) const;
    juce::String factoryKey(int index) const;

    juce::File directory() const;
    void setDirectory(juce::File);
    void rescan();

    bool saveUser(const juce::String& name, juce::String& error);
    bool renameUser(const juce::String& key, const juce::String& name, juce::String& error);
    bool deleteUser(const juce::String& key, juce::String& error);
    void setFavorite(const juce::String& key, bool favorite);
    bool isFavorite(const juce::String& key) const;

    std::vector<PresetEntry> filtered(const juce::String& search) const;
    std::optional<PresetEntry> find(const juce::String& key) const;
    const juce::String& activeKey() const noexcept { return active; }
    void setActiveKey(juce::String key);
    juce::String activeName() const;

    static juce::String sanitize(juce::String name);
private:
    void loadFavorites();
    void storeFavorites();
    juce::File favoritesFile() const;
    bool factoryNameTaken(const juce::String&) const;
    juce::AudioProcessorValueTreeState& state;
    juce::String pluginName;
    juce::File directoryOverride;
    std::vector<FactoryPreset> factory;
    std::vector<PresetEntry> users;
    juce::StringArray favorites;
    juce::String active;
};
}
