// ------------------------------------------------
// File : channel.cpp
// Date: 2013/11/29
// Author: zhouhy
// Desc: 
//		Channel streaming classes. These do the actual 
//		streaming of media between clients. 
//
// (c) 2013 www.skyworth.com
// 
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "common.h"
#include "socket.h"
#include "channel.h"
#include "servent.h"
#include "servmgr.h"
#include "sys.h"
#include "http.h"
#include "ff.h"
#include "ramdisk.h"
#include "buffer.h"
#include "skyworth_dvb.h"

#define _DEBUG
#ifdef _DEBUG
#define AV_STREAM_NUM (2)

unsigned int m_segment = 0;
static ts_stream_t * pMuxAudioInput = NULL; //only for test
static ts_stream_t* pMuxVideoInput = NULL; //only for test

SKYDVB_S32 skydvb_filter_fn(SKYDVB_U32 u32FilterId,  SKYDVB_U8* pU8Buffer, SKYDVB_U32 u32BufferLen)	
{    

    if (!pMuxAudioInput || !pMuxVideoInput)
        return SKYDVB_FAILURE;
    
    if (u32FilterId == pMuxAudioInput->h_filter)
    {
        if (pMuxAudioInput->p_fifo)
        {
            if (buffer_fifoPut(pMuxAudioInput->p_fifo, pU8Buffer, u32BufferLen) != u32BufferLen)
            {
                //printf("audio buffer input error!\n");
                return SKYDVB_FAILURE;
            }
        }                 
    }
    else if (u32FilterId == pMuxVideoInput->h_filter)
    {
        if (pMuxVideoInput->p_fifo)
        {
            if (buffer_fifoPut(pMuxVideoInput->p_fifo, pU8Buffer, u32BufferLen) != u32BufferLen)
            {
                //printf("video buffer input error!\n");
                return SKYDVB_FAILURE;
            }            
        }
    }
    return SKYDVB_SUCCESS;
}
#endif

static block_t *WritePSISection( dvbpsi_psi_section_t* p_section )
{
    block_t   *p_psi, *p_first = NULL;

    while( p_section )
    {
        int i_size = (uint32_t)(p_section->p_payload_end - p_section->p_data) +
                  (p_section->b_syntax_indicator ? 4 : 0);

        p_psi = block_Alloc( i_size + 1 );
        if( !p_psi )
            goto error;
        p_psi->i_buffer = i_size + 1;

        p_psi->p_buffer[0] = 0; /* pointer */
        memcpy( p_psi->p_buffer + 1,
                p_section->p_data,
                i_size );

        block_ChainAppend( &p_first, p_psi );

        p_section = p_section->p_next;
    }

    return( p_first );

error:
    if( p_first )
        block_ChainRelease( p_first );
    return NULL;
}

static void PEStoTS(buffer_chain_t *c, block_t *p_pes, int i_pid)
{
    /* get PES total size */
    uint8_t *p_data = p_pes->p_buffer;
    int      i_size = p_pes->i_buffer;
    int      i_continuity_counter = 0;
    bool    b_new_pes = true;

    for (;;)
    {
        /* write header
         * 8b   0x47    sync byte
         * 1b           transport_error_indicator
         * 1b           payload_unit_start
         * 1b           transport_priority
         * 13b          pid
         * 2b           transport_scrambling_control
         * 2b           if adaptation_field 0x03 else 0x01
         * 4b           continuity_counter
         */

        int i_copy = __MIN( i_size, 184 );
        bool b_adaptation_field = i_size < 184;
        block_t *p_ts = block_Alloc( 188 );
        if (p_ts == NULL) return ;
        
        p_ts->p_buffer[0] = 0x47;
        p_ts->p_buffer[1] = ( b_new_pes ? 0x40 : 0x00 )|((i_pid >> 8 )&0x1f );
        p_ts->p_buffer[2] = (i_pid & 0xff);
        p_ts->p_buffer[3] = ( b_adaptation_field ? 0x30 : 0x10 )|i_continuity_counter;
        
        b_new_pes = false;
        i_continuity_counter = (i_continuity_counter+1)%16;

        if( b_adaptation_field )
        {
            int i_stuffing = 184 - i_copy;

            p_ts->p_buffer[4] = i_stuffing - 1;
            if( i_stuffing > 1 )
            {
                p_ts->p_buffer[5] = 0x00;
                for (int i = 6; i < 6 + i_stuffing - 2; i++ )
                {
                    p_ts->p_buffer[i] = 0xff;
                }
            }
        }
        /* copy payload */
        memcpy( &p_ts->p_buffer[188 - i_copy], p_data, i_copy );
        p_data += i_copy;
        i_size -= i_copy;

        BufferChainAppend( c, p_ts );

        if( i_size <= 0 )
        {
            block_t *p_next = p_pes->p_next;

            p_pes->p_next = NULL;
            block_Release( p_pes );
            if( p_next == NULL )
                return;

            b_new_pes = true;
            p_pes = p_next;
            i_size = p_pes->i_buffer;
            p_data = p_pes->p_buffer;
        }
    }
}

