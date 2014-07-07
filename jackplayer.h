#ifndef JACKPLAYER_H
#define JACKPLAYER_H

#include <QObject>

#include <set>

#include <jack/jack.h>

class Wave;

class JackPlayer : public QObject {
  Q_OBJECT

public:
  JackPlayer(QObject *parent=0);
  ~JackPlayer(void);

  const Wave* loadWave(const QString& filename);

private:
  void timerEvent(QTimerEvent *event);

  std::set<Wave> samples;

  static int process(jack_nframes_t, void *);

private slots:
  void pause();
};

#endif
