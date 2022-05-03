/*
 * djl_mqtt.c
 *
 *  Created on: 2020��9��25��
 *      Author: admin
 */

#include <iot/mqtt/djl_mqtt.h>
#include "util_printf.h"
#include "util_other.h"
#include "util_delay.h"
#include "uart_driver.h"
#include "djl_config.h"
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#define URC_RECV "+QMTRECV:"
#define URC_LOST "+QMTSTAT:"
static const char       Host[]        = djl_MQTT_HOST;
static const char       DevName[]     = djl_MQTT_DEV_NAME;
static const char       DevId[]       = djl_MQTT_DEV_ID;
static const int        PortNum       = 1883;
static const u8         KeepAliveTime = 20;
static djl_mqtt_config_t _config       = {
    .dev_id = DevId, .dev_name = DevName, .host = Host, .qos = QOS0_MOST_ONECE};

const djl_mqtt_config_t* djl_mqtt_config_get() { return &_config; }
void                    djl_mqtt_config_set_qos(djl_qos_t qos) { _config.qos = qos; }

void djl_mqtt_config_set_host(const char* host) { _config.host = host; }
void djl_mqtt_config_set_dev_name(const char* dev_name) { _config.dev_name = dev_name; }
void djl_mqtt_config_set_dev_id(const char* dev_id) { _config.dev_id = dev_id; }

static Semaphore_Struct _respSemS;
static Semaphore_Handle _respSem = NULL;
static bool             _resp_sem_init() {
    Semaphore_Params semParams;
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    Semaphore_construct(&_respSemS, 0, &semParams);
    _respSem = Semaphore_handle(&_respSemS);
    return true;
}
static void _resp_func(void) {
    if (NULL != _respSem) {
        Semaphore_post(_respSem);
    }
}

static djl_at_response_t _at_resp = {.buf      = {'\0'},
                                    .key      = {'\0'},
                                    .buf_size = djl_AT_RESPONSE_BUF_SIZE,
                                    .buf_len  = 0,
                                    .end_sign = '\0',
                                    .is_set   = false,
                                    .func     = _resp_func,
                                    .status   = AT_RESP_NULL,
                                    .line_cnt = 0,
                                    .line_num = 0};

static void djl_at_response_init() {
    _at_resp.buf[0]   = '\0';
    _at_resp.key[0]   = '\0';
    _at_resp.buf_len  = 0;
    _at_resp.is_set   = false;
    _at_resp.end_sign = '\0';
    _at_resp.status   = AT_RESP_NULL;
    _at_resp.line_cnt = 0;
    _at_resp.line_num = 0;
}

djl_at_response_t* djl_at_response_get() { return &_at_resp; }

static void djl_at_response_set_line(u8 line_num) {
    djl_at_response_init();
    _at_resp.is_set   = true;
    _at_resp.line_num = line_num;
}

static s8 djl_at_response_check_key(const char* __restrict _format, ...) {
    _at_resp.key[0] = '\0';
    va_list args;
    va_start(args, _format);
    vsnprintf(_at_resp.key, djl_AT_RESPONSE_KEY_SIZE, _format, args);
    va_end(args);
    if (strstr(_at_resp.buf, _at_resp.key)) {
        return djl_OK;
    }
    return djl_ERR;
}

static void djl_at_response_set_end_sign(const char end_sign) {
    _at_resp.is_set   = true;
    _at_resp.end_sign = end_sign;
}
static djl_at_urc_t _urc_table[Urc_CNT] = {
    {URC_RECV, "\r\n", NULL},
    {URC_LOST, "\r\n", NULL},
};

#define _IS_USE_STATIS
#ifdef _IS_USE_STATIS  //ͳ����Ϣ
static djl_at_staist_t _statis = {.at_rsp_err_cnt  = 0,
                                 .connect_ok_cnt  = 0,
                                 .connect_cnt     = 0,
                                 .at_timeout_cnt  = 0,
                                 .pub_err_cnt     = 0,
                                 .reset_cnt       = 0,
                                 .lost_urc_cnt    = 0,
                                 .recv_urc_cnt    = 0,
                                 .sub_err_cnt     = 0,
                                 .open_err_cnt    = 0,
                                 .connect_err_cnt = 0};

