/**
 * @brief HTTP endpoint for Google Push notifications (triggered by calander event changes)
 *
 * © Copyright 2016, 2022, Sander and Coert Vonk
 * 
 * This file is part of CALalarm.
 * 
 * CALalarm is free software: you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 * 
 * CALalarm is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with CALalarm. 
 * If not, see <https://www.gnu.org/licenses/>.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
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
        if (CONFIG_CALALARM_DBGLVL_HTTPD > 1) {
            ESP_LOGI(TAG, "Listening at http://" IPSTR "%s", IP2STR(ip), http_uri->uri);
        }
#endif
    }

	// mDNS

	ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("calalarm"));
    ESP_ERROR_CHECK(mdns_instance_name_set("CALalarm interface"));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_instance_name_set("_http", "_tcp", "CALalarm"));
}