#pragma once

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#endif
#define ALIGN( type ) __attribute__((aligned( __alignof__( type ) )))
#define PACK( type )  __attribute__((aligned( __alignof__( type ) ), packed ))
#define PACK8  __attribute__((aligned( __alignof__( uint8_t ) ), packed ))
#ifndef MIN
#define MIN(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
#endif
#ifndef MIN
#define MAX(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#endif

#define WIFI_DEVMAC_LEN (6)
#define WIFI_DEVNAME_LEN (32)
#define WIFI_DEVIPADDR_LEN (16)

typedef struct ipc_t {
    QueueHandle_t toClientQ;
    QueueHandle_t toDisplayQ;
    QueueHandle_t toMqttQ;
    struct dev {
        char ipAddr[WIFI_DEVIPADDR_LEN];
        char name[WIFI_DEVNAME_LEN];
        struct connectCnt {
            uint wifi;
            uint mqtt;
        } connectCnt;
    } dev;

} ipc_t;

typedef enum toMqttMsgType_t {
    TO_MQTT_MSGTYPE_RESTART,
    TO_MQTT_MSGTYPE_PUSH,
    TO_MQTT_MSGTYPE_WHO,
    TO_MQTT_MSGTYPE_DBG,
} toMqttMsgType_t;

typedef struct toMqttMsg_t {
    toMqttMsgType_t dataType;
    char * data;  // must be freed by recipient
} toMqttMsg_t;

typedef enum fromMqttMsgType_t {
    FROM_MQTT_MSGTYPE_CTRL
} fromMqttMsgType_t;

typedef struct fromMqttMsg_t {
    fromMqttMsgType_t dataType;
    char * data;  // must be freed by recipient
} fromMqttMsg_t;

typedef enum toDisplayMsgType_t {
    TO_DISPLAY_MSGTYPE_JSON,
} toDisplayMsgType_t;

typedef struct toDisplayMsg_t {
    toDisplayMsgType_t dataType;
    char * data;  // must be freed by recipient
} toDisplayMsg_t;

typedef enum toClientMsgType_t {
    TO_CLIENT_MSGTYPE_TRIGGER,
} toClientMsgType_t;

typedef struct toClientMsg_t {
    toClientMsgType_t dataType;
    char * data;  // must be freed by recipient
} toClientMsg_t;

void sendToClient(toClientMsgType_t const dataType, char const * const data, ipc_t const * const ipc);
void sendToDisplay(toDisplayMsgType_t const dataType, char const * const data, ipc_t const * const ipc);
void sendToMqtt(toMqttMsgType_t const dataType, char const * const data, ipc_t const * const ipc);