#endif
const djl_at_staist_t* djl_mqtt_statis_get() {
#ifdef _IS_USE_STATIS  //ͳ����Ϣ
    return &_statis;
#else
    return NULL;
#endif
}
#define _IS_USE_STATUS
#ifdef _IS_USE_STATUS
static djl_at_status_t _status = {.mqtt_sts = MQTT_STS_FREE, .is_building = false};
#endif
const djl_at_status_t* djl_mqtt_status_get() {
#ifdef _IS_USE_STATUS
    return &_status;
#else
    return NULL
#endif
}

bool djl_mqtt_status_free() {
#ifdef _IS_USE_STATUS
    return _status.mqtt_sts == MQTT_STS_FREE;
#else
    return true;
#endif
}
bool djl_mqtt_status_busy() {
#ifdef _IS_USE_STATUS
    return _status.mqtt_sts != MQTT_STS_FREE;
#else
    return false;
#endif
}
bool djl_mqtt_status_building() {
#ifdef _IS_USE_STATUS
    return _status.is_building;
#else
    return false;
#endif
}
static void djl_at_urc_table_set(djlCallBack_char_t funcRecv, djlCallBack_char_t funcStat) {
    _urc_table[Urc_RECV].func = funcRecv;
    _urc_table[Urc_STAT].func = funcStat;
}

const djl_at_urc_t* djl_at_check_urc(const djl_uart_rx_t* uart_buf) {
    u8                 i = 0, prefix_len = 0, suffix_len = 0;
    u8                 bufsz  = 0;
    const char*        buffer = NULL;
    const djl_at_urc_t* urc    = NULL;
    buffer                    = uart_buf->buf;
    bufsz                     = uart_buf->len;
    for (i = 0; i < Urc_CNT; i++) {
        urc        = &_urc_table[i];
        prefix_len = strlen(urc->cmd_prefix);
        suffix_len = strlen(urc->cmd_suffix);
        if (bufsz < prefix_len + suffix_len) {
            continue;
        }
        if ((prefix_len ? !strncmp(buffer, urc->cmd_prefix, prefix_len) : 1) &&
            (suffix_len ? !strncmp(buffer + bufsz - suffix_len, urc->cmd_suffix, suffix_len) : 1)) {
#ifdef _IS_USE_STATIS
            if (strstr(urc->cmd_prefix, URC_LOST)) {
                _statis.lost_urc_cnt++;
            } else if (strstr(urc->cmd_prefix, URC_RECV)) {
                _statis.recv_urc_cnt++;
            }
#endif
            return urc;
        }
    }
    return NULL;
}

static u32 _djl_ms_to_tick(u32 ms) { return (ms * 1000 / Clock_tickPeriod); }

#define _djl_AT_PAYLOAD_SIZE 96
static char _payload[_djl_AT_PAYLOAD_SIZE] = {'\0'};  //�ַ�������,�������ݷ��ͺͲ���������
static at_resp_status_t djl_at_exec_cmd(u32 timeout, const char* __restrict _format, ...) {
    va_list args;
    va_start(args, _format);
    char* cmd = uart_vsprintf(_format, args);
    va_end(args);
    if (djl_OK != uart_write_str(cmd)) {
        return AT_RESP_ERROR;
    }
    if (NULL != _respSem) {
        bool isok = Semaphore_pend(_respSem, _djl_ms_to_tick(timeout));
        if (isok) {
#ifdef _IS_USE_STATIS
            if (AT_RESP_OK != _at_resp.status) {
                _statis.at_rsp_err_cnt++;
            }
#endif
            return _at_resp.status;
        } else {
#ifdef _IS_USE_STATIS
            _statis.at_timeout_cnt++;
#endif
            return AT_RESP_TIMEOUT;
        }
    } else {
        return AT_RESP_ERROR;
    }
}

static int at_resp_parse_line_by_key(const char* key, const char* resp_expr, ...) {
    va_list args;
    int     resp_args_num = 0;
    char*   pStr          = NULL;
    if (pStr = strstr(_at_resp.buf, key)) {
        va_start(args, resp_expr);
        resp_args_num = vsscanf(pStr, resp_expr, args);
        va_end(args);
    }
    return resp_args_num;
}

