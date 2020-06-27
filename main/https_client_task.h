#pragma once

typedef struct {
    QueueHandle_t triggerQ;
    QueueHandle_t jsonQ;
} https_client_task_ipc_t;

void https_client_task(void * ipc);