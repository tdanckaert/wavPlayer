#include "waveview.h"
#include "wave.h"

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
  isDragging(false),
  selection(nullptr),
  zoomLevel(1.0),
  wave(nullptr)
{
  setScene(new QGraphicsScene(this));
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

  indicator = scene()->addLine(0,0,0,0);
}

void WaveView::drawWave(const Wave *wave) {
  pixmaps.clear();
  scene()->clear();

  QPen pen;
  pen.setCosmetic(true);
  pen.setColor(Qt::green);
  indicator = scene()->addLine(0,0,0,pixmapHeight(), pen);
  indicator->setZValue(0.5); // draw on top of pixmaps

  selection = scene()->addRect(QRectF(0.0,0.0,1,1));
  selection->setVisible(false);
  selection->setOpacity(0.3);
  selection->setBrush(Qt::blue);
  selection->setZValue(-0.5);

  this->wave = wave;

  scene()->setSceneRect(0,0,static_cast<float>(wave->samples.size()/wave->channels), pixmapHeight());
  fitInView(0,0,wave->samples.size()/wave->channels,pixmapHeight());

  maxAmplitude = fabs(*std::max_element(wave->samples.begin(), wave->samples.end(), 
                                        [] (decltype(wave->samples[0]) a, decltype(wave->samples[0]) b) { return fabs(a) < fabs(b); }));

  pen.setColor(Qt::black);
  scene()->addLine(0, 0.5*pixmapHeight(), static_cast<float>(wave->samples.size()/wave->channels), 0.5*pixmapHeight(), pen);

  horizontalScrollBar()->setSliderPosition(0);

  zoomLevel = 1/transform().m11();
}

unsigned int WaveView::visibleRange(void) {
  if (!isInteractive() ) {
    // when not interactive, always keep the entire wave visible:
    return scene()->sceneRect().width();
  } else {
    auto xLeft = static_cast<int>(mapToScene(QPoint(2,0)).x());
    // the width of the visible scene is equal to (viewport width - 4) (border of 2px on each side?)
    auto xRight = static_cast<int>(mapToScene(QPoint(std::max(viewport()->width()-2,0),0)).x());
    return xRight - xLeft;
  }
}

void WaveView::resizeEvent(QResizeEvent* event ) {
  QGraphicsView::resizeEvent(event);
  fitInView(QRectF(mapToScene(QPoint(2,0)),
                   QSizeF(visibleRange(), pixmapHeight()) ) );
}

void WaveView::wheelEvent(QWheelEvent *event) {
  if (isInteractive() ) {
    if (event->orientation() == Qt::Horizontal) {
      // scroll view horizontally
      QGraphicsView::wheelEvent(event);
    } else {
      // vertical: zoom in/out
      auto scaleFact = event->delta() > 0
        ? 1.10 : 1/1.10;
      // don't zoom in further if zoomLevel is already smaller than 1
      if (scaleFact < 1.0 || zoomLevel > 1.0) 
        setTransform(transform() * QTransform::fromScale(scaleFact,1.0));
    }
  }
}

void WaveView::mousePressEvent(QMouseEvent *event) {
  QGraphicsView::mousePressEvent(event);
  qDebug() << "Mouse event at"<< event->x() << event->y() << mapToScene(event->x(),event->y());
  if (wave) {
    if (markerAt(event->pos() ) ) {
      emit waveClicked(event);
    } else if (event->button() == Qt::LeftButton 
        && event->modifiers() == Qt::NoModifier) {
      // make previous selection disappear on a left-click
      selection->setVisible(false);
      emit rangeSelected(0, 0);

      isDragging = true;
      dragStart = event->pos();
    }
  }
}

QGraphicsItem* WaveView::markerAt(QPoint pos) {
  auto clickedItem = scene()->itemAt(mapToScene(pos), transform());
  if (clickedItem && clickedItem->zValue() >= 1.) {
    return clickedItem;
  } else {
    return nullptr;
  }
}

void WaveView::mouseReleaseEvent(QMouseEvent *event) {
  QGraphicsView::mouseReleaseEvent(event);
  if (event->button() == Qt::LeftButton) {
    isDragging = false;
    qDebug() << __func__ << ": x() = " << event->x() 
             << ", " << " dragStart.x() = " << dragStart.x();
    if (wave && 
        (event->modifiers() == Qt::ControlModifier || !selection->isVisible() ) ) {
      // pass on the event when Ctrl is pressed, or when it's just a click (no selection).
      emit waveClicked(event);
    }
    if (selection->isVisible() && event->modifiers() == Qt::NoModifier) {
      auto rect = selection->rect();
      if (rect.right() > scene()->width() ) {
        rect.setRight(scene()->width() );
        selection->setRect(rect);
      }
      if (rect.left() < 0 ) {
        rect.setLeft(0);
        selection->setRect(rect);
      }
      emit rangeSelected(rect.left(), rect.right() );
    }
  }
}

