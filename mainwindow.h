#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <set>

class Wave;

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

  std::set<Wave> samples;

  void drawWave(const std::set<Wave>::iterator&);

private slots:
  void on_actionQuit_triggered();
  void on_actionOpen_triggered();
  void pause();
  void cleanup(void);

  void on_splitter_splitterMoved(int pos, int index);
};

#endif // MAINWINDOW_H
