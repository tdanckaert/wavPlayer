#ifndef JACKPLAYER_H
#define JACKPLAYER_H

#include <memory>

#include <QObject>

#include "spsc_queue.hpp"

#include <jack/jack.h>

enum PlayState {
  PLAYING,
  LOOPING,
  STOPPED
};

class Wave;

typedef boost::lockfree::spsc_queue<std::unique_ptr<Wave>, boost::lockfree::capacity<10> > spsc_wave_queue;

class JackPlayer : public QObject {
  Q_OBJECT

public:
  JackPlayer(QObject *parent=0);
  ~JackPlayer();

  const Wave* loadWave(Wave w);
  void setLoopStart(unsigned int start);
  void setLoopEnd(unsigned int end);
  const Wave& getCurWave() const;

public slots:
  void pause();
  void play(unsigned int start=0, unsigned int end=0);
  void loop(unsigned int start=0, unsigned int end=0);
  void stop();

private:

  class Command {
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

  Command(Type t=Stop, unsigned int start=0, unsigned int end=0) : start(start),
      end(end),
      type(t) {};
  };


  PlayState state;
  std::unique_ptr<Wave> curSample;
  jack_port_t *outputPort1, *outputPort2;
  jack_client_t *client;

  unsigned int playbackIndex; /* 0 to curSample->size() */
  unsigned int loopStart; /* 0 to curSample->size() */
  unsigned int loopEnd;
  unsigned int playEnd;

  boost::lockfree::spsc_queue<Command, boost::lockfree::capacity<10> > eventQueue;

  spsc_wave_queue inQueue; // samples in
  spsc_wave_queue outQueue; // samples out, can be freed

  static int process_wrap(jack_nframes_t, void *);
  int process(jack_nframes_t nframes);

  void timerEvent(QTimerEvent *event);
  class Command;
  void sendCommand(const Command &e);
  void readCommands();
  void writeBuffer(jack_nframes_t nframes);
  void reset();

signals:
  void positionChanged(unsigned int playPos);
};

#endif
