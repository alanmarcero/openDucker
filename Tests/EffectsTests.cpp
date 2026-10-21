#include "Effects.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void near(double a,double b,double tolerance,const char* message) { require(std::abs(a-b)<=tolerance,message); }
double energy(duck::Stereo x) { return x.left*x.left+x.right*x.right; }
duck::EffectsParameters delayParameters() {
 duck::EffectsParameters p; p.reverbEnabled=false; p.delaySeconds=.01; p.feedback=.5; p.pingPong=false; return p;
}
void delayTests() {
 duck::Effects fx; fx.prepare(48000); auto p=delayParameters(); fx.setParameters(p);
 for(int n=0;n<=1440;++n) {
  auto y=fx.process(n==0 ? duck::Stereo{1,1}:duck::Stereo{},n==0?1:0);
  double expected=n==480?1:n==960?.5:n==1440?.25:0;
  near(y.left,expected,1e-10,"Mono delay sample offset or feedback is wrong");
  near(y.right,y.left,1e-12,"Mono delay is not mono");
 }
 p.pingPong=true; fx.setParameters(p); fx.reset();
 for(int n=0;n<=1440;++n) {
  auto y=fx.process(n==0?duck::Stereo{1,1}:duck::Stereo{},n==0?1:0);
  if(n==480) { near(y.left,1,1e-10,"First ping-pong repeat must start left"); near(y.right,0,1e-10,"First repeat leaks right"); }
  if(n==960) { near(y.right,.5,1e-10,"Ping-pong did not alternate"); near(y.left,0,1e-10,"Second repeat leaks left"); }
 }
 fx.reset(); for(int n=0;n<2000;++n) near(energy(fx.process({},0)),0,0,"Reset retained delay tail");
 std::cout<<"PASS delay timing, feedback, mono, ping-pong, reset\n";
}
double probe(bool reverb,double bass,double treble,double hz) {
 duck::Effects fx; fx.prepare(48000); auto p=delayParameters(); p.feedback=0; p.delaySeconds=.001;
 p.reverbEnabled=reverb; p.delayEnabled=!reverb; p.reverbSeconds=.2;
 p.delayBassDb=p.reverbBassDb=bass; p.delayTrebleDb=p.reverbTrebleDb=treble;
 fx.setParameters(p); double e=0;
 for(int n=0;n<96000;++n) { double v=.01*std::sin(6.283185307179586*hz*n/48000); auto y=fx.process({v,v},std::abs(v)); if(n>=48000) e+=energy(y); }
 return e;
}
void automationTests() {
 duck::Effects fx; fx.prepare(48000); auto p=delayParameters(); p.feedback=0; fx.setParameters(p);
 double previous=0;
 for(int n=0;n<96000;++n) {
  if(n>=24000&&n<48000&&n%64==0) { p.delaySeconds=.03+.02*std::sin(n*.00031); fx.setParameters(p); }
  const double x=.1*std::sin(n*.13); auto y=fx.process({x,x},.1);
  if(n>20000) require(std::abs(y.left-previous)<.025,"Continuous delay automation clicks");
  previous=y.left;
 }
 fx.reset(); p.pingPong=true; p.delaySeconds=.01; fx.setParameters(p);
 double sum=0,side=0;
 for(int n=0;n<2000;++n) { auto y=fx.process(n==0?duck::Stereo{1,-1}:duck::Stereo{},n==0?1:0); sum+=std::abs(y.left+y.right); side+=std::abs(y.left-y.right); }
 require(sum<1e-10&&side>1,"Ping-pong loses signed stereo or mono compatibility");
 for(bool toggleReverb:{false,true}) {
  fx.reset(); p=delayParameters(); p.delaySeconds=.0173; p.delayEnabled=!toggleReverb; p.reverbEnabled=toggleReverb; p.feedback=0; fx.setParameters(p);
  previous=0;
  for(int n=0;n<96000;++n) {
   if(n>24000&&n%791==0) { if(toggleReverb) p.reverbEnabled=!p.reverbEnabled; else p.delayEnabled=!p.delayEnabled; fx.setParameters(p); }
   const double x=.1*std::sin(n*.13); auto y=fx.process({x,x},.1);
   if(n>20000) require(std::abs(y.left-previous)<.025,"Stage enable/disable clicks");
   previous=y.left;
  }
 }
 std::cout<<"PASS delay automation, stage fades and signed stereo compatibility\n";
}
void eqTests() {
 for(bool reverb:{false,true}) {
  const double lo=probe(reverb,0,0,60),hi=probe(reverb,0,0,12000);
  require(probe(reverb,12,0,60)>lo*8,"Bass boost shelf missing");
  require(probe(reverb,-12,0,60)<lo*.15,"Bass cut shelf missing");
  require(probe(reverb,0,12,12000)>hi*8,"Treble boost shelf missing");
  require(probe(reverb,0,-12,12000)<hi*.15,"Treble cut shelf missing");
  require(probe(reverb,12,0,12000)<hi*1.15,"Bass shelf affects treble");
  require(probe(reverb,0,12,60)<lo*1.15,"Treble shelf affects bass");
 }
 std::cout<<"PASS independent boost/cut shelves on both effects\n";
}
std::vector<double> reverbRender(int mode,double seconds,double width,double duration) {
 duck::Effects fx; fx.prepare(48000); duck::EffectsParameters p;
 p.delayEnabled=false; p.reverbType=mode; p.reverbSeconds=seconds; p.reverbWidth=width; fx.setParameters(p);
 std::vector<double> out; out.reserve(static_cast<size_t>(48000*duration));
 for(int n=0;n<static_cast<int>(48000*duration);++n) {
  auto y=fx.process(n==0?duck::Stereo{1,1}:duck::Stereo{},n==0?1:0);
  require(std::isfinite(y.left)&&std::isfinite(y.right),"Nonfinite reverb");
  if(width==0) near(y.left,y.right,1e-12,"Width zero is not mono");
  out.push_back(energy(y));
 }
 return out;
}
void reverbTests() {
 std::array<std::vector<double>,3> renders;
 for(int mode=0;mode<3;++mode) renders[mode]=reverbRender(mode,2,0,1);
 for(int a=0;a<3;++a) for(int b=a+1;b<3;++b) {
  double difference=0; for(size_t i=0;i<renders[a].size();++i) difference+=std::abs(renders[a][i]-renders[b][i]);
  require(difference>.01,"Reverb presets are identical");
 }
 for(int mode=0;mode<3;++mode) {
 const auto longTail=reverbRender(mode,25,1,34);
 std::vector<double> integrated(longTail.size()+1,0);
 for(size_t n=longTail.size();n>0;--n) integrated[n-1]=integrated[n]+longTail[n-1];
 const auto dbAt=[&](double t) { return 10*std::log10(integrated[static_cast<size_t>(48000*t)]/integrated[48000]); };
 const double slope=(dbAt(18)-dbAt(6))/12;
 const double estimated=-60/slope;
 require(estimated>23&&estimated<27,"Measured reverb RT60 is not approximately 25 seconds");
 require(dbAt(20)>-55,"25-second reverb dies before 20 seconds");
 size_t nonzero=0; for(size_t i=24000;i<48000;++i) if(longTail[i]>1e-16) ++nonzero;
 require(nonzero>22000,"Reverb lacks late reflection density");
 std::cout<<"PASS preset "<<mode<<", mono width, density, measured RT60="<<estimated<<" seconds\n";
 }
}
void sustainTests() {
 duck::Effects dry,compressed; dry.prepare(48000); compressed.prepare(48000);
 auto p=delayParameters(); p.delayEnabled=false; p.sustain=0; dry.setParameters(p); p.sustain=1; compressed.setParameters(p);
 double peakDry=0,peakWet=0,lateDry=0,lateWet=0;
 for(int n=0;n<144000;++n) {
  const double x=std::exp(-n/24000.0)*std::sin(n*.13);
  auto a=dry.process({x,x},std::abs(x)),b=compressed.process({x,x},std::abs(x));
  peakDry=std::max(peakDry,std::abs(a.left)); peakWet=std::max(peakWet,std::abs(b.left));
  if(n>96000) { lateDry+=energy(a); lateWet+=energy(b); }
 }
 require(lateWet/(peakWet*peakWet)>lateDry/(peakDry*peakDry)*4,"Sustain does not lift quieter tail at comparable peak");
 compressed.reset(); for(int i=0;i<48000;++i) near(energy(compressed.process({},0)),0,0,"Sustain generates noise from silence");
 std::cout<<"PASS linked sustain tail lift and silence\n";
}
void destroyTests() {
 duck::Effects fx,clean; fx.prepare(48000); clean.prepare(48000); duck::EffectsParameters p;
 p.delayEnabled=false; p.reverbSeconds=25; p.destroy=1; fx.setParameters(p); p.destroy=0; clean.setParameters(p);
 near(fx.tailSeconds(),25,.1,"25-second nominal tail estimate is wrong");
 double difference=0;
 for(int n=0;n<static_cast<int>(24*48000);++n) {
  const auto in=n==0?duck::Stereo{1,1}:duck::Stereo{};
  auto y=fx.process(in,n==0?1:0),z=clean.process(in,n==0?1:0);
  if(n<20*48000) { near(fx.destroyActivity(),0,1e-12,"Destroy starts before final five seconds"); near(y.left,z.left,1e-12,"Destroy zero activity alters audio"); }
  if(n>22*48000) difference+=std::abs(y.left-z.left);
 }
 require(fx.destroyActivity()>.8&&difference>1e-6,"Destroy is not active on final reverb tail");
 for(int n=0;n<4800;++n) fx.process({},.1);
 require(fx.destroyActivity()<.001,"Raw source retrigger did not fade destroy off");
 auto d=delayParameters(); d.delaySeconds=1; d.feedback=.2; fx.setParameters(d); const auto shortTail=fx.tailSeconds();
 d.feedback=.8; fx.setParameters(d); require(fx.tailSeconds()>shortTail*4,"Delay feedback does not change destroy timing");
 duck::Effects shortDelay,longDelay; shortDelay.prepare(48000); longDelay.prepare(48000);
 d.delaySeconds=.2; d.feedback=.2; d.destroy=1; shortDelay.setParameters(d); d.feedback=.8; longDelay.setParameters(d);
 for(int n=0;n<48000;++n) {
  auto input=n==0?duck::Stereo{1,1}:duck::Stereo{}; const double source=n==0?1:0;
  shortDelay.process(input,source); longDelay.process(input,source);
 }
 require(shortDelay.destroyActivity()>.9&&longDelay.destroyActivity()==0,"Delay feedback does not move actual Destroy onset");
 std::cout<<"PASS phrase-tail destroy timing, transparency, source retrigger\n";
}
void shortDestroyTests() {
 for(double seconds:{.001,.01,.05}) {
  duck::Effects destroyed,clean; destroyed.prepare(48000); clean.prepare(48000);
  auto p=delayParameters(); p.delaySeconds=seconds; p.feedback=0; p.destroy=1; destroyed.setParameters(p);
  p.destroy=0; clean.setParameters(p); double difference=0,activityAtRepeat=0;
  for(int n=0;n<static_cast<int>(48000*(seconds+.01));++n) {
   const auto input=n==0?duck::Stereo{1,.5}:duck::Stereo{};
   auto a=destroyed.process(input,n==0?1:0),b=clean.process(input,n==0?1:0);
   if(energy(b)>0) { difference+=std::abs(a.left-b.left)+std::abs(a.right-b.right); activityAtRepeat=destroyed.destroyActivity(); }
  }
  require(activityAtRepeat>.5&&difference>.01,"Destroy misses the audible sub-80-ms delay tail");
 }
 duck::Effects fx; fx.prepare(48000); auto p=delayParameters(); p.delaySeconds=.01; p.feedback=0; p.destroy=1; fx.setParameters(p);
 for(int n=0;n<4800;++n) {
  const double x=.2*std::sin(n*2*3.141592653589793*80/48000);
  fx.process({x,x},std::abs(x)); near(fx.destroyActivity(),0,0,"Short-tail detector treats ordinary zero crossings as a phrase end");
 }
 std::cout<<"PASS Destroy during audible short tails and zero-crossing hold\n";
}
void monoHistoryTests() {
 duck::Effects fx; fx.prepare(48000); auto p=delayParameters(); p.delaySeconds=.2; p.feedback=.9; p.pingPong=true; fx.setParameters(p);
 double tailEnergy=0;
 for(int n=0;n<48000;++n) {
  if(n==4800) { p.pingPong=false; fx.setParameters(p); }
  auto y=fx.process(n==0?duck::Stereo{1,1}:duck::Stereo{},n==0?1:0);
  if(n>=14400) { near(y.left,y.right,n>=24000?0:.0001,"Mono leaves stored ping-pong history stereo"); tailEnergy+=energy(y); }
 }
 require(tailEnergy>.01,"Mono transition erased the stored tail");
 fx.reset(); p.pingPong=true; p.delaySeconds=.013; p.feedback=.8; fx.setParameters(p); double previous=0;
 for(int n=0;n<48000;++n) {
  if(n==24000) { p.pingPong=false; fx.setParameters(p); }
  const double x=.03*std::sin(n*.037); auto y=fx.process({x,x},std::abs(x));
  if(n>10000) require(std::abs(y.left-previous)<.01,"Mono transition clicks with stored audio");
  previous=y.left;
 }
 std::cout<<"PASS Mono fold-down of stored tails and transition continuity\n";
}
void resetHistoryTests() {
 // Eight-second wrap coverage at 8 kHz keeps the full stale-history check fast.
 duck::Effects fx,fresh; fx.prepare(8000); fresh.prepare(8000); auto p=delayParameters();
 p.delaySeconds=8; p.feedback=.8; p.reverbEnabled=true; p.reverbSeconds=.2; p.sustain=.5; p.destroy=1;
 fx.setParameters(p); fresh.setParameters(p);
 for(int n=0;n<65000;++n) fx.process({.1*std::sin(n*.13),.1*std::cos(n*.07)},.1);
 fx.reset();
 for(int n=0;n<65000;++n) {
  auto y=fx.process({},0); near(y.left,0,0,"Reset replays stale left audio after a buffer wrap"); near(y.right,0,0,"Reset replays stale right audio after a buffer wrap");
 }
 // Compare fresh and reset states with new input, including fractional taps and Destroy replay.
 for(double seconds:{.0100625,.2,8.0}) {
  p.delaySeconds=seconds; fx.setParameters(p); fresh.setParameters(p);
  for(int n=0;n<1000;++n) fx.process({.2*std::sin(n*.17),.1*std::cos(n*.23)},.2);
  fx.reset(); fresh.reset();
  for(int n=0;n<65000;++n) {
   auto input=n<31?duck::Stereo{.2*std::sin(n*.7),-.1*std::cos(n*.3)}:duck::Stereo{};
   auto a=fx.process(input,n<31?.2:0),b=fresh.process(input,n<31?.2:0);
   near(a.left,b.left,0,"Logical reset differs from fresh left state"); near(a.right,b.right,0,"Logical reset differs from fresh right state");
  }
 }
 std::cout<<"PASS reset discards every stale buffer and matches fresh processing\n";
}
void robustnessTests() {
 for(double sr:{44100.,48000.,96000.,192000.}) {
  duck::Effects fx; fx.prepare(sr); duck::EffectsParameters p;
  p.delaySeconds=.003; p.feedback=.95; p.reverbSeconds=25; p.delayBassDb=p.delayTrebleDb=p.reverbBassDb=p.reverbTrebleDb=12; p.sustain=p.destroy=1;
  fx.setParameters(p); double renderedEnergy=0;
  for(int n=0;n<static_cast<int>(sr);++n) {
   if(n==static_cast<int>(sr/2)) { p.delaySeconds=0; p.reverbType=2; fx.setParameters(p); }
   auto y=fx.process(n<100?duck::Stereo{1,-.5}:duck::Stereo{},n<100?1:0);
   require(std::isfinite(y.left)&&std::isfinite(y.right)&&std::abs(y.left)<1000&&std::abs(y.right)<1000,"Extreme settings unstable");
   renderedEnergy+=energy(y);
  }
  require(renderedEnergy>.01,"Extreme-controls probe never reached the output");
  fx.reset(); for(int n=0;n<1000;++n) near(energy(fx.process({},0)),0,0,"Reset retained effects tail");
  p.delaySeconds=p.feedback=p.reverbSeconds=p.sustain=p.destroy=std::numeric_limits<double>::quiet_NaN(); fx.setParameters(p);
  for(int n=0;n<1000;++n) require(std::isfinite(energy(fx.process({.1,.2},.2))),"Invalid parameter causes NaN");
 }
 duck::Effects a,b; a.prepare(48000); b.prepare(48000); duck::EffectsParameters p; p.delaySeconds=.013; p.destroy=.5; p.sustain=.7; a.setParameters(p); b.setParameters(p);
 for(int n=0;n<48000;++n) {
  if(n%127==0) b.setParameters(p);
  auto input=duck::Stereo{std::sin(n*.137)*.01,std::cos(n*.081)*.02};
  auto x=a.process(input,.02),y=b.process(input,.02);
  near(x.left,y.left,1e-12,"Parameter refresh/block partition changes output"); near(x.right,y.right,1e-12,"Stereo block partition changes output");
 }
 std::cout<<"PASS sample rates, extreme/invalid controls, reset, parameter partition invariance\n";
}
}
int main(int argc,char** argv) { try {
 if(argc>1) {
  const std::string_view test=argv[1];
  if(test=="short-destroy") shortDestroyTests();
  else if(test=="mono-history") monoHistoryTests();
  else if(test=="reset-history") resetHistoryTests();
  else require(false,"Unknown test group");
 } else { delayTests(); automationTests(); eqTests(); reverbTests(); sustainTests(); destroyTests(); shortDestroyTests(); monoHistoryTests(); resetHistoryTests(); robustnessTests(); }
 } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; } }
