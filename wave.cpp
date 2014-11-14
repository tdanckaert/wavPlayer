#include "wave.h"

#include <stdio.h>

#include <sndfile.hh>
#include <mpg123.h>

#include <QString>
#include <QDebug>

#include <string>
#include <stdexcept>
#include <vector>

using std::vector;
using std::string;

class Mpg123Handle {
public:
  Mpg123Handle(const QString & fileName) {
    auto err = mpg123_init();
    if (err != MPG123_OK || (mh = mpg123_new(NULL, &err)) == NULL)
      throw std::runtime_error("Can't initialize mpg123."); 

    mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_FORCE_FLOAT, 0.);

    if (mpg123_open(mh, fileName.toLatin1().constData()) != MPG123_OK
        || mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK )
      throw std::runtime_error(string("mpg123 error: ") + mpg123_strerror(mh));

    mpg123_format_none(mh);
    mpg123_format(mh, rate, channels, encoding);
  };

  ~Mpg123Handle(void) {
    // TODO: error checking?
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
  };

  vector<float> read(void) {
    vector<float> result;
    vector<float> buffer(mpg123_outblock(mh)/sizeof(buffer[0]));

    int err = MPG123_OK;
    size_t done = 0;
    do {
      err = mpg123_read(mh, reinterpret_cast<unsigned char*>(&buffer[0]),
                        buffer.size()*sizeof(buffer[0]), &done);
      for(size_t i=0; i<done/sizeof(buffer[0]); ++i) {
        result.push_back(buffer[i]);
      }
    } while (err==MPG123_OK || err== MPG123_NEED_MORE);

    if(err != MPG123_DONE)
      throw std::runtime_error(string("Warning, mpg123 decoding ended prematurely: ") +
                               (err == MPG123_ERR ? mpg123_strerror(mh) : mpg123_plain_strerror(err)) );
    return result;
  };

  int getChannels() { return channels; };

  int getSamplerate() { return rate; };

private:
  mpg123_handle *mh;
  int channels, encoding;
  long rate;  
};

Wave Wave::openSoundFile(const QString& fileName) {

  // try to create a "Sndfile" handle
  SndfileHandle fileHandle( fileName.toUtf8().data() , SFM_READ,  SF_FORMAT_WAV | SF_FORMAT_FLOAT , 1 , 44100);
  // get the number of frames in the sample
  int size  = fileHandle.frames();

  if(!size) { // if libsndfile reports size 0, try opening as mp3
    Mpg123Handle mp3(fileName);
    return Wave(mp3.read(), mp3.getChannels(), mp3.getSamplerate());
  } else { // open using libsndfile
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
    
    return Wave(std::move(samples), channels, samplerate);
  }
}
