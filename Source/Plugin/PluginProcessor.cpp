#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout DuckerProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"amount",1},"Amount",juce::NormalisableRange<float>{0,100,0.1f},65.0f,juce::AudioParameterFloatAttributes{}.withLabel("%")));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"shape",1},"Slope",juce::StringArray{"Smooth","Punch","Snap","Swell"},0));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"rate",1},"Rate",juce::StringArray{"1/2","1/4","1/8","1/16","1/4 triplet","1/8 dotted"},1));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));
    return result;
}
DuckerProcessor::DuckerProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"openDucker",layout()) {
    amount=parameters.getRawParameterValue("amount"); shape=parameters.getRawParameterValue("shape");
    rate=parameters.getRawParameterValue("rate"); bypass=parameters.getRawParameterValue("bypass");
}
void DuckerProcessor::prepareToPlay(double sr,int) {engine.prepare(sr);setLatencySamples(0);}
bool DuckerProcessor::isBusesLayoutSupported(const BusesLayout& buses) const {
    return buses.getMainInputChannelSet()==buses.getMainOutputChannelSet()
        && (buses.getMainOutputChannelSet()==juce::AudioChannelSet::mono() || buses.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());
}
template<typename Sample> void DuckerProcessor::process(juce::AudioBuffer<Sample>& buffer,bool bypassed) {
    juce::ScopedNoDenormals noDenormals;
    for(int channel=getTotalNumInputChannels();channel<buffer.getNumChannels();++channel) buffer.clear(channel,0,buffer.getNumSamples());
    duck::Transport t;
    if(auto* playHead=getPlayHead()) if(const auto position=playHead->getPosition()) {
        t.playing=position->getIsPlaying();
        if(const auto ppq=position->getPpqPosition()) if(const auto bpm=position->getBpm()) {
            t.ppq=*ppq; t.bpm=*bpm;
            t.valid=std::isfinite(t.ppq) && std::abs(t.ppq)<1e12 && std::isfinite(t.bpm) && t.bpm>0 && t.bpm<=10000;
        }
        t.looping=position->getIsLooping();
        if(const auto points=position->getLoopPoints()) {t.loopStart=points->ppqStart;t.loopEnd=points->ppqEnd;}
    }
    engine.beginBlock(t,{amount->load(std::memory_order_relaxed)*0.01,static_cast<int>(shape->load(std::memory_order_relaxed)),
        static_cast<int>(rate->load(std::memory_order_relaxed)),bypassed || bypass->load(std::memory_order_relaxed)>0.5f});
    double gain=visualGain.load(std::memory_order_relaxed);
    const auto channels=buffer.getArrayOfWritePointers();
    for(int sample=0;sample<buffer.getNumSamples();++sample) {
        gain=engine.nextGain();
        for(int channel=0;channel<buffer.getNumChannels();++channel) channels[channel][sample]*=static_cast<Sample>(gain);
    }
    visualPhase.store(static_cast<float>(engine.phase()),std::memory_order_relaxed);
    visualGain.store(static_cast<float>(gain),std::memory_order_relaxed);
    hostBpm.store(t.valid ? static_cast<float>(t.bpm) : 0,std::memory_order_relaxed);
    syncState.store(t.valid ? (t.playing ? 2 : 1) : 0,std::memory_order_relaxed);
}
void DuckerProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) {process(b,false);}
void DuckerProcessor::processBlock(juce::AudioBuffer<double>& b,juce::MidiBuffer&) {process(b,false);}
void DuckerProcessor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer&) {process(b,true);}
void DuckerProcessor::processBlockBypassed(juce::AudioBuffer<double>& b,juce::MidiBuffer&) {process(b,true);}
juce::AudioProcessorParameter* DuckerProcessor::getBypassParameter() const {return parameters.getParameter("bypass");}
juce::AudioProcessorEditor* DuckerProcessor::createEditor() {return new DuckerEditor(*this);}
void DuckerProcessor::getStateInformation(juce::MemoryBlock& data) {
    if(auto xml=parameters.copyState().createXml()) copyXmlToBinary(*xml,data);
}
void DuckerProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new DuckerProcessor();}
