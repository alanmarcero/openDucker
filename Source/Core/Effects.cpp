#include "Effects.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace duck {
namespace {
constexpr double pi=std::numbers::pi;
double bounded(double x,double lo,double hi,double fallback) noexcept {
 return std::isfinite(x)?std::clamp(x,lo,hi):fallback;
}
double lerp(double a,double b,double t) noexcept { return a+(b-a)*t; }
double clean(double x) noexcept { return std::isfinite(x)&&std::abs(x)>1e-30?x:0; }
Stereo mix(Stereo a,Stereo b,double t) noexcept { return {lerp(a.left,b.left,t),lerp(a.right,b.right,t)}; }
}

// Reset validity instead of touching storage: audio-thread reset cost must not grow with sample rate.
void Effects::Line::clear() noexcept { position=valid=0; }

void Effects::Shelf::configure(double rate,double hz,double db,bool high,bool immediate) noexcept {
 // RBJ shelves: reciprocal boost/cut responses, with a gentle slope of one.
 const double a=std::pow(10.0,db/40),w=2*pi*std::min(hz,rate*.4)/rate;
 const double c=std::cos(w),s=std::sin(w),beta=s*std::sqrt(2*a);
 double b0,b1,b2,a0,a1,a2;
 if(high) {
  b0=a*((a+1)+(a-1)*c+beta); b1=-2*a*((a-1)+(a+1)*c); b2=a*((a+1)+(a-1)*c-beta);
  a0=(a+1)-(a-1)*c+beta; a1=2*((a-1)-(a+1)*c); a2=(a+1)-(a-1)*c-beta;
 } else {
  b0=a*((a+1)-(a-1)*c+beta); b1=2*a*((a-1)-(a+1)*c); b2=a*((a+1)-(a-1)*c-beta);
  a0=(a+1)+(a-1)*c+beta; a1=-2*((a-1)+(a+1)*c); a2=(a+1)+(a-1)*c-beta;
 }
 target={b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};
 if(immediate) coefficients=target;
}
Stereo Effects::Shelf::process(Stereo input,double smoothing) noexcept {
 for(size_t i=0;i<5;++i) coefficients[i]=lerp(coefficients[i],target[i],smoothing);
 std::array<double,2> x{input.left,input.right};
 for(size_t i=0;i<2;++i) {
  const double y=coefficients[0]*x[i]+z1[i];
  z1[i]=clean(coefficients[1]*x[i]-coefficients[3]*y+z2[i]);
  z2[i]=clean(coefficients[2]*x[i]-coefficients[4]*y); x[i]=y;
 }
 return {x[0],x[1]};
}
void Effects::Shelf::clear() noexcept { z1={}; z2={}; coefficients=target; }

void Effects::Reverb::prepare(double rate,int type) {
 // Incommensurate paths and allpass diffusion give each space its own density and texture.
 constexpr double delays[3][8]={
  {.0173,.0199,.0233,.0277,.0311,.0347,.0383,.0419},
  {.0311,.0377,.0437,.0533,.0617,.0713,.0799,.0893},
  {.0113,.0137,.0179,.0211,.0253,.0299,.0343,.0397}};
 constexpr double diff[3][4]={ {.0031,.0047,.0067,.0091}, {.0043,.0061,.0097,.0137}, {.0023,.0037,.0059,.0083} };
 for(size_t i=0;i<8;++i) lines[i].samples.assign(static_cast<size_t>(std::max(1.0,std::round(delays[type][i]*rate))),0);
 for(size_t i=0;i<4;++i) diffusers[i].samples.assign(static_cast<size_t>(std::max(1.0,std::round(diff[type][i]*rate))),0);
 diffusion=type==0?.58:type==1?.68:.76;
 damping=std::exp(-2*pi*(type==0?6500:type==1?9000:14000)/rate);
 clear();
}
void Effects::Reverb::configure(double rate,double seconds,bool immediate) noexcept {
 for(size_t i=0;i<8;++i) {
  targetGains[i]=std::pow(10.0,-3*static_cast<double>(lines[i].samples.size())/(rate*seconds));
  if(immediate) gains[i]=targetGains[i];
 }
}
Stereo Effects::Reverb::process(Stereo input,double smoothing) noexcept {
 std::array<double,2> diffuse{input.left,input.right};
 for(size_t i=0;i<4;++i) {
  double& x=diffuse[i%2]; auto& line=diffusers[i]; const double delayed=line.read();
  const double y=delayed-diffusion*x; line.write(clean(x+diffusion*y)); x=y;
 }
 std::array<double,8> y{};
 double sum=0,left=0,right=0;
 constexpr std::array<double,8> rightSigns{1,-1,1,-1,-1,1,-1,1};
 constexpr std::array<double,8> injectSigns{1,1,-1,-1,1,1,-1,-1};
 for(size_t i=0;i<8;++i) {
  const double x=lines[i].read();
  // Mild damping keeps the midband RT60 calibrated while progressively softening the top end.
  dampingState[i]=clean(lerp(x,dampingState[i],damping));
  y[i]=.999*x+.001*dampingState[i]; sum+=y[i];
  left+=x; right+=x*rightSigns[i];
  gains[i]=lerp(gains[i],targetGains[i],smoothing);
 }
 const double mid=(diffuse[0]+diffuse[1])*.5,side=(diffuse[0]-diffuse[1])*.5;
 for(size_t i=0;i<8;++i) {
  const double injection=(mid*injectSigns[i]+side*rightSigns[i])*.3535533905932738;
  // Householder feedback is orthogonal; scalar per-line losses set RT60 without positive EQ feedback.
  lines[i].write(clean(injection+gains[i]*(y[i]-.25*sum)));
 }
 return {left*.3535533905932738,right*.3535533905932738};
}
void Effects::Reverb::clear() noexcept {
 for(auto& l:lines) l.clear(); for(auto& l:diffusers) l.clear(); dampingState={}; gains=targetGains;
}

