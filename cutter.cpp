#include "cutter.h"
#include "jackplayer.h"

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QDebug>

class Cutter::Marker : public  QGraphicsPolygonItem {
public:
  Marker(Cutter &parent) : QGraphicsPolygonItem(), parent(parent) {};

private:
  Cutter& parent;

  QVariant itemChange(GraphicsItemChange change, const QVariant &value) {
    qDebug() << __func__ ;
    if (change == ItemScenePositionHasChanged) {
      // value is the new position.
      QPointF newPos = value.toPointF();
      qDebug() << __func__ << "newPos: "  << newPos;
      newPos.setY(0);
      setPos(newPos);
      std::sort(parent.cuts.begin(), parent.cuts.end(),
                [] (QGraphicsItem *a, QGraphicsItem *b) { return a->pos().x() < b->pos().x(); } );
      return newPos;
    } 
    return QGraphicsPolygonItem::itemChange(change, value);
  };
};

void Cutter::setView(QGraphicsView *v) {
  view = v;
  connect(view, SIGNAL(waveClicked(Qt::MouseButton,unsigned int)),
          this, SLOT(handleMousePress(Qt::MouseButton, unsigned int)) );
}

void Cutter::handleMousePress(Qt::MouseButton button, unsigned int pos) {
  auto iAfter = std::find_if(cuts.begin(), cuts.end(), 
                             [pos] (decltype(cuts[0]) a) { return ( a->pos().x() > pos );});
  if (button == Qt::RightButton) {
    if (iAfter != cuts.end()) {
      cuts.insert(iAfter, addCut(pos));
    } else {
      cuts.push_back(addCut(pos));
    }
  } else if (button == Qt::LeftButton && 
             iAfter != cuts.begin() && iAfter != cuts.end() ) {
    player->play((*(iAfter-1))->pos().x(), (*iAfter)->pos().x());
  }
}

QGraphicsItem *Cutter::addCut(unsigned int pos) {
  QPen pen;
  pen.setColor(Qt::red);
  pen.setCosmetic(true);

  QPolygonF poly;
  poly << QPointF(0,0) << QPointF(0,15) << QPointF(10,0) << QPointF(0,0);
  qDebug() << __func__ << "polygon closed?" << poly.isClosed();
  auto p=new Marker(*this);
  p->setPen(pen);
  p->setBrush(Qt::red);
  p->setPolygon(poly);
  p->setFlag(QGraphicsItem::ItemIsMovable);
  p->setFlag(QGraphicsItem::ItemIgnoresTransformations);
  p->setFlag(QGraphicsItem::ItemSendsScenePositionChanges);
  view->scene()->addItem(p);

  p->setPos(pos,0);

  auto line = new QGraphicsLineItem(p);
  line->setPen(pen);
  line->setLine(QLineF(0, 0, 0, view->mapFromScene(0, view->scene()->sceneRect().height() ).y() ) );
  return p;
}
