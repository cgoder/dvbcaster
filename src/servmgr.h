// ------------------------------------------------
// File : servmgr.h
// Date: 2013/11/29
// Author: zhouhy
// Desc: 
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

#ifndef _SERVMGR_H
#define _SERVMGR_H

#include "servent.h"

// ----------------------------------
class ServFilter 
{
public:
    enum 
    {
        F_PRIVATE  = 0x01,
        F_BAN	   = 0x02,
        F_NETWORK  = 0x04,
        F_DIRECT   = 0x08
    };

    ServFilter() {init();}
    void init()
    {
        flags = 0;
        host.init();
    }

    Host host;
    unsigned int flags;
};

// ----------------------------------
// ServMgr keeps track of Servents
class ServMgr
{
public:
    enum {
        MAX_FILTERS = 50,
        MAX_DIRECT = 10,			//
    };

    enum AUTH_TYPE
    {
        AUTH_COOKIE,
        AUTH_HTTPBASIC
    };

    ServMgr();
    static THREAD_PROC serverProc(ThreadInfo *);

    bool start();
    Servent* allocServent();

    unsigned int numStreams(Servent::TYPE,bool);
    bool isFiltered(int,Host &h);
    void quit();
    
    void setFilterDefaults();
    
    unsigned int getUptime()
    {
        return sys->getTime()-startTime;
    }
    
    unsigned int numActiveOnPort(int);

    bool directFull() 
    {
        return numStreams(Servent::T_DIRECT,false) >= maxDirect;
    }

    static ThreadInfo serverThread;

    Servent *servents;
    WLock	lock;

    unsigned int maxDirect; 
    unsigned int maxServIn;

    Host serverHost;

    bool restartServer;
    bool autoServe;

    unsigned int allowServer;
    unsigned int startTime;

    GnuID	sessionID;
    GnuID	networkID;

    ServFilter filters[MAX_FILTERS];
    int numFilters;
	
    CookieList	cookieList;
    AUTH_TYPE	authType;
    char	password[64];

    int serventNum;
};

// ----------------------------------
extern ServMgr *servMgr;


#endif
