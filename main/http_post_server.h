#pragma once
#include <esp_http_server.h>

typedef struct {
    QueueHandle_t toClientQ;
} http_post_server_ipc_t;

void http_post_server_stop(httpd_handle_t server);
httpd_handle_t http_post_server_start(void * ipc_void);
