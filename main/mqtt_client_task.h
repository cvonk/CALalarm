#pragma once

typedef struct {
    QueueHandle_t toClientQ;
    QueueHandle_t toMqttQ;
} mqtt_client_task_ipc_t;

void mqtt_client_task(void * ipc_void);

