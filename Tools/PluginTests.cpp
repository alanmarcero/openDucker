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
    const auto bass = [](const std::vector<double>& s) { return tone(s, 50, 9600, 21600); };
    const auto treble = [](const std::vector<double>& s) { return tone(s, 5000, 9600, 21600); };
    require(bass(full) < 0.02 && treble(full) < 0.02, "Full-band ducking leaves signal at the bottom of the curve");
    require(bass(banded) < 0.06, "Band mode does not duck the bass");
    require(treble(banded) > 0.36 && treble(banded) < 0.44, "Band mode ducks the treble");
    require(tone(soloed, 50, 9600, 21600) > 0.36, "Band solo removes the bass");
    require(tone(soloed, 5000, 9600, 21600) < 0.02, "Band solo leaks the treble");
}
template<typename Sample> void testEffectsRouting() {
    const double midGain = duck::cycleGain(duck::defaultCurve, 0.5, 1);
    for(int target = 0; target < 3; ++target) for(int position = 0; position < 2; ++position) {
        DuckerProcessor p;
        set(p, "fxEnabled", 1); set(p, "delayEnabled", 1); set(p, "reverbEnabled", 0); set(p, "delayMode", 0);
        set(p, "delayLength", 2); set(p, "delayFeedback", 0); set(p, "fxMix", 50); set(p, "mix", 100); set(p, "smoothing", 0);
        set(p, "duckTarget", static_cast<float>(target)); set(p, "duckPosition", static_cast<float>(position));
        p.prepareToPlay(48000, 256);
        Host host; host.position.setIsPlaying(true); host.position.setBpm(120);
        p.setPlayHead(&host); juce::MidiBuffer midi;
        juce::AudioBuffer<Sample> warm(2, 24000); warm.clear(); host.position.setPpqPosition(-1); p.processBlock(warm, midi);
        juce::AudioBuffer<Sample> audio(2, 12001); audio.clear(); audio.setSample(0, 0, Sample {1}); audio.setSample(1, 0, Sample {1});
        host.position.setPpqPosition(0); p.processBlock(audio, midi);
        const double expectedDry = target == 1 ? 0.5 : 0;
        const double expectedWet = target == 0 ? 0.5 : (position == 0 ? 0 : 0.5 * midGain);
        require(std::abs(audio.getSample(0, 0) - expectedDry) < 1e-5, "Dry routing ducks the wrong branch");
        require(std::abs(audio.getSample(0, 12000) - expectedWet) < 1e-4, "Wet routing has wrong placement or gain");
        require(std::abs(audio.getSample(0, 12000) - audio.getSample(1, 12000)) < 1e-7, "Mono delay changes stereo link");
        set(p, "bypass", 1); warm.clear(); host.position.setPpqPosition(0.6); p.processBlock(warm, midi);
        audio.setSize(2, 256); audio.clear(); audio.setSample(0, 0, Sample {0.5}); audio.setSample(1, 0, Sample {-0.25});
        p.processBlock(audio, midi);
        require(std::abs(audio.getSample(0, 0) - 0.5) < 1e-7 && std::abs(audio.getSample(1, 0) + 0.25) < 1e-7, "Global bypass does not pass original audio");
        p.setPlayHead(nullptr);
    }
    DuckerProcessor restored;
    for(auto* parameter : restored.getParameters()) {
        const bool original = parameter->getParameterIndex() < 13;
        require(original ? parameter->getVersionHint() == 1 : parameter->getVersionHint() > 1, "New parameters can reorder saved AU automation");
    }
    restored.parameters.getParameter("fxEnabled")->setValueNotifyingHost(1);
    juce::ValueTree old {"openDucker"};
    for(const auto* id : {"mix", "curve", "rate", "length", "shift", "trigger", "retrigger", "threshold", "smoothing", "bandOn", "bandFrequency", "bandSolo", "bypass"}) {
        auto* parameter = restored.parameters.getParameter(id);
        juce::ValueTree value {"PARAM"};
        value.setProperty("id", id, nullptr);
        value.setProperty("value", parameter->convertFrom0to1(parameter->getDefaultValue()), nullptr);
        old.addChild(value, -1, nullptr);
    }
    juce::MemoryBlock data; juce::AudioProcessor::copyXmlToBinary(*old.createXml(), data);
    restored.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    require(restored.parameters.getRawParameterValue("fxEnabled")->load() < 0.5f, "Old sessions inherit enabled effects");
    require(restored.getTailLengthSeconds() < 1e-12, "Disabled effects report a tail");
    restored.parameters.getParameter("fxEnabled")->setValueNotifyingHost(1);
    restored.parameters.getParameter("reverbLength")->setValueNotifyingHost(1);
    require(restored.getTailLengthSeconds() >= 25, "Host may truncate long reverb tails");
}
template<typename Sample> void testFxMatrix() {
    for(int effect = 0; effect < 6; ++effect) for(int target = 0; target < 3; ++target) for(int position = 0; position < 2; ++position) {
        DuckerProcessor routed, clean, pre; Host host; host.position.setIsPlaying(true); host.position.setBpm(120);
        for(auto* p : {&routed, &clean, &pre}) {
            set(*p, "fxEnabled", 1); set(*p, "delayEnabled", effect == 0 || effect >= 4 ? 1.0f : 0.0f);
            set(*p, "reverbEnabled", effect == 1 || effect >= 4 ? 1.0f : 0.0f);
            set(*p, "sustain", effect == 2 ? 65.0f : 0.0f); set(*p, "destroy", effect == 3 ? 80.0f : 0.0f); set(*p, "destroyMode", 1);
            set(*p, "fxFlow", effect == 5 ? 0.0f : 1.0f); set(*p, "delayLength", 0); set(*p, "delayFeedback", 20);
            set(*p, "fxMix", 100); set(*p, "mix", 0); set(*p, "smoothing", 0); p->setPlayHead(&host);
        }
        set(routed, "fxMix", 40); set(routed, "mix", 100); set(routed, "duckTarget", static_cast<float>(target)); set(routed, "duckPosition", static_cast<float>(position));
        for(auto* p : {&routed, &clean, &pre}) p->prepareToPlay(48000, 257);
        duck::Engine envelope; envelope.prepare(48000); juce::MidiBuffer midi;
        for(int start = 0; start < 48000; start += 257) {
            const int count = std::min(257, 48000 - start); host.position.setPpqPosition(start / 24000.0);
            envelope.beginBlock({start / 24000.0, 120, true, true}, {duck::defaultCurve, 1, 0, 1.0, 0.0, duck::Trigger::Sync, true});
            juce::AudioBuffer<Sample> a(2, count), b(2, count), c(2, count); std::vector<double> gains;
            for(int i = 0; i < count; ++i) {
                const double gain = envelope.nextGain(); gains.push_back(gain);
                for(int ch = 0; ch < 2; ++ch) {
                    const auto x = static_cast<Sample>(0.04 * std::sin((start + i) * (ch == 0 ? 0.071 : 0.113)));
                    a.setSample(ch, i, x); b.setSample(ch, i, x); c.setSample(ch, i, static_cast<Sample>(x * gain));
                }
            }
            auto dry = a; routed.processBlock(a, midi); clean.processBlock(b, midi); pre.processBlock(c, midi);
            for(int i = 0; i < count; ++i) for(int ch = 0; ch < 2; ++ch) {
                const auto gain = gains[static_cast<size_t>(i)];
                const double wet = target == 0 ? b.getSample(ch, i) : position == 0 ? c.getSample(ch, i) : b.getSample(ch, i) * gain;
                const double expected = 0.6 * dry.getSample(ch, i) * (target == 1 ? 1 : gain) + 0.4 * wet;
                require(std::abs(a.getSample(ch, i) - expected) < 2e-5, "Clean/ducked FX matrix leaks or ducks the wrong path");
            }
        }
        for(auto* p : {&routed, &clean, &pre}) p->setPlayHead(nullptr);
    }
    std::cout << "PASS all clean/ducked routes for Delay, Reverb, Sustain, Destroy, Parallel and Serial\n";
}
void testNewFxModes() {
    DuckerProcessor p; set(p, "fxFlow", 1); set(p, "destroyMode", 1);
    set(p, "fxEnabled", 1); set(p, "delayEnabled", 0); set(p, "reverbEnabled", 0); set(p, "destroy", 100); set(p, "fxMix", 100); set(p, "mix", 0);
    p.prepareToPlay(48000, 256); juce::MidiBuffer midi; juce::AudioBuffer<double> buffer(2, 24000);
    double difference = 0;
    for(int i = 0; i < 24000; ++i) { buffer.setSample(0, i, 0.1 * std::sin(i * 0.13)); buffer.setSample(1, i, 0.1 * std::sin(i * 0.13)); }
    p.processBlock(buffer, midi);
    for(int i = 12000; i < 24000; ++i) difference += std::abs(buffer.getSample(0, i) - 0.1 * std::sin(i * 0.13));
    require(difference > 10 && p.destroyLevel.load() > 0.9, "Always Destroy is inaudible on sustained input without time effects");
    require(p.getTailLengthSeconds() >= 0.035, "Host may truncate standalone Destroy stutters");
    set(p, "delayEnabled", 1); set(p, "reverbEnabled", 1); set(p, "destroy", 0); set(p, "delayFeedback", 0);
    p.prepareToPlay(48000, 256); buffer.setSize(2, 4800); buffer.clear(); buffer.setSample(0, 0, 1); buffer.setSample(1, 0, 1); p.processBlock(buffer, midi);
    require(buffer.getMagnitude(0, buffer.getNumSamples()) > 0.01, "Parallel reverb waits for the quarter-note delay");
    require(std::abs(buffer.getSample(0, 0)) < 1e-12, "Fully wet parallel FX leaks dry audio");
    auto legacy = p.parameters.copyState();
    for(const auto* id : {"fxFlow", "destroyMode"}) legacy.removeChild(legacy.getChildWithProperty("id", id), nullptr);
    juce::MemoryBlock state; juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), state);
    p.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    require(p.parameters.getRawParameterValue("fxFlow")->load() < 0.5 && p.parameters.getRawParameterValue("destroyMode")->load() < 0.5, "Legacy sessions lose Serial/Tail modes");
    std::cout << "PASS audible FX modes and legacy session defaults\n";
}
template<typename Sample> void testStandaloneSustain() {
    DuckerProcessor p;
    for(const auto& [id, value] : std::initializer_list<std::pair<const char*, float>> {
        {"fxEnabled", 1}, {"delayEnabled", 0}, {"reverbEnabled", 0}, {"sustain", 100}, {"fxMix", 100}, {"mix", 0}}) set(p, id, value);
    p.prepareToPlay(48000, 256); juce::MidiBuffer midi;
    juce::AudioBuffer<Sample> audio(2, 48000);
    for(int i = 0; i < audio.getNumSamples(); ++i) {
        audio.setSample(0, i, static_cast<Sample>(0.01 * std::sin(i * 0.13)));
        audio.setSample(1, i, audio.getSample(0, i));
    }
    p.processBlock(audio, midi);
    require(audio.getMagnitude(0, 24000, 24000) > 0.1, "Sustain is silently disabled when Delay and Reverb are off");
    std::cout << "PASS standalone Sustain through the actual processor\n";
}
template<typename Sample> bool testHostReset(const char* precision) {
    DuckerProcessor p;
    set(p, "fxEnabled", 1); set(p, "delayEnabled", 1); set(p, "reverbEnabled", 0); set(p, "fxMix", 100);
    set(p, "mix", 0); set(p, "delayLength", 0); set(p, "delayFeedback", 0); set(p, "delayMode", 0);
    set(p, "duckTarget", 1); set(p, "duckPosition", 0);
    Host host; host.position.setIsPlaying(true); host.position.setBpm(120); host.position.setPpqPosition(0); p.setPlayHead(&host);
    p.prepareToPlay(48000, 64); juce::MidiBuffer midi;
    juce::AudioBuffer<Sample> buffer(2, 64);
    const auto impulse = [&] {
        buffer.setSize(2, 64); buffer.clear(); buffer.setSample(0, 0, Sample {1}); buffer.setSample(1, 0, Sample {1}); p.processBlock(buffer, midi);
    };
    const auto tail = [&] {
        buffer.setSize(2, 6000); buffer.clear(); p.processBlock(buffer, midi); return buffer.getMagnitude(0, buffer.getNumSamples());
    };
    impulse(); host.position.setIsPlaying(false);
    require(tail() > 0.9, "Stopping transport discards the delay tail");
    require(p.outputPeak.load() > 0.9f, "Reset regression did not populate the meter");
    impulse();
    std::vector<float> values;
    for(auto* parameter : p.getParameters()) values.push_back(parameter->getValue());
    juce::AudioProcessor& hostProcessor = p; hostProcessor.reset();
    const double peak = tail();
    std::cout << precision << " silent output peak after host reset=" << peak << '\n';
    for(int i = 0; i < p.getParameters().size(); ++i)
        require(std::abs(p.getParameters()[i]->getValue() - values[static_cast<size_t>(i)]) < 1e-7f, "Host reset changes parameters or routing");
    if(!std::isfinite(peak) || peak > 0) return false;
    require(p.outputPeak.load() <= 0 && p.destroyLevel.load() <= 0, "Host reset leaves stale effect meters");
    impulse(); require(tail() > 0.9, "Host reset disables configured effects");
    p.setPlayHead(nullptr);
    return true;
}
template<typename Sample> void testMonoAndDisabledEffects() {
    DuckerProcessor p;
    juce::AudioProcessor::BusesLayout buses; buses.inputBuses = {juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled()};
    buses.outputBuses = {juce::AudioChannelSet::mono()};
    require(p.setBusesLayout(buses), "Mono layout rejected");
    set(p, "fxEnabled", 1); set(p, "delayEnabled", 1); set(p, "reverbEnabled", 0); set(p, "fxMix", 100);
    set(p, "mix", 0); set(p, "smoothing", 0); set(p, "delayLength", 0); set(p, "delayFeedback", 0); set(p, "delayMode", 1);
    p.prepareToPlay(48000, 256); juce::MidiBuffer midi;
    Host host; host.position.setIsPlaying(true); host.position.setBpm(120); host.position.setPpqPosition(-1); p.setPlayHead(&host);
    juce::AudioBuffer<Sample> buffer(1, 24000); buffer.clear(); buffer.setSample(0, 0, Sample {1}); p.processBlock(buffer, midi);
    require(std::abs(buffer.getSample(0, 0)) < 1e-7, "Prepared fully wet preset leaks dry input at startup");
    buffer.setSize(1, 3001); buffer.clear(); buffer.setSample(0, 0, Sample {1}); host.position.setPpqPosition(0); p.processBlock(buffer, midi);
    require(std::abs(buffer.getSample(0, 3000) - 0.5) < 1e-6, "Mono output loses ping-pong echo");
    require(std::abs(p.outputPeak.load() - 0.5f) < 1e-5f, "Mono peak meter does not measure the actual output");
    set(p, "delayEnabled", 0); set(p, "duckTarget", 1); set(p, "mix", 100); set(p, "smoothing", 0);
    buffer.setSize(1, 24000); buffer.clear(); host.position.setPpqPosition(1); p.processBlock(buffer, midi);
    for(int i = 0; i < buffer.getNumSamples(); ++i) buffer.setSample(0, i, Sample {1});
    host.position.setPpqPosition(2); p.processBlock(buffer, midi);
    require(std::abs(buffer.getSample(0, 0)) < 1e-7, "Both FX off does not restore original ducking");
    require(std::abs(buffer.getSample(0, 12000) - duck::cycleGain(duck::defaultCurve, 0.5, 1)) < 1e-6, "FX Mix changes ducking gain with both FX off");
    require(p.getTailLengthSeconds() < 1e-9, "Both FX off reports a tail");
    p.setPlayHead(nullptr);
}
void testEqDetents(DuckerProcessor& p, juce::AudioProcessorEditor& editor) {
    for(const auto* id : {"delayBass", "delayTreble", "reverbBass", "reverbTreble"}) {
        auto* knob = dynamic_cast<juce::Slider*>(find(editor, id)); require(knob != nullptr, "EQ knob missing");
        auto* parameter = p.parameters.getParameter(id);
        const auto start = knob->getLocalBounds().toFloat().getCentre();
        const auto time = juce::Time::getCurrentTime();
        const auto event = [&](float distance, bool dragged) {
            return juce::MouseEvent {juce::Desktop::getInstance().getMainMouseSource(), start + juce::Point<float> {distance, 0},
                juce::ModifierKeys {juce::ModifierKeys::leftButtonModifier}, 1, 0, 0, 0, 0, knob, knob, time, start, time, 1, dragged};
        };
        for(const double direction : {1.0, -1.0}) {
            const double initial = -direction * 0.8;
            knob->setValue(initial, juce::sendNotificationSync); knob->mouseDown(event(0, false));
            for(const double attempted : {-direction * 0.1, direction * 0.1, direction * 0.8}) {
                const float distance = static_cast<float>((attempted - initial) / (knob->getMaximum() - knob->getMinimum()) * knob->getMouseDragSensitivity());
                knob->mouseDrag(event(distance, true));
                const double expected = std::abs(attempted) < 0.2 ? 0 : attempted;
                require(std::abs(knob->getValue() - expected) < 0.011, "EQ drag misses the zero detent or cannot leave it");
                require(std::abs(p.parameters.getRawParameterValue(id)->load() - expected) < 0.011, "EQ detent does not update its parameter");
            }
            knob->mouseUp(event(0, true));
        }
        for(const float value : {-0.1f, 0.1f}) {
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            require(std::abs(knob->getValue() - value) < 0.011 && std::abs(p.parameters.getRawParameterValue(id)->load() - value) < 0.011, "EQ detent quantizes host automation");
            require(std::abs(knob->snapValue(value, juce::Slider::notDragging) - value) < 1e-7, "EQ detent changes non-drag input");
        }
        knob->mouseDoubleClick(event(0, false)); require(std::abs(knob->getValue()) < 0.011, "EQ double-click default changed");
    }
    for(const auto* id : {"delayLength", "delayFeedback", "reverbLength", "reverbWidth", "sustain", "destroy", "fxMix", "output"}) {
        auto* knob = dynamic_cast<juce::Slider*>(find(editor, id)); require(knob != nullptr, "Non-EQ knob missing");
        require(std::abs(knob->snapValue(0.1, juce::Slider::absoluteDrag) - 0.1) < 1e-7, "EQ detent changes a non-EQ knob");
    }
    std::cout << "PASS EQ zero detents in both drag directions, continuous host automation and double-click defaults\n";
}
void testEffectsControls(DuckerProcessor& p, juce::AudioProcessorEditor& editor) {
    const std::array<const char*, 12> knobs {"delayLength", "delayFeedback", "delayBass", "delayTreble", "reverbLength", "reverbWidth", "reverbBass", "reverbTreble", "sustain", "destroy", "fxMix", "output"};
    for(const auto* id : knobs) {
        auto* knob = dynamic_cast<juce::Slider*>(find(editor, id)); require(knob != nullptr, "Effects knob missing");
        auto* parameter = p.parameters.getParameter(id);
        knob->setValue(parameter->convertFrom0to1(0.8f), juce::sendNotificationSync);
        require(std::abs(knob->getValue() - p.parameters.getRawParameterValue(id)->load()) < 0.011, "Effects knob disconnected");
    }
    for(const auto* id : {"duckTarget", "duckPosition", "delayMode", "reverbType", "fxFlow", "destroyMode"}) {
        auto* choice = dynamic_cast<juce::ComboBox*>(find(editor, id)); require(choice != nullptr, "Effects selector missing");
        for(int i = 1; i <= choice->getNumItems(); ++i) {
            choice->setSelectedId(i, juce::sendNotificationSync);
            require(std::abs(p.parameters.getRawParameterValue(id)->load() - static_cast<float>(i - 1)) < 0.01f, "Effects selector disconnected");
        }
    }
    auto* target = dynamic_cast<juce::ComboBox*>(find(editor, "duckTarget"));
    auto* position = find(editor, "duckPosition"); target->setSelectedId(1, juce::sendNotificationSync);
    require(!position->isEnabled(), "Dry-only leaves meaningless position switch enabled");
    target->setSelectedId(3, juce::sendNotificationSync); require(position->isEnabled(), "Both does not restore position switch");
    for(const auto* id : {"fxEnabled", "delayEnabled", "reverbEnabled"}) {
        auto* button = dynamic_cast<juce::TextButton*>(find(editor, id)); require(button != nullptr, "Effects switch missing");
        button->setToggleState(false, juce::sendNotificationSync); require(p.parameters.getRawParameterValue(id)->load() < 0.5f, "Effect cannot turn off");
        button->setToggleState(true, juce::sendNotificationSync); require(p.parameters.getRawParameterValue(id)->load() > 0.5f, "Effect cannot turn on");
    }
    auto* toggle = dynamic_cast<juce::TextButton*>(find(editor, "effectsToggle")); require(toggle != nullptr, "Effects collapse missing");
    toggle->onClick(); require(editor.getHeight() == 510 && !find(editor, "reverbType")->isShowing(), "Effects panel does not collapse");
    require(p.parameters.getRawParameterValue("fxEnabled")->load() > 0.5f, "Collapsing panel disables audio");
    toggle->onClick(); require(editor.getHeight() == 724, "Effects panel does not expand");
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
        testStandaloneSustain<float>(); testStandaloneSustain<double>(); testNewFxModes();
        testFxMatrix<float>(); testFxMatrix<double>();
        testEffectsRouting<float>(); testEffectsRouting<double>();
        testMonoAndDisabledEffects<float>(); testMonoAndDisabledEffects<double>();
        const bool floatReset = testHostReset<float>("float"), doubleReset = testHostReset<double>("double");
        require(floatReset && doubleReset, "Host reset leaves an audible effects tail");

        const juce::File output = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]) : juce::File::getCurrentWorkingDirectory();
        output.createDirectory();
        Host host; host.position.setIsPlaying(true); host.position.setBpm(128); host.position.setPpqPosition(0);
        DuckerProcessor p; p.setPlayHead(&host); p.prepareToPlay(sampleRate, 512);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        editor->setSize(1000, 724);
        testControls(p, *editor);
        testEffectsControls(p, *editor);
        testEqDetents(p, *editor);
        for(auto* parameter : p.getParameters()) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        for(const auto& [id, value] : std::initializer_list<std::pair<const char*, float>> {{"rate", 1}, {"trigger", 0}, {"curve", 2}, {"mix", 79},
            {"shift", 0}, {"threshold", -24}, {"retrigger", 1}, {"bandFrequency", 150}, {"smoothing", 1}, {"length", 100}, {"fxEnabled", 1}}) set(p, id, value);
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
            require(identified != nullptr && std::abs(restored.parameters.getParameter(identified->paramID)->getValue() - parameter->getValue()) < 1e-6f, "Session recall misses a parameter");
        }
        p.setPlayHead(nullptr);
        std::cout << "PASS float/double sync, mix, bypass, MIDI and audio triggers, sidechain layout, band split and solo, FX routing/matrix/reset, all native controls, session recall and screenshots\n";
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
