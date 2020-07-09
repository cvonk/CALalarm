/**
 * @brief Read calender events from Google Apps script and forward them as messages
 *
 * CLOSED SOURCE, NOT FOR PUBLIC RELEASE
 * (c) Copyright 2020, Sander and Coert Vonk
 * All rights reserved. Use of copyright notice does not imply publication.
 * All text above must be included in any redistribution
 **/

#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_tls.h>
#include <esp_http_client.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>

#include "https_client_task.h"
#include "ipc_msgs.h"

static const char * TAG = "https_client_task";
static char * _data = NULL;
static int _data_len = 0;

esp_err_t
_http_event_handle(esp_http_client_event_t *evt)
{
    // gets called twice in a row because of the redirect that GAS uses

    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        // store data ptr and len here, because content_length returns -1
        _data = evt->data;
        _data_len = evt->data_len;
    }
    return ESP_OK;
}

void
https_client_task(void * ipc_void)
{
    https_client_task_ipc_t * const ipc = ipc_void;

    while (1) {

        esp_http_client_config_t config = {
            .url = CONFIG_CLOCK_GAS_CALENDAR_URL,
            .event_handler = _http_event_handle,
            .buffer_size = 2048, // big enough so "Location:" in the header doesn't get split over 2 chunks
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            int const status = esp_http_client_get_status_code(client);

            // content_length returns -1 to indicate the data arrived chunked, instead use the data_len that we stored
            ESP_LOGI(TAG, "status = %d, _data_len = %d", status, _data_len);
            if (status == 200) {

                _data[_data_len] = '\0';
                ESP_LOGI(TAG, "body = \"%.*s\"", _data_len, _data);

                {
                    toDisplayMsg_t msg = {
                        .dataType = TO_DISPLAY_MSGTYPE_JSON,
                        .data = strdup(_data)
                    };
                    if (xQueueSendToBack(ipc->toDisplayQ, &msg, 0) != pdPASS) {
                        ESP_LOGW(TAG, "Queue full");
                        free(msg.data);
                    }
                }
                {   // send a copy to MQTT broker
                    toMqttMsg_t msg = {
                        .dataType = TO_MQTT_MSGTYPE_DATA,
                        .data = strdup(_data)
                    };
                    if (xQueueSendToBack(ipc->toMqttQ, &msg, 0) != pdPASS) {
                        ESP_LOGE(TAG, "toMqttQ full");
                        free(msg.data);
                    }
                }
            }
        }
        esp_http_client_cleanup(client);

        toClientMsg_t msg;
        if (xQueueReceive(ipc->toClientQ, &msg, CONFIG_CLOCK_GAS_INTERVAL * 60000L / portTICK_PERIOD_MS) == pdPASS) {
            free(msg.data);
            ESP_LOGI(TAG, "triggered");
        }
    }
}
