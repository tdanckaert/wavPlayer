#ifndef CUTTER_H
#define CUTTER_H

#include <QObject>

#include <vector>

class QGraphicsView;
class QGraphicsItem;
class QMouseEvent;
class JackPlayer;

class Cutter :public QObject {
  Q_OBJECT

  public:
  Cutter(QObject *parent=0, JackPlayer *p=0, QGraphicsView *v=0) : QObject(parent), player(p), view(v) {};

  void setView(QGraphicsView *v);
  void clear(void) { cuts.clear(); };

private:
  class Marker;
  class VerticalLine;
  JackPlayer *player;
  QGraphicsView *view;
  std::vector<QGraphicsItem *> cuts;
  QGraphicsItem *addCut(unsigned int pos);

public slots:
  void handleMousePress(Qt::MouseButton button, unsigned int pos);
};

#endif
