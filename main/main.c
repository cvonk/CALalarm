/**
 * @brief Clock Calendar using ESP32
 *
 * CLOSED SOURCE, NOT FOR PUBLIC RELEASE
 * (c) Copyright 2020, Sander and Coert Vonk
 * All rights reserved. Use of copyright notice does not imply publication.
 * All text above must be included in any redistribution
**/

#include <sdkconfig.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_ota_ops.h>

#include "ota_task.h"
#include "reset_task.h"
#include "http_post_server.h"
#include "https_client_task.h"
#include "display_task.h"
#include "mqtt_client_task.h"
#include "mqtt_msg.h"

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))
#define ALIGN( type ) __attribute__((aligned( __alignof__( type ) )))
#define PACK( type )  __attribute__((aligned( __alignof__( type ) ), packed ))
#define PACK8  __attribute__((aligned( __alignof__( uint8_t ) ), packed ))

static char const * const TAG = "main_app";
static EventGroupHandle_t _wifi_event_group;
typedef enum {
    WIFI_EVENT_CONNECTED = BIT0
} my_wifi_event_t;

static http_post_server_ipc_t _http_post_server_ipc;

static char _devIPAddr[WIFI_DEVIPADDR_LEN];

static void
_initNvsFlash(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static void
_wifiStaStart(void * arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "STA start");
    esp_wifi_connect();
}

static void
_wifiDisconnectHandler(void * arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "Disconnected from WiFi");
    xEventGroupClearBits(_wifi_event_group, WIFI_EVENT_CONNECTED);

    httpd_handle_t * server = (httpd_handle_t *) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        http_post_server_stop(*server);
    }
    esp_wifi_connect();
}

static void
_wifiConnectHandler(void * arg, esp_event_base_t event_base,  int32_t event_id, void * event_data)
{
    ESP_LOGI(TAG, "Connected to WiFi");
    xEventGroupSetBits(_wifi_event_group, WIFI_EVENT_CONNECTED);

    ip_event_got_ip_t const * const event = (ip_event_got_ip_t *) event_data;
    snprintf(_devIPAddr, WIFI_DEVIPADDR_LEN, IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "IP addr = %s", _devIPAddr);

    httpd_handle_t * server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        ESP_LOGI(TAG, "%s ipc = %p", __func__, &_http_post_server_ipc);
        *server = http_post_server_start(&_http_post_server_ipc);
    }
    //pool_mdns_init();
}

static void
_connect2wifi(void)
{
    _wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();  // init WiFi with configuration from non-volatile storage
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &_wifiStaStart, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &_wifiDisconnectHandler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &_wifiConnectHandler, &server));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // wait until either the connection is established
    EventBits_t bits = xEventGroupWaitBits(_wifi_event_group, WIFI_EVENT_CONNECTED, pdFALSE, pdFALSE, portMAX_DELAY);
    if (!bits) esp_restart();  // give up
}

static void
_mac2devname(uint8_t const * const mac, char * const name, size_t name_len) {
	typedef struct {
		uint8_t const mac[WIFI_DEVMAC_LEN];
		char const * const name;
	} knownBrd_t;
	static knownBrd_t knownBrds[] = {
        { {0x30, 0xAE, 0xA4, 0xCC, 0x42, 0x78}, "esp32-wrover-1" }
	};
	for (uint ii=0; ii < ARRAYSIZE(knownBrds); ii++) {
		if (memcmp(mac, knownBrds[ii].mac, WIFI_DEVMAC_LEN) == 0) {
			strncpy(name, knownBrds[ii].name, name_len);
			return;
		}
	}
	snprintf(name, name_len, "esp32_%02x%02x",
			 mac[WIFI_DEVMAC_LEN-2], mac[WIFI_DEVMAC_LEN-1]);
}

void
app_main()
{
    ESP_LOGI(TAG, "starting ..");
    xTaskCreate(&reset_task, "reset_task", 4096, NULL, 5, NULL);

    _initNvsFlash();

    QueueHandle_t jsonQ = xQueueCreate(2, sizeof(char *));
    QueueHandle_t triggerQ = xQueueCreate(2, sizeof(char *));
    QueueHandle_t toMqttQ = xQueueCreate(2, sizeof(toMqttMsg_t));
    if (!jsonQ || !triggerQ) esp_restart();

    _http_post_server_ipc.triggerQ = triggerQ;
    ESP_LOGI(TAG, "%s triggerQ = %p", __func__, triggerQ);
    // http_post_server is started in response to getting an IP address assigned

    _connect2wifi();  // waits for connection established

    // first message to toMqttQ is the IP address

    {
        toMqttMsg_t msg = {
            .dataType = TO_MQTT_MSGTYPE_DEVIPADDR,
            .data = strdup(_devIPAddr)
        };
        if (xQueueSendToBack(toMqttQ, &msg, 0) != pdPASS) {
            ESP_LOGE(TAG, "toMqttQ full (1st)");  // should never happen, since its the first msg
            free(msg.data);
        }
	}

	// second message to toMqttQ is the device name (rx'ed by mqtt_client_task)

    uint8_t mac[WIFI_DEVMAC_LEN];
    esp_base_mac_addr_get(mac);
	char devName[WIFI_DEVNAME_LEN];
	_mac2devname(mac, devName, WIFI_DEVNAME_LEN);
    {
        toMqttMsg_t msg = {
            .dataType = TO_MQTT_MSGTYPE_DEVNAME,
            .data = strdup(devName)
        };
        if (xQueueSendToBack(toMqttQ, &msg, 0) != pdPASS) {
            ESP_LOGE(TAG, "toMqttQ full (2nd)");  // should never happen, since its the first msg
            free(msg.data);
        }
    }

    xTaskCreate(&ota_task, "ota_task", 4096, NULL, 5, NULL);

    static display_task_ipc_t display_task_ipc;
    display_task_ipc.jsonQ = jsonQ;
    xTaskCreate(&display_task, "display_task", 4096, &display_task_ipc, 5, NULL);

    static https_client_task_ipc_t https_client_task_ipc;
    https_client_task_ipc.triggerQ = triggerQ;
    https_client_task_ipc.jsonQ = jsonQ;
    xTaskCreate(&https_client_task, "https_client_task", 4096, &https_client_task_ipc, 5, NULL);

    static mqtt_client_task_ipc_t mqtt_client_task_ipc;
    mqtt_client_task_ipc.triggerQ = triggerQ;
    mqtt_client_task_ipc.toMqttQ = toMqttQ;
    xTaskCreate(&mqtt_client_task, "mqtt_client_task", 2*4096, &mqtt_client_task_ipc, 5, NULL);
}