#pragma once

typedef struct {
    QueueHandle_t toClientQ;
    QueueHandle_t toDisplayQ;
    QueueHandle_t toMqttQ;
} https_client_task_ipc_t;

void https_client_task(void * ipc);