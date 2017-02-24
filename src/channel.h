// ------------------------------------------------
// File : channel.h
// Date: 2013/11/29
// Author: zhouhy
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

#ifndef _CHANNEL_H
#define _CHANNEL_H

#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "sys.h"
#include "dvbpsi.h"
#include "stream.h"
#include "buffer.h"

#define MAX_STREAM_NUM (8)

typedef enum
{
    UNKNOWN_ES = 0x00,
    VIDEO_ES   = 0x01,
    AUDIO_ES   = 0x02,
    SPU_ES     = 0x03,
    NAV_ES     = 0x04,
}es_format_category_e;

typedef int64_t mtime_t;

typedef struct 
{
    bool    b_used;          

    int    i_cat;   /**< ES category @see es_format_category_e */
    
    uint32_t    h_filter;
    uint16_t    i_org_pid;   // stream pid
    uint32_t    i_org_type;   //stream type    
    
    buffer_fifo_t*    p_fifo;
} ts_stream_t;


typedef struct 
{
    uint32_t     h_fileop;
    char         rpath[32];    /*Directory*/
    int          seg_index;  /**< segment num */     
    int          i_pkt_num;   /**< segment pkts */ 
    mtime_t     i_first_dts; /**< current segment first dts */    
    mtime_t     i_last_dts; 
}ts_segment_t;

// ----------------------------------
class Channel
{
public:
	
    enum STATUS
    {
        S_NONE,
        S_WAIT,
        S_CONNECTING,
        S_CLOSING,
        S_RECEIVING,
        S_IDLE,			
        S_ERROR		
    };

    Channel();
    void reset();
    void close();

    void initID(const char *);
    bool matchID(GnuID &);

    int startStream(void);
    int stopStream(void);
    static THREAD_PROC stream(ThreadInfo *); //mux  thread
    
    bool isPlaying()
    {
        return (status == S_RECEIVING);
    }		
    bool isActive()
    {
        return (status != S_NONE && status != S_ERROR);
    }    
    void setStatus(STATUS s);
    int   getCurrentSegment(void);
    void getDvbPsi(buffer_chain_t* );
public:
    int     status;   //通道状态
    int     serviceID;      //频道号  
    GnuID   chanID;       //通道标识符       
    WLock   lock;
    
    Channel*    next;       
    ThreadInfo  thread;    
private:    
    int     i_pmt_pid;
    int     i_pcr_pid;
    
    int     i_transport_id;
    int     i_network_id;
    int     i_service_id;   //program number;
    
    ts_segment_t   m_segment;
    ts_stream_t     m_stream[MAX_STREAM_NUM];    
};

// ----------------------------------
class ChanMgr
{
public:
    ChanMgr();
    void quit();
    Channel* createChannel(unsigned short servid);
    Channel* findChannel(GnuID &);	
    Channel* findChannel(unsigned short);
    
public:
    WLock	lock;
    Channel* channels; //channel list
    unsigned int channelNums;
};

extern ChanMgr *chanMgr;

#endif
