#pragma once
#include <algorithm>
#include <cmath>

namespace duck {
/// Fires once when a signal rises above the threshold, then re-arms after it falls
/// well below it and a short hold-off passes.
class TransientDetector {
public:
    void prepare(double rate) noexcept {
        sampleRate = rate; level = 0; armed = true; holdLeft = 0;
        releaseCoefficient = std::exp(-1.0 / (0.02 * rate));
        holdSamples = static_cast<int>(0.03 * rate);
    }
    void setThresholdDb(double decibels) noexcept { threshold = std::pow(10.0, std::clamp(decibels, -90.0, 0.0) / 20.0); }
    bool process(double sample) noexcept {
        const double magnitude = std::abs(sample);
        level = magnitude > level ? magnitude : level * releaseCoefficient;
        if(holdLeft > 0) { --holdLeft; return false; }
        if(armed && level >= threshold) { armed = false; holdLeft = holdSamples; return true; }
        if(!armed && level < threshold * 0.5) armed = true;
        return false;
    }
private:
    double sampleRate = 48000, level = 0, threshold = 0.063, releaseCoefficient = 0;
    int holdSamples = 0, holdLeft = 0;
    bool armed = true;
};
}
