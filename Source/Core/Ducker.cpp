#include "Ducker.h"
#include <algorithm>
#include <cmath>

namespace duck {
namespace {
double fraction(double x) noexcept { return x - std::floor(x); }
bool usableBpm(double bpm) noexcept { return std::isfinite(bpm) && bpm > 0 && bpm <= 10000; }
}

double cycleGain(int curve, double phase, double length, double tension, double floor) noexcept {
    length = std::clamp(length, 0.05, 1.0);
    double x = std::clamp(phase, 0.0, 1.0) / length;
    if (x < 1.0 && std::abs(tension) > 1e-4) {
        const double expFactor = std::pow(2.5, -std::clamp(tension, -1.0, 1.0));
        x = std::pow(x, expFactor);
    }
    const double raw = curveValue(curve, x);
    floor = std::clamp(floor, 0.0, 1.0);
    return floor + (1.0 - floor) * raw;
}

void Engine::prepare(double rate) noexcept {
    *this = Engine{};
    sampleRate = std::isfinite(rate) && rate >= 8000 && rate <= 768000 ? rate : 48000;
}

double Engine::cycleSamples() const noexcept {
    return cycleBeats[static_cast<size_t>(parameters.rate)] * 60.0 * sampleRate / bpm;
}

void Engine::beginBlock(Transport t, Parameters p) noexcept {
    p.curve = std::clamp(p.curve, 0, curveCount - 1);
    p.rate = std::clamp(p.rate, 0, static_cast<int>(cycleBeats.size()) - 1);
    p.smoothing = std::clamp(p.smoothing, 0, static_cast<int>(smoothingMilliseconds.size()) - 1);
    p.length = std::isfinite(p.length) ? std::clamp(p.length, 0.05, 1.0) : 1.0;
    p.shift = std::isfinite(p.shift) ? std::clamp(p.shift, 0.0, 1.0) : 0.0;
    p.tension = std::isfinite(p.tension) ? std::clamp(p.tension, -1.0, 1.0) : 0.0;
    p.floor = std::isfinite(p.floor) ? std::clamp(p.floor, 0.0, 1.0) : 0.0;
    parameters = p;
    const double milliseconds = smoothingMilliseconds[static_cast<size_t>(p.smoothing)];
    smoothingMs = milliseconds;
    attackCoefficient = std::exp(-1.0 / (0.0015 * sampleRate));
    releaseCoefficient = milliseconds > 0 ? std::exp(-1.0 / (milliseconds * 0.001 * sampleRate)) : 0.0;
    const bool hostTiming = t.valid && std::isfinite(t.ppq) && std::abs(t.ppq) < 1e12 && usableBpm(t.bpm);
    bpm = hostTiming ? t.bpm : (bpm > 0.0 ? bpm : 120.0);
    syncActive = hostTiming && t.playing;
    step = bpm / (60.0 * sampleRate);
    blockPpq = hostTiming ? t.ppq : 0.0;
    offset = 0;
    if(p.trigger == Trigger::Sync) isRunning = true;
}

bool Engine::trigger() noexcept {
    if(parameters.trigger == Trigger::Sync) return false;
    if(isRunning && !parameters.retrigger) return false;
    // Legato retrigger: glide from the partially recovered gain into the new
    // curve over 2 ms instead of jumping to phase 0. Small gaps need no glide,
    // and a first trigger from idle must still duck on its own sample.
    legatoElapsed = 0;
    legatoTotal = 0;
    if(isRunning && gain > 0.15) {
        legatoStart = gain;
        legatoTotal = std::max(1, static_cast<int>(0.002 * sampleRate));
    }
    isRunning = true;
    elapsed = 0;
    return true;
}

double Engine::nextGain() noexcept {
    const int rate = std::clamp(parameters.rate, 0, static_cast<int>(cycleBeats.size()) - 1);
    const double cycleLen = cycleBeats[static_cast<size_t>(rate)];
    double currentBeat = freeBeat + (cycleLen > 0 ? step / cycleLen : 0);
    if(syncActive) currentBeat = (blockPpq + static_cast<double>(offset) * step) / cycleLen;
    if(!std::isfinite(currentBeat)) currentBeat = 0;
    freeBeat = fraction(currentBeat);
    unshiftedBeat = freeBeat;

    double target = 1.0;
    if(parameters.trigger == Trigger::Sync) {
        isRunning = true;
        const double cycleLen = cycleBeats[static_cast<size_t>(parameters.rate)];
        double currentCyclePhase = freePhase + step / cycleLen;
        if(syncActive) {
            const double ppq = blockPpq + offset * step;
            currentCyclePhase = ppq / cycleLen;
        }
        freePhase = fraction(currentCyclePhase);
        lastPhase = fraction(freePhase - parameters.shift);
        if(lastPhase < 0.0) lastPhase += 1.0;
        target = cycleGain(parameters.curve, lastPhase, parameters.length, parameters.tension, parameters.floor);
        ++offset;
    } else {
        if(isRunning) {
            const double phase = elapsed / cycleSamples() - parameters.shift;
            elapsed += 1;
            if(phase >= 1) isRunning = false;
            lastPhase = std::clamp(phase, 0.0, 1.0);
            if(isRunning && phase >= 0) target = cycleGain(parameters.curve, phase, parameters.length, parameters.tension, parameters.floor);
        }
        ++offset;
    }

    // Asymmetric log-domain ballistics: large downward steps take a 1.5-3 ms
    // minimum-jerk S-curve to the floor, small downward drift tracks with a
    // fast attack, and recovery follows the slower exponential release.
    if(smoothingMs <= 0) {
        gain = target;
        transitioning = false;
    } else {
        constexpr double floorLin = 1e-4;
        const double logTarget = std::log(std::max(target, floorLin));
        double logGain = std::log(std::max(gain, floorLin));
        if(target < gain) {
            if(!transitioning && gain - target > 0.08) {
                transitioning = true;
                transitionStartLog = logGain;
                transitionElapsed = 0;
                transitionTotal = std::max(1, static_cast<int>(std::clamp(smoothingMs, 1.5, 3.0) * 0.001 * sampleRate));
            }
            if(transitioning) {
                ++transitionElapsed;
                const double progress = std::clamp(static_cast<double>(transitionElapsed) / transitionTotal, 0.0, 1.0);
                const double jerk = progress * progress * progress * (progress * (progress * 6 - 15) + 10);
                logGain = transitionStartLog + (logTarget - transitionStartLog) * jerk;
                if(transitionElapsed >= transitionTotal) transitioning = false;
            } else {
                logGain += (1 - attackCoefficient) * (logTarget - logGain);
            }
        } else {
            transitioning = false;
            logGain += (1 - releaseCoefficient) * (logTarget - logGain);
            if(logTarget - logGain < 5e-4) logGain = logTarget;
        }
        gain = std::clamp(std::exp(logGain), 0.0, 1.0);
    }

    double output = gain;
    if(legatoTotal > 0) {
        ++legatoElapsed;
        const double progress = std::clamp(static_cast<double>(legatoElapsed) / legatoTotal, 0.0, 1.0);
        const double jerk = progress * progress * progress * (progress * (progress * 6 - 15) + 10);
        output = legatoStart + (gain - legatoStart) * jerk;
        if(legatoElapsed >= legatoTotal) legatoTotal = 0;
    }
    return std::clamp(output, 0.0, 1.0);
}
}
