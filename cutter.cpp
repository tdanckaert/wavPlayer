#include "cutter.h"
#include "jackplayer.h"
#include "wave.h"

#include <sndfile.hh>

#include <QObject>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QDebug>

#include <math.h>
#include <cassert>
#include <stdexcept>

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

Cutter::Cutter(QObject *parent, JackPlayer *p, QGraphicsView *v) : 
  QObject(parent), player(p), view(v), slice(nullptr), sliceStart(nullptr), sliceEnd(nullptr), toDelete(nullptr), deleteMenu() {
  auto deleteAction = new QAction("delete", &deleteMenu);
  deleteMenu.addAction(deleteAction);
  connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteMarker()));
}

void Cutter::setView(QGraphicsView *v) {
  view = v;
  connect(view, SIGNAL(waveClicked(Qt::MouseButton, QPointF)),
          this, SLOT(handleMousePress(Qt::MouseButton, QPointF)) );
}

void Cutter::handleMousePress(Qt::MouseButton button, QPointF scenePos) {
  auto iAfter = std::find_if(cuts.begin(), cuts.end(), 
                             [scenePos] (decltype(cuts[0]) a) 
                             { return ( a->pos().x() > scenePos.x() );} );
  auto clickedItem = view->scene()->itemAt(scenePos, view->transform());
  bool clickedOnMarker = clickedItem && clickedItem->zValue() >= 1.0;

  if (button == Qt::RightButton) {
    if( clickedOnMarker) {
      toDelete = static_cast<Cutter::Marker*>(clickedItem);
      deleteMenu.popup(QCursor::pos());
    } else {
      if(scenePos.x() < 0) {
        // when clicked left of the wave, add a cut at the left border:
        scenePos.setX(0);
      } else if (scenePos.x() > view->scene()->width()) {
        // when clicked right of the wave, add a cut at the right:
        scenePos.setX(view->scene()->width());
      }
      // if we already have a cut at that position, don't add another
      if ( std::find_if(cuts.begin(), cuts.end(),
                        [scenePos] (decltype(cuts[0]) a)
                        { return (a->pos().x() == scenePos.x() );} ) != cuts.end() ) {
        return;
      }

      auto newCut = addCut(scenePos.x() );
      connect(static_cast<Marker *>(newCut), SIGNAL(positionChanged(unsigned int)),
              this, SLOT(markerMoved(unsigned int)) );
      if (iAfter != cuts.end()) {
        cuts.insert(iAfter, newCut);
      } else {
        cuts.push_back(newCut);
      }
      emit cutsChanged(cuts.size() > 1);
      updateLoop();
    }
  } else if (button == Qt::LeftButton && 
             iAfter != cuts.begin() && iAfter != cuts.end() ) {
    if( !clickedOnMarker) {
      updateSlice(*(iAfter-1),*iAfter);
      playSlice();
    }
  }
}

void Cutter::drawSlice(void) {
  if(sliceStart && sliceEnd) {
    auto xStart = sliceStart->pos().x();
    auto xEnd = sliceEnd->pos().x();
    auto rect = QRectF(xStart, -5, xEnd-xStart,
                       10+ view->scene()->height() );
    if(slice) {
      slice->setVisible(true);
      slice->setRect(rect);
    } else {
      slice = view->scene()->addRect(rect);
      slice->setPen(QPen(Qt::lightGray));
      slice->setBrush(Qt::lightGray);
      slice->setZValue(-1);
    }
  } else if (slice) {
    slice->setVisible(false);
  }
}

Cutter::Marker *Cutter::addCut(unsigned int pos) {
  QPen pen;
  pen.setColor(Qt::red);
  pen.setCosmetic(true);

  QPolygonF poly;
  poly << QPointF(0,-5) << QPointF(0,15) << QPointF(13.3,-5) << QPointF(0,-5);
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
  p->setZValue(1.0);

  auto line = new VerticalLine(p);
  line->setPen(pen);

  // check if we have created a new marker inside the active slice:
  if(sliceStart && pos > sliceStart->pos().x()
     && pos < sliceEnd ->pos().x() ) {
    if ( (pos - sliceStart->pos().x())
         < (sliceEnd->pos().x() - pos) ) {
      // new marker is closer to sliceStart than to sliceEnd
      // -> active slice is between Start and new
      updateSlice(sliceStart, p);
    } else {
      // closer to sliceEnd -> active slice is between new and End
      updateSlice(p, sliceEnd);
    }
  }

  return p;
}

void Cutter::updateLoop(void) {
  if(cuts.size() >= 2) {
    player->setLoopStart(cuts.front()->pos().x() );
    player->setLoopEnd(cuts.back()->pos().x() );
  }
}

