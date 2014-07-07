#ifndef wave_h
#define wave_h

#include <vector>

class QString;

class Wave {
  
public:
  Wave(std::vector<float> samples, unsigned int channels) :
    channels(channels),  samples(samples) {}
  
  bool operator<(const Wave& rhs) const 
  {
    return samples < rhs.samples;
  };

  static Wave openSoundFile(const QString& fileName);
  
  const unsigned int channels;
  const std::vector<float> samples;
};

#endif