void WaveView::mouseMoveEvent(QMouseEvent *event) {
  QGraphicsView::mouseMoveEvent(event);
  if (isDragging) {
    auto pos = mapToScene(event->x(),event->y());
    auto dragStartX = mapToScene(dragStart).x();

    qreal xLeft, width;
    if (dragStartX < pos.x()) {
      xLeft = dragStartX;
      width = pos.x() - xLeft;
    } else {
      xLeft = pos.x();
      width = dragStartX - xLeft ;
    }
    auto rect = QRectF(QRectF(xLeft, -5, width,
                              10+ scene()->height()));
    selection->setRect(rect);
    // only select if mouse has moved more than 5 px
    selection->setVisible(fabs(event->x() - dragStart.x()) > 5);
  }
}

void WaveView::paintEvent(QPaintEvent *event) {
  updateGraphics();
  QGraphicsView::paintEvent(event);
};

inline void WaveView::checkZoomLevel(void) {
  auto stretchRatio = transform().m11()*zoomLevel;
  // we want to keep stretchRatio between 1.3 and 0.9
  // either we have to zoom out
  if (stretchRatio < 0.9 ) {
    qDebug() << "zoom out";
    zoomLevel = 1.29/transform().m11();
  } else if (stretchRatio > 1.3) {
    qDebug() << "zoom in";
    zoomLevel = 0.91/transform().m11();
  }
  qDebug() << "transform horizontal stretch: " << transform().m11() 
           << "zoomLevel:" << zoomLevel << "- ratio:" << stretchRatio;
}

void WaveView::updateIndicator(unsigned int playPos) {
  indicator->setPos(playPos, 0.0);
}

void WaveView::updateGraphics(void) {
  qDebug() << __func__;

  checkZoomLevel();

  int samplesPerTile = static_cast<int>(TILEWIDTH*zoomLevel);

  // make sure we have enough pixmaps to cover the whole visible area
  while(wave &&
        (viewport()->width()/0.9 + TILEWIDTH) > pixmaps.size() * TILEWIDTH) {
    auto item = new QGraphicsPixmapItem();
    scene()->addItem(item);
    pixmaps.push_back(item);
   }

  // check all pixmaps still have the correct position and zoomLevel:
  if (pixmaps.size()) {
    // get sample offset of the left border of the view
    auto xLeft = static_cast<int>(mapToScene(QPoint(2,0)).x());
    if (xLeft < 0)
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
  auto offset = wavePos*wave->channels;
  auto samplesPerTile = static_cast<unsigned int>(TILEWIDTH*zoomLevel);

  // when zoomed out, don't draw each sample (becomes too slow)
  unsigned int step = 1 + zoomLevel/2;

  if (offset < wave->samples.size()) {
    // draw pixmap
    vector<QPointF> points;
    points.reserve(1+2*samplesPerTile/step);
    for(unsigned int j=0 ; j<= samplesPerTile && (offset+j*wave->channels) < wave->samples.size(); j+=step) {
      float ymin = 1;
      float ymax = -1;
      unsigned int substep = 1+step/20;
      for(unsigned int k=0; k<step && (offset+ (k+j)*wave->channels < wave->samples.size()) ; k+=substep) {
        auto y =wave->samples[offset+(j+k)*wave->channels];
        if (y>ymax) {
          ymax = y;
        } else if (y < ymin) {
          ymin = y;
        }
      }
      if (ymin < 1)
        points.push_back(QPointF(j/zoomLevel, center -ampl*ymin) );
      if (ymax > -1)
        points.push_back(QPointF(j/zoomLevel, center -ampl*ymax) );
    }
    QPainter painter(&map);

    if (zoomLevel < 1.1) {
      QPen pen;
      pen.setWidth(2);
      painter.setPen(pen);
    }
    painter.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);
    painter.drawPolyline(&points[0], points.size());
  }
  
  qDebug() << "set item position to" << wavePos;
  item->setPos(QPointF(wavePos, 0.0));
  item->setTransform(QTransform::fromScale(zoomLevel,1.0));
  item->setVisible(true);
  item->setPixmap(map);
}

float WaveView::pixmapHeight(void) const {
  return 0.6 * QApplication::desktop()->screenGeometry().height();
}
