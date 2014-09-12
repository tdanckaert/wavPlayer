#include "jackplayer.h"
#include "wave.h"

#include <vector>
#include <iostream>
#include <assert.h>

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

class JackPlayer::Command {
public:
  enum Type {
    Play,
    Loop,
    Pause,
    Stop
  };

  unsigned int start;
  unsigned int end;
  Type type;

  Command(Type t=Stop, int start=0, int end=0) : start(start),
                                                 end(end),
                                                 type(t) {};
};

JackPlayer::JackPlayer(QObject *parent) : QObject(parent)
 {
  client = jack_client_open("wavPlayer", JackNullOption, 0 , 0);
  if (client == nullptr) {
    cerr << __func__ << " client failed!" << endl;
  }

  eventBuffer = jack_ringbuffer_create(sizeof(Command)*10);
  jack_ringbuffer_mlock(eventBuffer);

  inQueue = jack_ringbuffer_create(sizeof(set<Wave>::iterator)*10);
  jack_ringbuffer_mlock(inQueue);
  outQueue = jack_ringbuffer_create(sizeof(set<Wave>::iterator)*10);
  jack_ringbuffer_mlock(outQueue);
  
  outputPort1 = jack_port_register ( client,
                                    "output1",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsOutput,
                                    0 );
  outputPort2 = jack_port_register ( client,
                                    "output2",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsOutput,
                                    0 );

  jack_set_process_callback( client, process_wrap, this );

  state = STOPPED;
  haveSample = false;

  startTimer(20);

  jack_activate(client);

  jack_connect(client, "wavPlayer:output1", "system:playback_1");
  jack_connect(client, "wavPlayer:output2", "system:playback_2");

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

  // queue new sample if needed
  while(jack_ringbuffer_read_space(inQueue) >= sizeof(set<Wave>::iterator) ) {
    set<Wave>::iterator i;
    jack_ringbuffer_read(inQueue, (char*)&i, sizeof(i) );
    if(haveSample && curSample != i 
       && jack_ringbuffer_write_space(outQueue) >= sizeof(curSample)) {
      jack_ringbuffer_write(outQueue, (char*)&curSample, sizeof(curSample));
    }
    curSample = i;
    haveSample = true;
    reset();
  }

  // read and process incoming events (play/pause/loop/...)
  readCommands();

  // write to Jack output buffer
  writeBuffer(nframes);
  return 0;
}

void JackPlayer::play(unsigned int start, unsigned int end) {
  qDebug() << __func__;
  sendCommand({Command::Play, start, end});
}

void JackPlayer::loop(unsigned int start, unsigned int end) {
  qDebug() << __func__;
  sendCommand({Command::Loop, start, end});
}

void JackPlayer::pause(void) {
  qDebug() << __func__;
  sendCommand(Command::Pause);
}

void JackPlayer::stop(void) {
  qDebug() << __func__;
  sendCommand(Command::Stop);
}

void JackPlayer::setLoopStart(unsigned int start) {
  if(haveSample) {
    loopStart = start*(curSample->channels);
    qDebug() << __func__ << loopStart;
  }
}

void JackPlayer::setLoopEnd(unsigned int end) {
  if(haveSample) {
    loopEnd = end*(curSample->channels);
    qDebug() << __func__ << loopEnd;
  }
}

void JackPlayer::sendCommand(const Command &e) {
  if(jack_ringbuffer_write_space(eventBuffer) >= sizeof(Command)) {
    jack_ringbuffer_write(eventBuffer, (const char*)&e, sizeof(Command));
  } else {
    cerr << "Can't write to eventBuffer" << endl;
  }
}

/* */
inline void JackPlayer::reset(void) {
  assert(haveSample);
  playbackIndex = curSample->samples.size();
  playEnd = curSample->samples.size();
}

