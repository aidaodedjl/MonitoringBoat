/*
 * aw_easylink_def.h
 *
 *  Created on: 2020��9��23��
 *      Author: admin
 */

#ifndef EASYLINK_AW_EASYLINK_DEF_H_
#define EASYLINK_AW_EASYLINK_DEF_H_
#include "util_def.h"
#define EASYLINK_MAX_DATA_LENGTH 20
typedef enum {

    Easylink_cmdIds_configReq = 1, 
    Easylink_cmdIds_configRsp,      
    Easylink_cmdIds_config,         

    Easylink_cmdIds_sensor_data,     
    Easylink_cmdIds_sensor_dataRsp,  
    Easylink_cmdIds_sensor_dataReq,  
    Easylink_cmdIds_status_data,    
    Easylink_cmdIds_status_dataRsp,  
    Easylink_cmdIds_status_dataReq, 

    Easylink_cmdIds_IOReq,  
    Easylink_cmdIds_IORsp,  

    Easylink_cmdIds_notify,    
    Easylink_cmdIds_notifyRsp  
} Easylink_cmdIds_t;

typedef enum {
    Easylink_cmdId = 0,  
    Easylink_payload
} Easylink_packet_t;

typedef struct _Easylink_Sensor_t {
    uint8_t  id;
    int8_t   val;
    uint16_t bat;
    int8_t   rssi;
    uint8_t  cnt;
} Easylink_Sensor_t;

typedef struct _Easylink_Status_data_t {
    u8  id;
    u16 bat;
    u16 cur[3];  
    u8  chs;
} Easylink_Status_data_t;

typedef struct _Easylink_IO_t {
    uint8_t    id;
    E_sw_sts_t chs[3];
    uint16_t   delay;
    uint8_t    cnt;
} Easylink_IO_t;

typedef struct _Easylink_ACK_t {
    Easylink_cmdIds_t ackCmdId;
    uint8_t           id;
    uint8_t           cnt;
} Easylink_ACK_t;

typedef struct _Easylink_TX_t {
    Easylink_cmdIds_t ackCmdId;
    u8                payload[EASYLINK_MAX_DATA_LENGTH];
    u8                len;
    u8                dstAdd;
} Easylink_TX_t;

typedef enum {
    Easylink_powerOn = 0,  
    Easylink_lowpower
} Easylink_NotifyStatus_t;

typedef struct _Easylink_Notify_t {
    uint8_t                 id;
    Easylink_NotifyStatus_t sts;
    uint16_t                bat;
    uint8_t                 cnt;
} Easylink_Notify_t;
typedef enum {
    Easylink_status_rxing,
    Easylink_status_txing,
    Easylink_status_waitAcking,
    Easylink_status_waitRxAborting,
    Easylink_status_waitTxAborting,
    Easylink_status_waitAckAborting,
    Easylink_status_init,
    Easylink_status_free,
    Easylink_status_null
} Easylink_Status_t;
#endif /* EASYLINK_AW_EASYLINK_DEF_H_ */
