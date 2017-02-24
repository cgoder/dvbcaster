#ifndef __SKYWORTH_DVB_H__
#define __SKYWORTH_DVB_H__

#include "skyworth_types.h"

/*高频头类型*/
typedef enum 
{
    TUNER_TYPE_CABLE,           /*有线类型    */
    TUNER_TYPE_SATELITE,  /*卫星信号类型*/
    TUNER_TYPE_RESERVE = 0xFF   /*预留*/
}Tuner_Type_e;

/*调制方式类型*/
typedef enum {
    MODULATION_TYPE_QAM16,
    MODULATION_TYPE_QAM32,
    MODULATION_TYPE_QAM64,
    MODULATION_TYPE_QAM128,
    MODULATION_TYPE_QAM256,
    MODULATION_TYPE_QPSK,
    MODULATION_TYPE_8PSK,
    MODULATION_TYPE_RESEVE = 0xFF
}MODULATION_TYPE_e;

/*向前纠错外码类型*/
typedef enum 
{
    FEC_OUTER_NONE,             /*未定义     */
    FEC_OUTER_WITHOUT,      /*无FEC外码  */
    FEC_OUTER_RS,                /*RS(204/188)*/
    FEC_OUTER_RESERVE = 0xFF     /*预留*/
}FEC_OUTER_e;

/*向前纠错内码类型*/
typedef enum
{
    FEC_INNER_NONE,                         /*未定义     */
    FEC_INNER_CONVOLUTION_1_2,  /*卷积码率1/2*/
    FEC_INNER_CONVOLUTION_2_3,  /*卷积码率2/3*/
    FEC_INNER_CONVOLUTION_3_4,  /*卷积码率3/4*/
    FEC_INNER_CONVOLUTION_5_6,  /*卷积码率5/6*/
    FEC_INNER_CONVOLUTION_7_8,  /*卷积码率7/8*/
    FEC_INNER_CONVOLUTION_8_9,  /*卷积码率8/9*/
    FEC_INNER_RESERVE = 0xFF   /*预留*/
}FEC_INNER_e;

/* type of polarization (sat) */
typedef enum {
    POLARIZATION_TYPE_HORIZONTAL,       /*线性   水平极化*/
    POLARIZATION_TYPE_VERTICAL,          /*线性   垂直极化*/
    POLARIZATION_TYPE_LEFT,                 /*环型极化 左半圆*/
    POLARIZATION_TYPE_RIGHT,               /*环型极化 右半圆*/ 
    POLARIZATION_TYPE_RESERVE = 0xFF   
}POLARIZATION_TYPE_e;

typedef enum {
    TUNER_STATE_IDLE,       
    TUNER_STATE_LOST,
    TUNER_STATE_LOCKED,
    TUNER_STATE_SCANNING,
    TUNER_STATE_RESERVE= 0xFF
}TUNER_STATE_e;

typedef struct 
{
    SKYDVB_U32  frequency;    /*频率(单位 KHz)*/
    SKYDVB_U32  symbol_rate;  /*符号率(单位 Ksymbol/s)*/
    SKYDVB_U32  modulation;   /*有线信号调制方式(CableTnr_Modulation_e) */   
    SKYDVB_U32  fec_outer;    /*向前纠错外码*/
    SKYDVB_U32  fec_inner;    /*向前纠错内码*/
}Cable_Param_t;

typedef struct  
{
    SKYDVB_U32  frequency;         /*频率(单位 KHz)*/
    SKYDVB_U32  symbol_rate;       /*符号率(单位 Ksymbol/s)*/
    SKYDVB_U32  modulation;        /*卫星信号调制方式(MODULATION_TYPE_e)*/
    SKYDVB_U16  orbital_position;  /*轨道位置*/
    bool   west_east_flag;    /*东西标志*/
    SKYDVB_U32  polarization;      /*极化方式(POLARIZATION_TYPE_e) */
    SKYDVB_U32  fec_inner;          /*向前纠错内码*/
}Satelite_Param_t;

typedef struct 
{
    Tuner_Type_e    type;       /*高频头类型*/      
    union{
        Cable_Param_t    cab;  /*有线高频头参数*/
        Satelite_Param_t   sat;  /*卫星高频头参数*/
    }param;                                 
}Tuner_Param_t;

typedef struct 
{
    SKYDVB_U32    total_counter;  /*单位时间内元码计数总数*/
    SKYDVB_U32    error_counter;  /*单位时间内出现的误码数*/
}Siganl_ber_t;

typedef struct {
    SKYDVB_U32    level; /*信号电平值*/
    SKYDVB_U32    strength;  /*信号强度*/
    SKYDVB_U32    snr;       /*信号信噪比/信号质量*/
    Siganl_ber_t   ber;       /*信号误码率*/
}Signal_Info_t;

