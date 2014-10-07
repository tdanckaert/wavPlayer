#ifndef CUTTER_H
#define CUTTER_H

#include <QObject>
#include <QMenu>

#include <vector>

class WaveView;
class QGraphicsItem;
class QGraphicsRectItem;
class QMouseEvent;
class JackPlayer;

class Cutter :public QObject {
  Q_OBJECT

  public:
  Cutter(QObject *parent=0, JackPlayer *p=0, WaveView *v=0);

  void setView(WaveView *v);
  void clear(void);
  void loop(void);

  void exportSamples(const QString& path) const;

private:
  class Marker;
  class VerticalLine;
  JackPlayer *player;
  WaveView *view;
  QGraphicsRectItem *slice;
  Marker *sliceStart;
  Marker *sliceEnd;
  Marker *toDelete;
  QMenu deleteMenu;
  std::vector<Marker *> cuts;
  Marker *addCut(unsigned int pos);
  void updateSlice(Marker *start, Marker *end);
  void drawSlice(void);
  void updateLoop(void);

public slots:
  void handleMousePress(QMouseEvent *event);
  void nextSlice(void);
  void prevSlice(void);
  void playSlice(void);

private slots:
  void markerMoved(unsigned int newPos);
  void deleteMarker();

signals:
  void cutsChanged(bool haveSlice);
};

#endif
