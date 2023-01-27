#include "mainwindow.h"

#include <H5Cpp.h>
#include <QtConcurrent/qtconcurrentrun.h>
#include <math.h>
#include <omp.h>
#include <qwt_color_map.h>
#include <qwt_plot.h>
#include <qwt_scale_engine.h>

#include <QFile>
#include <QFileDialog>
#include <iostream>

#include "abs.h"
#include "ui_mainwindow.h"

// Custom class of color map
class ColorMap : public QwtLinearColorMap {
 public:
  ColorMap() : QwtLinearColorMap(Qt::darkBlue, Qt::darkRed) {
    addColorStop(0.2, Qt::blue);
    addColorStop(0.4, Qt::cyan);
    addColorStop(0.6, Qt::yellow);
    addColorStop(0.8, Qt::red);
  }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  // Set up the UI
  ui->setupUi(this);

  // Set color map range (num of Neutron counts)
  range_max = 100.0;
  range_min = 0.0;
  m_total_hits = 0;

  // Set up the timer
  mytimer = new QTimer(this);

  // Set up the plot
  const int ii = (int)(DSCALE * 512);
  const int jj = (int)(DSCALE * 500);
  //
  vhisto.resize(ii * ii);
  my2dhisto = vhisto.data();
  histo_data = new QwtMatrixRasterData();
  histo_data->setInterval(Qt::XAxis, QwtInterval(0, jj));
  histo_data->setInterval(Qt::YAxis, QwtInterval(0, jj));
  histo_data->setInterval(Qt::ZAxis, QwtInterval(range_min, range_max));
  histo_data->setValueMatrix(vhisto, ii);
  //
  histo = new QwtPlotSpectrogram;
  histo->setDisplayMode(QwtPlotSpectrogram::DisplayMode::ImageMode, true);
  histo->setColorMap(new ColorMap);  // QwtLinearColorMap(Qt::white,Qt::black)
  histo->setData(histo_data);
  histo->attach(ui->qwthistoPlot);
  ui->qwthistoPlot->replot();

  // Init the clustering algorithm
  // NOTE: use adaptive search box with a feature of 5.0 pixels
  //       use weighted centroid for approximating neutron event
  clustering_alg = new ABS(5.0);
  clustering_alg->set_method("centroid");

  // Connect signals and slots
  connect(mytimer, SIGNAL(timeout()), this, SLOT(handletimer()));
  connect(ui->selectfile, SIGNAL(clicked()), this, SLOT(handlereadfile()));
  connect(ui->updaterange, SIGNAL(clicked()), this, SLOT(handlespinbox()));
  connect(ui->savedata, SIGNAL(clicked()), this, SLOT(handlesavedata()));
}

MainWindow::~MainWindow() { delete ui; }

/**
 * @brief Periodically update triggered by internal timer
 *
 */
void MainWindow::handletimer() {
  // update percentage of data processed
  // TODO: need to find a way to adapte the percentage number
  //
  // update time used to process data
  auto elapsed = myelapsedtime.elapsed();
  ui->elapsed_time->setText(QString::number(elapsed));
  // update total number of hits
  ui->totalhits->setText(QString::number(m_total_hits));
  // update number of clusters (neutron events) found
  ui->totalclusters->setText(QString::number(m_total_events));
}

/**
 * @brief Read data from file and process hits to events
 *
 * @note This function does more than just reading file as it also process the
 *       data and update the histogram plot.
 */
