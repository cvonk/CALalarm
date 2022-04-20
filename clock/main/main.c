/**
 * @brief Clock Calendar using ESP32
 *
 * CLOSED SOURCE, NOT FOR PUBLIC RELEASE
 * (c) Copyright 2020, Sander and Coert Vonk
 * All rights reserved. Use of copyright notice does not imply publication.
 * All text above must be included in any redistribution
**/

#include <esp_system.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "ota_update_task.h"
#include "wifi_connect.h"
#include "factory_reset_task.h"

#include "httpd/httpd.h"
#include "http/https_client_task.h"
#include "ipc/ipc.h"

#include "display_task.h"
#include "mqtt_task.h"

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))
#define ALIGN( type ) __attribute__((aligned( __alignof__( type ) )))
#define PACK( type )  __attribute__((aligned( __alignof__( type ) ), packed ))
#define PACK8  __attribute__((aligned( __alignof__( uint8_t ) ), packed ))

static char const * const TAG = "main_app";

typedef struct wifi_connect_priv_t {
    ipc_t * ipc;
    httpd_handle_t httpd_handle;
} wifi_connect_priv_t;

static void
_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static void
_mac2devname(uint8_t const * const mac, char * const name, size_t name_len) {
	typedef struct {
		uint8_t const mac[WIFI_DEVMAC_LEN];
		char const * const name;
	} knownBrd_t;
	static knownBrd_t knownBrds[] = {
        { {0x30, 0xAE, 0xA4, 0x1A, 0x20, 0xF0}, "opnclock-1" },
        { {0x30, 0xAE, 0xA4, 0x24, 0x2C, 0x98}, "opnclock-2" },
        { {0x30, 0xae, 0xa4, 0xcc, 0x45, 0x04}, "esp32-wrover-1" },
        { {0x30, 0xAE, 0xA4, 0xCC, 0x42, 0x78}, "esp32-wrover-2" },
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

static esp_err_t
_wifi_connect_cb(void * const priv_void, esp_ip4_addr_t const * const ip)
{
    wifi_connect_priv_t * const priv = priv_void;
    ipc_t * const ipc = priv->ipc;

    // note the MAC and IP addresses

    snprintf(ipc->dev.ipAddr, WIFI_DEVIPADDR_LEN, IPSTR, IP2STR(ip));
    uint8_t mac[WIFI_DEVMAC_LEN];
    ESP_ERROR_CHECK(esp_base_mac_addr_get(mac));
	_mac2devname(mac, ipc->dev.name, WIFI_DEVNAME_LEN);
    ESP_LOGI(TAG, "%s / %s / %u", ipc->dev.ipAddr, ipc->dev.name, ipc->dev.connectCnt.wifi);

    // start HTTP server
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&priv->httpd_handle, &httpd_config));

    httpd_register_handlers(priv->httpd_handle, ip, ipc);

    ipc->dev.connectCnt.wifi++;
    return ESP_OK;
}

/*
 * Stop the HTTP server
 */

static esp_err_t
_wifi_disconnect_cb(void * const priv_void, bool const auth_err)
{
    wifi_connect_priv_t * const priv = priv_void;
    ipc_t * const ipc = priv->ipc;

    if (priv->httpd_handle) {
        httpd_stop(priv->httpd_handle);
        priv->httpd_handle = NULL;
    }
    if (auth_err) {
        //ipc->dev.wifi .count.wifiAuthErr++;
        // 2BD: should probably reprovision on repeated auth_err and return ESP_FAIL
    }
    ESP_LOGW(TAG, "Wifi disconnect connectCnt=%u", ipc->dev.connectCnt.wifi);
    return ESP_OK;
}

static void
__attribute__((noreturn)) _delete_task()
{
    ESP_LOGI(TAG, "Exiting task ..");
    (void)vTaskDelete(NULL);

    while (1) {  // FreeRTOS requires that tasks never return
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

/*
 * Connect to WiFi accesspoint.
 * Register callbacks when connected or disconnected.
 */

static void
_connect2wifi_and_start_httpd(ipc_t * const ipc)
{
    static wifi_connect_priv_t priv = {};
    priv.ipc = ipc;

    wifi_connect_config_t wifi_connect_config = {
        .onConnect = _wifi_connect_cb,
        .onDisconnect = _wifi_disconnect_cb,
        .priv = &priv,
    };
    ESP_ERROR_CHECK(wifi_connect_init(&wifi_connect_config));

    wifi_config_t * wifi_config_addr = NULL;
#ifdef CONFIG_OPNCLOCK_HARDCODED_WIFI_CREDENTIALS
    if (strlen(CONFIG_OPNCLOCK_HARDCODED_WIFI_SSID)) {
        ESP_LOGW(TAG, "Using SSID from Kconfig");
        wifi_config_t wifi_config = {
            .sta = {
                .ssid = CONFIG_OPNCLOCK_HARDCODED_WIFI_SSID,
                .password = CONFIG_OPNCLOCK_HARDCODED_WIFI_PASSWD,
            }
        };
        wifi_config_addr = &wifi_config;
    } else
#endif
    {
        ESP_LOGW(TAG, "Using SSID from nvram");
    }

    esp_err_t err = wifi_connect_start(wifi_config_addr);
    if (err == ESP_ERR_WIFI_SSID) {
        ESP_LOGE(TAG, "Wi-Fi SSID/passwd not provisioned");
        _delete_task();
    }
    ESP_ERROR_CHECK(err);
}

void
app_main()
{
    _init_nvs();

    ESP_LOGI(TAG, "starting ..");
    xTaskCreate(&factory_reset_task, "factory_reset_task", 4096, NULL, 5, NULL);

    static ipc_t ipc;
    ipc.toClientQ = xQueueCreate(2, sizeof(toClientMsg_t));
    ipc.toDisplayQ = xQueueCreate(2, sizeof(toDisplayMsg_t));
    ipc.toMqttQ = xQueueCreate(2, sizeof(toMqttMsg_t));
    ipc.dev.connectCnt.wifi = 0;
    ipc.dev.connectCnt.mqtt = 0;
    assert(ipc.toDisplayQ && ipc.toClientQ && ipc.toMqttQ);

    _connect2wifi_and_start_httpd(&ipc);

    // from here the tasks take over

    xTaskCreate(&ota_update_task, "ota_update_task", 4096, "clock", 5, NULL);
    xTaskCreate(&display_task, "display_task", 4096, &ipc, 5, NULL);
    xTaskCreate(&https_client_task, "https_client_task", 4096, &ipc, 5, NULL);
    xTaskCreate(&mqtt_task, "mqtt_task", 2*4096, &ipc, 5, NULL);
}