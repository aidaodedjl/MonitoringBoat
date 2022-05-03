/*
 * djl_easylink.h
 *
 *  Created on: 2020��9��23��
 *      Author: admin
 */

#ifndef EASYLINK_djl_EASYLINK_H_
#define EASYLINK_djl_EASYLINK_H_
#include <iot/easylink/djl_easylink_def.h>
#include <iot/easylink/EasyLink.h>
#include "djl_config.h"
#include "util_def.h"
#include <ti/sysbios/knl/Semaphore.h>
typedef void (*djl_easylink_rxCB_t)(EasyLink_RxPacket* rxPacket, EasyLink_Status status);
bool              djl_easylink_init(djl_easylink_rxCB_t rxCb);
bool              djl_easylink_rx();
Easylink_Status_t djl_easylink_status_get();
bool              djl_easylink_send_with_ack(u8* pData, u8 len, u8 dstAddr);
bool              djl_easylink_send_ack(Easylink_ACK_t* ack, u8 dstAddr);
bool              djl_easylink_send_notify(Easylink_Notify_t* notify, u8 dstAddr);
bool              djl_easylink_send_sensor(Easylink_Sensor_t* sensor, u8 dstAddr);
bool              djl_easylink_send_io(Easylink_IO_t* io, u8 dstAddr);
void              djl_payloadToIO(u8* payload, Easylink_IO_t* io);
int               djl_ioToPayload(Easylink_IO_t* io, u8* payload);
void              djl_payloadToSensor(u8* payload, Easylink_Sensor_t* sensor);
int               djl_sensorToPayload(Easylink_Sensor_t* sensor, u8* payload);
djl_BOOL djl_easylink_init_buf();
#endif /* EASYLINK_djl_EASYLINK_H_ */
