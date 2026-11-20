#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace duck {

// 24 dB/oct (4th-order Butterworth) ZDF TPT SVF sidechain detector filter
// supporting HPF, LPF, and bandpass isolation when both cutoffs are active.
class SidechainFilter {
public:
    void prepare(double rate) noexcept {
        sampleRate = std::max(1000.0, rate);
        smoothCoeff = 1.0 - std::exp(-1.0 / (0.008 * sampleRate));
        reset();
        setCutoffs(highPassHz_, lowPassHz_);
    }

    void setCutoffs(double highPassHz, double lowPassHz) noexcept {
        const bool nextHighPassEnabled = highPassHz > 0.0;
        const bool nextLowPassEnabled = lowPassHz > 0.0;
        if (nextHighPassEnabled != highPassEnabled) hpStages = {};
        if (nextLowPassEnabled != lowPassEnabled) lpStages = {};
        if ((nextHighPassEnabled && nextLowPassEnabled) != (highPassEnabled && lowPassEnabled)) bpStages = {};
        highPassEnabled = nextHighPassEnabled;
        lowPassEnabled = nextLowPassEnabled;
        highPassHz_ = std::clamp(std::isfinite(highPassHz) ? highPassHz : 0.0, 0.0, sampleRate * 0.48);
        lowPassHz_ = std::clamp(std::isfinite(lowPassHz) ? lowPassHz : 0.0, 0.0, sampleRate * 0.48);
        if (highPassEnabled) targetHpLog = std::log(std::max(10.0, highPassHz_));
        if (lowPassEnabled) targetLpLog = std::log(std::max(10.0, lowPassHz_));
        if (!started) {
            currentHpLog = targetHpLog;
            currentLpLog = targetLpLog;
            updateGains();
        }
    }

    double process(double input) noexcept {
        if (!highPassEnabled && !lowPassEnabled) return input;
        started = true;
        if (std::abs(targetHpLog - currentHpLog) > 1e-12 || std::abs(targetLpLog - currentLpLog) > 1e-12) {
            currentHpLog += smoothCoeff * (targetHpLog - currentHpLog);
            currentLpLog += smoothCoeff * (targetLpLog - currentLpLog);
            updateGains();
        }

        // Crossed HPF/LPF cutoffs form a 24 dB/oct ZDF SVF bandpass centered at their geometric mean.
        if (highPassEnabled && lowPassEnabled && highPassHz_ > lowPassHz_) {
            const double first = stepSvf(bpStages[0], input, bpG, 0.85).bp;
            return stepSvf(bpStages[1], first, bpG, 0.85).bp;
        }

        double output = input;
        constexpr double kButter4A = 1.8477590650225735;
        constexpr double kButter4B = 0.7653668647301796;
        if (highPassEnabled) {
            output = stepSvf(hpStages[0], output, hpG, kButter4A).hp;
            output = stepSvf(hpStages[1], output, hpG, kButter4B).hp;
        }
        if (lowPassEnabled) {
            output = stepSvf(lpStages[0], output, lpG, kButter4A).lp;
            output = stepSvf(lpStages[1], output, lpG, kButter4B).lp;
        }
        return output;
    }

    void reset() noexcept {
        hpStages = {};
        lpStages = {};
        bpStages = {};
        started = false;
        currentHpLog = targetHpLog;
        currentLpLog = targetLpLog;
        updateGains();
    }

private:
    struct Stage { double ic1eq = 0.0, ic2eq = 0.0; };
    struct SvfOut { double lp = 0.0, bp = 0.0, hp = 0.0; };

    static SvfOut stepSvf(Stage& s, double x, double g, double k) noexcept {
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;
        const double v3 = x - s.ic2eq;
        const double v1 = a1 * s.ic1eq + a2 * v3;
        const double v2 = s.ic2eq + a2 * s.ic1eq + a3 * v3;
        s.ic1eq = 2.0 * v1 - s.ic1eq;
        s.ic2eq = 2.0 * v2 - s.ic2eq;
        return {v2, k * v1, x - k * v1 - v2};
    }

    void updateGains() noexcept {
        const double hpHz = std::clamp(std::exp(currentHpLog), 10.0, sampleRate * 0.48);
        const double lpHz = std::clamp(std::exp(currentLpLog), 10.0, sampleRate * 0.48);
        const double centerHz = std::clamp(std::exp(0.5 * (currentHpLog + currentLpLog)), 10.0, sampleRate * 0.48);
        hpG = std::tan(std::numbers::pi * hpHz / sampleRate);
        lpG = std::tan(std::numbers::pi * lpHz / sampleRate);
        bpG = std::tan(std::numbers::pi * centerHz / sampleRate);
    }

    double sampleRate = 48000.0;
    double highPassHz_ = 0.0, lowPassHz_ = 0.0;
    double targetHpLog = std::log(80.0), currentHpLog = std::log(80.0);
    double targetLpLog = std::log(500.0), currentLpLog = std::log(500.0);
    double smoothCoeff = 0.002;
    double hpG = 0.005, lpG = 0.03, bpG = 0.01;
    std::array<Stage, 2> hpStages {}, lpStages {}, bpStages {};
    bool highPassEnabled = false, lowPassEnabled = false, started = false;
};

} // namespace duck
