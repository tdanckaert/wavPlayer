#include "waveview.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDebug>
#include <QGraphicsPixmapItem>

#define TILEWIDTH 300

using std::vector;

WaveView::WaveView(QWidget *parent) :
  QGraphicsView(parent),
  curPos(0)
{
  setScene(new QGraphicsScene(this));
  //  auto width = QApplication::desktop()->screenGeometry().width();
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void WaveView::drawWave(const vector<float> *wave, unsigned int channels) {
  auto height = 0.6 * QApplication::desktop()->screenGeometry().height();
  float ampl = 0.5*height;
  setSceneRect(0,0,wave->size()/channels, height);

  scene()->addLine(0,ampl,wave->size()/channels,ampl);

  // draw pixmaps:
  pixmaps.reserve(3 + width()/TILEWIDTH); // enough to cover width + 1 margin at each side
  qDebug() << "drawing" << (3 + width()/TILEWIDTH) << "tiles.";
  for(unsigned int i=0; i< (3 + width()/TILEWIDTH); ++i) {
    auto map = QPixmap(TILEWIDTH, height);
    map.fill(Qt::transparent);

    // get wave offset for pixmaps[i]
    auto offset = getOffset(i);

    if (offset >=0 && static_cast<unsigned int>(offset) < wave->size()/channels) {
      qDebug() << "drawing at offset" << offset;
      // draw pixmap
      vector<QPointF> points;
      points.reserve(1+TILEWIDTH);
      for(unsigned int j = 0 ; j<= (TILEWIDTH) && j < wave->size()/channels-offset; ) {
        points.push_back(QPointF(j, ampl*wave->at(offset+j) + ampl) );
        j+=channels;
      }
      QPainter painter(&map);
      QRectF rectangle(10.0, 20.0, 80.0, 60.0);

      painter.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);
      painter.drawPolyline(&points[0], points.size());
    }

    // add to scene and store pointer in pixmaps
    auto item = scene()->addPixmap(map);
    qDebug() << "pixmap" << i << "set offset to" << offset;
    if(offset >= 0) {
      item->setPos(QPointF(offset, 0.0));
    } else {
      item->setVisible(false);
    }
    pixmaps.push_back(item);
  }
  for (auto item : pixmaps) {
    qDebug() << item->pos();
  }
}

//  |...|...<|...|...|...|>...|...|
int WaveView::getOffset(unsigned int pixmapIndex) {
  int viewOffset = static_cast<int>(curPos/TILEWIDTH);
  return (viewOffset -1 + pixmapIndex ) * TILEWIDTH;
}