void Cutter::markerMoved(unsigned int pos) {
  std::sort(cuts.begin(), cuts.end(),
            [] (QGraphicsItem *a, QGraphicsItem *b) { return a->pos().x() < b->pos().x(); });

  updateLoop();

  auto movedMarker = qobject_cast<Cutter::Marker *>(QObject::sender());
  if(sliceStart) {
    if (sliceStart->pos().x() > sliceEnd->pos().x()) {
      updateSlice(sliceEnd, sliceStart);
    }
    // Check if one of the current sliceStart/End markers has moved,
    // and rebuild the current slice around the marker which has *not*
    // moved.
    if(movedMarker == sliceStart) {
      auto iMarker = std::find(cuts.begin(), cuts.end(), sliceEnd);
      if (iMarker == cuts.begin()) {
        ++iMarker;
      }
      updateSlice(*(iMarker-1), *iMarker);
    } else if(movedMarker == sliceEnd) {
      auto iMarker = std::find(cuts.begin(), cuts.end(), sliceStart);
      if (iMarker == (cuts.end()-1) ) {
        --iMarker;
      }
      updateSlice(*iMarker, *(1+iMarker));
    } else if(pos > sliceStart->pos().x() && pos < sliceEnd->pos().x()) {
      // third case: another marker was moved into the current slice.
      // Rebuild the slice around the closest pair of markers.
      if( (pos -sliceStart->pos().x()) < (sliceEnd->pos().x() - pos) ) {
        // new slice is closer to sliceStart -> make the slice between
        // sliceStart & new slice the active slice
        updateSlice(sliceStart, movedMarker);
      } else {
        // new slice is closer to sliceEnd -> (new slice, sliceEnd)
        // becomes the active slice
        updateSlice(movedMarker, sliceEnd);
      }
    }
  }
}

void Cutter::playSlice(void) {
  if(sliceStart && sliceEnd) {
    player->play(sliceStart->pos().x(), sliceEnd->pos().x() );
  }
}

void Cutter::nextSlice(void) {
  if(sliceStart) {
    auto current = std::find(cuts.begin(), cuts.end(), sliceStart);
    qDebug() << "cuts: " << cuts.size();
    ++current;
    if(current == cuts.end() || current == (cuts.end()-1) ) {
      qDebug() << "at last cut, wraparound" ;
      current = cuts.begin();
    }
    updateSlice(*current, *(1+current));
  } else if (cuts.size() >= 2) {
    // if no slice is active, make the first slice the active one
    updateSlice(cuts[0], cuts[1]);
  }
  playSlice();
}

void Cutter::prevSlice(void) {
  if(sliceStart) {
    auto current = std::find(cuts.begin(), cuts.end(), sliceStart);
    assert(current != cuts.end());
    if (current == cuts.begin()) {
      current = --cuts.end();
    }
    updateSlice(*(current-1), *current);
  } else if (cuts.size() >= 2) {
    // if no slice is active, make the last slice the active one
    updateSlice( *(cuts.end() - 2), *(cuts.end() -1) );
  }
  playSlice();
}

void Cutter::loop(void) {
  player->loop();
}

void Cutter::clear(void) {
  cuts.clear();
  emit cutsChanged(false); // there are no slices -> disable export
  player->setLoopStart(0);
  player->setLoopEnd(view->scene()->width());
}

void Cutter::deleteMarker() {
  assert(toDelete);
  auto iMarker = std::find(cuts.begin(), cuts.end(), toDelete);
  if(toDelete == sliceStart) {
    if(iMarker != cuts.begin()) {
      updateSlice(*(iMarker -1), sliceEnd);
    } else {
      updateSlice(nullptr, nullptr);
    }
  } else if (toDelete == sliceEnd) {
    if( (iMarker + 1) != cuts.end() ) {
      updateSlice(sliceStart, *(iMarker+1));
    } else {
      updateSlice(nullptr, nullptr);
    }
  }
  cuts.erase(iMarker);
  emit cutsChanged(cuts.size() > 1);

  updateLoop();

  delete toDelete;
  toDelete = nullptr;
}

inline void Cutter::updateSlice(Marker *start, Marker *end) {
  sliceStart = start;
  sliceEnd = end;
  drawSlice();
}

void Cutter::exportSamples(const QString& path) const {
  assert(cuts.size() > 1); // need at least one slice to export -> minimum of 2 cuts

  const int format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
  const int sampleRate = 44100;
  const Wave& wave = player->getCurWave();

  // decimals needed for the number of cuts: 1 + log10(number of slices)
  unsigned int nDecimals = 1+floor(log10(cuts.size()-1));

  unsigned int nWaves=1;
  for(auto iCut = cuts.begin(); (1+iCut) != cuts.end();++iCut) {
    unsigned int start = (*iCut)->x();
    unsigned int end = (*(1+iCut))->x();
    auto fileName = path + QString("%1.wav").arg(nWaves++, nDecimals, 10, QChar('0'));
    SndfileHandle outFile(fileName.toAscii().constData(), SFM_WRITE,
                          format, wave.channels, sampleRate);
    if (!outFile) {
      auto errMsg =  ("Error opening file " + fileName).toAscii().constData();
      throw std::runtime_error(errMsg);
    }
    outFile.writef(&wave.samples[start*wave.channels], end-start);
  }
  
}
