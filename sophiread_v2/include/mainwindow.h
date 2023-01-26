#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <qwt_interval.h>
#include <qwt_math.h>
#include <qwt_matrix_raster_data.h>
#include <qwt_plot_spectrogram.h>

#include <QElapsedTimer>
#include <QLabel>
#include <QMainWindow>
#include <QTimer>

#include "tpx3.h"

#define HSIZEM1 4095
#define DSCALE 8.0

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

  QElapsedTimer myelapsedtime;
  QwtMatrixRasterData *histo_data;
  QwtPlotSpectrogram *histo;

 public slots:
  void handletimer();
  void handlereadfile();
  void handlespinbox();
  void handlesavedata();

 private:
  Ui::MainWindow *ui;
  QTimer *mytimer;
  double rangemax;
  double rangemin;
};

#endif  // MAINWINDOW_H
