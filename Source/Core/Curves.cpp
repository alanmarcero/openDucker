#include "Curves.h"
#include <algorithm>
#include <cmath>

namespace duck {
namespace {
double clamp01(double x) noexcept { return std::clamp(x, 0.0, 1.0); }
double smooth(double x) noexcept { x = clamp01(x); return x * x * (3 - 2 * x); }
double smoother(double x) noexcept { x = clamp01(x); return x * x * x * (x * (x * 6 - 15) + 10); }
/// Ramp up over [0, rise], hold at one, ramp down over [fallStart, fallEnd], then stay at zero.
double trapezoid(double x, double rise, double fallStart, double fallEnd) noexcept {
    if(x < fallStart) return smooth(x / rise);
    return smooth((fallEnd - x) / (fallEnd - fallStart));
}
}

double curveValue(int index, double x) noexcept {
    x = clamp01(x);
    switch(std::clamp(index, 0, curveCount - 1)) {
        case 0: return 1 - std::pow(1 - x, 3);
        case 1: return x * x;
        case 2: return smoother(x);
        case 3: return smooth(std::pow(x, 0.6));
        case 4: return trapezoid(x, 0.3, 0.7, 1.0);
        case 5: return 1 - smooth(x / 0.08);
        case 6:
            if(x < 0.55) return 1 - smooth(x / 0.55);
            return x < 0.85 ? smooth((x - 0.55) / 0.3) : 1.0;
        case 7: return x < 0.8 ? std::pow(x / 0.8, 2.0) : (1 - x) / 0.2;
        case 8: return x;
        case 9: return std::pow(smooth(x), 1.6);
        case 10: return smooth((x - 0.15) / 0.55);
        case 11: return x < 0.9 ? smooth(x / 0.15) : smooth((1 - x) / 0.1);
        case 12: return trapezoid(x, 0.25, 0.5, 0.75);
        case 13: return 1 - x;
        case 14:
            if(x < 0.35) return 1 - smooth(x / 0.35);
            return x < 0.65 ? 0.0 : std::pow(smooth((x - 0.65) / 0.35), 2.0);
        default:
            if(x < 0.25) return 0.55 * smooth(x / 0.25);
            if(x < 0.45) return 0.55 - 0.35 * smooth((x - 0.25) / 0.2);
            return 0.2 + 0.8 * smooth((x - 0.45) / 0.55);
    }
}
}
