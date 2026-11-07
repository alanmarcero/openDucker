#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
double trimGain(float db) { return std::pow(10.0, std::round(static_cast<double>(db) * 100) / 2000); }
constexpr std::array<double, 8> delayBeats {0.125, 0.25, 0.5, 0.75, 1, 1.5, 2, 4};
constexpr std::array<const char*, 13> ids {"mix", "curve", "rate", "length", "shift", "trigger", "retrigger", "threshold",
    "smoothing", "bandOn", "bandFrequency", "bandSolo", "bypass"};
constexpr std::array<const char*, 21> fxIds {"fxEnabled", "duckTarget", "duckPosition", "fxMix", "delayEnabled", "delayLength",
    "delayFeedback", "delayBass", "delayTreble", "delayMode", "reverbEnabled", "reverbType",
    "reverbLength", "reverbWidth", "reverbBass", "reverbTreble", "sustain", "destroy", "output", "fxFlow", "destroyMode"};
}

juce::AudioProcessorValueTreeState::ParameterLayout DuckerProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    const auto number = [&](const char* id, const char* name, juce::NormalisableRange<float> range, float initial, const char* unit, int version = 1) {
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {id, version}, name, range, initial,
            juce::AudioParameterFloatAttributes {}.withLabel(unit)));
    };
    const auto toggle = [&](const char* id, const char* name, bool initial, int version = 1) {
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID {id, version}, name, initial));
    };
    const auto choice = [&](const char* id, const char* name, juce::StringArray choices, int initial, int version = 1) {
        result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID {id, version}, name, choices, initial));
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

    toggle("fxEnabled", "Effects enabled", false, 2);
    choice("duckTarget", "Duck", {"Dry", "Wet", "Both"}, 2, 2);
    choice("duckPosition", "Ducking position", {"Before FX", "After FX"}, 1, 2);
    number("fxMix", "FX mix", {0, 100, 0.01f}, 30, "%", 2);
    toggle("delayEnabled", "Delay enabled", true, 2);
    choice("delayLength", "Delay length", {"1/32", "1/16", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2", "1 bar"}, 3, 2);
    number("delayFeedback", "Delay feedback", {0, 95, 0.01f}, 35, "%", 2);
    number("delayBass", "Delay bass", {-12, 12, 0.01f}, 0, "dB", 2);
    number("delayTreble", "Delay treble", {-12, 12, 0.01f}, 0, "dB", 2);
    choice("delayMode", "Delay mode", {"Mono", "Ping-pong"}, 1, 2);
    toggle("reverbEnabled", "Reverb enabled", true, 2);
    choice("reverbType", "Reverb type", {"Room", "Hall", "Plate"}, 1, 2);
    number("reverbLength", "Reverb length", {0.2f, 25, 0.01f, 0.45f}, 6, "s", 2);
    number("reverbWidth", "Reverb width", {0, 100, 0.01f}, 100, "%", 2);
    number("reverbBass", "Reverb bass", {-12, 12, 0.01f}, 0, "dB", 2);
    number("reverbTreble", "Reverb treble", {-12, 12, 0.01f}, 0, "dB", 2);
    number("sustain", "Sustain", {0, 100, 0.01f}, 0, "%", 2);
    number("destroy", "Destroy", {0, 100, 0.01f}, 0, "%", 2);
    number("output", "Output", {-24, 12, 0.01f}, 0, "dB", 2);
    choice("fxFlow", "FX flow", {"Serial", "Parallel"}, 1, 2);
    choice("destroyMode", "Destroy mode", {"Tail", "Always"}, 1, 2);
    return result;
}

DuckerProcessor::DuckerProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)
          .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      parameters(*this, nullptr, "openDucker", layout()) {
    for(size_t i = 0; i < ids.size(); ++i) values[i] = parameters.getRawParameterValue(ids[i]);
    for(size_t i = 0; i < fxIds.size(); ++i) fxParameters[i] = parameters.getRawParameterValue(fxIds[i]);
    for(auto& bin : inputBins) bin.store(0);
    for(auto& bin : outputBins) bin.store(0);
}

