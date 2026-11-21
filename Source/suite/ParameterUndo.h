#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace suite {
bool onMessageThread() noexcept;

/// Groups parameter edits into one undo step per begin/end gesture.
class ParameterUndo : private juce::AudioProcessorParameter::Listener {
public:
    ParameterUndo(juce::AudioProcessor&, juce::AudioProcessorValueTreeState&);
    ~ParameterUndo() override;

    void suspend() noexcept;
    void resume() noexcept;
    bool isSuspended() const noexcept { return suspended > 0; }
    bool isApplying() const noexcept { return applying; }

    void beginTransaction(const juce::String& name);
    bool perform(juce::UndoableAction* action);
    bool undoStep();
    bool redoStep();
    bool canUndo() const;
    bool canRedo() const;
    void clearHistory();
    // Called after each recorded parameter edit.
    std::function<void()> onEdit;

private:
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;
    void applyNormalised(const juce::String& id, float normalised);

    class ValueAction;
    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    juce::UndoManager undo;
    int suspended = 0;
    bool applying = false;
    bool gestureOpen = false;
    juce::String gestureId;
    float gestureStart = 0;
};
}
