// ------------------------------------------------
// File : servent.cpp
// Date: 2013/11/29
// Author: zhouhy
// Desc: 
//		Servents are the actual connections between clients. They do the handshaking,
//		transfering of data and processing of GnuPackets. Each servent has one socket allocated
//		to it on connect, it uses this to transfer all of its data.
//
// (c) 2002 peercast.org
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
// todo: make lan->yp not check firewall

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ff.h"
#include "ramdisk.h"
#include "sys.h"
#include "xml.h"
#include "html.h"
#include "http.h"
#include "servmgr.h"
#include "servent.h"

#define MAX_RFS_SIZE (65*1024)
const int DIRECT_WRITE_TIMEOUT = 3;
extern unsigned int m_segment;
// -----------------------------------
char *getCGIarg(const char *str, const char *arg)
{
    if (!str)
        return NULL;

    char *s = strstr((char*)str,(char*)arg);

    if (!s)
        return NULL;

    s += strlen(arg);

    return s;
}

// -----------------------------------
bool cmpCGIarg(const char *str, const char *arg, const char *value)
{
    if ((!str) || (!strlen(value)))
        return false;

    if (strncasecmp(str,arg,strlen(arg)) == 0)
    {
        str += strlen(arg);
        return strncmp(str,value,strlen(value))==0;
    }else
        return false;
}
// -----------------------------------
bool hasCGIarg(const char *str, const char *arg)
{
    if (!str)
        return false;

    char *s = strstr((char*)str,(char*)arg);

    if (!s)
        return false;

    return true;
}
// -----------------------------------
char *nextCGIarg(const char *cp, char *cmd, char *arg)
{
    if (!*cp)
        return NULL;

    int cnt=0;

    // fetch command
    while (*cp)
    {
        char c = *cp++;
        if (c == '=')
            break;
        else
            *cmd++ = c;

        cnt++;
        if (cnt >= (MAX_CGI_LEN-1))
            break;
    }
    *cmd = 0;

    cnt=0;
    // fetch arg
    while (*cp)
    {
        char c = *cp++;
        if (c == '&')
            break;
        else
            *arg++ = c;

        cnt++;
        if (cnt >= (MAX_CGI_LEN-1))
            break;
    }
    *arg = 0;

    return (char*)cp;
}

// -----------------------------------
bool getCGIargBOOL(const char *a)
{
    return (strcmp(a,"1")==0);
}

// -----------------------------------
int getCGIargINT(const char *a)
{
    return atoi(a);
}

// -----------------------------------
bool	Servent::isPrivate() 
{
    Host h = getHost();
    return servMgr->isFiltered(ServFilter::F_PRIVATE,h) || h.isLocalhost();
}

// -----------------------------------
bool	Servent::isAllowed(int a) 
{
    Host h = getHost();

    if (servMgr->isFiltered(ServFilter::F_BAN,h))
        return false;

    return (allow&a)!=0;
}

// -----------------------------------
bool	Servent::isFiltered(int f) 
{
    Host h = getHost();
    return servMgr->isFiltered(f,h);
}

// -----------------------------------
Servent::Servent(int index)
{
    sock = NULL;
    next = NULL;
    reset();
}

// -----------------------------------
Servent::~Servent()
{	

}

// -----------------------------------
void	Servent::kill() 
{
    thread.shutdown();

    setStatus(S_CLOSING);
    if (sock)
    {
        sock->close();
        delete sock;
        sock = NULL;
    }

    if (type != T_SERVER)
    {
        reset();
        setStatus(S_FREE);
    }
}
// -----------------------------------
void	Servent::abort() 
{
    thread.shutdown();
    if (sock)
    {
        sock->close();
    }
}

// -----------------------------------
void Servent::reset()
{
    servPort = 0;

    networkID.clear();

    chanID.clear();

    outputProtocol = SP_UNKNOWN;

    sock = NULL;
    allow = ALLOW_ALL;	

    status = S_NONE;
    type = T_NONE;
}

// -----------------------------------
Host Servent::getHost()
{
    Host h(0,0);

    if (sock)
        h = sock->host;

    return h;
}

// -----------------------------------
bool Servent::initServer(Host &h)
{
    try
    {
        checkFree();

        status = S_WAIT;

        createSocket();

        sock->bind(h);

        thread.data = this;

        thread.func = serverProc;

        type = T_SERVER;

        if (!sys->startThread(&thread))
            throw StreamException("Can`t start thread");
    }
    catch(StreamException &e)
    {
        kill();
        return false;
    }

    return true;
}
// -----------------------------------
void Servent::checkFree()
{
    if (sock)
        throw StreamException("Socket already set");
    if (thread.active)
        throw StreamException("Thread already active");
}

