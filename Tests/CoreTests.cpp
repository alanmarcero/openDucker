#include "Ducker.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void near(double a, double b, double tolerance, const char* message) { require(std::abs(a-b) <= tolerance, message); }

std::vector<double> render(int blockSize, double sr = 48000, double bpm = 120, double start = 0, int count = 96000) {
    duck::Engine engine; engine.prepare(sr);
    std::vector<double> out;
    for (int offset=0; offset<count;) {
        const auto n = std::min(blockSize, count-offset);
        engine.beginBlock({start + offset*bpm/(60*sr), bpm, true, true}, {1, 0, 1, false});
        for (int i=0; i<n; ++i) out.push_back(engine.nextGain());
        offset += n;
    }
    return out;
}

int main() {
    try {
        const auto reference = render(1);
        for (int beat=0; beat<4; ++beat) near(reference[static_cast<size_t>(beat*24000)], 0, 1e-12, "Beat minimum is late");
        near(reference[12000], 0.504526999, 0.001, "Half-beat recovery is wrong");
        for (int block : {7,64,511,1024,4096}) {
            const auto actual = render(block);
            for (size_t i=0; i<actual.size(); ++i) near(actual[i], reference[i], 2e-12, "Buffer size changes timing");
        }
        for (double sr : {44100,48000,88200,96000,192000}) {
            const auto result=render(511,sr,120,0,static_cast<int>(sr+1));
            near(result[static_cast<size_t>(sr/2)],0,1e-12,"Sample-rate timing failure");
        }
        const auto negative = render(257,48000,120,-2);
        for (size_t i=0;i<negative.size();++i) near(negative[i], reference[i],2e-12,"Negative PPQ failure");
        const auto longTimeline=render(257,48000,120,1000000);
        for(size_t i=0;i<longTimeline.size();++i) near(longTimeline[i],reference[i],1e-7,"Long-session drift");
        std::cout << "PASS host-grid minima, sample rates, negative PPQ, long sessions, buffer partitions\n";

        for(int shape=0;shape<4;++shape) for(int rate=0;rate<6;++rate) {
            duck::Engine e; e.prepare(48000);
            e.beginBlock({0,120,true,true},{1,shape,rate,false});
            double last=e.nextGain();
            for(int i=1;i<96000;++i) {
                const double gain=e.nextGain();
                require(std::isfinite(gain) && gain>=0 && gain<=1,"Gain could clip audio");
                require(std::abs(gain-last)<0.012,"Discontinuous periodic envelope");
                last=gain;
            }
        }
        for(int shape=0;shape<4;++shape) {
            near(duck::envelope(0,shape,0.006),0,0,"Shape starts above silence");
            near(duck::envelope(1,shape,0.006),0,0,"Shape end discontinuity");
            near(duck::envelope(0.994,shape,0.006),1,1e-12,"Shape truncates recovery");
        }
        std::cout << "PASS four shapes, all rates, bounded gain and continuous beat boundaries\n";

        duck::Engine loop; loop.prepare(48000);
        loop.beginBlock({3.99,120,true,true,true,1.25,4.0},{1,0,1,false});
        double loopLast=0;
        for(int i=0;i<240;++i) loopLast=loop.nextGain();
        near(loop.nextGain(),loopLast,1e-11,"Fractional loop clicks");
        near(loop.phase(),0.25,1e-11,"In-buffer loop didn't wrap phase");
        loop.beginBlock({1.25+1.0/24000,120,true,true,true,1.25,4.0},{1,0,1,false});
        for(int i=0;i<96;++i) loop.nextGain();
        near(loop.phase(),0.254,1e-11,"Loop transition delays phase");
        near(loop.nextGain(),duck::envelope(0.25+97.0/24000,0,0.006),1e-11,"Loop transition never completes");
        duck::Engine tempo; tempo.prepare(48000);
        tempo.beginBlock({0,120,true,true},{1,0,1,false});
        for(int i=0;i<12000;++i) tempo.nextGain();
        tempo.beginBlock({0.5,60,true,true},{1,0,1,false});
        for(int i=0;i<24000;++i) tempo.nextGain();
        near(tempo.nextGain(),0,1e-12,"Tempo change missed beat");
        duck::Engine tempoEdge; tempoEdge.prepare(48000);
        tempoEdge.beginBlock({0.997-1.0/24000,120,true,true},{1,0,1,false});
        const double beforeTempo=tempoEdge.nextGain();
        tempoEdge.beginBlock({0.997,60,true,true},{1,0,1,false});
        const double afterTempo=tempoEdge.nextGain();
        near(afterTempo,0.5,1e-12,"Tempo width is not smoothed independently of phase");
        require(std::abs(afterTempo-beforeTempo)<0.012,"Tempo change clicks in pre-duck ramp");
        for(int blockSize : {1,64,240,511,4096}) for(int rate : {1,5}) {
            duck::Engine partitioned; partitioned.prepare(48000);
            duck::Engine single; single.prepare(48000);
            single.beginBlock({3.99,120,true,true,true,1.25,4},{1,0,rate,false});
            for(int offset=0;offset<96000;) {
                const double absolute=3.99+offset/24000.0;
                const double position=absolute<4 ? absolute : 1.25+std::fmod(absolute-1.25,2.75);
                partitioned.beginBlock({position,120,true,true,true,1.25,4},{1,0,rate,false});
                const int n=std::min(blockSize,96000-offset);
                for(int i=0;i<n;++i) { const auto a=partitioned.nextGain(),b=single.nextGain(); if(std::abs(a-b)>2e-11) std::cerr << "block=" << blockSize << " rate=" << rate << " sample=" << offset+i << " a=" << a << " b=" << b << " phase=" << partitioned.phase() << " refPhase=" << single.phase() << "\n"; near(a,b,2e-11,"Loop transition depends on buffer partition"); }
                offset+=n;
            }
        }
        duck::Engine integerLoop; integerLoop.prepare(48000);
        integerLoop.beginBlock({3.99,120,true,true,true,0,4},{1,0,1,false});
        for(int i=0;i<240;++i) integerLoop.nextGain();
        near(integerLoop.nextGain(),0,1e-12,"Whole-beat loop minimum is smeared");
        std::cout << "PASS in-buffer loops, fractional loop points, tempo changes\n";

        duck::Engine e; e.prepare(48000);
        e.beginBlock({0,120,true,true},{0,0,1,false});
        for(int i=0;i<1000;++i) near(e.nextGain(),1,0,"Zero amount isn't transparent");
        e.beginBlock({1000.0/24000,120,true,true},{1,3,3,false});
        double previous=1;
        for(int i=0;i<1000;++i) {const auto gain=e.nextGain(); require(std::abs(gain-previous)<0.02,"Automation clicks"); previous=gain;}
        e.beginBlock({9.0,120,true,true},{1,3,3,false});
        near(e.nextGain(),previous,1e-12,"Seek clicks");
        for(int i=1;i<=96;++i) e.nextGain();
        near(e.phase(), std::fmod((9.0+96.0/24000)/0.25,1.0),1e-12,"Seek slews clock phase");
        e.beginBlock({0,120,false,true},{1,3,3,false});
        for(int i=0;i<1000;++i) e.nextGain();
        near(e.nextGain(),1,0,"Stopped host remains ducked");
        for(double bad : {0.0,-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
            duck::Engine invalid; invalid.prepare(48000);
            invalid.beginBlock({0,bad,true,true},{1,0,1,false});
            near(invalid.nextGain(),1,0,"Invalid BPM not dry");
        }
        duck::Engine absent; absent.prepare(48000);
        absent.beginBlock({0,120,true,false},{1,0,1,false});
        near(absent.nextGain(),1,0,"Missing PPQ runs an unsynced clock");
        std::cout << "PASS zero amount, automation, seek, stop, invalid/missing host timing\n";
    } catch(const std::exception& e) {std::cerr << "FAIL: " << e.what() << '\n'; return 1;}
}
