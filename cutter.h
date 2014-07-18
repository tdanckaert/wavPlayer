#ifndef CUTTER_H
#define CUTTER_H

#include <QObject>
#include <QPointF>

#include <vector>

class QGraphicsView;
class QGraphicsItem;
class QGraphicsRectItem;
class QMouseEvent;
class JackPlayer;

class Cutter :public QObject {
  Q_OBJECT

  public:
  Cutter(QObject *parent=0, JackPlayer *p=0, QGraphicsView *v=0) : QObject(parent), player(p), view(v),
                                                                   slice(nullptr), sliceStart(nullptr), sliceEnd(nullptr) {};

  void setView(QGraphicsView *v);
  void clear(void) { cuts.clear(); };

private:
  class Marker;
  class VerticalLine;
  JackPlayer *player;
  QGraphicsView *view;
  QGraphicsRectItem *slice;
  QGraphicsItem *sliceStart;
  QGraphicsItem *sliceEnd;
  std::vector<QGraphicsItem *> cuts;
  QGraphicsItem *addCut(unsigned int pos);
  void drawSlice(void);

public slots:
  void handleMousePress(Qt::MouseButton button, QPointF scenePos);
  void nextSlice(void);
  void prevSlice(void);
  void playSlice(void);

private slots:
  void markerMoved(unsigned int newPos);
};

#endif
