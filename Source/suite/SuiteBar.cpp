#include "SuiteBar.h"
#include <functional>
#include <vector>

namespace suite {
namespace {
class PresetBrowser final : public juce::Component {
public:
    PresetBrowser(SuiteHost& hostIn, Palette paletteIn, std::function<void()> changedIn)
        : host(hostIn), palette(paletteIn), changed(std::move(changedIn)) {
        const auto style = [&](juce::TextEditor& editor) {
            editor.setColour(juce::TextEditor::backgroundColourId, palette.panel);
            editor.setColour(juce::TextEditor::textColourId, palette.text);
            editor.setColour(juce::TextEditor::outlineColourId, palette.muted.withAlpha(0.4f));
            editor.setColour(juce::TextEditor::highlightColourId, palette.highlight.withAlpha(0.45f));
        };
        search.setComponentID("presetSearch");
        search.setTextToShowWhenEmpty("Search presets", palette.muted);
        search.onTextChange = [this] { rebuild(); };
        style(search);
        name.setComponentID("presetName");
        name.setTextToShowWhenEmpty("Preset name", palette.muted);
        style(name);
        save.setButtonText("Save"); save.setComponentID("presetSave");
        renameButton.setButtonText("Rename"); renameButton.setComponentID("presetRename");
        removeButton.setButtonText("Delete"); removeButton.setComponentID("presetDelete");
        save.onClick = [this] { saveCurrent(); };
        renameButton.onClick = [this] { renameCurrent(); };
        removeButton.onClick = [this] { deleteCurrent(); };
        for(auto* child : std::initializer_list<juce::Component*>{&search, &name, &save, &renameButton, &removeButton, &viewport, &error})
            addAndMakeVisible(*child);
        viewport.setViewedComponent(&list, false);
        viewport.setScrollBarsShown(true, false);
        error.setColour(juce::Label::textColourId, juce::Colour {0xffff8d7a});
        error.setJustificationType(juce::Justification::centredLeft);
        rebuild();
    }
    ~PresetBrowser() override { setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override { g.fillAll(palette.background); }

    void resized() override {
        auto area = getLocalBounds().reduced(8);
        search.setBounds(area.removeFromTop(28));
        area.removeFromTop(6);
        viewport.setBounds(area.removeFromTop(getHeight() - 132));
        area.removeFromTop(6);
        name.setBounds(area.removeFromTop(28));
        area.removeFromTop(6);
        auto buttons = area.removeFromTop(32);
        const int width = buttons.getWidth() / 3;
        save.setBounds(buttons.removeFromLeft(width).reduced(2, 0));
        renameButton.setBounds(buttons.removeFromLeft(width).reduced(2, 0));
        removeButton.setBounds(buttons.reduced(2, 0));
        error.setBounds(area.removeFromTop(22));
        layoutRows();
    }

    void rebuild() {
        list.removeAllChildren();
        rows.clear();
        const auto entries = host.presets().filtered(search.getText());
        for(const auto& entry : entries) {
            auto row = std::make_unique<juce::TextButton>((entry.favorite ? "* " : "") + entry.name + (entry.factory ? "" : "  ·"));
            row->setComponentID(entry.factory ? "factoryPreset" : "userPreset");
            row->setTooltip(entry.factory ? "Factory preset" : "User preset");
            row->setToggleState(entry.key == selected, juce::dontSendNotification);
            row->onClick = [this, key = entry.key, factory = entry.factory, presetName = entry.name] {
                selected = key;
                selectedFactory = factory;
                name.setText(factory ? juce::String() : presetName, juce::dontSendNotification);
                if(!host.loadPreset(key)) error.setText("Could not load that preset.", juce::dontSendNotification);
                else error.setText({}, juce::dontSendNotification);
                if(changed) changed();
                juce::MessageManager::callAsync([safe = juce::Component::SafePointer<PresetBrowser>(this)] {
                    if(safe != nullptr) safe->rebuild();
                });
            };
            list.addAndMakeVisible(*row);
            rows.push_back(std::move(row));
        }
        renameButton.setEnabled(!selectedFactory && selected.isNotEmpty());
        removeButton.setEnabled(!selectedFactory && selected.isNotEmpty());
        layoutRows();
    }

private:
    void layoutRows() {
        const int width = std::max(120, viewport.getWidth() - viewport.getScrollBarThickness() - 2);
        int y = 0;
        for(auto& row : rows) {
            row->setBounds(0, y, width, 28);
            y += 30;
        }
        list.setSize(width, std::max(y, 30));
    }

    void saveCurrent() {
        juce::String message;
        if(!host.savePreset(name.getText(), message)) error.setText(message, juce::dontSendNotification);
        else {
            error.setText({}, juce::dontSendNotification);
            selected = host.presets().activeKey();
            selectedFactory = false;
        }
        if(changed) changed();
        rebuild();
    }

    void renameCurrent() {
        juce::String message;
        if(!host.renamePreset(selected, name.getText(), message)) error.setText(message, juce::dontSendNotification);
        else {
            error.setText({}, juce::dontSendNotification);
            selected = host.presets().activeKey();
        }
        if(changed) changed();
        rebuild();
    }

    void deleteCurrent() {
        juce::String message;
        if(!host.deletePreset(selected, message)) error.setText(message, juce::dontSendNotification);
        else {
            error.setText({}, juce::dontSendNotification);
            selected.clear();
            selectedFactory = true;
        }
        if(changed) changed();
        rebuild();
    }

    SuiteHost& host;
    Palette palette;
    std::function<void()> changed;
    juce::TextEditor search, name;
    juce::TextButton save, renameButton, removeButton;
    juce::Viewport viewport;
    juce::Component list;
    juce::Label error;
    std::vector<std::unique_ptr<juce::TextButton>> rows;
    juce::String selected;
    bool selectedFactory = true;
};
}

SuiteBar::SuiteBar(SuiteHost& hostIn, Palette paletteIn) : host(hostIn), palette(paletteIn) {
    undoButton.setComponentID("undo"); undoButton.setTooltip("Undo the last parameter edit.");
    redoButton.setComponentID("redo"); redoButton.setTooltip("Redo the last undone edit.");
    aButton.setComponentID("slotA"); aButton.setTooltip("Recall slot A.");
    bButton.setComponentID("slotB"); bButton.setTooltip("Recall slot B.");
    aToBButton.setComponentID("copyAtoB"); aToBButton.setTooltip("Copy slot A into slot B.");
    bToAButton.setComponentID("copyBtoA"); bToAButton.setTooltip("Copy slot B into slot A.");
    presetButton.setComponentID("presets"); presetButton.setTooltip("Browse factory and user presets.");
    favoriteButton.setComponentID("favorite"); favoriteButton.setTooltip("Mark the current preset as a favorite.");
    undoButton.onClick = [this] { host.undoEdit(); refresh(); };
    redoButton.onClick = [this] { host.redoEdit(); refresh(); };
    aButton.onClick = [this] { host.selectSlotA(); refresh(); };
    bButton.onClick = [this] { host.selectSlotB(); refresh(); };
    aToBButton.onClick = [this] { host.copyAToB(); refresh(); };
    bToAButton.onClick = [this] { host.copyBToA(); refresh(); };
    presetButton.onClick = [this] { showBrowser(); };
    favoriteButton.onClick = [this] {
        const auto key = host.presets().activeKey();
        if(key.isNotEmpty()) host.setPresetFavorite(key, !host.presets().isFavorite(key));
        refresh();
    };
    for(auto* button : {&undoButton, &redoButton, &aButton, &bButton, &aToBButton, &bToAButton, &presetButton, &favoriteButton}) {
        button->setClickingTogglesState(false);
        addAndMakeVisible(*button);
    }
    refresh();
}

void SuiteBar::paint(juce::Graphics& g) {
    g.setColour(palette.panel.withAlpha(0.85f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
}

void SuiteBar::resized() {
    auto row = getLocalBounds();
    const int gap = 4;
    const auto take = [&](int width) {
        auto bounds = row.removeFromLeft(width);
        row.removeFromLeft(gap);
        return bounds;
    };
    undoButton.setBounds(take(64));
    redoButton.setBounds(take(64));
    row.removeFromLeft(gap);
    aButton.setBounds(take(42));
    bButton.setBounds(take(42));
    aToBButton.setBounds(take(54));
    bToAButton.setBounds(take(54));
    row.removeFromLeft(gap);
    favoriteButton.setBounds(row.removeFromRight(48));
    row.removeFromRight(gap);
    presetButton.setBounds(row);
}

void SuiteBar::refresh() {
    undoButton.setEnabled(host.canUndo());
    redoButton.setEnabled(host.canRedo());
    aButton.setToggleState(host.slotIsA(), juce::dontSendNotification);
    bButton.setToggleState(!host.slotIsA(), juce::dontSendNotification);
    const auto key = host.presets().activeKey();
    presetButton.setButtonText(host.presets().activeName());
    favoriteButton.setToggleState(key.isNotEmpty() && host.presets().isFavorite(key), juce::dontSendNotification);
    favoriteButton.setEnabled(key.isNotEmpty());
}

void SuiteBar::showBrowser() {
    auto browser = std::make_unique<PresetBrowser>(host, palette, [this] { refresh(); });
    browser->setLookAndFeel(&getLookAndFeel());
    browser->setSize(320, 420);
    juce::CallOutBox::launchAsynchronously(std::move(browser), presetButton.getScreenBounds(), nullptr);
}
}
