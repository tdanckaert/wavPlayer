#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "wave.h"

#include <iostream>
#include <stdexcept>

#include <QFileDialog>
#include <QShortcut>
#include <QDebug>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QApplication>
#include <QDesktopWidget>
#include <QMessageBox>

using std::vector;
using std::cerr;
using std::endl;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    cutter(this, &player, ui->zoomView)
{
  ui->setupUi(this);

  ui->waveOverview->setScene(new QGraphicsScene(this));
  ui->waveOverview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->waveOverview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->waveOverview->setInteractive(false);

  cutter.setView(ui->zoomView);

  connect(&player, SIGNAL(positionChanged(unsigned int)), ui->zoomView, SLOT(updateIndicator(unsigned int)) );
  connect(&player, SIGNAL(positionChanged(unsigned int)), ui->waveOverview, SLOT(updateIndicator(unsigned int)) );

  auto shortcutPrevSlice = new QShortcut(QKeySequence(Qt::Key_Left), this);
  auto shortcutNextSlice = new QShortcut(QKeySequence(Qt::Key_Right), this);
  auto shortcutPlay = new QShortcut(QKeySequence(Qt::Key_Space), this);

  connect(shortcutPrevSlice, SIGNAL(activated()), &cutter, SLOT(prevSlice()) );
  connect(shortcutNextSlice, SIGNAL(activated()), &cutter, SLOT(nextSlice()) );
  connect(shortcutPlay, SIGNAL(activated()), &cutter, SLOT(playSlice()) );

  enableExport(false);
  connect(&cutter, SIGNAL(cutsChanged(bool)), this, SLOT(enableExport(bool)) );
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
  auto fileName = QFileDialog::getOpenFileName();
  if (!fileName.isEmpty()) {
    try {
      auto pWave = player.loadWave(Wave::openSoundFile(fileName));
      ui->waveOverview->drawWave(pWave);
      ui->zoomView->drawWave(pWave);
      cutter.clear();
    } catch (std::runtime_error& e) {
      QMessageBox msgBox;
      msgBox.setText("Error opening file.");
      msgBox.exec();
      on_actionOpen_triggered();
    }
  }
}

void MainWindow::on_actionStop_triggered()
{
  player.stop();
}

void MainWindow::on_actionLoop_triggered()
{
  cutter.loop();
}

void MainWindow::on_actionPlay_triggered()
{
  player.play();
}

void MainWindow::on_actionPause_triggered()
{
  player.pause();
}

void MainWindow::on_actionExport_triggered()
{
  auto path = QFileDialog::getSaveFileName(this, tr("Export Directory"));
  if (!path.isEmpty())
    cutter.exportSamples(path);
}

void MainWindow::enableExport(bool enabled) {
  ui->actionExport->setEnabled(enabled);
}
