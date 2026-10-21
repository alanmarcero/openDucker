#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct Host : juce::AudioPlayHead {
    PositionInfo position;
    juce::Optional<PositionInfo> getPosition() const override {return position;}
};
juce::Component* find(juce::Component& root,const juce::String& id) {
    if(root.getComponentID()==id) return &root;
    for(auto* child:root.getChildren()) if(auto* c=find(*child,id)) return c;
    return nullptr;
}
void save(juce::Component& editor,const juce::File& directory,const juce::String& name) {
    auto stream=directory.getChildFile(name).createOutputStream();
    require(stream!=nullptr,"Cannot create screenshot");
    stream->setPosition(0); stream->truncate();
    require(juce::PNGImageFormat{}.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds(),true,1.5f),*stream),"Screenshot failed");
}
template<typename Sample> void testAudio() {
    DuckerProcessor p;
    Host host;
    host.position.setIsPlaying(true); host.position.setBpm(120); host.position.setPpqPosition(0);
    p.setPlayHead(&host);
    p.parameters.getParameter("amount")->setValueNotifyingHost(1);
    p.prepareToPlay(48000,511);
    juce::MidiBuffer midi;
    for(int start=0;start<48000;start+=511) {
        const int n=std::min(511,48000-start);
        juce::AudioBuffer<Sample> audio(2,n);
        for(int i=0;i<n;++i) {audio.setSample(0,i,Sample{0.8});audio.setSample(1,i,Sample{-0.4});}
        host.position.setPpqPosition(start/24000.0);
        p.processBlock(audio,midi);
        for(int i=0;i<n;++i) {
            const double left=audio.getSample(0,i),right=audio.getSample(1,i);
            require(left>=0 && left<=0.800001,"Plugin increases amplitude");
            require(std::abs(left+2*right)<1e-7,"Stereo link broken");
            if((start+i)%24000==0) require(std::abs(left)<1e-10,"JUCE adapter misses beat minimum");
        }
    }
    require(p.getLatencySamples()==0,"Unexpected latency");
    require(p.syncState.load()==2,"Transport not reported as synced");
    host.position.setBpm({});
    juce::AudioBuffer<Sample> audio(2,511);
    for(int i=0;i<511;++i) {audio.setSample(0,i,Sample{0.75}); audio.setSample(1,i,Sample{0.75});}
    p.processBlock(audio,midi);
    require(audio.getSample(0,510)==Sample{0.75},"Missing tempo does not become dry");
    require(p.syncState.load()==0,"Missing tempo claims sync");
    p.setPlayHead(nullptr);
}
}

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        testAudio<float>(); testAudio<double>();
        DuckerProcessor p; p.prepareToPlay(48000,256);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* amount=dynamic_cast<juce::Slider*>(find(*editor,"amount"));
        require(amount!=nullptr,"Amount knob missing");
        amount->setValue(80,juce::sendNotificationSync);
        require(p.parameters.getRawParameterValue("amount")->load()==80,"Knob disconnected");
        auto* shape=dynamic_cast<juce::TextButton*>(find(*editor,"shape2"));
        require(shape!=nullptr,"Slope control missing");
        shape->onClick();
        require(p.parameters.getRawParameterValue("shape")->load()==2,"Slope disconnected");
        auto* rate=dynamic_cast<juce::ComboBox*>(find(*editor,"rate"));
        require(rate!=nullptr,"Rate selector missing");
        rate->setSelectedId(3,juce::sendNotificationSync);
        require(p.parameters.getRawParameterValue("rate")->load()==2,"Rate disconnected");
        auto* bypass=dynamic_cast<juce::TextButton*>(find(*editor,"bypass"));
        require(bypass!=nullptr,"Bypass missing");
        bypass->setToggleState(true,juce::sendNotificationSync);
        require(p.getBypassParameter()->getValue()==1,"Bypass disconnected");
        juce::MemoryBlock state; p.getStateInformation(state);
        DuckerProcessor restored; restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        for(const auto* id:{"amount","shape","rate","bypass"})
            require(p.parameters.getParameter(id)->getValue()==restored.parameters.getParameter(id)->getValue(),"Session recall failed");
        const juce::File output=juce::File::getCurrentWorkingDirectory().getChildFile(argc>1 ? argv[1] : "docs/screenshots");
        require(output.createDirectory().wasOk(),"Cannot create output directory");
        bypass->setToggleState(false,juce::sendNotificationSync);
        amount->setValue(65,juce::sendNotificationSync);
        rate->setSelectedId(2,juce::sendNotificationSync);
        p.parameters.getParameter("shape")->setValueNotifyingHost(0);
        require(dynamic_cast<juce::TextButton*>(find(*editor,"shape0"))->getToggleState(),"Slope highlight lags host automation");
        Host host; host.position.setIsPlaying(true);host.position.setBpm(128);host.position.setPpqPosition(0.37);
        p.setPlayHead(&host);
        juce::AudioBuffer<float> buffer(2,256);buffer.clear();juce::MidiBuffer midi;
        p.processBlock(buffer,midi);
        save(*editor,output,"openDucker.png");
        amount->setValue(100,juce::sendNotificationSync);
        p.parameters.getParameter("shape")->setValueNotifyingHost(1);
        save(*editor,output,"openDucker-punch.png");
        p.setPlayHead(nullptr);
        std::cout<<"PASS float/double host sync, stereo, missing timing, latency, native controls, session recall, screenshots\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
