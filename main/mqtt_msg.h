#pragma once

typedef enum toMqttMsgType_t {
    TO_MQTT_MSGTYPE_CTRL,
    TO_MQTT_MSGTYPE_DATA,
    TO_MQTT_MSGTYPE_DEVIPADDR,
    TO_MQTT_MSGTYPE_DEVNAME,
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

#define WIFI_DEVMAC_LEN (6)
#define WIFI_DEVNAME_LEN (32)
#define WIFI_DEVIPADDR_LEN (16)