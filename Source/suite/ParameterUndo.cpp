#include "ParameterUndo.h"

namespace suite {
bool onMessageThread() noexcept {
    auto* manager = juce::MessageManager::getInstanceWithoutCreating();
    return manager != nullptr && manager->isThisTheMessageThread();
}

class ParameterUndo::ValueAction : public juce::UndoableAction {
public:
    ValueAction(ParameterUndo& ownerIn, juce::String idIn, float beforeIn, float afterIn)
        : owner(ownerIn), id(std::move(idIn)), before(beforeIn), after(afterIn) {}
    bool perform() override { owner.applyNormalised(id, after); return true; }
    bool undo() override { owner.applyNormalised(id, before); return true; }
    int getSizeInUnits() override { return 1; }
private:
    ParameterUndo& owner;
    juce::String id;
    float before, after;
};

ParameterUndo::ParameterUndo(juce::AudioProcessor& processorIn, juce::AudioProcessorValueTreeState& stateIn)
    : processor(processorIn), state(stateIn) {
    for(auto* parameter : processor.getParameters()) parameter->addListener(this);
}

ParameterUndo::~ParameterUndo() {
    for(auto* parameter : processor.getParameters()) parameter->removeListener(this);
}

void ParameterUndo::suspend() noexcept { ++suspended; }
void ParameterUndo::resume() noexcept { if(suspended > 0) --suspended; }

void ParameterUndo::beginTransaction(const juce::String& name) { undo.beginNewTransaction(name); }

bool ParameterUndo::perform(juce::UndoableAction* action) { return undo.perform(action); }

bool ParameterUndo::undoStep() { return onMessageThread() && undo.undo(); }
bool ParameterUndo::redoStep() { return onMessageThread() && undo.redo(); }
bool ParameterUndo::canUndo() const { return undo.canUndo(); }
bool ParameterUndo::canRedo() const { return undo.canRedo(); }
void ParameterUndo::clearHistory() { undo.clearUndoHistory(); }

void ParameterUndo::applyNormalised(const juce::String& id, float normalised) {
    const bool wasApplying = applying;
    applying = true;
    if(auto* parameter = state.getParameter(id)) parameter->setValueNotifyingHost(normalised);
    applying = wasApplying;
}

void ParameterUndo::parameterGestureChanged(int parameterIndex, bool gestureIsStarting) {
    if(applying || suspended > 0 || !onMessageThread()) return;
    auto& list = processor.getParameters();
    if(parameterIndex < 0 || parameterIndex >= list.size()) return;
    auto* parameter = list[parameterIndex];
    auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
    if(identified == nullptr) return;
    if(gestureIsStarting) {
        gestureOpen = true;
        gestureId = identified->paramID;
        gestureStart = parameter->getValue();
        return;
    }
    if(!gestureOpen || gestureId != identified->paramID) return;
    gestureOpen = false;
    const float end = parameter->getValue();
    if(std::abs(end - gestureStart) < 1.0e-6f) return;
    undo.beginNewTransaction(parameter->getName(48));
    undo.perform(new ValueAction(*this, gestureId, gestureStart, end));
}
}
