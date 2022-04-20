#pragma once
#include <esp_wifi.h>
#include <esp_http_server.h>

#include "../ipc/ipc.h"

/* httpd.c */
void httpd_register_handlers(httpd_handle_t const httpd_handle, esp_ip4_addr_t const * const ip, ipc_t const * const ipc);

/* httpd_google_push.c */
esp_err_t _httpd_google_push_handler(httpd_req_t * req);