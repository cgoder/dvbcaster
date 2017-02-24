#ifndef __SKYWORTH_DVB_H__
#define __SKYWORTH_DVB_H__

#include "skyworth_types.h"

/*��Ƶͷ����*/
typedef enum 
{
    TUNER_TYPE_CABLE,           /*��������    */
    TUNER_TYPE_SATELITE,  /*�����ź�����*/
    TUNER_TYPE_RESERVE = 0xFF   /*Ԥ��*/
}Tuner_Type_e;

/*���Ʒ�ʽ����*/
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

/*��ǰ������������*/
typedef enum 
{
    FEC_OUTER_NONE,             /*δ����     */
    FEC_OUTER_WITHOUT,      /*��FEC����  */
    FEC_OUTER_RS,                /*RS(204/188)*/
    FEC_OUTER_RESERVE = 0xFF     /*Ԥ��*/
}FEC_OUTER_e;

/*��ǰ������������*/
typedef enum
{
    FEC_INNER_NONE,                         /*δ����     */
    FEC_INNER_CONVOLUTION_1_2,  /*�������1/2*/
    FEC_INNER_CONVOLUTION_2_3,  /*�������2/3*/
    FEC_INNER_CONVOLUTION_3_4,  /*�������3/4*/
    FEC_INNER_CONVOLUTION_5_6,  /*�������5/6*/
    FEC_INNER_CONVOLUTION_7_8,  /*�������7/8*/
    FEC_INNER_CONVOLUTION_8_9,  /*�������8/9*/
    FEC_INNER_RESERVE = 0xFF   /*Ԥ��*/
}FEC_INNER_e;

/* type of polarization (sat) */
typedef enum {
    POLARIZATION_TYPE_HORIZONTAL,       /*����   ˮƽ����*/
    POLARIZATION_TYPE_VERTICAL,          /*����   ��ֱ����*/
    POLARIZATION_TYPE_LEFT,                 /*���ͼ��� ���Բ*/
    POLARIZATION_TYPE_RIGHT,               /*���ͼ��� �Ұ�Բ*/ 
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
    SKYDVB_U32  frequency;    /*Ƶ��(��λ KHz)*/
    SKYDVB_U32  symbol_rate;  /*������(��λ Ksymbol/s)*/
    SKYDVB_U32  modulation;   /*�����źŵ��Ʒ�ʽ(CableTnr_Modulation_e) */   
    SKYDVB_U32  fec_outer;    /*��ǰ��������*/
    SKYDVB_U32  fec_inner;    /*��ǰ��������*/
}Cable_Param_t;

typedef struct  
{
    SKYDVB_U32  frequency;         /*Ƶ��(��λ KHz)*/
    SKYDVB_U32  symbol_rate;       /*������(��λ Ksymbol/s)*/
    SKYDVB_U32  modulation;        /*�����źŵ��Ʒ�ʽ(MODULATION_TYPE_e)*/
    SKYDVB_U16  orbital_position;  /*���λ��*/
    bool   west_east_flag;    /*������־*/
    SKYDVB_U32  polarization;      /*������ʽ(POLARIZATION_TYPE_e) */
    SKYDVB_U32  fec_inner;          /*��ǰ��������*/
}Satelite_Param_t;

typedef struct 
{
    Tuner_Type_e    type;       /*��Ƶͷ����*/      
    union{
        Cable_Param_t    cab;  /*���߸�Ƶͷ����*/
        Satelite_Param_t   sat;  /*���Ǹ�Ƶͷ����*/
    }param;                                 
}Tuner_Param_t;

typedef struct 
{
    SKYDVB_U32    total_counter;  /*��λʱ����Ԫ���������*/
    SKYDVB_U32    error_counter;  /*��λʱ���ڳ��ֵ�������*/
}Siganl_ber_t;

typedef struct {
    SKYDVB_U32    level; /*�źŵ�ƽֵ*/
    SKYDVB_U32    strength;  /*�ź�ǿ��*/
    SKYDVB_U32    snr;       /*�ź������/�ź�����*/
    Siganl_ber_t   ber;       /*�ź�������*/
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
	SMARTCARD_EVENT_IN = 0,		/*�������¼�*/
	SMARTCARD_EVENT_OUT,		/*���γ��¼�*/
	SMARTCARD_EVENT_INVALID	/*��Ч*/
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
