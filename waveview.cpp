#include "waveview.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QScrollBar>
#include <QDebug>
#include <QGraphicsPixmapItem>
#include <QResizeEvent>

#define TILEWIDTH 300

using std::vector;

WaveView::WaveView(QWidget *parent) :
  QGraphicsView(parent),
  curPos(0),
  zoomLevel(10.0),
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

  scene()->setSceneRect(0,0,static_cast<float>(wave->size()/channels)/zoomLevel, height);
  qDebug() << "sample length:" << wave->size()/channels << endl
           << "sceneRect" << scene()->sceneRect();

  auto ampl = 0.5*height;
  scene()->addLine(0, ampl, static_cast<float>(wave->size()/channels)/zoomLevel, ampl);

  // draw pixmaps:
  pixmaps.resize(2+width()/TILEWIDTH);
  qDebug() << "width()=" << width() << endl
           << "drawing" << (1+width()/TILEWIDTH) << "pixmaps";

  for(unsigned int i=0; i<pixmaps.size(); ++i) {
    pixmaps[i] = new QGraphicsPixmapItem();
    scene()->addItem(pixmaps[i]);
    drawPixmap(pixmaps[i], i*TILEWIDTH*zoomLevel);
  }

  horizontalScrollBar()->setSliderPosition(0);
}

void WaveView::scrollContentsBy(int dx, int dy) {
  QGraphicsView::scrollContentsBy(dx,dy);
  updateGraphics();
}

void WaveView::resizeEvent(QResizeEvent* event ) {
  while(event->size().width() + TILEWIDTH > pixmaps.size() * TILEWIDTH) {
    auto item = new QGraphicsPixmapItem();
    qDebug() << __func__ << "add item to scene";
    scene()->addItem(item);
    qDebug() << __func__ << "add item to pixmaps";
    pixmaps.push_back(item);
  }
  QGraphicsView::resizeEvent(event);
}

void WaveView::updateGraphics(void) {
  // check all pixmaps are still in the correct position:
  qDebug() << __func__;
  if(wave) {
    unsigned int indexLeft = horizontalScrollBar()->value()/TILEWIDTH % pixmaps.size();
    unsigned int wavePos = (horizontalScrollBar()->value()/TILEWIDTH) * TILEWIDTH*zoomLevel;
    
    for(unsigned int i=0; i<pixmaps.size(); ++i) {
      auto index = (indexLeft + i) % pixmaps.size();
      auto pixmap = pixmaps[index];
      if (fabs(pixmap->x()*zoomLevel - wavePos) > 1) {
        drawPixmap(pixmap, wavePos);
      }
      wavePos += TILEWIDTH*zoomLevel;
    }
  }
}

void WaveView::drawPixmap(QGraphicsPixmapItem *item, unsigned int wavePos) {
  auto map = QPixmap(TILEWIDTH, height);
  map.fill(Qt::transparent);

  qDebug() << "draw new pixmap at wavePos" << wavePos;
  
  auto ampl = 0.5*height;
  auto offset = wavePos*channels;
  if (offset < wave->size()) {
    // draw pixmap
    vector<QPointF> points;
    points.reserve(1+TILEWIDTH*zoomLevel);
    for(unsigned int j = 0 ; j<= (TILEWIDTH*zoomLevel) && (offset+j*channels) < wave->size();++j) {
      points.push_back(QPointF(j/zoomLevel, ampl*wave->at(offset+j*channels) + ampl) );
    }
    QPainter painter(&map);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);
    qDebug() << "drawing" << points.size() <<"samples";
    painter.drawPolyline(&points[0], points.size());
  }
  
  qDebug() << "set item position to" << (wavePos/zoomLevel);
  item->setPos(QPointF(wavePos/zoomLevel, 0.0));
  item->setVisible(true);
  item->setPixmap(map);
}
