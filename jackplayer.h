#ifndef JACKPLAYER_H
#define JACKPLAYER_H

#include <QObject>

#include <set>

#include <jack/jack.h>
#include <jack/ringbuffer.h>

class Wave;

  enum PlayState {
    PLAYING,
    STOPPED
  };
  
  enum PlayEvent {
    PAUSE
  };

class JackPlayer : public QObject {
  Q_OBJECT

public:
  JackPlayer(QObject *parent=0);
  ~JackPlayer(void);

  const Wave* loadWave(const QString& filename);

  int process(jack_nframes_t nframes);

private:
  void timerEvent(QTimerEvent *event);

  std::set<Wave> samples;

  static int process_wrap(jack_nframes_t, void *);

  bool haveSample;
  std::set<Wave>::iterator curSample;
  jack_port_t *outputPort;
  jack_client_t *client;
  unsigned int playbackIndex;
  PlayState state;
  jack_ringbuffer_t *eventBuffer;

  jack_ringbuffer_t *inQueue; // samples in
  jack_ringbuffer_t *outQueue; // samples out, can be freed

private slots:
  void pause();
};

#endif