void Effects::prepare(double sampleRate) {
 rate=bounded(sampleRate,8000,384000,48000);
 delayLeft.assign(static_cast<size_t>(rate*8)+4,0); delayRight.assign(delayLeft.size(),0);
 stutter.assign(static_cast<size_t>(rate*.06)+1,{});
 for(int i=0;i<3;++i) reverbs[static_cast<size_t>(i)].prepare(rate,i);
 smoothing=1-std::exp(-1/(rate*.02)); tapStep=1/(rate*.03);
 gainSmoothing=1-std::exp(-1/(rate*.003)); activitySmoothing=1-std::exp(-1/(rate*.01)); destroyRelease=std::exp(-1/(rate*.03));
 detectorAttack=std::exp(-1/(rate*.001)); detectorRelease=std::exp(-1/(rate*.12));
 prepared=true; initialized=false; setParameters(parameters); reset();
}
void Effects::setParameters(const EffectsParameters& p) noexcept {
 parameters=p;
 parameters.delaySeconds=bounded(p.delaySeconds,1/rate,8,.5);
 parameters.feedback=bounded(p.feedback,0,.95,.35);
 parameters.delayBassDb=bounded(p.delayBassDb,-12,12,0); parameters.delayTrebleDb=bounded(p.delayTrebleDb,-12,12,0);
 parameters.reverbType=std::clamp(p.reverbType,0,2);
 parameters.reverbSeconds=bounded(p.reverbSeconds,.2,25,6); parameters.reverbWidth=bounded(p.reverbWidth,0,1,1);
 parameters.reverbBassDb=bounded(p.reverbBassDb,-12,12,0); parameters.reverbTrebleDb=bounded(p.reverbTrebleDb,-12,12,0);
 parameters.sustain=bounded(p.sustain,0,1,0); parameters.destroy=bounded(p.destroy,0,1,0);
 const double nextDelay=parameters.delaySeconds*rate;
 targetDelaySamples=nextDelay;
 if(!initialized) { delaySamples=oldDelaySamples=nextDelay; tapFade=1; }
 else if(nextDelay!=delaySamples&&tapFade>=1) {
  oldDelaySamples=delaySamples; delaySamples=nextDelay; tapFade=0;
 }
 delayBass.configure(rate,250,parameters.delayBassDb,false,!initialized);
 delayTreble.configure(rate,4000,parameters.delayTrebleDb,true,!initialized);
 reverbBass.configure(rate,250,parameters.reverbBassDb,false,!initialized);
 reverbTreble.configure(rate,4000,parameters.reverbTrebleDb,true,!initialized);
 for(auto& r:reverbs) r.configure(rate,parameters.reverbSeconds,!initialized);
 makeupTarget=std::pow(10.0,1.2*parameters.sustain); compressionExponentTarget=1-1/(1+7*parameters.sustain);
 if(!initialized) {
  parallelMix=parameters.parallel?1:0;
  makeup=makeupTarget; compressionExponent=compressionExponentTarget;
  delayMix=parameters.delayEnabled?1:0; reverbMix=parameters.reverbEnabled?1:0;
  feedback=parameters.feedback; pingPong=parameters.pingPong?1:0; width=parameters.reverbWidth;
  sustain=parameters.sustain; destroy=parameters.destroy; presetMix={}; presetMix[static_cast<size_t>(parameters.reverbType)]=1;
 }
 const double delayTail=parameters.delaySeconds*(parameters.feedback>0?1+std::log(.001)/std::log(parameters.feedback):1);
 const double delayLength=parameters.delayEnabled?delayTail:0, reverbLength=parameters.reverbEnabled?parameters.reverbSeconds:0;
 tailEstimate=(parameters.parallel?std::max(delayLength,reverbLength):delayLength+reverbLength)*(1+.4*parameters.sustain);
 // Short tails need a shorter phrase hold and attack so Destroy acts before their audio has ended.
 quietHold=std::min(.08,tailEstimate*.1);
 const double attackSeconds=std::max(1/rate,std::min(.01,tailEstimate*.05));
 destroyAttackSmoothing=1-std::exp(-1/(rate*attackSeconds));
 // prepare establishes the default, but the first caller configuration should start without a ramp.

}
void Effects::reset() noexcept {
 delayValid=stutterValid=0;
 for(auto& r:reverbs) r.clear();
 delayBass.clear(); delayTreble.clear(); reverbBass.clear(); reverbTreble.clear();
 delayPosition=stutterPosition=0; clock=0; held={}; envelope=destroyEnvelope=activity=quietSeconds=0; compressionGain=1;
 sourceActive=phraseSeen=false; initialized=false; setParameters(parameters);
}
double Effects::delayRead(const std::vector<double>& buffer,double samples) const noexcept {
 double at=static_cast<double>(delayPosition)-samples;
 if(at<0) at+=static_cast<double>(buffer.size());
 const size_t first=static_cast<size_t>(at),next=first+1==buffer.size()?0:first+1;
 const double firstSample=first<delayValid?buffer[first]:0;
 const double nextSample=next<delayValid?buffer[next]:0;
 return lerp(firstSample,nextSample,at-static_cast<double>(first));
}
Stereo Effects::delay(Stereo input) noexcept {
 const Stereo a{delayRead(delayLeft,oldDelaySamples),delayRead(delayRight,oldDelaySamples)};
 const Stereo b{delayRead(delayLeft,delaySamples),delayRead(delayRight,delaySamples)};
 const auto delayed=mix(a,b,tapFade); tapFade=std::min(1.0,tapFade+tapStep);
 // Finish each tap crossfade before adopting the latest automated target, avoiding discontinuous composite taps.
 if(tapFade>=1&&targetDelaySamples!=delaySamples) { oldDelaySamples=delaySamples; delaySamples=targetDelaySamples; tapFade=0; }
 const double mid=(input.left+input.right)*.5,side=(input.left-input.right)*.5;
 // The side component sums to zero. A mono source therefore starts left and alternates on repeats.
 const Stereo injection{lerp(mid,mid+side,pingPong),lerp(mid,-side,pingPong)};
 delayLeft[delayPosition]=clean(injection.left+feedback*lerp(delayed.left,delayed.right,pingPong));
 delayRight[delayPosition]=clean(injection.right+feedback*lerp(delayed.right,delayed.left,pingPong));
 if(delayValid<delayLeft.size()) ++delayValid;
 if(++delayPosition==delayLeft.size()) delayPosition=0;
 const auto equalized=delayTreble.process(delayBass.process(delayed,smoothing),smoothing);
 // Fold existing stereo history too; changing injection alone leaves old ping-pong repeats separated.
 const double outputMid=(equalized.left+equalized.right)*.5;
 const double outputSide=(equalized.left-equalized.right)*.5*pingPong;
 return {outputMid+outputSide,outputMid-outputSide};
}
Stereo Effects::reverb(Stereo input) noexcept {
 Stereo output{};
 for(size_t i=0;i<3;++i) {
  presetMix[i]=lerp(presetMix[i],static_cast<int>(i)==parameters.reverbType?1:0,smoothing);
  // Keep every preset warm so switching spaces crossfades actual tails without rebuilding buffers.
  const auto y=reverbs[i].process(input,smoothing);
  output.left+=presetMix[i]*y.left; output.right+=presetMix[i]*y.right;
 }
 const double mid=(output.left+output.right)*.5,side=(output.left-output.right)*.5*width;
 return reverbTreble.process(reverbBass.process({mid+side,mid-side},smoothing),smoothing);
}
Stereo Effects::compress(Stereo input) noexcept {
 const double peak=std::max(std::abs(input.left),std::abs(input.right));
 envelope=clean(lerp(peak,envelope,peak>envelope?detectorAttack:detectorRelease));
 if(sustain<1e-9) return input;
 const double reduction=envelope>.125?std::pow(.125/envelope,compressionExponent):1;
 compressionGain=lerp(compressionGain,makeup*reduction,gainSmoothing);
 return {input.left*compressionGain,input.right*compressionGain};
}
Stereo Effects::distort(Stereo input,double sourcePeak) noexcept {
 sourcePeak=std::isfinite(sourcePeak)?std::abs(sourcePeak):0;
 if(sourcePeak>=.0001) { sourceActive=true; phraseSeen=true; quietSeconds=0; }
 else if(sourcePeak<.00003162277660168379) { quietSeconds+=1/rate; if(quietSeconds>=quietHold) sourceActive=false; }
 const double window=std::min(5.0,tailEstimate),start=tailEstimate-window;
 const double ramp=window>0?std::clamp((quietSeconds-start)/std::min(1.0,window),0.0,1.0):0;
 const double target=parameters.destroyAlways?destroy:(phraseSeen&&!sourceActive?destroy*ramp*ramp*(3-2*ramp):0);
 activity=lerp(activity,target,target>activity?destroyAttackSmoothing:activitySmoothing);
 if(activity<1e-12) activity=0;
 const double peak=std::max(std::abs(input.left),std::abs(input.right));
 destroyEnvelope=clean(std::max(peak,destroyEnvelope*destroyRelease));
 stutter[stutterPosition]=input;
 if(stutterValid<stutter.size()) ++stutterValid;
 if(activity>0&&destroyEnvelope>1e-20) {
  const double scale=std::max(1e-12,destroyEnvelope),drive=1+12*activity;
  const int bits=static_cast<int>(16-12*activity),hold=1+static_cast<int>(23*activity);
  const double steps=static_cast<double>(1<<bits);
  if(clock%static_cast<uint64_t>(hold)==0) {
   const auto crush=[&](double x) { return std::round(std::tanh(x/scale*drive)/std::tanh(drive)*steps)/steps*scale; };
   held={crush(input.left),crush(input.right)};
  }
  // Bound replay to the current decaying envelope; stale samples can never sustain a tail forever.
  const double bound=scale*1.5;
  Stereo wet{std::clamp(held.left,-bound,bound),std::clamp(held.right,-bound,bound)};
  const double phase=static_cast<double>(clock%static_cast<uint64_t>(rate*.12))/(rate*.12);
  if(phase<.25) {
   const size_t back=static_cast<size_t>(rate*.035);
   const size_t index=(stutterPosition+stutter.size()-back)%stutter.size();
   auto repeat=index<stutterValid?stutter[index]:Stereo{}; repeat.left=std::clamp(repeat.left,-bound,bound); repeat.right=std::clamp(repeat.right,-bound,bound);
   const double blend=.5*activity*std::sin(pi*phase/.25);
   wet=mix(wet,repeat,blend);
  }
  input=mix(input,wet,activity);
 } else if(destroyEnvelope==0) held={};
 if(++stutterPosition==stutter.size()) stutterPosition=0;
 ++clock;
 return input;
}
Stereo Effects::process(Stereo input,double sourcePeak) noexcept {
 if(!prepared) return input;
 initialized=true;
 input.left=clean(input.left); input.right=clean(input.right);
 feedback=lerp(feedback,parameters.feedback,smoothing); pingPong=lerp(pingPong,parameters.pingPong?1:0,smoothing);
 if(std::abs(pingPong-(parameters.pingPong?1:0))<1e-8) pingPong=parameters.pingPong?1:0;
 makeup=lerp(makeup,makeupTarget,smoothing); compressionExponent=lerp(compressionExponent,compressionExponentTarget,smoothing);
 width=lerp(width,parameters.reverbWidth,smoothing); sustain=lerp(sustain,parameters.sustain,smoothing); destroy=lerp(destroy,parameters.destroy,smoothing);
 delayMix=lerp(delayMix,parameters.delayEnabled?1:0,smoothing);
 reverbMix=lerp(reverbMix,parameters.reverbEnabled?1:0,smoothing);
 // Warm stages preserve valid current tails across bypass fades and avoid clearing large buffers on the audio thread.
 parallelMix=lerp(parallelMix,parameters.parallel?1:0,smoothing);
 const auto delayed=delay(input);
 const auto reverberated=reverb(mix(input,delayed,delayMix*(1-parallelMix)));
 const auto serial=mix(mix(input,delayed,delayMix),reverberated,reverbMix);
 const double direct=(1-delayMix)*(1-reverbMix);
 const Stereo parallel{delayMix*delayed.left+reverbMix*reverberated.left+direct*input.left,
                       delayMix*delayed.right+reverbMix*reverberated.right+direct*input.right};
 input=mix(serial,parallel,parallelMix);
 return distort(compress(input),sourcePeak);
}
double Effects::tailSeconds() const noexcept { return tailEstimate; }
double Effects::destroyActivity() const noexcept { return activity; }
}