SKYDVB_S32 skydvb_tuner_init(SKYDVB_VOID);
SKYDVB_S32 skydvb_tuner_exit(SKYDVB_VOID);
SKYDVB_S32 skydvb_tuner_lock(SKYDVB_S32 index, Tuner_Param_t* param);   
SKYDVB_S32 skydvb_tuner_getSignal(SKYDVB_S32 index, Signal_Info_t* signal);
SKYDVB_S32 skydvb_tuner_getState(SKYDVB_S32 index, TUNER_STATE_e* state);

//demux porting

#define SKYDVB_FILTER_DEEP (16)
typedef SKYDVB_S32 (*skydvb_filter_cb)(SKYDVB_U32 u32FilterId,  SKYDVB_U8* pU8Buffer, SKYDVB_U32 u32BufferLen);	

/* demux channel types */
typedef enum
{
    DEMUX_CHANNEL_SEC,
    DEMUX_CHANNEL_PES,
    DEMUX_CHANNEL_TS,
    DEMUX_CHANNEL_RESERVE = 0xFF
} DEMUX_CHANNEL_e;

typedef struct 
{
    SKYDVB_U8 u8FilterDeep;	
    SKYDVB_U8 u8ArrFilterMacth[SKYDVB_FILTER_DEEP];
    SKYDVB_U8 u8ArrFilterMask[SKYDVB_FILTER_DEEP];	/*Mask*/
    SKYDVB_U8 u8ArrFilterNeg[SKYDVB_FILTER_DEEP];	/*Negate*/
    skydvb_filter_cb filterCB;	
}FILTER_PARA_S;

SKYDVB_S32 skydvb_demux_init(SKYDVB_VOID);
SKYDVB_S32 skydvb_demux_exit(SKYDVB_VOID);

SKYDVB_U32 skydvb_demux_createChannel(SKYDVB_S32 index, DEMUX_CHANNEL_e type, SKYDVB_S32 poolsize);
SKYDVB_S32 skydvb_demux_closeChannel(SKYDVB_U32 hchannel);
SKYDVB_S32 skydvb_demux_setChannelPid(SKYDVB_U32 hchannel, SKYDVB_U16 pid);
SKYDVB_S32 skydvb_demux_startChannel(SKYDVB_U32 hchannel);
SKYDVB_S32 skydvb_demux_stopChannel(SKYDVB_U32 hchannel);

SKYDVB_U32 skydvb_demux_createFilter(SKYDVB_U32 hchannel); //filter attach to channel
SKYDVB_S32 skydvb_demux_closeFilter(SKYDVB_U32 hfilter);
SKYDVB_S32 skydvb_demux_setFilter(SKYDVB_U32 hfilter,FILTER_PARA_S* para);
SKYDVB_S32 skydvb_demux_enableFilter(SKYDVB_U32 hfilter);
SKYDVB_S32 skydvb_demux_disableFilter(SKYDVB_U32 hfilter);

//smartcard porting
typedef enum 
{
	SMARTCARD_EVENT_IN = 0,		/*卡插入事件*/
	SMARTCARD_EVENT_OUT,		/*卡拔出事件*/
	SMARTCARD_EVENT_INVALID	/*无效*/
} SMARTCARD_EVENT_E;

typedef	SKYDVB_VOID (*skydvb_smcartcard_cb)(SKYDVB_S32 cardSlot, SMARTCARD_EVENT_E eSmcEvent);
 
SKYDVB_S32 skydvb_smartcard_init(SKYDVB_VOID);
SKYDVB_S32 skydvb_smartcard_exit(SKYDVB_VOID);

SKYDVB_S32 skydvb_smartcard_open(SKYDVB_S32 cardslot, skydvb_smcartcard_cb pFnSmcCB);
SKYDVB_S32 skydvb_smartcard_close(SKYDVB_S32 cardslot);
SKYDVB_S32 skydvb_smartcard_reset(SKYDVB_S32 cardslot, SKYDVB_U8* pU8AtrData, SKYDVB_U8* pU16AtrLen);
SKYDVB_S32 skydvb_smartcard_transfer(SKYDVB_S32 cardSlot,SKYDVB_U8* pU8SendData, SKYDVB_U16* pU16SendLen, SKYDVB_U8* pU8RecvData, SKYDVB_U16* pU16RecvLen);

//descrambler porting
typedef enum
{
	DESCRAMBLER_TYPE_PES,
	DESCRAMBLER_TYPE_TS,
	DESCRAMBLER_TYPE_INVALID
} DESCRAMBLER_MODE_e;

SKYDVB_U32 skydvb_descrambler_allocate(DESCRAMBLER_MODE_e encryptmode);
SKYDVB_U32 skydvb_descrambler_free(SKYDVB_U32 handle);
SKYDVB_S32 skydvb_descrambler_setpid(SKYDVB_U32 handle, SKYDVB_U16 pid);
SKYDVB_S32 skydvb_descrambler_set_oddkey(SKYDVB_U32 handle, SKYDVB_U8 *key, SKYDVB_U8 len);
SKYDVB_S32 skydvb_descrambler_set_evenkey(SKYDVB_U32 handle, SKYDVB_U8 *key,SKYDVB_U8 len);
#endif
