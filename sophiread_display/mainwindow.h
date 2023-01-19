#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QElapsedTimer>
#include "hit_worker.h"
#include <qwt_interval.h>
#include <qwt_matrix_raster_data.h>
#include <qwt_plot_spectrogram.h>
#include <qwt_math.h>

#define HSIZEM1 4095
#define DSCALE 8.0
#define MAXTOFINDEX 170000   //this implies 100nsec resolution

struct cluster {
    int slotinuse;
    unsigned int tof;
    int firstspdr;
    int minx;
    int maxx;
    int miny;
    int maxy;
    int totalTOT;
    int runningX;
    int runningY;
    int totalpix;
    int firstkk;
    int lastkk;
};

struct newRawPacket {
    int myFToA;
    int myTOT;
    int myTOA;
    int x;
    int y;
    int spdrtime;
    unsigned int tof;
};

struct hitinfo {
    unsigned long bytesread;
    unsigned long bytesinfile;
    unsigned long total_hits;
    int ci;
    int pi;
    unsigned int chuckheaders;
    unsigned int numTDCs;  //timing packet counts
    unsigned int numGDCs;  //global timing packet counts
    unsigned long totalClusters;
    int fileparsedone;
    int clusterparsedone;
};

extern int filetohits(QString inname, struct newRawPacket *myhits, struct hitinfo *myinfo, QLabel *infobox);

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    struct hitinfo myinfo;
    QVector<double> vhisto;
    unsigned int elapsed_seconds;
    long int myet;
    double *my2dhisto;
    unsigned int *mytofhisto;
    class Hit_worker *myhitworker;
    struct newRawPacket *myhits;  //hit worker deals with these
    QElapsedTimer myelapsedtime;
    QwtMatrixRasterData   *histo_data;
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

#endif // MAINWINDOW_H
