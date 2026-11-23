#include "PresetLibrary.h"
#include <algorithm>

namespace suite {
PresetLibrary::PresetLibrary(juce::AudioProcessorValueTreeState& stateIn, juce::String pluginNameIn)
    : state(stateIn), pluginName(std::move(pluginNameIn)) {}

void PresetLibrary::setFactory(std::vector<FactoryPreset> presets) {
    factory = std::move(presets);
    if(active.isEmpty() && !factory.empty()) active = factoryKey(0);
    rescan();
}

int PresetLibrary::numFactory() const { return static_cast<int>(factory.size()); }

juce::String PresetLibrary::factoryName(int index) const {
    if(index < 0 || index >= numFactory()) return {};
    return factory[static_cast<size_t>(index)].name;
}

juce::String PresetLibrary::factoryKey(int index) const {
    const auto name = factoryName(index);
    return name.isEmpty() ? juce::String() : "factory:" + name;
}

juce::File PresetLibrary::directory() const {
    if(directoryOverride != juce::File()) return directoryOverride;
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Alan Marcero").getChildFile(pluginName).getChildFile("Presets");
}

void PresetLibrary::setDirectory(juce::File directoryIn) {
    directoryOverride = std::move(directoryIn);
    rescan();
}

juce::File PresetLibrary::favoritesFile() const { return directory().getChildFile("favorites.txt"); }

void PresetLibrary::loadFavorites() {
    favorites.clear();
    favorites.addLines(favoritesFile().loadFileAsString());
    favorites.removeEmptyStrings(true);
    favorites.removeDuplicates(true);
}

void PresetLibrary::storeFavorites() {
    directory().createDirectory();
    favoritesFile().replaceWithText(favorites.joinIntoString("\n") + (favorites.isEmpty() ? "" : "\n"));
}

void PresetLibrary::rescan() {
    loadFavorites();
    users.clear();
    const auto dir = directory();
    if(!dir.isDirectory()) return;
    juce::Array<juce::File> files;
    dir.findChildFiles(files, juce::File::findFiles, false, "*.xml");
    for(const auto& file : files) {
        const auto xml = juce::parseXML(file);
        if(xml == nullptr || !xml->hasTagName(state.state.getType())) continue;
        PresetEntry entry;
        entry.name = file.getFileNameWithoutExtension();
        entry.key = "user:" + entry.name;
        entry.state = juce::ValueTree::fromXml(*xml);
        users.push_back(std::move(entry));
    }
    std::sort(users.begin(), users.end(), [](const PresetEntry& a, const PresetEntry& b) {
        return a.name.compareIgnoreCase(b.name) < 0;
    });
}

juce::String PresetLibrary::sanitize(juce::String name) {
    name = name.trim().substring(0, 80);
    juce::String cleaned;
    for(auto character : name) {
        if(character < 32 || juce::String("\\/:*?\"<>|").containsChar(character)) continue;
        cleaned += character;
    }
    cleaned = cleaned.trim();
    if(cleaned.isEmpty() || cleaned == "." || cleaned == "..") return {};
    return cleaned;
}

bool PresetLibrary::factoryNameTaken(const juce::String& name) const {
    for(const auto& preset : factory)
        if(preset.name.equalsIgnoreCase(name)) return true;
    return false;
}

bool PresetLibrary::saveUser(const juce::String& requested, juce::String& error) {
    const auto name = sanitize(requested);
    if(name.isEmpty()) { error = "Enter a preset name."; return false; }
    if(factoryNameTaken(name)) { error = "Factory presets are read-only."; return false; }
    auto dir = directory();
    if(!dir.createDirectory()) { error = "Could not create the preset folder."; return false; }
    const auto file = dir.getChildFile(name + ".xml");
    const auto xml = state.copyState().createXml();
    if(xml == nullptr || !xml->writeTo(file)) { error = "Could not write the preset."; return false; }
    active = "user:" + name;
    edited = false;
    rescan();
    return true;
}

bool PresetLibrary::renameUser(const juce::String& key, const juce::String& requested, juce::String& error) {
    if(!key.startsWith("user:")) { error = "Factory presets are read-only."; return false; }
    const auto name = sanitize(requested);
    if(name.isEmpty()) { error = "Enter a preset name."; return false; }
    if(factoryNameTaken(name)) { error = "Factory presets are read-only."; return false; }
    const auto oldName = key.fromFirstOccurrenceOf("user:", false, false);
    const auto source = directory().getChildFile(oldName + ".xml");
    const auto target = directory().getChildFile(name + ".xml");
    if(!source.existsAsFile()) { error = "That preset is no longer on disk."; return false; }
    if(source != target && target.existsAsFile()) { error = "A preset with that name already exists."; return false; }
    if(source != target && !source.moveFileTo(target)) { error = "Could not rename the preset."; return false; }
    const auto renamed = "user:" + name;
    if(favorites.contains(key)) {
        favorites.removeString(key);
        favorites.addIfNotAlreadyThere(renamed);
        storeFavorites();
    }
    if(active == key) active = renamed;
    rescan();
    return true;
}

bool PresetLibrary::deleteUser(const juce::String& key, juce::String& error) {
    if(!key.startsWith("user:")) { error = "Factory presets are read-only."; return false; }
    const auto name = key.fromFirstOccurrenceOf("user:", false, false);
    const auto file = directory().getChildFile(name + ".xml");
    if(file.existsAsFile() && !file.deleteFile()) { error = "Could not delete the preset."; return false; }
    if(favorites.contains(key)) {
        favorites.removeString(key);
        storeFavorites();
    }
    if(active == key) {
        active.clear();
        edited = false;
    }
    rescan();
    return true;
}

void PresetLibrary::setFavorite(const juce::String& key, bool favorite) {
    if(key.isEmpty() || find(key) == std::nullopt) return;
    if(favorite) favorites.addIfNotAlreadyThere(key);
    else favorites.removeString(key);
    storeFavorites();
}

bool PresetLibrary::isFavorite(const juce::String& key) const { return favorites.contains(key); }

std::vector<PresetEntry> PresetLibrary::filtered(const juce::String& search) const {
    const auto query = search.trim();
    std::vector<PresetEntry> result;
    const auto matches = [&](const juce::String& name) { return query.isEmpty() || name.containsIgnoreCase(query); };
    for(int i = 0; i < numFactory(); ++i) {
        if(!matches(factory[static_cast<size_t>(i)].name)) continue;
        PresetEntry entry;
        entry.name = factory[static_cast<size_t>(i)].name;
        entry.key = factoryKey(i);
        entry.factory = true;
        entry.favorite = isFavorite(entry.key);
        entry.program = i;
        entry.state = factory[static_cast<size_t>(i)].state;
        result.push_back(std::move(entry));
    }
    for(const auto& user : users) {
        if(!matches(user.name)) continue;
        auto entry = user;
        entry.favorite = isFavorite(entry.key);
        result.push_back(std::move(entry));
    }
    return result;
}

std::optional<PresetEntry> PresetLibrary::find(const juce::String& key) const {
    for(const auto& entry : filtered({}))
        if(entry.key == key) return entry;
    return std::nullopt;
}

void PresetLibrary::setActiveKey(juce::String key, bool isEdited) {
    active = std::move(key);
    edited = isEdited;
}

juce::String PresetLibrary::activeName() const {
    if(auto entry = find(active)) return entry->name;
    return active.isEmpty() ? juce::String("Preset") : juce::String("Custom");
}
}
