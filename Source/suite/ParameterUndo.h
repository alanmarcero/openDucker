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
    // Each A/B slot keeps its own history: 0 is A, 1 is B.
    void selectHistory(size_t slot);
    void clearHistory(size_t slot);
    // Called after each recorded parameter edit.
    std::function<void()> onEdit;

private:
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;
    void applyNormalised(const juce::String& id, float normalised);

    class ValueAction;
    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    std::array<juce::UndoManager, 2> histories;
    size_t activeHistory = 0;
    int suspended = 0;
    bool applying = false;
    bool gestureOpen = false;
    juce::String gestureId;
    float gestureStart = 0;
};
}
