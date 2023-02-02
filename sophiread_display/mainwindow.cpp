#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QFile>
#include <sys/stat.h>
#include <QtConcurrent/qtconcurrentrun.h>
#include <qwt_plot.h>
#include <qwt_color_map.h>
#include <qwt_scale_engine.h>

// Custom class of color map 
class ColorMap: public QwtLinearColorMap
{
public:
    ColorMap():
        QwtLinearColorMap( Qt::darkBlue, Qt::darkRed )
    {
        addColorStop( 0.2, Qt::blue );
        addColorStop( 0.4, Qt::cyan );
        addColorStop( 0.6, Qt::yellow );
        addColorStop( 0.8, Qt::red );
    }
};


// read file to hit 
int filetohits(QString filename, struct newRawPacket *myhits, struct hitinfo *myinfo, unsigned int *mytofhisto, QLabel *infobox)
{
    char data[128];                                     // pixel hit data + TDC data, no need 128 bytes
    char inbytes[128];                                  // mostly to store data packet header, no need 128 bytes
    int notempty=1;                     
    int i;                                              // store the number of bytes read for each hit event                 
    int dwords; 
    QString inname;
    int k;
    int n;
    int itof;
//    unsigned long hits=0;
    unsigned long *tdclast,mytdc;
    unsigned  short pixaddr, dcol,spix,pix;
    unsigned short *spdrtime; //bytes  0,1
    unsigned short *nTOT; //bytes 2,3
    unsigned int *nTOA; //bytes 3,4,5,6
    unsigned int *npixaddr;  //bytes 4,5,6,7
    struct newRawPacket mynewraw;
    mytdc=0;

// Load data from raw file 
    FILE *infile;
    struct stat myfilestat;
    inname=filename.section("/",-1,-1);
    infobox->setText("reading  file" + inname);

    infile=fopen(qPrintable(filename),"rb");
    fstat(fileno(infile),&myfilestat); 
    myinfo->bytesinfile=myfilestat.st_size;             // store file size (hitinfo/myinfo)

    while(notempty)
    {
        i=fread(&inbytes[0],1,8,infile);                // read 8 1-bytes data to inbytes[0] (8 bytes = 64 bits)
        if (i==0)                                       // if num of bytes read is 0, break 
        {                                               // loop to read file 
            notempty=0;
            break;
        }
        myinfo->bytesread+=i;                           // store bytes read (hitinfo/myinfo)
        if (inbytes[0]=='T' && inbytes[1]=='P' && inbytes[2]=='X')  // check for data packet header 

        {
            myinfo->chuckheaders+=1;                   // count number of data packet header (hitinfo/myinfo)
            dwords=256*inbytes[7]+inbytes[6];          // get total number of bits(?) in chunk (2^8 = 256)
            dwords=dwords  >> 3;                       // data words is the number of bytes chunk? (>> 3 means divide by 8)
            /* ------------------------------------------------------------------------------------------------*/
            // testing purpose
            // dwords = 3  
            /* ------------------------------------------------------------------------------------------------*/

            for (k=0;k<dwords;k++)                     // for each data word (1 byte?)
            {
                i=fread(&data[0],1,8,infile);          // read the next 8 bytes from raw file 
                myinfo->bytesread+=i;                  // increment number of bytes read (hitinfo/myinfo)
                
                if ((data[7] & 0xF0) == 0xb0)         //Chip data: ToA and ToT timestamp packet, x, y    
                {
                    /* ------------------------------------------------------------------------------------------------*/
                    /* ------------------------------------------------------------------------------------------------*/
                    // char data[128] 
                    // 0: 0-7 bits
                    // 1: 8-15 bits
                    // 2: 16-23 bits
                    // 3: 24-31 bits
                    // 4: 32-39 bits
                    // 5: 40-47 bits
                    // 6: 48-55 bits
                    // 7: 56-63 bits
                    // little endian: the least significant value in the sequence is stored first 
                    /* ------------------------------------------------------------------------------------------------*/

                    spdrtime=(unsigned short *)(&data[0]);          // spider time  (16 bits)
                    nTOT=(unsigned short *)(&data[2]);              // ToT          (10 bits)
                    nTOA=(unsigned int *)(&data[3]);                // ToA          (14 bits)
                    mynewraw.myFToA= *nTOT & 0xF;                   // FToA         (4 bits)
                    mynewraw.myTOT=(*nTOT >> 4) & 0x3FF;            
                    mynewraw.myTOA=(*nTOA >> 6) & 0x3FFF;     
                    
                    npixaddr=(unsigned int *)(&data[4]);            // PixAddr      (16 bits)
                    pixaddr=(*npixaddr >> 12) & 0xFFFF;  
                    dcol=((pixaddr & 0xFE00)>>8);
                    spix=((pixaddr & 0x1F8) >>1);
                    pix=pixaddr & 0x7;                              
                    mynewraw.x=dcol+(pix >> 2);                     // x coordinate
                    mynewraw.y=spix+(pix & 0x3);                    // y coordinate 

                    mynewraw.spdrtime=16384*(*spdrtime)+mynewraw.myTOA; // not the same as global_timestamp 
                    mynewraw.tof=mynewraw.spdrtime-mytdc;               // supposedly for time-of-flight 
                    
                    itof=(int)(mynewraw.tof*0.25);                  // tof in seconds 

                    // not sure why the tofhist is updated this way...??
                    if (itof<MAXTOFINDEX && mynewraw.x > 250 && mynewraw.x < 425 && mynewraw.y > 125 && mynewraw.y < 325)
                    {
                        mytofhisto[itof]++;
                    }

                    // check for number of chip (our case is quad chip inbytes[4] == 3)
                    if (inbytes[4]==0)                  // single
                    {
                        mynewraw.x=mynewraw.x+256;
                    }
                    else if (inbytes[4]== 1)            // double
                    {
                        mynewraw.x=511-mynewraw.x;
                        mynewraw.y=511-mynewraw.y;
                    }
                    else if (inbytes[4]==2)             // triple
                    {
                        mynewraw.x=255-mynewraw.x;
                        mynewraw.y=511-mynewraw.y;
                    }

                    myinfo->total_hits+=1;

                    n=(myinfo->pi+1) & HSIZEM1;         // get the next slot n = (pi+1) & HSIZEM1 

                    while (n==myinfo->ci)               // let the program sleep for 100 usec 
                    {                                   // while the next slot n == ci 
                        usleep(100);
                    }

                    if (n !=  myinfo->ci)               // copy raw data to myhits 
                    {                                   // otherwise the pi = n 
                        memcpy(&myhits[myinfo->pi],&mynewraw,sizeof(mynewraw));
                        myinfo->pi=n;
                    }
                }
                else if (data[7]  == 0x6F) // & 0xF0) == 0x60)  // TDC data packets
                {                                               // unclear what is going on here
                    myinfo->numTDCs+=1;
                    tdclast=(unsigned long *)(&data[0]);
                    mytdc=(((*tdclast) >> 12) & 0x3fffffff); 

                }
                else if ((data[7] & 0xF0) == 0x40)      // for controls
                {
                    myinfo->numGDCs+=1;
                }
            }
        }

    }
    fclose(infile);
    myinfo->fileparsedone=1;
    return 10;
}


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    myhits=new struct newRawPacket[HSIZEM1 +1];
    mytofhisto=new unsigned int[MAXTOFINDEX];
    myinfo.bytesread=0;
    myinfo.bytesinfile=1;
    myinfo.total_hits=0;
    myinfo.ci=0;
    myinfo.pi=0;
    myinfo.chuckheaders=0;
    myinfo.numGDCs=0;
    myinfo.numTDCs=0;
    myinfo.totalClusters=0;
    myinfo.fileparsedone=0;
    myinfo.clusterparsedone=0;
    rangemax = 100.0;
    rangemin = 0.0;
    //    int i,j;
    int ii,jj;
    ii=(int)(DSCALE*512);
    vhisto.resize(ii*ii);
