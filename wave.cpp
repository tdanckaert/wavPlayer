#include "wave.h"

#include <stdio.h>

#include <sndfile.hh>

#include <stdexcept>

#include <QString>
#include <QDebug>

using std::vector;

Wave Wave::openSoundFile(const QString& fileName) {

  // create a "Sndfile" handle, it's part of the sndfile library we 
  // use to load samples
  SndfileHandle fileHandle( fileName.toUtf8().data() , SFM_READ,  SF_FORMAT_WAV | SF_FORMAT_FLOAT , 1 , 44100);
  
  // get the number of frames in the sample
  int size  = fileHandle.frames();

  // handle size 0?
  if(!size)
    throw std::runtime_error("Error opening file.");
  
  // get some more info of the sample
  int channels = fileHandle.channels();
  int samplerate = fileHandle.samplerate();
  
  //  result.resize(channels*size);
  vector<float> samples(channels*size);
  
  // this tells sndfile to 
  fileHandle.read( &samples.at(0) , samples.size() );
  
  qDebug() << "Loaded a file with " << channels << " channels, and a samplerate of " <<
    samplerate << " with " << size << " samples, so its duration is " <<
    size / samplerate << " seconds long.";
  
  return Wave(std::move(samples), channels);
}
