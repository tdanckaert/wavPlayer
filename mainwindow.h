#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <set>
#include <vector>

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

  std::set<std::vector<float> > samples;

  void drawWave(const std::set<std::vector<float> >::iterator&);

private slots:
  void on_actionQuit_triggered();
  void on_actionOpen_triggered();
  void pause();
  void cleanup(void);

};

#endif // MAINWINDOW_H
