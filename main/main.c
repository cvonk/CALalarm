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
#include "mqtt_task.h"
#include "ipc_msgs.h"

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))
#define ALIGN( type ) __attribute__((aligned( __alignof__( type ) )))
#define PACK( type )  __attribute__((aligned( __alignof__( type ) ), packed ))
#define PACK8  __attribute__((aligned( __alignof__( uint8_t ) ), packed ))

static char const * const TAG = "main_app";
static EventGroupHandle_t _wifi_event_group;
typedef enum {
    WIFI_EVENT_CONNECTED = BIT0
} my_wifi_event_t;

typedef struct event_handler_arg_t {
    ipc_t const * const ipc;
    httpd_handle_t server;
} event_handler_arg_t;

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
_wifiDisconnectHandler(void * arg_void, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
    ESP_LOGI(TAG, "Disconnected from WiFi");
    xEventGroupClearBits(_wifi_event_group, WIFI_EVENT_CONNECTED);

    event_handler_arg_t * const arg = arg_void;
    if (arg->server) {
        ESP_LOGI(TAG, "Stopping webserver");
        http_post_server_stop(arg->server);
        arg->server = NULL;
    }
    esp_wifi_connect();
}

static void
_wifiConnectHandler(void * arg_void, esp_event_base_t event_base,  int32_t event_id, void * event_data)
{
    ESP_LOGI(TAG, "Connected to WiFi");
    event_handler_arg_t * arg = arg_void;
    xEventGroupSetBits(_wifi_event_group, WIFI_EVENT_CONNECTED);

    ip_event_got_ip_t const * const event = (ip_event_got_ip_t *) event_data;
    snprintf(_devIPAddr, WIFI_DEVIPADDR_LEN, IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "IP addr = %s", _devIPAddr);

    if (!arg->server) {
        ESP_LOGI(TAG, "Starting webserver");
        arg->server = http_post_server_start(arg->ipc);
    }
    //pool_mdns_init();
}

static void
_connect2wifi(ipc_t const * const ipc)
{
    _wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();  // init WiFi with configuration from non-volatile storage
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    event_handler_arg_t event_handler_arg = {
        .ipc = ipc,
        .server = NULL
    };
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &_wifiStaStart, &event_handler_arg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &_wifiDisconnectHandler, &event_handler_arg));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &_wifiConnectHandler, &event_handler_arg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (strlen(CONFIG_WIFI_SSID) && strlen(CONFIG_WIFI_PASSWD)) {
        wifi_config_t wifi_config = {
            .sta = {
                .ssid = CONFIG_WIFI_SSID,
                .password = CONFIG_WIFI_PASSWD
            }
        };
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    };
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
        { {0x30, 0xAE, 0xA4, 0xCC, 0x42, 0x78}, "esp32-wrover-1" },
        { {0x30, 0xAE, 0xA4, 0x1A, 0x20, 0xF0}, "sander" },
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

    static ipc_t ipc;
    ipc.toClientQ = xQueueCreate(2, sizeof(toClientMsg_t));
    ipc.toDisplayQ = xQueueCreate(2, sizeof(toDisplayMsg_t));
    ipc.toMqttQ = xQueueCreate(2, sizeof(toMqttMsg_t));
    if (!ipc.toDisplayQ || !ipc.toClientQ || !ipc.toMqttQ) esp_restart();

    _connect2wifi(&ipc);  // waits for connection established

    // first messages to toMqttQ are the IP address and device name

    uint8_t mac[WIFI_DEVMAC_LEN];
    esp_base_mac_addr_get(mac);
	char devName[WIFI_DEVNAME_LEN];
	_mac2devname(mac, devName, WIFI_DEVNAME_LEN);
    sendToMqtt(TO_MQTT_MSGTYPE_DEVIPADDR, _devIPAddr, &ipc);
    sendToMqtt(TO_MQTT_MSGTYPE_DEVNAME, devName, &ipc);

    // from here the tasks take over

    xTaskCreate(&ota_task, "ota_task", 4096, NULL, 5, NULL);
    xTaskCreate(&display_task, "display_task", 4096, &ipc, 5, NULL);
    xTaskCreate(&https_client_task, "https_client_task", 4096, &ipc, 5, NULL);
    xTaskCreate(&mqtt_task, "mqtt_task", 2*4096, &ipc, 5, NULL);
}