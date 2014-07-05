#ifndef WAVEVIEW_H
#define WAVEVIEW_H

#include <QGraphicsView>
#include <vector>

class WaveView : public QGraphicsView
{
  Q_OBJECT
public:
  explicit WaveView(QWidget *parent = 0);
  void drawWave(const std::vector<float> *wave, unsigned int channels);

  void drawPixmap(QGraphicsPixmapItem *item, unsigned int wavePos);

private:
  int channels;
  float zoomLevel;
  float maxAmplitude;
  std::vector<QGraphicsPixmapItem *> pixmaps;
  const std::vector<float> *wave;

  float pixmapHeight(void);
  void updatePixmaps(void);
  void updateGraphics(void);
  unsigned int visibleRange(void);

protected:
  void scrollContentsBy(int dx, int dy);
  void resizeEvent(QResizeEvent *event);
  void wheelEvent(QWheelEvent *event);
  void paintEvent(QPaintEvent *event);
    
signals:
    
public slots:
    
};

#endif // WAVEVIEW_H