//    i=vhisto.size();
    my2dhisto=vhisto.data();
    myhitworker=new class Hit_worker(myhits,&myinfo,my2dhisto,mytofhisto);
    ui->setupUi(this);
    mytimer = new QTimer(this);
    connect(mytimer,SIGNAL(timeout()),this, SLOT(handletimer()));
    connect(ui->selectfile,SIGNAL(clicked()),this,SLOT(handlereadfile()));
    connect(ui->updaterange,SIGNAL(clicked()),this,SLOT(handlespinbox()));
    connect(ui->savedata,SIGNAL(clicked()),this,SLOT(handlesavedata()));

//    for (i=0;i<512;i++)
//    {
//        for (j=0;j<512;j++)
//        {
//            vhisto[512*i+j]=sqrt(i*i+j*j);
//        }
//    }

    histo_data=new QwtMatrixRasterData;
    jj=(int)DSCALE*500;
    histo_data->setInterval(Qt::XAxis,QwtInterval(0,jj));
    histo_data->setInterval(Qt::YAxis,QwtInterval(0,jj));
    histo_data->setInterval(Qt::ZAxis,QwtInterval(rangemin,rangemax));
    histo_data->setValueMatrix(vhisto,ii);

    histo=new QwtPlotSpectrogram;

    histo->setDisplayMode(QwtPlotSpectrogram::DisplayMode::ImageMode,true);
    histo->setColorMap(new ColorMap); //QwtLinearColorMap(Qt::white,Qt::black) );
    histo->setData(histo_data);

    histo->attach(ui->qwthistoPlot);
    ui->qwthistoPlot->replot();

}

