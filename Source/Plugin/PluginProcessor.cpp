#include "PluginProcessor.h"
#include "FactoryPresets.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace {
double trimGain(float db) { return std::pow(10.0, std::round(static_cast<double>(db) * 100) / 2000); }
constexpr std::array<double, 8> delayBeats {0.125, 0.25, 0.5, 0.75, 1, 1.5, 2, 4};
constexpr std::array<const char*, 19> ids {"mix", "curve", "rate", "length", "shift", "trigger", "retrigger", "threshold",
    "smoothing", "bandOn", "bandFrequency", "bandSolo", "bypass", "detectorHpf", "detectorLpf", "detectorMonitor",
    "tension", "floor", "bandSlope"};
constexpr std::array<const char*, 22> fxIds {"fxEnabled", "duckTarget", "duckPosition", "fxMix", "delayEnabled", "delayLength",
    "delayFeedback", "delayBass", "delayTreble", "delayMode", "reverbEnabled", "reverbType",
    "reverbLength", "reverbWidth", "reverbBass", "reverbTreble", "sustain", "destroy", "output", "fxFlow", "destroyMode", "lookahead"};

// The editor, typed entry and host automation all use these texts and parsers.
struct Format {
    std::function<juce::String(float, int)> text;
    std::function<float(const juce::String&)> parse = [](const juce::String& typed) { return typed.getFloatValue(); };
};
const Format percent {[](float value, int) { return juce::String(juce::roundToInt(value)) + "%"; }};
const Format signedPercent {[](float value, int) {
    const int rounded = juce::roundToInt(value);
    return (rounded > 0 ? "+" : "") + juce::String(rounded) + "%";
}};
const Format decibels {[](float value, int) {
    const float tenths = std::round(value * 10.0f) / 10.0f;
    return (tenths > 0 ? "+" : "") + juce::String(std::abs(tenths) < 0.05f ? 0.0f : tenths, 1) + " dB";
}};
const Format hertz {[](float value, int) {
    if(juce::roundToInt(value) < 1000) return juce::String(juce::roundToInt(value)) + " Hz";
    return juce::String(value / 1000.0f, 2) + " kHz";
}, [](const juce::String& typed) { return typed.getFloatValue() * (typed.containsIgnoreCase("k") ? 1000.0f : 1.0f); }};
const Format filterHertz {[](float value, int length) {
    if(value < 1.0f) return juce::String("Off");
    return hertz.text(value, length);
}, hertz.parse};
const Format milliseconds {[](float value, int) {
    if(std::round(value * 10.0f) < 100.0f) return juce::String(value, 1) + " ms";
    if(juce::roundToInt(value) < 1000) return juce::String(juce::roundToInt(value)) + " ms";
    return juce::String(value / 1000.0f, 2) + " s";
}, [](const juce::String& typed) {
    const bool inSeconds = typed.trim().endsWithIgnoreCase("s") && !typed.trim().endsWithIgnoreCase("ms");
    return typed.getFloatValue() * (inSeconds ? 1000.0f : 1.0f);
}};
const Format lookaheadTime {[](float value, int length) {
    if(value < 0.05f) return juce::String("Off");
    return milliseconds.text(value, length);
}, milliseconds.parse};
const Format secondsTime {[](float value, int length) { return milliseconds.text(value * 1000.0f, length); },
    [](const juce::String& typed) { return typed.getFloatValue() * (typed.trim().endsWithIgnoreCase("ms") ? 0.001f : 1.0f); }};
}

