#ifndef JACKPLAYER_H
#define JACKPLAYER_H

#include <QObject>

#include <set>

#include <jack/jack.h>
#include <jack/ringbuffer.h>

enum PlayState {
  PLAYING,
  LOOP,
  STOPPED
};

class Wave;

class JackPlayer : public QObject {
  Q_OBJECT

public:
  JackPlayer(QObject *parent=0);
  ~JackPlayer(void);

  const Wave* loadWave(const QString& filename);

  int process(jack_nframes_t nframes);

private:
  std::set<Wave> samples;

  bool haveSample;
  std::set<Wave>::iterator curSample;
  jack_port_t *outputPort;
  jack_client_t *client;

  unsigned int playbackIndex; /* 0 to curSample->size() */
  unsigned int loopStart; /* 0 to curSample->size() */
  unsigned int loopEnd; /* 0 to curSample->size() */
  unsigned int playEnd;

  PlayState state;
  jack_ringbuffer_t *eventBuffer;

  jack_ringbuffer_t *inQueue; // samples in
  jack_ringbuffer_t *outQueue; // samples out, can be freed

  static int process_wrap(jack_nframes_t, void *);
  void timerEvent(QTimerEvent *event);

private slots:
  void pause();
};

#endif
