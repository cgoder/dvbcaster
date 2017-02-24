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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include "buffer.h"

#define BLOCK_ALIGN        32
/** Initial reserved header and footer size. */
#define BLOCK_PADDING      32

static void block_generic_Release (void *block)
{
    block_t *p_block = (block_t*)block;
    /* That is always true for blocks allocated with block_Alloc(). */
    assert (p_block->p_start == (unsigned char *)(p_block + 1));
    free (p_block);
}

void block_Init(block_t *b, void *buf, size_t size )
{
    /* Fill all fields to their default */
    b->p_next = NULL;
    b->p_buffer = (uint8_t*)buf;
    b->i_buffer = size;
    b->p_start = (uint8_t*)buf;
    b->i_size = size;
}

block_t *block_Alloc (size_t size)
{
    /* 2 * BLOCK_PADDING: pre + post padding */
    const size_t alloc = sizeof (block_t) + BLOCK_ALIGN + (2 * BLOCK_PADDING)
                       + size;
    block_t *b = (block_t*)malloc (alloc);
    if (b == NULL)
        return NULL;

    block_Init (b, b + 1, alloc - sizeof (*b));
    b->p_buffer += BLOCK_PADDING + BLOCK_ALIGN - 1;
    b->p_buffer = (uint8_t *)(((uintptr_t)b->p_buffer) & ~(BLOCK_ALIGN - 1));
    b->i_buffer = size;
    b->pf_release = block_generic_Release;
    return b;
}

void block_Release( block_t *p_block )
{
    p_block->pf_release( p_block );
}

void block_ChainAppend(block_t **pp_list, block_t *p_block )
{
    if( *pp_list == NULL )
    {
        *pp_list = p_block;
    }
    else
    {
        block_t *p = *pp_list;

        while( p->p_next ) p = p->p_next;
        p->p_next = p_block;
    }
}

void block_ChainRelease( block_t *p_block )
{
    while( p_block )
    {
        block_t *p_next = p_block->p_next;
        block_Release( p_block );
        p_block = p_next;
    }
}

void BufferChainInit(buffer_chain_t *c)
{
    c->i_depth = 0;
    c->p_first = NULL;
    c->pp_last = &c->p_first;
}

void BufferChainAppend(buffer_chain_t *c, block_t *b )
{
    *c->pp_last = b;
    c->i_depth++;

    while( b->p_next )
    {
        b = b->p_next;
        c->i_depth++;
    }
    c->pp_last = &b->p_next;
}

block_t *BufferChainGet(buffer_chain_t *c )
{
    block_t *b = c->p_first;

    if( b )
    {
        c->i_depth--;
        c->p_first = b->p_next;

        if( c->p_first == NULL )
        {
            c->pp_last = &c->p_first;
        }

        b->p_next = NULL;
    }
    return b;
}

block_t *BufferChainPeek(buffer_chain_t *c )
{
    block_t *b = c->p_first;

    return b;
}

void BufferChainClean(buffer_chain_t *c )
{
    block_t *b;

    while( (b = BufferChainGet( c ) ) )
    {
        block_Release( b );
    }
    BufferChainInit( c );
}

buffer_fifo_t *buffer_fifoNew(int size)
{
    buffer_fifo_t* p_fifo =  (buffer_fifo_t*)malloc(sizeof( buffer_fifo_t));
    if (!p_fifo)
        return NULL;
    
    p_fifo->buffer = (unsigned char*)malloc(size);
    if (!p_fifo->buffer)
    {
        free(p_fifo);
        return NULL;
    }
    p_fifo->writep = 0;
    p_fifo->readp = 0;
    p_fifo->bsize = size;
    return p_fifo;
}

void buffer_fifoRelease(buffer_fifo_t * p_fifo)
{
    if (!p_fifo) return ;
    
    if (p_fifo->buffer)
        free(p_fifo->buffer);
    
    free(p_fifo);
}

int buffer_fifoPut(buffer_fifo_t *p_fifo, unsigned char *buffer, unsigned int size)
{
    int length = 0;
    int remsize = 0;
    unsigned int u32Writep = 0;
    unsigned int u32Readp = 0;

    if (!p_fifo || buffer == NULL || size <= 0) return 0;

    u32Writep = p_fifo->writep;
    u32Readp = p_fifo->readp;

    if (u32Writep >= u32Readp)
    {
        length = u32Writep - u32Readp;
    }
    else
    {
        length = p_fifo->bsize - u32Readp + u32Writep;
    }
    
    remsize = p_fifo->bsize - length;
    if (remsize <= size)
    {
        return 0;
    }

    if (u32Writep >= u32Readp)
    {
        if ((p_fifo->bsize - u32Writep) >= size)
        {
            memcpy(p_fifo->buffer + u32Writep, buffer, size);
            p_fifo->writep += size;
        }
        else
        {
            length = p_fifo->bsize - u32Writep;
            remsize = size - length;
            memcpy(p_fifo->buffer + u32Writep, buffer, length);
            memcpy(p_fifo->buffer, buffer+length, remsize);
            p_fifo->writep = remsize;
        }
    }
    else
    {
        memcpy(p_fifo->buffer + u32Writep, buffer, size);
        p_fifo->writep += size;
    }
    return size;
}    

