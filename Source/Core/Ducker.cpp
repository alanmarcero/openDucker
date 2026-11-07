#include "Ducker.h"
#include <algorithm>
#include <cmath>

namespace duck {
namespace {
double fraction(double x) noexcept { return x - std::floor(x); }
bool usableBpm(double bpm) noexcept { return std::isfinite(bpm) && bpm > 0 && bpm <= 10000; }
}

double cycleGain(int curve, double phase, double length) noexcept {
    length = std::clamp(length, 0.05, 1.0);
    return curveValue(curve, std::clamp(phase, 0.0, 1.0) / length);
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
    if(p.trigger == Trigger::Sync) isRunning = false;
    parameters = p;
    const double milliseconds = smoothingMilliseconds[static_cast<size_t>(p.smoothing)];
    smoothingCoefficient = milliseconds > 0 ? std::exp(-1.0 / (milliseconds * 0.001 * sampleRate)) : 0.0;
    const bool hostTiming = t.valid && std::isfinite(t.ppq) && std::abs(t.ppq) < 1e12 && usableBpm(t.bpm);
    bpm = hostTiming ? t.bpm : 120.0;
    syncActive = hostTiming && t.playing;
    step = bpm / (60.0 * sampleRate);
    blockPpq = hostTiming ? t.ppq : 0.0;
    offset = 0;
}

void Engine::trigger() noexcept {
    if(parameters.trigger == Trigger::Sync) return;
    if(isRunning && !parameters.retrigger) return;
    isRunning = true;
    elapsed = 0;
}

double Engine::nextGain() noexcept {
    double target = 1;
    if(parameters.trigger == Trigger::Sync) {
        isRunning = syncActive;
        lastPhase = 0;
        if(syncActive) {
            const double ppq = blockPpq + offset * step;
            lastPhase = fraction(ppq / cycleBeats[static_cast<size_t>(parameters.rate)] - parameters.shift);
            target = cycleGain(parameters.curve, lastPhase, parameters.length);
        }
    } else if(isRunning) {
        const double phase = elapsed / cycleSamples() - parameters.shift;
        elapsed += 1;
        if(phase >= 1) isRunning = false;
        lastPhase = std::clamp(phase, 0.0, 1.0);
        if(isRunning && phase >= 0) target = cycleGain(parameters.curve, phase, parameters.length);
    }
    ++offset;
    gain = target + (gain - target) * smoothingCoefficient;
    return std::clamp(gain, 0.0, 1.0);
}
}
