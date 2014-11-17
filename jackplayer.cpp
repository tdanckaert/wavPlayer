#include "jackplayer.h"
#include "wave.h"

#include <assert.h>

#include <QTimer>
#include <QDebug>

#include <sndfile.hh>
#include <samplerate.h>

#include <algorithm>
#include <stdexcept>
#include <iostream>

using std::array;
using std::unique_ptr;
using std::pair;
using std::make_pair;
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

JackPlayer::JackPlayer(QObject *parent) : QObject(parent), curSample(nullptr), resampler(nullptr)
 {
  client = jack_client_open("wavPlayer", JackNullOption, 0 , 0);
  if (client == nullptr) {
    cerr << __func__ << " client failed!" << endl;
  }

  outputPort1 = jack_port_register (client,
                                    "output1",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsOutput,
                                    0);
  outputPort2 = jack_port_register (client,
                                    "output2",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsOutput,
                                    0);

  jack_set_process_callback( client, process_wrap, this );

  samplerate = jack_get_sample_rate(client);

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
  pair<unique_ptr<Wave>, SRC_STATE_ptr > newSample;
  while(inQueue.pop(newSample)) {
    // if we have a sample, make sure we are able to push it onto the
    // outQueue so it gets cleaned up in the other thread:
    if (curSample != nullptr || outQueue.push(make_pair(std::move(curSample), std::move(resampler)) ) ) {
        curSample = std::move(newSample.first);
        resampler = std::move(newSample.second);
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
      inputIndex = playbackIndex = e.start;
      playEnd = e.end ? e.end : curSample->samples.size();
      src_reset(resampler.get());
      qDebug() << "Play: " << playbackIndex << playEnd;
      break;
    case Command::Loop:
      state = LOOPING;
      if(e.start) {
        loopStart = e.start;
      }
      inputIndex = playbackIndex = loopStart;
      if (e.end) {
        loopEnd = e.end;
      }
      src_reset(resampler.get());
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
  pair<unique_ptr<Wave>, SRC_STATE_ptr> pOut;
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

  int error = 0;
  auto pSrc = SRC_STATE_ptr(src_new(SRC_SINC_FASTEST, pWave->channels, &error));
  if (error) {
    throw std::runtime_error(src_strerror(error) );
  }

  if (inQueue.push(make_pair(std::move(pWave), std::move(pSrc) ) ) ) {
    return result;
  } else {
    return nullptr;
  }
}

void JackPlayer::writeBuffer(jack_nframes_t nframes) {
  float* outputBuffer1= static_cast<float*>(jack_port_get_buffer(outputPort1, nframes));
  float* outputBuffer2= static_cast<float*>(jack_port_get_buffer(outputPort2, nframes));

  unsigned int frames_gen = 0;
  // in each iteration, we fill the outputBuffers until we have written 'nframes' frames, or until we have reached playEnd/loopEnd
  while (frames_gen < nframes) {
    if (state == STOPPED) { // generate empty frames
      for(; frames_gen < nframes; ++frames_gen) {
        outputBuffer2[frames_gen] = outputBuffer1[frames_gen] = 0.;
      }
    } else { // state == PLAYING or LOOPING
      auto end = (state == PLAYING) ? playEnd : loopEnd;
      const auto inputLeft = (end > playbackIndex) ? (end - playbackIndex) / curSample->channels : 0;
      const auto src_ratio = static_cast<double>(samplerate)/curSample->samplerate;
      const unsigned long outputLeft = round(inputLeft * src_ratio);

      // update state
      if ( !inputLeft || !outputLeft ) {
        switch (state) {
        case PLAYING:
          state = STOPPED;
          break;
        case LOOPING:
          if(loopStart < curSample->samples.size()) {
            inputIndex = playbackIndex = loopStart;
            src_reset(resampler.get());
          }
          break;
        default:
          break;
        }
      }

      if (samplerate == curSample->samplerate) {
        // no resampling: directly copy data to outputBuffers
        auto maxFrames = std::min<unsigned long>(frames_gen + inputLeft, nframes);
        for(; frames_gen < maxFrames; ++frames_gen) {
          outputBuffer1[frames_gen] = curSample->samples[playbackIndex];
          if (curSample->channels >=2) {
            outputBuffer2[frames_gen] = curSample->samples[++playbackIndex];
            playbackIndex += (curSample->channels -1);
          } else {
            outputBuffer2[frames_gen] = curSample->samples[playbackIndex];
            ++playbackIndex;
          }
        }
      } else {
        // resampling
        SRC_DATA src_data;
        src_data.data_in = const_cast<float *>(&curSample->samples[inputIndex]);
        src_data.data_out = resampleBuffer.data();
        src_data.input_frames = (curSample->samples.size()-playbackIndex)/curSample->channels;
        // number of output frames we can generate before reaching
        // playEnd/loopEnd, or reaching the end of resampleBuffer
        src_data.output_frames = std::min<unsigned long>(
          { outputLeft, nframes - frames_gen,
              resampleBuffer.size()/curSample->channels } );
        src_data.src_ratio = src_ratio;
        src_data.end_of_input = 0;

        int error = src_process(resampler.get(), &src_data);
        if (error) {
          throw std::runtime_error(src_strerror(error) );
        }

        inputIndex += curSample->channels * src_data.input_frames_used;
        playbackIndex += curSample->channels * round(src_data.output_frames_gen / src_ratio);

        for(unsigned int i=0; i < src_data.output_frames_gen*curSample->channels;) {
          outputBuffer1[frames_gen] = resampleBuffer[i];
          if (curSample->channels >=2) {
            outputBuffer2[frames_gen] = resampleBuffer[i+1];
          } else {
            outputBuffer2[frames_gen] = resampleBuffer[i];
          }
          i += (curSample->channels);
          ++frames_gen;
        }
      }
    }
  }
}

const Wave& JackPlayer::getCurWave(void) const {
  return *curSample;
}
