#ifndef wave_h
#define wave_h

#include <vector>

class Wave {

 public:
 Wave(std::vector<float> samples, unsigned int channels) :
  channels(channels),  samples(samples) {}
  
  bool operator<(const Wave& rhs) const 
  {
    return samples < rhs.samples;  //assume that you compare the record based on a
  };

  const unsigned int channels;
  const std::vector<float> samples;
};

#endif
