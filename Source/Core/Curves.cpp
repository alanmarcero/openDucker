#include "Curves.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace duck {
namespace {
struct Knot {
    double x = 0.0;
    double y = 0.0;
};

struct Spline {
    std::array<Knot, 6> knots {};
    std::array<double, 6> tangents {};
    size_t count = 0;
};

// Builds a C1-continuous Fritsch-Carlson monotonic cubic Hermite spline from knots in [0, 1].
Spline makeSpline(std::initializer_list<Knot> list, bool easeStart = true, bool easeEnd = true) noexcept {
    Spline s;
    s.count = std::min<size_t>(list.size(), s.knots.size());
    std::copy_n(list.begin(), s.count, s.knots.begin());
    if (s.count < 2) return s;

    std::array<double, 5> h {};
    std::array<double, 5> d {};
    size_t idx = 0;
    while (idx + 1 < s.count) {
        h[idx] = std::max(1e-9, s.knots[idx + 1].x - s.knots[idx].x);
        d[idx] = (s.knots[idx + 1].y - s.knots[idx].y) / h[idx];
        ++idx;
    }

    const auto oneSided = [](double h0, double h1, double d0, double d1) noexcept {
        const double raw = ((2.0 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
        if (raw * d0 <= 0.0) return 0.0;
        if (d0 * d1 < 0.0 && std::abs(raw) > 3.0 * std::abs(d0)) return 3.0 * d0;
        return raw;
    };

    if (!easeStart) {
        s.tangents[0] = s.count == 2 ? d[0] : oneSided(h[0], h[1], d[0], d[1]);
    }
    const size_t last = s.count - 1;
    if (!easeEnd) {
        s.tangents[last] = s.count == 2 ? d[0] : oneSided(h[last - 1], h[last - 2], d[last - 1], d[last - 2]);
    }

    size_t k = 1;
    while (k < last) {
        if (d[k - 1] * d[k] > 0.0) {
            const double w1 = 2.0 * h[k] + h[k - 1];
            const double w2 = h[k] + 2.0 * h[k - 1];
            s.tangents[k] = (w1 + w2) / (w1 / d[k - 1] + w2 / d[k]);
        }
        ++k;
    }

    size_t seg = 0;
    while (seg < last) {
        if (std::abs(d[seg]) < 1e-12) {
            s.tangents[seg] = 0.0;
            s.tangents[seg + 1] = 0.0;
        }
        if (std::abs(d[seg]) >= 1e-12) {
            const double alpha = s.tangents[seg] / d[seg];
            const double beta = s.tangents[seg + 1] / d[seg];
            const double radiusSq = alpha * alpha + beta * beta;
            if (radiusSq > 9.0) {
                const double tau = 3.0 / std::sqrt(radiusSq);
                s.tangents[seg] = tau * alpha * d[seg];
                s.tangents[seg + 1] = tau * beta * d[seg];
            }
        }
        ++seg;
    }
    return s;
}

const std::array<Spline, curveCount>& curveTable() noexcept {
    static const std::array<Spline, curveCount> splines {{
        makeSpline({{0.0, 0.0}, {0.25, 0.578125}, {0.5, 0.875}, {0.75, 0.984375}, {1.0, 1.0}}, false, true),
        makeSpline({{0.0, 0.0}, {0.25, 0.0625}, {0.5, 0.25}, {0.75, 0.5625}, {1.0, 1.0}}, true, false),
        makeSpline({{0.0, 0.0}, {0.25, 0.1035}, {0.5, 0.5}, {0.75, 0.8965}, {1.0, 1.0}}, true, true),
        makeSpline({{0.0, 0.0}, {0.15, 0.256}, {0.4, 0.614}, {0.7, 0.896}, {1.0, 1.0}}, true, true),
        makeSpline({{0.0, 0.0}, {0.3, 1.0}, {0.7, 1.0}, {1.0, 0.0}}, true, true),
        makeSpline({{0.0, 1.0}, {0.08, 0.0}, {1.0, 0.0}}, true, true),
        makeSpline({{0.0, 1.0}, {0.55, 0.0}, {0.85, 1.0}, {1.0, 1.0}}, true, true),
        makeSpline({{0.0, 0.0}, {0.4, 0.25}, {0.8, 1.0}, {1.0, 0.0}}, true, true),
        makeSpline({{0.0, 0.0}, {1.0, 1.0}}, false, false),
        makeSpline({{0.0, 0.0}, {0.25, 0.036}, {0.5, 0.33}, {0.75, 0.785}, {1.0, 1.0}}, true, true),
        makeSpline({{0.0, 0.0}, {0.15, 0.0}, {0.7, 1.0}, {1.0, 1.0}}, true, true),
        makeSpline({{0.0, 0.0}, {0.15, 1.0}, {0.9, 1.0}, {1.0, 0.0}}, true, true),
        makeSpline({{0.0, 0.0}, {0.25, 1.0}, {0.5, 1.0}, {0.75, 0.0}, {1.0, 0.0}}, true, true),
        makeSpline({{0.0, 1.0}, {1.0, 0.0}}, false, false),
        makeSpline({{0.0, 1.0}, {0.35, 0.0}, {0.65, 0.0}, {0.825, 0.25}, {1.0, 1.0}}, true, true),
        makeSpline({{0.0, 0.0}, {0.25, 0.55}, {0.45, 0.2}, {1.0, 1.0}}, true, true),
    }};
    return splines;
}
}

double curveValue(int index, double x) noexcept {
    const double clamped = std::clamp(x, 0.0, 1.0);
    const auto& s = curveTable()[static_cast<size_t>(std::clamp(index, 0, curveCount - 1))];
    if (clamped <= s.knots[0].x) return s.knots[0].y;
    const size_t last = s.count - 1;
    if (clamped >= s.knots[last].x) return s.knots[last].y;

    size_t seg = 0;
    while (seg + 1 < last && clamped >= s.knots[seg + 1].x) ++seg;

    const double span = s.knots[seg + 1].x - s.knots[seg].x;
    const double t = (clamped - s.knots[seg].x) / span;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
    const double h10 = t3 - 2.0 * t2 + t;
    const double h01 = -2.0 * t3 + 3.0 * t2;
    const double h11 = t3 - t2;
    const double value = h00 * s.knots[seg].y + h10 * span * s.tangents[seg]
        + h01 * s.knots[seg + 1].y + h11 * span * s.tangents[seg + 1];
    return std::clamp(value, 0.0, 1.0);
}
}