int djl_mqtt_get_csq() {
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_QUERY_CSQ;
#endif
    int csq = -1;
    djl_at_response_set_line(djl_MQTT_OK_LINE_NUM);
    at_resp_status_t rsp_sts = djl_at_exec_cmd(AT_DEFAULT_TIMEOUT, "AT+CSQ\r\n");
    if (AT_RESP_OK == rsp_sts) {
        if (at_resp_parse_line_by_key("+CSQ:", "+CSQ:%d", &csq) <= 0) {
            return -1;
        }
    }
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_FREE;
#endif
    return csq;
}

int djl_mqtt_set_ate(djl_at_ate_t ate) {
    djl_at_response_set_line(djl_MQTT_OK_LINE_NUM);
    at_resp_status_t rsp_sts = djl_at_exec_cmd(AT_DEFAULT_TIMEOUT, "ATE%d\r\n", (int)ate);
    return rsp_sts;
}
static int mqtt_check_net(u8 cnt) {
    u8 i = 0;
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_CHECK_NET;
#endif
    djl_at_response_set_line(djl_MQTT_OK_LINE_NUM);
    at_resp_status_t rsp_sts = djl_at_exec_cmd(AT_SHORT_TIMEOUT, "AT+CEREG?\r\n");
    if (AT_RESP_OK == rsp_sts) {
        if (djl_OK != djl_at_response_check_key("+CEREG:0,1")) {
            rsp_sts = AT_RESP_ERROR;
        }
    }
    while (i++ < cnt && AT_RESP_OK != rsp_sts) {
        djl_delay_ms(1000);
        djl_at_response_set_line(djl_MQTT_OK_LINE_NUM);
        rsp_sts = djl_at_exec_cmd(AT_SHORT_TIMEOUT, "AT+CEREG?\r\n");
        if (AT_RESP_OK == rsp_sts) {
            if (djl_OK != djl_at_response_check_key("+CEREG:0,1")) {
                rsp_sts = AT_RESP_ERROR;
            }
        }
    }
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_FREE;
#endif
#ifdef _IS_USE_STATIS
    if (AT_RESP_OK != rsp_sts) _statis.net_err_cnt++;
#endif
    return rsp_sts;
}

static int djl_mqtt_set_alive(u32 alive_time) {
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_CONFIG;
#endif
    djl_at_response_set_line(djl_MQTT_OK_LINE_NUM);
    at_resp_status_t rsp_sts = djl_at_exec_cmd(
        AT_DEFAULT_TIMEOUT, "" AT_MQTT_ALIVE "" AT_END_CR_LF "", ConnectID_0, alive_time);
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_FREE;
#endif
    return rsp_sts;
}

static int djl_mqtt_reboot() {
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_REBOOT;
#endif
    djl_at_response_set_line(djl_MQTT_OK_LINE_NUM);
    at_resp_status_t rsp_sts = djl_at_exec_cmd(AT_DEFAULT_TIMEOUT, "" AT_REBOOT "" AT_END_CR_LF "");
#ifdef _IS_USE_STATIS
    _statis.reset_cnt++;
#endif
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_FREE;
#endif
    return rsp_sts;
}

static int djl_mqtt_set_will(djl_qos_t qos, u8 retain, const char* msg, const char* topic) {
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_CONFIG;
#endif
    djl_at_response_set_line(djl_MQTT_OK_LINE_NUM);
    at_resp_status_t rsp_sts =
        djl_at_exec_cmd(AT_DEFAULT_TIMEOUT, "" AT_MQTT_WILL "" AT_END_CR_LF "", ConnectID_0, qos,
                       retain, topic, msg);
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_FREE;
#endif
    return rsp_sts;
}

#define _MQTT_CONFIG_STR_SIZE 32
static char _config_str[_MQTT_CONFIG_STR_SIZE] = {'\0'};
static int  djl_mqtt_set_will_f(djl_qos_t qos, u8 retain, const char* msg,
                               const char* __restrict _format, ...) {
    int res        = 0;
    _config_str[0] = '\0';
    va_list args;
    va_start(args, _format);
    vsnprintf(_config_str, _MQTT_CONFIG_STR_SIZE, _format, args);
    va_end(args);
    res            = djl_mqtt_set_will(qos, retain, msg, _config_str);
    _config_str[0] = '\0';
    return res;
}

