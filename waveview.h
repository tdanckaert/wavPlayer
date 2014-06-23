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

  void updateGraphics(void);

private:
  unsigned int curPos; // current position of the view
  int channels;
  float height;
  std::vector<QGraphicsPixmapItem *> pixmaps;
  const std::vector<float> *wave;

protected:
  void scrollContentsBy(int dx, int dy);
  void resizeEvent(QResizeEvent *event);
    
signals:
    
public slots:
    
};

#endif // WAVEVIEW_H