// -----------------------------------------------------------------------------
// Initialise the channel to its default settings of unallocated and reset.
// -----------------------------------------------------------------------------
Channel::Channel()
{
    next = NULL;
    reset();
}

// -----------------------------------------------------------------------------
void Channel::close()
{
    lock.on();
    for (int i = 0; i< MAX_STREAM_NUM; i++)
    {
        if (!m_stream[i].b_used)
            continue;
        if (m_stream[i].h_filter)
            //stop filter      

        if (m_stream[i].p_fifo)
            buffer_fifoRelease(m_stream[i].p_fifo);            

    }
    reset();
    lock.off();
}

// -----------------------------------------------------------------------------
void Channel::setStatus(STATUS s)
{
    if (s != status)
    {
        status = s;
    }
}
	
// -----------------------------------------------------------------------------
// Reset channel and make it available 
// -----------------------------------------------------------------------------
void Channel::reset()
{
    serviceID = 0;
    chanID.clear();

    i_pmt_pid = 0x1fff;    
    i_pcr_pid = 0x1fff;
    
    i_transport_id = 0;
    i_network_id = 0;
    i_service_id = 0;
    
    memset(&m_segment,0x0,sizeof(ts_segment_t));
    
    for (int i = 0; i< MAX_STREAM_NUM; i++)
    {
        memset(&m_stream[i],0x0,sizeof(ts_stream_t));
        m_stream[i].b_used = false;
    }
    
    setStatus(S_NONE);	    
}

// -----------------------------------
int Channel::startStream(void)
{
    lock.on();
    if (status != S_WAIT)
    {
        lock.off();
        return 0;
    }    
    
    setStatus(S_CONNECTING);
    
#ifdef _DEBUG
    i_pmt_pid = 0x18b0;    
    i_pcr_pid = 0x18a7;
    
    i_transport_id = 0xe;
    i_network_id = 0xe;
    i_service_id = 0x277;
    
    m_stream[0].b_used = true;
    m_stream[0].h_filter = 1;
    
    m_stream[0].i_cat = AUDIO_ES;
    m_stream[0].i_org_pid = 0x18a8;
    m_stream[0].i_org_type = 0x04;//stream_type first VLC_CODEC_MPGA;
    m_stream[0].p_fifo = buffer_fifoNew(64*1024);
    if (!m_stream[0].p_fifo)
    {
        printf("[dvbcast] input buffer allocate failed!\n");
        goto ERROR;
    }    
    pMuxAudioInput = &m_stream[0];

    m_stream[1].b_used = true;
    m_stream[1].h_filter = 2;   

    m_stream[1].i_cat = VIDEO_ES;
    m_stream[1].i_org_pid = 0x18a7;
    m_stream[1].i_org_type = 0x02;//VLC_CODEC_MPGV;
    m_stream[1].p_fifo = buffer_fifoNew(512*1024);
    if (!m_stream[1].p_fifo)
    {
        printf("[dvbcast] input buffer allocate failed!\n");
        goto ERROR;
    }    
    pMuxVideoInput = &m_stream[1];

    thread.data = this;
    thread.func = stream;
    if (!sys->startThread(&thread))
    {
        printf("[dvbcast] thread start error!\n");
        goto ERROR;        
    }
    setStatus(S_RECEIVING);
    lock.off();
    
    return 0;
    
ERROR:
    close();    
    pMuxAudioInput = NULL;
    pMuxVideoInput = NULL;
    lock.off();
#endif
    return -1;    
}

