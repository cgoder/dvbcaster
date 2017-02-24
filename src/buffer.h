// ------------------------------------------------
// File : buffer.h
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

#ifndef BUFFER_H
#define BUFFER_H 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <stdint.h>

typedef void (*block_free_t) (void *);

typedef struct block_t
{
    block_t    *p_next;

    uint8_t    *p_buffer; /**< Payload start */
    size_t      i_buffer; /**< Payload length */
    uint8_t    *p_start; /**< Buffer start */
    size_t      i_size; /**< Buffer total size */
    
    /* Rudimentary support for overloading block (de)allocation. */
    block_free_t pf_release;
}block_t;

typedef struct 
{
    int     i_depth;
    block_t *p_first;
    block_t **pp_last;
}buffer_chain_t;

typedef struct 
{
    int   writep;                  
    int   readp;
    unsigned char* buffer; 
    unsigned int  bsize;        
}buffer_fifo_t;

void block_Init( block_t *, void *, size_t );
block_t *block_Alloc( size_t );
void block_Release( block_t *p_block );
void block_ChainAppend(block_t **pp_list, block_t *p_block );
void block_ChainRelease( block_t *p_block);

void BufferChainInit(buffer_chain_t *c);
void BufferChainAppend(buffer_chain_t *c, block_t *b );
block_t *BufferChainGet(buffer_chain_t *c );
block_t *BufferChainPeek(buffer_chain_t *c );
void BufferChainClean(buffer_chain_t *c );

buffer_fifo_t *buffer_fifoNew(int);
void buffer_fifoRelease(buffer_fifo_t *);
int buffer_fifoPut(buffer_fifo_t *, unsigned char *, unsigned int);
int buffer_fifoGet(buffer_fifo_t *,unsigned char*,unsigned int);
int buffer_fifoPeek(buffer_fifo_t*,unsigned char*,unsigned int);
int buffer_fifoSkip(buffer_fifo_t*,unsigned int);
int buffer_fifoFsize(buffer_fifo_t*);
int buffer_fifoBsize(buffer_fifo_t*);

#endif 

