#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "jackplayer.h"
#include "cutter.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT
  
public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

private:
  Ui::MainWindow *ui;
  JackPlayer player;
  Cutter cutter;

private slots:
  void on_actionQuit_triggered();
  void on_actionOpen_triggered();
  void on_actionPlay_triggered();
  void on_actionPlayHalf_triggered();
  void on_actionLoop_triggered();
  void on_actionPause_triggered();
  void on_actionStop_triggered();
};

#endif // MAINWINDOW_H
