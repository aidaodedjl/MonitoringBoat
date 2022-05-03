/*
 * djl_easylink.c
 *
 *  Created on: 2020��9��23��
 *      Author: admin
 */

#include <iot/easylink/djl_easylink.h>
#include "djl_config.h"
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/BIOS.h>
#include <ti/drivers/dpl/HwiP.h>
#define _TX_TIMEOUT_MS (3000000 / Clock_tickPeriod)  // 3000ms
static djl_easylink_rxCB_t _easylink_rxCb   = NULL;
static Semaphore_Handle   _easylinkSem     = NULL;
static Easylink_Status_t  _easylink_status = Easylink_status_null;

Easylink_Status_t djl_easylink_status_get() { return _easylink_status; }
static void       _easylink_sem_init() {
    Semaphore_Params params;
    Semaphore_Params_init(&params);
    _easylinkSem = Semaphore_create(0, &params, NULL);
    if (_easylinkSem == NULL) {
        // System_abort("Semaphore creation failed");
    }
}

static void _easylink_txDoneCb(EasyLink_Status status) {
    //_easylink_status = Easylink_status_free;
    if (status == EasyLink_Status_Success) {
    } else if (status == EasyLink_Status_Aborted) {
    } else {
    }
    if (_easylinkSem != NULL) {
        Semaphore_post(_easylinkSem);
    }
}
#define _RX_TIMEOUT_MS (3000000 / Clock_tickPeriod)  // 3000ms

static bool _easylink_init() {
    EasyLink_Params easyLink_params;
    EasyLink_Params_init(&easyLink_params);
    if (EasyLink_init(&easyLink_params) != EasyLink_Status_Success) {
        return false;
    }
    return true;
}


static bool _easylink_send(u8* pData, u8 len, u8 dstAddr, bool isAck) {
    bool isTxRxOK = false;
    memcpy(_txPacket.payload, pData, len);
    _txPacket.len        = len;
    _txPacket.dstAddr[0] = dstAddr;
    isTxRxOK             = _easylink_tx(&_txPacket, isAck);
    return isTxRxOK;
}

bool djl_easylink_send_with_ack(u8* pData, u8 len, u8 dstAddr) {
    return _easylink_send(pData, len, dstAddr, true);
}

bool djl_easylink_init(djl_easylink_rxCB_t rxCb) {
    _easylink_rxCb = rxCb;
#ifdef IS_USE_RX
    djl_easylink_init_buf();
#endif
    _easylink_sem_init();
    bool isok = _easylink_init();
    if (isok) {
        _easylink_status = Easylink_status_init;
    }
    return isok;
}

bool djl_easylink_rx() {
    bool            isOK   = true;
    EasyLink_Status status = EasyLink_receiveAsync(_easylink_rxDoneCb, 0);
    if (EasyLink_Status_Success != status) {
        isOK = false;
    }
    _easylink_status = Easylink_status_rxing;
    return isOK;
}

bool djl_easylink_send_ack(Easylink_ACK_t* ack, u8 dstAddr) {
    u8 payload[3] = {0};
    payload[0]    = ack->ackCmdId;
    payload[1]    = ack->id;
    payload[2]    = ack->cnt;
    bool isOK     = _easylink_send(payload, sizeof(payload), dstAddr, false);
    return isOK;
}
#define _djl_EASYLINK_NOTIFY_SIZE 3
int djl_notifyToPayload(Easylink_Notify_t* notify, u8* payload) {
    payload[0] = Easylink_cmdIds_notify;
    payload[1] = notify->id;
    payload[2] = notify->cnt;
    return _djl_EASYLINK_NOTIFY_SIZE;
}

void djl_payloadToNotify(u8* payload, Easylink_Notify_t* notify) {
    notify->id  = payload[1];
    notify->cnt = payload[2];
}
bool djl_easylink_send_notify(Easylink_Notify_t* notify, u8 dstAddr) {
    u8 payload[_djl_EASYLINK_NOTIFY_SIZE] = {0};
    djl_notifyToPayload(notify, payload);
    return _easylink_send(payload, sizeof(payload), dstAddr, true);
}