int buffer_fifoGet(buffer_fifo_t *p_fifo,unsigned char*buffer,unsigned int size)
{
    int length = 0;
    int remsize = 0;
    int buffer_size = 0;
    unsigned int u32Writep = 0;
    unsigned int u32Readp = 0;
    
    if (!p_fifo||buffer == NULL || size <= 0) return 0;
    
    u32Writep = p_fifo->writep;
    u32Readp = p_fifo->readp;
    
    if (u32Writep >= u32Readp)
    {
        buffer_size = u32Writep - u32Readp;
    }
    else if (u32Writep < u32Readp)
    {
        buffer_size = p_fifo->bsize - u32Readp + u32Writep;
    }
    
    if (buffer_size < size)
    {
        return 0;
    }

    if (u32Writep >= u32Readp)
    {
        memcpy((char*)buffer, p_fifo->buffer+u32Readp, size);
        p_fifo->readp += size;
    }
    else
    {
        length = p_fifo->bsize - u32Readp;
        if ( length >= size )
        {
            memcpy((char*)buffer, p_fifo->buffer+u32Readp, size);
            p_fifo->readp += size;
        }
        else
        {
            remsize = size - length;
            memcpy((char*)buffer, p_fifo->buffer+u32Readp,length);
            memcpy((char*)buffer+length, p_fifo->buffer, remsize);
            p_fifo->readp = remsize;
        }
    }
    return  size;
}

int buffer_fifoPeek(buffer_fifo_t* p_fifo,unsigned char* buffer,unsigned int size)
{
    int length = 0;
    int remsize = 0;
    int buffer_size = 0;
    unsigned int u32Writep = 0;
    unsigned int u32Readp = 0;

    if (!p_fifo || buffer == NULL || size <= 0) return 0;

    u32Writep = p_fifo->writep;
    u32Readp = p_fifo->readp;
    if (u32Writep >= u32Readp)
    {
        buffer_size = u32Writep - u32Readp;
    }
    else if (u32Writep < u32Readp)
    {
        buffer_size = p_fifo->bsize - u32Readp + u32Writep;
    }
    
    if (buffer_size < size)
    {
        return 0;
    }

    if (u32Writep >= u32Readp)
    {
        memcpy((char*)buffer, p_fifo->buffer+u32Readp, size);
    }
    else
    {
        length = p_fifo->bsize - u32Readp;
        if ( length >= size )
        {
            memcpy((char*)buffer, p_fifo->buffer+u32Readp, size);
            p_fifo->readp += size;
        }
        else
        {
            remsize = size - length;
            memcpy((char*)buffer, p_fifo->buffer+u32Readp,length);
            memcpy((char*)buffer+length, p_fifo->buffer, remsize);
        }
    }
    return  size;
}

int buffer_fifoSkip(buffer_fifo_t* p_fifo,unsigned int size)
{
    int length = 0;
    int remsize = 0;
    int buffer_size = 0;
    unsigned int u32Writep = 0;
    unsigned int u32Readp = 0;

    if (!p_fifo ||  size <= 0) return 0;

    u32Writep = p_fifo->writep;
    u32Readp = p_fifo->readp;    
    if (u32Writep >= u32Readp)
    {
        buffer_size = u32Writep - u32Readp;
    }
    else if (u32Writep < u32Readp)
    {
        buffer_size = p_fifo->bsize - u32Readp + u32Writep;
    }
    
    if (buffer_size < size)
    {
        return 0;
    }

    if (u32Writep >= u32Readp)
    {
        p_fifo->readp += size;
    }
    else
    {
        length = p_fifo->bsize - u32Readp;
        if ( length >= size )
        {
            p_fifo->readp += size;
        }
        else
        {
            remsize = size - length;
            p_fifo->readp = remsize;
        }
    }
    return  size;
}

int buffer_fifoFsize(buffer_fifo_t* p_fifo)
{
    int length = 0;
    int remsize = 0;
    unsigned int u32Writep = 0;
    unsigned int u32Readp = 0;

    if (!p_fifo) return 0;
    u32Writep = p_fifo->writep;
    u32Readp = p_fifo->readp;    

    if (u32Writep >= u32Readp)
    {
        length = u32Writep - u32Readp;
    }
    else
    {
        length = p_fifo->bsize - u32Readp + u32Writep;
    }
    
    remsize = p_fifo->bsize - length -1;
    return remsize;
}


int buffer_fifoBsize(buffer_fifo_t* p_fifo)
{
    int buffer_size = 0;
    unsigned int u32Writep = 0;
    unsigned int u32Readp = 0;

    if (!p_fifo) return 0;
    u32Writep = p_fifo->writep;
    u32Readp = p_fifo->readp;    

    if (u32Writep >= u32Readp)
    {
        buffer_size = u32Writep - u32Readp;
    }
    else if (u32Writep < u32Readp)
    {
        buffer_size = p_fifo->bsize - u32Readp + u32Writep;
    }
    return buffer_size;    
}


