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

static char const * const TAG = "main_app";
static EventGroupHandle_t _wifi_event_group;
typedef enum {
    WIFI_EVENT_CONNECTED = BIT0
} my_wifi_event_t;

static http_post_server_ipc_t _http_post_server_ipc;

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

#if 0
static void
_eventHandler(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG,"Disconnected from WiFi, waiting before retry");
        vTaskDelay(30000L / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "reconnecting to the WiFi ..");
        esp_wifi_connect();

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t const * const event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Connected, got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(_wifi_event_group, WIFI_EVENT_CONNECTED);
    }
}
#endif

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

    //tcpip_adapter_init();
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

void
app_main()
{
    ESP_LOGI(TAG, "starting ..");
    xTaskCreate(&reset_task, "reset_task", 4096, NULL, 5, NULL);

    _initNvsFlash();

    QueueHandle_t jsonQ = xQueueCreate(2, sizeof(char *));
    QueueHandle_t triggerQ = xQueueCreate(2, sizeof(char *));
    if (!jsonQ || !triggerQ) esp_restart();

    _http_post_server_ipc.triggerQ = triggerQ;
    ESP_LOGI(TAG, "%s triggerQ = %p", __func__, triggerQ);
    // http_post_server is started in response to getting an IP address assigned

    _connect2wifi();  // waits for connection established

    //xTaskCreate(&ota_task, "ota_task", 4096, NULL, 5, NULL);

    static display_task_ipc_t display_task_ipc;
    display_task_ipc.jsonQ = jsonQ;
    xTaskCreate(&display_task, "display_task", 4096, &display_task_ipc, 5, NULL);


    static https_client_task_ipc_t https_client_task_ipc;
    https_client_task_ipc.triggerQ = triggerQ;
    https_client_task_ipc.jsonQ = jsonQ;
    xTaskCreate(&https_client_task, "https_client_task", 4096, &https_client_task_ipc, 5, NULL);
}