MainWindow::~MainWindow()
{
    delete this->myhits;
    delete ui;
    delete mytimer;
    delete myhitworker;
//    delete histo_data;
    delete histo;
    delete mytofhisto;

}

void MainWindow::handletimer()
{
    double percent;
//    int i;
    QString qs;
    percent=100.0*myinfo.bytesread/myinfo.bytesinfile;
    qs.sprintf("%.1f" ,percent);
    ui->percent_complete->setText(qs);
    this->myet=this->myelapsedtime.elapsed();
    qs.sprintf("%.2f" ,.001*this->myet);
    ui->elapsed_time->setText(qs);
    qs.sprintf("%lu" ,myinfo.total_hits);
    ui->totalhits->setText(qs);
    qs.sprintf("%lu" ,myinfo.totalClusters);
    ui->totalclusters->setText(qs);
//    histo->setData(histo_data);
//    histo->attach(ui->qwthistoPlot);
    ui->qwthistoPlot->replot();
    if (myinfo.fileparsedone==1)
    {
        myhitworker->killme();
        qs.sprintf("File parse complete");
        ui->infobox->setText(qs);
        if (myinfo.clusterparsedone==1)
        {
            mytimer->stop();
        }
    }

}

void MainWindow::handlereadfile()
{
    QString  qs;
    if (mytimer->isActive())
    {
        qs.sprintf("File read already in progress");
        ui->infobox->setText(qs);
        return;
    }
    QString filename = QFileDialog::getOpenFileName(this,"Timepix File to open", QDir::currentPath(),"All files (*.*) ;; Timepix raw (*.tpx3)");
    if  (!filename.isNull())
    {
        myinfo.bytesread=0;
        myinfo.bytesinfile=1;
        myinfo.total_hits=0;
        myinfo.ci=0;
        myinfo.pi=0;
        myinfo.chuckheaders=0;
        myinfo.numGDCs=0;
        myinfo.numTDCs=0;
        myinfo.totalClusters=0;
        myinfo.fileparsedone=0;
        myinfo.clusterparsedone=0;
        int ii;
        ii=(int)DSCALE*512;
        memset(my2dhisto,0,ii*ii*sizeof(double));
        memset(mytofhisto,0,MAXTOFINDEX*sizeof(unsigned int));
        mytimer->start(2000);
        this->elapsed_seconds=0;
        myelapsedtime.start();
        QFuture<int> hworker = QtConcurrent::run(myhitworker,&Hit_worker::runme);
        QFuture<int> hf=QtConcurrent::run(filetohits,filename,myhits,&myinfo,mytofhisto,ui->infobox);
    }

}

void MainWindow::handlespinbox()
{
    rangemax=(double)ui->spinmaxrange->value();
    rangemin=(double)ui->spinminrange->value();
    histo_data->setInterval(Qt::ZAxis,QwtInterval(rangemin,rangemax));
    if (!mytimer->isActive())
        ui->qwthistoPlot->replot();

}

void MainWindow::handlesavedata()
{
    QString  qs;
    if (mytimer->isActive())
    {
        qs.sprintf("File read in progress-wait until complete");
        ui->infobox->setText(qs);
        return;
    }
    double *swaphisto=NULL;
    QString filename = QFileDialog::getSaveFileName(this,"Save to File Name", QDir::currentPath(),"All files (*.*) ;; Pic Binary (*.dat)");
    if  (!filename.isNull())
    {
        FILE *outfile;
        int ii;
        int i,j;
        ii=(int)(DSCALE*512);
        swaphisto=new double[ii*ii];
        //swap axis of  data
        for (i=0;i<ii;i++)  //x
        {
            for (j=0;j<ii;j++)  //y
            {
                swaphisto[ii*i+j]=my2dhisto[ii*j+i];
            }
        }
        outfile=fopen(qPrintable(filename),"wb");
        fwrite(&swaphisto[0],sizeof(double),ii*ii,outfile);
        fclose(outfile);

    }
// save tof spectrum.....
    QString toffilename = QFileDialog::getSaveFileName(this,"Save TOF spec to File Name", QDir::currentPath(),"All files (*.*) ;; Pic Binary (*.dat)");
    if  (!toffilename.isNull())
    {
        FILE *toffile;
        toffile=fopen(qPrintable(toffilename),"wb");
        fwrite(&mytofhisto[0],sizeof(unsigned int),MAXTOFINDEX,toffile);
        fclose(toffile);
    }
    if (swaphisto != NULL)
        delete(swaphisto);
}

