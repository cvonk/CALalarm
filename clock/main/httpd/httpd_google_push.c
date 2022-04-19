/**
 * @brief OPNclock - HTTPd: HTTP server callback for endpoint "/api/push"
 *
 * © Copyright 2014, 2019, 2022, Sander and Coert Vonk
 * 
 * This file is part of OPNclock.
 * OPNclock is free software: you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 * OPNclock is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with OPNclock. 
 * If not, see <https://www.gnu.org/licenses/>.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "httpd.h"

#define MAX_CONTENT_LEN (2048)
#define MIN(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
#define MAX(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })

static char const * const TAG = "httpd_google_push";

esp_err_t
_httpd_google_push_handler(httpd_req_t * req)
{
    ipc_t const * const ipc = req->user_ctx;

    if (req->content_len >= MAX_CONTENT_LEN) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }
    // +1 so even for no content body, we have a buf
    char * const buf = malloc(req->content_len + 1);  // https_client_task reads from Q and frees mem
    assert(buf);

    uint len = 0;
    while (len < req->content_len) {
        uint received = httpd_req_recv(req, buf + len, req->content_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post value");
            return ESP_FAIL;
        }
        len += received;
    }
    uint const grs_len = 10;
    char grs[grs_len];
    bool const pushAck = httpd_req_get_hdr_value_str(req, "X-Goog-Resource-State", grs, grs_len) == ESP_OK && strcmp(grs, "sync") == 0;
    if (!pushAck) {
        buf[req->content_len] = '\0';
        ESP_LOGI(TAG, "Google push notification");

        sendToClient(TO_CLIENT_MSGTYPE_TRIGGER, buf, ipc);
        sendToMqtt(TO_MQTT_MSGTYPE_PUSH, "{ \"response\": \"pushed by Google\" }", ipc);

    }
    free(buf);
    httpd_resp_sendstr(req, "thank you");
    return ESP_OK;
}