/* Read all queued events from eventBuffer. */
void JackPlayer::readCommands(void) {
  while(jack_ringbuffer_read_space(eventBuffer) >= sizeof(Command) ) {
    Command e;
    jack_ringbuffer_read( eventBuffer, (char*)&e, sizeof(Command) );

    if (!haveSample) {
      // If no sample is loaded, we just empty the buffer and ignore the events
      continue;
    }

    // Check that the command we received is valid for the current
    // sample (in principle, we could receive commands for another
    // sample due to synchronization issues)
    e.start*=curSample->channels;
    e.end*=curSample->channels;
    if(e.start > curSample->samples.size() 
       || e.end > curSample->samples.size()) {
      reset();
      state = STOPPED;
      continue;
    }
    switch(e.type) {
    case Command::Play:
      reset();
      state = PLAYING;
      playbackIndex = e.start;
      playEnd = e.end ? e.end : curSample->samples.size();
      qDebug() << "Play: " << playbackIndex << playEnd;
      break;
    case Command::Loop:
      state = LOOPING;
      if(e.start) {
        loopStart = e.start;
      }
      playbackIndex = loopStart;
      if (e.end) {
        loopEnd = e.end;
      }
      qDebug() << "Loop: " << playbackIndex << playEnd;
      break;
    case Command::Pause:
      switch(state) {
      qDebug() << "JackPlayer Pause/Unpause";
      case PLAYING:
      case LOOPING:
        state = STOPPED;
        break;
      case STOPPED:
        // TODO: keep track if we have to return to looping or to a single play
        state = PLAYING;
        break;
      }
      break;
    case Command::Stop:
      qDebug() << "JackPlayer Stopping";
      reset();
      state = STOPPED;
      break;
    }
  }
}

void JackPlayer::timerEvent(QTimerEvent *event __attribute__ ((unused)) ) {
  set<Wave>::iterator i;
  while(jack_ringbuffer_read_space(outQueue) >= sizeof(i) ) {
    jack_ringbuffer_read(outQueue, (char*)&i, sizeof(i) );
    qDebug() << __func__ << ": erasing sample";
    samples.erase(i);
 }

  if(haveSample)
    emit positionChanged(playbackIndex/curSample->channels);

}

const Wave* JackPlayer::loadWave(Wave wave) {
  auto iWave = samples.insert(wave);
  if(jack_ringbuffer_write_space(inQueue) >= sizeof(iWave.first)) {
    jack_ringbuffer_write(inQueue, (const char *)&iWave.first, sizeof(iWave.first));
    return &(*iWave.first);
  } else {
    samples.erase(iWave.first);
    return nullptr;
  }
}

void JackPlayer::writeBuffer(jack_nframes_t nframes) {
  float* outputBuffer1= static_cast<float*>(jack_port_get_buffer(outputPort1, nframes));
  float* outputBuffer2= static_cast<float*>(jack_port_get_buffer(outputPort2, nframes));
  
  for (jack_nframes_t i = 0; i < nframes; ++i) {
    // Check if playbackIndex is past the end.  When in a loop: return to start, if not: stop.
    if (state == PLAYING && playbackIndex >= playEnd ) {
      state = STOPPED;
    } else if (state == LOOPING && playbackIndex >= loopEnd) {
      if(loopStart < curSample->samples.size()) {
        playbackIndex = loopStart;
      } else {
        state = STOPPED;
      }
    }

    if (state == PLAYING || state == LOOPING ) {
      outputBuffer1[i] = curSample->samples[playbackIndex];
      if (curSample->channels >=2) {
        outputBuffer2[i] = curSample->samples[++playbackIndex];
        playbackIndex += (curSample->channels -1);
      } else {
        outputBuffer2[i] = curSample->samples[playbackIndex];
        ++playbackIndex;
      }
    } else {
      outputBuffer2[i] = outputBuffer1[i] = 0.;
    }
  }
}

const Wave& JackPlayer::getCurWave(void) const {
  return *curSample;
}
