/**
* @brief mqtt_client_task, fowards scan results to and control messages from MQTT broker
 **/
// Copyright © 2020, Coert Vonk
// SPDX-License-Identifier: MIT

#include <sdkconfig.h>
#include <stdlib.h>
#include <string.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <esp_ota_ops.h>

#include "mqtt_task.h"
#include "ipc_msgs.h"

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))
#define ALIGN( type ) __attribute__((aligned( __alignof__( type ) )))
#define PACK( type )  __attribute__((aligned( __alignof__( type ) ), packed ))
#define PACK8  __attribute__((aligned( __alignof__( uint8_t ) ), packed ))

static char const * const TAG = "mqtt_task";

static EventGroupHandle_t _mqttEventGrp = NULL;
typedef enum {
	MQTT_EVENT_CONNECTED_BIT = BIT0
} mqttEvent_t;

static struct {
    char * data;
    char * ctrl;
    char * ctrlGroup;
} _topic;

static esp_mqtt_client_handle_t _connect2broker(ipc_t const * const ipc);  // forward decl

void
sendToMqtt(toMqttMsgType_t const dataType, char const * const data, ipc_t const * const ipc)
{
    toMqttMsg_t msg = {
        .dataType = dataType,
        .data = strdup(data)
    };
    assert(msg.data);
    if (xQueueSendToBack(ipc->toMqttQ, &msg, 0) != pdPASS) {
        ESP_LOGE(TAG, "toMqttQ full");
        free(msg.data);
    }
}

static esp_err_t
_mqttEventHandler(esp_mqtt_event_handle_t event) {

    ipc_t const * const ipc = event->user_context;

	switch (event->event_id) {
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Broker disconnected");
            xEventGroupClearBits(_mqttEventGrp, MQTT_EVENT_CONNECTED_BIT);
        	// reconnect is part of the SDK
            break;
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Broker connected");
            xEventGroupSetBits(_mqttEventGrp, MQTT_EVENT_CONNECTED_BIT);
            esp_mqtt_client_subscribe(event->client, _topic.ctrl, 1);
            esp_mqtt_client_subscribe(event->client, _topic.ctrlGroup, 1);
            ESP_LOGI(TAG, " publishing to \"%s\"", _topic.data);
            ESP_LOGI(TAG, " subscribed to \"%s\", \"%s\"", _topic.ctrl, _topic.ctrlGroup);
            break;
        case MQTT_EVENT_DATA:
            if (event->topic && event->data_len == event->total_data_len) {  // quietly ignores chunked messaegs

                if (strncmp("restart", event->data, event->data_len) == 0) {

                    sendToMqtt(TO_MQTT_MSGTYPE_RESTART, "{ \"response\": \"restarting\" }", ipc);
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    esp_restart();

                } else if (strncmp("push", event->data, event->data_len) == 0) {

                    sendToMqtt(TO_MQTT_MSGTYPE_PUSH, "{ \"response\": \"pushed by MQTT\" }", ipc);
                    sendToClient(TO_CLIENT_MSGTYPE_TRIGGER, "mqtt push", ipc);

                } else if (strncmp("who", event->data, event->data_len) == 0) {

                    esp_partition_t const * const running_part = esp_ota_get_running_partition();
                    esp_app_desc_t running_app_info;
                    ESP_ERROR_CHECK(esp_ota_get_partition_description(running_part, &running_app_info));

                    wifi_ap_record_t ap_info;
                    ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));

                    char * payload;
                    int const payload_len = asprintf(&payload,
                        "{ \"name\": \"%s\", \"address\": \"%s\", \"firmware\": { \"version\": \"%s.%s\", \"date\": \"%s %s\" }, \"wifi\": { \"SSID\": \"%s\", \"RSSI\": %d }, \"mem\": { \"heap\": %u } }",
                        ipc->dev.name, ipc->dev.ipAddr,
                        running_app_info.project_name, running_app_info.version,
                        running_app_info.date, running_app_info.time,
                        ap_info.ssid, ap_info.rssi, heap_caps_get_free_size(MALLOC_CAP_8BIT)
                    );
                    assert(payload_len >= 0);
                    sendToMqtt(TO_MQTT_MSGTYPE_WHO, payload, ipc);
                    free(payload);
                }
            }
            break;
        default:
            break;
	}
	return ESP_OK;
}

