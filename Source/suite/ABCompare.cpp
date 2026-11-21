#include "ABCompare.h"

namespace suite {
ABCompare::ABCompare(juce::AudioProcessorValueTreeState& stateIn, PresetLibrary& presetsIn) : state(stateIn), presets(presetsIn) {
    resetBothFromCurrent();
}

void ABCompare::captureActive() {
    auto& active = activeIsA ? slotA : slotB;
    active = {state.copyState(), presets.activeKey(), presets.isEdited()};
}

void ABCompare::load(const Slot& slot) {
    if(slot.state.isValid()) state.replaceState(slot.state.createCopy());
    presets.setActiveKey(slot.presetKey, slot.presetEdited);
}

bool ABCompare::selectA() {
    if(activeIsA) return false;
    captureActive();
    activeIsA = true;
    load(slotA);
    return true;
}

bool ABCompare::selectB() {
    if(!activeIsA) return false;
    captureActive();
    activeIsA = false;
    load(slotB);
    return true;
}

void ABCompare::copyAToB() {
    if(activeIsA) captureActive();
    slotB = {slotA.state.createCopy(), slotA.presetKey, slotA.presetEdited};
    if(!activeIsA) load(slotB);
}

void ABCompare::copyBToA() {
    if(!activeIsA) captureActive();
    slotA = {slotB.state.createCopy(), slotB.presetKey, slotB.presetEdited};
    if(activeIsA) load(slotA);
}

juce::ValueTree ABCompare::toTree() const {
    juce::ValueTree tree {"suite"};
    tree.setProperty("active", activeIsA ? "A" : "B", nullptr);
    juce::ValueTree a {"slotA"}, b {"slotB"};
    a.addChild(slotA.state.createCopy(), -1, nullptr);
    b.addChild(slotB.state.createCopy(), -1, nullptr);
    a.setProperty("presetKey", slotA.presetKey, nullptr);
    a.setProperty("presetEdited", slotA.presetEdited, nullptr);
    b.setProperty("presetKey", slotB.presetKey, nullptr);
    b.setProperty("presetEdited", slotB.presetEdited, nullptr);
    tree.addChild(a, -1, nullptr);
    tree.addChild(b, -1, nullptr);
    return tree;
}

void ABCompare::readTree(const juce::ValueTree& tree) {
    auto a = tree.getChildWithName("slotA");
    auto b = tree.getChildWithName("slotB");
    if(!a.isValid() || !b.isValid() || a.getNumChildren() == 0 || b.getNumChildren() == 0) {
        resetBothFromCurrent();
        return;
    }
    // Sessions saved before slots kept a preset key use the session's preset.
    slotA = {a.getChild(0).createCopy(), a.getProperty("presetKey", presets.activeKey()).toString(), static_cast<bool>(a.getProperty("presetEdited"))};
    slotB = {b.getChild(0).createCopy(), b.getProperty("presetKey", presets.activeKey()).toString(), static_cast<bool>(b.getProperty("presetEdited"))};
    activeIsA = tree.getProperty("active").toString() != "B";
}

void ABCompare::resetBothFromCurrent() {
    slotA = {state.copyState(), presets.activeKey(), presets.isEdited()};
    slotB = {slotA.state.createCopy(), slotA.presetKey, slotA.presetEdited};
    activeIsA = true;
}
}