static int djl_mqtt_close() {
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_CLOSE;
#endif
    djl_at_response_set_line(djl_MQTT_SUCC_LINE_NUM);
    at_resp_status_t rsp_sts =
        djl_at_exec_cmd(AT_DEFAULT_TIMEOUT, "AT+QMTCLOSE=%d\r\n", ConnectID_0);
    if (AT_RESP_OK == rsp_sts) {
        if (djl_OK !=
            djl_at_response_check_key(AT_MQTT_CLOSE_SUCC, ConnectID_0, AT_MQTT_RES_CODE_OK)) {
            rsp_sts = AT_RESP_ERROR;
        }
    }
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_FREE;
#endif
    return rsp_sts;
}

static int djl_mqtt_open(const char* hostName, u32 port) {
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_OPEN;
#endif
    djl_at_response_set_line(djl_MQTT_SUCC_LINE_NUM);
    at_resp_status_t rsp_sts = djl_at_exec_cmd(AT_DEFAULT_TIMEOUT, "AT+QMTOPEN=%d,\"%s\",%d\r\n",
                                              ConnectID_0, hostName, port);

    if (AT_RESP_OK == rsp_sts) {
        if (djl_OK != djl_at_response_check_key(AT_MQTT_OPEN_SUCC, ConnectID_0, AT_MQTT_RES_OK)) {
            rsp_sts = AT_RESP_ERROR;
        }
    }

#ifdef _IS_USE_STATIS
    if (AT_RESP_OK != rsp_sts) _statis.open_err_cnt++;
#endif
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_FREE;
#endif
    return rsp_sts;
}

static int djl_mqtt_connect(const char* clientID) {
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_CONNECT;
#endif
    djl_at_response_set_line(djl_MQTT_SUCC_LINE_NUM);
    at_resp_status_t rsp_sts = djl_at_exec_cmd(
        AT_LONG_TIMEOUT, "" AT_MQTT_CONNECT "" AT_END_CR_LF "", ConnectID_0, clientID);

    if (AT_RESP_OK == rsp_sts) {
        if (djl_OK != djl_at_response_check_key(AT_MQTT_CONNECT_SUCC, ConnectID_0, AT_MQTT_RES_OK,
                                              AT_MQTT_RES_CODE_OK)) {
            rsp_sts = AT_RESP_ERROR;
        }
    }
#ifdef _IS_USE_STATIS
    _statis.connect_cnt++;
    if (AT_RESP_OK != rsp_sts) _statis.connect_err_cnt++;
#endif
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_FREE;
#endif
    return rsp_sts;
}

static int djl_mqtt_connect_f(const char* __restrict _format, ...) {
    int res        = 0;
    _config_str[0] = '\0';
    va_list args;
    va_start(args, _format);
    vsnprintf(_config_str, _MQTT_CONFIG_STR_SIZE, _format, args);
    va_end(args);
    res            = djl_mqtt_connect(_config_str);
    _config_str[0] = '\0';
    return res;
}
static int djl_mqtt_sub(djl_qos_t qos, const char* topic) {
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_SUB;
#endif
    djl_at_response_set_line(djl_MQTT_SUCC_LINE_NUM);
    at_resp_status_t rsp_sts = djl_at_exec_cmd(AT_DEFAULT_TIMEOUT, "" AT_MQTT_SUB "" AT_END_CR_LF "",
                                              ConnectID_0, MsgID_1, topic, qos);

    if (AT_RESP_OK == rsp_sts) {
        if (djl_OK !=
            djl_at_response_check_key(AT_MQTT_SUB_SUCC, ConnectID_0, MsgID_1, AT_MQTT_RES_OK, qos)) {
            rsp_sts = AT_RESP_ERROR;
        }
    }

#ifdef _IS_USE_STATIS
    if (AT_RESP_OK != rsp_sts) _statis.sub_err_cnt++;
#endif
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_FREE;
#endif
    return rsp_sts;
}

static int djl_mqtt_sub_f(djl_qos_t qos, const char* __restrict _format, ...) {
    int res        = 0;
    _config_str[0] = '\0';
    va_list args;
    va_start(args, _format);
    vsnprintf(_config_str, _MQTT_CONFIG_STR_SIZE, _format, args);
    va_end(args);
    res            = djl_mqtt_sub(qos, _config_str);
    _config_str[0] = '\0';
    return res;
}

