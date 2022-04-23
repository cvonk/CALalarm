/**
 * @brief buzzer_task - Sound piezo element and haptic; stop when STOP_ALARM button is pressed
 *
 * To the extent possible under law, the author(s) have dedicated all copyright and related and
 * neighboring rights to this software to the public domain worldwide. This software is
 * distributed without any warranty. You should have received a copy of the CC0 Public Domain
 * Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
 * 
 * SPDX-License-Identifier: CC0-1.0
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

static char const * const TAG = "buzzer_task";

void
sendToBuzzer(toBuzzerMsgType_t const dataType, ipc_t const * const ipc)
{
    toDisplayMsg_t msg = {
        .dataType = dataType,
    };
    if (xQueueSendToBack(ipc->toBuzzerQ, &msg, 0) != pdPASS) {
        ESP_LOGE(TAG, "toBuzzerQ full");
        free(msg.data);
    }
}

static void inline IRAM_ATTR
_sendToBuzzerFromISR(toBuzzerMsgType_t const dataType, ipc_t const * const ipc)
{
    toDisplayMsg_t msg = {
        .dataType = dataType,
    };
    
    if (xQueueSendToBackFromISR(ipc->toBuzzerQ, &msg, 0) != pdPASS) {
        free(msg.data);
    }
}

static void IRAM_ATTR
_button_isr_handler(void * arg)
{
    ipc_t * const ipc = (ipc_t *) arg;

    #define CONFIG_CALALARM_ALARM_OFF_DEBOUNCE_MSEC (100)

    static int64_t start = 0;
    if (gpio_get_level(CONFIG_CALALARM_ALARM_OFF_PIN) == 0) {
        start = esp_timer_get_time();
    } else {
        if (esp_timer_get_time() - start > CONFIG_CALALARM_ALARM_OFF_DEBOUNCE_MSEC * 1000L) {
            _sendToBuzzerFromISR(TO_BUZZER_MSGTYPE_STOP, ipc);
        }
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

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          CONFIG_CALALARM_PIEZO3V_PIN // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4095) // 50% duty cycle ((2 ** 13) - 1) * 50% = 4095
#define LEDC_FREQUENCY          (1000) // Frequency [Hz]

static void
_buzzer_init()
{
    // prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0,
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
    uint8_t buzzer_cnt = 0;

    while (1) {
        toBuzzerMsg_t msg;
        if (xQueueReceive(ipc->toBuzzerQ, &msg, (TickType_t)(1000 / portTICK_PERIOD_MS)) == pdPASS) {

            switch (msg.dataType) {
                case TO_BUZZER_MSGTYPE_START:
                    ESP_LOGI(TAG, "rx start");
                    buzzer_on = true;
                    buzzer_cnt = 0;
                    break;
                case TO_BUZZER_MSGTYPE_STOP:
                    ESP_LOGI(TAG, "rx stop");
                    buzzer_on = false;
                    break;
            }
        }
        if (buzzer_on) {
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
            gpio_set_level(CONFIG_CALALARM_HAPTIC3V_PIN, buzzer_cnt % 2);
            buzzer_cnt++;
        } else {
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
            gpio_set_level(CONFIG_CALALARM_HAPTIC3V_PIN, 0);
        }
    }
}
