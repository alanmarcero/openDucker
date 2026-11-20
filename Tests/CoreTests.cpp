#include "Ducker.h"
#include "Crossover.h"
#include "TransientDetector.h"
#include "SidechainFilter.h"
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

    // Every curve must be C1 continuous (no derivative kinks across [0, 1]).
    constexpr int steps = 2000;
    constexpr double h = 1.0 / steps;
    int curveIdx = 0;
    while (curveIdx < duck::curveCount) {
        double prevSlope = (duck::curveValue(curveIdx, h) - duck::curveValue(curveIdx, 0.0)) / h;
        int stepIdx = 1;
        while (stepIdx < steps) {
            const double x0 = stepIdx * h;
            const double slope = (duck::curveValue(curveIdx, x0 + h) - duck::curveValue(curveIdx, x0)) / h;
            require(std::abs(slope - prevSlope) < 0.5, "Curve contains a C0 derivative corner");
            prevSlope = slope;
            ++stepIdx;
        }
        ++curveIdx;
    }
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
    require(std::abs(engine.beatPhase() - 0.25) < 1e-12, "Beat phase does not follow the host cycle length");
    p.rate = 1; p.shift = 0.5;
    engine.beginBlock(playing(0.5), p);
    require(engine.nextGain() < 1e-9, "Shift does not move the curve within the cycle");
    require(std::abs(engine.beatPhase() - 0.5) < 1e-12, "Shift moves the host cycle position");
    p.shift = 0;
    duck::Transport stopped = playing(0.1); stopped.playing = false;
    engine.beginBlock(stopped, p);
    require(engine.running(), "Stopped transport remains running in free-running mode");
    engine.beginBlock({0.1, 120, true, false}, p);
    require(!engine.synced() && engine.running(), "Missing host timing ducks in free-running mode");
    duck::Engine smoothEngine;
    smoothEngine.prepare(48000);
    p.smoothing = 3;
    smoothEngine.beginBlock(playing(0), p);
    const double first = smoothEngine.nextGain();
    require(first > 0.5 && first < 1, "Smoothing does not limit the first step");
    smoothEngine.beginBlock(playing(0.1, 120), p);
    for(int i = 0; i < 4800; ++i) smoothEngine.nextGain();
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
    const double beforeTrigger = engine.nextGain();
    engine.trigger();
    const double afterTrigger = engine.nextGain();
    require(std::abs(afterTrigger - beforeTrigger) < 0.05, "Sync mode responds to triggers");
    p.trigger = duck::Trigger::Audio; p.shift = 0.25;
    engine.beginBlock({0, 120, false, true}, p);
    engine.trigger();
    require(engine.nextGain() == 1, "Shift does not delay a triggered curve");
}

