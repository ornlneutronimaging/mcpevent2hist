#include "mainwindow.h"

#include <QtConcurrent/qtconcurrentrun.h>
#include <qwt_color_map.h>
#include <qwt_plot.h>
#include <qwt_scale_engine.h>

#include <QFile>
#include <QFileDialog>

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
  ui->setupUi(this);
}

MainWindow::~MainWindow() { delete ui; }