juce::AudioProcessorValueTreeState::ParameterLayout DuckerProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    const auto number = [&](const char* id, const char* name, juce::NormalisableRange<float> range, float initial, const Format& format, int version = 1) {
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {id, version}, name, range, initial,
            juce::AudioParameterFloatAttributes {}.withStringFromValueFunction(format.text).withValueFromStringFunction(format.parse)));
    };
    const auto toggle = [&](const char* id, const char* name, bool initial, int version = 1) {
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID {id, version}, name, initial));
    };
    // Typed choice names ignore case.
    const auto choice = [&](const char* id, const char* name, juce::StringArray choices, int initial, int version = 1) {
        result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID {id, version}, name, choices, initial,
            juce::AudioParameterChoiceAttributes {}.withValueFromStringFunction([choices](const juce::String& typed) { return choices.indexOf(typed.trim(), true); })));
    };
    juce::StringArray curves;
    for(const auto* name : duck::curveNames) curves.add(name);
    number("mix", "Mix", {0, 100, 0.1f}, 100, percent);
    choice("curve", "Curve", curves, duck::defaultCurve);
    choice("rate", "Length", {"1/8", "1/4", "1/2", "1/1"}, 1);
    number("length", "Slope", {5, 100, 0.1f}, 100, percent);
    number("shift", "Shift", {0, 100, 0.1f}, 0, percent);
    choice("trigger", "Trigger", {"Sync", "MIDI", "Audio"}, 0);
    toggle("retrigger", "Retrigger", true);
    number("threshold", "Audio threshold", {-60, 0, 0.1f}, -24, decibels);
    choice("smoothing", "Smoothing", {"Off", "Low", "Medium", "High"}, 1);
    toggle("bandOn", "Band", false);
    number("bandFrequency", "Band frequency", {30, 1000, 1, 0.4f}, 150, hertz);
    toggle("bandSolo", "Band solo", false);
    toggle("bypass", "Bypass", false);
    number("detectorHpf", "Detector high-pass", {0, 20000, 1, 0.4f}, 0, filterHertz, 2);
    number("detectorLpf", "Detector low-pass", {0, 20000, 1, 0.4f}, 0, filterHertz, 2);
    toggle("detectorMonitor", "Detector monitor", false, 2);

    toggle("fxEnabled", "Effects enabled", false, 2);
    choice("duckTarget", "Duck", {"Dry", "Wet", "Both"}, 2, 2);
    choice("duckPosition", "Ducking position", {"Before FX", "After FX"}, 1, 2);
    number("fxMix", "FX mix", {0, 100, 0.01f}, 30, percent, 2);
    toggle("delayEnabled", "Delay enabled", true, 2);
    choice("delayLength", "Delay length", {"1/32", "1/16", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2", "1 bar"}, 3, 2);
    number("delayFeedback", "Delay feedback", {0, 95, 0.01f}, 35, percent, 2);
    number("delayBass", "Delay bass", {-12, 12, 0.01f}, 0, decibels, 2);
    number("delayTreble", "Delay treble", {-12, 12, 0.01f}, 0, decibels, 2);
    choice("delayMode", "Delay mode", {"Mono", "Ping-pong"}, 1, 2);
    toggle("reverbEnabled", "Reverb enabled", true, 2);
    choice("reverbType", "Reverb type", {"Room", "Hall", "Plate"}, 1, 2);
    number("reverbLength", "Reverb length", {0.2f, 25, 0.01f, 0.45f}, 6, secondsTime, 2);
    number("reverbWidth", "Reverb width", {0, 100, 0.01f}, 100, percent, 2);
    number("reverbBass", "Reverb bass", {-12, 12, 0.01f}, 0, decibels, 2);
    number("reverbTreble", "Reverb treble", {-12, 12, 0.01f}, 0, decibels, 2);
    number("sustain", "Sustain", {0, 100, 0.01f}, 0, percent, 2);
    number("destroy", "Destroy", {0, 100, 0.01f}, 0, percent, 2);
    number("output", "Output", {-24, 12, 0.01f}, 0, decibels, 2);
    choice("fxFlow", "FX flow", {"Serial", "Parallel"}, 1, 2);
    choice("destroyMode", "Destroy mode", {"Tail", "Always"}, 1, 2);
    number("tension", "Curve tension", {-100, 100, 0.5f}, 0, signedPercent, 3);
    number("floor", "Duck floor", {0, 100, 0.5f}, 0, percent, 3);
    choice("bandSlope", "Band slope", {"12 dB/oct", "24 dB/oct", "48 dB/oct"}, 1, 3);
    number("lookahead", "Lookahead", {0, 5, 0.1f}, 0, lookaheadTime, 4);
    return result;
}

DuckerProcessor::DuckerProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)
          .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      parameters(*this, nullptr, "openDucker", layout()),
      parameterUndo(*this, parameters),
      presetLibrary(parameters, "openDucker"),
      abCompare(parameters, presetLibrary) {
    for(size_t i = 0; i < ids.size(); ++i) values[i] = parameters.getRawParameterValue(ids[i]);
    for(size_t i = 0; i < fxIds.size(); ++i) fxParameters[i] = parameters.getRawParameterValue(fxIds[i]);
    for(auto& bin : inputBins) bin.store(0);
    for(auto& bin : outputBins) bin.store(0);
    for(auto& bin : sourceBins) bin.store(0);
    presetLibrary.setFactory(duck::makeFactoryPresets(parameters));
    abCompare.resetBothFromCurrent();
    parameterUndo.onEdit = [this] { presetLibrary.markEdited(); };
}

