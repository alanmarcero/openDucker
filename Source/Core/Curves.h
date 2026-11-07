#pragma once
#include <array>

namespace duck {
inline constexpr int curveCount = 16;
inline constexpr std::array<const char*, curveCount> curveNames {
    "Quick Rise", "Slow Start", "Classic S", "Early S", "Trapezoid", "Gate", "Dip", "Peak",
    "Ramp", "Soft Start", "Hold and Rise", "Block", "Short Pulse", "Linear Fall", "Valley", "Double Wave"};
constexpr int defaultCurve = 2;

/// Gain (0 = fully ducked, 1 = unity) of a curve at position x (0..1) through the curve.
double curveValue(int index, double x) noexcept;
}
