#pragma once

#if 0
typedef struct {
    QueueHandle_t toDisplayQ;
    QueueHandle_t toMqttQ;
} buzzer_task_ipc_t;
#endif

void buzzer_task(void * ipc_void);