// -----------------------------------
int Channel::stopStream(void)
{
    lock.on();
    if (status == S_NONE)
    {
        lock.off();
        return 0;
    }
    
    setStatus(S_CLOSING);

#ifdef _DEBUG
    
    for (int i = 0; i < MAX_STREAM_NUM; i++)
    {
        if (m_stream[i].b_used)
        {
            if (m_stream[i].h_filter)
            {
                //release
            }
            m_stream[i].h_filter = 0;                 
        }
    }
#endif       
    thread.shutdown();	
    lock.off();
    return 0;
}

// -----------------------------------
bool	Channel::matchID(GnuID &id)
{
    if (id.isSet())
        if (chanID.isSame(id))
            return true;
    return false;
}

// -----------------------------------
void Channel::initID(const char *n)
{
    chanID.clear();	
    chanID.fromStr(n);
}

int Channel::getCurrentSegment(void)
{
    return (m_segment.seg_index);
}

void Channel::getDvbPsi(buffer_chain_t* p_chain)
{
    block_t              *p_pat;
    dvbpsi_pat_t         pat;
    dvbpsi_psi_section_t *p_section;
    dvbpsi_pmt_t*   p_dvbpmt = NULL;

    p_dvbpmt = (dvbpsi_pmt_t*)malloc(sizeof(dvbpsi_pmt_t));    
    if (p_dvbpmt == NULL )
            return;
    
    dvbpsi_pat_init(&pat, 0x1, 0x1, 1);      /* b_current_next */
    /* add all programs */
    dvbpsi_pat_program_add( &pat, i_service_id, i_pmt_pid);
    
    p_section = dvbpsi_pat_sections_generate(&pat, 0);
    p_pat = WritePSISection( p_section );

    PEStoTS(p_chain,  p_pat, 0x0);

    dvbpsi_DeletePSISections(p_section);
    dvbpsi_pat_empty(&pat);
    
    dvbpsi_pmt_init(p_dvbpmt,i_service_id, 0x1, 1, i_pcr_pid);
    
    for (int i_stream = 0; i_stream < MAX_STREAM_NUM; i_stream++ )
    {
        ts_stream_t *p_stream = &m_stream[i_stream];
        if (p_stream->b_used)
        {
            dvbpsi_pmt_es_t *p_es = dvbpsi_pmt_es_add(p_dvbpmt,
                p_stream->i_org_type, p_stream->i_org_pid );
        }        
    }

    dvbpsi_psi_section_t *sect = NULL;
	
    sect = dvbpsi_pmt_sections_generate(p_dvbpmt);

    block_t *pmt = WritePSISection( sect );

    PEStoTS(p_chain, pmt, i_pmt_pid);

    dvbpsi_DeletePSISections(sect);

    dvbpsi_pmt_empty(p_dvbpmt);   
    free(p_dvbpmt);
}

