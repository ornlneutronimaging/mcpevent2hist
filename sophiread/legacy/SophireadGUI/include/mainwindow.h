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

#include "clustering.h"
#include "tpx3.h"

#define HSIZEM1 4095
// #define DSCALE 8.0
#define ROTANGLE -1.0

namespace Ui {
class MainWindow;
}

/**
 * @brief The UI for SophireadDisplay.
 *
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

  QVector<double> vhisto;
  double *my2dhisto;

  QElapsedTimer myelapsedtime;  // track time used to process data
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
  double range_max;
  double range_min;
  std::vector<NeutronEvent> m_events;
  int m_total_hits;
  int m_total_events;
  ClusteringAlgorithm *clustering_alg;
};

#endif  // MAINWINDOW_H
