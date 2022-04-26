/**
 * @brief buzzer_task - Sound piezo element and haptic; stop when STOP_ALARM button is pressed
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
 */

#include <stdio.h>
#include <string.h>
#include <esp_system.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <driver/gpio.h>
#include "driver/ledc.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ipc/ipc.h"
#include "buzzer_task.h"

// static char const * const TAG = "buzzer_task";

void
sendToBuzzer(toBuzzerMsgType_t const dataType, ipc_t const * const ipc)
{
    toDisplayMsg_t msg = {
        .dataType = dataType,
    };
    xQueueSendToBack(ipc->toBuzzerQ, &msg, 0);
}

static void inline IRAM_ATTR
_sendToBuzzerFromISR(toBuzzerMsgType_t const dataType, ipc_t const * const ipc)
{
    toDisplayMsg_t msg = {
        .dataType = dataType,
    };
    xQueueSendToBackFromISR(ipc->toBuzzerQ, &msg, 0);
}

static void IRAM_ATTR
_button_isr_handler(void * arg)
{
    ipc_t * const ipc = (ipc_t *) arg;

    if (gpio_get_level(CONFIG_CALALARM_ALARM_OFF_PIN) == 0) {
        _sendToBuzzerFromISR(TO_BUZZER_MSGTYPE_STOP, ipc);
    } else {
    }
}

static void
_button_isr_init(ipc_t * const ipc)
{
    gpio_pad_select_gpio(CONFIG_CALALARM_ALARM_OFF_PIN);
    gpio_set_direction(CONFIG_CALALARM_ALARM_OFF_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(CONFIG_CALALARM_ALARM_OFF_PIN, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(CONFIG_CALALARM_ALARM_OFF_PIN, _button_isr_handler, ipc);
}

static void
_buzzer_init()
{
    // use LED Control PWM timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_13_BIT, // Set duty resolution to 13 bits
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 1000,  // [Hz]
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // use LED Control PWM channel
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = CONFIG_CALALARM_PIEZO3V_PIN,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 0, // set later
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));  
}

void
buzzer_task(void * ipc_void)
{
    ipc_t * const ipc = (ipc_t *) ipc_void;

    _button_isr_init(ipc);
    _buzzer_init();

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << CONFIG_CALALARM_HAPTIC3V_PIN,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };    
    ESP_ERROR_CHECK( gpio_config(&io_conf) );

    bool buzzer_on = false;
    bool haptic_active = false;

    while (1) {
        toBuzzerMsg_t msg;
        if (xQueueReceive(ipc->toBuzzerQ, &msg, (TickType_t)(1000 / portTICK_PERIOD_MS)) == pdPASS) {

            switch (msg.dataType) {
                case TO_BUZZER_MSGTYPE_START:
                    buzzer_on = true;
                    haptic_active = true;
                    break;
                case TO_BUZZER_MSGTYPE_STOP:
                    buzzer_on = false;
                    break;
            }
        }

        if (buzzer_on) {
            uint32_t const duty = 409; // 5% duty cycle ((2 ** 13) - 1) * 5%
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
            gpio_set_level(CONFIG_CALALARM_HAPTIC3V_PIN, haptic_active);
            haptic_active =! haptic_active;
        } else {
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
            gpio_set_level(CONFIG_CALALARM_HAPTIC3V_PIN, 0);
        }
    }
}