void testBallisticsAndLegato() {
    // Asymmetric ballistics: downward duck clears in a few ms, recovery stays slow.
    duck::Engine engine;
    engine.prepare(48000);
    duck::Parameters p;
    p.smoothing = 3;
    p.rate = 3; // 4-beat cycle: phase jumps below stay clear of the wrap while measuring.
    p.curve = 8; // Ramp: target tracks cycle phase, so host jumps are clean steps.
    engine.beginBlock(playing(3.96), p);
    int i = 0;
    while (i < 200) { engine.nextGain(); ++i; }
    require(engine.nextGain() > 0.9, "Ballistics setup does not reach unity");
    engine.beginBlock(playing(0), p);
    int downSamples = 0;
    while (downSamples < 4800 && engine.nextGain() >= 0.1) ++downSamples;
    require(downSamples < 240, "Downward duck attack is not fast with high smoothing");
    engine.beginBlock(playing(3.96), p);
    int upSamples = 0;
    while (upSamples < 9600 && engine.nextGain() <= 0.9) ++upSamples;
    require(upSamples > 3 * downSamples, "Recovery is not slower than the downward attack");

    // Legato retrigger: mid-cycle retrigger glides from the current gain, no click jump.
    duck::Engine legato;
    legato.prepare(48000);
    duck::Parameters mp;
    mp.smoothing = 0;
    mp.trigger = duck::Trigger::Midi;
    legato.beginBlock({0, 120, false, true}, mp);
    legato.trigger();
    i = 0;
    while (i < 16000) { legato.nextGain(); ++i; }
    const double beforeRetrigger = legato.nextGain();
    require(beforeRetrigger > 0.6 && beforeRetrigger < 0.85, "Legato setup is not mid-recovery");
    legato.trigger();
    const double first = legato.nextGain();
    require(first > 0.5, "Legato retrigger jumps straight to the floor");
    double previous = first;
    i = 0;
    while (i < 200) {
        const double g = legato.nextGain();
        require(std::abs(g - previous) < 0.1, "Legato retrigger trajectory clicks");
        previous = g;
        ++i;
    }
    require(previous < 0.05, "Legato retrigger does not settle onto the new curve");
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

    // LR2, LR4, and LR8 must reconstruct flat allpass magnitude right at a 40 Hz crossover and smooth cutoff jumps.
    int slope = 0;
    while (slope < 3) {
        duck::Crossover lr;
        lr.prepare(48000);
        lr.setSlope(slope);
        lr.setFrequency(40.0);
        double atCutoffSum = 0.0;
        double atCutoffIn = 0.0;
        int n = 0;
        while (n < 48000) {
            const double x = std::sin(6.283185307179586 * 40.0 * n / 48000.0);
            const auto out = lr.split(0, x);
            if (n >= 24000) {
                const double s = out.low + out.high;
                atCutoffSum += s * s;
                atCutoffIn += x * x;
            }
            ++n;
        }
        require(std::abs(atCutoffSum / atCutoffIn - 1.0) < 0.02, "LR crossover is not flat-sum at 40 Hz");
        ++slope;
    }

    duck::Crossover sweep;
    sweep.prepare(48000);
    sweep.setSlope(2);
    sweep.setFrequency(45.0);
    double prevLow = 0.0;
    int sIdx = 0;
    while (sIdx < 9600) {
        if (sIdx == 4800) sweep.setFrequency(800.0);
        const double x = 0.5 * std::sin(6.283185307179586 * 90.0 * sIdx / 48000.0);
        const double low = sweep.split(0, x).low;
        if (sIdx > 2400) require(std::abs(low - prevLow) < 0.05, "Crossover cutoff jump zippers");
        prevLow = low;
        ++sIdx;
    }
    duck::TransientDetector detector; detector.prepare(48000); detector.setThresholdDb(-24);
    int fires = 0;
    for(int burst = 0; burst < 3; ++burst) for(int i = 0; i < 24000; ++i) fires += detector.process(i < 2400 ? 0.8 * std::sin(i * 0.3) : 0.0);
    require(fires == 3, "Detector does not fire exactly once per burst");
    fires = 0;
    for(int i = 0; i < 48000; ++i) fires += detector.process(0.01);
    require(fires == 0, "Detector fires below threshold");

    // Sustained synth bass swell above threshold must not false-trigger.
    duck::TransientDetector fluxDetector;
    fluxDetector.prepare(48000);
    fluxDetector.setThresholdDb(-24);
    fluxDetector.setHysteresisDb(6.0);
    int bassFires = 0;
    int bIdx = 0;
    while (bIdx < 96000) {
        const double t = bIdx / 48000.0;
        const double env = std::min(1.0, t / 0.12);
        const double synthBass = env * (0.42 * std::sin(6.283185307179586 * 50.0 * t)
            + 0.14 * std::sin(6.283185307179586 * 100.0 * t));
        bassFires += fluxDetector.process(synthBass);
        ++bIdx;
    }
    require(bassFires == 0, "Detector false-triggers on sustained synth bass");

    // Ghost kicks riding on top of a continuous loud bass bed must re-arm and fire every beat.
    fluxDetector.prepare(48000);
    fluxDetector.setThresholdDb(-24);
    fluxDetector.setHysteresisDb(4.5);
    int ghostFires = 0;
    int gIdx = 0;
    while (gIdx < 96000) {
        const double t = gIdx / 48000.0;
        const double bedEnv = std::min(1.0, t / 0.10);
        const double bed = bedEnv * 0.28 * std::sin(6.283185307179586 * 55.0 * t);
        const int inBeat = (gIdx >= 12000) ? ((gIdx - 12000) % 21000) : -1;
        const double kickEnv = inBeat >= 0 ? std::exp(-inBeat / (48000.0 * 0.035)) : 0.0;
        const double ghostKick = 0.42 * kickEnv * std::sin(6.283185307179586 * 62.0 * t);
        ghostFires += fluxDetector.process(bed + ghostKick);
        ++gIdx;
    }
    require(ghostFires == 4, "Detector misses ghost kicks in a dense bass loop");
}

void testSidechainFilters() {
    const auto rms = [](double frequency, double highPass, double lowPass) {
        duck::SidechainFilter filter;
        filter.prepare(48000.0);
        filter.setCutoffs(highPass, lowPass);
        double energy = 0.0;
        for(int i = 0; i < 48000; ++i) {
            const double input = std::sin(6.283185307179586 * frequency * i / 48000.0);
            const double output = filter.process(input);
            if(i >= 24000) energy += output * output;
        }
        return std::sqrt(energy / 24000.0);
    };
    require(std::abs(rms(100, 0, 0) - std::sqrt(0.5)) < 1e-8, "Detector filter bypass changes the signal");
    require(rms(100, 1000, 0) < 0.12, "Detector high-pass leaves low frequencies in the trigger path");
    require(rms(5000, 1000, 0) > 0.65, "Detector high-pass removes frequencies above its cutoff");
    require(rms(5000, 0, 500) < 0.12, "Detector low-pass leaves high frequencies in the trigger path");
    require(rms(100, 0, 500) > 0.65, "Detector low-pass removes frequencies below its cutoff");

    // 24 dB/oct ZDF SVF must isolate a 60 Hz kick from a 180 Hz snare and vice versa.
    require(rms(60, 40, 90) > 0.62, "24 dB/oct bandpass attenuates the 60 Hz kick fundamental");
    require(rms(180, 40, 90) < 0.055, "6 dB/oct filter leaks 180 Hz snare into 60 Hz kick detector");
    require(rms(60, 180, 0) < 0.02, "24 dB/oct high-pass leaks 60 Hz kick into snare detector");
}
}

int main() {
    try {
        testCurves(); testSync(); testTriggers(); testBallisticsAndLegato(); testBandAndDetector(); testSidechainFilters();
        std::cout << "PASS curves, sync, shift, length, smoothing, triggers, retrigger, band split, transient detection and detector filters\n";
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
