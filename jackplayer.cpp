#include "jackplayer.h"
#include "wave.h"

#include <vector>
#include <iostream>

#include <QTimer>
#include <QDebug>

#include <sndfile.hh>
//#include <jack/jack.h>
#include <jack/ringbuffer.h>

using std::set;
using std::cerr;
using std::endl;

enum PlayState {
  PLAYING,
  STOPPED
};

enum PlayEvent {
  PAUSE
};

static bool haveSample;
static set<Wave>::iterator curSample;
static jack_port_t *outputPort;
static jack_client_t *client;
static unsigned int playbackIndex;
static PlayState state;
static jack_ringbuffer_t *eventBuffer;

static jack_ringbuffer_t *inQueue; // samples in
static jack_ringbuffer_t *outQueue; // samples out, can be freed

JackPlayer::JackPlayer(QObject *parent) : QObject(parent)
 {

  playbackIndex = 0;
  outputPort = nullptr;

  playbackIndex = 0;
  outputPort = nullptr;
  
  client = jack_client_open("wavPlayer", JackNullOption, 0 , 0);
  if (client == nullptr) {
    cerr << __func__ << " client failed!" << endl;
  }

  eventBuffer = jack_ringbuffer_create(sizeof(PlayEvent)*10);
  jack_ringbuffer_mlock(eventBuffer);

  inQueue = jack_ringbuffer_create(sizeof(set<Wave>::iterator)*10);
  jack_ringbuffer_mlock(inQueue);
  outQueue = jack_ringbuffer_create(sizeof(set<Wave>::iterator)*10);
  jack_ringbuffer_mlock(outQueue);

  startTimer(500);
  
  outputPort = jack_port_register ( client,
                                    "output",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsOutput,
                                    0 );

  jack_set_process_callback( client, process, 0 );

  state = STOPPED;
  haveSample = false;

  jack_activate(client);

  jack_connect(client, "wavPlayer:output", "system:playback_1");
  jack_connect(client, "wavPlayer:output", "system:playback_2");

}

JackPlayer::~JackPlayer(void) {
  jack_deactivate(client);
  jack_ringbuffer_free(eventBuffer);
  jack_ringbuffer_free(inQueue);
  jack_ringbuffer_free(outQueue);
  jack_client_close(client);
  qDebug() << __func__ << "closed client";
}

int JackPlayer::process(jack_nframes_t nframes, void *) {
  // read and process incoming events
  set<Wave>::iterator i;
  while(jack_ringbuffer_read_space(inQueue) >= sizeof(i) ) {
    jack_ringbuffer_read(inQueue, (char*)&i, sizeof(i) );
    if(haveSample && curSample != i 
       && jack_ringbuffer_write_space(outQueue) >= sizeof(curSample)) {
      jack_ringbuffer_write(outQueue, (char*)&curSample, sizeof(curSample));
    }
    curSample = i;
    haveSample = true;
    playbackIndex = 0;
  }

  while(jack_ringbuffer_read_space(eventBuffer) >= sizeof(PlayEvent) ) {
    PlayEvent e;
    jack_ringbuffer_read( eventBuffer, (char*)&e, sizeof(PlayEvent) );
    switch(e) {
    case PAUSE:
      switch(state) {
      case PLAYING:
        state = STOPPED;
        break;
      case STOPPED:
        state = haveSample ? PLAYING : STOPPED;
        break;
      }
      break;
    }
  }

  float* outputBuffer= (float*)jack_port_get_buffer ( outputPort, nframes);

  // this is the intresting part, we work with each sample of audio data
  // one by one, copying them across from the vector that has the sample
  // in it, to the output buffer of JACK.
    switch(state) {
    case PLAYING:
      for (jack_nframes_t i = 0; i < nframes; ++i)
        {
          // here we check if index has gone played the last sample, and if it
          // has, then we reset it to 0 (the start of the sample).
          if ( playbackIndex >= curSample->samples.size() ) {
            playbackIndex = 0;
          }
    
          outputBuffer[i] = curSample->samples[playbackIndex++];
          
          for(unsigned int j=1; j<curSample->channels; ++j) {
            outputBuffer[i] += curSample->samples[playbackIndex++];
          }
          outputBuffer[i] /= curSample->channels;
        }
      break;
    case STOPPED:
      for (jack_nframes_t i=0; i < nframes; ++i) {
        outputBuffer[i] = 0.;
      }
      break;
    }
  
  return 0;
}

void JackPlayer::pause(void) {
  PlayEvent e = PAUSE;
  if(jack_ringbuffer_write_space(eventBuffer) >= sizeof(PlayEvent)) {
    jack_ringbuffer_write(eventBuffer, (const char*)&e, sizeof(PlayEvent));
  } else {
    cerr << "Can't write to eventBuffer" << endl;
  }
}

void JackPlayer::timerEvent(QTimerEvent *event __attribute__ ((unused)) ) {
  set<Wave>::iterator i;
  while(jack_ringbuffer_read_space(outQueue) >= sizeof(i) ) {
    jack_ringbuffer_read(outQueue, (char*)&i, sizeof(i) );
    qDebug() << __func__ << ": erasing sample";
    samples.erase(i);
 }
}

const Wave* JackPlayer::loadWave(const QString &fileName) {
  if(!fileName.isEmpty()) {
    auto iWave = samples.insert(Wave::openSoundFile(fileName));
    if(iWave.first->samples.size() 
       && jack_ringbuffer_write_space(inQueue) >= sizeof(iWave.first)) {
      jack_ringbuffer_write(inQueue, (const char *)&iWave.first, sizeof(iWave.first));
      return &(*iWave.first);
    } else {
      samples.erase(iWave.first);
      return nullptr;
    }
  } else {
    return nullptr;
  }
}
