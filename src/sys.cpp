// ------------------------------------------------
// File : sys.cpp
// Date: 2013/11/29
// Author: zhouhy
// Desc: 
//  Sys is a base class for all things systemy, like starting threads, creating sockets etc..
//  Lock is a very basic cross platform CriticalSection class  
//  SJIS-UTF8 conversion by ????
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

#include "common.h"
#include "sys.h"
#include "socket.h"
#include <stdlib.h>
#include <time.h>

// -----------------------------------
#define isASCII(a) (a <= 0x7f) 
#define isPLAINASCII(a) (((a >= '0') && (a <= '9')) || ((a >= 'a') && (a <= 'z')) || ((a >= 'A') && (a <= 'Z')))
#define isUTF8(a,b) ((a & 0xc0) == 0xc0 && (b & 0x80) == 0x80 )
#define isESCAPE(a,b) ((a == '&') && (b == '#'))
#define isHTMLSPECIAL(a) ((a == '&') || (a == '\"') || (a == '\'') || (a == '<') || (a == '>'))

// -----------------------------------
static int base64chartoval(char input)
{
    if(input >= 'A' && input <= 'Z')
        return input - 'A';
    else if(input >= 'a' && input <= 'z')
        return input - 'a' + 26;
    else if(input >= '0' && input <= '9')
        return input - '0' + 52;
    else if(input == '+')
        return 62;
    else if(input == '/')
        return 63;
    else if(input == '=')
        return -1;
    else
        return -2;
}

// ------------------------------------------
Sys::Sys()
{
    idleSleepTime = 10;
    numThreads=0;
}

// ------------------------------------------
void Sys::sleepIdle()
{
    sleep(idleSleepTime);
}

// ------------------------------------------
bool Host::isLocalhost()
{
    return loopbackIP() || (ip == ClientSocket::getIP(NULL)); 
}
// ------------------------------------------
void Host::fromStrName(const char *str, int p)
{
    if (!strlen(str))
    {
        port = 0;
        ip = 0;
        return;
    }

    char name[128];
    strncpy(name,str,sizeof(name)-1);
    name[127] = '\0';
    port = p;
    char *pp = strstr(name,":");
    if (pp)
    {
        port = atoi(pp+1);
        pp[0] = 0;
    }

    ip = ClientSocket::getIP(name);
}
// ------------------------------------------
void Host::fromStrIP(const char *str, int p)
{
    unsigned int ipb[4];
    unsigned int ipp;

    if (strstr(str,":"))
    {
        if (sscanf(str,"%03d.%03d.%03d.%03d:%d",&ipb[0],&ipb[1],&ipb[2],&ipb[3],&ipp) == 5)
        {
            ip = ((ipb[0]&0xff) << 24) | ((ipb[1]&0xff) << 16) | ((ipb[2]&0xff) << 8) | ((ipb[3]&0xff));
            port = ipp;
        }else
        {
            ip = 0;
            port = 0;
        }
    }else{
        port = p;
        if (sscanf(str,"%03d.%03d.%03d.%03d",&ipb[0],&ipb[1],&ipb[2],&ipb[3]) == 4)
            ip = ((ipb[0]&0xff) << 24) | ((ipb[1]&0xff) << 16) | ((ipb[2]&0xff) << 8) | ((ipb[3]&0xff));
        else
            ip = 0;
    }
}
// -----------------------------------
bool Host::isMemberOf(Host &h)
{
    if (h.ip==0)
        return false;

    if( h.ip0() != 255 && ip0() != h.ip0() )
        return false;
    if( h.ip1() != 255 && ip1() != h.ip1() )
        return false;
    if( h.ip2() != 255 && ip2() != h.ip2() )
        return false;
    if( h.ip3() != 255 && ip3() != h.ip3() )
        return false;

    return true;
}

// -----------------------------------
char *trimstr(char *s1)
{
    while (*s1)
    {
        if ((*s1 == ' ') || (*s1 == '\t'))
            s1++;
        else
            break;
    }

    char *s = s1;

    if(strlen(s1) > 0) {
        s1 = s1+strlen(s1);

        while (*--s1)
            if ((*s1 != ' ') && (*s1 != '\t'))
                break;
        s1[1] = 0;
    }
    return s;
}

// -----------------------------------
char *stristr(const char *s1, const char *s2)
{
    while (*s1)
    {
        if (TOUPPER(*s1) == TOUPPER(*s2))
        {
            const char *c1 = s1;
            const char *c2 = s2;

            while (*c1 && *c2)
            {
                if (TOUPPER(*c1) != TOUPPER(*c2))
                break;
                c1++;
                c2++;
            }
            if (*c2==0)
                return (char *)s1;
        }
        s1++;
    }
    return NULL;
}
// -----------------------------------
bool String::isValidURL()
{
    return (strncasecmp(data,"http://",7)==0) || (strncasecmp(data,"mailto:",7)==0);
}

