#ifndef HIT_WORKER_H
#define HIT_WORKER_H

#include <QDebug>
#include "mainwindow.h"
#include <unistd.h>
#include <sys/types.h>



class Hit_worker
{
public:
    Hit_worker(struct newRawPacket *myrawpacket, struct hitinfo *myinfo, double *my2dhisto, unsigned int *mytofhisto);
    ~Hit_worker(void);

    // shared memory among threads 
    struct cluster *myclusters;             // an array of clusters
    struct newRawPacket *myhits;            // an array of hits 
    struct hitinfo *myinfo;                 // file info 
    double *my2dhisto;                      // 2D array of counts/TOT
    unsigned int *mytofhisto;               // an array of ToF??

    int runme();                            // main method by hit worker (get centroiding)
    void killme();
    int getkillflag();
    long int totalClusters;                 // total number of clusters (including rejected ones)
    long int acceptedClusters;              // number of accepted clusters

    int killthread;
    long int gettotalClusters();
    double mycos;  //these are used for picture rotation
    double mysin;

};

#endif // HIT_WORKER_H
