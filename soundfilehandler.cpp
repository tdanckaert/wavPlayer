#include "soundfilehandler.h"

#include <sndfile.hh>
#include <mpg123.h>

#include <QString>

#include <string>
#include <stdexcept>
#include <vector>
#include <memory>

using std::vector;
using std::string;

using std::unique_ptr;

auto mpg123_deleter = [] (mpg123_handle *h) {
  mpg123_close(h);
  mpg123_delete(h);
};

typedef unique_ptr<mpg123_handle, decltype(mpg123_deleter)> mpg123_ptr;

static mpg123_ptr make_mpg123_ptr() {
  int err;
  auto handle = mpg123_ptr(mpg123_new(NULL, &err), mpg123_deleter);
  if (handle.get() == NULL)
    throw std::runtime_error("Can't allocate mpg123 handle.");

  mpg123_param(handle.get(), MPG123_ADD_FLAGS, MPG123_FORCE_FLOAT, 0.);

  return handle;
}

static Wave read_mp3(const QString & fileName) {
  
  vector<float> samples;
  int channels, encoding;
  long rate;  
  
  auto handle = make_mpg123_ptr();
  if (mpg123_open(handle.get(), fileName.toLatin1().constData()) != MPG123_OK
      || mpg123_getformat(handle.get(), &rate, &channels, &encoding) != MPG123_OK )
    throw std::runtime_error(string("mpg123 error: ") + mpg123_strerror(handle.get()));
  
  mpg123_format_none(handle.get());
  mpg123_format(handle.get(), rate, channels, encoding);
  
  vector<float> buffer(mpg123_outblock(handle.get())/sizeof(buffer[0]));
  
  int err = MPG123_OK;
  size_t done = 0;
  do {
    err = mpg123_read(handle.get(), reinterpret_cast<unsigned char*>(&buffer[0]),
                      buffer.size()*sizeof(buffer[0]), &done);
    for(size_t i=0; i<done/sizeof(buffer[0]); ++i) {
      samples.push_back(buffer[i]);
    }
  } while (err==MPG123_OK || err== MPG123_NEED_MORE);
  
  if(err != MPG123_DONE) {
    throw std::runtime_error(string("Warning, mpg123 decoding ended prematurely: ") +
                             (err == MPG123_ERR ? mpg123_strerror(handle.get()) : mpg123_plain_strerror(err)) );
  }
  return Wave(std::move(samples), channels, rate);
}

SoundFileHandler::SoundFileHandler() {
  auto err = mpg123_init();

  if (err != MPG123_OK)
    throw std::runtime_error("Can't initialize mpg123.");
}

SoundFileHandler::~SoundFileHandler() {
  mpg123_exit();
}

Wave SoundFileHandler::read(const QString& fileName) const {

  // try to create a "Sndfile" handle
  SndfileHandle fileHandle( fileName.toUtf8().data() , SFM_READ,  SF_FORMAT_WAV | SF_FORMAT_FLOAT , 1 , 44100);
  // get the number of frames in the sample
  int size  = fileHandle.frames();

  if(!size) { // if libsndfile reports size 0, try opening as mp3
    return read_mp3(fileName);
  } else { // open using libsndfile
    // get some more info of the sample
    int channels = fileHandle.channels();
    int samplerate = fileHandle.samplerate();
    
    //  result.resize(channels*size);
    vector<float> samples(channels*size);
  
    fileHandle.read(samples.data(), samples.size() );
  
    return Wave(std::move(samples), channels, samplerate);
  }
}
