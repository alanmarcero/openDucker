#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ABCompare.h"
#include "ArtworkBeatClock.h"
#include "Crossover.h"
#include "Ducker.h"
#include "Effects.h"
#include "ParameterUndo.h"
#include "PresetLibrary.h"
#include "SidechainFilter.h"
#include "SuiteHost.h"
#include "TransientDetector.h"

class DuckerProcessor final : public juce::AudioProcessor, public suite::SuiteHost {
public:
    static constexpr int displayBins = 320;
    DuckerProcessor();
    void prepareToPlay(double, int) override;
    void reset() override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "openDucker"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    void undoEdit() override;
    void redoEdit() override;
    bool canUndo() const override;
    bool canRedo() const override;
    void selectSlotA() override;
    void selectSlotB() override;
    bool slotIsA() const override;
    void copyAToB() override;
    void copyBToA() override;
    suite::PresetLibrary& presets() override { return presetLibrary; }
    bool loadPreset(const juce::String&) override;
    bool savePreset(const juce::String&, juce::String&) override;
    bool renamePreset(const juce::String&, const juce::String&, juce::String&) override;
    bool deletePreset(const juce::String&, juce::String&) override;
    void setPresetFavorite(const juce::String&, bool) override;

    juce::AudioProcessorValueTreeState parameters;
    suite::ParameterUndo parameterUndo;
    suite::PresetLibrary presetLibrary;
    suite::ABCompare abCompare;
    /// Peak of the input, output, and beat-locked source at each position of the cycle.
    std::array<std::atomic<float>, displayBins> inputBins, outputBins, sourceBins;
    std::atomic<bool> sourceIsSidechain {false};
    float artworkPhase() const noexcept { return artworkClock.currentPhase(); }
    std::atomic<float> visualPhase {0}, visualBeat {0}, visualGain {1}, hostBpm {0};
    std::atomic<float> outputPeak {0}, destroyLevel {0};
    std::atomic<bool> visualRunning {false};
    std::atomic<int> syncState {0}; // 0: unavailable, 1: stopped, 2: playing
    // Accepted MIDI and audio triggers, and the cycle position of the last one.
    std::atomic<int> triggerCount {0};
    std::atomic<float> triggerBeat {0};
    // The highest detector level since the editor last read and reset it.
    std::atomic<float> detectorPeak {0};
    // The session saves these editor settings.
    std::atomic<int> editorWidth {1000};
    std::atomic<bool> effectsShown {true};
    void triggerDemoImpact() noexcept { triggerDemo.store(true); }

private:
    std::atomic<bool> triggerDemo {false};
    int demoImpactSamplesLeft = 0;
    double demoPhase = 0;
    uint32_t demoSeed = 0x9e3779b9;
    float demoFilterL = 0, demoFilterR = 0;
    enum Id { Mix, Curve, Rate, Length, Shift, TriggerMode, Retrigger, Threshold, Smoothing, BandOn, BandFrequency, BandSolo, Bypass,
        DetectorHpf, DetectorLpf, DetectorMonitor, Tension, Floor, BandSlope, IdCount };
    enum FxParameter { FxEnabled, DuckTarget, DuckPosition, FxMix, DelayEnabled, DelayLength,
        DelayFeedback, DelayBass, DelayTreble, DelayMode, ReverbEnabled, ReverbType,
        ReverbLength, ReverbWidth, ReverbBass, ReverbTreble, Sustain, Destroy, Output, FxFlow, DestroyMode, Lookahead, FxCount };
    struct SessionSnapshot {
        juce::ValueTree parameters;
        juce::ValueTree ab;
        int program = 0;
        juce::String presetKey;
        bool presetEdited = false;
    };
    class SnapshotAction;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    template<typename Sample> void process(juce::AudioBuffer<Sample>&, juce::MidiBuffer&, bool bypassed);
    SessionSnapshot snap();
    void restoreSnapshot(const SessionSnapshot&);
    void pushUndo(const juce::String&, SessionSnapshot before, SessionSnapshot after);
    int currentProgram = 0;
    float value(Id id) const noexcept { return values[id]->load(std::memory_order_relaxed); }
    float fx(FxParameter id) const noexcept { return fxParameters[id]->load(std::memory_order_relaxed); }
    duck::Engine engine;
    duck::Crossover crossover;
    duck::TransientDetector detector;
    std::array<duck::SidechainFilter, 2> detectorFilters;
    duck::Effects effects;
    std::array<std::vector<double>, 2> lookaheadLines;
    size_t lookaheadPosition = 0;
    int lookaheadSamples = -1;
    std::array<std::atomic<float>*, IdCount> values {};
    std::array<std::atomic<float>*, FxCount> fxParameters {};
    juce::SmoothedValue<double> mixAmount, bandBlend, soloBlend, dryDuck, wetPreDuck, wetPostDuck, fxBlend, wetMix, outputGain, activeBlend;
    std::atomic<bool> resetEffects {false};
    bool effectsRunning = false;
    ArtworkBeatClock artworkClock;
    double sampleRate = 48000, bandFrequency = -1;
    int lastBin = -1, sourceLastBin = -1, sourceRate = -1;
    float binInput = 0, binOutput = 0, sourceHold = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckerProcessor)
};