// -----------------------------------
void String::setFromTime(unsigned int t)
{
    char *p = ctime((time_t*)&t);
    if (p)
        strcpy(data,p);
    else
        strcpy(data,"-");
    type = T_ASCII;
}
// -----------------------------------
void String::setFromStopwatch(unsigned int t)
{
    unsigned int sec,min,hour,day;

    sec = t%60;
    min = (t/60)%60;
    hour = (t/3600)%24;
    day = (t/86400);

    if (day)
        sprintf(data,"%d day, %d hour",day,hour);
    else if (hour)
        sprintf(data,"%d hour, %d min",hour,min);
    else if (min)
        sprintf(data,"%d min, %d sec",min,sec);
    else if (sec)
        sprintf(data,"%d sec",sec);
    else
        sprintf(data,"-");

    type = T_ASCII;
}
// -----------------------------------
void String::setFromString(const char *str, TYPE t)
{
    int cnt=0;
    bool quote=false;
    while (*str)
    {
        bool add=true;
        if (*str == '\"')
        {
            if (quote) 
                break;
            else 
                quote = true;
            add = false;
        }else if (*str == ' ')
        {
            if (!quote)
            {
                if (cnt)
                    break;
                else
                    add = false;
            }
        }

        if (add)
        {
            data[cnt++] = *str++;
            if (cnt >= (MAX_LEN-1))
                break;
        }else
            str++;
    }
    data[cnt] = 0;
    type = t;
}

// -----------------------------------
int String::base64WordToChars(char *out,const char *input)
{
    char *start = out;
    signed char vals[4];

    vals[0] = base64chartoval(*input++);
    vals[1] = base64chartoval(*input++);
    vals[2] = base64chartoval(*input++);
    vals[3] = base64chartoval(*input++);

    if(vals[0] < 0 || vals[1] < 0 || vals[2] < -1 || vals[3] < -1) 
        return 0;

    *out++ = vals[0]<<2 | vals[1]>>4;
    if(vals[2] >= 0)
        *out++ = ((vals[1]&0x0F)<<4) | (vals[2]>>2);
    else
        *out++ = 0;

    if(vals[3] >= 0)
        *out++ = ((vals[2]&0x03)<<6) | (vals[3]);
    else
        *out++ = 0;

    return out-start;
}

// -----------------------------------
void String::BASE642ASCII(const char *input)
{
    char *out = data;
    int len = strlen(input);

    while(len >= 4) 
    {
        out += base64WordToChars(out,input);
        input += 4;
        len -= 4;
    }
    *out = 0;
}

// -----------------------------------
void String::UNKNOWN2UNICODE(const char *in,bool safe)
{
    MemoryStream utf8(data,MAX_LEN-1);

    unsigned char c;
    unsigned char d;

    while ((c = *in++))
    {
        d = *in;

        if (isUTF8(c,d))  // utf8 encoded 
        {
            int numChars=0;
            int i;

            for(i=0; i<6; i++)
            {
                if (c & (0x80>>i))
                    numChars++;
                else
                    break;
            }

            utf8.writeChar(c);
            for(i=0; i<numChars-1; i++)
            utf8.writeChar(*in++);
        }
        else if (isESCAPE(c,d))  // html escape tags &#xx;
        {
            in++;
            char code[16];
            char *cp = code;
            while ((c=*in++)) 
            {
                if (c!=';')
                    *cp++ = c;
                else
                    break;
            }
            *cp = 0;
            utf8.writeUTF8(strtoul(code,NULL,10));
        }
        else if (isPLAINASCII(c)) // plain ascii : a-z 0-9 etc..
        {
            utf8.writeUTF8(c);
        }
        else if (isHTMLSPECIAL(c) && safe)   
        {
            const char *str = NULL;
            if (c == '&') str = "&amp;";
            else if (c == '\"') str = "&quot;";
            else if (c == '\'') str = "&#039;";
            else if (c == '<') str = "&lt;";
            else if (c == '>') str = "&gt;";
            else str = "?";

            utf8.writeString(str);
        }
        else
        {
            utf8.writeUTF8(c);
        }

        if (utf8.pos >= (MAX_LEN-10))
            break;
    }

    utf8.writeChar(0); // null terminate
}

