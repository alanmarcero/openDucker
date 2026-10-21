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
int sparkPixels(const juce::Image& frame,const juce::Image& previous={}) {
    int count=0;
    for(int y=80;y<290;++y) for(int x=350;x<686;++x) {
        if(x>=418 && x<644) continue;
        const auto pixel=frame.getPixelAt(x,y);
        if(pixel.getRed()>160 && pixel.getGreen()>100 && pixel.getBlue()<pixel.getGreen()*0.75f
            && (previous.isNull() || pixel!=previous.getPixelAt(x,y))) ++count;
    }
    return count;
}
int changedPixels(const juce::Image& a,const juce::Image& b) {
    int count=0;
    for(int y=0;y<a.getHeight();++y) for(int x=0;x<a.getWidth();++x)
        if(a.getPixelAt(x,y)!=b.getPixelAt(x,y)) ++count;
    return count;
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
template<typename Sample> void testEffectsRouting() {
    for(int target=0;target<3;++target) for(int position=0;position<2;++position) {
        DuckerProcessor p;
        const auto set=[&](const char* id,float value) {
            auto* parameter=p.parameters.getParameter(id);require(parameter!=nullptr,"Effects parameter missing");
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        set("fxEnabled",1);set("delayEnabled",1);set("reverbEnabled",0);set("delayMode",0);
        set("delayLength",2);set("delayFeedback",0);set("fxMix",50);set("amount",100);
        set("duckTarget",static_cast<float>(target));set("duckPosition",static_cast<float>(position));
        p.prepareToPlay(48000,256);
        Host host;host.position.setIsPlaying(true);host.position.setBpm(120);
        p.setPlayHead(&host);juce::MidiBuffer midi;
        juce::AudioBuffer<Sample> warm(2,24000);warm.clear();host.position.setPpqPosition(-1);p.processBlock(warm,midi);
        juce::AudioBuffer<Sample> audio(2,12001);audio.clear();audio.setSample(0,0,Sample{1});audio.setSample(1,0,Sample{1});
        host.position.setPpqPosition(0);p.processBlock(audio,midi);
        const double expectedDry=target==1 ? 0.5 : 0;
        const double expectedWet=target==0 ? 0.5 : (position==0 ? 0 : 0.5*duck::envelope(0.5,0,0.006));
        require(std::abs(audio.getSample(0,0)-expectedDry)<1e-5,"Dry routing ducks the wrong branch");
        require(std::abs(audio.getSample(0,12000)-expectedWet)<1e-4,"Wet routing has wrong placement or gain");
        require(std::abs(audio.getSample(0,12000)-audio.getSample(1,12000))<1e-7,"Mono delay changes stereo link");
        set("bypass",1);warm.clear();host.position.setPpqPosition(0.6);p.processBlock(warm,midi);
        audio.setSize(2,256);audio.clear();audio.setSample(0,0,Sample{0.5});audio.setSample(1,0,Sample{-0.25});
        p.processBlock(audio,midi);
        require(std::abs(audio.getSample(0,0)-0.5)<1e-7 && std::abs(audio.getSample(1,0)+0.25)<1e-7,"Global bypass does not pass original audio");
        p.setPlayHead(nullptr);
    }
    DuckerProcessor restored;
    for(auto* parameter:restored.getParameters()) {
        const bool original=parameter->getParameterIndex()<4;
        require(original ? parameter->getVersionHint()==1 : parameter->getVersionHint()>1,"New parameters can reorder saved AU automation");
    }
    restored.parameters.getParameter("fxEnabled")->setValueNotifyingHost(1);
    juce::ValueTree old{"openDucker"};
    for(const auto* id:{"amount","shape","rate","bypass"}) {
        auto* parameter=restored.parameters.getParameter(id);juce::ValueTree value{"PARAM"};
        value.setProperty("id",id,nullptr);value.setProperty("value",parameter->convertFrom0to1(parameter->getDefaultValue()),nullptr);old.addChild(value,-1,nullptr);
    }
    juce::MemoryBlock data;juce::AudioProcessor::copyXmlToBinary(*old.createXml(),data);
    restored.setStateInformation(data.getData(),static_cast<int>(data.getSize()));
    require(restored.parameters.getRawParameterValue("fxEnabled")->load()<0.5f,"Old sessions inherit enabled effects");
    require(restored.getTailLengthSeconds()<1e-12,"Disabled effects report a tail");
    restored.parameters.getParameter("fxEnabled")->setValueNotifyingHost(1);
    restored.parameters.getParameter("reverbLength")->setValueNotifyingHost(1);
    require(restored.getTailLengthSeconds()>=25,"Host may truncate long reverb tails");
}
template<typename Sample> bool testHostReset(const char* precision) {
    DuckerProcessor p;
    const auto set=[&](const char* id,float value) {auto* parameter=p.parameters.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));};
    set("fxEnabled",1);set("delayEnabled",1);set("reverbEnabled",0);set("fxMix",100);
    set("amount",0);set("delayLength",0);set("delayFeedback",0);set("delayMode",0);
    set("duckTarget",1);set("duckPosition",0);
    Host host;host.position.setIsPlaying(true);host.position.setBpm(120);host.position.setPpqPosition(0);p.setPlayHead(&host);
    p.prepareToPlay(48000,64);juce::MidiBuffer midi;
    juce::AudioBuffer<Sample> buffer(2,64);
    const auto impulse=[&] {
        buffer.setSize(2,64);buffer.clear();buffer.setSample(0,0,Sample{1});buffer.setSample(1,0,Sample{1});p.processBlock(buffer,midi);
    };
    const auto tail=[&] {
        buffer.setSize(2,6000);buffer.clear();p.processBlock(buffer,midi);return buffer.getMagnitude(0,buffer.getNumSamples());
    };
    impulse();host.position.setIsPlaying(false);
    require(tail()>.9,"Stopping transport discards the delay tail");
    require(p.outputPeak.load()>.9f,"Reset regression did not populate the meter");
    impulse();
    std::vector<float> values;
    for(auto* parameter:p.getParameters()) values.push_back(parameter->getValue());
    juce::AudioProcessor& hostProcessor=p;hostProcessor.reset();
    const double peak=tail();
    std::cout<<precision<<" silent output peak after host reset="<<peak<<'\n';
    for(int i=0;i<p.getParameters().size();++i)
        require(std::abs(p.getParameters()[i]->getValue()-values[static_cast<size_t>(i)])<1e-7f,"Host reset changes parameters or routing");
    if(!std::isfinite(peak) || peak>0) return false;
    require(p.outputPeak.load()<=0 && p.destroyLevel.load()<=0,"Host reset leaves stale effect meters");
    impulse();require(tail()>.9,"Host reset disables configured effects");
    p.setPlayHead(nullptr);
    return true;
}
template<typename Sample> void testMonoAndDisabledEffects() {
    DuckerProcessor p;
    auto buses=p.getBusesLayout();buses.inputBuses.set(0,juce::AudioChannelSet::mono());buses.outputBuses.set(0,juce::AudioChannelSet::mono());
    require(p.setBusesLayout(buses),"Mono layout rejected");
    const auto set=[&](const char* id,float value) {auto* parameter=p.parameters.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));};
    set("fxEnabled",1);set("delayEnabled",1);set("reverbEnabled",0);set("fxMix",100);
    set("amount",0);set("delayLength",0);set("delayFeedback",0);set("delayMode",1);
    p.prepareToPlay(48000,256);juce::MidiBuffer midi;
    Host host;host.position.setIsPlaying(true);host.position.setBpm(120);host.position.setPpqPosition(-1);p.setPlayHead(&host);
    juce::AudioBuffer<Sample> buffer(1,24000);buffer.clear();buffer.setSample(0,0,Sample{1});p.processBlock(buffer,midi);
    require(std::abs(buffer.getSample(0,0))<1e-7,"Prepared fully wet preset leaks dry input at startup");
    buffer.setSize(1,3001);buffer.clear();buffer.setSample(0,0,Sample{1});host.position.setPpqPosition(0);p.processBlock(buffer,midi);
    require(std::abs(buffer.getSample(0,3000)-.5)<1e-6,"Mono output loses ping-pong echo");
    require(std::abs(p.outputPeak.load()-.5f)<1e-5f,"Mono peak meter does not measure the actual output");
    set("delayEnabled",0);set("duckTarget",1);set("amount",100);
    buffer.setSize(1,24000);buffer.clear();host.position.setPpqPosition(1);p.processBlock(buffer,midi);
    for(int i=0;i<buffer.getNumSamples();++i) buffer.setSample(0,i,Sample{1});
    host.position.setPpqPosition(2);p.processBlock(buffer,midi);
    require(std::abs(buffer.getSample(0,0))<1e-7,"Both FX off does not restore original ducking");
    require(buffer.getSample(0,12000)>.4,"FX Mix silences audio with both FX off");
    require(p.getTailLengthSeconds()<1e-9,"Both FX off reports a tail");
    p.setPlayHead(nullptr);
}
void testEqDetents(DuckerProcessor& p,juce::AudioProcessorEditor& editor) {
    for(const auto* id:{"delayBass","delayTreble","reverbBass","reverbTreble"}) {
        auto* knob=dynamic_cast<juce::Slider*>(find(editor,id));require(knob!=nullptr,"EQ knob missing");
        auto* parameter=p.parameters.getParameter(id);
        const auto start=knob->getLocalBounds().toFloat().getCentre();
        const auto time=juce::Time::getCurrentTime();
        const auto event=[&](float distance,bool dragged) {
            return juce::MouseEvent{juce::Desktop::getInstance().getMainMouseSource(),start+juce::Point<float>{distance,0},
                juce::ModifierKeys{juce::ModifierKeys::leftButtonModifier},1,0,0,0,0,knob,knob,time,start,time,1,dragged};
        };
        for(const double direction:{1.0,-1.0}) {
            const double initial=-direction*.8;
            knob->setValue(initial,juce::sendNotificationSync);knob->mouseDown(event(0,false));
            for(const double attempted:{-direction*.1,direction*.1,direction*.8}) {
                const float distance=static_cast<float>((attempted-initial)/(knob->getMaximum()-knob->getMinimum())*knob->getMouseDragSensitivity());
                knob->mouseDrag(event(distance,true));
                const double expected=std::abs(attempted)<.2 ? 0 : attempted;
                require(std::abs(knob->getValue()-expected)<.011,"EQ drag misses the zero detent or cannot leave it");
                require(std::abs(p.parameters.getRawParameterValue(id)->load()-expected)<.011,"EQ detent does not update its parameter");
            }
            knob->mouseUp(event(0,true));
        }
        for(const float value:{-.1f,.1f}) {
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            require(std::abs(knob->getValue()-value)<.011 && std::abs(p.parameters.getRawParameterValue(id)->load()-value)<.011,"EQ detent quantizes host automation");
            require(std::abs(knob->snapValue(value,juce::Slider::notDragging)-value)<1e-7,"EQ detent changes non-drag input");
        }
        knob->mouseDoubleClick(event(0,false));require(std::abs(knob->getValue())<.011,"EQ double-click default changed");
    }
    for(const auto* id:{"delayLength","delayFeedback","reverbLength","reverbWidth","sustain","destroy","fxMix","output"}) {
        auto* knob=dynamic_cast<juce::Slider*>(find(editor,id));require(knob!=nullptr,"Non-EQ knob missing");
        require(std::abs(knob->snapValue(.1,juce::Slider::absoluteDrag)-.1)<1e-7,"EQ detent changes a non-EQ knob");
    }
    std::cout<<"PASS EQ zero detents in both drag directions, continuous host automation and double-click defaults\n";
}
void testEffectsControls(DuckerProcessor& p,juce::AudioProcessorEditor& editor) {
    const std::array<const char*,12> knobs{"delayLength","delayFeedback","delayBass","delayTreble","reverbLength","reverbWidth","reverbBass","reverbTreble","sustain","destroy","fxMix","output"};
    for(const auto* id:knobs) {
        auto* knob=dynamic_cast<juce::Slider*>(find(editor,id));require(knob!=nullptr,"Effects knob missing");
        auto* parameter=p.parameters.getParameter(id);
        knob->setValue(parameter->convertFrom0to1(.8f),juce::sendNotificationSync);
        require(std::abs(knob->getValue()-p.parameters.getRawParameterValue(id)->load())<.011,"Effects knob disconnected");
    }
    for(const auto* id:{"duckTarget","duckPosition","delayMode","reverbType"}) {
        auto* choice=dynamic_cast<juce::ComboBox*>(find(editor,id));require(choice!=nullptr,"Effects selector missing");
        for(int i=1;i<=choice->getNumItems();++i) {
            choice->setSelectedId(i,juce::sendNotificationSync);
            require(std::abs(p.parameters.getRawParameterValue(id)->load()-static_cast<float>(i-1))<.01f,"Effects selector disconnected");
        }
    }
    auto* target=dynamic_cast<juce::ComboBox*>(find(editor,"duckTarget"));
    auto* position=find(editor,"duckPosition");target->setSelectedId(1,juce::sendNotificationSync);
    require(!position->isEnabled(),"Dry-only leaves meaningless position switch enabled");
    target->setSelectedId(3,juce::sendNotificationSync);require(position->isEnabled(),"Both does not restore position switch");
    for(const auto* id:{"fxEnabled","delayEnabled","reverbEnabled"}) {
        auto* button=dynamic_cast<juce::TextButton*>(find(editor,id));require(button!=nullptr,"Effects switch missing");
        button->setToggleState(false,juce::sendNotificationSync);require(p.parameters.getRawParameterValue(id)->load()<.5f,"Effect cannot turn off");
        button->setToggleState(true,juce::sendNotificationSync);require(p.parameters.getRawParameterValue(id)->load()>.5f,"Effect cannot turn on");
    }
    auto* toggle=dynamic_cast<juce::TextButton*>(find(editor,"effectsToggle"));require(toggle!=nullptr,"Effects collapse missing");
    toggle->onClick();require(editor.getHeight()==460 && !find(editor,"reverbType")->isShowing(),"Effects panel does not collapse");
    require(p.parameters.getRawParameterValue("fxEnabled")->load()>.5f,"Collapsing panel disables audio");
    toggle->onClick();require(editor.getHeight()==800,"Effects panel does not expand");
}
}

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        testAudio<float>(); testAudio<double>();
        testEffectsRouting<float>();testEffectsRouting<double>();
        testMonoAndDisabledEffects<float>();testMonoAndDisabledEffects<double>();
        const bool floatReset=testHostReset<float>("float"),doubleReset=testHostReset<double>("double");
        require(floatReset && doubleReset,"Host reset leaves an audible effects tail");
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
        testEffectsControls(p,*editor);
        testEqDetents(p,*editor);
        juce::MemoryBlock state; p.getStateInformation(state);
        DuckerProcessor restored; restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        for(auto* parameter:p.getParameters()) {
            const auto* identified=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
            require(identified!=nullptr && std::abs(parameter->getValue()-restored.parameters.getParameter(identified->paramID)->getValue())<1e-6f,"Session recall failed");
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
        }
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
        amount->setValue(0,juce::sendNotificationSync);
        save(*editor,output,"openDucker-dry.png");
        buffer.setSize(2,1);buffer.clear();
        const auto stage=[&] {return editor->createComponentSnapshot({350,79,336,218});};
        const auto atBeat=[&](double ppq) {
            host.position.setPpqPosition(ppq);p.processBlock(buffer,midi);return stage();
        };
        const auto animates=[&] {
            const auto first=stage();juce::Thread::sleep(120);
            const auto second=stage();juce::Thread::sleep(120);
            const auto third=stage();
            return changedPixels(first,second)>200 || changedPixels(first,third)>200;
        };
        const auto beat=atBeat(0);
        save(*editor,output,"openDucker-pump-beat.png");
        const auto recovery=atBeat(0.5);
        require(changedPixels(beat,recovery)>200,"Speaker does not pump with host position at zero Amount");
        save(*editor,output,"openDucker-pump-recovery.png");
        require(changedPixels(beat,atBeat(4))==0,"Speaker drifts from the host after a seek");
        host.position.setBpm(90);
        require(changedPixels(recovery,atBeat(4.5))==0,"Tempo change breaks the speaker's beat alignment");
        rate->setSelectedId(3,juce::sendNotificationSync);
        require(changedPixels(beat,atBeat(0.5))==0,"Speaker does not follow the selected eighth-note rate");
        require(changedPixels(recovery,atBeat(0.25))==0,"Speaker recovery ignores the selected rate");
        host.position.setIsPlaying(false);p.processBlock(buffer,midi);
        require(animates(),"Speaker stops animating when playback stops");
        host.position.setBpm({});p.processBlock(buffer,midi);
        require(animates(),"Speaker stops animating without host timing");
        host.position.setIsPlaying(true);host.position.setBpm(128);
        rate->setSelectedId(2,juce::sendNotificationSync);
        atBeat(0.37);
        amount->setValue(35,juce::sendNotificationSync);
        save(*editor,output,"openDucker-smoke.png");
        amount->setValue(69.9,juce::sendNotificationSync);
        save(*editor,output,"openDucker-before-sparks.png");
        require(sparkPixels(editor->createComponentSnapshot(editor->getLocalBounds()))==0,"Sparks appear below 70%");
        amount->setValue(70,juce::sendNotificationSync);
        const auto firstSparks=editor->createComponentSnapshot(editor->getLocalBounds());
        require(sparkPixels(firstSparks)>20,"Sparks missing at 70%");
        save(*editor,output,"openDucker-sparks.png");
        juce::Thread::sleep(120);
        require(sparkPixels(editor->createComponentSnapshot(editor->getLocalBounds()),firstSparks)>20,"Sparks do not animate while the knob is held at 70%");
        save(*editor,output,"openDucker-sparks-next.png");
        amount->setValue(100,juce::sendNotificationSync);
        p.parameters.getParameter("shape")->setValueNotifyingHost(1);
        save(*editor,output,"openDucker-punch.png");
        require(sparkPixels(editor->createComponentSnapshot(editor->getLocalBounds()))>20,"Sparks missing at 100%");
        p.parameters.getParameter("amount")->setValueNotifyingHost(0.699f);
        require(sparkPixels(editor->createComponentSnapshot(editor->getLocalBounds()))==0,"Sparks remain after automation lowers Amount below 70%");
        p.setPlayHead(nullptr);
        std::cout<<"PASS float/double host sync, FX routing/bypass/startup, mono audio/meter, AU parameter ordering, all native controls, session recall, speaker pump, sparks and screenshots\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
