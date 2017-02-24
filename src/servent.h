// ------------------------------------------------
// File : servent.h
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


#ifndef _SERVENT_H
#define _SERVENT_H

// ----------------------------------
#include "socket.h"
#include "sys.h"
#include "channel.h"
#include "http.h"
#include "rtsp.h"

// ----------------------------------
// Servent handles the actual connection between clients
class Servent
{
public:
    enum TYPE					
    {
        T_NONE,					// Not allocated
        T_INCOMING,				// Unknown incoming
        T_SERVER,				// The main server 
        T_DIRECT,				// Outgoing direct connection
    };


    enum STATUS
    {
        S_NONE,
        S_CONNECTING,
        S_PROTOCOL,
        S_HANDSHAKE,
        S_CONNECTED,
        S_CLOSING,
        S_LISTENING,
        S_ERROR,
        S_WAIT,
        S_FREE
    };

    enum PROTOCOL
    {
        SP_UNKNOWN,
        SP_HTTP,
        SP_RTSP
    };

    enum ALLOW
    {
        ALLOW_HTML		= 0x01,
        ALLOW_BROADCAST 	= 0x02,
        ALLOW_NETWORK	= 0x04,
        ALLOW_DIRECT		= 0x08,
        ALLOW_ALL			= 0xff
    };

    Servent(int);
    ~Servent();

    void	reset();
    bool	initServer(Host &);
    void	initIncoming(ClientSocket *,unsigned int);

    void	checkFree();

    //	funcs for handling status/type
    void	setStatus(STATUS);

    // static funcs that do the actual work in the servent thread
    static	THREAD_PROC serverProc(ThreadInfo *);
    static	THREAD_PROC incomingProc(ThreadInfo *);

    void	handshakeCMD(const char *cmd);
    bool	handshakeAuth(HTTP &,const char *);
    bool	handshakeStream(const char*);

    void	handshakeRTSP(RTSP &);
    void	handshakeHTTP(HTTP &);

    void	handshakeLocalFile(const char *);

    void	sendRawChannel(void);
    bool	canStream(Channel *);

    bool	isConnected() {return status == S_CONNECTED;}

    bool	isAllowed(int);
    bool	isFiltered(int);

    //connection handling funcs
    void	createSocket();
    void	kill();
    void	abort();
    bool	isPrivate();

    Host	getHost();

    TYPE type;
    STATUS status;

    GnuID	networkID;
    GnuID	chanID;

    ThreadInfo	thread;
    unsigned int allow;
    
    ClientSocket *sock;
    int servPort;

    PROTOCOL	outputProtocol;
    Cookie	cookie;
    
    Servent	*next;
};

#endif

