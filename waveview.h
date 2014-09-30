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

private:
  QGraphicsLineItem *indicator;
  float zoomLevel;
  float maxAmplitude;
  std::vector<QGraphicsPixmapItem *> pixmaps;
  const Wave *wave;

  float pixmapHeight(void) const;
  void checkZoomLevel(void);
  void updateGraphics(void);
  unsigned int visibleRange(void);

protected:
  void scrollContentsBy(int dx, int dy);
  void resizeEvent(QResizeEvent *event);
  void wheelEvent(QWheelEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void paintEvent(QPaintEvent *event);

signals:
  void waveClicked(QMouseEvent *event);

public slots:
  void updateIndicator(unsigned int playPos);
};

#endif // WAVEVIEW_H
