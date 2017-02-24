// ------------------------------------------------
// File : usys.h
// Date: 2013/11/29
// Author: zhouhy
// Desc: 
//  WSys derives from Sys to provide basic win32 functions such as starting threads.
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

#ifndef _USYS_H
#define _USYS_H
// ------------------------------------
#include "socket.h"
#include "sys.h"

// ------------------------------------
class USys : public Sys
{
public:
    USys();

    virtual ClientSocket *createSocket();
    virtual bool   startThread(ThreadInfo *);
    virtual void   sleep(int );
    virtual unsigned int  getTime();
    virtual unsigned int  rnd() {return rndGen.next();}
    virtual void   endThread(ThreadInfo *);
    virtual void   waitThread(ThreadInfo *);

    dvbcast::Random rndGen;
};                               


// ------------------------------------
#endif
