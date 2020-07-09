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

#include "mqtt_client_task.h"
#include "ipc_msgs.h"

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))
#define ALIGN( type ) __attribute__((aligned( __alignof__( type ) )))
#define PACK( type )  __attribute__((aligned( __alignof__( type ) ), packed ))
#define PACK8  __attribute__((aligned( __alignof__( uint8_t ) ), packed ))

static char const * _devIPAddr = "";
static char const * _devName = "";

static char const * const TAG = "mqtt_client_task";
static mqtt_client_task_ipc_t const * _ipc = NULL;

static EventGroupHandle_t _mqttEventGrp = NULL;
typedef enum {
	MQTT_EVENT_CONNECTED_BIT = BIT0
} mqttEvent_t;

static struct {
    char * data;
    char * ctrl;
    char * ctrlGroup;
} _topic;

static esp_mqtt_client_handle_t _client;

static void _connect2broker(void);  // forward decl

static esp_err_t
_mqttEventHandler(esp_mqtt_event_handle_t event) {

	switch (event->event_id) {
        case MQTT_EVENT_DISCONNECTED:
        	_connect2broker();
            break;
        case MQTT_EVENT_CONNECTED:
            xEventGroupSetBits(_mqttEventGrp, MQTT_EVENT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_DATA:
            if (event->topic && event->data_len == event->total_data_len) {  // quietly ignores chunked messaegs

                if (strncmp("restart", event->data, event->data_len) == 0) {

                    char const * const payload = "{ \"response\": \"restarting\" }";
                    esp_mqtt_client_publish(event->client, _topic.data, payload, strlen(payload), 1, 0);
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    esp_restart();

                } else if (strncmp("push", event->data, event->data_len) == 0) {

                    char const * const payload = "{ \"response\": \"pushing\" }";
                    esp_mqtt_client_publish(event->client, _topic.data, payload, strlen(payload), 1, 0);

                    toClientMsg_t msg = {
                        .dataType = TO_CLIENT_MSGTYPE_TRIGGER,
                        .data = strdup("mqtt push")
                    };
                    if (xQueueSendToBack(_ipc->toClientQ, &msg, 0) != pdPASS) {
                        ESP_LOGW(TAG, "Queue full");
                        free(msg.data);
                    }

                } else if (strncmp("who", event->data, event->data_len) == 0) {

                    esp_partition_t const * const running_part = esp_ota_get_running_partition();
                    esp_app_desc_t running_app_info;
                    esp_ota_get_partition_description(running_part, &running_app_info);

                    wifi_ap_record_t ap_info;
                    esp_wifi_sta_get_ap_info(&ap_info);

                    char * format = "{ \"name\": \"%s\", \"address\": \"%s\", \"firmware\": { \"version\": \"%s.%s\", \"date\": \"%s %s\" }, \"wifi\": { \"SSID\": \"%s\", \"RSSI\": %d }, \"mem\": { \"heap\": %u } }";
                    uint const wiggleRoom = 40;
                    uint const payloadLen = strlen(format) + WIFI_DEVNAME_LEN + WIFI_DEVIPADDR_LEN + ARRAYSIZE(running_app_info.project_name) + ARRAYSIZE(running_app_info.version) + ARRAYSIZE(running_app_info.date) + ARRAYSIZE(running_app_info.time) + ARRAYSIZE(ap_info.ssid) + 3 + wiggleRoom;
                    char * const payload = malloc(payloadLen);

                    snprintf(payload, payloadLen, format, _devName, _devIPAddr,
                             running_app_info.project_name, running_app_info.version,
                             running_app_info.date, running_app_info.time,
                             ap_info.ssid, ap_info.rssi, heap_caps_get_free_size(MALLOC_CAP_8BIT));

                    esp_mqtt_client_publish(event->client, _topic.data, payload, strlen(payload), 1, 0);
                    free(payload);
                }
            }
            break;
        default:
            break;
	}
	return ESP_OK;
}

static void
_connect2broker(void) {

    xEventGroupClearBits(_mqttEventGrp, MQTT_EVENT_CONNECTED_BIT);
    {
        const esp_mqtt_client_config_t mqtt_cfg = { .event_handle = _mqttEventHandler };
        _client = esp_mqtt_client_init(&mqtt_cfg);

        esp_mqtt_client_set_uri(_client, CONFIG_CLOCK_MQTT_URL);
        esp_mqtt_client_start(_client);
    }
	EventBits_t bits = xEventGroupWaitBits(_mqttEventGrp, MQTT_EVENT_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
	if (!bits) esp_restart();  // give up

    esp_mqtt_client_subscribe(_client, _topic.ctrl, 1);
    esp_mqtt_client_subscribe(_client, _topic.ctrlGroup, 1);
	ESP_LOGI(TAG, "Connected to MQTT Broker");
}

void
mqtt_client_task(void * ipc) {

	_ipc = ipc;
    ESP_LOGI(TAG, "%s starting ..", __func__);

	// first message from _ipc->toMqttQ is the WiFi IP address

	toMqttMsg_t msg;
	if (xQueueReceive(_ipc->toMqttQ, &msg, (TickType_t)(30000L / portTICK_PERIOD_MS)) == pdPASS) {
        if (msg.dataType != TO_MQTT_MSGTYPE_DEVIPADDR) {
            ESP_LOGE(TAG, "unexpected dataType(%d)", msg.dataType);
            esp_restart();
        }
        _devIPAddr = msg.data;
		// do not free(msg.data) as we keep refering to it
    } else {
        ESP_LOGE(TAG, "Didn't receive devIPAddr");
        esp_restart();
    }

	// second message from _ipc->toMqttQ is the WiFi Device Name

	if (xQueueReceive(_ipc->toMqttQ, &msg, (TickType_t)(30000L / portTICK_PERIOD_MS)) == pdPASS) {

        if (msg.dataType != TO_MQTT_MSGTYPE_DEVNAME) {
            ESP_LOGE(TAG, "unexpected dataType(%d)", msg.dataType);
            esp_restart();
        }
		_devName = msg.data;
        _topic.data     = malloc(strlen(CONFIG_CLOCK_MQTT_DATA_TOPIC) + 1 + strlen(_devName) + 1);
        _topic.ctrl      = malloc(strlen(CONFIG_CLOCK_MQTT_CTRL_TOPIC) + 1 + strlen(_devName) + 1);
        _topic.ctrlGroup = malloc(strlen(CONFIG_CLOCK_MQTT_CTRL_TOPIC) + 1);
		sprintf(_topic.data, "%s/%s", CONFIG_CLOCK_MQTT_DATA_TOPIC, _devName);  // sent msgs
        sprintf(_topic.ctrl, "%s/%s", CONFIG_CLOCK_MQTT_CTRL_TOPIC, _devName);  // received device specific ctrl msg
        sprintf(_topic.ctrlGroup, "%s", CONFIG_CLOCK_MQTT_CTRL_TOPIC);          // received group ctrl msg
        ESP_LOGI(TAG, "%s, %s, %s", _topic.data, _topic.ctrl, _topic.ctrlGroup);
		// do not free(msg.data) as we keep refering to it
	} else {
        ESP_LOGE(TAG, "Didn't receive devName");
        esp_restart();
    }

	// connect to MQTT broker, and subcribe to ctrl topic

	_mqttEventGrp = xEventGroupCreate();
	_connect2broker();

	// all remaining messages from _ipc->toMqttQ are iBeacon scan results formatted as JSON (tx'd by ble_scan_task)

	while (1) {
		if (xQueueReceive(_ipc->toMqttQ, &msg, (TickType_t)(1000L / portTICK_PERIOD_MS)) == pdPASS) {

            switch (msg.dataType) {
                case TO_MQTT_MSGTYPE_DATA:
                    ESP_LOGI(TAG, "data %s \"%s\"", _topic.data, msg.data);
        			esp_mqtt_client_publish(_client, _topic.data, msg.data, strlen(msg.data), 1, 0);
                    break;
                case TO_MQTT_MSGTYPE_CTRL:
                    ESP_LOGI(TAG, "ctrl %s \"%s\"", _topic.data, msg.data);
        			esp_mqtt_client_publish(_client, _topic.ctrl, msg.data, strlen(msg.data), 1, 0);
                    break;
                case TO_MQTT_MSGTYPE_DEVIPADDR:
                case TO_MQTT_MSGTYPE_DEVNAME:
                    ESP_LOGE(TAG, "unexpected dataType(%d)", msg.dataType);
                    break;
           }
			free(msg.data);
		}
	}
}