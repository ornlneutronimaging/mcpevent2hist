/**
 * @brief Qt based GUI application for dynamic display of neutron events from
 *        Timepix3 raw data.
 *
 */
#include <qwt.h>

#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  MainWindow w;
  w.show();

  return a.exec();
}