static int djl_mqtt_waiting_at() {
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_CHECK_AT;
#endif
    at_resp_status_t rsp_sts = AT_RESP_ERROR;
    djl_at_response_set_line(djl_MQTT_OK_LINE_NUM);
    rsp_sts = djl_at_exec_cmd(AT_DEFAULT_TIMEOUT, "AT\r\n");
    while (AT_RESP_OK != rsp_sts) {
        djl_delay_ms(1000);
        djl_at_response_set_line(djl_MQTT_OK_LINE_NUM);
        rsp_sts = djl_at_exec_cmd(AT_DEFAULT_TIMEOUT, "AT\r\n");
    }
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_FREE;
#endif
    return rsp_sts;
}

static int djl_mqtt_build_net() {
    int res = djl_ERR;
    res     = djl_mqtt_waiting_at();
    res     = djl_mqtt_set_alive(KeepAliveTime);
    res = djl_mqtt_set_will_f(QOS1_LEASET_ONCE, NO_RETAIN, "{\"CON\":\"0\"}", "%s/%s/nty", DevName,
                             DevId);

    res = mqtt_check_net(100);
    if (djl_OK != res) {
        return djl_ERR;
    }

    res = djl_mqtt_close();

    res = djl_mqtt_open(Host, PortNum);
    if (djl_OK != res) {
        return djl_ERR;
    }
    res = djl_mqtt_connect_f("%s_%s", DevName, DevId);
    if (djl_OK != res) {
        return djl_ERR;
    }
    res = djl_mqtt_sub_f(QOS1_LEASET_ONCE, "%s/%s/cmd", DevName, DevId);
    if (djl_OK != res) {
        return djl_ERR;
    }
    return djl_OK;
}

static int djl_mqtt_rebuild_net() {
    int res = djl_ERR;
    res     = mqtt_check_net(100);
    if (djl_OK != res) {
        return djl_ERR;
    }
    res = djl_mqtt_close();
    res = djl_mqtt_open(Host, PortNum);
    if (djl_OK != res) {
        return djl_ERR;
    }
    res = djl_mqtt_connect_f("%s_%s", DevName, DevId);
    if (djl_OK != res) {
        return djl_ERR;
    }
    res = djl_mqtt_sub_f(QOS1_LEASET_ONCE, "%s/%s/cmd", DevName, DevId);
    if (djl_OK != res) {
        return djl_ERR;
    }
    return djl_OK;
}

static int djl_mqtt_connecting() {
#ifdef _IS_USE_STATUS
    _status.is_building = true;
#endif
    while (djl_OK != djl_mqtt_connect_cnt(10)) {
        djl_delay_ms(1000);
    }
#ifdef _IS_USE_STATUS
    _status.is_building = false;
#endif
#ifdef _IS_USE_STATIS
    _statis.connect_ok_cnt++;
#endif

    return 0;
}

static s8 djl_mqtt_pub(djl_qos_t qos, u8 retain, const char* topic, const char* msg) {
#ifdef _IS_USE_STATUS
    _status.mqtt_sts = MQTT_STS_PUB;
#endif
    int msg_len = strlen(msg);
    djl_at_response_set_end_sign('>');
    int msgId = MsgID_1;
    if (QOS0_MOST_ONECE == qos) {
        msgId = MsgID_0;
    }

#ifndef IS_USE_djl_DOOR
    at_resp_status_t rsp_sts =
        djl_at_exec_cmd(AT_DEFAULT_TIMEOUT, "AT+QMTPUB=%d,%d,%d,%d,\"%s/%s/%s\",%d\r\n", ConnectID_0,
                       msgId, (int)qos, (int)retain, DevName, DevId, topic, msg_len);
#else
    at_resp_status_t rsp_sts =
        djl_at_exec_cmd(AT_DEFAULT_TIMEOUT, "AT+QMTPUB=%d,%d,%d,%d,\"%s\",%d\r\n", ConnectID_0,
                       msgId, (int)qos, (int)retain, topic, msg_len);
#endif
    if (AT_RESP_OK == rsp_sts) {
        djl_at_response_set_line(djl_MQTT_SUCC_LINE_NUM);
        rsp_sts = djl_at_exec_cmd(AT_DEFAULT_TIMEOUT, msg);
        if (AT_RESP_OK == rsp_sts) {
            if (djl_OK !=
                djl_at_response_check_key(AT_MQTT_PUB_SUCC, ConnectID_0, msgId, AT_MQTT_RES_OK)) {
                rsp_sts = AT_RESP_ERROR;
            }
        }

        if (AT_RESP_OK == rsp_sts) {
            return djl_OK;
        } else {
#ifdef _IS_USE_STATIS
            _statis.pub_err_cnt++;
#endif
            return djl_ERR;
        }
    } else {
#ifdef _IS_USE_STATIS
        _statis.pub_err_cnt++;
#endif
#ifdef _IS_USE_STATUS
        _status.mqtt_sts = MQTT_STS_FREE;
#endif
        return djl_ERR;
    }
}
#define _MQTT_PUB_TRY_CNT 3
static s8 djl_mqtt_pubing(djl_qos_t qos, u8 retain, const char* topic, const char* msg) {
    s8 isOK = djl_mqtt_pub(qos, retain, topic, msg);
    while (djl_OK != isOK) {
        djl_delay_ms(1000);
        isOK = djl_mqtt_pub(qos, retain, topic, msg);
        if (djl_OK != isOK) {
            djl_mqtt_connecting();
            isOK = djl_mqtt_pub(qos, retain, topic, msg);
        }
    }
    return isOK;
}

