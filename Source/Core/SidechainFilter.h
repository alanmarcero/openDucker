#pragma once

#include <algorithm>
#include <cmath>

namespace duck {

/// First-order detector-path HPF/LPF. A cutoff of zero bypasses that stage.
class SidechainFilter {
public:
    void prepare(double rate) noexcept {
        sampleRate = std::max(1000.0, rate);
        reset();
        updateCoefficients();
    }

    void setCutoffs(double highPassHz, double lowPassHz) noexcept {
        const bool nextHighPassEnabled = highPassHz > 0.0;
        const bool nextLowPassEnabled = lowPassHz > 0.0;
        if(nextHighPassEnabled != highPassEnabled) highPassInput = highPassOutput = 0.0;
        if(nextLowPassEnabled != lowPassEnabled) lowPassOutput = 0.0;
        highPassEnabled = nextHighPassEnabled;
        lowPassEnabled = nextLowPassEnabled;
        highPassHz_ = std::clamp(highPassHz, 0.0, sampleRate * 0.49);
        lowPassHz_ = std::clamp(lowPassHz, 0.0, sampleRate * 0.49);
        updateCoefficients();
    }

    double process(double input) noexcept {
        double output = input;
        if(highPassEnabled) {
            output = highPassAlpha * (highPassOutput + output - highPassInput);
            highPassInput = input;
            highPassOutput = output;
        }
        if(lowPassEnabled) {
            lowPassOutput += lowPassAlpha * (output - lowPassOutput);
            output = lowPassOutput;
        }
        return output;
    }

    void reset() noexcept {
        highPassInput = highPassOutput = lowPassOutput = 0.0;
    }

private:
    void updateCoefficients() noexcept {
        constexpr double twoPi = 6.283185307179586476925286766559;
        highPassAlpha = 1.0 / (1.0 + twoPi * highPassHz_ / sampleRate);
        lowPassAlpha = 1.0 - std::exp(-twoPi * lowPassHz_ / sampleRate);
    }

    double sampleRate = 48000.0;
    double highPassHz_ = 0.0, lowPassHz_ = 0.0;
    double highPassAlpha = 1.0, lowPassAlpha = 1.0;
    double highPassInput = 0.0, highPassOutput = 0.0, lowPassOutput = 0.0;
    bool highPassEnabled = false, lowPassEnabled = false;
};

} // namespace duck
