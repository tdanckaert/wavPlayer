#include "waveview.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QScrollBar>
#include <QDebug>
#include <QGraphicsPixmapItem>
#include <QResizeEvent>

#include <assert.h>
#include <algorithm>

#define TILEWIDTH 300

using std::vector;

WaveView::WaveView(QWidget *parent) :
  QGraphicsView(parent),
  zoomLevel(1.0),
  wave(nullptr)
{
  setScene(new QGraphicsScene(this));
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void WaveView::drawWave(const vector<float> *wave, unsigned int nChannels) {
  pixmaps.clear();
  scene()->clear();
  this->wave = wave;
  channels = nChannels;

  scene()->setSceneRect(0,0,static_cast<float>(wave->size()/channels), pixmapHeight());
  fitInView(0,0,wave->size()/channels,pixmapHeight());

  maxAmplitude = fabs(*std::max_element(wave->begin(), wave->end(), 
                                        [] (decltype(wave->at(0)) a, decltype(wave->at(0)) b) { return fabs(a) < fabs(b); }));

  scene()->addLine(0, 0.5*pixmapHeight(), static_cast<float>(wave->size()/channels), 0.5*pixmapHeight());

  horizontalScrollBar()->setSliderPosition(0);
  zoomLevel = 1/transform().m11();

}

void WaveView::scrollContentsBy(int dx, int dy) {
  qDebug() << __func__ << dx << dy;
  QGraphicsView::scrollContentsBy(dx,dy);
}

unsigned int WaveView::visibleRange(void) {
  auto xLeft = static_cast<int>(mapToScene(QPoint(2,0)).x());
  // the width of the visible scene is equal to (viewport width - 4) (border of 2px on each side?)
  auto xRight = static_cast<int>(mapToScene(QPoint(std::max(viewport()->width()-2,0),0)).x());
  return xRight - xLeft;
}

void WaveView::resizeEvent(QResizeEvent* event ) {
  QGraphicsView::resizeEvent(event);
  fitInView(QRectF(mapToScene(QPoint(2,0)),
                   QSizeF(visibleRange(), pixmapHeight()) ) );
}

void WaveView::wheelEvent(QWheelEvent *event) {
  auto zoomFact = event->delta() > 0
    ? 1.15 : 1/1.15;
  setTransform(transform() * QTransform::fromScale(zoomFact,1.0));
  auto stretchRatio = transform().m11()*zoomLevel;
  // either we have to zoom out
  while (stretchRatio > 1.2) {
    zoomLevel /= 1.2;
    stretchRatio /= 1.2;
  }
  // or zoom in
  while (stretchRatio < 1.0) {
    zoomLevel *= 1.2;
    stretchRatio *= 1.2;
  }
  qDebug() << "transform horizontal stretch: " << transform().m11() 
           << "zoomLevel:" << zoomLevel << "- ratio:" << stretchRatio;
}

void WaveView::paintEvent(QPaintEvent *event) {
  updateGraphics();
  QGraphicsView::paintEvent(event);
};

void WaveView::updateGraphics(void) {
  qDebug() << __func__;

  int samplesPerTile = static_cast<int>(TILEWIDTH*zoomLevel);

  // make sure we have enough pixmaps to cover the whole visible area
  while(wave &&
        (visibleRange() + samplesPerTile > pixmaps.size() * samplesPerTile) ) {
    auto item = new QGraphicsPixmapItem();
    scene()->addItem(item);
    pixmaps.push_back(item);
   }

  // check all pixmaps still have the correct position and zoomLevel:
  if(pixmaps.size()) {
    // get sample offset of the left border of the view
    auto xLeft = static_cast<int>(mapToScene(QPoint(2,0)).x());
    if(xLeft < 0)
       xLeft = 0;

    unsigned int indexLeft = (xLeft / samplesPerTile) % pixmaps.size();
    unsigned int wavePos = (xLeft / samplesPerTile) * samplesPerTile;

    qDebug() << "xLeft" << xLeft << "indexLeft:" << indexLeft << endl
             << "wavePos" << wavePos;
    
    for(unsigned int i=0; i<pixmaps.size(); ++i) {
      auto index = (indexLeft + i) % pixmaps.size();
      auto pixmap = pixmaps[index];
      if ( fabs(pixmap->x() - wavePos) > 1 || pixmap->transform().m11() != zoomLevel ) {
        qDebug() << "pixmap at" <<  pixmap->x() << "wavePos:" << wavePos;
        drawPixmap(pixmap, wavePos);
      }
      wavePos += samplesPerTile;
    }
  }
}

void WaveView::drawPixmap(QGraphicsPixmapItem *item, unsigned int wavePos) {
  auto map = QPixmap(TILEWIDTH, pixmapHeight());
  map.fill(Qt::transparent);

  qDebug() << "draw new pixmap at wavePos" << wavePos;
  
  auto ampl = 0.5*pixmapHeight()/maxAmplitude;
  auto center = 0.5*pixmapHeight();
  auto offset = wavePos*channels;
  auto samplesPerTile = static_cast<unsigned int>(TILEWIDTH*zoomLevel);

  if (offset < wave->size()) {
    // draw pixmap
    vector<QPointF> points;
    points.reserve(1+samplesPerTile);
    for(unsigned int j = 0 ; j<= samplesPerTile && (offset+j*channels) < wave->size();++j) {
      points.push_back(QPointF(j/zoomLevel, -ampl*wave->at(offset+j*channels) + center ) );
    }
    QPainter painter(&map);

    painter.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);
    qDebug() << "drawing" << points.size() <<"samples";
    painter.drawPolyline(&points[0], points.size());
  }
  
  qDebug() << "set item position to" << wavePos;
  item->setPos(QPointF(wavePos, 0.0));
  item->setTransform(QTransform::fromScale(zoomLevel,1.0));
  item->setVisible(true);
  item->setPixmap(map);
}

inline float WaveView::pixmapHeight(void) {
  return 0.6 * QApplication::desktop()->screenGeometry().height();
}
