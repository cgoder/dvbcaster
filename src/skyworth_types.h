#ifndef __SKYDVB_TYPES_H__
#define __SKYDVB_TYPES_H__


typedef unsigned char SKYDVB_U8;
typedef unsigned char SKYDVB_UCHAR;
typedef unsigned short SKYDVB_U16;
typedef unsigned int SKYDVB_U32;
typedef unsigned long SKYDVB_LONG;

typedef signed char SKYDVB_S8;
typedef short SKYDVB_S16;
typedef int SKYDVB_S32;

typedef unsigned long long SKYDVB_U64;
typedef long long SKYDVB_S64;

typedef char SKYDVB_CHAR;
typedef char* SKYDVB_PCHAR;

typedef float SKYDVB_FLOAT;
typedef double SKYDVB_DOUBLE;

typedef enum
{
    SKYDVB_FALSE    = 0,
    SKYDVB_TRUE     = 1,
}SKYDVB_BOOL;

#define SKYDVB_VOID void

#ifndef SKYDVB_NULL
#define SKYDVB_NULL             (0L)
#endif


#define SKYDVB_SUCCESS       (1)
#define SKYDVB_FAILURE       (-1)

#endif
