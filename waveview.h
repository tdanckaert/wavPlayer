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

private:
  unsigned int curPos; // current position of the view
  int getOffset(unsigned int pixmapIndex); // get the position in the wave associated with pixmapIndex
  std::vector<QGraphicsPixmapItem *> pixmaps;
  std::vector<float> *wave;
    
signals:
    
public slots:
    
};

#endif // WAVEVIEW_H
