#include "cutter.h"
#include "jackplayer.h"

#include <QObject>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QDebug>

#include <cassert>

class Cutter::Marker : public QObject, public QGraphicsPolygonItem {
  Q_OBJECT

public:
  Marker(QObject *parent = 0) : QObject(parent), QGraphicsPolygonItem() {};

private:

  QVariant itemChange(GraphicsItemChange change, const QVariant &value) {
    qDebug() << __func__ ;
    if (change == ItemScenePositionHasChanged) {
      // value is the new position.
      QPointF newPos = value.toPointF();
      qDebug() << __func__ << "newPos: "  << newPos;
      if(newPos.x() < 0) {
        newPos.setX(0);
      } else if (newPos.x() > scene()->width()) {
        newPos.setX(scene()->width());
      }
      newPos.setY(0);
      setPos(newPos);
      emit positionChanged(static_cast<unsigned int>(newPos.x()));
      return newPos;
    } 
    return QGraphicsPolygonItem::itemChange(change, value);
  };

signals:
  void positionChanged(unsigned int pos);
};

class Cutter::VerticalLine : public QGraphicsLineItem {

public:
  VerticalLine(QGraphicsItem *parentItem) : QGraphicsLineItem(parentItem) {};
  
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    setLine(0, 0, 0, QPointF(0,scene()->views().first()->height()).y());
    QGraphicsLineItem::paint(painter, option, widget);
  };
};

#include "cutter.moc" // necessary to force moc to process this file's Q_OBJECT macros?

void Cutter::setView(QGraphicsView *v) {
  view = v;
  connect(view, SIGNAL(waveClicked(Qt::MouseButton,unsigned int)),
          this, SLOT(handleMousePress(Qt::MouseButton, unsigned int)) );
}

void Cutter::handleMousePress(Qt::MouseButton button, unsigned int pos) {
  auto iAfter = std::find_if(cuts.begin(), cuts.end(), 
                             [pos] (decltype(cuts[0]) a) { return ( a->pos().x() > pos );});
  if (button == Qt::RightButton) {
    auto newCut = addCut(pos);
    connect(static_cast<Marker *>(newCut), SIGNAL(positionChanged(unsigned int)),
            this, SLOT(markerMoved(unsigned int)) );
    if (iAfter != cuts.end()) {
      cuts.insert(iAfter, newCut);
    } else {
      cuts.push_back(newCut);
    }
  } else if (button == Qt::LeftButton && 
             iAfter != cuts.begin() && iAfter != cuts.end() ) {
    sliceStart = *(iAfter-1);
    sliceEnd = *(iAfter);
    drawSlice();
  }
}

void Cutter::drawSlice(void) {
  if(sliceStart && sliceEnd) {
    auto xStart = sliceStart->pos().x();
    auto xEnd = sliceEnd->pos().x();
    auto rect = QRectF(xStart, -5, xEnd-xStart,
                       10+ view->scene()->height() );
    if(slice) {
      slice->setRect(rect);
    } else {
      slice = view->scene()->addRect(rect);
      slice->setPen(QPen(Qt::lightGray));
      slice->setBrush(Qt::lightGray);
      slice->setZValue(-1);
    }
  }
}

QGraphicsItem *Cutter::addCut(unsigned int pos) {
  QPen pen;
  pen.setColor(Qt::red);
  pen.setCosmetic(true);

  QPolygonF poly;
  poly << QPointF(0,0) << QPointF(0,15) << QPointF(10,0) << QPointF(0,0);
  qDebug() << __func__ << "polygon closed?" << poly.isClosed();
  auto p=new Marker(0);
  p->setPen(pen);
  p->setBrush(Qt::red);
  p->setPolygon(poly);
  p->setFlag(QGraphicsItem::ItemIsMovable);
  p->setFlag(QGraphicsItem::ItemIgnoresTransformations);
  p->setFlag(QGraphicsItem::ItemSendsScenePositionChanges);
  view->scene()->addItem(p);
  p->setPos(pos,0);

  auto line = new VerticalLine(p);
  line->setPen(pen);

  return p;
}

void Cutter::markerMoved(unsigned int pos) {
  std::sort(cuts.begin(), cuts.end(),
            [] (QGraphicsItem *a, QGraphicsItem *b) { return a->pos().x() < b->pos().x(); });
  drawSlice();
}

void Cutter::playSlice(void) {
  if(sliceStart && sliceEnd) {
    player->play(sliceStart->pos().x(), sliceEnd->pos().x() );
  }
}

// TODO: handle cases where sliceStart > sliceEnd due to moving cuts
void Cutter::nextSlice(void) {
  if(sliceStart) {
    auto current = std::find(cuts.begin(), cuts.end(), sliceStart);
    qDebug() << "cuts: " << cuts.size();
    ++current;
    if(current == (cuts.end()-1) ) {
      qDebug() << "at last cut, wraparound" ;
      current = cuts.begin();
    }
    sliceStart = *current;
    sliceEnd = *(++current);
    drawSlice();
  }
}

void Cutter::prevSlice(void) {
  if(sliceStart) {
    auto current = std::find(cuts.begin(), cuts.end(), sliceStart);
    assert(current != cuts.end());
    if (current == cuts.begin()) {
      current = --cuts.end();
    }
    sliceEnd = *current;
    sliceStart  = *(--current);
    drawSlice();
  }
}
