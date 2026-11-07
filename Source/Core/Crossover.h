#pragma once
#include <array>
#include <cmath>
#include <algorithm>

namespace duck {
struct Bands { double low = 0, high = 0; };

/// Fourth-order Linkwitz-Riley crossover. The two bands sum to an all-pass of the input,
/// so changing only one band's gain leaves the other untouched.
class Crossover {
public:
    void prepare(double rate) noexcept { sampleRate = rate; states = {}; setFrequency(frequency); }
    void setFrequency(double hertz) noexcept {
        frequency = std::clamp(hertz, 20.0, sampleRate * 0.45);
        const double omega = 6.283185307179586 * frequency / sampleRate;
        const double alpha = std::sin(omega) / (2 * 0.7071067811865476), cosine = std::cos(omega);
        const double a0 = 1 + alpha;
        low.b0 = (1 - cosine) * 0.5 / a0; low.b1 = (1 - cosine) / a0; low.b2 = low.b0;
        high.b0 = (1 + cosine) * 0.5 / a0; high.b1 = -(1 + cosine) / a0; high.b2 = high.b0;
        a1 = -2 * cosine / a0; a2 = (1 - alpha) / a0;
    }
    Bands split(size_t channel, double input) noexcept {
        auto& state = states[std::min<size_t>(channel, 1)];
        return {run(low, state[0], run(low, state[1], input)), run(high, state[2], run(high, state[3], input))};
    }
private:
    struct Coefficients { double b0 = 0, b1 = 0, b2 = 0; };
    struct Stage { double z1 = 0, z2 = 0; };
    double run(const Coefficients& c, Stage& s, double x) const noexcept {
        const double y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - a1 * y + s.z2;
        s.z2 = c.b2 * x - a2 * y;
        return y;
    }
    double sampleRate = 48000, frequency = 150, a1 = 0, a2 = 0;
    Coefficients low, high;
    std::array<std::array<Stage, 4>, 2> states {};
};
}
