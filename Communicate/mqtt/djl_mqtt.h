/*
 * djl_mqtt.h
 *
 *  Created on: 2020��9��25��
 *      Author: admin
 */

#ifndef STACK_djl_MQTT_H_
#define STACK_djl_MQTT_H_
#include <iot/mqtt/djl_mqtt_def.h>
#include "djl_config.h"
#include "util_def.h"
#include <string.h>
#include <stdio.h>

const djl_at_urc_t* djl_at_check_urc(const djl_uart_rx_t* uart_buf);
djl_at_response_t*  djl_at_response_get();

int djl_mqtt_pub_add_f(const char* __restrict _format, ...);
int djl_mqtt_pub_flush(djl_qos_t qos, u8 retain, const char* topic);
int djl_mqtt_pub_f(djl_qos_t qos, u8 retain, const char* topic, const char* __restrict _format, ...);
int djl_mqtt_init(djlCallBack_char_t funcRecv, djlCallBack_char_t funcStat);
int djl_mqtt_pub_status();
int djl_mqtt_net_building();
int djl_mqtt_get_csq();
int djl_mqtt_set_ate(djl_at_ate_t ate);
const djl_at_staist_t*   djl_mqtt_statis_get();
const djl_at_status_t*   djl_mqtt_status_get();
bool                    djl_mqtt_status_free();
bool                    djl_mqtt_status_busy();
bool                    djl_mqtt_status_building();
const djl_mqtt_config_t* djl_mqtt_config_get();
void                    djl_mqtt_config_set_qos(djl_qos_t qos);
int                     djl_mqtt_reset(u8 is_connect);
void                    djl_mqtt_config_set_host(const char* host);
void                    djl_mqtt_config_set_dev_name(const char* dev_name);
void                    djl_mqtt_config_set_dev_id(const char* dev_id);
#endif /* STACK_djl_MQTT_H_ */
