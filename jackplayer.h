#ifndef JACKPLAYER_H
#define JACKPLAYER_H

#include <QObject>

#include <set>

#include "wave.h"

#include <jack/jack.h>
#include <jack/ringbuffer.h>

enum PlayState {
  PLAYING,
  LOOPING,
  STOPPED
};

class Wave;

class JackPlayer : public QObject {
  Q_OBJECT

public:
  JackPlayer(QObject *parent=0);
  ~JackPlayer(void);

  const Wave* loadWave(Wave w);
  void setLoopStart(unsigned int start);
  void setLoopEnd(unsigned int end);
  const Wave& getCurWave(void) const;

public slots:
  void pause();
  void play(unsigned int start=0, unsigned int end=0);
  void loop(unsigned int start=0, unsigned int end=0);
  void stop();

private:
  std::set<Wave> samples;

  PlayState state;
  bool haveSample;
  std::set<Wave>::iterator curSample;
  jack_port_t *outputPort;
  jack_client_t *client;

  unsigned int playbackIndex; /* 0 to curSample->size() */
  unsigned int loopStart; /* 0 to curSample->size() */
  unsigned int loopEnd;
  unsigned int playEnd;

  jack_ringbuffer_t *eventBuffer;

  jack_ringbuffer_t *inQueue; // samples in
  jack_ringbuffer_t *outQueue; // samples out, can be freed

  static int process_wrap(jack_nframes_t, void *);
  int process(jack_nframes_t nframes);

  void timerEvent(QTimerEvent *event);
  class Command;
  void sendCommand(const Command &e);
  void readCommands(void);
  void writeBuffer(jack_nframes_t nframes);
  void reset(void);

signals:
  void positionChanged(unsigned int playPos);
};

#endif
