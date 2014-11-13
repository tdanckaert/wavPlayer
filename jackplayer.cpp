#include "jackplayer.h"
#include "wave.h"

#include <vector>
#include <iostream>
#include <assert.h>

#include <QTimer>
#include <QDebug>

#include <sndfile.hh>

using std::unique_ptr;
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

JackPlayer::JackPlayer(QObject *parent) : QObject(parent), curSample(nullptr)
 {
  client = jack_client_open("wavPlayer", JackNullOption, 0 , 0);
  if (client == nullptr) {
    cerr << __func__ << " client failed!" << endl;
  }

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

  startTimer(20);

  jack_activate(client);

  jack_connect(client, "wavPlayer:output1", "system:playback_1");
  jack_connect(client, "wavPlayer:output2", "system:playback_2");

}

JackPlayer::~JackPlayer(void) {
  jack_deactivate(client);
  jack_client_close(client);
  qDebug() << __func__ << "closed client";
}

int JackPlayer::process_wrap(jack_nframes_t nframes, void *player) {
  return static_cast<JackPlayer *>(player)->process(nframes);
}

int JackPlayer::process(jack_nframes_t nframes) {

  // queue new sample if needed
  unique_ptr<Wave> newSample;
  while(inQueue.pop(newSample)) {
    // if we have a sample, make sure we are able to push it onto the
    // outQueue so it gets cleaned up in the other thread:
    if (curSample != nullptr || outQueue.push(std::move(curSample) ) ) {
        curSample = std::move(newSample);
        reset();
    }
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
  if (curSample != nullptr) {
    loopStart = start*(curSample->channels);
    qDebug() << __func__ << loopStart;
  }
}

void JackPlayer::setLoopEnd(unsigned int end) {
  if (curSample != nullptr) {
    loopEnd = end*(curSample->channels);
    qDebug() << __func__ << loopEnd;
  }
}

void JackPlayer::sendCommand(const Command &e) {
  if (!eventQueue.push(e) ) {
    cerr << "Can't write to eventQueue" << endl;
  }
}

/* */
inline void JackPlayer::reset(void) {
  assert (curSample != nullptr);
  playbackIndex = curSample->samples.size();
  playEnd = curSample->samples.size();
}

/* Read all queued events from eventBuffer. */
void JackPlayer::readCommands(void) {
  Command e;
  while (eventQueue.pop(e) ) {

    if (curSample == nullptr) {
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
  unique_ptr<Wave> pOut;
  while (outQueue.pop(pOut) ) {
    // pop outQueue until empty...
    qDebug() << __func__ << ": erasing sample";
 }

  if (curSample != nullptr)
    emit positionChanged(playbackIndex/curSample->channels);

}

const Wave* JackPlayer::loadWave(Wave wave) {
  auto pWave = unique_ptr<Wave>(new Wave(std::move(wave)));
  Wave *result = pWave.get();
  if (inQueue.push(std::move(pWave) ) ) {
    return result;
  } else {
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
