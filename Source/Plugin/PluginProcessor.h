#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Ducker.h"

class DuckerProcessor final : public juce::AudioProcessor {
public:
    DuckerProcessor();
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<double>&,juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override {return true;}
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override {return true;}
    const juce::String getName() const override {return "openDucker";}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    double getTailLengthSeconds() const override {return 0;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return {};}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> visualPhase{0}, visualGain{1}, hostBpm{0};
    std::atomic<int> syncState{0}; // 0: unavailable, 1: stopped, 2: playing
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    template<typename Sample> void process(juce::AudioBuffer<Sample>&,bool bypassed);
    duck::Engine engine;
    std::atomic<float>* amount=nullptr;
    std::atomic<float>* shape=nullptr;
    std::atomic<float>* rate=nullptr;
    std::atomic<float>* bypass=nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckerProcessor)
};
