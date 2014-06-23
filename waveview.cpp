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
  wave(nullptr)
{
  setScene(new QGraphicsScene(this));
  //  auto width = QApplication::desktop()->screenGeometry().width();
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void WaveView::drawWave(const vector<float> *wave, unsigned int nChannels) {
  pixmaps.clear();
  scene()->clear();
  this->wave = wave;
  channels = nChannels;
  height = 0.6 * QApplication::desktop()->screenGeometry().height();

  setSceneRect(0,0,wave->size()/channels, height);
  horizontalScrollBar()->setSliderPosition(0);

  auto ampl = 0.5*height;
  scene()->addLine(0, ampl, wave->size()/channels, ampl);

  // draw pixmaps:
  pixmaps.resize(1+width()/TILEWIDTH);
  for(unsigned int i=0; i<pixmaps.size(); ++i) {
    pixmaps[i] = new QGraphicsPixmapItem();
    scene()->addItem(pixmaps[i]);
    drawPixmap(pixmaps[i], i*TILEWIDTH);
  }
}

void WaveView::scrollContentsBy(int dx, int dy) {
  QGraphicsView::scrollContentsBy(dx,dy);
  updateGraphics();
}

void WaveView::resizeEvent(QResizeEvent* event ) {
  if(wave) {
    while(event->size().width() + TILEWIDTH > pixmaps.size() * TILEWIDTH) {
      auto item = new QGraphicsPixmapItem();
      qDebug() << __func__ << "add item to scene";
      scene()->addItem(item);
      qDebug() << __func__ << "add item to pixmaps";
      pixmaps.push_back(item);
    }
    updateGraphics();
  }
}

void WaveView::updateGraphics(void) {
  // check all pixmaps are still in the correct position:
  qDebug() << __func__;
  unsigned int indexLeft = horizontalScrollBar()->value()/TILEWIDTH % pixmaps.size();
  unsigned int wavePos =  (horizontalScrollBar()->value()/TILEWIDTH) * TILEWIDTH;
  for(unsigned int i=0; i<pixmaps.size(); ++i) {
    auto index = (indexLeft + i) % pixmaps.size();
    auto pixmap = pixmaps[index];
    if (pixmap->x() != wavePos) {
      drawPixmap(pixmap, wavePos);
    }
    wavePos += TILEWIDTH;
  }
}

void WaveView::drawPixmap(QGraphicsPixmapItem *item, unsigned int wavePos) {
  auto map = QPixmap(TILEWIDTH, height);
  map.fill(Qt::transparent);

  qDebug() << "draw new pixmap at wavePos" << wavePos << endl;
  
  auto ampl = 0.5*height;
  if (wavePos < wave->size()/channels) {
    // draw pixmap
    vector<QPointF> points;
    points.reserve(1+TILEWIDTH);
    for(unsigned int j = 0 ; j<= (TILEWIDTH) && j < (wave->size()/channels-wavePos); ) {
      points.push_back(QPointF(j, ampl*wave->at(wavePos+j) + ampl) );
      j+=channels;
    }
    QPainter painter(&map);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);
    painter.drawPolyline(&points[0], points.size());
  }
  
  qDebug() << "set item position" << endl;
  item->setPos(QPointF(wavePos, 0.0));
  item->setVisible(true);
  item->setPixmap(map);
}