// -----------------------------------
void String::ASCII2HTML(const char *in)
{
    char *op = data;
    char *oe = data+MAX_LEN-10;
    unsigned char c;
    const char *p = in;
    while ((c = *p++))
    {

        if (((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')))
        {
            *op++ = c;
        }else
        {
            sprintf(op,"&#x%02X;",(int)c);
            op+=6;
        }
        if (op >= oe)
            break;
    }
    *op = 0;
}
// -----------------------------------
void String::ASCII2ESC(const char *in, bool safe)
{
    char *op = data;
    char *oe = data+MAX_LEN-10;
    const char *p = in;
    unsigned char c;
    while ((c = *p++))
    {
        if (((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')))
            *op++ = c;
        else
        {
            *op++ = '%';
            if (safe)
                *op++ = '%';
            *op=0;
            sprintf(op,"%02X",(int)c);
            op+=2;
        }
        if (op >= oe)
            break;
    }
    *op=0;
}
// -----------------------------------
void String::HTML2ASCII(const char *in)
{
    unsigned char c;
    char *o = data;
    char *oe = data+MAX_LEN-10;
    const char *p = in;
    while ((c = *p++))
    {
        if ((c == '&') && (p[0] == '#'))
        {
            p++;
            char code[8];
            char *cp = code;
            char ec = *p++;  // hex/dec
            while ((c=*p++)) 
            {
                if (c!=';')
                    *cp++ = c;
                else
                    break;
            }
            *cp = 0;
            c = (unsigned char)strtoul(code,NULL,ec=='x'?16:10);
        }
        *o++ = c;
        if (o >= oe)
            break;
    }

    *o=0;
}
// -----------------------------------
void String::HTML2UNICODE(const char *in)
{
    MemoryStream utf8(data,MAX_LEN-1);

    unsigned char c;
    while ((c = *in++))
    {
        if ((c == '&') && (*in == '#'))
        {
            in++;
            char code[16];
            char *cp = code;
            char ec = *in++;  // hex/dec
            while ((c=*in++)) 
            {
                if (c!=';')
                    *cp++ = c;
                else
                    break;
            }
            *cp = 0;
            utf8.writeUTF8(strtoul(code,NULL,ec=='x'?16:10));
        }else
            utf8.writeUTF8(c);

        if (utf8.pos >= (MAX_LEN-10))
            break;
    }

    utf8.writeUTF8(0);
}

// -----------------------------------
void String::ESC2ASCII(const char *in)
{
    unsigned char c;
    char *o = data;
    char *oe = data+MAX_LEN-10;
    const char *p = in;
    while ((c = *p++))
    {
        if (c == '+')
            c = ' ';
        else if (c == '%')
        {
            if (p[0] == '%')
                p++;

            char hi = TOUPPER(p[0]);
            char lo = TOUPPER(p[1]);
            c = (TONIBBLE(hi)<<4) | TONIBBLE(lo);
            p+=2;
        }
        *o++ = c;
        if (o >= oe)
            break;
    }

    *o=0;
}
// -----------------------------------
void String::ASCII2META(const char *in, bool safe)
{
    char *op = data;
    char *oe = data+MAX_LEN-10;
    const char *p = in;
    unsigned char c;
    while ((c = *p++))
    {
        switch (c)
        {
        case '%':
            if (safe)
            *op++='%';
            break;
        case ';':
            c = ':';
            break;
        }

        *op++=c;
        if (op >= oe)
            break;
    }
    *op=0;
}
// -----------------------------------
void String::convertTo(TYPE t)
{
    if (t != type)
    {
        String tmp = *this;

        // convert to ASCII
        switch (type)
        {
        case T_UNKNOWN:
        case T_ASCII:
            break;
        case T_HTML:
            tmp.HTML2ASCII(data);
            break;
        case T_ESC:
        case T_ESCSAFE:
            tmp.ESC2ASCII(data);
            break;
        case T_META:
        case T_METASAFE:
            break;
        case T_BASE64:
            tmp.BASE642ASCII(data);
            break;
        default:
            break;
        }

        // convert to new format
        switch (t)
        {
        case T_UNKNOWN:
        case T_ASCII:
            strcpy(data,tmp.data);
            break;
        case T_UNICODE:
            UNKNOWN2UNICODE(tmp.data,false);
            break;
        case T_UNICODESAFE:
            UNKNOWN2UNICODE(tmp.data,true);
            break;
        case T_HTML:
            ASCII2HTML(tmp.data);
            break;
        case T_ESC:
            ASCII2ESC(tmp.data,false);
            break;
        case T_ESCSAFE:
            ASCII2ESC(tmp.data,true);
            break;
        case T_META:
            ASCII2META(tmp.data,false);
            break;
        case T_METASAFE:
            ASCII2META(tmp.data,true);
            break;
        default:
            break;
        }

        type = t;
    }
}

// ---------------------------
void GnuID::toStr(char *str)
{
    str[0] = 0;
    for(int i=0; i<4; i++)
    {
        char tmp[8];
        unsigned char ipb = id[i];

        sprintf(tmp,"%02X",ipb);
        strcat(str,tmp);
    }
}
// ---------------------------
void GnuID::fromStr(const char *str)
{
    clear();

    if (strlen(str) < 8)
        return;

    char buf[8];

    buf[2] = 0;
    
    for (int i=0; i< 4; i++)
    {
        buf[0] = str[i*2];
        buf[1] = str[i*2+1];
        id[i] = (unsigned char)strtoul(buf,NULL,16);
    }
}

// ---------------------------
void GnuID::generate(unsigned short sid)
{
    clear();
    
    for(int i=0; i < 4; i++)
        id[i] = sys->rnd();

    id[0] = ((sid&0xff00)>>8);
    id[1] = ((sid&0x00ff));
}
 
// ---------------------------
void ThreadInfo::shutdown()
{
    active = false;
}