void MainWindow::handlereadfile() {
  QString qs;
  // Check if data processing in progress
  // NOTE: active timer means data processing is in progress
  if (mytimer->isActive()) {
    qs.asprintf("File read already in progress");
    ui->infobox->setText(qs);
    return;
  }

  // Get file name
  QString filename = QFileDialog::getOpenFileName(
      this, "Timepix File to open", QDir::currentPath(),
      "All files (*.*) ;; Timepix raw (*.tpx3)");

  // DEVNOTE:
  // The original QtConcurrent::run() function is not safe guarding content of
  // `myhits`, which could leads to racing conditions.
  // A safer and better multi-threading solution is needed here.

  // Start the timer
  mytimer->start(2000);

  // Read raw data from file
  if (filename.isNull()) {
    qs.asprintf("No file selected");
    ui->infobox->setText(qs);
    return;
  }

  auto hits = readTimepix3RawData(filename.toStdString());
  m_total_hits = hits.size();
  ui->totalhits->setText(QString::number(m_total_hits));
  std::cout << "Total hits: " << m_total_hits << std::endl;

  // Perform clustering
  clustering_alg->fit(hits);

  // Generate neutron events from clustering results
  m_events = clustering_alg->get_events(hits);
  m_total_events = m_events.size();
  ui->totalclusters->setText(QString::number(m_total_events));
  std::cout << "Total events: " << m_total_events << std::endl;

  // Convert neutron events to 2D histogram data
  const double mycos = cos(ROTANGLE * M_PI / 180.0);
  const double mysin = sin(ROTANGLE * M_PI / 180.0);
  const int isize = (int)(DSCALE * 512);
#pragma omp parallel for
  for (auto e : m_events) {
    auto x = e.getX();
    auto y = e.getY();
    // correction for detector mounting angle
    // NOTE: this rotation correction is still being investigated
    //       might not be needed in the future.
    if (ROTANGLE != 0.0) {
      auto x1 = x * mycos - y * mysin;
      auto y1 = x * mysin + y * mycos;
      x = x1;
      y = y1;
    }
    if (x >= 0 && x < isize && y >= 0 && y < isize)
#pragma omp atomic
      my2dhisto[(int)(x + y * isize)] += 1;
  }

  // Update the histogram plot
  ui->qwthistoPlot->replot();

  // Stop the timer
  mytimer->stop();

  std::cout << "Done" << std::endl;
}

/**
 * @brief update the histogram plot color map range
 *
 */
void MainWindow::handlespinbox() {
  range_max = (double)ui->spinmaxrange->value();
  range_min = (double)ui->spinminrange->value();
  histo_data->setInterval(Qt::ZAxis, QwtInterval(range_min, range_max));
  if (!mytimer->isActive()) {
    ui->qwthistoPlot->replot();
  };
}

/**
 * @brief Handle save data button
 *
 */
void MainWindow::handlesavedata() {
  QString qs;
  // Check if data processing in progress
  // NOTE: active timer means data processing is in progress
  if (mytimer->isActive()) {
    qs.asprintf("File read already in progress");
    ui->infobox->setText(qs);
    return;
  }

  // Save 2D histogram
  double *swaphisto = NULL;
  QString filename = QFileDialog::getSaveFileName(
      this, "Save 2D hist as binary", QDir::currentPath(),
      "All files (*.*) ;; Pic Binary (*.dat)");
  if (!filename.isNull()) {
    FILE *outfile;
    int ii;
    int i, j;
    ii = (int)(DSCALE * 512);
    swaphisto = new double[ii * ii];
    // swap axis of  data
    for (i = 0; i < ii; i++)  // x
    {
      for (j = 0; j < ii; j++)  // y
      {
        swaphisto[ii * i + j] = my2dhisto[ii * j + i];
      }
    }
    outfile = fopen(qPrintable(filename), "wb");
    fwrite(&swaphisto[0], sizeof(double), ii * ii, outfile);
    fclose(outfile);
  }

  // Save neutron events to HDF5 archive
  filename = QFileDialog::getSaveFileName(this, "Save events to HDF5",
                                          QDir::currentPath(),
                                          "All files (*.*) ;; HDF5 (*.hdf5)");
  if (!filename.isNull()) {
    // create HDF5 archive
    H5::H5File events_file(filename.toStdString(), H5F_ACC_TRUNC);

    hsize_t dims[1] = {m_events.size()};
    H5::DataSpace dataspace(1, dims);
    H5::IntType datatype(H5::PredType::NATIVE_UINT);

    H5::Group group = events_file.createGroup("events");

    // write out X
    std::vector<int> x(m_events.size());
    std::transform(m_events.begin(), m_events.end(), x.begin(),
                   [](const NeutronEvent &e) { return e.getX(); });
    H5::DataSet dataset = group.createDataSet("X", datatype, dataspace);
    dataset.write(&x[0], H5::PredType::NATIVE_UINT);

    // write out Y
    std::vector<int> y(m_events.size());
    std::transform(m_events.begin(), m_events.end(), y.begin(),
                   [](const NeutronEvent &e) { return e.getY(); });
    dataset = group.createDataSet("Y", datatype, dataspace);
    dataset.write(&y[0], H5::PredType::NATIVE_UINT);

    // write out TOF
    std::vector<unsigned int> tof(m_events.size());
    std::transform(m_events.begin(), m_events.end(), tof.begin(),
                   [](const NeutronEvent &e) { return (int)e.getTOF(); });
    dataset = group.createDataSet("TOF", datatype, dataspace);
    dataset.write(&tof[0], H5::PredType::NATIVE_UINT);

    // close file
    events_file.close();
    std::cout << "Saved events to " << filename.toStdString() << std::endl;
  }
}