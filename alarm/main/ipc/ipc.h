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
    QueueHandle_t toBuzzerQ;
    struct dev {
        char ipAddr[WIFI_DEVIPADDR_LEN];
        char name[WIFI_DEVNAME_LEN];
        struct connectCnt {
            uint wifi;
        } connectCnt;
    } dev;

} ipc_t;

// to display

typedef enum toDisplayMsgType_t {
    TO_DISPLAY_MSGTYPE_JSON,
    TO_DISPLAY_MSGTYPE_STATUS
} toDisplayMsgType_t;

typedef struct toDisplayMsg_t {
    toDisplayMsgType_t dataType;
    char * data;  // must be freed by recipient
} toDisplayMsg_t;

// to client

typedef enum toClientMsgType_t {
    TO_CLIENT_MSGTYPE_TRIGGER
} toClientMsgType_t;

typedef struct toClientMsg_t {
    toClientMsgType_t dataType;
    char * data;  // must be freed by recipient
} toClientMsg_t;

// to buzzer

typedef enum toBuzzerMsgType_t {
    TO_BUZZER_MSGTYPE_START,
    TO_BUZZER_MSGTYPE_STOP
} toBuzzerMsgType_t;

typedef struct toBuzzerMsg_t {
    toBuzzerMsgType_t dataType;
} toBuzzerMsg_t;


void sendToClient(toClientMsgType_t const dataType, char const * const data, ipc_t const * const ipc);
void sendToDisplay(toDisplayMsgType_t const dataType, char const * const data, ipc_t const * const ipc);
void sendToBuzzer(toBuzzerMsgType_t const dataType, ipc_t const * const ipc);