#define _djl_EASYLINK_SENSOR_SIZE 6

int djl_sensorToPayload(Easylink_Sensor_t* sensor, u8* payload) {
    payload[0] = Easylink_cmdIds_sensor_data;
    payload[1] = sensor->id;
    payload[2] = sensor->val;
    payload[3] = (sensor->bat >> 8) & 0xFF;
    payload[4] = (u8)(sensor->bat & 0xFF);
    payload[5] = sensor->cnt;
    return _djl_EASYLINK_SENSOR_SIZE;
}
void djl_payloadToSensor(u8* payload, Easylink_Sensor_t* sensor) {
    sensor->id  = payload[1];
    sensor->val = payload[2];
    sensor->bat = (payload[3] << 8) | payload[4];
    sensor->cnt = payload[5];
}

bool djl_easylink_send_sensor(Easylink_Sensor_t* sensor, u8 dstAddr) {
    u8 payload[_djl_EASYLINK_SENSOR_SIZE] = {0};
    djl_sensorToPayload(sensor, payload);
    return _easylink_send(payload, sizeof(payload), dstAddr, true);
}

#define _djl_EASYLINK_STATUS_DATA_SIZE 11

int djl_statusDataToPayload(Easylink_Status_data_t* status, u8* payload) {
    payload[0] = Easylink_cmdIds_status_data;
    payload[1] = status->id;
    payload[2] = (status->bat >> 8) & 0xFF;
    payload[3] = (u8)(status->bat & 0xFF);

    payload[4] = (status->cur[0] >> 8) & 0xFF;
    payload[5] = (u8)(status->cur[0] & 0xFF);

    payload[6] = (status->cur[1] >> 8) & 0xFF;
    payload[7] = (u8)(status->cur[1] & 0xFF);

    payload[8]  = (status->cur[2] >> 8) & 0xFF;
    payload[9]  = (u8)(status->cur[2] & 0xFF);
    payload[10] = status->chs;
    return _djl_EASYLINK_STATUS_DATA_SIZE;
}

void djl_payloadToStatusData(u8* payload, Easylink_Status_data_t* status) {
    status->id     = payload[1];
    status->bat    = (payload[2] << 8) | payload[3];
    status->cur[0] = (payload[4] << 8) | payload[5];
    status->cur[1] = (payload[6] << 8) | payload[7];
    status->cur[2] = (payload[8] << 8) | payload[9];
    status->chs    = payload[10];
}

#define _djl_EASYLINK_IO_SIZE 8
int djl_ioToPayload(Easylink_IO_t* io, u8* payload) {
    payload[0] = Easylink_cmdIds_IOReq;
    payload[1] = io->id;
    payload[2] = io->chs[djl_SW_CH1];
    payload[3] = io->chs[djl_SW_CH2];
    payload[4] = io->chs[djl_SW_CH3];
    payload[5] = (io->delay >> 8) & 0xFF;
    payload[6] = (u8)(io->delay & 0xFF);
    payload[7] = io->cnt;
    return _djl_EASYLINK_IO_SIZE;
}

void djl_payloadToIO(u8* payload, Easylink_IO_t* io) {
    io->id             = payload[1];
    io->chs[djl_SW_CH1] = (E_sw_sts_t)payload[2];
    io->chs[djl_SW_CH2] = (E_sw_sts_t)payload[3];
    io->chs[djl_SW_CH3] = (E_sw_sts_t)payload[4];
    io->delay          = (payload[5] << 8) | payload[6];
    io->cnt            = payload[7];
}

bool djl_easylink_send_io(Easylink_IO_t* io, u8 dstAddr) {
    u8 payload[_djl_EASYLINK_IO_SIZE] = {0};
    djl_ioToPayload(io, payload);
    return _easylink_send(payload, sizeof(payload), dstAddr, true);
}