void DuckerProcessor::prepareToPlay(double rate, int) {
    sampleRate = rate;
    engine.prepare(rate); crossover.prepare(rate); detector.prepare(rate); effects.prepare(rate);
    setLatencySamples(0);
    effectsRunning = false;
    bandFrequency = -1; lastBin = -1; binInput = binOutput = 0;
    for(auto* smoothed : {&mixAmount, &bandBlend, &soloBlend, &dryDuck, &wetPreDuck, &wetPostDuck, &fxBlend, &wetMix, &outputGain, &activeBlend})
        smoothed->reset(rate, 0.02);
    const bool enabled = fx(FxEnabled) > 0.5f && (fx(DelayEnabled) > 0.5f || fx(ReverbEnabled) > 0.5f || fx(Sustain) > 0 || fx(Destroy) > 0);
    const int target = static_cast<int>(fx(DuckTarget));
    const bool before = fx(DuckPosition) < 0.5f;
    mixAmount.setCurrentAndTargetValue(value(Mix) * 0.01);
    bandBlend.setCurrentAndTargetValue(value(BandOn) > 0.5f ? 1.0 : 0.0);
    soloBlend.setCurrentAndTargetValue(value(BandOn) > 0.5f && value(BandSolo) > 0.5f ? 1.0 : 0.0);
    dryDuck.setCurrentAndTargetValue(!enabled || target != 1 ? 1.0 : 0.0);
    wetPreDuck.setCurrentAndTargetValue(enabled && target != 0 && before ? 1.0 : 0.0);
    wetPostDuck.setCurrentAndTargetValue(enabled && target != 0 && !before ? 1.0 : 0.0);
    fxBlend.setCurrentAndTargetValue(enabled && value(Bypass) < 0.5f ? 1.0 : 0.0);
    wetMix.setCurrentAndTargetValue(fx(FxMix) * 0.01);
    outputGain.setCurrentAndTargetValue(trimGain(fx(Output)));
    activeBlend.setCurrentAndTargetValue(value(Bypass) > 0.5f ? 0.0 : 1.0);
    outputPeak.store(0); destroyLevel.store(0);
}

void DuckerProcessor::reset() {
    resetEffects.store(true);
    outputPeak.store(0, std::memory_order_relaxed);
    destroyLevel.store(0, std::memory_order_relaxed);
    lastBin = -1; binInput = binOutput = 0;
    for(auto& bin : inputBins) bin.store(0, std::memory_order_relaxed);
    for(auto& bin : outputBins) bin.store(0, std::memory_order_relaxed);
}

