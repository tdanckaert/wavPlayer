#ifndef wave_h
#define wave_h

#include <vector>

class QString;

class Wave {
  
 public:
 Wave(std::vector<float> samples, unsigned int channels, unsigned int samplerate) :
   channels(channels), samplerate(samplerate), samples(std::move(samples) ) {};
  
  const unsigned int channels;
  const unsigned int samplerate;
  const std::vector<float> samples;
};
#endif