int djl_mqtt_pub_f(djl_qos_t qos, u8 retain, const char* topic, const char* __restrict _format, ...) {
    int res     = 0;
    _payload[0] = '\0';
    va_list args;
    va_start(args, _format);
    vsnprintf(_payload, _djl_AT_PAYLOAD_SIZE, _format, args);
    va_end(args);
    res         = djl_mqtt_pubing(qos, retain, topic, _payload);
    _payload[0] = '\0';
    return res;
}

int djl_mqtt_pub_add_f(const char* __restrict _format, ...) {
    int     res  = 0;
    int     len  = strlen(_payload);
    int     size = _djl_AT_PAYLOAD_SIZE;
    va_list args;
    va_start(args, _format);
    int cnt = vsnprintf(&_payload[len], size - len, _format, args);
    va_end(args);
    if ((cnt + len) > size) {
        res = -1;
    }
    return res;
}

int djl_mqtt_pub_flush(djl_qos_t qos, u8 retain, const char* topic) {
    int res = 0;
    int len = strlen(_payload);
    if (len < 1) {
        return -2;
    }
    res         = djl_mqtt_pub(qos, retain, topic, _payload);
    _payload[0] = '\0';
    return res;
}

int djl_mqtt_pub_status() {
#ifdef _IS_USE_STATIS
    return djl_mqtt_pub_f(
        QOS0_MOST_ONECE, NO_RETAIN, "sts",
        "{CON:%d,RST:%d,CNC:%d,PBE:%d,ARE:%d,ATC:%d,RVC:%d,LOC:%d,SBE:%d,OPE:%d,CNE:%d,"
        "NTE:%d,CSQ:%d}",
        _statis.connect_ok_cnt, _statis.reset_cnt, _statis.connect_cnt, _statis.pub_err_cnt,
        _statis.at_rsp_err_cnt, _statis.at_timeout_cnt, _statis.recv_urc_cnt, _statis.lost_urc_cnt,
        _statis.sub_err_cnt, _statis.open_err_cnt, _statis.connect_err_cnt, _statis.net_err_cnt,
        djl_mqtt_get_csq());
#else
    return djl_mqtt_pub_f(QOS0_MOST_ONECE, NO_RETAIN, "sts", "{CSQ:%d}", djl_mqtt_get_csq());
#endif
}

int djl_mqtt_net_building() {
    djl_mqtt_connecting();
#ifdef _IS_USE_STATIS
    return djl_mqtt_pub_f(QOS1_LEASET_ONCE, NO_RETAIN, "nty",
                         "{\"CON\":\"%d\",\"RST\":\"%d\",\"CSQ\":\"%d\"}", _statis.connect_ok_cnt,
                         _statis.reset_cnt, djl_mqtt_get_csq());
#else
    return djl_mqtt_pub_f(QOS1_LEASET_ONCE, NO_RETAIN, "nty", "{\"CON\":\"%d\"}", 1);
#endif
}
int djl_mqtt_init(djlCallBack_char_t funcRecv, djlCallBack_char_t funcStat) {
    _resp_sem_init();
    djl_at_urc_table_set(funcRecv, funcStat);
    return 0;
}

