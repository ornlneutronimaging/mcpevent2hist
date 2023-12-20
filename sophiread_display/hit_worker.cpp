#include "hit_worker.h"
#include <errno.h>
#include "mainwindow.h"
#include "math.h"

#define NUMCLUSTERS 128
#define CLUSTERADD 5
#define ROTANGLE -1.0
#define PI 3.14159265


Hit_worker::Hit_worker(struct newRawPacket *myrawpackets, struct hitinfo *info, double *histo, unsigned int *tofhisto)
{
    // Constructor
    myhits=myrawpackets;
    myinfo=info;
    my2dhisto=histo;
    mytofhisto=tofhisto;
    totalClusters=0;
    mycos=cos(ROTANGLE*PI/180.0);
    mysin=sin(ROTANGLE*PI/180.0);
    acceptedClusters=0;
    this->killthread=0;

//    int i,j;
//    for (i=0;i<512;i++)
//    {
//        for (j=0;j<512;j++)
//        {
//            my2dhisto[512*i+j]=sqrt(i*i+j*j);
//        }
//    }

}

Hit_worker::~Hit_worker()
{
    // Destructor
    this->killthread=1;

}

int Hit_worker::runme()
{
    int nomatch;
    int i,k;
    unsigned int itof;
    int pindex=0;                   // these  are  for the cluster memory area.
    int cindex=0;
    double xmean,ymean;             //
    int ix,iy;
    int isize=(int)DSCALE*512;
    totalClusters=0;
    acceptedClusters=0;
    this->killthread=0;
    myclusters=new struct cluster[NUMCLUSTERS];
    memset(&myclusters[0],0,(NUMCLUSTERS)*sizeof(struct cluster));

//    for (i=0;i<512;i++)
//    {
//        for (k=0;k<512;k++)
//        {
//            my2dhisto[512*i+k]=sqrt(i*i+k*k);
//        }
//    }

//tbd handle error.
    while (this->killthread==0)
    {
        usleep(10);
        while (myinfo->ci != myinfo->pi)
        {
            //
            nomatch=1;

            // going through a list of 128 clusters and
            // see if the hit belongs to an existing cluster
            // or a new cluster
            for (i=0;i<NUMCLUSTERS;i++)
            {
                if (myclusters[i].slotinuse==1)
                {
                    //check if  I belong to this  cluster with time and spatial constraint
                    // time check (~100~ns time window w.r.t. first hit in the cluster)
                    k=myclusters[i].firstspdr- myhits[myinfo->ci].spdrtime;

                    if (abs(k) < 3)      // in ns units

                    //spatial check
                    {
                    //is it this cluster or another at the same time?
                    //could I use FTOA to avoid this?

                        // check if it is within +5 pixels away cluster boundary
                        if ((myhits[myinfo->ci].x <= myclusters[i].maxx+CLUSTERADD) &&
                            (myhits[myinfo->ci].x >= myclusters[i].minx-CLUSTERADD) &&
                            (myhits[myinfo->ci].y <= myclusters[i].maxy+CLUSTERADD) &&
                            (myhits[myinfo->ci].y >= myclusters[i].miny-CLUSTERADD))

                        {
                            // update cluster information
                            myclusters[i].totalpix+=1;
                            myclusters[i].runningX+=myhits[myinfo->ci].myTOT*myhits[myinfo->ci].x;
                            myclusters[i].runningY+=myhits[myinfo->ci].myTOT*myhits[myinfo->ci].y;
                            myclusters[i].totalTOT+=myhits[myinfo->ci].myTOT;

                            // update cluster boundary as necessary
                            if (myhits[myinfo->ci].x < myclusters[i].minx)
                            {
                                myclusters[i].minx=myhits[myinfo->ci].x;
                            }
                            else if (myhits[myinfo->ci].x > myclusters[i].maxx)
                            {
                                myclusters[i].maxx=myhits[myinfo->ci].x;
                            }
                            if (myhits[myinfo->ci].y < myclusters[i].miny)
                            {
                                myclusters[i].miny=myhits[myinfo->ci].y;
                            }
                            else if (myhits[myinfo->ci].y > myclusters[i].maxy)
                            {
                                myclusters[i].maxy=myhits[myinfo->ci].y;
                            }
                            nomatch=0;
                            break;
                        }
                    }
                }
            }
            if (nomatch==1)
            {  //nothing matches, add a  new entry at producer index
                if ((pindex+1) % NUMCLUSTERS == cindex)  //full, pop the cindex off and place at pindex
                {
                    this->totalClusters+=1;
                    myinfo->totalClusters+=1;

                    // Cluster acceptance criteria: if the number of hits greater than 10
                    if (myclusters[cindex].totalpix > 10)
                    {
                        this->acceptedClusters+=1;
                        xmean=DSCALE*myclusters[cindex].runningX/myclusters[cindex].totalTOT; // does totalTOT set to 1?
                        ymean=DSCALE*myclusters[cindex].runningY/myclusters[cindex].totalTOT;

                        if (ROTANGLE != 0.0)
                        {
                            ix=int(xmean*mycos-ymean*mysin);
                            iy=int(xmean*mysin+ymean*mycos);
                            if (ix>=0 && ix < isize && iy >= 0 and iy< isize)
                                my2dhisto[isize*iy+ix]+=1;
                        }
                        else
                        {
                            ix=int(xmean);
                            iy=int(ymean);
                            my2dhisto[isize*iy+ix]+=1;
                        }
//                        itof=(myclusters[cindex].tof >> 2);
//                        if (myclusters[cindex].tof<MAXTOFINDEX)
//                        {
//                            mytofhisto[myclusters[cindex].tof]++;
//                        }
                        //note that x and y axis are swapped in picture
//                        my2dhisto[isize*iy+ix]+=1;
//                        my2dhisto[1024*ix+iy]+=1;
                    }
                    myclusters[cindex].slotinuse=0;
                    myclusters[pindex].minx=myhits[myinfo->ci].x;
                    myclusters[pindex].miny=myhits[myinfo->ci].y;
                    myclusters[pindex].maxx=myhits[myinfo->ci].x;
                    myclusters[pindex].maxy=myhits[myinfo->ci].y;
//                    myclusters[pindex].firstTOA=myhits[myinfo->ci].myTOA;
                    myclusters[pindex].firstspdr=myhits[myinfo->ci].spdrtime;
                    myclusters[pindex].slotinuse=1;
                    myclusters[pindex].totalpix=1;  //total pix
                    myclusters[pindex].runningX=myhits[myinfo->ci].x*myhits[myinfo->ci].myTOT; //running x
                    myclusters[pindex].runningY=myhits[myinfo->ci].y*myhits[myinfo->ci].myTOT; //running y
                    myclusters[pindex].totalTOT=myhits[myinfo->ci].myTOT; //total energy (esum or total TOT)
                    pindex=(pindex+1) % NUMCLUSTERS;
                    cindex=(cindex+1) % NUMCLUSTERS;
                }
                else
                {  //just put it in the pindex

//                    myclusters[pindex].firstTOA=myhits[myinfo->ci].myTOA;
                    myclusters[pindex].firstspdr=myhits[myinfo->ci].spdrtime;
                    myclusters[pindex].tof=myhits[myinfo->ci].tof;
                    myclusters[pindex].slotinuse=1;
                    myclusters[pindex].totalpix=1;  //total pix
                    myclusters[pindex].minx=myhits[myinfo->ci].x;
                    myclusters[pindex].miny=myhits[myinfo->ci].y;
                    myclusters[pindex].maxx=myhits[myinfo->ci].x;
                    myclusters[pindex].maxy=myhits[myinfo->ci].y;
                    myclusters[pindex].runningX=myhits[myinfo->ci].x*myhits[myinfo->ci].myTOT; //running x
                    myclusters[pindex].runningY=myhits[myinfo->ci].y*myhits[myinfo->ci].myTOT; //running y
                    myclusters[pindex].totalTOT=myhits[myinfo->ci].myTOT; //total energy (esum or total TOT)
                    pindex=(pindex+1) % NUMCLUSTERS;
                }
            }

            myinfo->ci= (myinfo->ci+ 1  ) & HSIZEM1;

        }

    }
    delete(myclusters);
    myinfo->clusterparsedone=1;
    return 1;
}

long int Hit_worker::gettotalClusters()
{
    return this->totalClusters;
}

int Hit_worker::getkillflag()
{
    return this->killthread;
}

void Hit_worker::killme()
{
    killthread=1;
}





