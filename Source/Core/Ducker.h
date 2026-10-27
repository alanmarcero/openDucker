#pragma once
#include <array>

namespace duck {
inline constexpr std::array<double,6> cycleBeats {2.0, 1.0, 0.5, 0.25, 2.0/3.0, 0.75};
struct Transport {
    double ppq=0, bpm=120;
    bool playing=false, valid=false, looping=false;
    double loopStart=0, loopEnd=0;
};
struct Parameters { double amount=0.65; int shape=0, rate=1; bool bypass=false; double hold=0, recovery=1, phaseOffset=0; };

double envelope(double phase, int shape, double edge, double hold=0, double recovery=1) noexcept;

class Engine {
public:
    void prepare(double sampleRate) noexcept;
    void beginBlock(Transport, Parameters) noexcept;
    double nextGain() noexcept;
    double phase() const noexcept { return lastPhase; }
    bool synced() const noexcept { return active; }
private:
    double wrapLoop(double ppq) const noexcept;
    double sampleRate=48000, step=0, blockPpq=0, expectedPpq=0, lastPhase=0, lastGain=1;
    double amount=0, amountTarget=0, amountStep=0, transitionFrom=1;
    double previousPpq=0, edgeBpm=120, edgeBpmTarget=120, edgeBpmStep=0;
    std::array<double,3> timing{0,1,0}, previousTiming{0,1,0};
    double timingCorrection=0, timingCorrectionStep=0;
    int timingTransitionLeft=0;
    bool timingChanged=false;
    std::array<double,4> shapes{}, shapeTargets{}, shapeSteps{};
    std::array<double,6> rates{}, rateTargets{}, rateSteps{};
    Transport transport;
    int offset=0, rampLeft=0, rampLength=240, transitionLeft=0, transitionLength=96, edgeRampLeft=0, selectedRate=1;
    bool initialized=false, active=false, loopEnabled=false, renderedActive=false;
};
}
