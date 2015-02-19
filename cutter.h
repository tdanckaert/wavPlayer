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
  enum LoopState { None, Selection, Slices};
  class Marker;
  class VerticalLine;
  JackPlayer *player;
  WaveView *view;
  QGraphicsRectItem *slice;
  Marker *sliceStart;
  Marker *sliceEnd;
  Marker *toDelete;
  QMenu deleteMenu;
  QMenu addMenu;
  std::vector<Marker *> cuts;
  Marker *addMarker(unsigned int pos);
  void addCut(qreal scene_x);
  LoopState loopState;

  void updateSlice(Marker *start, Marker *end);
  void drawSlice(void);
  void updateLoop(void);
  void playSlice(void);
  unsigned int selectionStart;
  unsigned int selectionEnd;
  qreal addMenuPos;

public slots:
  void handleMousePress(QMouseEvent *event);
  void nextSlice(void);
  void prevSlice(void);
  void play(void);
  void selectRange(unsigned int, unsigned int);
  void addCut();

private slots:
  void markerMoved(unsigned int newPos);
  void deleteMarker();

signals:
  void cutsChanged(bool haveSlice);
};

#endif
