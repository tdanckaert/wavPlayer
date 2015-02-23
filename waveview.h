#ifndef WAVEVIEW_H
#define WAVEVIEW_H

#include <QGraphicsView>
#include <vector>

class Wave;

class WaveView : public QGraphicsView
{
  Q_OBJECT
public:
  explicit WaveView(QWidget *parent = 0);
  void drawWave(const Wave *wave);
  void drawPixmap(QGraphicsPixmapItem *item, unsigned int wavePos);
  QGraphicsItem *markerAt(QPoint pos);
  void zoomIn();
  void zoomOut();
  void zoomToSelection();

private:
  bool isDragging;
  QPoint dragStart;
  QGraphicsRectItem *selection;
  QGraphicsLineItem *indicator;
  float zoomLevel;
  float maxAmplitude;
  std::vector<QGraphicsPixmapItem *> pixmaps;
  const Wave *wave;

  void initScene(void);
  float pixmapHeight(void) const;
  void checkZoomLevel(void);
  void updateGraphics(void);
  unsigned int visibleRange(void);

protected:
  void resizeEvent(QResizeEvent *event);
  void wheelEvent(QWheelEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void paintEvent(QPaintEvent *event);

signals:
  void waveClicked(QMouseEvent *event);
  void rangeSelected(unsigned int selectionStart, unsigned int selectionEnd);

public slots:
  void updateIndicator(unsigned int playPos);
};

#endif // WAVEVIEW_H
