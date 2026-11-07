#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
constexpr std::array<const char*, 13> ids {"mix", "curve", "rate", "length", "shift", "trigger", "retrigger", "threshold",
    "smoothing", "bandOn", "bandFrequency", "bandSolo", "bypass"};
}

juce::AudioProcessorValueTreeState::ParameterLayout DuckerProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    const auto number = [&](const char* id, const char* name, juce::NormalisableRange<float> range, float initial, const char* unit) {
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {id, 1}, name, range, initial,
            juce::AudioParameterFloatAttributes {}.withLabel(unit)));
    };
    const auto toggle = [&](const char* id, const char* name, bool initial) {
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID {id, 1}, name, initial));
    };
    const auto choice = [&](const char* id, const char* name, juce::StringArray choices, int initial) {
        result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID {id, 1}, name, choices, initial));
    };
    juce::StringArray curves;
    for(const auto* name : duck::curveNames) curves.add(name);
    number("mix", "Mix", {0, 100, 0.1f}, 100, "%");
    choice("curve", "Curve", curves, duck::defaultCurve);
    choice("rate", "Length", {"1/8", "1/4", "1/2", "1/1"}, 1);
    number("length", "Slope", {5, 100, 0.1f}, 100, "%");
    number("shift", "Shift", {0, 100, 0.1f}, 0, "%");
    choice("trigger", "Trigger", {"Sync", "MIDI", "Audio"}, 0);
    toggle("retrigger", "Retrigger", true);
    number("threshold", "Audio threshold", {-60, 0, 0.1f}, -24, "dB");
    choice("smoothing", "Smoothing", {"Off", "Low", "Medium", "High"}, 1);
    toggle("bandOn", "Band", false);
    number("bandFrequency", "Band frequency", {30, 1000, 1, 0.4f}, 150, "Hz");
    toggle("bandSolo", "Band solo", false);
    toggle("bypass", "Bypass", false);
    return result;
}

DuckerProcessor::DuckerProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)
          .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      parameters(*this, nullptr, "openDucker", layout()) {
    for(size_t i = 0; i < ids.size(); ++i) values[i] = parameters.getRawParameterValue(ids[i]);
    for(auto& bin : inputBins) bin.store(0);
    for(auto& bin : outputBins) bin.store(0);
}

void DuckerProcessor::prepareToPlay(double rate, int) {
    sampleRate = rate;
    engine.prepare(rate); crossover.prepare(rate); detector.prepare(rate);
    setLatencySamples(0);
    bandFrequency = -1; lastBin = -1; binInput = binOutput = 0;
    for(auto* smoothed : {&mixAmount, &bandBlend, &soloBlend, &activeBlend}) smoothed->reset(rate, 0.02);
    mixAmount.setCurrentAndTargetValue(value(Mix) * 0.01);
    bandBlend.setCurrentAndTargetValue(value(BandOn) > 0.5f ? 1.0 : 0.0);
    soloBlend.setCurrentAndTargetValue(value(BandOn) > 0.5f && value(BandSolo) > 0.5f ? 1.0 : 0.0);
    activeBlend.setCurrentAndTargetValue(value(Bypass) > 0.5f ? 0.0 : 1.0);
}

void DuckerProcessor::reset() {
    lastBin = -1; binInput = binOutput = 0;
    for(auto& bin : inputBins) bin.store(0, std::memory_order_relaxed);
    for(auto& bin : outputBins) bin.store(0, std::memory_order_relaxed);
}

bool DuckerProcessor::isBusesLayoutSupported(const BusesLayout& buses) const {
    const auto isMonoOrStereo = [](const juce::AudioChannelSet& set) {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
    };
    const auto sidechain = buses.getChannelSet(true, 1);
    return buses.getMainInputChannelSet() == buses.getMainOutputChannelSet() && isMonoOrStereo(buses.getMainOutputChannelSet())
        && (sidechain.isDisabled() || isMonoOrStereo(sidechain));
}

