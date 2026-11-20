#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace duck {
// Dual-envelope crest-factor / flux transient detector with adjustable Schmitt-trigger hysteresis.
class TransientDetector {
public:
    void prepare(double rate) noexcept {
        sampleRate = std::max(1000.0, rate);
        reset();
        fastRelease = std::exp(-1.0 / (0.022 * sampleRate));
        slowRmsAttack = 1.0 - std::exp(-1.0 / (0.035 * sampleRate));
        slowRmsRelease = 1.0 - std::exp(-1.0 / (0.100 * sampleRate));
        slowPeakAttack = 1.0 - std::exp(-1.0 / (0.025 * sampleRate));
        slowPeakRelease = std::exp(-1.0 / (0.080 * sampleRate));
        // 26 ms peak-hold bridges full fundamental periods of asymmetric bass down to 38 Hz.
        peakHoldSamples = static_cast<int>(0.026 * sampleRate);
        holdSamples = static_cast<int>(0.025 * sampleRate);
        setHysteresisDb(hysteresisDb_);
    }

    void reset() noexcept {
        fastPeak = 0.0;
        slowMeanSq = 0.0;
        slowPeak = 0.0;
        prevSample = 0.0;
        peakHistory.fill(0.0);
        historyPosition = 0;
        peakHoldLeft = 0;
        holdLeft = 0;
        armed = true;
    }

    void setThresholdDb(double decibels) noexcept {
        threshold = std::pow(10.0, std::clamp(std::isfinite(decibels) ? decibels : -24.0, -90.0, 0.0) / 20.0);
        updateSchmittBounds();
    }

    void setHysteresisDb(double decibels) noexcept {
        hysteresisDb_ = std::clamp(std::isfinite(decibels) ? decibels : 6.0, 1.0, 18.0);
        updateSchmittBounds();
    }

    bool process(double sample) noexcept {
        const double cleanSample = std::isfinite(sample) ? sample : 0.0;
        const double magnitude = std::abs(cleanSample);
        const double slew = std::abs(cleanSample - prevSample);
        prevSample = cleanSample;

        const double prevSlowRms = std::sqrt(2.0 * slowMeanSq);
        const double prevBaseline = std::max(prevSlowRms, slowPeak);

        if (magnitude >= fastPeak) {
            fastPeak = magnitude;
            peakHoldLeft = peakHoldSamples;
        }
        if (magnitude < fastPeak) {
            if (peakHoldLeft > 0) --peakHoldLeft;
            if (peakHoldLeft == 0) fastPeak *= fastRelease;
        }

        const double sq = magnitude * magnitude;
        const double rmsAlpha = sq > slowMeanSq ? slowRmsAttack : slowRmsRelease;
        slowMeanSq += rmsAlpha * (sq - slowMeanSq);
        slowPeak = fastPeak >= slowPeak
            ? slowPeak + slowPeakAttack * (fastPeak - slowPeak)
            : slowPeak * slowPeakRelease;

        // Envelope rise over the last ~11 ms: slow swells stay under it, kicks exceed it.
        const double delayedPeak = peakHistory[historyPosition];
        peakHistory[historyPosition] = fastPeak;
        historyPosition = (historyPosition + 1) % peakHistory.size();
        const double riseWindow = fastPeak - delayedPeak;

        if (holdLeft > 0) {
            --holdLeft;
            return false;
        }

        const bool slowSwell = riseWindow < threshold * 0.8 && slew < threshold * 0.25;
        if (slowSwell && fastPeak >= threshold) slowPeak = fastPeak;

        const double baseline = std::max(slowSwell ? fastPeak : prevBaseline, 1e-9);
        const double crest = fastPeak / baseline;
        const double flux = fastPeak - baseline;
        if (!armed && (fastPeak < rearmLevel || crest < rearmCrest)) {
            armed = true;
        }
        if (armed && fastPeak >= threshold && flux >= threshold * 0.6 && crest >= triggerCrest) {
            armed = false;
            holdLeft = holdSamples;
            return true;
        }
        return false;
    }

private:
    void updateSchmittBounds() noexcept {
        rearmLevel = threshold * std::pow(10.0, -hysteresisDb_ / 20.0);
        rearmCrest = 1.0 + (triggerCrest - 1.0) * std::pow(10.0, -hysteresisDb_ / 10.0);
    }

    static constexpr double triggerCrest = 1.22;
    double sampleRate = 48000.0;
    double fastPeak = 0.0, slowMeanSq = 0.0, slowPeak = 0.0, prevSample = 0.0;
    double threshold = 0.0630957344, hysteresisDb_ = 6.0;
    double rearmLevel = 0.0316, rearmCrest = 1.10;
    double fastRelease = 0.999, slowRmsAttack = 0.001, slowRmsRelease = 0.0002;
    double slowPeakAttack = 0.001, slowPeakRelease = 0.9997;
    std::array<double, 512> peakHistory {};
    size_t historyPosition = 0;
    int peakHoldSamples = 1248, peakHoldLeft = 0;
    int holdSamples = 1200, holdLeft = 0;
    bool armed = true;
};
}
