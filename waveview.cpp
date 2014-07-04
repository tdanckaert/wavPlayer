#include "waveview.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QScrollBar>
#include <QDebug>
#include <QGraphicsPixmapItem>
#include <QResizeEvent>
#include <assert.h>

#define TILEWIDTH 300

using std::vector;

WaveView::WaveView(QWidget *parent) :
  QGraphicsView(parent),
  curPos(0),
  zoomLevel(5.0),
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
  height = 0.6 * QApplication::desktop()->screenGeometry().height();

  scene()->setSceneRect(0,0,static_cast<float>(wave->size()/channels), height);
  qDebug() << "sample length:" << wave->size()/channels << endl
           << "sceneRect" << scene()->sceneRect();

  setTransform(QTransform::fromScale(1/zoomLevel,1.0));

  auto ampl = 0.5*height;
  scene()->addLine(0, ampl, static_cast<float>(wave->size()/channels), ampl);

  // draw pixmaps:
  auto xLeft = static_cast<int>(mapToScene(QPoint(2,0)).x());
  auto xRight = static_cast<int>(mapToScene(QPoint(std::max(viewport()->width()-4, 0),
                                                   0)).x());
  unsigned int visibleRange = xRight - xLeft;
  int samplesPerTile =  static_cast<int>(TILEWIDTH*zoomLevel);
  pixmaps.resize(2+static_cast<int>(visibleRange/samplesPerTile));
  qDebug() << "width()=" << width() << endl
           << "drawing" << (1+width()/TILEWIDTH) << "pixmaps";

  for(unsigned int i=0; i<pixmaps.size(); ++i) {
    pixmaps[i] = new QGraphicsPixmapItem();
    scene()->addItem(pixmaps[i]);
  }

  updateAll = true;
  updateGraphics();

  horizontalScrollBar()->setSliderPosition(0);
}

void WaveView::scrollContentsBy(int dx, int dy) {
  qDebug() << __func__ << dx << dy;
  QGraphicsView::scrollContentsBy(dx,dy);
  updateGraphics();
}

void WaveView::updatePixmaps(void) {
  int samplesPerTile = static_cast<int>(TILEWIDTH*zoomLevel);
  while(wave &&
        (visibleRange() + samplesPerTile > pixmaps.size() * samplesPerTile) ) {
    auto item = new QGraphicsPixmapItem();
    qDebug() << __func__ << "add item to scene";
    scene()->addItem(item);
    qDebug() << __func__ << "add item to pixmaps";
    pixmaps.push_back(item);
   }
}

unsigned int WaveView::visibleRange(void) {
  auto xLeft = static_cast<int>(mapToScene(QPoint(0,0)).x());
  // the width of the visible scene is equal to (viewport width - 4) (border of 2px on each side?)
  auto xRight = static_cast<int>(mapToScene(QPoint(std::max(viewport()->width()-4,0),0)).x());
  return xRight - xLeft;
}

void WaveView::resizeEvent(QResizeEvent* event ) {
  QGraphicsView::resizeEvent(event);
  fitInView(QRectF(mapToScene(QPoint(2,2)),
                   QSizeF(visibleRange(), height) ) );
  updatePixmaps();
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
    updateAll = true;
  }
  // or zoom in
  while (stretchRatio < 1.0) {
    zoomLevel *= 1.2;
    stretchRatio *= 1.2;
    updateAll = true;
  }
  qDebug() << "transform horizontal stretch: " << transform().m11() 
           << "zoomLevel:" << zoomLevel << "- ratio:" << stretchRatio;
  updatePixmaps();
  updateGraphics();
}

void WaveView::updateGraphics(void) {
  // check all pixmaps are still in the correct position:
  qDebug() << __func__;
  if(pixmaps.size()) {
    // get sample offset of the left border of the view
    auto xLeft = static_cast<int>(mapToScene(QPoint(2,0)).x());
    if(xLeft < 0)
       xLeft = 0;

    int samplesPerTile = static_cast<int>(TILEWIDTH*zoomLevel);
    unsigned int indexLeft = xLeft / samplesPerTile % pixmaps.size();
    unsigned int wavePos = xLeft / samplesPerTile * samplesPerTile;

       qDebug() << "xLeft" << xLeft << "indexLeft:" << indexLeft << endl
       << "wavePos" << wavePos << "updateAll?" << updateAll;
    
    for(unsigned int i=0; i<pixmaps.size(); ++i) {
      auto index = (indexLeft + i) % pixmaps.size();
      auto pixmap = pixmaps[index];
      //      qDebug() << "pixmap" << index << ": wavePos =" << wavePos << ", x() =" << pixmap->x();
      if (updateAll || fabs(pixmap->x() - wavePos) > 1) {
        qDebug() << "pixmap at" <<  pixmap->x() << "wavePos:" << wavePos;
        drawPixmap(pixmap, wavePos);
      }
      wavePos += samplesPerTile;
    }
    updateAll = false;
  }
}

void WaveView::drawPixmap(QGraphicsPixmapItem *item, unsigned int wavePos) {
  auto map = QPixmap(TILEWIDTH, height);
  map.fill(Qt::transparent);

  qDebug() << "draw new pixmap at wavePos" << wavePos;
  
  auto ampl = 0.5*height;
  auto offset = wavePos*channels;
  auto samplesPerTile = static_cast<unsigned int>(TILEWIDTH*zoomLevel);

  if (offset < wave->size()) {
    // draw pixmap
    vector<QPointF> points;
    points.reserve(1+samplesPerTile);
    for(unsigned int j = 0 ; j<= samplesPerTile && (offset+j*channels) < wave->size();++j) {
      points.push_back(QPointF(j/zoomLevel, ampl*wave->at(offset+j*channels) + ampl) );
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
