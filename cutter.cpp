#include "cutter.h"
#include "jackplayer.h"
#include "wave.h"
#include "waveview.h"

#include <sndfile.hh>

#include <QObject>
#include <QGraphicsItem>
#include <QMouseEvent>
#include <QDebug>
#include <QFile>
#include <QMessageBox>

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

Cutter::Cutter(QObject *parent, JackPlayer *p, WaveView *v) : 
  QObject(parent), player(p), view(v), slice(nullptr), sliceStart(nullptr), sliceEnd(nullptr), toDelete(nullptr), deleteMenu(), selectionStart(0), selectionEnd(0) {
  auto deleteAction = new QAction("delete", &deleteMenu);
  deleteMenu.addAction(deleteAction);
  connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteMarker()) );

  auto cutAction = new QAction("add cut", &addMenu);
  addMenu.addAction(cutAction);
  connect(cutAction, SIGNAL(triggered()), this, SLOT(addCut()) );
}

void Cutter::setView(WaveView *v) {
  view = v;
  connect(view, SIGNAL(waveClicked(QMouseEvent *)),
          this, SLOT(handleMousePress(QMouseEvent *)));
  connect(view, SIGNAL(rangeSelected(unsigned int, unsigned int)),
          this, SLOT(selectRange(unsigned int, unsigned int)));
}

void Cutter::handleMousePress(QMouseEvent *event) {
  auto scenePos = view->mapToScene(event->x(),event->y());
  auto button = event->button();
  auto iAfter = std::find_if(cuts.begin(), cuts.end(), 
                             [scenePos] (decltype(cuts[0]) a) 
                             { return ( a->pos().x() > scenePos.x() );} );

  auto clickedMarker = view->markerAt(event->pos());
  if (clickedMarker && button == Qt::RightButton) {
    toDelete = static_cast<Cutter::Marker*>(clickedMarker);
    deleteMenu.popup(QCursor::pos());
  } else if (button == Qt::RightButton) { // right click: show context menu
    addMenuPos = scenePos.x();
    addMenu.popup(QCursor::pos());
  } else if (button == Qt::LeftButton) {
    if(event->modifiers() & Qt::ControlModifier) { // CTRL-click: add cut
      addCut(scenePos.x());
    } else if (!clickedMarker 
               && iAfter != cuts.begin() && iAfter != cuts.end()) { // left click inside a slice
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

void Cutter::addCut() {
  addCut(addMenuPos);
  addMenuPos = 0;
}

void Cutter::addCut(qreal scene_x) {
  if(scene_x < 0) {
    // when clicked left of the wave, add a cut at the left border:
    scene_x = 0;
  } else if (scene_x > view->scene()->width()) {
    // when clicked right of the wave, add a cut at the right:
    scene_x = view->scene()->width();
  }
  auto i_insert = std::find_if(cuts.begin(), cuts.end(),
                               [scene_x] (decltype(cuts[0]) a)
                               { return (a->pos().x() >= scene_x );} );
  // if we already have a cut at that position, don't add another
  if ( i_insert != cuts.end() 
       && (*i_insert)->pos().x() == scene_x)
    return;
  
  auto newMarker = addMarker(scene_x);
  connect(static_cast<Marker *>(newMarker), SIGNAL(positionChanged(unsigned int)),
          this, SLOT(markerMoved(unsigned int)) );
  if (i_insert != cuts.end()) {
    cuts.insert(i_insert, newMarker);
  } else {
    cuts.push_back(newMarker);
  }
  emit cutsChanged(cuts.size() > 1);
  updateLoop();
}

Cutter::Marker *Cutter::addMarker(unsigned int pos) {
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
  player->setLoopStart(0);
  player->setLoopEnd(view->scene()->width());

  switch(loopState) {
  case Slices:
    if (cuts.size() >= 2) {
      player->setLoopStart(cuts.front()->pos().x() );
      player->setLoopEnd(cuts.back()->pos().x() );
    } 
    break;
  case Selection:
    if(selectionEnd > selectionStart) {
      player->setLoopStart(selectionStart);
      player->setLoopEnd(selectionEnd);
    }
    break;
  default:
    break;
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

void Cutter::play() {
  if(selectionEnd > selectionStart) {
    // we have a selection, so play that
    player->play(selectionStart, selectionEnd);
  } else if (sliceStart && sliceEnd) {
    playSlice();
  } else {
    player->play();
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

  // update loopState before updateing the loop
  if(selectionEnd > selectionStart) {
    loopState = Selection;
  } else {
    loopState = Slices;
  }
  updateLoop();
  player->loop();
}

void Cutter::clear(void) {
  cuts.clear();
  emit cutsChanged(false); // there are no slices -> disable export
  slice = nullptr;
  updateSlice(nullptr, nullptr);
  loopState = None;
  player->setLoopStart(0);
  qDebug() << __func__ << " set loop end to " << view->scene()->width();
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

  // soundfile parameters are the same for each slice:
  const int format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
  const int sampleRate = 44100;
  const Wave& wave = player->getCurWave();

  // decimals needed for the number of cuts: 1 + log10(number of slices)
  unsigned int nDecimals = 1+floor(log10(cuts.size()-1));

  unsigned int nWaves=1;
  bool overwriteAll = false;
  for(auto iCut = cuts.begin(); (1+iCut) != cuts.end();++iCut) {
    unsigned int start = (*iCut)->x();
    unsigned int end = (*(1+iCut))->x();
    auto fileName = path + QString("%1.wav").arg(nWaves++, nDecimals, 10, QChar('0'));

    if(QFile::exists(fileName) && !overwriteAll) {
      QMessageBox askOverwrite(QMessageBox::Question,
                               "Overwrite existing file?",
                               "A file named " + fileName + " already exists. Do you want to replace it?",
                               QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll);
      askOverwrite.setDefaultButton(QMessageBox::NoToAll);
      switch (askOverwrite.exec()) {
      case QMessageBox::Yes:
        break;
      case QMessageBox::YesToAll:
        overwriteAll = true;
        break;
      case QMessageBox::No:
        continue;
        break;
      case QMessageBox::NoToAll:
        return; 
        break;
      }
    }

    SndfileHandle outFile(fileName.toLatin1().constData(), SFM_WRITE,
                          format, wave.channels, sampleRate);
    if (!outFile) {
      auto errMsg =  ("Error opening file " + fileName).toLatin1().constData();
      throw std::runtime_error(errMsg);
    }
    outFile.writef(&wave.samples[start*wave.channels], end-start);
  }
  
}

void Cutter::selectRange(unsigned int selectionStart, unsigned int selectionEnd) {
  this->selectionStart = selectionStart;
  this->selectionEnd = selectionEnd;

  switch(loopState) {
  case Selection:
    // if we are already looping a selection, stop playing when a new
    // selection is made (don't automatically loop the new selection)
    player->stop();
    loopState = None;
    break;
  default:
    break;
  }
  updateLoop();
}
