/**
 * @brief HTTP endpoint for Google Push notifications (triggered by calander event changes)
 *
 * CLOSED SOURCE, NOT FOR PUBLIC RELEASE
 * (c) Copyright 2020, Sander and Coert Vonk
 * All rights reserved. Use of copyright notice does not imply publication.
 * All text above must be included in any redistribution
 **/

#include <string.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_http_server.h>
#include <mdns.h>

#include "httpd.h"
#include "../ipc/ipc.h"

// static const char * TAG = "httpd";

static httpd_uri_t _httpd_uris[] = {
    {
        .uri = "/api/push",
        .method = HTTP_POST,
        .handler = _httpd_google_push_handler
    }
};

/*
 * Register the URI handlers for incoming GET requests.
 */

void
httpd_register_handlers(httpd_handle_t const httpd_handle, esp_ip4_addr_t const * const ip, ipc_t const * const ipc)
{
    httpd_uri_t * http_uri = _httpd_uris;
    for (int ii = 0; ii < ARRAY_SIZE(_httpd_uris); ii++, http_uri++) {

        http_uri->user_ctx = (void *) ipc;
        ESP_ERROR_CHECK( httpd_register_uri_handler(httpd_handle, http_uri) );
#if 0        
        if (CONFIG_OPNCLOCK_DBGLVL_HTTPD > 1) {
            ESP_LOGI(TAG, "Listening at http://" IPSTR "%s", IP2STR(ip), http_uri->uri);
        }
#endif
    }

	// mDNS

	ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("opnclock"));
    ESP_ERROR_CHECK(mdns_instance_name_set("OPNclock interface"));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_instance_name_set("_http", "_tcp", "OPNclock"));
}