// -----------------------------------
void Servent::initIncoming(ClientSocket *s, unsigned int a)
{

    try
    {

        checkFree();

        type = T_INCOMING;
        sock = s;
        allow = a;
        thread.data = this;
        thread.func = incomingProc;

        setStatus(S_PROTOCOL);

        char ipStr[64];
        sock->host.toStr(ipStr);
        printf("Incoming from %s \n",ipStr);

        if (!sys->startThread(&thread))
            throw StreamException("Can`t start thread");
    }
    catch(StreamException &e)
    {
        kill();
        printf("INCOMING FAILED: %s \n",e.msg);
    }
}

// -----------------------------------
void Servent::createSocket()
{
    if (sock)
        printf("[dvbcast] Servent::createSocket attempt made while active \n");

    sock = sys->createSocket();
}

// -----------------------------------
void Servent::setStatus(STATUS s)
{
    if (s != status)
    {
        status = s;
    }
}

// -----------------------------------
void Servent::handshakeRTSP(RTSP &rtsp)
{
    type = T_DIRECT;	
    outputProtocol = SP_RTSP;             

    throw HTTPException(HTTP_SC_BADREQUEST,400);
}

// -----------------------------------
bool Servent::handshakeAuth(HTTP &http,const char *args)
{
    char user[64],pass[64];
    user[0] = pass[0] = 0;

    char *pwd  = getCGIarg(args, "pass=");

    if ((pwd) && strlen(servMgr->password))
    {
        String tmp = pwd;
        char *as = strstr(tmp.cstr(),"&");
        if (as) *as = 0;
        if (strcmp(tmp,servMgr->password)==0)
        {
            while (http.nextHeader());
                return true;
        }
    }

    Cookie gotCookie;
    cookie.clear();

    while (http.nextHeader())
    {
        char *arg = http.getArgStr();
        if (!arg)
            continue;
        switch (servMgr->authType)
        {
        case ServMgr::AUTH_HTTPBASIC:
            if (http.isHeader("Authorization"))
                http.getAuthUserPass(user,pass);
            break;
        case ServMgr::AUTH_COOKIE:
            if (http.isHeader("Cookie"))
            {
                char *idp=arg;
                while ((idp = strstr(idp,"id=")))
                {
                    idp+=3;
                    gotCookie.set(idp,sock->host.ip);
                    if (servMgr->cookieList.contains(gotCookie))
                    {
                        cookie = gotCookie;
                        break;
                    }
                }
            }
            break;
        }
    }

#ifdef _DEBUG
    //if (sock->host.isLocalhost()) //test
#endif
    return true;
    
    switch (servMgr->authType)
    {
    case ServMgr::AUTH_HTTPBASIC:
        if ((strcmp(pass,servMgr->password)==0) && strlen(servMgr->password))
            return true;
        break;
    case ServMgr::AUTH_COOKIE:
        if (servMgr->cookieList.contains(cookie))
            return true;
        break;
    }

    if (servMgr->authType == ServMgr::AUTH_HTTPBASIC)
    {
        http.writeLine(HTTP_SC_UNAUTHORIZED);
        http.writeLine("WWW-Authenticate: Basic realm=\"Admin\"");
    }
    else if (servMgr->authType == ServMgr::AUTH_COOKIE)
    {
        String file;
        file.append("/html/login.html");
        handshakeLocalFile(file);
    }
    return false;
}

