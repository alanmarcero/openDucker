#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <tuple>

namespace {
double trimGain(float db) { return std::pow(10.0,std::round(static_cast<double>(db)*100)/2000); }
constexpr std::array<double,8> delayBeats{0.125,0.25,0.5,0.75,1,1.5,2,4};
constexpr std::array<const char*,21> fxIds{"fxEnabled","duckTarget","duckPosition","fxMix","delayEnabled","delayLength",
    "delayFeedback","delayBass","delayTreble","delayMode","reverbEnabled","reverbType",
    "reverbLength","reverbWidth","reverbBass","reverbTreble","sustain","destroy","output","fxFlow","destroyMode"};
constexpr std::array<const char*,3> envelopeIds{"hold","recovery","phaseOffset"};
}
juce::AudioProcessorValueTreeState::ParameterLayout DuckerProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"amount",1},"Amount",juce::NormalisableRange<float>{0,100,0.1f},65.0f,juce::AudioParameterFloatAttributes{}.withLabel("%")));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"shape",1},"Slope",juce::StringArray{"Smooth","Punch","Snap","Swell"},0));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"rate",1},"Rate",juce::StringArray{"1/2","1/4","1/8","1/16","1/4 triplet","1/8 dotted"},1));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));
    const auto number=[&](const char* id,const char* name,float low,float high,float initial,const char* unit,float skew=1.0f) {
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,2},name,
            juce::NormalisableRange<float>{low,high,0.01f,skew},initial,juce::AudioParameterFloatAttributes{}.withLabel(unit)));
    };
    const auto toggle=[&](const char* id,const char* name,bool initial) {
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,2},name,initial));
    };
    const auto choice=[&](const char* id,const char* name,juce::StringArray choices,int initial) {
        result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id,2},name,choices,initial));
    };
    toggle("fxEnabled","Effects enabled",false);
    choice("duckTarget","Duck",{"Dry","Wet","Both"},2);
    choice("duckPosition","Ducking position",{"Before FX","After FX"},1);
    number("fxMix","FX mix",0,100,30,"%");
    toggle("delayEnabled","Delay enabled",true);
    choice("delayLength","Delay length",{"1/32","1/16","1/8","1/8 dotted","1/4","1/4 dotted","1/2","1 bar"},3);
    number("delayFeedback","Delay feedback",0,95,35,"%");
    number("delayBass","Delay bass",-12,12,0,"dB");
    number("delayTreble","Delay treble",-12,12,0,"dB");
    choice("delayMode","Delay mode",{"Mono","Ping-pong"},1);
    toggle("reverbEnabled","Reverb enabled",true);
    choice("reverbType","Reverb type",{"Room","Hall","Plate"},1);
    number("reverbLength","Reverb length",0.2f,25,6,"s",0.45f);
    number("reverbWidth","Reverb width",0,100,100,"%");
    number("reverbBass","Reverb bass",-12,12,0,"dB");
    number("reverbTreble","Reverb treble",-12,12,0,"dB");
    number("sustain","Sustain",0,100,0,"%");
    number("destroy","Destroy",0,100,0,"%");
    number("output","Output",-24,12,0,"dB");
    for(const auto& spec:std::array<std::tuple<const char*,const char*,float,float,float>,3>{{
        {"hold","Hold",0,80,0},{"recovery","Recovery",2,100,100},{"phaseOffset","Phase",0,100,0}}}) {
        const auto& [id,name,low,high,initial]=spec;
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,3},name,
            juce::NormalisableRange<float>{low,high,0.1f},initial,juce::AudioParameterFloatAttributes{}.withLabel("%")));
    }
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"fxFlow",3},"FX flow",juce::StringArray{"Serial","Parallel"},1));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"destroyMode",3},"Destroy mode",juce::StringArray{"Tail","Always"},1));
    return result;
}
DuckerProcessor::DuckerProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"openDucker",layout()) {
    amount=parameters.getRawParameterValue("amount"); shape=parameters.getRawParameterValue("shape");
    rate=parameters.getRawParameterValue("rate"); bypass=parameters.getRawParameterValue("bypass");
    for(size_t i=0;i<fxIds.size();++i) fxParameters[i]=parameters.getRawParameterValue(fxIds[i]);
    for(size_t i=0;i<envelopeIds.size();++i) envelopeParameters[i]=parameters.getRawParameterValue(envelopeIds[i]);
}
void DuckerProcessor::prepareToPlay(double sr,int) {
    currentSampleRate=sr;engine.prepare(sr);effects.prepare(sr);setLatencySamples(0);effectsRunning=false;
    for(auto* value:{&dryDuck,&wetPreDuck,&wetPostDuck,&fxBlend,&wetMix,&outputGain,&activeBlend}) value->reset(sr,0.02);
    const bool enabled=fxParameters[FxEnabled]->load()>0.5f && (fxParameters[DelayEnabled]->load()>0.5f || fxParameters[ReverbEnabled]->load()>0.5f
        || fxParameters[Sustain]->load()>0 || fxParameters[Destroy]->load()>0);
    const int target=static_cast<int>(fxParameters[DuckTarget]->load());
    const bool before=fxParameters[DuckPosition]->load()<0.5f;
    dryDuck.setCurrentAndTargetValue(!enabled || target!=1 ? 1.0 : 0.0);
    wetPreDuck.setCurrentAndTargetValue(enabled && target!=0 && before ? 1.0 : 0.0);
    wetPostDuck.setCurrentAndTargetValue(enabled && target!=0 && !before ? 1.0 : 0.0);
    fxBlend.setCurrentAndTargetValue(enabled && bypass->load()<0.5f ? 1.0 : 0.0);
    wetMix.setCurrentAndTargetValue(fxParameters[FxMix]->load()*0.01);
    outputGain.setCurrentAndTargetValue(trimGain(fxParameters[Output]->load()));
    activeBlend.setCurrentAndTargetValue(bypass->load()>0.5f ? 0.0 : 1.0);
    outputPeak.store(0);destroyLevel.store(0);
}
void DuckerProcessor::reset() {
    resetEffects.store(true);
    outputPeak.store(0,std::memory_order_relaxed);destroyLevel.store(0,std::memory_order_relaxed);
}
double DuckerProcessor::getTailLengthSeconds() const {
    if(fxParameters[FxEnabled]->load()<0.5f || bypass->load()>0.5f) return 0;
    const auto bpm=hostBpm.load();
    const auto length=delayBeats[static_cast<size_t>(juce::jlimit(0,7,static_cast<int>(fxParameters[DelayLength]->load())))];
    const double seconds=std::min(8.0,length*60.0/(bpm>0 ? bpm : 120.0));
    const double feedback=fxParameters[DelayFeedback]->load()*0.01;
    const double delayTail=fxParameters[DelayEnabled]->load()>0.5f ? seconds*(1+(feedback>0 ? std::ceil(std::log(0.00001)/std::log(feedback)) : 0)) : 0;
    const double reverbTail=fxParameters[ReverbEnabled]->load()>0.5f ? fxParameters[ReverbLength]->load()*2.0 : 0;
    const double destroyTail=fxParameters[Destroy]->load()>0
        && (delayTail>0 || reverbTail>0 || fxParameters[DestroyMode]->load()>0.5f) ? .06 : 0;
    return delayTail+reverbTail+destroyTail;
}
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
        static_cast<int>(rate->load(std::memory_order_relaxed)),bypassed || bypass->load(std::memory_order_relaxed)>0.5f,
        envelopeParameters[0]->load(std::memory_order_relaxed)*.01,envelopeParameters[1]->load(std::memory_order_relaxed)*.01,
        envelopeParameters[2]->load(std::memory_order_relaxed)*.01});
    const auto fx=[&](FxParameter index) {return fxParameters[index]->load(std::memory_order_relaxed);};
    const bool globalBypass=bypassed || bypass->load(std::memory_order_relaxed)>0.5f;
    const bool enabled=fx(FxEnabled)>0.5f && (fx(DelayEnabled)>0.5f || fx(ReverbEnabled)>0.5f || fx(Sustain)>0 || fx(Destroy)>0);
    const int target=static_cast<int>(fx(DuckTarget));
    const bool before=fx(DuckPosition)<0.5f;
    dryDuck.setTargetValue(!enabled || target!=1 ? 1.0 : 0.0);
    wetPreDuck.setTargetValue(enabled && target!=0 && before ? 1.0 : 0.0);
    wetPostDuck.setTargetValue(enabled && target!=0 && !before ? 1.0 : 0.0);
    fxBlend.setTargetValue(enabled && !globalBypass ? 1.0 : 0.0);
    activeBlend.setTargetValue(globalBypass ? 0.0 : 1.0);
    wetMix.setTargetValue(fx(FxMix)*0.01);
    outputGain.setTargetValue(trimGain(fx(Output)));
    duck::EffectsParameters settings;
    settings.delayEnabled=fx(DelayEnabled)>0.5f;
    const double delayTempo=t.valid ? t.bpm : 120.0;
    settings.delaySeconds=std::min(8.0,delayBeats[static_cast<size_t>(juce::jlimit(0,7,static_cast<int>(fx(DelayLength))))]*60/delayTempo);
    settings.feedback=fx(DelayFeedback)*0.01;settings.pingPong=fx(DelayMode)>0.5f;
    settings.delayBassDb=fx(DelayBass);settings.delayTrebleDb=fx(DelayTreble);
    settings.reverbEnabled=fx(ReverbEnabled)>0.5f;settings.reverbType=static_cast<int>(fx(ReverbType));
    settings.reverbSeconds=fx(ReverbLength);settings.reverbWidth=fx(ReverbWidth)*0.01;
    settings.reverbBassDb=fx(ReverbBass);settings.reverbTrebleDb=fx(ReverbTreble);
    settings.sustain=fx(Sustain)*0.01;settings.destroy=fx(Destroy)*0.01;
    settings.parallel=fx(FxFlow)>0.5f;settings.destroyAlways=fx(DestroyMode)>0.5f;
    if(resetEffects.exchange(false)) {effects.reset();effectsRunning=false;outputPeak.store(0,std::memory_order_relaxed);}
    effects.setParameters(settings);
    double gain=visualGain.load(std::memory_order_relaxed);
    const auto channels=buffer.getArrayOfWritePointers();
    const bool stereo=buffer.getNumChannels()>1;
    double blockPeak=0;
    for(int sample=0;sample<buffer.getNumSamples();++sample) {
        gain=engine.nextGain();
        const duck::Stereo input{static_cast<double>(channels[0][sample]),static_cast<double>(channels[stereo ? 1 : 0][sample])};
        const double dryGain=1+(gain-1)*dryDuck.getNextValue();
        const double preGain=1+(gain-1)*wetPreDuck.getNextValue();
        const double postGain=1+(gain-1)*wetPostDuck.getNextValue();
        const double blend=fxBlend.getNextValue();
        const double mix=wetMix.getNextValue()*blend;
        duck::Stereo wet;
        if(blend>0) {
            wet=effects.process({input.left*preGain,input.right*preGain},std::max(std::abs(input.left),std::abs(input.right)));
            effectsRunning=true;
        } else if(effectsRunning) {effects.reset();effectsRunning=false;}
        const double trim=outputGain.getNextValue();
        const double active=activeBlend.getNextValue();
        double left=((1-mix)*input.left*dryGain+mix*wet.left*postGain)*trim;
        double right=((1-mix)*input.right*dryGain+mix*wet.right*postGain)*trim;
        left=input.left+(left-input.left)*active;right=input.right+(right-input.right)*active;
        channels[0][sample]=static_cast<Sample>(stereo ? left : (left+right)*0.5);
        if(stereo) channels[1][sample]=static_cast<Sample>(right);
        blockPeak=std::max(blockPeak,stereo ? std::max(std::abs(left),std::abs(right)) : std::abs((left+right)*0.5));
    }
    outputPeak.store(static_cast<float>(std::max(blockPeak,outputPeak.load()*std::exp(-buffer.getNumSamples()/(currentSampleRate*0.25)))),std::memory_order_relaxed);
    destroyLevel.store(effectsRunning ? static_cast<float>(effects.destroyActivity()) : 0,std::memory_order_relaxed);
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
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(parameters.state.getType())) {
        auto state=juce::ValueTree::fromXml(*xml);
        for(const auto* id:fxIds) if(!state.getChildWithProperty("id",id).isValid()) {
            auto* parameter=parameters.getParameter(id);
            juce::ValueTree value{"PARAM"};value.setProperty("id",id,nullptr);
            value.setProperty("value",juce::String(id)=="fxFlow" || juce::String(id)=="destroyMode" ? 0.0f : parameter->convertFrom0to1(parameter->getDefaultValue()),nullptr);state.addChild(value,-1,nullptr);
        }
        for(const auto* id:envelopeIds) if(!state.getChildWithProperty("id",id).isValid()) {
            auto* parameter=parameters.getParameter(id);juce::ValueTree value{"PARAM"};value.setProperty("id",id,nullptr);
            value.setProperty("value",parameter->convertFrom0to1(parameter->getDefaultValue()),nullptr);state.addChild(value,-1,nullptr);
        }
        parameters.replaceState(state);resetEffects.store(true);
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new DuckerProcessor();}
