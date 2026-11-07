#include "Ducker.h"
#include "Crossover.h"
#include "TransientDetector.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) { if(!ok) throw std::runtime_error(message); }
duck::Transport playing(double ppq, double bpm = 120) { return {ppq, bpm, true, true}; }

void testCurves() {
    for(int curve = 0; curve < duck::curveCount; ++curve) {
        require(duck::curveNames[static_cast<size_t>(curve)][0] != '\0', "Curve is unnamed");
        for(int i = 0; i <= 1000; ++i) {
            const double value = duck::curveValue(curve, i / 1000.0);
            require(value >= -1e-12 && value <= 1 + 1e-12, "Curve leaves the 0..1 gain range");
            if(i > 0) require(std::abs(value - duck::curveValue(curve, (i - 1) / 1000.0)) < 0.2, "Curve has a discontinuity");
        }
    }
    require(std::abs(duck::curveValue(duck::defaultCurve, 0)) < 1e-12 && std::abs(duck::curveValue(duck::defaultCurve, 1) - 1) < 1e-12,
        "Default curve does not run from full duck to unity");
    double previous = 0;
    for(int i = 0; i <= 100; ++i) {
        const double value = duck::curveValue(duck::defaultCurve, i / 100.0);
        require(value >= previous - 1e-12, "Default curve is not monotonic");
        previous = value;
    }
    require(duck::cycleGain(2, 0.5, 0.5) > 0.999, "Curve does not end at the selected length");
    require(duck::cycleGain(2, 0.25, 0.5) > 0.49 && duck::cycleGain(2, 0.25, 0.5) < 0.51, "Length does not compress the curve");
}

void testSync() {
    duck::Engine engine;
    engine.prepare(48000);
    duck::Parameters p; p.smoothing = 0;
    engine.beginBlock(playing(0), p);
    require(engine.nextGain() < 1e-9, "Sync does not start fully ducked on the beat");
    engine.beginBlock(playing(0.5), p);
    const double middle = engine.nextGain();
    require(std::abs(middle - duck::cycleGain(2, 0.5, 1)) < 1e-12, "Sync gain does not follow host position");
    p.rate = 3;
    engine.beginBlock(playing(1), p);
    require(std::abs(engine.nextGain() - duck::cycleGain(2, 0.25, 1)) < 1e-12, "Rate does not change the cycle length");
    p.rate = 1; p.shift = 0.5;
    engine.beginBlock(playing(0.5), p);
    require(engine.nextGain() < 1e-9, "Shift does not move the curve within the cycle");
    p.shift = 0;
    duck::Transport stopped = playing(0.1); stopped.playing = false;
    engine.beginBlock(stopped, p);
    require(engine.nextGain() == 1 && !engine.running(), "Stopped transport ducks");
    engine.beginBlock({0.1, 120, true, false}, p);
    require(engine.nextGain() == 1 && !engine.synced(), "Missing host timing ducks");
    p.smoothing = 3;
    engine.beginBlock(playing(0), p);
    const double first = engine.nextGain();
    require(first > 0.5 && first < 1, "Smoothing does not limit the first step");
    engine.beginBlock(playing(0.1, 120), p);
    for(int i = 0; i < 4800; ++i) engine.nextGain();
}

void testTriggers() {
    duck::Engine engine;
    engine.prepare(48000);
    duck::Parameters p; p.smoothing = 0; p.trigger = duck::Trigger::Midi;
    engine.beginBlock({0, 120, false, true}, p);
    require(engine.nextGain() == 1, "MIDI mode ducks before a trigger");
    engine.trigger();
    require(engine.nextGain() < 1e-9 && engine.running(), "Trigger does not restart the curve");
    for(int i = 0; i < 12000; ++i) engine.nextGain();
    const double halfway = engine.nextGain();
    require(std::abs(halfway - duck::cycleGain(2, 12001.0 / 24000, 1)) < 1e-9, "Triggered curve ignores tempo and rate");
    for(int i = 0; i < 12100; ++i) engine.nextGain();
    require(!engine.running() && engine.nextGain() == 1, "Triggered curve does not return to unity");
    p.retrigger = false;
    engine.beginBlock({0, 120, false, true}, p);
    engine.trigger();
    for(int i = 0; i < 1000; ++i) engine.nextGain();
    engine.trigger();
    require(engine.nextGain() > 0 && engine.phase() > 0.04, "Retrigger off restarts a running curve");
    p.retrigger = true;
    engine.beginBlock({0, 120, false, true}, p);
    engine.trigger();
    require(engine.nextGain() < 1e-9, "Retrigger on ignores a new event");
    p.trigger = duck::Trigger::Sync;
    engine.beginBlock({0, 120, false, true}, p);
    engine.trigger();
    require(engine.nextGain() == 1, "Sync mode responds to triggers");
    p.trigger = duck::Trigger::Audio; p.shift = 0.25;
    engine.beginBlock({0, 120, false, true}, p);
    engine.trigger();
    require(engine.nextGain() == 1, "Shift does not delay a triggered curve");
}

void testBandAndDetector() {
    duck::Crossover band; band.prepare(48000); band.setFrequency(150);
    double lowEnergy = 0, highEnergy = 0, sumEnergy = 0, inputEnergy = 0;
    for(int i = 0; i < 48000; ++i) {
        const double bass = std::sin(6.283185307179586 * 50 * i / 48000), treble = std::sin(6.283185307179586 * 5000 * i / 48000);
        const auto bands = band.split(0, bass + treble);
        if(i < 4800) continue;
        lowEnergy += bands.low * bands.low; highEnergy += bands.high * bands.high;
        sumEnergy += (bands.low + bands.high) * (bands.low + bands.high); inputEnergy += (bass + treble) * (bass + treble);
    }
    require(lowEnergy > 0.9 * 0.5 * 43200 && lowEnergy < 1.1 * 0.5 * 43200, "Low band does not isolate the bass");
    require(highEnergy > 0.9 * 0.5 * 43200 && highEnergy < 1.1 * 0.5 * 43200, "High band does not isolate the treble");
    require(std::abs(sumEnergy / inputEnergy - 1) < 0.02, "Bands do not recombine with flat magnitude");
    duck::TransientDetector detector; detector.prepare(48000); detector.setThresholdDb(-24);
    int fires = 0;
    for(int burst = 0; burst < 3; ++burst) for(int i = 0; i < 24000; ++i) fires += detector.process(i < 2400 ? 0.8 * std::sin(i * 0.3) : 0.0);
    require(fires == 3, "Detector does not fire exactly once per burst");
    fires = 0;
    for(int i = 0; i < 48000; ++i) fires += detector.process(0.01);
    require(fires == 0, "Detector fires below threshold");
}
}

int main() {
    try {
        testCurves(); testSync(); testTriggers(); testBandAndDetector();
        std::cout << "PASS curves, sync, shift, length, smoothing, triggers, retrigger, band split and transient detection\n";
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