// -----------------------------------
void Servent::handshakeHTTP(HTTP &http)
{
    char *in = http.cmdLine;
    
    if (http.isRequest("GET /"))
    {
        char *fn = in+4;
        char *pt = strstr(fn,HTTP_PROTO1);
        if (pt) pt[-1] = 0;

        if (strncmp(fn,"/stream/",8)==0) //only for dvb stream /stream/channelid/filename
        {
            String filePath = fn+7;
            String chanStr = fn+8;
            
            if (!sock->host.isLocalhost())
            {
                if (!isAllowed(ALLOW_DIRECT) || !isFiltered(ServFilter::F_DIRECT))
                    throw HTTPException(HTTP_SC_UNAVAILABLE,503);			
            }

            if (!handshakeAuth(http,fn))
                throw HTTPException(HTTP_SC_UNAUTHORIZED,401);	
            
            type = T_DIRECT;	
            outputProtocol = SP_HTTP;             

            chanID.fromStr(chanStr);               
            handshakeStream(filePath);	
        }
        else if (strncmp(fn,"/admin?",7)==0)
        {
            String args = fn+7;
            if (!isAllowed(ALLOW_HTML))
                throw HTTPException(HTTP_SC_UNAVAILABLE,503);

            if (!handshakeAuth(http,fn))
                throw HTTPException(HTTP_SC_UNAUTHORIZED,401);	

            handshakeCMD(args);
        }
        else if (strncmp(fn,"/admin?/",8)==0)
        {
            String args = fn+8;
            if (!isAllowed(ALLOW_HTML))
                throw HTTPException(HTTP_SC_UNAVAILABLE,503);

            if (!handshakeAuth(http,fn))
                throw HTTPException(HTTP_SC_UNAUTHORIZED,401);	

            handshakeCMD(args);
        }
        else if (strncmp(fn,"/html/",6)==0) //使用/html 路径, 后续完善成/apppath/html/
        {
            String dirName = fn; //获取app 路径

            if (!isAllowed(ALLOW_HTML))
                throw HTTPException(HTTP_SC_UNAVAILABLE,503);
            
            if (!handshakeAuth(http,fn))
                throw HTTPException(HTTP_SC_UNAUTHORIZED,401);	

            handshakeLocalFile(dirName);
        }
        else 
        {
            while (http.nextHeader());
            http.writeLine(HTTP_SC_FOUND);
            http.writeLine("Location: /html/index.html");
            http.writeLine("");
        }
    }
    else
    {
        throw HTTPException(HTTP_SC_BADREQUEST,400);
    }
}

// -----------------------------------
void Servent::handshakeCMD(const char *cmd)
{
    char arg[MAX_CGI_LEN];
    char curr[MAX_CGI_LEN];

    char	jumpStr[128];
    char	*jumpArg=NULL;
    bool	retHTML=false;

    HTTP http(*sock);
    HTML html("",*sock);

    //数据统一使用XML 的方式组织发送。
    if (cmpCGIarg(cmd,"cmd=","startservice"))
    {
        int serviceid = -1;
        const char *cp = cmd;

        while (cp=nextCGIarg(cp,curr,arg))
        {
            if (strcmp(curr,"serviceid") == 0)
                serviceid = getCGIargINT(arg);
        }

#if 1 //_DEBUG            
        char IDString[16];
        Channel* p_channel = chanMgr->createChannel(serviceid);
        if (p_channel)
        {
            p_channel->startStream();
            p_channel->chanID.toStr(IDString);

            http.writeLine(HTTP_SC_FOUND);
            http.writeLineF("Location: /stream/%s",IDString);
            http.writeLine("");
        }
        else
        {
            http.writeLine(HTTP_SC_NOTFOUND);
            http.writeLine("");
        }        
#endif        
        /*like :<startservice>
              <param>
                  <networkid>
                  <transportid>
                  <serviceid>
              </param>
              <result>
                  <code> </code>是否成功的标记,失败的话  返回原因，系统资源不够，权限不够等等.
                  <hls>http://xxx/F8AD999X81DAX2/xxx.m3u8 </hls> //成功返回播放地址
                  <rtsp>rtsp://xxxv/F8AD999X81DAX2/xx.ts </rtsp>   //成功返回播放地址 
              </result> 
        */
    }    
    else if (cmpCGIarg(cmd,"cmd=","stopservice"))
    {
        int serviceid = -1;
        const char *cp = cmd;

        while (cp=nextCGIarg(cp,curr,arg))
        {
            if (strcmp(curr,"serviceid") == 0)
                serviceid = getCGIargINT(arg);
        }

#if 1 //_DEBUG            
        GnuID chID;
        chID.fromStr("FFFFFFFF");
        
        Channel* p_channel = chanMgr->findChannel(serviceid);
        if (p_channel)
        {
            p_channel->stopStream();        
        }            
        http.writeLine(HTTP_SC_OK);
        http.writeLine("");
#endif        
    }
    else if (cmpCGIarg(cmd,"cmd=","logout"))
    {
        retHTML = true;
        sprintf(jumpStr,"/html/index.html");					
        jumpArg = jumpStr;
        servMgr->cookieList.remove(cookie);
    }
    else if (cmpCGIarg(cmd,"cmd=","login"))
    {
        GnuID id;
        char idstr[64];

        retHTML = true;

        id.generate(0xff9e);
        id.toStr(idstr);

        cookie.set(idstr,sock->host.ip);
        servMgr->cookieList.add(cookie);	

        http.writeLine(HTTP_SC_FOUND);
        if (servMgr->cookieList.neverExpire)
            http.writeLineF("%s id=%s; path=/; expires=\"Mon, 01-Jan-3000 00:00:00 GMT\";",HTTP_HS_SETCOOKIE,idstr);
        else
            http.writeLineF("%s id=%s; path=/;",HTTP_HS_SETCOOKIE,idstr);
        http.writeLineF("Location: /html/index.html");
        http.writeLine("");
    }
    else
    {
        retHTML = true;
        sprintf(jumpStr,"/html/index.html");					
        jumpArg = jumpStr;
    }
    
    if (retHTML)
    {
        if (jumpArg)
        {
            String jmp(jumpArg,String::T_HTML);
            jmp.convertTo(String::T_ASCII);
            html.locateTo(jmp.cstr());
        }
    }
    //搜索,系统设置等功能后续完善。
}

