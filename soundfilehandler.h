#ifndef soundfilehandler_h
#define soundfilehandler_h

#include "wave.h"

class SoundFileHandler {

public:
  SoundFileHandler();
  ~SoundFileHandler();

  Wave read(const QString& fileName) const;
};

#endif
