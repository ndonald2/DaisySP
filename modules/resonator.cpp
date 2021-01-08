//#include "stmlib/dsp/cosine_oscillator.h"
//#include <algorithm>
//#include "stmlib/dsp/units.h"
//#include "plaits/resources.h"

#include "resonator.h"
#include <math.h>

using namespace daisysp;

void Resonator::Init(float position, int resolution, float sample_rate) {
	sample_rate_ = sample_rate;

	SetFreq(440.f);
	SetStructure(.5f);
	SetBrightness(.5f);
	SetDamping(.5f);

  resolution_ = fmin(resolution, kMaxNumModes);
    
  for (int i = 0; i < resolution; ++i) {
    mode_amplitude_[i] = cos(position * TWOPI_F) * 0.25f;
  }
  
  for (int i = 0; i < kMaxNumModes / kModeBatchSize; ++i) {
    mode_filters_[i].Init();
  }
}

inline float NthHarmonicCompensation(int n, float stiffness) {
  float stretch_factor = 1.0f;
  for (int i = 0; i < n - 1; ++i) {
    stretch_factor += stiffness;
    if (stiffness < 0.0f) {
      stiffness *= 0.93f;
    } else {
      stiffness *= 0.98f;
    }
  }
  return 1.0f / stretch_factor;
}

float Resonator::Process(const float in) {
  //convert Hz to cycles / sample		
  float out = 0.f;

  float stiffness = CalcStiff(structure_);  
  float f0 = frequency_ * NthHarmonicCompensation(3, stiffness);
  float brightness = brightness_;
    
  float harmonic = f0;
  float stretch_factor = 1.0f;
  
  float input = damping_ * 79.7f;
  float ratio = powf(2.f, (float)(int)input * ratiofrac_);
  input = input - (float)(int)input;
  float semitone =  powf(2.f, (input * 256.f) * semitonefrac_);
  float q_sqrt = ratio * semitone;
  //float q_sqrt = SemitonesToRatio(damping * 79.7f);
  
  float q = 500.0f * q_sqrt * q_sqrt;
  brightness *= 1.0f - structure_ * 0.3f;
  brightness *= 1.0f - damping_ * 0.3f;
  float q_loss = brightness * (2.0f - brightness) * 0.85f + 0.15f;
  
  float mode_q[kModeBatchSize];
  float mode_f[kModeBatchSize];
  float mode_a[kModeBatchSize];
  int batch_counter = 0;
  
  ResonatorSvf<kModeBatchSize>* batch_processor = &mode_filters_[0];
  
  
  for (int i = 0; i < resolution_; ++i) {
    float mode_frequency = harmonic * stretch_factor;
    if (mode_frequency >= 0.499f) {
      mode_frequency = 0.499f;
    }
    const float mode_attenuation = 1.0f - mode_frequency * 2.0f;
    
    mode_f[batch_counter] = mode_frequency;
    mode_q[batch_counter] = 1.0f + mode_frequency * q;
    mode_a[batch_counter] = mode_amplitude_[i] * mode_attenuation;
    ++batch_counter;
    
    if (batch_counter == kModeBatchSize) {
      batch_counter = 0;
      batch_processor->Process<FILTER_MODE_BAND_PASS, true>(
          mode_f,
          mode_q,
          mode_a,
          in,
          &out,
          1);
      ++batch_processor;
    }
    
    stretch_factor += stiffness;
    if (stiffness < 0.0f) {
      // Make sure that the partials do not fold back into negative frequencies.
      stiffness *= 0.93f;
    } else {
      // This helps adding a few extra partials in the highest frequencies.
      stiffness *= 0.98f;
    }
    harmonic += f0;
    q *= q_loss;	
  }
  
  	return out;
}

 void Resonator::SetFreq(float freq){
	frequency_ = freq / sample_rate_;
 }
 
 void Resonator::SetStructure(float structure){
	structure_ = structure;
 }
 
  void Resonator::SetBrightness(float brightness){
	brightness_ = brightness;
  }

  void Resonator::SetDamping(float damping){
	damping_ = damping;
  }

float Resonator::CalcStiff(float sig){
	if(sig < .25f){
		sig = .25 - sig;
		sig = -sig * .25;
	}
	else if(sig < .3f){
		sig = 0.f;
	}
	else if(sig < .9f){
		sig -= .3f;
		sig *= 1.66666666f; // approx div by .6
	}
	else{
		sig -= .9f;
		sig *= 10; // div by .1
		sig *= sig;
		sig = 1.5 - cos(sig * PI_F) * .5f;
	}
	return sig;
}