// ------------------------------------------------
// File : dvbcast.cpp
// Date: 2013/11/29
// Author: zhouhy
// Desc: 
//	dvbcast interface 
//
// (c) 2002 www.skyworth.com
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include "sys.h"
#include "usys.h"
#include "channel.h"
#include "servmgr.h"
#include "ramdisk.h"
#include "ff.h"

Sys *sys = NULL;
ChanMgr *chanMgr = NULL;
ServMgr *servMgr = NULL;

// -----------------------------------
// -----------------------------------
void DVBCAST_Init(void)
{
    sys = new USys();
    if (!sys) return ;
        
    servMgr = new ServMgr();
    if (!servMgr) return ;

    chanMgr = new ChanMgr();
    if (!chanMgr) return ;

    ramdisk_init();
    servMgr->start();
}

void DVBCAST_Quit(void)
{
    if (servMgr)
        servMgr->quit();

    if (chanMgr)
        chanMgr->quit();
    
    ramdisk_destory();
}

void DVBCAST_Start(void)
{

}
#if 0
extern SKYDVB_S32 skydvb_filter_fn(SKYDVB_U32 u32FilterId,  SKYDVB_U8* pU8Buffer, SKYDVB_U32 u32BufferLen);	

int main(void)
{
    unsigned short pid = 0x1fff;
    unsigned char buffer[188];
    unsigned int icount = 0;
    FILE* fhandle = fopen("698_m.ts","rb");

    DVBCAST_Init();
    while(true)
    {
        int isize = fread(buffer,1,188,fhandle);
        if (isize <= 0)
        {
            break;
        }

        pid = ((buffer[1]&0x1f)<<8 )|buffer[2];
        if (pid == 0x18a7)
        {
            for (;;)
            {
                if (skydvb_filter_fn(2,(SKYDVB_U8*)buffer,188) != SKYDVB_SUCCESS)
                {
                    usleep(10*1000);
                    continue;
                }
                break;
            }
        }
        else if(pid == 0x18a8)
        {
            for (;;)
            {
                if (skydvb_filter_fn(1,(SKYDVB_U8*)buffer,188) != SKYDVB_SUCCESS)
                {
                    usleep(10*1000);
                    continue;
                }
                break;
            }            
        }        
    }
    fclose(fhandle);
    
    while(1)
    {
        usleep(1000);
    }
}
#endif

