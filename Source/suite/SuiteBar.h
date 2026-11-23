#pragma once
#include "SuiteHost.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace suite {
struct Palette {
    juce::Colour panel;
    juce::Colour text;
    juce::Colour muted;
    juce::Colour accent;
    juce::Colour highlight;
    juce::Colour background;
};

class SuiteBar final : public juce::Component {
public:
    SuiteBar(SuiteHost&, Palette);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();
    std::unique_ptr<juce::Component> createBrowser();
private:
    void showBrowser();
    SuiteHost& host;
    Palette palette;
    std::function<void()> onDisarmBrowser;
    juce::TextButton undoButton {"Undo"}, redoButton {"Redo"}, aButton {"A"}, bButton {"B"},
        aToBButton {"A>B"}, bToAButton {"B>A"}, presetButton {"Presets"}, favoriteButton {"Fav"},
        previousPresetButton {"<"}, nextPresetButton {">"};
};
}
