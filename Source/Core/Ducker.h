#pragma once
#include <array>
#include "Curves.h"

namespace duck {
inline constexpr std::array<double, 4> cycleBeats {0.5, 1.0, 2.0, 4.0}; // 1/8, 1/4, 1/2, 1/1
inline constexpr std::array<double, 4> smoothingMilliseconds {0.0, 1.0, 3.0, 8.0};
enum class Trigger { Sync = 0, Midi = 1, Audio = 2 };

struct Transport {
    double ppq = 0, bpm = 120;
    bool playing = false, valid = false;
};
struct Parameters {
    int curve = defaultCurve, rate = 1, smoothing = 1;
    double length = 1, shift = 0; // fractions of the cycle
    Trigger trigger = Trigger::Sync;
    bool retrigger = true;
};

/// Gain of the selected curve at a cycle phase. The curve spans `length` of the cycle,
/// then holds its final value until the cycle restarts.
double cycleGain(int curve, double phase, double length) noexcept;

class Engine {
public:
    void prepare(double sampleRate) noexcept;
    void beginBlock(Transport, Parameters) noexcept;
    /// Restart the curve from an audio or MIDI event. Ignored in Sync mode.
    void trigger() noexcept;
    double nextGain() noexcept;
    /// Position inside the cycle (0..1) of the most recent sample.
    double phase() const noexcept { return lastPhase; }
    /// True while the curve is advancing (host playing in Sync mode, or after a trigger).
    bool running() const noexcept { return isRunning; }
    bool synced() const noexcept { return syncActive; }
private:
    double cycleSamples() const noexcept;
    double sampleRate = 48000, step = 0, blockPpq = 0, bpm = 120, smoothingCoefficient = 0;
    double gain = 1, lastPhase = 0, elapsed = 0;
    Parameters parameters;
    bool syncActive = false, isRunning = false;
    int offset = 0;
};
}