// -----------------------------------
void Servent::handshakeLocalFile(const char *fn)
{
    HTTP http(*sock);
    HTML html("",*sock);
    String fileName;

    fileName.append(fn);
    char *args = strstr(fileName.cstr(),"?");
    if (args) *args++=0;

    if (fileName.contains(".htm"))
    {
        html.writeOK(MIME_HTML);
        html.writeTemplate(fileName.cstr(),args);
    }
    else if (fileName.contains(".css"))
    {
        html.writeOK(MIME_CSS);
        html.writeRawFile(fileName.cstr());
    }else if (fileName.contains(".jpg"))
    {
        html.writeOK(MIME_JPEG);
        html.writeRawFile(fileName.cstr());
    }
    else if (fileName.contains(".gif"))
    {
        html.writeOK(MIME_GIF);
        html.writeRawFile(fileName.cstr());
    }else if (fileName.contains(".png"))
    {
        html.writeOK(MIME_PNG);
        html.writeRawFile(fileName.cstr());
    }
}

// -----------------------------------
bool Servent::handshakeStream(const char* fn)
{
    bool chanReady=false;	
    bool sReady = false;
    String fileName;
   
    fileName.append(fn);
    char *args = strstr(fileName.cstr(),"?");
    if (args) *args++=0;

    printf("[dvbcast] GET/ %s \n",fn);

    Channel *channel= chanMgr->findChannel(chanID);
    if (channel)
    {
        chanReady = canStream(channel);
        if (!chanReady)
            throw HTTPException(HTTP_SC_UNAVAILABLE,503);       
    }
    
    //wait for channel file ready
    for (int i=0; i < 20; i++)   
    {
        Channel* ch = chanMgr->findChannel(chanID);
        if (!ch)  
            throw HTTPException(HTTP_SC_NOTFOUND,404);       

        if (!thread.active || !sock->active())
            throw StreamException("thread closed");
        
        if (ch->isPlaying())
        {            
            sReady = true;
            break;
        }
        sys->sleep(10);
    }    
    if (!sReady) throw HTTPException(HTTP_SC_UNAVAILABLE,503);            

    setStatus(S_CONNECTED);

    if (outputProtocol == SP_HTTP)
    {        
        sock->writeLine(HTTP_SC_OK);
        sock->writeLine("Accept-Ranges: none"); //no seekable 
        sock->writeLineF("Content-Length: 0");
        sock->writeLineF("%s %s",HTTP_HS_CONTENT,MIME_TS);
        sock->writeLine("Cache-Control: no-cache");	
        sock->writeLine("Connection: close");
        sock->writeLine("");
    }
    else if (outputProtocol == SP_RTSP)
    {
        return false;
    }
    
    sendRawChannel();          
    setStatus(S_CLOSING);
    return true;
}

