#include <qwt_interval.h>
#include <qwt_math.h>
#include <qwt_matrix_raster_data.h>
#include <qwt_plot_spectrogram.h>

#include <QElapsedTimer>
#include <QLabel>
#include <QMainWindow>
#include <QTimer>

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

 private:
  Ui::MainWindow *ui;
};