template<typename Sample> void DuckerProcessor::process(juce::AudioBuffer<Sample>& buffer, juce::MidiBuffer& midi, bool bypassed) {
    juce::ScopedNoDenormals noDenormals;
    duck::Transport t;
    if(auto* playHead = getPlayHead()) if(const auto position = playHead->getPosition()) {
        t.playing = position->getIsPlaying();
        if(const auto ppq = position->getPpqPosition()) if(const auto bpm = position->getBpm()) {
            t.ppq = *ppq; t.bpm = *bpm;
            t.valid = std::isfinite(t.ppq) && std::abs(t.ppq) < 1e12 && std::isfinite(t.bpm) && t.bpm > 0 && t.bpm <= 10000;
        }
    }
    const auto trigger = static_cast<duck::Trigger>(juce::jlimit(0, 2, static_cast<int>(value(TriggerMode))));
    engine.beginBlock(t, {static_cast<int>(value(Curve)), static_cast<int>(value(Rate)), static_cast<int>(value(Smoothing)),
        value(Length) * 0.01, value(Shift) * 0.01, trigger, value(Retrigger) > 0.5f});
    detector.setThresholdDb(value(Threshold));
    if(std::abs(value(BandFrequency) - bandFrequency) > 0.001) { bandFrequency = value(BandFrequency); crossover.setFrequency(bandFrequency); }
    const bool inactive = bypassed || value(Bypass) > 0.5f;
    const bool band = value(BandOn) > 0.5f;
    mixAmount.setTargetValue(value(Mix) * 0.01);
    bandBlend.setTargetValue(band ? 1.0 : 0.0);
    soloBlend.setTargetValue(band && value(BandSolo) > 0.5f ? 1.0 : 0.0);
    activeBlend.setTargetValue(inactive ? 0.0 : 1.0);

    auto main = getBusBuffer(buffer, true, 0);
    const bool hasSidechain = getBusCount(true) > 1 && getBus(true, 1)->isEnabled();
    auto sidechain = hasSidechain ? getBusBuffer(buffer, true, 1) : juce::AudioBuffer<Sample>();
    const int channels = main.getNumChannels(), samples = main.getNumSamples();
    const auto events = midi.begin(), endOfEvents = midi.end();
    auto nextEvent = events;
    const auto flushBin = [&] {
        if(lastBin < 0) return;
        inputBins[static_cast<size_t>(lastBin)].store(binInput, std::memory_order_relaxed);
        outputBins[static_cast<size_t>(lastBin)].store(binOutput, std::memory_order_relaxed);
    };
    double gain = 1;
    for(int sample = 0; sample < samples; ++sample) {
        if(trigger == duck::Trigger::Midi) {
            for(; nextEvent != endOfEvents && (*nextEvent).samplePosition <= sample; ++nextEvent)
                if((*nextEvent).getMessage().isNoteOn()) engine.trigger();
        }
        if(trigger == duck::Trigger::Audio) {
            double level = 0;
            const auto& source = hasSidechain ? sidechain : main;
            for(int channel = 0; channel < source.getNumChannels(); ++channel)
                level = std::max(level, std::abs(static_cast<double>(source.getSample(channel, sample))));
            if(detector.process(level)) engine.trigger();
        }
        gain = engine.nextGain();
        const double ducked = 1 + (gain - 1) * mixAmount.getNextValue();
        const double bandAmount = bandBlend.getNextValue(), solo = soloBlend.getNextValue(), active = activeBlend.getNextValue();
        float inputPeak = 0, outputPeak = 0;
        for(int channel = 0; channel < channels; ++channel) {
            const double input = main.getSample(channel, sample);
            const auto bands = crossover.split(static_cast<size_t>(channel), input);
            const double selected = input + (bands.low - input) * bandAmount;
            const double rest = input + (bands.low + bands.high - input) * bandAmount;
            double output = rest + selected * (ducked - 1);
            output += (selected * ducked - output) * solo;
            output = input + (output - input) * active;
            main.setSample(channel, sample, static_cast<Sample>(output));
            inputPeak = std::max(inputPeak, static_cast<float>(std::abs(input)));
            outputPeak = std::max(outputPeak, static_cast<float>(std::abs(output)));
        }
        if(!engine.running()) continue;
        const int bin = juce::jlimit(0, displayBins - 1, static_cast<int>(engine.phase() * displayBins));
        if(bin != lastBin) { flushBin(); lastBin = bin; binInput = inputPeak; binOutput = outputPeak; continue; }
        binInput = std::max(binInput, inputPeak); binOutput = std::max(binOutput, outputPeak);
    }
    flushBin();
    visualPhase.store(static_cast<float>(engine.phase()), std::memory_order_relaxed);
    visualGain.store(static_cast<float>(gain), std::memory_order_relaxed);
    visualRunning.store(engine.running(), std::memory_order_relaxed);
    hostBpm.store(t.valid ? static_cast<float>(t.bpm) : 0, std::memory_order_relaxed);
    syncState.store(t.valid ? (t.playing ? 2 : 1) : 0, std::memory_order_relaxed);
}

void DuckerProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { process(b, m, false); }
void DuckerProcessor::processBlock(juce::AudioBuffer<double>& b, juce::MidiBuffer& m) { process(b, m, false); }
void DuckerProcessor::processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { process(b, m, true); }
void DuckerProcessor::processBlockBypassed(juce::AudioBuffer<double>& b, juce::MidiBuffer& m) { process(b, m, true); }
juce::AudioProcessorParameter* DuckerProcessor::getBypassParameter() const { return parameters.getParameter("bypass"); }
juce::AudioProcessorEditor* DuckerProcessor::createEditor() { return new DuckerEditor(*this); }
void DuckerProcessor::getStateInformation(juce::MemoryBlock& data) {
    if(auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, data);
}
void DuckerProcessor::setStateInformation(const void* data, int size) {
    if(auto xml = getXmlFromBinary(data, size)) if(xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DuckerProcessor(); }
