#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace duck {
struct Bands { double low = 0.0, high = 0.0; };

// ZDF TPT Linkwitz-Riley crossover (LR2 / LR4 / LR8) with per-sample log-Hz smoothing
// and exact allpass flat-sum reconstruction across all three slopes.
class Crossover {
public:
    void prepare(double rate) noexcept {
        sampleRate = std::max(1000.0, rate);
        smoothCoeff = 1.0 - std::exp(-1.0 / (0.015 * sampleRate));
        reset();
        setFrequency(frequency);
    }

    void reset() noexcept {
        states = {};
        started = false;
        currentLogHz = targetLogHz;
        updateIntegratorGain();
    }

    void setSlope(int slopeIndex) noexcept {
        const int next = std::clamp(slopeIndex, 0, 2);
        if (next != slopeMode) {
            slopeMode = next;
            states = {};
        }
    }

    void setFrequency(double hertz) noexcept {
        frequency = std::clamp(std::isfinite(hertz) ? hertz : 150.0, 20.0, sampleRate * 0.45);
        targetLogHz = std::log(frequency);
        if (!started) {
            currentLogHz = targetLogHz;
            updateIntegratorGain();
        }
    }

    Bands split(size_t channel, double input) noexcept {
        started = true;
        if (channel == 0 && std::abs(targetLogHz - currentLogHz) > 1e-12) {
            currentLogHz += smoothCoeff * (targetLogHz - currentLogHz);
            updateIntegratorGain();
        }

        auto& state = states[std::min<size_t>(channel, 1)];
        if (slopeMode == 0) {
            const auto s0 = stepStage(state[0], input, gCoeff, 2.0);
            return {s0.lp, -s0.hp};
        }
        if (slopeMode == 1) {
            constexpr double kButter2 = std::numbers::sqrt2;
            const auto s0 = stepStage(state[0], input, gCoeff, kButter2);
            const double low = stepStage(state[1], s0.lp, gCoeff, kButter2).lp;
            const double high = stepStage(state[2], s0.hp, gCoeff, kButter2).hp;
            return {low, high};
        }

        // LR8 cascades two 4th-order Butterworth sections (2 * cos(pi/8) and 2 * sin(pi/8)).
        constexpr double kButter4A = 1.8477590650225735;
        constexpr double kButter4B = 0.7653668647301796;
        const auto s0 = stepStage(state[0], input, gCoeff, kButter4A);
        const double l24 = stepStage(state[1], s0.lp, gCoeff, kButter4B).lp;
        const double h24 = stepStage(state[2], s0.hp, gCoeff, kButter4B).hp;
        const double low = stepStage(state[4], stepStage(state[3], l24, gCoeff, kButter4A).lp, gCoeff, kButter4B).lp;
        const double high = stepStage(state[6], stepStage(state[5], h24, gCoeff, kButter4A).hp, gCoeff, kButter4B).hp;
        return {low, high};
    }

private:
    struct Stage { double ic1eq = 0.0, ic2eq = 0.0; };
    struct StageOut { double lp = 0.0, hp = 0.0; };

    static StageOut stepStage(Stage& s, double x, double g, double k) noexcept {
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;
        const double v3 = x - s.ic2eq;
        const double v1 = a1 * s.ic1eq + a2 * v3;
        const double v2 = s.ic2eq + a2 * s.ic1eq + a3 * v3;
        s.ic1eq = 2.0 * v1 - s.ic1eq;
        s.ic2eq = 2.0 * v2 - s.ic2eq;
        return {v2, x - k * v1 - v2};
    }

    void updateIntegratorGain() noexcept {
        const double hz = std::clamp(std::exp(currentLogHz), 20.0, sampleRate * 0.45);
        gCoeff = std::tan(std::numbers::pi * hz / sampleRate);
    }

    double sampleRate = 48000.0;
    double frequency = 150.0;
    double targetLogHz = std::log(150.0);
    double currentLogHz = std::log(150.0);
    double smoothCoeff = 0.001;
    double gCoeff = 0.0049;
    int slopeMode = 1;
    bool started = false;
    std::array<std::array<Stage, 7>, 2> states {};
};
}
