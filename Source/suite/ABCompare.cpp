#include "ABCompare.h"

namespace suite {
ABCompare::ABCompare(juce::AudioProcessorValueTreeState& stateIn) : state(stateIn) {
    resetBothFromCurrent();
}

void ABCompare::captureActive() {
    auto current = state.copyState();
    if(activeIsA) slotA = current;
    else slotB = current;
}

void ABCompare::load(const juce::ValueTree& tree) {
    if(tree.isValid()) state.replaceState(tree.createCopy());
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
    slotB = slotA.createCopy();
    if(!activeIsA) load(slotB);
}

void ABCompare::copyBToA() {
    if(!activeIsA) captureActive();
    slotA = slotB.createCopy();
    if(activeIsA) load(slotA);
}

juce::ValueTree ABCompare::toTree() const {
    juce::ValueTree tree {"suite"};
    tree.setProperty("active", activeIsA ? "A" : "B", nullptr);
    juce::ValueTree a {"slotA"}, b {"slotB"};
    a.addChild(slotA.createCopy(), -1, nullptr);
    b.addChild(slotB.createCopy(), -1, nullptr);
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
    slotA = a.getChild(0).createCopy();
    slotB = b.getChild(0).createCopy();
    activeIsA = tree.getProperty("active").toString() != "B";
}

void ABCompare::resetBothFromCurrent() {
    slotA = state.copyState();
    slotB = slotA.createCopy();
    activeIsA = true;
}
}