void DuckerProcessor::prepareToPlay(double rate, int) {
    sampleRate = rate;
    artworkClock.reset();
    engine.prepare(rate); crossover.prepare(rate); detector.prepare(rate); effects.prepare(rate);
    for(auto& filter : detectorFilters) { filter.prepare(rate); filter.reset(); }
    for(auto& line : lookaheadLines) line.assign(static_cast<size_t>(rate * 0.005) + 2, 0.0);
    lookaheadPosition = 0;
    lookaheadSamples = juce::jlimit(0, static_cast<int>(lookaheadLines[0].size()) - 1,
        static_cast<int>(std::round(fx(Lookahead) * 0.001 * rate)));
    setLatencySamples(lookaheadSamples);
    effectsRunning = false;
    bandFrequency = -1; lastBin = -1; sourceLastBin = -1; binInput = binOutput = sourceHold = 0;
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
    for(auto& line : lookaheadLines) std::fill(line.begin(), line.end(), 0.0);
    lookaheadPosition = 0;
    crossover.reset();
    for(auto& filter : detectorFilters) filter.reset();
    triggerDemo.store(false);
    demoImpactSamplesLeft = 0;
    demoPhase = 0;
    demoFilterL = demoFilterR = 0;
    outputPeak.store(0, std::memory_order_relaxed);
    destroyLevel.store(0, std::memory_order_relaxed);
    lastBin = -1; sourceLastBin = -1; binInput = binOutput = sourceHold = 0;
    for(auto& bin : inputBins) bin.store(0, std::memory_order_relaxed);
    for(auto& bin : outputBins) bin.store(0, std::memory_order_relaxed);
    for(auto& bin : sourceBins) bin.store(0, std::memory_order_relaxed);
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
    auto* playHead = getPlayHead();
    const auto position = playHead ? playHead->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo>{};
    artworkClock.update(position, buffer.getNumSamples(), sampleRate);
    if (position) {
        t.playing = position->getIsPlaying();
        if(const auto ppq = position->getPpqPosition()) if(const auto bpm = position->getBpm()) {
            t.ppq = *ppq; t.bpm = *bpm;
            t.valid = std::isfinite(t.ppq) && std::abs(t.ppq) < 1e12 && std::isfinite(t.bpm) && t.bpm > 0 && t.bpm <= 10000;
        }
    }
    const auto trigger = static_cast<duck::Trigger>(juce::jlimit(0, 2, static_cast<int>(value(TriggerMode))));
    engine.beginBlock(t, {static_cast<int>(value(Curve)), static_cast<int>(value(Rate)), static_cast<int>(value(Smoothing)),
        value(Length) * 0.01, value(Shift) * 0.01, trigger, value(Retrigger) > 0.5f, value(Tension) * 0.01, value(Floor) * 0.01});
    detector.setThresholdDb(value(Threshold));
    crossover.setSlope(static_cast<int>(value(BandSlope)));
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
    const int requestedLookahead = lookaheadLines[0].empty() ? 0 : juce::jlimit(0, static_cast<int>(lookaheadLines[0].size()) - 1,
        static_cast<int>(std::round(fx(Lookahead) * 0.001 * sampleRate)));
    if(requestedLookahead != lookaheadSamples) {
        lookaheadSamples = requestedLookahead;
        setLatencySamples(lookaheadSamples);
        for(auto& line : lookaheadLines) std::fill(line.begin(), line.end(), 0.0);
        lookaheadPosition = 0;
    }

    auto main = getBusBuffer(buffer, true, 0);
    const bool hasSidechain = getBusCount(true) > 1 && getBus(true, 1)->isEnabled();
    auto sidechain = hasSidechain ? getBusBuffer(buffer, true, 1) : juce::AudioBuffer<Sample>();
    sourceIsSidechain.store(hasSidechain, std::memory_order_relaxed);
    const int rateIndex = juce::jlimit(0, 3, static_cast<int>(value(Rate)));
    if(rateIndex != sourceRate) {
        sourceRate = rateIndex;
        for(auto& bin : sourceBins) bin.store(0.0f, std::memory_order_relaxed);
        sourceLastBin = -1; sourceHold = 0;
    }
    const juce::AudioBuffer<Sample>* sourceBuffer = hasSidechain ? &sidechain : &main;
    const auto sourcePeak = [&](int index) {
        if(sourceBuffer->getNumChannels() <= 0) return 0.0f;
        const float left = std::abs(static_cast<float>(sourceBuffer->getSample(0, index)));
        const float right = sourceBuffer->getNumChannels() > 1 ? std::abs(static_cast<float>(sourceBuffer->getSample(1, index))) : left;
        return std::max(left, right);
    };
    const auto flushSource = [&] {
        if(sourceLastBin < 0) return;
        sourceBins[static_cast<size_t>(sourceLastBin)].store(sourceHold, std::memory_order_relaxed);
    };
    const int channels = main.getNumChannels(), samples = main.getNumSamples();
    const bool stereo = channels > 1;
    const double detectorHpf = value(DetectorHpf), detectorLpf = value(DetectorLpf);
    for(auto& filter : detectorFilters) filter.setCutoffs(detectorHpf, detectorLpf);
    const bool detectorMonitor = value(DetectorMonitor) > 0.5f;
    const auto events = midi.begin(), endOfEvents = midi.end();
    auto nextEvent = events;
    if(triggerDemo.exchange(false)) {
        demoImpactSamplesLeft = static_cast<int>(sampleRate * 2.5);
        demoPhase = 0;
    }
    const auto flushBin = [&] {
        if(lastBin < 0) return;
        inputBins[static_cast<size_t>(lastBin)].store(binInput, std::memory_order_relaxed);
        outputBins[static_cast<size_t>(lastBin)].store(binOutput, std::memory_order_relaxed);
    };
    double gain = 1, blockPeak = 0, detectorBlockPeak = 0;
    for(int sample = 0; sample < samples; ++sample) {
        double demoL = 0, demoR = 0;
        if(demoImpactSamplesLeft > 0) {
            constexpr double twoPi = 6.28318530717958647692;
            const double totalDuration = sampleRate * 2.5;
            const double timeSec = (totalDuration - static_cast<double>(demoImpactSamplesLeft)) / sampleRate;
            const double thumpDecay = std::exp(-timeSec / 0.12);
            const double thumpFreq = 45.0 + 85.0 * std::exp(-timeSec / 0.04);
            demoPhase += (twoPi * thumpFreq) / sampleRate;
            const double thump = std::sin(demoPhase) * thumpDecay * 0.75;
            const double click = timeSec < 0.003 ? (1.0 - timeSec / 0.003) * 0.5 : 0.0;
            const double crashEnv = std::exp(-timeSec / 0.55);
            auto nextRnd = [this]() {
                demoSeed ^= demoSeed << 13;
                demoSeed ^= demoSeed >> 17;
                demoSeed ^= demoSeed << 5;
                return (static_cast<double>(demoSeed) / 2147483648.0) - 1.0;
            };
            const double rawL = nextRnd();
            const double rawR = nextRnd();
            demoFilterL += static_cast<float>(0.35 * (rawL - static_cast<double>(demoFilterL)));
            demoFilterR += static_cast<float>(0.35 * (rawR - static_cast<double>(demoFilterR)));
            const double crashL = static_cast<double>(demoFilterL) * crashEnv * 0.45;
            const double crashR = static_cast<double>(demoFilterR) * crashEnv * 0.45;
            demoL = thump + click + crashL;
            demoR = thump + click + crashR;
            --demoImpactSamplesLeft;
        }

        bool triggered = false;
        if(trigger == duck::Trigger::Midi) {
            for(; nextEvent != endOfEvents && (*nextEvent).samplePosition <= sample; ++nextEvent)
                if((*nextEvent).getMessage().isNoteOn() && engine.trigger()) triggered = true;
        }
        const bool needDetector = trigger == duck::Trigger::Audio || detectorMonitor;
        double detectorLeft = 0.0, detectorRight = 0.0;
        if(needDetector) {
            const auto& detectorSource = hasSidechain ? sidechain : main;
            const double sourceLeft = detectorSource.getNumChannels() > 0 ? static_cast<double>(detectorSource.getSample(0, sample)) : 0.0;
            const double sourceRight = detectorSource.getNumChannels() > 1 ? static_cast<double>(detectorSource.getSample(1, sample)) : sourceLeft;
            detectorLeft = detectorFilters[0].process(sourceLeft);
            detectorRight = detectorFilters[1].process(sourceRight);
            if(trigger == duck::Trigger::Audio) {
                const double level = std::max({std::abs(demoL), std::abs(demoR), std::abs(detectorLeft), std::abs(detectorRight)});
                detectorBlockPeak = std::max(detectorBlockPeak, level);
                if(detector.process(level) && engine.trigger()) triggered = true;
            }
        }
        const float incoming = sourcePeak(sample);
        gain = engine.nextGain();
        if(triggered) {
            triggerCount.fetch_add(1, std::memory_order_relaxed);
            triggerBeat.store(static_cast<float>(engine.beatPhase()), std::memory_order_relaxed);
        }
        const int beatBin = juce::jlimit(0, displayBins - 1, static_cast<int>(engine.beatPhase() * displayBins));
        if(beatBin != sourceLastBin) { flushSource(); sourceLastBin = beatBin; sourceHold = incoming; }
        else sourceHold = std::max(sourceHold, incoming);
        const double ducked = 1 + (gain - 1) * mixAmount.getNextValue();
        const double bandAmount = bandBlend.getNextValue(), solo = soloBlend.getNextValue(), active = activeBlend.getNextValue();
        const double dryMask = dryDuck.getNextValue(), preMask = wetPreDuck.getNextValue(), postMask = wetPostDuck.getNextValue();
        const double blend = fxBlend.getNextValue(), mix = wetMix.getNextValue() * blend, trim = outputGain.getNextValue();
        const double rawLeft = static_cast<double>(main.getSample(0, sample));
        const double rawRight = static_cast<double>(main.getSample(stereo ? 1 : 0, sample));
        double delayedLeft = rawLeft, delayedRight = rawRight;
        if(lookaheadSamples > 0 && !lookaheadLines[0].empty()) {
            // The detector and engine above read the undelayed source, so the
            // duck reaches its floor while the transient is still in this line.
            lookaheadLines[0][lookaheadPosition] = rawLeft;
            lookaheadLines[1][lookaheadPosition] = rawRight;
            const size_t readPosition = (lookaheadPosition + lookaheadLines[0].size() - static_cast<size_t>(lookaheadSamples)) % lookaheadLines[0].size();
            delayedLeft = lookaheadLines[0][readPosition];
            delayedRight = lookaheadLines[1][readPosition];
            lookaheadPosition = (lookaheadPosition + 1) % lookaheadLines[0].size();
        }
        const duck::Stereo input {delayedLeft + demoL, delayedRight + demoR};
        const bool useCrossover = bandAmount > 0.0;
        const duck::Bands bandsL = useCrossover ? crossover.split(0, input.left) : duck::Bands{input.left, 0.0};
        const duck::Bands bandsR = useCrossover ? (stereo ? crossover.split(1, input.right) : bandsL) : duck::Bands{input.right, 0.0};
        const auto shapeChannel = [&](duck::Bands bands, double mask) {
            const double branchGain = 1.0 + (ducked - 1.0) * mask;
            const double highGain = (branchGain + (1.0 - branchGain) * bandAmount) * (1.0 - solo);
            return bands.low * branchGain + bands.high * highGain;
        };
        const double dryLeft = shapeChannel(bandsL, dryMask);
        const double dryRight = shapeChannel(bandsR, dryMask);
        duck::Stereo wet;
        if(blend <= 0 && effectsRunning) {
            effects.reset();
            effectsRunning = false;
        }
        if(blend > 0) {
            wet = effects.process({shapeChannel(bandsL, preMask), shapeChannel(bandsR, preMask)},
                std::max(std::abs(input.left), std::abs(input.right)));
            effectsRunning = true;
        }
        const double postGain = 1 + (ducked - 1) * postMask;
        double left = ((1 - mix) * dryLeft + mix * wet.left * postGain) * trim;
        double right = ((1 - mix) * dryRight + mix * wet.right * postGain) * trim;
        left = input.left + (left - input.left) * active;
        right = input.right + (right - input.right) * active;
        if(detectorMonitor) { left = detectorLeft; right = detectorRight; }
        const double monoOut = (left + right) * 0.5;
        main.setSample(0, sample, static_cast<Sample>(stereo ? left : monoOut));
        if(stereo) main.setSample(1, sample, static_cast<Sample>(right));
        const double sampleOutPeak = stereo ? std::max(std::abs(left), std::abs(right)) : std::abs(monoOut);
        blockPeak = std::max(blockPeak, sampleOutPeak);
        const float inputPeak = static_cast<float>(stereo ? std::max(std::abs(input.left), std::abs(input.right)) : std::abs(input.left));
        const float outPeak = static_cast<float>(sampleOutPeak);
        if(!engine.running()) continue;
        const int bin = juce::jlimit(0, displayBins - 1, static_cast<int>(engine.beatPhase() * displayBins));
        if(bin != lastBin) { flushBin(); lastBin = bin; binInput = inputPeak; binOutput = outPeak; continue; }
        binInput = std::max(binInput, inputPeak); binOutput = std::max(binOutput, outPeak);
    }
    flushBin();
    flushSource();
    detectorPeak.store(std::max(detectorPeak.load(std::memory_order_relaxed), static_cast<float>(detectorBlockPeak)), std::memory_order_relaxed);
    outputPeak.store(static_cast<float>(std::max(blockPeak, outputPeak.load() * std::exp(-samples / (sampleRate * 0.25)))), std::memory_order_relaxed);
    destroyLevel.store(effectsRunning ? static_cast<float>(effects.destroyActivity()) : 0, std::memory_order_relaxed);
    visualPhase.store(static_cast<float>(engine.phase()), std::memory_order_relaxed);
    visualBeat.store(static_cast<float>(engine.beatPhase()), std::memory_order_relaxed);
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

class DuckerProcessor::SnapshotAction : public juce::UndoableAction {
public:
    SnapshotAction(DuckerProcessor& ownerIn, SessionSnapshot beforeIn, SessionSnapshot afterIn)
        : owner(ownerIn), before(std::move(beforeIn)), after(std::move(afterIn)) {}
    bool perform() override { owner.restoreSnapshot(after); return true; }
    bool undo() override { owner.restoreSnapshot(before); return true; }
    int getSizeInUnits() override { return 64; }
private:
    DuckerProcessor& owner;
    SessionSnapshot before, after;
};

DuckerProcessor::SessionSnapshot DuckerProcessor::snap() {
    return {parameters.copyState(), currentProgram, presetLibrary.activeKey(), presetLibrary.isEdited()};
}

void DuckerProcessor::restoreSnapshot(const SessionSnapshot& snapshot) {
    parameterUndo.suspend();
    parameters.replaceState(snapshot.parameters.createCopy());
    currentProgram = juce::jlimit(0, std::max(0, getNumPrograms() - 1), snapshot.program);
    presetLibrary.setActiveKey(snapshot.presetKey, snapshot.presetEdited);
    parameterUndo.resume();
    resetEffects.store(true);
}

void DuckerProcessor::pushUndo(const juce::String& name, SessionSnapshot before, SessionSnapshot after) {
    if(parameterUndo.isSuspended() || parameterUndo.isApplying() || !suite::onMessageThread()) return;
    if(before.program == after.program && before.presetKey == after.presetKey
        && before.parameters.isEquivalentTo(after.parameters)) return;
    parameterUndo.beginTransaction(name);
    parameterUndo.perform(new SnapshotAction(*this, std::move(before), std::move(after)));
}

int DuckerProcessor::getNumPrograms() { return std::max(1, presetLibrary.numFactory()); }
int DuckerProcessor::getCurrentProgram() { return currentProgram; }
void DuckerProcessor::setCurrentProgram(int index) {
    if(presetLibrary.numFactory() <= 0) return;
    loadPreset(presetLibrary.factoryKey(juce::jlimit(0, presetLibrary.numFactory() - 1, index)));
}
const juce::String DuckerProcessor::getProgramName(int index) { return presetLibrary.factoryName(index); }

void DuckerProcessor::undoEdit() { parameterUndo.undoStep(); }
void DuckerProcessor::redoEdit() { parameterUndo.redoStep(); }
bool DuckerProcessor::canUndo() const { return parameterUndo.canUndo(); }
bool DuckerProcessor::canRedo() const { return parameterUndo.canRedo(); }
bool DuckerProcessor::slotIsA() const { return abCompare.isA(); }

// Switching slots swaps undo histories. Copying into a slot clears that slot's history.
void DuckerProcessor::selectSlotA() {
    if(!abCompare.selectA()) return;
    parameterUndo.selectHistory(0);
    resetEffects.store(true);
}
void DuckerProcessor::selectSlotB() {
    if(!abCompare.selectB()) return;
    parameterUndo.selectHistory(1);
    resetEffects.store(true);
}
void DuckerProcessor::copyAToB() {
    abCompare.copyAToB();
    parameterUndo.clearHistory(1);
    if(!abCompare.isA()) resetEffects.store(true);
}
void DuckerProcessor::copyBToA() {
    abCompare.copyBToA();
    parameterUndo.clearHistory(0);
    if(abCompare.isA()) resetEffects.store(true);
}

bool DuckerProcessor::loadPreset(const juce::String& key) {
    const auto entry = presetLibrary.find(key);
    if(!entry) return false;
    const auto before = snap();
    parameterUndo.suspend();
    parameters.replaceState(entry->state.createCopy());
    if(entry->factory) currentProgram = entry->program;
    presetLibrary.setActiveKey(key);
    abCompare.captureActive();
    parameterUndo.resume();
    resetEffects.store(true);
    pushUndo("Preset", before, snap());
    return true;
}
bool DuckerProcessor::savePreset(const juce::String& name, juce::String& error) { return presetLibrary.saveUser(name, error); }
bool DuckerProcessor::renamePreset(const juce::String& key, const juce::String& name, juce::String& error) {
    return presetLibrary.renameUser(key, name, error);
}
bool DuckerProcessor::deletePreset(const juce::String& key, juce::String& error) { return presetLibrary.deleteUser(key, error); }
void DuckerProcessor::setPresetFavorite(const juce::String& key, bool favorite) { presetLibrary.setFavorite(key, favorite); }

void DuckerProcessor::getStateInformation(juce::MemoryBlock& data) {
    abCompare.captureActive();
    auto state = parameters.copyState();
    state.setProperty("program", currentProgram, nullptr);
    state.setProperty("presetKey", presetLibrary.activeKey(), nullptr);
    state.setProperty("presetEdited", presetLibrary.isEdited(), nullptr);
    state.setProperty("editorWidth", editorWidth.load(), nullptr);
    state.setProperty("effectsShown", effectsShown.load(), nullptr);
    state.addChild(abCompare.toTree(), -1, nullptr);
    if(auto xml = state.createXml()) copyXmlToBinary(*xml, data);
}
void DuckerProcessor::setStateInformation(const void* data, int size) {
    if(auto xml = getXmlFromBinary(data, size)) if(xml->hasTagName(parameters.state.getType())) {
        auto state = juce::ValueTree::fromXml(*xml);
        auto suite = state.getChildWithName("suite");
        if(suite.isValid()) state.removeChild(suite, nullptr);
        const bool hasProgram = state.hasProperty("program");
        const int program = hasProgram ? static_cast<int>(state.getProperty("program")) : currentProgram;
        const auto presetKey = state.getProperty("presetKey").toString();
        const bool presetEdited = state.getProperty("presetEdited");
        editorWidth.store(static_cast<int>(state.getProperty("editorWidth", 1000)));
        effectsShown.store(static_cast<bool>(state.getProperty("effectsShown", true)));
        state.removeProperty("program", nullptr);
        state.removeProperty("presetKey", nullptr);
        state.removeProperty("presetEdited", nullptr);
        state.removeProperty("editorWidth", nullptr);
        state.removeProperty("effectsShown", nullptr);
        for(const auto* id : fxIds) if(!state.getChildWithProperty("id", id).isValid()) {
            auto* parameter = parameters.getParameter(id);
            juce::ValueTree child {"PARAM"}; child.setProperty("id", id, nullptr);
            child.setProperty("value", juce::String(id) == "fxFlow" || juce::String(id) == "destroyMode" ? 0.0f : parameter->convertFrom0to1(parameter->getDefaultValue()), nullptr);
            state.addChild(child, -1, nullptr);
        }
        for(const auto* id : {"tension", "floor", "bandSlope"}) if(!state.getChildWithProperty("id", id).isValid()) {
            auto* parameter = parameters.getParameter(id);
            juce::ValueTree child {"PARAM"}; child.setProperty("id", id, nullptr);
            child.setProperty("value", parameter->convertFrom0to1(parameter->getDefaultValue()), nullptr);
            state.addChild(child, -1, nullptr);
        }
        parameterUndo.suspend();
        parameters.replaceState(state);
        parameterUndo.resume();
        if(presetKey.isNotEmpty()) presetLibrary.setActiveKey(presetKey, presetEdited);
        if(suite.isValid()) abCompare.readTree(suite);
        else abCompare.resetBothFromCurrent();
        if(hasProgram) currentProgram = juce::jlimit(0, std::max(0, getNumPrograms() - 1), program);
        parameterUndo.clearHistory();
        parameterUndo.selectHistory(abCompare.isA() ? 0 : 1);
        resetEffects.store(true);
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DuckerProcessor(); }
