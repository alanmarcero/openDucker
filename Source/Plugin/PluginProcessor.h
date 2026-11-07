#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Crossover.h"
#include "Ducker.h"
#include "Effects.h"
#include "TransientDetector.h"

class DuckerProcessor final : public juce::AudioProcessor {
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
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState parameters;
    /// Peak of the input and output at each position of the cycle, for the display.
    std::array<std::atomic<float>, displayBins> inputBins, outputBins;
    std::atomic<float> visualPhase {0}, visualGain {1}, hostBpm {0};
    std::atomic<float> outputPeak {0}, destroyLevel {0};
    std::atomic<bool> visualRunning {false};
    std::atomic<int> syncState {0}; // 0: unavailable, 1: stopped, 2: playing

private:
    enum Id { Mix, Curve, Rate, Length, Shift, TriggerMode, Retrigger, Threshold, Smoothing, BandOn, BandFrequency, BandSolo, Bypass, IdCount };
    enum FxParameter { FxEnabled, DuckTarget, DuckPosition, FxMix, DelayEnabled, DelayLength,
        DelayFeedback, DelayBass, DelayTreble, DelayMode, ReverbEnabled, ReverbType,
        ReverbLength, ReverbWidth, ReverbBass, ReverbTreble, Sustain, Destroy, Output, FxFlow, DestroyMode, FxCount };
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    template<typename Sample> void process(juce::AudioBuffer<Sample>&, juce::MidiBuffer&, bool bypassed);
    float value(Id id) const noexcept { return values[id]->load(std::memory_order_relaxed); }
    float fx(FxParameter id) const noexcept { return fxParameters[id]->load(std::memory_order_relaxed); }
    duck::Engine engine;
    duck::Crossover crossover;
    duck::TransientDetector detector;
    duck::Effects effects;
    std::array<std::atomic<float>*, IdCount> values {};
    std::array<std::atomic<float>*, FxCount> fxParameters {};
    juce::SmoothedValue<double> mixAmount, bandBlend, soloBlend, dryDuck, wetPreDuck, wetPostDuck, fxBlend, wetMix, outputGain, activeBlend;
    std::atomic<bool> resetEffects {false};
    bool effectsRunning = false;
    double sampleRate = 48000, bandFrequency = -1;
    int lastBin = -1;
    float binInput = 0, binOutput = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckerProcessor)
};