// -----------------------------------
THREAD_PROC Channel::stream(ThreadInfo *thread)
{
    char tmpPath[64];
    unsigned short pid = 0x1fff;
    unsigned char pk_buffer[TSPKT_LEN];    
    unsigned char peek_buffer[TSPKT_LEN*3];    
    Channel *pChannel = (Channel *)thread->data;
    unsigned int i_wcount = 0;
    
    pChannel->chanID.toStr(tmpPath);
    sprintf(pChannel->m_segment.rpath,"/%s",tmpPath);
    pChannel->m_segment.h_fileop = 0;
    pChannel->m_segment.i_pkt_num = 0;
    pChannel->m_segment.i_first_dts = 0;
    pChannel->m_segment.i_last_dts = 0;
    pChannel->m_segment.seg_index = 1;

    printf("[dvbcast]segment path: %s \n",pChannel->m_segment.rpath);    
    ramfs_fmkdir(pChannel->m_segment.rpath);

    while (thread->active)
    {
        char  oldPath[64];
        char  newPath[64];
        int i_need_sleep = 0;
        
        if (!pChannel->m_segment.h_fileop)
        {
            buffer_chain_t chain_ts; 
            int     i_packet_count = 0;

            sprintf(tmpPath,"%s/%d.ts",pChannel->m_segment.rpath,pChannel->m_segment.seg_index);                       
            pChannel->m_segment.h_fileop = ramfs_fopen(tmpPath,FA_WRITE | FA_CREATE_ALWAYS, 2*1024*1024);
            if (pChannel->m_segment.h_fileop == 0)
            {
                printf("[dvbcast] stream thread ramfs_fopen failed\n");
                sys->sleep(10);
                continue;
            }            
            pChannel->m_segment.i_pkt_num = 0;

            BufferChainInit(&chain_ts);            
            pChannel->getDvbPsi(&chain_ts);
            i_packet_count = chain_ts.i_depth;
			
            for (int i = 0; i < i_packet_count; i++)
            {
                int i_size = 0;                
                block_t *p_ts = BufferChainGet(&chain_ts);                
                if (p_ts == NULL) break;

                i_size = ramfs_fwrite(pChannel->m_segment.h_fileop,p_ts->p_buffer, p_ts->i_buffer);
                if (i_size <= 0)
                {
                    printf("[dvbcast] stream thread ramfs_fwrite failed\n");                    
                }
                block_Release(p_ts);
            }                
        }

        for (int i = 0; i < MAX_STREAM_NUM; i++)/*Read TS Packet*/
        {
            int i_skip = 0;
            if (!pChannel->m_stream[i].b_used) 
            {
                i_need_sleep++;
                continue;
            }            
            if (!buffer_fifoGet(pChannel->m_stream[i].p_fifo, pk_buffer, TSPKT_LEN))
            {
                i_need_sleep++;
                continue;
            }
           
            /* Check sync byte and re-sync if needed */
            if (pk_buffer[0] != 0x47)
            {
                if (!buffer_fifoPeek(pChannel->m_stream[i].p_fifo, peek_buffer, TSPKT_LEN*3)) /*PEEK Size: 188*3*/
                    continue;                    

                while (i_skip < TSPKT_LEN)
                {
                    if ( ( peek_buffer[i_skip] == 0x47) &&
                        ( peek_buffer[i_skip + TSPKT_LEN] == 0x47))
                        break;
                    i_skip++;
                }
                buffer_fifoSkip(pChannel->m_stream[i].p_fifo,i_skip);
                continue;
            }

            /*check pcr data ,for sync audio,video stream*/
            if (pChannel->m_stream[i].i_org_pid == pChannel->i_pcr_pid)   
            {
                mtime_t i_pcr = -1;                
                if ((pk_buffer[3]&0x20 ) && /* adaptation */
                    ( pk_buffer[5]&0x10 ) &&
                    ( pk_buffer[4] >= 7 ) )
                {
                    /* PCR is 33 bits */
                    i_pcr = ( (mtime_t)pk_buffer[6] << 25 ) |
                            ( (mtime_t)pk_buffer[7] << 17 ) |
                            ( (mtime_t)pk_buffer[8] << 9 ) |
                            ( (mtime_t)pk_buffer[9] << 1 ) |
                            ( (mtime_t)pk_buffer[10] >> 7 );                    
                    i_pcr = i_pcr * 100 / 9;
                    
                    if (pChannel->m_segment.i_first_dts == 0)
                        pChannel->m_segment.i_first_dts = i_pcr;                                        
                    pChannel->m_segment.i_last_dts = i_pcr;                    
                }
            }

            i_wcount = ramfs_fwrite(pChannel->m_segment.h_fileop, pk_buffer, TSPKT_LEN);
            if (i_wcount <= 0)
            {
                printf("[dvbcast] stream thread ramfs_fwrite failed\n");            
            }
            pChannel->m_segment.i_pkt_num++;
        }
        
        if (i_need_sleep == MAX_STREAM_NUM)
        {
            sys->sleep(1);
            continue;
        }
            
        if (pChannel->m_segment.i_last_dts <= 
            (pChannel->m_segment.i_first_dts + SEGMENT_DURATION)
            && (pChannel->m_segment.i_pkt_num <= SEGMENT_PKTS))
            continue;
        
        ramfs_fclose(pChannel->m_segment.h_fileop);
        pChannel->m_segment.h_fileop = 0;
        printf("[dvbcast] segment : %d done !\n",pChannel->m_segment.seg_index);

        pChannel->m_segment.i_pkt_num = 0;
        pChannel->m_segment.i_first_dts = 0;
        pChannel->m_segment.i_last_dts = 0;
        pChannel->m_segment.seg_index++;       
        if (pChannel->m_segment.seg_index > SEGMENT_NUM)
        {
            int index = pChannel->m_segment.seg_index - SEGMENT_NUM;
            sprintf(tmpPath,"%s/%d.ts", pChannel->m_segment.rpath,index);
            ramfs_fdelete(tmpPath);     
        }
    }
        
    if (pChannel->m_segment.h_fileop) 
    {
        ramfs_fclose(pChannel->m_segment.h_fileop);
    }
    ramfs_frmdir(pChannel->m_segment.rpath);
    
    pChannel->close();	
    sys->endThread(thread);
    return 0;
}	

