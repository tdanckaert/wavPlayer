#include "cutter.h"
#include "jackplayer.h"

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QMouseEvent>
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
      if (newPos.y() != 0) {
        newPos.setY(0);
        setPos(newPos);
        parent.sortMarkers();
      }
      return newPos;
    } 
    return QGraphicsItem::itemChange(change, value);
  };
};

void Cutter::setView(QGraphicsView *v) {
  view = v;
  connect(view, SIGNAL(waveClicked(Qt::MouseButton,unsigned int)), this, SLOT(handleMousePress(Qt::MouseButton, unsigned int)) );
}

void Cutter::sortMarkers(void) {
  std::sort(cuts.begin(), cuts.end(),
            [] (QGraphicsItem *a, QGraphicsItem *b) { return a->pos().x() < b->pos().x(); } );
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
  pen.setBrush(Qt::red);
  pen.setCosmetic(true);

  QPolygonF poly;
  poly << QPointF(0,0) << QPointF(0,20) << QPointF(15,0) << QPointF(0,0);
  auto p=new Marker(*this);
  view->scene()->addItem(p);
  p->setPen(pen);
  p->setPolygon(poly);
  p->setFlag(QGraphicsItem::ItemIsMovable);
  p->setFlag(QGraphicsItem::ItemIgnoresTransformations);
  p->setFlag(QGraphicsItem::ItemSendsScenePositionChanges);

  p->setPos(pos,0);

  auto line = new QGraphicsLineItem(p);
  line->setPen(pen);
  line->setLine(QLineF(0, 0, 0, view->mapFromScene(0, view->scene()->sceneRect().height() ).y() ) );
  return p;
}