static esp_mqtt_client_handle_t
_connect2broker(ipc_t const * const ipc) {

    esp_mqtt_client_handle_t client;
    xEventGroupClearBits(_mqttEventGrp, MQTT_EVENT_CONNECTED_BIT);
    {
        const esp_mqtt_client_config_t mqtt_cfg = {
            .event_handle = _mqttEventHandler,
            .user_context = (void *)ipc,
            .uri = CONFIG_CLOCK_MQTT_URL,
        };
        client = esp_mqtt_client_init(&mqtt_cfg);
        //ESP_ERROR_CHECK(esp_mqtt_client_set_uri(client, CONFIG_CLOCK_MQTT_URL));
        ESP_ERROR_CHECK(esp_mqtt_client_start(client));
    }
	assert(xEventGroupWaitBits(_mqttEventGrp, MQTT_EVENT_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY));
    return client;
}

static char *
_type2subtopic(toMqttMsgType_t const type)
{
    struct mapping {
        toMqttMsgType_t const type;
        char const * const subtopic;
    } mapping[] = {
        { TO_MQTT_MSGTYPE_RESTART, "restart" },
        { TO_MQTT_MSGTYPE_PUSH, "push" },
        { TO_MQTT_MSGTYPE_WHO, "who" },
        { TO_MQTT_MSGTYPE_DBG, "dbg" },
    };
    for (uint ii = 0; ii < ARRAYSIZE(mapping); ii++) {
        if (type == mapping[ii].type) {
            return mapping[ii].subtopic;
        }
    }
    return NULL;
}

void
mqtt_task(void * ipc_void) {

    ESP_LOGI(TAG, "starting ..");
	ipc_t * ipc = ipc_void;

    _topic.data      = malloc(strlen(CONFIG_CLOCK_MQTT_DATA_TOPIC) + 1 + strlen(ipc->dev.name) + 1);
    _topic.ctrl      = malloc(strlen(CONFIG_CLOCK_MQTT_CTRL_TOPIC) + 1 + strlen(ipc->dev.name) + 1);
    _topic.ctrlGroup = malloc(strlen(CONFIG_CLOCK_MQTT_CTRL_TOPIC) + 1);
    assert(_topic.data && _topic.ctrl && _topic.ctrlGroup);
    sprintf(_topic.data, "%s/%s", CONFIG_CLOCK_MQTT_DATA_TOPIC, ipc->dev.name);
    sprintf(_topic.ctrl, "%s/%s", CONFIG_CLOCK_MQTT_CTRL_TOPIC, ipc->dev.name);
    sprintf(_topic.ctrlGroup, "%s", CONFIG_CLOCK_MQTT_CTRL_TOPIC);

	_mqttEventGrp = xEventGroupCreate();
    esp_mqtt_client_handle_t const client = _connect2broker(ipc);

	while (1) {
        toMqttMsg_t msg;
		if (xQueueReceive(ipc->toMqttQ, &msg, (TickType_t)(1000L / portTICK_PERIOD_MS)) == pdPASS) {
            char * subtopic = _type2subtopic(msg.dataType);
            char * topic;
            if (subtopic) {
                topic = asprintf("%s/%s", _topic.data, subtopic);
            } else {
                topic = strdup(_topic.data);
            }
            esp_mqtt_client_publish(client, topic, msg.data, strlen(msg.data), 1, 0);
            free(topic);
            free(msg.data);
		}
	}
}