// ------------------------------------------------
// File : servmgr.cpp
// Date: 2013/11/29
// Author: zhouhy
// Desc: 
//		Management class for handling multiple servent connections.
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

#include <stdlib.h>
#include "servent.h"
#include "servmgr.h"

ThreadInfo ServMgr::serverThread;

static const int DEFAULT_PORT	= 7900;

// -----------------------------------
ServMgr::ServMgr()
{
    authType = AUTH_COOKIE;
    cookieList.init();
    password[0]=0;

    serventNum = 0;

    startTime = sys->getTime();

    allowServer =  Servent::ALLOW_ALL;

    maxServIn = 50;
    maxDirect = MAX_DIRECT;

    sessionID.generate(0xff7e);
    networkID.generate(0xff6e);

    serverHost.fromStrIP("127.0.0.1",DEFAULT_PORT);

    autoServe = true;
    restartServer=false;

    setFilterDefaults();
    servents = NULL;
}

// -----------------------------------
void ServMgr::setFilterDefaults()
{
    numFilters = 0;

    filters[numFilters].host.fromStrIP("255.255.255.255",0);
    filters[numFilters].flags = ServFilter::F_NETWORK|ServFilter::F_DIRECT;
    numFilters++;
}

// -----------------------------------
Servent *ServMgr::allocServent()
{
    lock.on();

    Servent *s = servents;
    while (s)
    {
        if (s->status == Servent::S_FREE)
            break;
        s=s->next;
    }

    if (!s)
    {
        s = new Servent(++serventNum);
        if (!s) 
        {
            lock.off();
            return NULL;
        }
        s->next = servents;
        servents = s;

        printf("allocated servent ,serventNum: %d \n",serventNum);
    }
    else
    {
        printf("reused servent , serventNum: %d !\n",serventNum);
    }

    s->reset();

    lock.off();

    return s;
}

// -----------------------------------
unsigned int ServMgr::numActiveOnPort(int port)
{
    unsigned int cnt=0;

    Servent *s = servents;
    while (s)
    {
        if (s->thread.active && s->sock && (s->servPort == port))
            cnt++;
        s=s->next;
    }
    return cnt;
}

// -----------------------------------
void ServMgr::quit()
{
    printf("ServMgr is quitting.. \n");

    serverThread.shutdown();

    Servent *s = servents;
    while (s)
    {
        try
        {
            if (s->thread.active)
            {
                s->thread.shutdown();
            }

        }catch(StreamException &)
        {

        }
        s=s->next;
    }	
}


// -----------------------------------
bool ServMgr::isFiltered(int fl, Host &h)
{
    for(int i=0; i<numFilters; i++)
        if (filters[i].flags & fl)
            if (h.isMemberOf(filters[i].host))
                return true;
	return false;
}

// --------------------------------------------------
unsigned int ServMgr::numStreams(Servent::TYPE tp, bool all)
{
    int cnt = 0;
    Servent *sv = servents;
    while (sv)
    {
        if (sv->isConnected())
            if (sv->type == tp)
                if (all || !sv->isPrivate())
                    cnt++;
        sv=sv->next;
    }
    return cnt;
}

// --------------------------------------------------
bool ServMgr::start()
{
    char idStr[64];


    const char *priv;
#if PRIVATE_BROADCASTER
    priv = "(private)";
#else
    priv = "";
#endif
    printf("Dvbcast %s, %s \n",PCX_VERSTRING,priv);

    sessionID.toStr(idStr);
    printf("SessionID: %s \n",idStr);

    serverThread.func = ServMgr::serverProc;
    if (!sys->startThread(&serverThread))
        return false;
    return true;
}

// --------------------------------------------------
int ServMgr::serverProc(ThreadInfo *thread)
{
    Servent *serv = servMgr->allocServent();

    while (thread->active)
    {

        if (servMgr->restartServer)
        {
            serv->abort();		// force close
            servMgr->quit();

            servMgr->restartServer = false;
        }

        if (servMgr->autoServe)
        {
            serv->allow = servMgr->allowServer;
            if (!serv->sock)
            {
                printf("Starting servers \n");
                //if (servMgr->serverHost.ip != 0)
                {
                    Host h = servMgr->serverHost;

                    if (!serv->sock)
                        serv->initServer(h);
                }
            }
        }else{
            // stop server
            serv->abort();		// force close

            // cancel incoming connectuions
            Servent *s = servMgr->servents;
            while (s)
            {
                if (s->type == Servent::T_INCOMING)
                    s->thread.active = false;
                s=s->next;
            }
        }

        sys->sleepIdle();

    }

    sys->endThread(thread);
    //	thread->unlock();
    return 0;
}