// -----------------------------------
int Servent::incomingProc(ThreadInfo *thread)
{
    Servent *sv = (Servent*)thread->data;

    char ipStr[64];
    sv->sock->host.toStr(ipStr);

    try 
    {
        char buf[1024];
        char sb[64];

        sv->setStatus(Servent::S_HANDSHAKE);
        sv->sock->readLine(buf,sizeof(buf));	
        if (stristr(buf,HTTP_PROTO1))	
        {	
            HTTP http(*sv->sock);
            http.initRequest(buf);
            sv->handshakeHTTP(http);
        } 
        else if (stristr(buf,RTSP_PROTO1))
        {
            RTSP rtsp(*sv->sock);
            rtsp.initRequest(buf);
            sv->handshakeRTSP(rtsp);
        }
    }
    catch(HTTPException &e)
    {
        try
        {
            sv->sock->writeLine(e.msg);
            if (e.code == 401)
                sv->sock->writeLine("WWW-Authenticate: Basic realm=\"Admin\"");
            sv->sock->writeLine("");
        }
        catch(StreamException &)
        {
        }
        printf("Incoming from %s: %s \n",ipStr,e.msg);
    }
    catch(StreamException &e)
    {
        printf("Incoming from %s: %s \n",ipStr,e.msg);
    }

    sv->kill();
    sys->endThread(thread);
    return 0;
}

bool Servent::canStream(Channel *ch)
{
    if (ch==NULL)
        return false;

    if (!isPrivate()) 
    {
        if  (!ch->isActive() || (type == T_DIRECT && servMgr->directFull()))
            return false;
    }

    return true;
}

// -----------------------------------
void Servent::sendRawChannel(void)
{
    unsigned char buffer[TSPKT_LEN];
    uint32_t    h_fileop = 0;
    char    tmpPath[64];
    char    segPath[16];
    int       rSegIndex = 1;
    
    chanID.toStr(segPath);    
    try
    {             
        sock->setNagle(true);
        sock->setLinger(1);                        
        sock->setBufSize(64*1024);
        sock->setWriteTimeout(DIRECT_WRITE_TIMEOUT*1000); 
        
        while ((thread.active) && sock->active())
        {
            int i_size = 0;
            int wSegIndex = 0;
            
            if (h_fileop == 0) // open new file
            {
                Channel *pChannel = chanMgr->findChannel(chanID);
                if (!pChannel)
                    throw StreamException("Channel not closed");
                
                wSegIndex = pChannel->getCurrentSegment();
                if (rSegIndex >= wSegIndex)
                {
                    sys->sleep(10);
                    continue;
                }
                                
                if ((rSegIndex + SAFE_SEGMENT) < wSegIndex)// 1,2,3,4 5                   
                    rSegIndex = wSegIndex - SAFE_SEGMENT;
                
                sprintf(tmpPath,"/%s/%d.ts", segPath, rSegIndex);        
                h_fileop = ramfs_fopen(tmpPath,FA_OPEN_EXISTING | FA_READ, MAX_RFS_SIZE);
                if (h_fileop == 0)
                {
                    sys->sleep(100);
                    continue;
                }
                rSegIndex++;                
            }
            
            i_size = ramfs_fread(h_fileop, buffer, TSPKT_LEN);
            if (i_size <= 0)
            {
                ramfs_fclose(h_fileop);
                h_fileop = 0;
                continue;
            }            
            sock->write(buffer, i_size);
        }		
        
    }		
    catch(StreamException &e)
    {
        printf("Stream channel: %s \n",e.msg);
    }
    
    if (h_fileop != 0)
    {
        ramfs_fclose(h_fileop);
    }
}

// -----------------------------------
int Servent::serverProc(ThreadInfo *thread)
{
    Servent *sv = (Servent*)thread->data;
    try 
    {
        if (!sv->sock)
            throw StreamException("Server has no socket");

        sv->setStatus(S_LISTENING);

        char servIP[64];
        sv->sock->host.toStr(servIP);

        printf("Server started: %s  \n",servIP);		

        while ((thread->active) && (sv->sock->active()))
        {
            if (servMgr->numActiveOnPort(sv->sock->host.port) < servMgr->maxServIn)
            {
                ClientSocket *cs = sv->sock->accept();
                if (cs)
                {	
                    printf("accepted incoming \n");
                    Servent *ns = servMgr->allocServent();
                    if (ns)
                    {
                        ns->servPort = sv->sock->host.port;
                        ns->networkID = servMgr->networkID;
                        ns->initIncoming(cs,sv->allow);
                    }else
                        printf("Out of servents \n");
                }
            }
            sys->sleep(100);
        }
    }
    catch(StreamException &e)
    {
        printf("Server Error: %s:%d \n",e.msg,e.err);
    }

    printf("Server stopped \n");
    sv->kill();
    sys->endThread(thread);
    return 0;
}
 
