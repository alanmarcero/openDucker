#include "Ducker.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace duck {
namespace {
double smooth(double x) noexcept { return x*x*(3-2*x); }
double fraction(double x) noexcept { return x-std::floor(x); }
}

double envelope(double phase, int shape, double edge) noexcept {
    phase=std::clamp(phase,0.0,1.0);
    edge=std::clamp(edge,0.000001,0.08);
    if(phase>1-edge) return smooth((1-phase)/edge);
    const double s=smooth(phase/(1-edge));
    switch(shape) {
        case 1: return s*s;
        case 2: return 1-(1-s)*(1-s)*(1-s);
        case 3: return s*s*s;
        default: return s;
    }
}

void Engine::prepare(double sr) noexcept {
    *this=Engine{};
    sampleRate=std::isfinite(sr) && sr>=8000 && sr<=768000 ? sr : 48000;
    rampLength=static_cast<int>(sampleRate*0.005);
    transitionLength=static_cast<int>(sampleRate*0.002);
}

double Engine::wrapLoop(double ppq) const noexcept {
    // Re-anchoring can move an exact boundary by a few floating-point ULPs.
    const double tolerance=8*std::numeric_limits<double>::epsilon()*std::max(1.0,std::abs(ppq));
    if(loopEnabled && ppq>=transport.loopEnd-tolerance) {
        if(ppq<transport.loopEnd) return transport.loopStart;
        return transport.loopStart+std::fmod(ppq-transport.loopStart,transport.loopEnd-transport.loopStart);
    }
    return ppq;
}

void Engine::beginBlock(Transport t, Parameters p) noexcept {
    const bool wasActive=active;
    transport=t;
    active=t.valid && t.playing && std::isfinite(t.ppq) && std::abs(t.ppq)<1e12
        && std::isfinite(t.bpm) && t.bpm>0 && t.bpm<=10000;
    step=active ? t.bpm/(60*sampleRate) : 0;
    if(active && (!initialized || !wasActive)) {
        edgeBpm=edgeBpmTarget=t.bpm; edgeRampLeft=0;
    } else if(active && t.bpm!=edgeBpmTarget) {
        edgeBpmTarget=t.bpm; edgeRampLeft=transitionLength;
        edgeBpmStep=(edgeBpmTarget-edgeBpm)/edgeRampLeft;
    }
    loopEnabled=active && t.looping && std::isfinite(t.loopStart) && std::isfinite(t.loopEnd)
        && std::abs(t.loopStart)<1e12 && std::abs(t.loopEnd)<1e12 && t.loopEnd>t.loopStart;
    blockPpq=active ? wrapLoop(t.ppq) : 0;
    offset=0;
    // Phase is never smoothed. Only amplitude at transport discontinuities is crossfaded.
    if(initialized && (active!=wasActive || (active && std::abs(blockPpq-expectedPpq)>std::max(step*1.5,1e-9)))) {
        transitionFrom=lastGain;
        transitionLeft=transitionLength;
    }
    p.shape=std::clamp(p.shape,0,3); p.rate=std::clamp(p.rate,0,5);
    selectedRate=p.rate;
    const double target=p.bypass ? 0 : (std::isfinite(p.amount) ? std::clamp(p.amount,0.0,1.0) : 0);
    std::array<double,4> newShapes{}; newShapes[static_cast<size_t>(p.shape)]=1;
    std::array<double,6> newRates{}; newRates[static_cast<size_t>(p.rate)]=1;
    if(!initialized) {
        amount=amountTarget=target;
        shapes=shapeTargets=newShapes; rates=rateTargets=newRates;
        initialized=true;
    } else if(target!=amountTarget || newShapes!=shapeTargets || newRates!=rateTargets) {
        amountTarget=target; shapeTargets=newShapes; rateTargets=newRates;
        rampLeft=rampLength;
        amountStep=(amountTarget-amount)/rampLength;
        for(size_t i=0;i<shapes.size();++i) shapeSteps[i]=(shapeTargets[i]-shapes[i])/rampLength;
        for(size_t i=0;i<rates.size();++i) rateSteps[i]=(rateTargets[i]-rates[i])/rampLength;
    }
}

double Engine::nextGain() noexcept {
    const double ppq=wrapLoop(blockPpq+static_cast<double>(offset)*step);
    if(active && renderedActive && loopEnabled && (ppq<previousPpq || step>=transport.loopEnd-transport.loopStart)) {
        // A loop is only seamless when its length is a whole number of every audible cycle.
        const double length=transport.loopEnd-transport.loopStart;
        for(size_t r=0;r<rates.size();++r) if(rates[r]>1e-12 && std::abs(length/cycleBeats[r]-std::round(length/cycleBeats[r]))>1e-9) {
            transitionFrom=lastGain; transitionLeft=transitionLength; break;
        }
    }
    lastPhase=active ? fraction(ppq/cycleBeats[static_cast<size_t>(selectedRate)]) : 0;
    double shaped=0;
    if(active) {
        for(size_t r=0;r<rates.size();++r) if(rates[r]>0) {
            const double phase=fraction(ppq/cycleBeats[r]);
            const double edge=std::min(0.08,0.003*edgeBpm/(60*cycleBeats[r]));
            double value=0;
            for(size_t s=0;s<shapes.size();++s) if(shapes[s]>0)
                value+=shapes[s]*envelope(phase,static_cast<int>(s),edge);
            shaped+=rates[r]*value;
        }
    }
    double gain=active ? 1-amount*(1-shaped) : 1;
    if(transitionLeft>0) {
        const double mix=smooth(1-static_cast<double>(transitionLeft)/transitionLength);
        gain=transitionFrom+(gain-transitionFrom)*mix;
        --transitionLeft;
    }
    if(rampLeft>0) {
        amount+=amountStep;
        for(size_t i=0;i<shapes.size();++i) shapes[i]+=shapeSteps[i];
        for(size_t i=0;i<rates.size();++i) rates[i]+=rateSteps[i];
        if(--rampLeft==0) {amount=amountTarget; shapes=shapeTargets; rates=rateTargets;}
    }
    if(edgeRampLeft>0) {
        edgeBpm+=edgeBpmStep;
        if(--edgeRampLeft==0) edgeBpm=edgeBpmTarget;
    }
    previousPpq=ppq; renderedActive=active;
    ++offset;
    expectedPpq=wrapLoop(blockPpq+static_cast<double>(offset)*step);
    lastGain=std::clamp(gain,0.0,1.0);
    return lastGain;
}
}
