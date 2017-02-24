/*----------------------------------------------------------------------------/
/  FatFs - FAT file system module  R0.08b                 (C)ChaN, 2011
/-----------------------------------------------------------------------------/
/ FatFs module is a generic FAT file system module for small embedded systems.
/ This is a free software that opened for education, research and commercial
/ developments under license policy of following terms.
/
/  Copyright (C) 2011, ChaN, all right reserved.
/
/ * The FatFs module is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include "ff.h"
#include "sys.h"
#include "ramdisk.h"
#include "common.h"
#include "buffer.h"

#define WR_CACHE_SIZE (1024)
#define PATH_LEN  (128) 
#define SECTOR_SIZE (512)
#define SECTOR_NUMS (24*1024)
#define SECTOR_SAFE (1024)
#define RAMFS_SIZE (SECTOR_NUMS*SECTOR_SIZE + SECTOR_SAFE) //8MB
#define ALIGN_PTR(ptr, alignment) (((unsigned int)(ptr) + (alignment)-1) & ~((unsigned int)((alignment)-1)))

#define MAX_QUEUE_LEN  (64)
#define QUICK_FW_LEN (8)

typedef enum
{
    F_OPEN = 0x1,
    F_CLOSE,
    F_READ,
    F_WRITE,
    F_DELETE,
    F_RENAME,
    F_MKDIR,
    F_RMDIR,
    F_SIZE,    
}OPERATOR_e;

typedef struct 
{
    FATFS*      p_fatfs;
    unsigned char* p_buffer;
    unsigned int    i_buffer;    
    unsigned char* p_align;
    uint32_t        h_msgq;
    uint32_t        thread_active;    
    pthread_mutex_t  s_mutex;
}ramdisk_t;

typedef struct
{
    uint32_t  msg;
    void*  handle;
    uint32_t  p1;
    uint32_t  p2;
}ramfs_msg_t;

typedef struct 
{
    int         head; 
    int         tail;
    ramfs_msg_t Item[MAX_QUEUE_LEN];     
}ramfs_msgq_t;

typedef struct 
{
    FIL*      p_hfile;
    sem_t*    p_hsem;
    buffer_fifo_t*  p_fifo;  //read or write buffer;    
    FRESULT         result;
}ramfs_fileop_t;

static ramdisk_t m_ramfs[1];
static void* ramfs_thread(void* thread);

static void* align_malloc(unsigned int size ,unsigned int alignment)
{
    unsigned char* ptr;
    unsigned char* tmp;
    if (!alignment) alignment = 4;

    tmp = (unsigned char*) malloc(size + alignment);
    if (tmp == NULL)
        return NULL;
    ptr = (unsigned char*)ALIGN_PTR(tmp,alignment);
    if (ptr == tmp)
    {
        ptr += alignment;
    }
    *(ptr -1) = (unsigned char)(ptr - tmp);
    return (void*)ptr;
}

static void align_free(void* pmem)
{
    unsigned char* ptr = (unsigned char*)pmem;

    if (ptr == NULL) 
        return ;
    
    ptr -= *(ptr -1);
    free(ptr);
}

static uint32_t msgq_create(void)
{
    ramfs_msgq_t* me = (ramfs_msgq_t*)malloc(sizeof(ramfs_msgq_t));
    if (me == NULL) return 0;

    me->head = 0;
    me->tail = 0;
    return (uint32_t)me;
}

static int msgq_destory(uint32_t handle)
{
    ramfs_msgq_t* me = (ramfs_msgq_t*)handle;
    
    if (me != NULL)
    {
        free(me);
    }
    return 0;
}

static int msgq_sendmsg(uint32_t handle, ramfs_msg_t *Msg)
{
    ramfs_msgq_t* me = (ramfs_msgq_t*)handle;
    if (me == NULL || Msg == NULL)  return -1;

    if ((me->tail + 1)%MAX_QUEUE_LEN == me->head)
    {
        return -1;
    }
    memcpy(&me->Item[me->tail], Msg, sizeof(ramfs_msg_t));
    me->tail = (me->tail+1)%(MAX_QUEUE_LEN);
    return 0;
}

static int msgq_recvmsg(uint32_t handle, ramfs_msg_t *Msg)
{
    ramfs_msgq_t* me = (ramfs_msgq_t*)handle;

    if (NULL == Msg || me == NULL) return -1;

    if ((me->head%MAX_QUEUE_LEN) == me->tail) return -1;

    memcpy(Msg, &me->Item[me->head], sizeof(ramfs_msg_t));
    me->head = (me->head+1)%(MAX_QUEUE_LEN);      
    return 0;
}

int ramdisk_init(void)
{
    int res = 0;
    pthread_mutexattr_t ma;
    pthread_attr_t attr;
    pthread_t tid;

    memset(m_ramfs,0x0,sizeof(ramdisk_t));
    m_ramfs->p_fatfs = (FATFS*)align_malloc(sizeof(FATFS), 4);
    if (m_ramfs->p_fatfs == NULL)
    {
        printf("[dvbcast] fatfs struct allocate failed!\n");
        goto oom;
    }

    m_ramfs->p_buffer = (unsigned char*)malloc(RAMFS_SIZE);
    if (m_ramfs->p_buffer == NULL)
    {
        printf("[dvbcast] ramdisk buffer allocate failed!\n");
        goto oom;
    }
    m_ramfs->i_buffer = RAMFS_SIZE;        
    memset(m_ramfs->p_buffer,0x0,RAMFS_SIZE);

    m_ramfs->p_align = (unsigned char*)ALIGN_PTR(m_ramfs->p_buffer,SECTOR_SIZE);
    printf("p_buffer: 0x%x,p_align: 0x%x \n",m_ramfs->p_buffer,m_ramfs->p_align);
    
    m_ramfs->h_msgq = msgq_create();
    if (m_ramfs->h_msgq == 0)
    {
        printf("[dvbcast] msg queue create  failed!\n");        
        goto oom;
    }
    
    pthread_mutexattr_init(&ma);
    pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
    if (0 != pthread_mutex_init(&m_ramfs->s_mutex,&ma) )
    {        
        pthread_mutexattr_destroy(&ma);
        goto oom;
    }
    pthread_mutexattr_destroy(&ma);

    res = f_mount(RAMFS_VOL, m_ramfs->p_fatfs);
    if (res != FR_OK)//fs mount failed
    {
        printf("[dvbcast] ramdisk mount failed: %d!\n",res);        
        goto oom;
    }
    
    res = f_mkfs(RAMFS_VOL, 0, 1);
    if (res != FR_OK)
    {
        printf("[dvbcast] ramdisk mkfs failed: %d!\n",res);        
        goto oom;
    }

    m_ramfs->thread_active = 1;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid,&attr,ramfs_thread,NULL);
    pthread_attr_destroy(&attr);
    
    return 0;
oom:    
    m_ramfs->thread_active = 0;
    pthread_mutex_destroy(&m_ramfs->s_mutex);    

    if (f_mount(RAMFS_VOL,NULL) != FR_OK)
    {
        printf("[dvbcast] ramdisk f_mount failed!\n");        
    }

    if (m_ramfs->p_buffer != NULL)
    {
        free(m_ramfs->p_buffer);
        m_ramfs->p_buffer = NULL;
    }
    m_ramfs->i_buffer = 0;
    m_ramfs->p_align = NULL;

    if (m_ramfs->h_msgq != 0)
    {
        msgq_destory(m_ramfs->h_msgq);
        m_ramfs->h_msgq = 0;    
    }    

    if (m_ramfs->p_fatfs != NULL)
    {
        align_free(m_ramfs->p_fatfs);
        m_ramfs->p_fatfs = NULL;
    }    
    return -1;
}

int ramdisk_destory(void)
{
    int ret = 0;
    
    m_ramfs->thread_active = 0;		
    ret = pthread_mutex_destroy(&m_ramfs->s_mutex);
    if (EBUSY == ret )
    {
        pthread_mutex_unlock( &m_ramfs->s_mutex );
        pthread_mutex_destroy( &m_ramfs->s_mutex );        
    }

    if (f_mount(RAMFS_VOL,NULL) != FR_OK)
    {
        printf("[dvbcast] ramdisk f_mount failed!\n");        
    }

    if (m_ramfs->p_buffer != NULL)
    {
        free(m_ramfs->p_buffer);
        m_ramfs->p_buffer = NULL;
    }
    m_ramfs->i_buffer = 0;
    m_ramfs->p_align = NULL;

    if (m_ramfs->h_msgq != 0)
    {
        msgq_destory(m_ramfs->h_msgq);
        m_ramfs->h_msgq = 0;    
    }    

    if (m_ramfs->p_fatfs != NULL)
    {
        align_free(m_ramfs->p_fatfs);
        m_ramfs->p_fatfs = NULL;
    }    
    
    memset(m_ramfs,0x0,sizeof(ramdisk_t));
    return 0;
}

DSTATUS disk_initialize (BYTE drv)
{
    return 0;
}

DSTATUS disk_status(BYTE drv)
{
    return 0;
}

DRESULT disk_read (BYTE drv,BYTE *buff, DWORD sector,BYTE count)
{
    DWORD offset;
    if (m_ramfs->p_align == NULL) return RES_ERROR;
    
    if ((sector+count) > SECTOR_NUMS)
    {
        printf("[dvbcast] disk_read invalid paramters \n");
        return RES_PARERR;    
    }
    
    offset = sector*SECTOR_SIZE;    
    memcpy(buff,m_ramfs->p_align+offset, count*SECTOR_SIZE);
    return RES_OK;
}

DRESULT disk_write (BYTE drv,const BYTE *buff,	DWORD sector,BYTE count)
{
    DWORD offset;
    if (m_ramfs->p_align == NULL) return RES_ERROR;
    
    if ((sector+count) > SECTOR_NUMS)
    {
        printf("[dvbcast] disk_write invalid paramters \n");
        return RES_PARERR;    
    }
        
    offset = sector*SECTOR_SIZE;    
    memcpy(m_ramfs->p_align+offset,buff,count*SECTOR_SIZE);
    return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(BYTE drv,	BYTE ctrl, void *buff)
{
	DRESULT res = RES_PARERR;
    
	switch (ctrl) {
	case CTRL_SYNC:
		res = RES_OK;
		break;

	case GET_SECTOR_COUNT:
		*(DWORD*)buff = SECTOR_NUMS;
		res = RES_OK;
		break;

	case GET_SECTOR_SIZE:
		*(WORD*)buff = SECTOR_SIZE;
		res = RES_OK;
		break;

	case GET_BLOCK_SIZE:
		*(DWORD*)buff = 1;
		res = RES_OK;
		break;
	}

	return res;
}

uint32_t ramfs_fopen(const char *path, BYTE mode, uint32_t i_cache)
{
    ramfs_msg_t msg[1];
    ramfs_fileop_t* pfile = NULL;
    
    if (path == NULL) return 0;
    
    pfile = (ramfs_fileop_t*)malloc(sizeof(ramfs_fileop_t));
    if (pfile == NULL)
    {
        printf("[dvbcast] ramfs_fopen handle allocate failed!\n");
        return 0;
    }
    
    memset(pfile,0x0,sizeof(ramfs_fileop_t));    
    pfile->p_hfile = (FIL*)align_malloc(sizeof(FIL), 4);
    if (pfile->p_hfile == NULL)
    {
        printf("[dvbcast] ramfs_fopen FIL allocate failed!\n");
        goto oom;
    }
    
    pfile->p_hsem = (sem_t*)malloc(sizeof(sem_t));
    if (pfile->p_hsem == NULL)
    {
        printf("[dvbcast] ramfs_fopen sem allocate failed!\n");
        goto oom;
    }
    
    pfile->p_fifo = buffer_fifoNew(i_cache);
    if (!pfile->p_fifo)
    {
        printf("[dvbcast] buffer_fifoNew allocate failed!\n");
        goto oom;
    }
    
    if (sem_init(pfile->p_hsem, 0, 0) != 0)
    {
        printf("[dvbcast] sem init failed!\n");
        goto oom;
    }

    pfile->result = FR_OK;
    msg->msg = F_OPEN;
    msg->handle = (void*)pfile;
    msg->p1 = (uint32_t)path;
    msg->p2 = (uint32_t)mode;
    
    pthread_mutex_lock(&m_ramfs->s_mutex);
    if (msgq_sendmsg(m_ramfs->h_msgq, msg) != 0)
    {
        printf("[dvbcast] _%s_ msg send error!\n",__FUNCTION__);
        pthread_mutex_unlock(&m_ramfs->s_mutex);
        goto oom;
    }
    pthread_mutex_unlock(&m_ramfs->s_mutex);

    if (sem_wait(pfile->p_hsem) != 0)
    {
        printf("[dvbcast] _%s_ sem_wait error!\n",__FUNCTION__);
        goto oom;
    }
    if (pfile->result != FR_OK)
        goto oom;

    return (uint32_t)pfile;
oom:    
    if (pfile->p_hsem != NULL)
    {
        sem_destroy(pfile->p_hsem);
        free(pfile->p_hsem);
        pfile->p_hsem = NULL;
    }
    
    if (pfile->p_fifo != NULL)
    {
        buffer_fifoRelease(pfile->p_fifo);  
        pfile->p_fifo = NULL;
    }

    if (pfile->p_hfile != NULL)
    {
        align_free(pfile->p_hfile);
        pfile->p_hfile = NULL;
    }
    
    free(pfile);
    return 0;
}

int ramfs_fclose(uint32_t handle)
{
    ramfs_msg_t msg[1];
    ramfs_fileop_t* pfile = (ramfs_fileop_t*)handle;

    if (pfile == NULL) return 0;
    
    pfile->result = FR_OK;
    msg->msg = F_CLOSE;
    msg->handle = (void*)pfile;

    pthread_mutex_lock(&m_ramfs->s_mutex);
    if (msgq_sendmsg(m_ramfs->h_msgq, msg) != 0)
    {
        printf("[dvbcast] _%s_ msg send error!\n",__FUNCTION__);
        pthread_mutex_unlock(&m_ramfs->s_mutex);
        return -1;
    }
    pthread_mutex_unlock(&m_ramfs->s_mutex);

    if (sem_wait(pfile->p_hsem) != 0)
    {
        printf("[dvbcast] _%s_ sem_wait error!\n",__FUNCTION__);
        return -1;
    }
    
    if (pfile->result != FR_OK)
    {
        printf("[dvbcast] file closed error: %d!\n",pfile->result);
        return -1;
    }    

    sem_destroy(pfile->p_hsem);    
    free(pfile->p_hsem);
    pfile->p_hsem = NULL;

    buffer_fifoRelease(pfile->p_fifo);  
    pfile->p_fifo = NULL;

    align_free(pfile->p_hfile);
    pfile->p_hfile = NULL;
        
    free(pfile);
    return 0;
}

int ramfs_fread(uint32_t handle, uint8_t* buffer, int size)
{
    int i_size = 0;
    ramfs_msg_t msg[1];
    ramfs_fileop_t* pfile = (ramfs_fileop_t*)handle;

    if (pfile == NULL || buffer == NULL || size <= 0) return 0;

    if (buffer_fifoGet(pfile->p_fifo, buffer, size))
    {
        return size;
    }

    pfile->result = FR_OK;
    msg->msg = F_READ;
    msg->handle = (void*)pfile;
    msg->p1 = size;    
    
    pthread_mutex_lock(&m_ramfs->s_mutex);
    if (msgq_sendmsg(m_ramfs->h_msgq, msg) != 0)
    {
        printf("[dvbcast] _%s_ msg send error!\n",__FUNCTION__);
        pthread_mutex_unlock(&m_ramfs->s_mutex);
        return 0;
    }
    pthread_mutex_unlock(&m_ramfs->s_mutex);

    if (sem_wait(pfile->p_hsem) != 0)
    {
        printf("[dvbcast] _%s_ sem_wait error!\n",__FUNCTION__);
    }    
    if (pfile->result != FR_OK)
    {
        printf("[dvbcast] file closed error: %d!\n",pfile->result);
        return 0;
    }    
    
    i_size = buffer_fifoBsize(pfile->p_fifo);
    if (i_size >= size) i_size = size;
    
    if (buffer_fifoGet(pfile->p_fifo, buffer, i_size))
    {
        return i_size;
    }
    return 0;
}

int ramfs_fwrite(uint32_t handle, uint8_t* buffer, int size)
{
    ramfs_msg_t msg[1];
    ramfs_fileop_t* pfile = (ramfs_fileop_t*)handle;

    if (pfile == NULL || buffer == NULL || size <= 0) return 0;

    if (buffer_fifoPut(pfile->p_fifo, buffer, size))
    {
        return size;
    }

    pfile->result = FR_OK;    
    msg->msg = F_WRITE;
    msg->handle = (void*)pfile;
    msg->p1 = size;    
    
    pthread_mutex_lock(&m_ramfs->s_mutex);
    if (msgq_sendmsg(m_ramfs->h_msgq, msg) != 0)
    {
        printf("[dvbcast] _%s_ msg send error!\n",__FUNCTION__);
        pthread_mutex_unlock(&m_ramfs->s_mutex);
        return 0;
    }
    pthread_mutex_unlock(&m_ramfs->s_mutex);

    if (sem_wait(pfile->p_hsem) != 0)
    {
        printf("[dvbcast] _%s_ sem_wait error!\n",__FUNCTION__);
    }    
    if (pfile->result != FR_OK)
    {
        printf("[dvbcast] file closed error: %d!\n",pfile->result); 
        return 0;          
    }    

    if (buffer_fifoPut(pfile->p_fifo, buffer, size))
    {
        return size;
    }
    return 0;
}

int ramfs_fdelete(const char *path)
{
    ramfs_msg_t msg[1];
    char* ppath = NULL;
    if (path == NULL) return 0;

    ppath = (char*)malloc(PATH_LEN);
    if (ppath == NULL)
        return 0;
    strncpy(ppath,path,PATH_LEN-1);
    ppath[PATH_LEN-1] = 0;
    
    msg->msg = F_DELETE;
    msg->p1 = (uint32_t)ppath;
    
    pthread_mutex_lock(&m_ramfs->s_mutex);
    if (msgq_sendmsg(m_ramfs->h_msgq, msg) != 0)
    {
        printf("[dvbcast] _%s_ msg send error!\n",__FUNCTION__);
        free(ppath);

        pthread_mutex_unlock(&m_ramfs->s_mutex);
        return -1;
    }
    pthread_mutex_unlock(&m_ramfs->s_mutex);
    return 0;
}

int ramfs_fmkdir(const char *path)
{
    ramfs_msg_t msg[1];
    char* ppath = NULL;

    if (path == NULL) return 0;

    ppath = (char*)malloc(PATH_LEN);
    if (ppath == NULL)
        return 0;
    strncpy(ppath,path,PATH_LEN-1);
    ppath[PATH_LEN-1] = 0;

    msg->msg = F_MKDIR;
    msg->p1 = (uint32_t)ppath;

    pthread_mutex_lock(&m_ramfs->s_mutex);
    if (msgq_sendmsg(m_ramfs->h_msgq, msg) != 0)
    {
        printf("[dvbcast] _%s_ msg send error!\n",__FUNCTION__);
        free(ppath);
        
        pthread_mutex_unlock(&m_ramfs->s_mutex);
        return -1;
    }
    pthread_mutex_unlock(&m_ramfs->s_mutex);
    return 0;
}

int ramfs_frmdir(const char *path)
{
    ramfs_msg_t msg[1];
    char* ppath = NULL;

    if (path == NULL) return 0;

    ppath = (char*)malloc(PATH_LEN);
    if (ppath == NULL)
        return 0;
    strncpy(ppath,path,PATH_LEN-1);
    ppath[PATH_LEN-1] = 0;

    msg->msg = F_RMDIR;
    msg->p1 = (uint32_t)ppath;
    
    pthread_mutex_lock(&m_ramfs->s_mutex);
    if (msgq_sendmsg(m_ramfs->h_msgq, msg) != 0)
    {
        printf("[dvbcast] _%s_ msg send error!\n",__FUNCTION__);
        pthread_mutex_unlock(&m_ramfs->s_mutex);
        return -1;
    }
    pthread_mutex_unlock(&m_ramfs->s_mutex);
    return 0;
}

static void* ramfs_thread(void* param)
{
    UINT    bw = 0;
    bool    b_sleep = true;
    unsigned char buffer[WR_CACHE_SIZE];
    ramfs_fileop_t* m_file[QUICK_FW_LEN];
    ramfs_msg_t rmsg[1];

    for (int i = 0; i< QUICK_FW_LEN; i++)
    {
        m_file[i] = NULL;
    }    
    while(m_ramfs->thread_active)
    {
        b_sleep = true;
        if (msgq_recvmsg(m_ramfs->h_msgq, rmsg) != 0)
        {
            for (int i = 0; i < QUICK_FW_LEN; i++)
            {
                int idx = 0;
                if (m_file[i] == NULL)
                {
                    continue;
                }
                
                for (idx = 0; idx < 64; idx++)
                {
                    if (buffer_fifoGet(m_file[i]->p_fifo,buffer, WR_CACHE_SIZE))
                    {
                        if (f_write(m_file[i]->p_hfile, buffer, WR_CACHE_SIZE, &bw) != FR_OK)
                        {
                            printf("[dvbcast] f_write error!\n");
                            break;
                        }             
                        continue;
                    }//end if
                    break;
                }                
                if (idx == 64) b_sleep = false;                
            }
            
            if (b_sleep)
            {
                sys->sleep(10);     
            }      
            continue;
        }
        
        if (rmsg->msg == F_OPEN)
        {
            char* path = (char*)rmsg->p1;
            BYTE  mode = (BYTE)rmsg->p2;
            ramfs_fileop_t* pfile = (ramfs_fileop_t*)rmsg->handle;
            
            pfile->result = f_open(pfile->p_hfile, path, mode);
            if (pfile->result == FR_OK)
            {
                if (mode & FA_WRITE)
                {
                    for (int i = 0; i< QUICK_FW_LEN; i++)
                    {
                        if (m_file[i] != NULL)
                            continue;
                        m_file[i] = pfile;
                        break;
                    }
                }
            }
            sem_post(pfile->p_hsem);
        }
        else if (rmsg->msg == F_CLOSE)
        {
            int i_size = 0;
            ramfs_fileop_t* pfile = (ramfs_fileop_t*)rmsg->handle;

            for (int i = 0; i < QUICK_FW_LEN; i++)
            {
                if (m_file[i] == pfile)
                {
                    for (;;)
                    {
                        i_size = buffer_fifoBsize(pfile->p_fifo);
                        if (i_size > WR_CACHE_SIZE) 
                            i_size = WR_CACHE_SIZE;

                        if (i_size <= 0)
                            break;

                        if (buffer_fifoGet(pfile->p_fifo, buffer, i_size))
                        {
                            int res = FR_OK;
                            res = f_write(pfile->p_hfile, buffer,i_size, &bw);
                            if (res != FR_OK)
                            {
                                printf("[dvbcast] F_CLOSE error res: %d!\n",res);
                            }
                            continue;
                        }
                        break;                
                    }
                    m_file[i] = NULL;
                    break;
                }
            }
            
            pfile->result = f_close(pfile->p_hfile);
            sem_post(pfile->p_hsem);
        }
        else if (rmsg->msg == F_READ)
        {
            int i_size = 0,res = FR_OK;
            ramfs_fileop_t* pfile = (ramfs_fileop_t*)rmsg->handle;

            for (int idx = 0; idx < 64; idx++)
            {
                i_size = buffer_fifoFsize(pfile->p_fifo);
                if (i_size > WR_CACHE_SIZE) 
                    i_size = WR_CACHE_SIZE;                

                if (i_size <= 0)
                    break;

                res = f_read(pfile->p_hfile, buffer, i_size, &bw);
                if (res != FR_OK || bw <= 0)
                {
                    break;
                }
                                
                if (!buffer_fifoPut(pfile->p_fifo, buffer, bw))
                {
                    printf("[dvbcast] f_read error bw: %u!\n",bw);
                    break;
                }
            }
            pfile->result = FR_OK;
            sem_post(pfile->p_hsem);
        }
        else if (rmsg->msg == F_WRITE)
        {
            int i_size = 0;
            ramfs_fileop_t* pfile = (ramfs_fileop_t*)rmsg->handle;

            for (int i = 0; i < QUICK_FW_LEN; i++)
            {
                if (m_file[i] == pfile)
                {
                    for (int idx = 0; idx < 64; idx++)
                    {
                        i_size = buffer_fifoBsize(pfile->p_fifo);
                        if (i_size > WR_CACHE_SIZE) 
                            i_size = WR_CACHE_SIZE;

                        if (i_size <= 0)
                            break;

                        if (buffer_fifoGet(pfile->p_fifo, buffer, i_size))
                        {
                            int res = FR_OK;
                            res = f_write(pfile->p_hfile, buffer,i_size, &bw);
                            if (res != FR_OK)
                            {
                                printf("[dvbcast] F_WRITE error res: %d!\n",res);
                            }
                            continue;
                        }
                        break;                
                    }
                    break;
                }
            }
            pfile->result = FR_OK;
            sem_post(pfile->p_hsem);
        }
        else if (rmsg->msg == F_DELETE)
        {
            char* path = (char*)rmsg->p1;
            if (!path) continue;

            if (f_unlink(path) != FR_OK)
            {
                printf("[dvbcast] f_unlink file error!\n");
            }
            free(path);
        }
        else if (rmsg->msg == F_RENAME)
        {
            char* oldpath = (char*)rmsg->p1;
            char* newpath = (char*)rmsg->p2;

            if (!oldpath || !newpath) continue;
            
            if (f_rename(oldpath, newpath) != FR_OK)
            {
                printf("[dvbcast] f_rename error!\n");
            }
            free(oldpath);
            free(newpath);
            
        }
        else if (rmsg->msg == F_MKDIR)
        {
            char* path = (char*)rmsg->p1;
            if (!path) continue;
            
            if (f_mkdir(path) != FR_OK)
            {
                printf("[dvbcast] f_mkdir error!\n");
            }
            free(path);
        }
        else if (rmsg->msg == F_RMDIR)
        {
            char* path = (char*)rmsg->p1;
            if (!path) continue;

            if (f_unlink(path) != FR_OK)
            {
                printf("[dvbcast] f_unlink dir error!\n");            
            }
            free(path);
        }
        else if (rmsg->msg == F_SIZE)
        {
            FILINFO fno;            
            char* path = (char*)rmsg->p1;

            uint32_t* i_size = (uint32_t*)rmsg->p2;
            sem_t* p_sem = (sem_t*)rmsg->handle;
            
            *i_size = 0;
            if (f_stat(path,&fno) ==FR_OK) 
            {
                *i_size = fno.fsize;
            }            
            sem_post(p_sem);
        }

    }
    
    return NULL;
}
