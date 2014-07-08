#include "jackplayer.h"
#include "wave.h"

#include <vector>
#include <iostream>

#include <QTimer>
#include <QDebug>

#include <sndfile.hh>

using std::set;
using std::cerr;
using std::endl;

/* 
 * state: playing or stopped
 *
 * if playing and "playEnd" is reached:
 *  -> go back to loopStart if loopStart < sample->size()
 *
 *  -> else: stop
 *
 */

enum PlayCommand {
  Play,
  Loop,
  Pause,
  Stop
};

class JackPlayer::PlayEvent {
public:
  int start;
  int stop;
  PlayCommand command;

  PlayEvent(PlayCommand cmd=Stop, int start=0, int stop=0) : start(start),
                                                        stop(stop),
                                                        command(cmd) {};
};

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

  jack_set_process_callback( client, process_wrap, this );

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

int JackPlayer::process_wrap(jack_nframes_t nframes, void *player) {
  return static_cast<JackPlayer *>(player)->process(nframes);
}

int JackPlayer::process(jack_nframes_t nframes) {

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
    playbackIndex = loopStart = curSample->samples.size();
  }

  while(jack_ringbuffer_read_space(eventBuffer) >= sizeof(PlayEvent) ) {
    PlayEvent e;
    jack_ringbuffer_read( eventBuffer, (char*)&e, sizeof(PlayEvent) );
    switch(e.command) {
    case Play:
      if (haveSample) {
        state = PLAYING;
        playEnd = curSample->samples.size();
        loopStart = curSample->samples.size();
      }
      playbackIndex = 0;
      //      playbackIndex=e.start;
      //      playEnd=e.stop;
      break;
    case Loop:
      if (haveSample) {
        state = PLAYING;
      //      loopStart = e.start;
        loopStart = 0;
      //      playEnd=e.stop;
        playEnd = curSample->samples.size();
        playbackIndex = loopStart;
      }
      break;
    case Pause:
      switch(state) {
      qDebug() << "JackPlayer Pause/Unpause";
      case PLAYING:
        state = STOPPED;
        break;
      case STOPPED:
        if (haveSample)
          state = PLAYING;
        break;
      }
      break;
    case Stop:
      qDebug() << "JackPlayer Stopping";
      state = STOPPED;
      if (haveSample) {
        playbackIndex = curSample->samples.size();
        loopStart = curSample->samples.size();
      }
      break;
    }
  }

  float* outputBuffer= (float*)jack_port_get_buffer ( outputPort, nframes);

  // this is the intresting part, we work with each sample of audio data
  // one by one, copying them across from the vector that has the sample
  // in it, to the output buffer of JACK.


  // Check if playbackIndex is past the end.  When in a loop: return to start, if not: stop.
  if ( state==PLAYING && playbackIndex >= curSample->samples.size() ) {
    if(loopStart < curSample->samples.size()) {
      playbackIndex = loopStart;
    } else {
      state = STOPPED;
    }
  }
  
  switch(state) {
  case PLAYING:
    for (jack_nframes_t i = 0; i < nframes; ++i)
      {
        
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

void JackPlayer::play(void) {
  qDebug() << __func__;
  sendEvent(Play);
}

void JackPlayer::loop(void) {
  qDebug() << __func__;
  sendEvent(Loop);
}

void JackPlayer::pause(void) {
  qDebug() << __func__;
  sendEvent(Pause);
}

void JackPlayer::stop(void) {
  qDebug() << __func__;
  sendEvent(Stop);
}

void JackPlayer::sendEvent(const PlayEvent &e) {
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