// -----------------------------------
void ChanMgr::quit()
{
    Channel *ch = channels;
    while (ch)
    {
        if (ch->thread.active)
            ch->thread.shutdown();
        ch=ch->next;
    }
}

// -----------------------------------
Channel *ChanMgr::findChannel(GnuID &id)
{
    Channel *ch = channels;
    while (ch)
    {
        if (ch->chanID.isSame(id))
            return ch;
        ch=ch->next;
    }
    return NULL;
}	

Channel* ChanMgr::findChannel(unsigned short servid)
{
    Channel *ch = channels;
    while (ch)
    {
        if (ch->serviceID == servid)
            return ch;
        ch=ch->next;
    }
    return NULL;    
}

// -----------------------------------
ChanMgr::ChanMgr()
{
    channelNums = 0;
    channels = NULL;	
}

// -----------------------------------
Channel *ChanMgr::createChannel(unsigned short srvid)
{
    GnuID chanId;

#ifdef _DEBUG
    chanId.fromStr("FFFFFFFF");    
#else
    chanId.generate(srvid); 
#endif

    lock.on();
    Channel* pChannel = findChannel(srvid);
    if (pChannel)
    {
        lock.off();
        return pChannel;
    }
        
    Channel *nc = channels;
    while (nc)
    {
        if (nc->status == Channel::S_NONE)
            break;
        nc=nc->next;
    }

    if (!nc)
    {
        nc = new Channel();
        if (!nc) 
        {
            lock.off();
            return NULL;
        }				
        nc->next = channels;
        channels = nc;
        channelNums++;
        printf("allocated channel, channelNums: %d \n",channelNums);
    }
    else
    {
        printf("reused channel ,channelNums: %d!\n",channelNums);
    }

    nc->reset();
    nc->chanID = chanId;
    nc->serviceID = srvid;
    nc->setStatus(Channel::S_WAIT);
    
    lock.off();
    return nc;
}



