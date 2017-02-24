// ------------------------------------------------
// File : usys.cpp
// Date: 2013/11/29
// Author: zhouhy
// Desc: 
//  LSys derives from Sys to provide basic Linux functions such as starting threads.
//
// (c) 2013 www.skyworth.com
// ------------------------------------------------
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// ------------------------------------------------
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include "usys.h"
#include "usocket.h"


// ---------------------------------
USys::USys()
{
    rndGen.setSeed(rnd()+getpid()); 
    rndSeed = rnd();
}

// ---------------------------------
unsigned int USys::getTime()
{
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

// ---------------------------------
ClientSocket *USys::createSocket()
{
    return new UClientSocket();
}
               

// ---------------------------------
void USys::endThread(ThreadInfo *info)
{
    numThreads--; 
    //pthread_exit(NULL);
}

// ---------------------------------
void USys::waitThread(ThreadInfo *info)
{
    //pthread_join(info->handle,NULL);
}

// ---------------------------------
typedef void *(*THREAD_PTR)(void *);
bool USys::startThread(ThreadInfo *info)
{
    info->active = true;

    printf("[dvbcast]New thread: %d \n",numThreads);

    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int r = pthread_create(&info->handle,&attr,(THREAD_PTR)info->func,info);

    pthread_attr_destroy(&attr);

    if (r)
    {
        printf("[dvbcast]Error creating thread %d: %d \n",numThreads,r);
        return false;
    }else
    {
        numThreads++;
        return true;
    }
}
// ---------------------------------
void USys::sleep(int ms)
{
    ::usleep(ms*1000);
}

