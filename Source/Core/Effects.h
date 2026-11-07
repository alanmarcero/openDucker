#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace duck {
struct Stereo { double left=0, right=0; };
struct EffectsParameters {
 bool delayEnabled=true;
 double delaySeconds=0.5, feedback=0.35, delayBassDb=0, delayTrebleDb=0;
 bool pingPong=true;
 bool reverbEnabled=true;
 int reverbType=1;
 double reverbSeconds=6, reverbWidth=1, reverbBassDb=0, reverbTrebleDb=0;
 double sustain=0, destroy=0;
 bool parallel=false, destroyAlways=false;
};
class Effects {
public:
 void prepare(double sampleRate);
 void reset() noexcept;
 void setParameters(const EffectsParameters&) noexcept;
 Stereo process(Stereo input,double sourcePeak) noexcept;
 double tailSeconds() const noexcept;
 double destroyActivity() const noexcept;
private:
 struct Line {
  std::vector<double> samples;
  size_t position=0,valid=0;
  double read() const noexcept { return valid==samples.size()?samples[position]:0; }
  double read(double delay) const noexcept;
  void write(double x) noexcept {
   samples[position]=x;
   if(valid<samples.size()) ++valid;
   if(++position==samples.size()) position=0;
  }
  void clear() noexcept;
 };
 struct Shelf {
  std::array<double,5> coefficients{1,0,0,0,0}, target{1,0,0,0,0};
  std::array<double,2> z1{},z2{};
  void configure(double rate,double hz,double db,bool high,bool immediate) noexcept;
  Stereo process(Stereo,double smoothing) noexcept;
  void clear() noexcept;
 };
 struct Reverb {
  std::array<Line,8> lines;
  std::array<Line,8> diffusers;
  std::array<double,8> lengths{}, gains{}, targetGains{}, poles{}, targetPoles{}, dampingState{}, phases{}, phaseSteps{};
  double diffusion=.65, highRatio=.5, depth=0;
  void prepare(double rate,int type);
  void configure(double rate,double seconds,bool immediate) noexcept;
  Stereo process(Stereo input,double smoothing) noexcept;
  void clear() noexcept;
 };
 Stereo delay(Stereo) noexcept;
 Stereo reverb(Stereo) noexcept;
 Stereo compress(Stereo) noexcept;
 Stereo distort(Stereo,double sourcePeak) noexcept;
 double delayRead(const std::vector<double>&,double samples) const noexcept;
 EffectsParameters parameters;
 std::vector<double> delayLeft,delayRight;
 std::vector<Stereo> stutter;
 std::array<Reverb,3> reverbs;
 Shelf delayBass,delayTreble,reverbBass,reverbTreble;
 std::array<double,3> presetMix{0,1,0};
 double rate=48000,smoothing=0,detectorAttack=0,detectorRelease=0;
 double delaySamples=24000,oldDelaySamples=24000,tapFade=1,tapStep=0,targetDelaySamples=24000;
 double feedback=.35,pingPong=1,width=1,sustain=0,destroy=0,delayMix=1,reverbMix=1;
 double parallelMix=0;
 double envelope=0,compressionGain=1,activity=0,quietSeconds=0,tailEstimate=6;
 double destroyEnvelope=0,gainSmoothing=0,activitySmoothing=0,destroyRelease=0;
 double quietHold=.08,destroyAttackSmoothing=0;
 double makeup=1,makeupTarget=1,compressionExponent=0,compressionExponentTarget=0;
 Stereo held{};
 size_t delayPosition=0,stutterPosition=0,delayValid=0,stutterValid=0;
 uint64_t clock=0;
 bool initialized=false,prepared=false,sourceActive=false,phraseSeen=false;
};
}