double DuckerProcessor::getTailLengthSeconds() const {
    if(fx(FxEnabled) < 0.5f || value(Bypass) > 0.5f) return 0;
    const auto bpm = hostBpm.load();
    const auto length = delayBeats[static_cast<size_t>(juce::jlimit(0, 7, static_cast<int>(fx(DelayLength))))];
    const double seconds = std::min(8.0, length * 60.0 / (bpm > 0 ? bpm : 120.0));
    const double feedback = fx(DelayFeedback) * 0.01;
    const double delayTail = fx(DelayEnabled) > 0.5f ? seconds * (1 + (feedback > 0 ? std::ceil(std::log(0.00001) / std::log(feedback)) : 0)) : 0;
    const double reverbTail = fx(ReverbEnabled) > 0.5f ? fx(ReverbLength) * 2.0 : 0;
    const double destroyTail = fx(Destroy) > 0 && (delayTail > 0 || reverbTail > 0 || fx(DestroyMode) > 0.5f) ? 0.06 : 0;
    return delayTail + reverbTail + destroyTail;
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
    const bool enabled = fx(FxEnabled) > 0.5f && (fx(DelayEnabled) > 0.5f || fx(ReverbEnabled) > 0.5f || fx(Sustain) > 0 || fx(Destroy) > 0);
    const int target = static_cast<int>(fx(DuckTarget));
    const bool before = fx(DuckPosition) < 0.5f;
    mixAmount.setTargetValue(value(Mix) * 0.01);
    bandBlend.setTargetValue(band ? 1.0 : 0.0);
    soloBlend.setTargetValue(band && value(BandSolo) > 0.5f ? 1.0 : 0.0);
    dryDuck.setTargetValue(!enabled || target != 1 ? 1.0 : 0.0);
    wetPreDuck.setTargetValue(enabled && target != 0 && before ? 1.0 : 0.0);
    wetPostDuck.setTargetValue(enabled && target != 0 && !before ? 1.0 : 0.0);
    fxBlend.setTargetValue(enabled && !inactive ? 1.0 : 0.0);
    wetMix.setTargetValue(fx(FxMix) * 0.01);
    outputGain.setTargetValue(trimGain(fx(Output)));
    activeBlend.setTargetValue(inactive ? 0.0 : 1.0);

    duck::EffectsParameters settings;
    settings.delayEnabled = fx(DelayEnabled) > 0.5f;
    const double delayTempo = t.valid ? t.bpm : 120.0;
    settings.delaySeconds = std::min(8.0, delayBeats[static_cast<size_t>(juce::jlimit(0, 7, static_cast<int>(fx(DelayLength))))] * 60 / delayTempo);
    settings.feedback = fx(DelayFeedback) * 0.01; settings.pingPong = fx(DelayMode) > 0.5f;
    settings.delayBassDb = fx(DelayBass); settings.delayTrebleDb = fx(DelayTreble);
    settings.reverbEnabled = fx(ReverbEnabled) > 0.5f; settings.reverbType = static_cast<int>(fx(ReverbType));
    settings.reverbSeconds = fx(ReverbLength); settings.reverbWidth = fx(ReverbWidth) * 0.01;
    settings.reverbBassDb = fx(ReverbBass); settings.reverbTrebleDb = fx(ReverbTreble);
    settings.sustain = fx(Sustain) * 0.01; settings.destroy = fx(Destroy) * 0.01;
    settings.parallel = fx(FxFlow) > 0.5f; settings.destroyAlways = fx(DestroyMode) > 0.5f;
    if(resetEffects.exchange(false)) { effects.reset(); effectsRunning = false; outputPeak.store(0, std::memory_order_relaxed); }
    effects.setParameters(settings);

    auto main = getBusBuffer(buffer, true, 0);
    const bool hasSidechain = getBusCount(true) > 1 && getBus(true, 1)->isEnabled();
    auto sidechain = hasSidechain ? getBusBuffer(buffer, true, 1) : juce::AudioBuffer<Sample>();
    const int channels = main.getNumChannels(), samples = main.getNumSamples();
    const bool stereo = channels > 1;
    const auto events = midi.begin(), endOfEvents = midi.end();
    auto nextEvent = events;
    const auto flushBin = [&] {
        if(lastBin < 0) return;
        inputBins[static_cast<size_t>(lastBin)].store(binInput, std::memory_order_relaxed);
        outputBins[static_cast<size_t>(lastBin)].store(binOutput, std::memory_order_relaxed);
    };
    double gain = 1, blockPeak = 0;
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
        const double dryMask = dryDuck.getNextValue(), preMask = wetPreDuck.getNextValue(), postMask = wetPostDuck.getNextValue();
        const double blend = fxBlend.getNextValue(), mix = wetMix.getNextValue() * blend, trim = outputGain.getNextValue();
        const duck::Stereo input {static_cast<double>(main.getSample(0, sample)), static_cast<double>(main.getSample(stereo ? 1 : 0, sample))};
        const auto bandsL = crossover.split(0, input.left);
        const auto bandsR = stereo ? crossover.split(1, input.right) : bandsL;
        const auto shapeChannel = [&](double x, duck::Bands bands, double mask) {
            const double branchGain = 1 + (ducked - 1) * mask;
            const double selected = x + (bands.low - x) * bandAmount;
            const double rest = x + (bands.low + bands.high - x) * bandAmount;
            const double shaped = rest + selected * (branchGain - 1);
            return shaped + (selected * branchGain - shaped) * solo;
        };
        const double dryLeft = shapeChannel(input.left, bandsL, dryMask);
        const double dryRight = shapeChannel(input.right, bandsR, dryMask);
        duck::Stereo wet;
        if(blend <= 0 && effectsRunning) {
            effects.reset();
            effectsRunning = false;
        }
        if(blend > 0) {
            wet = effects.process({shapeChannel(input.left, bandsL, preMask), shapeChannel(input.right, bandsR, preMask)},
                std::max(std::abs(input.left), std::abs(input.right)));
            effectsRunning = true;
        }
        const double postGain = 1 + (ducked - 1) * postMask;
        double left = ((1 - mix) * dryLeft + mix * wet.left * postGain) * trim;
        double right = ((1 - mix) * dryRight + mix * wet.right * postGain) * trim;
        left = input.left + (left - input.left) * active;
        right = input.right + (right - input.right) * active;
        const double monoOut = (left + right) * 0.5;
        main.setSample(0, sample, static_cast<Sample>(stereo ? left : monoOut));
        if(stereo) main.setSample(1, sample, static_cast<Sample>(right));
        const double sampleOutPeak = stereo ? std::max(std::abs(left), std::abs(right)) : std::abs(monoOut);
        blockPeak = std::max(blockPeak, sampleOutPeak);
        const float inputPeak = static_cast<float>(stereo ? std::max(std::abs(input.left), std::abs(input.right)) : std::abs(input.left));
        const float outPeak = static_cast<float>(sampleOutPeak);
        if(!engine.running()) continue;
        const int bin = juce::jlimit(0, displayBins - 1, static_cast<int>(engine.phase() * displayBins));
        if(bin != lastBin) { flushBin(); lastBin = bin; binInput = inputPeak; binOutput = outPeak; continue; }
        binInput = std::max(binInput, inputPeak); binOutput = std::max(binOutput, outPeak);
    }
    flushBin();
    outputPeak.store(static_cast<float>(std::max(blockPeak, outputPeak.load() * std::exp(-samples / (sampleRate * 0.25)))), std::memory_order_relaxed);
    destroyLevel.store(effectsRunning ? static_cast<float>(effects.destroyActivity()) : 0, std::memory_order_relaxed);
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
    if(auto xml = getXmlFromBinary(data, size)) if(xml->hasTagName(parameters.state.getType())) {
        auto state = juce::ValueTree::fromXml(*xml);
        for(const auto* id : fxIds) if(!state.getChildWithProperty("id", id).isValid()) {
            auto* parameter = parameters.getParameter(id);
            juce::ValueTree child {"PARAM"}; child.setProperty("id", id, nullptr);
            child.setProperty("value", juce::String(id) == "fxFlow" || juce::String(id) == "destroyMode" ? 0.0f : parameter->convertFrom0to1(parameter->getDefaultValue()), nullptr);
            state.addChild(child, -1, nullptr);
        }
        parameters.replaceState(state);
        resetEffects.store(true);
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DuckerProcessor(); }
