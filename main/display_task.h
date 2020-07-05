#pragma once

typedef struct {
    QueueHandle_t jsonQ;
} display_task_ipc_t;

void display_task(void * ipc_void);

