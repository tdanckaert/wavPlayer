#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "wave.h"

#include <iostream>

#include <QFileDialog>
#include <QShortcut>
#include <QDebug>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QApplication>
#include <QDesktopWidget>

using std::vector;
using std::cerr;
using std::endl;

void MainWindow::drawWave(const Wave *wave) {
  auto scene = ui->waveOverview->scene();
  scene->clear();

  ui->waveOverview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->waveOverview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto height = 0.6 *  QApplication::desktop()->screenGeometry().height();
  vector<QPointF> points;
  // todo: handle stereo
  points.reserve(wave->samples.size());
  float ampl = 0.5*height;
  for(unsigned int i=0; i<wave->samples.size(); ++i) {
    points.push_back(QPointF(i/150.0, ampl*wave->samples[i] +ampl ) );
  }

  auto map = QPixmap(wave->samples.size()/150.0, height);
  map.fill(Qt::transparent);

  QPainter painter(&map);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);

  painter.drawPolyline(&points[0], points.size());
  auto item = scene->addPixmap(map);

  ui->waveOverview->fitInView(item);

  // strange hack to set scene rectangle to actual bounding box of wave (taking into account transparency)
  ui->waveOverview->setSceneRect(
              QRectF(ui->waveOverview->mapToScene(QPoint(0,0)),
                     ui->waveOverview->mapToScene(QPoint(ui->waveOverview->width(),ui->waveOverview->height()))));

  qDebug() << "Scene rect: " << ui->waveOverview->sceneRect() << endl
           << "pixmap dimension: " << map.width() << " x " << map.height();

}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
  ui->setupUi(this);

  ui->waveOverview->setScene(new QGraphicsScene(this));
  ui->waveOverview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->waveOverview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto shortcutPlay = new QShortcut(QKeySequence(Qt::Key_Space), this);
  connect(shortcutPlay, SIGNAL(activated()), &player, SLOT(pause()));
  connect(&player, SIGNAL(positionChanged(unsigned int)), ui->zoomView, SLOT(updateIndicator(unsigned int)) );
  connect(ui->zoomView, SIGNAL(playCut(unsigned int,unsigned int)), &player, SLOT(play(unsigned int, unsigned int)) );
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionQuit_triggered()
{
    QCoreApplication::quit();
}

void MainWindow::on_actionOpen_triggered()
{
  auto pWave = player.loadWave(QFileDialog::getOpenFileName());
  if (pWave != nullptr) {
    drawWave(pWave);
    ui->zoomView->drawWave(pWave);
  }
}

void MainWindow::on_splitter_splitterMoved(int pos __attribute__ ((unused)), int index __attribute__ ((unused)) )
{
  auto item = ui->waveOverview->scene()->items().first();

  if (item != nullptr)
    ui->waveOverview->fitInView(item);
}

void MainWindow::on_actionStop_triggered()
{
  player.stop();
}

void MainWindow::on_actionLoop_triggered()
{
  player.loop();
}

void MainWindow::on_actionPlay_triggered()
{
  player.play();
}

void MainWindow::on_actionPause_triggered()
{
  player.pause();
}

void MainWindow::on_actionPlayHalf_triggered()
{
  player.play(ui->zoomView->scene()->width()/2,
              ui->zoomView->scene()->width());
}
