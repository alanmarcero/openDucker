#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <stdexcept>

namespace {
constexpr double sampleRate = 48000, beatSamples = 22500; // 128 BPM
void require(bool ok, const char* message) { if(!ok) throw std::runtime_error(message); }
struct Host : juce::AudioPlayHead {
    PositionInfo position;
    juce::Optional<PositionInfo> getPosition() const override { return position; }
};
juce::Component* find(juce::Component& root, const juce::String& id) {
    if(root.getComponentID() == id) return &root;
    for(auto* child : root.getChildren()) if(auto* found = find(*child, id)) return found;
    return nullptr;
}
template<typename T> T& control(juce::Component& root, const juce::String& id) {
    auto* found = dynamic_cast<T*>(find(root, id));
    require(found != nullptr, "A control is missing from the editor");
    return *found;
}
void set(DuckerProcessor& p, const char* id, float value) {
    auto* parameter = p.parameters.getParameter(id);
    require(parameter != nullptr, "Parameter missing");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(DuckerProcessor& p, const char* id) { return p.parameters.getRawParameterValue(id)->load(); }
void save(juce::Component& editor, const juce::File& directory, const juce::String& name) {
    auto stream = directory.getChildFile(name).createOutputStream();
    require(stream != nullptr, "Cannot create screenshot");
    stream->setPosition(0); stream->truncate();
    require(juce::PNGImageFormat {}.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds(), true, 2.0f), *stream), "Screenshot failed");
}
int yellowPixels(const juce::Image& image, juce::Rectangle<int> area) {
    int count = 0;
    for(int y = area.getY(); y < area.getBottom(); ++y) for(int x = area.getX(); x < area.getRight(); ++x) {
        const auto pixel = image.getPixelAt(x, y);
        if(pixel.getRed() > 220 && pixel.getGreen() > 190 && pixel.getBlue() < 110) ++count;
    }
    return count;
}
double kickAndBass(long n) {
    const double t = static_cast<double>(n % static_cast<long>(beatSamples)) / sampleRate, time = static_cast<double>(n) / sampleRate;
    const double kick = 0.85 * std::exp(-t * 12) * std::sin(6.283185307179586 * (50 * t + 3 * (1 - std::exp(-40 * t))));
    return std::clamp(kick + 0.45 * std::sin(6.283185307179586 * 55 * time) + 0.15 * std::sin(6.283185307179586 * 110 * time), -1.0, 1.0);
}
/// Feeds kick and bass through the processor in 512-sample blocks, with MIDI notes on every beat when requested.
template<typename Sample> void render(DuckerProcessor& p, Host& host, long from, long to, bool notes) {
    for(long start = from; start < to; start += 512) {
        const int n = static_cast<int>(std::min<long>(512, to - start));
        juce::AudioBuffer<Sample> audio(2, n);
        juce::MidiBuffer midi;
        for(int i = 0; i < n; ++i) {
            const auto sample = static_cast<Sample>(kickAndBass(start + i));
            audio.setSample(0, i, sample); audio.setSample(1, i, sample);
            if(notes && (start + i) % static_cast<long>(beatSamples) == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), i);
        }
        host.position.setPpqPosition(static_cast<double>(start) / beatSamples);
        p.processBlock(audio, midi);
    }
}
template<typename Sample> void testSync() {
    DuckerProcessor p; Host host;
    host.position.setIsPlaying(true); host.position.setBpm(120); host.position.setPpqPosition(0);
    p.setPlayHead(&host); set(p, "smoothing", 0);
    p.prepareToPlay(sampleRate, 511);
    juce::MidiBuffer midi;
    for(int start = 0; start < 48000; start += 511) {
        const int n = std::min(511, 48000 - start);
        juce::AudioBuffer<Sample> audio(2, n);
        for(int i = 0; i < n; ++i) { audio.setSample(0, i, Sample {0.8}); audio.setSample(1, i, Sample {-0.4}); }
        host.position.setPpqPosition(start / 24000.0);
        p.processBlock(audio, midi);
        for(int i = 0; i < n; ++i) {
            const double left = audio.getSample(0, i), right = audio.getSample(1, i), phase = std::fmod(start + i, 24000.0) / 24000.0;
            require(std::abs(left + 2 * right) < 1e-7, "Stereo link broken");
            require(std::abs(left - 0.8 * duck::cycleGain(duck::defaultCurve, phase, 1)) < 1e-6, "Output does not follow the Classic S curve");
        }
    }
    require(p.getLatencySamples() == 0 && p.syncState.load() == 2, "Unexpected latency or sync state");
    set(p, "mix", 0);
    juce::AudioBuffer<Sample> audio(2, 512);
    for(int i = 0; i < 512; ++i) { audio.setSample(0, i, Sample {0.5}); audio.setSample(1, i, Sample {0.5}); }
    const auto settle = [&] {
        for(int pass = 0; pass < 8; ++pass) {
            for(int i = 0; i < 512; ++i) { audio.setSample(0, i, Sample {0.5}); audio.setSample(1, i, Sample {0.5}); }
            host.position.setPpqPosition(0); p.processBlock(audio, midi);
        }
    };
    settle();
    require(std::abs(audio.getSample(0, 511) - 0.5) < 1e-6, "Mix 0% changes the signal");
    set(p, "mix", 50); settle();
    require(std::abs(audio.getSample(0, 0) - 0.25) < 1e-6, "Mix 50% does not blend the ducked and original signal");
    set(p, "mix", 100); set(p, "bypass", 1);
    settle();
    require(std::abs(audio.getSample(0, 511) - 0.5) < 1e-6, "Bypass does not restore the input");
    set(p, "bypass", 0); host.position.setBpm({}); p.processBlock(audio, midi);
    require(p.syncState.load() == 0, "Missing tempo claims sync");
    p.setPlayHead(nullptr);
    DuckerProcessor monoProcessor;
    juce::AudioProcessor::BusesLayout layout; layout.inputBuses = {juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled()};
    layout.outputBuses = {juce::AudioChannelSet::mono()};
    require(monoProcessor.setBusesLayout(layout), "Mono layout rejected");
    monoProcessor.prepareToPlay(sampleRate, 512);
    juce::AudioBuffer<Sample> mono(1, 512); mono.clear();
    for(int i = 0; i < 512; ++i) mono.setSample(0, i, Sample {0.5});
    monoProcessor.processBlock(mono, midi);
    require(mono.getSample(0, 100) == Sample {0.5}, "Mono processing without transport changes the signal");
}
template<typename Sample> void testTriggers() {
    DuckerProcessor p; Host host;
    host.position.setIsPlaying(false); host.position.setBpm(120); host.position.setPpqPosition(0);
    p.setPlayHead(&host); set(p, "smoothing", 0); set(p, "trigger", 1);
    p.prepareToPlay(sampleRate, 512);
    juce::AudioBuffer<Sample> audio(2, 512); juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 100);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 200);
    for(int i = 0; i < 512; ++i) { audio.setSample(0, i, Sample {0.6}); audio.setSample(1, i, Sample {0.6}); }
    p.processBlock(audio, midi);
    require(audio.getSample(0, 99) == Sample {0.6}, "MIDI mode ducks before a note");
    require(std::abs(audio.getSample(0, 100)) < 1e-9, "MIDI note does not restart the curve on its sample");
    require(audio.getSample(0, 300) > 0.0 && audio.getSample(0, 300) < 0.6, "Curve does not progress after the note");
    require(p.visualRunning.load(), "Triggered curve is not reported as running");
    set(p, "trigger", 2); set(p, "threshold", -24);
    DuckerProcessor audioMode; audioMode.setPlayHead(&host); set(audioMode, "smoothing", 0); set(audioMode, "trigger", 2);
    audioMode.prepareToPlay(sampleRate, 20000);
    juce::AudioBuffer<Sample> input(2, 20000); juce::MidiBuffer none;
    for(int i = 0; i < 20000; ++i) { const Sample level = i < 200 ? Sample {0.001} : Sample {0.5}; input.setSample(0, i, level); input.setSample(1, i, level); }
    audioMode.processBlock(input, none);
    require(std::abs(input.getSample(0, 199) - 0.001) < 1e-9, "Audio mode ducks below the threshold");
    require(std::abs(input.getSample(0, 200)) < 1e-9, "Audio transient does not trigger the curve");
    require(std::abs(input.getSample(0, 200 + 12000) - 0.5 * duck::cycleGain(duck::defaultCurve, 0.5, 1)) < 1e-6, "Audio-triggered curve has the wrong timing");
    require(audioMode.isBusesLayoutSupported([] { juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses = {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo()}; layout.outputBuses = {juce::AudioChannelSet::stereo()}; return layout; }()),
        "Stereo sidechain is rejected");
    require(!audioMode.isBusesLayoutSupported([] { juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses = {juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled()}; layout.outputBuses = {juce::AudioChannelSet::stereo()}; return layout; }()),
        "Mismatched main layout is accepted");
}
double tone(const std::vector<double>& signal, double hertz, long first, long last) {
    double sine = 0, cosine = 0;
    for(long i = first; i < last; ++i) {
        sine += signal[static_cast<size_t>(i)] * std::sin(6.283185307179586 * hertz * static_cast<double>(i) / sampleRate);
        cosine += signal[static_cast<size_t>(i)] * std::cos(6.283185307179586 * hertz * static_cast<double>(i) / sampleRate);
    }
    return 2 * std::hypot(sine, cosine) / static_cast<double>(last - first);
}
template<typename Sample> void testBand() {
    const auto run = [](bool band, bool solo, bool playing) {
        DuckerProcessor p; Host host;
        host.position.setIsPlaying(playing); host.position.setBpm(120); host.position.setPpqPosition(0);
        p.setPlayHead(&host); set(p, "smoothing", 0); set(p, "curve", 5); set(p, "bandOn", band ? 1.0f : 0.0f);
        set(p, "bandSolo", solo ? 1.0f : 0.0f); set(p, "bandFrequency", 150);
        p.prepareToPlay(sampleRate, 512);
        std::vector<double> output; juce::MidiBuffer midi;
        for(int start = 0; start < 48000; start += 512) {
            const int n = std::min(512, 48000 - start);
            juce::AudioBuffer<Sample> audio(2, n);
            for(int i = 0; i < n; ++i) {
                const double t = static_cast<double>(start + i) / sampleRate;
                const auto sample = static_cast<Sample>(0.4 * std::sin(6.283185307179586 * 50 * t) + 0.4 * std::sin(6.283185307179586 * 5000 * t));
                audio.setSample(0, i, sample); audio.setSample(1, i, sample);
            }
            host.position.setPpqPosition(start / 24000.0); p.processBlock(audio, midi);
            for(int i = 0; i < n; ++i) output.push_back(audio.getSample(0, i));
        }
        return output;
    };
    const auto full = run(false, false, true), banded = run(true, false, true), soloed = run(true, true, false);
    // Gate curve: fully ducked from 8% of each cycle onward. Measure inside the cycle's ducked region.
    const auto bass = [](const std::vector<double>& s) { return tone(s, 50, 9600, 21600); };
    const auto treble = [](const std::vector<double>& s) { return tone(s, 5000, 9600, 21600); };
    require(bass(full) < 0.02 && treble(full) < 0.02, "Full-band ducking leaves signal at the bottom of the curve");
    require(bass(banded) < 0.06, "Band mode does not duck the bass");
    require(treble(banded) > 0.36 && treble(banded) < 0.44, "Band mode ducks the treble");
    require(tone(soloed, 50, 9600, 21600) > 0.36, "Band solo removes the bass");
    require(tone(soloed, 5000, 9600, 21600) < 0.02, "Band solo leaks the treble");
}
void testControls(DuckerProcessor& p, juce::AudioProcessorEditor& editor) {
    auto& mix = control<juce::Slider>(editor, "mix");
    mix.setValue(80, juce::sendNotificationSync);
    require(get(p, "mix") == 80, "Mix knob disconnected");
    for(int i = 0; i < 4; ++i) {
        control<juce::TextButton>(editor, "rate" + juce::String(i)).onClick();
        require(get(p, "rate") == static_cast<float>(i), "Length button disconnected");
        require(control<juce::TextButton>(editor, "rate" + juce::String(i)).getToggleState(), "Length button does not show its state");
    }
    for(int i = 0; i < 3; ++i) {
        control<juce::TextButton>(editor, "trigger" + juce::String(i)).onClick();
        require(get(p, "trigger") == static_cast<float>(i), "Trigger button disconnected");
    }
    for(int i = 0; i < duck::curveCount; ++i) {
        control<juce::Button>(editor, "curve" + juce::String(i)).onClick();
        require(get(p, "curve") == static_cast<float>(i) && control<juce::Button>(editor, "curve" + juce::String(i)).getToggleState(), "Curve button disconnected");
    }
    control<juce::Button>(editor, "curve15").onClick();
    control<juce::Button>(editor, "next").onClick();
    require(get(p, "curve") == 0, "Next curve does not wrap");
    control<juce::Button>(editor, "previous").onClick();
    require(get(p, "curve") == 15, "Previous curve does not wrap");
    control<juce::Button>(editor, "previousBank").onClick();
    require(get(p, "curve") == 7, "Previous row jumps the wrong distance");
    control<juce::Button>(editor, "nextBank").onClick();
    require(get(p, "curve") == 15, "Next row jumps the wrong distance");
    control<juce::Button>(editor, "curve2").onClick();
    control<juce::Button>(editor, "retrigger").setToggleState(false, juce::sendNotificationSync);
    require(get(p, "retrigger") == 0, "Retrigger disconnected");
    control<juce::Button>(editor, "band").setToggleState(true, juce::sendNotificationSync);
    require(get(p, "bandOn") == 1, "Band button disconnected");
    control<juce::Button>(editor, "solo").setToggleState(true, juce::sendNotificationSync);
    require(get(p, "bandSolo") == 1, "Band solo disconnected");
    control<juce::Slider>(editor, "bandFrequency").setValue(300, juce::sendNotificationSync);
    require(std::abs(get(p, "bandFrequency") - 300) < 1, "Band slider disconnected");
    control<juce::Slider>(editor, "shift").setValue(25, juce::sendNotificationSync);
    require(std::abs(get(p, "shift") - 25) < 0.1, "Shift slider disconnected");
    control<juce::Slider>(editor, "threshold").setValue(-12, juce::sendNotificationSync);
    require(std::abs(get(p, "threshold") + 12) < 0.1, "Threshold slider disconnected");
    auto& display = control<juce::Component>(editor, "display");
    const auto time = juce::Time::getCurrentTime();
    const auto event = [&](juce::Point<float> position, bool dragged) {
        return juce::MouseEvent {juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys {juce::ModifierKeys::leftButtonModifier},
            1, 0, 0, 0, 0, &display, &display, time, position, time, 1, dragged};
    };
    const float width = static_cast<float>(display.getWidth());
    display.mouseDown(event({width - 3, 100}, false));
    display.mouseDrag(event({width * 0.5f, 100}, true));
    display.mouseUp(event({width * 0.5f, 100}, true));
    require(std::abs(get(p, "length") - 50) < 1, "Dragging the handle does not set the slope length");
    display.mouseDoubleClick(event({width * 0.5f, 100}, false));
    require(get(p, "length") == 100, "Double-clicking the handle does not reset the slope length");
    set(p, "bandOn", 0); set(p, "bandSolo", 0); set(p, "shift", 0); set(p, "retrigger", 1);
}
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        testSync<float>(); testSync<double>(); testTriggers<float>(); testTriggers<double>(); testBand<float>(); testBand<double>();
        const juce::File output = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]) : juce::File::getCurrentWorkingDirectory();
        output.createDirectory();
        Host host; host.position.setIsPlaying(true); host.position.setBpm(128); host.position.setPpqPosition(0);
        DuckerProcessor p; p.setPlayHead(&host); p.prepareToPlay(sampleRate, 512);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        editor->setSize(1000, 510);
        testControls(p, *editor);
        for(const auto& [id, value] : std::initializer_list<std::pair<const char*, float>> {{"rate", 1}, {"trigger", 0}, {"curve", 2}, {"mix", 79},
            {"shift", 0}, {"threshold", -24}, {"retrigger", 1}, {"bandFrequency", 150}, {"smoothing", 1}, {"length", 100}}) set(p, id, value);
        p.reset(); render<float>(p, host, 0, 22500 * 2, false);
        save(*editor, output, "openDucker-sync.png");
        const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        require(yellowPixels(image, {357, 55, 631, 295}) > 600, "The display does not draw the curve");
        require(yellowPixels(image, {357, 405, 631, 95}) > 100, "The selected curve button is not highlighted");
        set(p, "mix", 79); set(p, "curve", 4); set(p, "length", 72); set(p, "trigger", 2); set(p, "rate", 1);
        host.position.setIsPlaying(false);
        p.reset(); render<float>(p, host, 0, 22500 * 3, false);
        save(*editor, output, "openDucker-audio.png");
        set(p, "trigger", 1); set(p, "curve", 14); set(p, "bandOn", 1); set(p, "bandFrequency", 150); set(p, "mix", 100); set(p, "length", 100);
        p.reset(); render<float>(p, host, 0, 22500 * 3, true);
        save(*editor, output, "openDucker-band-midi.png");
        juce::MemoryBlock state; p.getStateInformation(state);
        DuckerProcessor restored; restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        for(auto* parameter : p.getParameters()) {
            const auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
            require(identified != nullptr && restored.parameters.getParameter(identified->paramID)->getValue() == parameter->getValue(), "Session recall misses a parameter");
        }
        p.setPlayHead(nullptr);
        std::cout << "PASS float/double sync, mix, bypass, MIDI and audio triggers, sidechain layout, band split and solo, all native controls, session recall and screenshots\n";
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
