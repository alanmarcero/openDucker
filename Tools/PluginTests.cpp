#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <stdexcept>

namespace {
constexpr double sampleRate = 48000, beatSamples = 22500; // 128 BPM
void require(bool ok, const char* message) { if(!ok) throw std::runtime_error(message); }
void checkButtonLayout(const juce::Component& parent, int minimum) {
    for (int i = 0; i < parent.getNumChildComponents(); ++i) {
        const auto* child = parent.getChildComponent(i);
        if (!child->isVisible()) continue;
        const bool actionable = dynamic_cast<const juce::Button*>(child) != nullptr
                             || dynamic_cast<const juce::ComboBox*>(child) != nullptr;
        if (!actionable) {
            checkButtonLayout(*child, minimum);
            continue;
        }
        const auto bounds = child->getBounds();
        require(bounds.getWidth() >= minimum && bounds.getHeight() >= minimum, "An action target is too small");
        for (int j = i + 1; j < parent.getNumChildComponents(); ++j) {
            const auto* other = parent.getChildComponent(j);
            if (!other->isVisible()) continue;
            if (dynamic_cast<const juce::Button*>(other) || dynamic_cast<const juce::ComboBox*>(other))
                require(!bounds.intersects(other->getBounds()), "Two action targets overlap");
        }
    }
}

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
int changedPixels(const juce::Image& before, const juce::Image& after, juce::Rectangle<int> area) {
    int count = 0;
    for(int y = area.getY(); y < area.getBottom(); ++y) for(int x = area.getX(); x < area.getRight(); ++x)
        if(before.getPixelAt(x, y) != after.getPixelAt(x, y)) ++count;
    return count;
}
int yellowPixels(const juce::Image& image, juce::Rectangle<int> area) {
    int count = 0;
    for(int y = area.getY(); y < area.getBottom(); ++y) for(int x = area.getX(); x < area.getRight(); ++x) {
        const auto pixel = image.getPixelAt(x, y);
        if(pixel.getRed() > 220 && pixel.getGreen() > 160 && pixel.getBlue() < 110) ++count;
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
    require(mono.getSample(0, 100) != Sample {0.5}, "Mono processing without transport ducks the signal");
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

template<typename Sample> void testLookahead() {
    DuckerProcessor p;
    Host host;
    host.position.setIsPlaying(false); host.position.setBpm(120); host.position.setPpqPosition(0);
    p.setPlayHead(&host); set(p, "smoothing", 0); set(p, "trigger", 1); set(p, "lookahead", 3);
    p.prepareToPlay(sampleRate, 512);
    require(p.getLatencySamples() == 144, "Lookahead does not report 3 ms of latency at 48 kHz");
    juce::AudioBuffer<Sample> audio(2, 26000); juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 200);
    int i = 0;
    while (i < 26000) {
        const auto s = static_cast<Sample>(i >= 200 ? 0.6 : 0.0);
        audio.setSample(0, i, s); audio.setSample(1, i, s);
        ++i;
    }
    p.processBlock(audio, midi);
    require(std::abs(static_cast<double>(audio.getSample(0, 200))) < 1e-9, "Lookahead leaks the transient before the delay elapses");
    require(std::abs(static_cast<double>(audio.getSample(0, 343))) < 1e-9, "Lookahead delay line is shorter than reported");
    require(std::abs(static_cast<double>(audio.getSample(0, 344))) < 0.01, "Transient emerges before the duck reaches the floor");
    require(std::abs(static_cast<double>(audio.getSample(0, 25900)) - 0.6) < 1e-6, "Delayed signal never emerges after lookahead");
    p.setPlayHead(nullptr);
    DuckerProcessor plain;
    plain.prepareToPlay(sampleRate, 512);
    require(plain.getLatencySamples() == 0, "Default lookahead reports latency");
}

template<typename Sample> void testDetectorFilters() {
    DuckerProcessor highPass;
    set(highPass, "trigger", 2);
    set(highPass, "smoothing", 0);
    set(highPass, "detectorHpf", 1000);
    highPass.prepareToPlay(sampleRate, 48000);
    juce::AudioBuffer<Sample> lowTone(2, 48000);
    juce::MidiBuffer midi;
    for(int i = 0; i < 48000; ++i) {
        const auto value = static_cast<Sample>(0.5 * std::sin(6.283185307179586 * 100.0 * i / sampleRate));
        lowTone.setSample(0, i, value); lowTone.setSample(1, i, value);
    }
    highPass.processBlock(lowTone, midi);
    require(!highPass.visualRunning.load(), "High-pass detector triggers on low-frequency energy");

    DuckerProcessor lowPass;
    set(lowPass, "trigger", 2);
    set(lowPass, "smoothing", 0);
    set(lowPass, "detectorLpf", 500);
    lowPass.prepareToPlay(sampleRate, 48000);
    juce::AudioBuffer<Sample> highTone(2, 48000);
    for(int i = 0; i < 48000; ++i) {
        const auto value = static_cast<Sample>(0.5 * std::sin(6.283185307179586 * 5000.0 * i / sampleRate));
        highTone.setSample(0, i, value); highTone.setSample(1, i, value);
    }
    lowPass.processBlock(highTone, midi);
    require(!lowPass.visualRunning.load(), "Low-pass detector triggers on high-frequency energy");

    DuckerProcessor monitor;
    set(monitor, "detectorLpf", 500);
    set(monitor, "detectorMonitor", 1);
    monitor.prepareToPlay(sampleRate, 48000);
    juce::AudioBuffer<Sample> audition(2, 48000);
    for(int i = 0; i < 48000; ++i) {
        const auto value = static_cast<Sample>(0.5 * std::sin(6.283185307179586 * 5000.0 * i / sampleRate));
        audition.setSample(0, i, value); audition.setSample(1, i, value);
    }
    monitor.processBlock(audition, midi);
    double energy = 0;
    for(int i = 24000; i < 48000; ++i) energy += audition.getSample(0, i) * audition.getSample(0, i);
    const double monitoredRms = std::sqrt(energy / 24000.0);
    require(monitoredRms < 0.08, "Detector listen does not output the filtered signal");
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
        if (solo) set(p, "mix", 0);
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

    // Partial bandBlend (0 < bandAmount < 1) must stay phase-coherent at bandFrequency without comb cancellation.
    DuckerProcessor coherent;
    Host coherentHost;
    coherentHost.position.setIsPlaying(false);
    coherentHost.position.setBpm(120);
    coherent.setPlayHead(&coherentHost);
    set(coherent, "mix", 0);
    set(coherent, "bandOn", 0);
    set(coherent, "bandFrequency", 150);
    coherent.prepareToPlay(sampleRate, 960);
    juce::MidiBuffer midi;
    juce::AudioBuffer<Sample> warmBuf(2, 4800);
    int wIdx = 0;
    while (wIdx < 4800) {
        const auto s = static_cast<Sample>(0.5 * std::sin(6.283185307179586 * 150.0 * wIdx / sampleRate));
        warmBuf.setSample(0, wIdx, s);
        warmBuf.setSample(1, wIdx, s);
        ++wIdx;
    }
    coherent.processBlock(warmBuf, midi);
    set(coherent, "bandOn", 1);
    juce::AudioBuffer<Sample> transBuf(2, 960);
    int tIdx = 0;
    while (tIdx < 960) {
        const auto s = static_cast<Sample>(0.5 * std::sin(6.283185307179586 * 150.0 * (4800 + tIdx) / sampleRate));
        transBuf.setSample(0, tIdx, s);
        transBuf.setSample(1, tIdx, s);
        ++tIdx;
    }
    coherent.processBlock(transBuf, midi);
    double midTransitionPeak = 0.0;
    int mIdx = 320;
    while (mIdx < 640) {
        midTransitionPeak = std::max(midTransitionPeak, std::abs(static_cast<double>(transBuf.getSample(0, mIdx))));
        ++mIdx;
    }
    require(midTransitionPeak > 0.38, "Multiband blend creates a phase comb notch at bandFrequency");
    coherent.setPlayHead(nullptr);
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
    toggle->onClick(); require(editor.getHeight() == 596 && !find(editor, "reverbType")->isShowing(), "Effects panel does not collapse");
    require(p.parameters.getRawParameterValue("fxEnabled")->load() > 0.5f, "Collapsing panel disables audio");
    toggle->onClick(); require(editor.getHeight() == 804, "Effects panel does not expand");
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
    auto& band = control<juce::Slider>(editor, "bandFrequency");
    require(band.getSliderStyle() == juce::Slider::LinearBar && band.getTextFromValue(150) == "150 Hz", "The band split is not a named bar");
    const auto bandGroup = control<juce::Component>(editor, "band").getBounds().getUnion(control<juce::Component>(editor, "solo").getBounds())
        .getUnion(band.getBounds()).getUnion(control<juce::Component>(editor, "bandSlope").getBounds());
    require(!bandGroup.intersects(control<juce::Component>(editor, "pushIt").getBounds())
        && !bandGroup.intersects(control<juce::Component>(editor, "effectsToggle").getBounds()), "The band controls are not grouped");
    control<juce::ComboBox>(editor, "bandSlope").setSelectedId(3, juce::sendNotificationSync);
    require(std::abs(get(p, "bandSlope") - 2) < 0.01f, "Band slope selector disconnected");
    control<juce::Slider>(editor, "shift").setValue(25, juce::sendNotificationSync);
    require(std::abs(get(p, "shift") - 25) < 0.1, "Shift slider disconnected");
    control<juce::Slider>(editor, "tension").setValue(40, juce::sendNotificationSync);
    require(std::abs(get(p, "tension") - 40) < 0.5, "Tension slider disconnected");
    control<juce::Slider>(editor, "floor").setValue(20, juce::sendNotificationSync);
    require(std::abs(get(p, "floor") - 20) < 0.5, "Floor slider disconnected");
    control<juce::Slider>(editor, "threshold").setValue(-12, juce::sendNotificationSync);
    require(std::abs(get(p, "threshold") + 12) < 0.1, "Threshold slider disconnected");
    control<juce::Slider>(editor, "detectorHpf").setValue(1200, juce::sendNotificationSync);
    require(std::abs(get(p, "detectorHpf") - 1200) < 1, "Detector high-pass slider disconnected");
    control<juce::Slider>(editor, "detectorLpf").setValue(8000, juce::sendNotificationSync);
    require(std::abs(get(p, "detectorLpf") - 8000) < 1, "Detector low-pass slider disconnected");
    control<juce::Button>(editor, "detectorMonitor").setToggleState(true, juce::sendNotificationSync);
    require(get(p, "detectorMonitor") > 0.5f, "Detector listen button disconnected");
    set(p, "shift", 0);
    auto& display = control<juce::Component>(editor, "display");
    const auto time = juce::Time::getCurrentTime();
    const auto event = [&](juce::Point<float> position, bool dragged) {
        return juce::MouseEvent {juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys {juce::ModifierKeys::leftButtonModifier},
            1, 0, 0, 0, 0, &display, &display, time, position, time, 1, dragged};
    };
    const float width = static_cast<float>(display.getWidth());
    display.mouseDown(event({width - 3, 100}, false));
    display.mouseDrag(event({width * 0.5f, 100}, true));
    const auto whileDragging = display.createComponentSnapshot(display.getLocalBounds());
    display.mouseUp(event({width * 0.5f, 100}, true));
    require(std::abs(get(p, "length") - 50) < 1, "Dragging the handle does not set the slope length");
    const auto grip = juce::Rectangle<int>(juce::roundToInt(width * 0.5f) - 56, 6, 56, 28);
    require(changedPixels(whileDragging, display.createComponentSnapshot(display.getLocalBounds()), grip) > 30, "The slope grip shows no value while dragging");
    display.mouseDoubleClick(event({width * 0.5f, 100}, false));
    require(get(p, "length") == 100, "Double-clicking the handle does not reset the slope length");
    auto& slopeBar = control<juce::Slider>(editor, "length");
    slopeBar.setValue(60, juce::sendNotificationSync);
    require(std::abs(get(p, "length") - 60) < 0.1f && slopeBar.getTextFromValue(60) == "60%", "The Slope bar is disconnected");
    for(const auto* id : {"bandFrequency", "shift", "tension", "floor", "threshold", "detectorHpf", "detectorLpf"}) {
        const auto& bar = control<juce::Slider>(editor, id);
        require(!bar.getSliderSnapsToMousePosition() && bar.isTextBoxEditable(), "A bar jumps to the click point or takes no typed value");
    }
    auto& thresholdBar = control<juce::Slider>(editor, "threshold");
    thresholdBar.setValue(-24, juce::sendNotificationSync);
    const auto press = [&](float x, bool dragged) {
        return juce::MouseEvent {juce::Desktop::getInstance().getMainMouseSource(), {x, 20}, juce::ModifierKeys {juce::ModifierKeys::leftButtonModifier},
            1, 0, 0, 0, 0, &thresholdBar, &thresholdBar, time, {5, 20}, time, 1, dragged};
    };
    thresholdBar.mouseDown(press(5, false));
    thresholdBar.mouseDrag(press(25, true));
    thresholdBar.mouseUp(press(25, true));
    require(std::abs(get(p, "threshold") - (-24.0 + 20.0 / thresholdBar.getMouseDragSensitivity() * 60.0)) < 0.2, "Dragging a bar jumps to the click point");
    auto* typedBox = dynamic_cast<juce::Label*>(thresholdBar.getChildComponent(0));
    require(typedBox != nullptr, "The threshold bar has no text box");
    typedBox->setText("-18 dB", juce::sendNotificationSync);
    require(std::abs(get(p, "threshold") + 18) < 0.05f, "Typing into a bar does not set its parameter");
    auto& lowPass = control<juce::Slider>(editor, "detectorLpf");
    require(lowPass.valueToProportionOfLength(0) > 0.999 && lowPass.valueToProportionOfLength(20000) < 0.99 && lowPass.valueToProportionOfLength(1) < 0.001,
        "The detector LPF does not put Off at the right end");
    set(p, "bandOn", 0); set(p, "bandSolo", 0); set(p, "shift", 0); set(p, "tension", 0); set(p, "floor", 0); set(p, "bandSlope", 1); set(p, "retrigger", 1);
    set(p, "threshold", -24); set(p, "length", 100);
}

void renderCycle(DuckerProcessor& processor, Host& host, int cycleSamples, bool sidechain, float mainLevel, int clickSamples) {
    const int channels = processor.getTotalNumInputChannels();
    for(int start = 0; start < cycleSamples; start += 512) {
        const int count = std::min(512, cycleSamples - start);
        juce::AudioBuffer<float> audio(channels, count);
        audio.clear();
        juce::MidiBuffer midi;
        for(int i = 0; i < count; ++i) {
            const float click = (start + i) < clickSamples ? 0.95f : 0.0f;
            const float mainSample = sidechain ? mainLevel : click;
            audio.setSample(0, i, mainSample);
            if(channels > 1) audio.setSample(1, i, mainSample);
            if(sidechain && channels > 2) {
                audio.setSample(2, i, click);
                if(channels > 3) audio.setSample(3, i, click);
            }
        }
        host.position.setPpqPosition(static_cast<double>(start) / static_cast<double>(cycleSamples));
        processor.processBlock(audio, midi);
    }
}

void testBeatLockedWaveform() {
    constexpr int cycleSamples = 24000; // 120 BPM, one beat, the default 1/4 cycle
    Host host;
    host.position.setIsPlaying(true); host.position.setBpm(120); host.position.setPpqPosition(0);
    DuckerProcessor side;
    juce::AudioProcessor::BusesLayout buses;
    buses.inputBuses = {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo()};
    buses.outputBuses = {juce::AudioChannelSet::stereo()};
    require(side.setBusesLayout(buses), "Sidechain layout rejected");
    side.setPlayHead(&host);
    set(side, "smoothing", 0); set(side, "rate", 1);
    side.prepareToPlay(sampleRate, 512);
    renderCycle(side, host, cycleSamples, true, 0.4f, 60);
    require(side.sourceIsSidechain.load(), "Connected sidechain is not the waveform source");
    require(side.sourceBins[0].load() > 0.8f, "Sidechain click is not at the start of the host cycle");
    require(side.sourceBins[static_cast<size_t>(DuckerProcessor::displayBins / 2)].load() < 0.05f, "Sidechain waveform is not locked to the cycle");
    side.reset();
    set(side, "shift", 50);
    renderCycle(side, host, cycleSamples, true, 0.4f, 60);
    require(side.sourceBins[0].load() > 0.8f, "Shift moved the beat-locked sidechain click");
    require(side.sourceBins[static_cast<size_t>(DuckerProcessor::displayBins / 2)].load() < 0.05f, "Shift redrew the sidechain across the cycle");
    side.setPlayHead(nullptr);

    DuckerProcessor direct;
    direct.setPlayHead(&host);
    set(direct, "smoothing", 0); set(direct, "rate", 1);
    direct.prepareToPlay(sampleRate, 512);
    renderCycle(direct, host, cycleSamples, false, 0.0f, 60);
    require(!direct.sourceIsSidechain.load(), "Main input was reported as a sidechain");
    require(direct.sourceBins[0].load() > 0.8f, "Main input click is not at the start of the host cycle");
    require(direct.sourceBins[static_cast<size_t>(DuckerProcessor::displayBins / 2)].load() < 0.05f, "Main input waveform is not locked to the cycle");
    direct.setPlayHead(nullptr);
}

void testParameterText() {
    DuckerProcessor p;
    for(auto* parameter : p.getParameters()) {
        const auto* number = dynamic_cast<juce::AudioParameterFloat*>(parameter);
        if(number == nullptr) continue;
        const auto& range = number->getNormalisableRange();
        for(const float position : {0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.0f, parameter->getDefaultValue()}) {
            const auto text = parameter->getText(position, 0);
            const float typed = parameter->getValueForText(text);
            require(parameter->getText(typed, 0) == text, "Typed parameter text does not round-trip");
            const float tolerance = std::max((range.end - range.start) * 0.01f, range.interval);
            require(std::abs(range.convertFrom0to1(typed) - range.convertFrom0to1(position)) <= tolerance,
                "Typed parameter text lands on another value");
        }
    }
    const auto typed = [&](const char* id, const char* text) {
        auto* parameter = p.parameters.getParameter(id);
        return parameter->convertFrom0to1(parameter->getValueForText(text));
    };
    require(std::abs(typed("mix", "50") - 50) < 1e-3f && std::abs(typed("mix", "50%") - 50) < 1e-3f, "Typed percent values are misread");
    require(std::abs(typed("detectorHpf", "6.8k") - 6800) < 1 && std::abs(typed("detectorHpf", "6.8 kHz") - 6800) < 1, "Typed kHz values are misread");
    require(typed("detectorLpf", "Off") < 0.5f, "Typed Off does not bypass the detector filter");
    require(std::abs(typed("lookahead", "2 ms") - 2) < 1e-3f && std::abs(typed("lookahead", "0.002 s") - 2) < 1e-3f, "Typed lookahead units are misread");
    require(std::abs(typed("reverbLength", "500 ms") - 0.5f) < 1e-3f, "Typed reverb ms values are misread");
    require(std::abs(typed("tension", "+20%") - 20) < 1e-3f && std::abs(typed("threshold", "-12 dB") + 12) < 1e-3f, "Typed signed values are misread");
    auto* bass = p.parameters.getParameter("delayBass");
    require(bass->getText(bass->convertTo0to1(-0.01f), 0) == "0.0 dB", "Gain text shows -0.0 dB");
}

void testEditorRecall() {
    DuckerProcessor first;
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(first.createEditor());
        editor->setSize(1300, 1045);
        control<juce::TextButton>(*editor, "effectsToggle").onClick();
        require(editor->getWidth() == 1300 && editor->getHeight() == 775, "Hiding FX does not keep the editor width");
    }
    juce::MemoryBlock state;
    first.getStateInformation(state);
    DuckerProcessor second;
    second.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    std::unique_ptr<juce::AudioProcessorEditor> editor(second.createEditor());
    require(editor->getWidth() == 1300 && control<juce::TextButton>(*editor, "effectsToggle").getButtonText() == "Show FX",
        "The editor does not reopen at its saved width and FX visibility");
    const auto scale = control<juce::Component>(*editor, "content").getTransform();
    require(std::abs(scale.mat00 - 1.3f) < 1e-4f && std::abs(scale.mat11 - 1.3f) < 1e-4f, "The editor content does not scale uniformly");
}

void testSuite(DuckerProcessor& processor, juce::AudioProcessorEditor& editor) {
    require(processor.getNumPrograms() == 16, "Factory bank should contain 16 patches");
    juce::StringArray names;
    for(int i = 0; i < processor.getNumPrograms(); ++i) {
        const auto name = processor.getProgramName(i);
        require(name.isNotEmpty() && !names.contains(name), "Factory program names must be present and unique");
        names.add(name);
    }
    processor.changeProgramName(0, "Renamed");
    require(processor.getProgramName(0) == "Classic Pump", "Factory presets must stay read-only");
    processor.setCurrentProgram(2);
    require(processor.getCurrentProgram() == 2, "Program index did not follow the host");
    require(std::abs(get(processor, "curve") - 5.0f) < 0.01f && std::abs(get(processor, "rate")) < 0.01f, "Trance Gate did not load");
    processor.setCurrentProgram(0);
    require(std::abs(get(processor, "curve") - 2.0f) < 0.01f && get(processor, "fxEnabled") < 0.5f, "Classic Pump is not the original default patch");
    processor.setCurrentProgram(8);
    require(get(processor, "fxEnabled") > 0.5f && get(processor, "destroy") > 70.0f, "Destroy Pump did not load");
    processor.undoEdit();
    require(get(processor, "fxEnabled") < 0.5f, "Loading a preset is not a single undo step");

    auto* mix = processor.parameters.getParameter("mix");
    const float mixStart = mix->getValue();
    mix->beginChangeGesture();
    mix->setValueNotifyingHost(mix->convertTo0to1(30));
    mix->setValueNotifyingHost(mix->convertTo0to1(45));
    mix->setValueNotifyingHost(mix->convertTo0to1(62));
    mix->endChangeGesture();
    require(std::abs(get(processor, "mix") - 62.0f) < 0.2f, "Parameter gesture did not apply");
    processor.undoEdit();
    require(std::abs(mix->getValue() - mixStart) < 1.0e-4f, "Undo did not group the gesture into one step");
    processor.redoEdit();
    require(std::abs(get(processor, "mix") - 62.0f) < 0.2f, "Redo did not restore the gesture");
    require(editor.keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)), "Undo shortcut was ignored");
    require(std::abs(mix->getValue() - mixStart) < 1.0e-4f, "Cmd/Ctrl+Z did not undo");
    require(editor.keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)), "Redo shortcut was ignored");
    require(std::abs(get(processor, "mix") - 62.0f) < 0.2f, "Shift+Cmd/Ctrl+Z did not redo");
    control<juce::Button>(editor, "undo").onClick();
    require(std::abs(mix->getValue() - mixStart) < 1.0e-4f, "Undo button did not undo");
    control<juce::Button>(editor, "redo").onClick();
    require(std::abs(get(processor, "mix") - 62.0f) < 0.2f, "Redo button did not redo");

    set(processor, "mix", 17);
    processor.copyAToB();
    set(processor, "mix", 73);
    processor.selectSlotB();
    require(!processor.slotIsA() && std::abs(get(processor, "mix") - 17.0f) < 0.2f, "Slot B did not recall its copy");
    processor.selectSlotA();
    require(processor.slotIsA() && std::abs(get(processor, "mix") - 73.0f) < 0.2f, "Slot A did not recall the live edit");
    processor.copyBToA();
    require(std::abs(get(processor, "mix") - 17.0f) < 0.2f, "Copy B to A did not apply");
    set(processor, "mix", 41);
    processor.copyAToB();
    set(processor, "mix", 88);
    processor.selectSlotB();
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    DuckerProcessor recalled;
    recalled.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    require(!recalled.slotIsA() && std::abs(get(recalled, "mix") - 41.0f) < 0.2f, "Session recall lost slot B");
    recalled.selectSlotA();
    require(recalled.slotIsA() && std::abs(get(recalled, "mix") - 88.0f) < 0.2f, "Session recall lost slot A");

    auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openDucker-preset-test");
    directory.deleteRecursively();
    processor.presets().setDirectory(directory);
    set(processor, "mix", 33);
    juce::String error;
    require(processor.savePreset("My Pump", error), "Could not save a user preset");
    require(!processor.savePreset("Classic Pump", error), "A factory preset name was overwritten");
    set(processor, "mix", 5);
    require(processor.loadPreset("user:My Pump"), "User preset did not load");
    require(std::abs(get(processor, "mix") - 33.0f) < 0.2f, "User preset did not restore its values");
    require(processor.renamePreset("user:My Pump", "Renamed Pump", error), "Could not rename a user preset");
    require(processor.presets().find("user:Renamed Pump").has_value(), "Renamed preset is missing");
    require(!processor.renamePreset("factory:Classic Pump", "Nope", error), "Factory preset was renamed");
    require(!processor.deletePreset("factory:Trance Gate", error), "Factory preset was deleted");
    processor.setPresetFavorite("factory:Trance Gate", true);
    const auto matches = processor.presets().filtered("gate");
    require(matches.size() >= 2, "Preset search missed a gate patch");
    bool favoriteFound = false;
    for(const auto& entry : matches) if(entry.name == "Trance Gate") favoriteFound = entry.favorite;
    require(favoriteFound, "Favorite flag did not survive search");
    DuckerProcessor fromDisk;
    fromDisk.presets().setDirectory(directory);
    require(fromDisk.presets().find("user:Renamed Pump").has_value(), "User preset was not written to disk");
    require(fromDisk.presets().isFavorite("factory:Trance Gate"), "Favorites were not written to disk");
    require(processor.deletePreset("user:Renamed Pump", error), "Could not delete a user preset");
    require(!processor.presets().find("user:Renamed Pump").has_value(), "Deleted preset is still listed");
    directory.deleteRecursively();
    std::cout << "PASS beat-locked waveform, factory programs, undo, A/B and user presets\n";
}
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        testSync<float>(); testSync<double>(); testTriggers<float>(); testTriggers<double>(); testLookahead<float>(); testLookahead<double>(); testDetectorFilters<float>(); testDetectorFilters<double>(); testBand<float>(); testBand<double>();
        testStandaloneSustain<float>(); testStandaloneSustain<double>(); testNewFxModes();
        testFxMatrix<float>(); testFxMatrix<double>();
        testEffectsRouting<float>(); testEffectsRouting<double>();
        testMonoAndDisabledEffects<float>(); testMonoAndDisabledEffects<double>();
        testBeatLockedWaveform();
        testParameterText();
        const bool floatReset = testHostReset<float>("float"), doubleReset = testHostReset<double>("double");
        require(floatReset && doubleReset, "Host reset leaves an audible effects tail");

        const juce::File output = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]) : juce::File::getCurrentWorkingDirectory();
        output.createDirectory();
        Host host; host.position.setIsPlaying(true); host.position.setBpm(128); host.position.setPpqPosition(0);
        DuckerProcessor p; p.setPlayHead(&host); p.prepareToPlay(sampleRate, 512);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        editor->setSize(1000, 804);
        juce::AudioBuffer<float> clockAudio(2, 256);
        juce::MidiBuffer clockMidi;
        clockAudio.clear();
        host.position.setIsPlaying(true);
        host.position.setBpm(140.0);
        host.position.setPpqPosition(-1.75);
        p.processBlock(clockAudio, clockMidi);
        require(std::abs(p.artworkPhase() - 0.25f) < 0.0001f, "SVG follows a negative host position");
        const auto onbeat = editor->createComponentSnapshot(juce::Rectangle<int>(52, 137, 51, 88));
        host.position.setPpqPosition(22.875);
        p.processBlock(clockAudio, clockMidi);
        require(std::abs(p.artworkPhase() - 0.875f) < 0.0001f, "SVG follows a host seek");
        const auto offbeat = editor->createComponentSnapshot(juce::Rectangle<int>(52, 137, 51, 88));
        int changed = 0;
        for (int y = 0; y < onbeat.getHeight(); ++y)
            for (int x = 0; x < onbeat.getWidth(); ++x)
                if (onbeat.getPixelAt(x, y) != offbeat.getPixelAt(x, y)) ++changed;
        require(changed > 30, "Native SVG pixels move with beat phase");
        host.position.setIsPlaying(false);
        host.position.setPpqPosition(0);
        p.processBlock(clockAudio, clockMidi);
        require(std::abs(p.artworkPhase() - 0.875f) < 0.0001f, "SVG holds phase when the host stops");
        p.setPlayHead(nullptr);
        p.processBlock(clockAudio, clockMidi);
        p.processBlock(clockAudio, clockMidi);
        require(p.artworkPhase() > 0.875f, "SVG advances on audio time without host transport");
        p.setPlayHead(&host);
        host.position.setIsPlaying(true);
        host.position.setBpm(128);
        host.position.setPpqPosition(0);
        checkButtonLayout(*editor, 40);
        editor->setSize(800, 643);
        checkButtonLayout(*editor, 32);
        editor->setSize(1000, 804);
        testControls(p, *editor);
        testEffectsControls(p, *editor);
        testEqDetents(p, *editor);
        testSuite(p, *editor);
        testEditorRecall();
        {
            // README and OpenVST Manager show this capture: the editor at the size it opens,
            // on the first factory patch, not the Sync demo configured below.
            Host previewHost;
            previewHost.position.setIsPlaying(true);
            previewHost.position.setBpm(128);
            previewHost.position.setPpqPosition(0);
            DuckerProcessor previewProcessor;
            previewProcessor.setPlayHead(&previewHost);
            previewProcessor.prepareToPlay(sampleRate, 512);
            require(previewProcessor.getCurrentProgram() == 0 && previewProcessor.getProgramName(0) == "Classic Pump", "Factory default patch is not Classic Pump");
            require(std::abs(get(previewProcessor, "mix") - 100.0f) < 0.2f && get(previewProcessor, "fxEnabled") < 0.5f, "Factory default patch was modified");
            std::unique_ptr<juce::AudioProcessorEditor> previewEditor(previewProcessor.createEditor());
            previewProcessor.reset();
            render<float>(previewProcessor, previewHost, 0, static_cast<long>(beatSamples) * 2, false);
            save(*previewEditor, output, "preview.png");
        }
        for(auto* parameter : p.getParameters()) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        for(const auto& [id, value] : std::initializer_list<std::pair<const char*, float>> {{"rate", 1}, {"trigger", 0}, {"curve", 2}, {"mix", 79},
            {"shift", 0}, {"threshold", -24}, {"retrigger", 1}, {"bandFrequency", 150}, {"smoothing", 1}, {"length", 100}, {"fxEnabled", 1}}) set(p, id, value);
        p.reset(); render<float>(p, host, 0, 22500 * 2, false);
        save(*editor, output, "openDucker-sync.png");
        const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        require(yellowPixels(image, {357, 99, 631, 295}) > 600, "The display does not draw the curve");
        require(yellowPixels(image, {357, 402, 631, 86}) > 100, "The selected curve button is not highlighted");
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
