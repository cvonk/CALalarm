/**
 * @brief display_task, received JSON, parses and drive LED circle accordingly
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
 **/

#include <sdkconfig.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/rmt.h>
#include <cJSON.h>
#include <driver/adc.h>
#include <soc/adc_channel.h>

#include "display_task.h"

#include "ipc/ipc.h"
#include "ssd1306.h"
#include "font8x8_basic.h"

// ADC Channels (ACD1 channel 6 is GPIO26 on ESP32)
#if CONFIG_IDF_TARGET_ESP32
# if (CONFIG_CALALARM_PHOTO3V_PIN == 34)
#   define ADC1_CHANNEL  (ADC1_GPIO34_CHANNEL)
# else
#  error "unsupported GPIO"
# endif
#else 
# error "unsupported target"
#endif

static char const * const TAG = "display_task";
static ipc_t * _ipc;

typedef struct {
    bool valid;
    char * title;
    time_t start, end;
} PACK8 event_t;

void
sendToDisplay(toDisplayMsgType_t const dataType, char const * const data, ipc_t const * const ipc)
{
    toDisplayMsg_t msg = {
        .dataType = dataType,
        .data = strdup(data)
    };
    assert(msg.data);
    if (xQueueSendToBack(ipc->toDisplayQ, &msg, 0) != pdPASS) {
        ESP_LOGE(TAG, "toDisplayQ full");
        free(msg.data);
    }
}

static time_t
_str2time(char * str) {  // e.g. 2020-06-25T22:30:16.329Z
    struct tm tm;
    if (strptime(str, "%Y-%m-%d %H:%M:%S", &tm) == NULL) {
        ESP_LOGE(TAG, "can't parse time (%s)", str);
        return 0;
    }
    return mktime(&tm);
}

static void
_setTime(time_t const time)
{
    struct timeval tv = { .tv_sec = time, .tv_usec = 0};
    settimeofday(&tv, NULL);
}

static time_t
_getTime(time_t * time_)
{
    return time(time_);
}

static uint
_json2event(char const * const serializedJson, time_t * const time, event_t * const event)
{
    event->valid = false;

    uint len = 0;
    if (serializedJson[0] != '{' || serializedJson[strlen(serializedJson)-1] != '}') {
        ESP_LOGW(TAG, "first/last JSON chr ('%c' '%c'", serializedJson[0], serializedJson[strlen(serializedJson)-1]);
        return len;
    }
    cJSON * const jsonRoot = cJSON_Parse(serializedJson);
    if (jsonRoot->type != cJSON_Object) {
        ESP_LOGE(TAG, "JSON root is not an Object");
        return 0;
    }
    cJSON const *const jsonTime = cJSON_GetObjectItem(jsonRoot, "time");
    if (!jsonTime || jsonTime->type != cJSON_String) {
        ESP_LOGE(TAG, "JSON.time is missing or not an String");
        return 0;
    }
    *time = _str2time(jsonTime->valuestring);

    cJSON const *const jsonEvents = cJSON_GetObjectItem(jsonRoot, "events");
    if (!jsonEvents || jsonEvents->type != cJSON_Array) {
        ESP_LOGE(TAG, "JSON.events is missing or not an Array");
        return 0;
    }

    if (cJSON_GetArraySize(jsonEvents) > 0) {

        cJSON const *const jsonEvent = cJSON_GetArrayItem(jsonEvents, 0);
        if (!jsonEvent || jsonEvent->type != cJSON_Object) {
            ESP_LOGE(TAG, "JSON.events[0] is missing or not an Object");
            return 0;
        }

        cJSON const *const jsonTitleObj = cJSON_GetObjectItem(jsonEvent, "title");
        cJSON const *const jsonStartObj = cJSON_GetObjectItem(jsonEvent, "start");
        cJSON const *const jsonEndObj = cJSON_GetObjectItem(jsonEvent, "end");

        if (!jsonTitleObj || jsonTitleObj->type != cJSON_String) {
            ESP_LOGE(TAG, "JSON.event[0].title is missing or not a String");
            return 0;
        }
        if (!jsonStartObj || jsonStartObj->type != cJSON_String) {
            ESP_LOGE(TAG, "JSON.event[0].start is missing or not a String");
            return 0;
        }
        if (!jsonEndObj || jsonEndObj->type != cJSON_String) {
            ESP_LOGE(TAG, "JSON.event[0].end is missing or not a String");
            return 0;
        }

        event->valid = true;
        event->title = strdup(jsonTitleObj->valuestring);
        event->start = _str2time(jsonStartObj->valuestring);
        event->end = _str2time(jsonEndObj->valuestring);
    }

    cJSON_Delete(jsonRoot);
    return len;
}

void
_init_oled(SSD1306_t * const dev)
{
    i2c_master_init(dev, CONFIG_SSD1306_SDA_GPIO, CONFIG_SSD1306_SCL_GPIO, CONFIG_SSD1306_RESET_GPIO);
	ssd1306_init(dev, 128, 32);
	ssd1306_clear_screen(dev, false);
	ssd1306_contrast(dev, 0xff);
}

void
_update_oled(SSD1306_t * const dev, time_t const now, event_t const * const event)
{
    struct tm nowTm;
    localtime_r(&now, &nowTm);

    char str[15];
    snprintf(str, sizeof(str), "%02d:%02d", nowTm.tm_hour, nowTm.tm_min);
    ssd1306_clear_line(dev, 0, false);
    ssd1306_clear_line(dev, 1, false);
    ssd1306_clear_line(dev, 2, false);
	ssd1306_display_text_x3(dev, 0, str, strlen(str), false);

    ssd1306_clear_line(dev, 3, false);
    if (event->valid) {
        struct tm startTm;
        localtime_r(&event->start, &startTm);
        snprintf(str, sizeof(str), "%02d:%02d %s", startTm.tm_hour, startTm.tm_min, event->title);
    	ssd1306_display_text(dev, 3, str, strlen(str), false);
    } else {
        char const no_alarm[] = "No alarm set";
    	ssd1306_display_text(dev, 3, no_alarm, strlen(no_alarm), false);
    }
}

void
_update_buzzer(time_t const now, event_t const * const event, ipc_t * ipc)
{
    struct tm nowTm, startTm;
    localtime_r(&now, &nowTm);
    localtime_r(&event->start, &startTm);

    if (event->valid && nowTm.tm_hour == startTm.tm_hour && nowTm.tm_min == startTm.tm_min) {
        sendToBuzzer(TO_BUZZER_MSGTYPE_START, ipc);
    }
}

void
display_task(void * ipc_void)
{
    _ipc = ipc_void;

    // init OLED display
    SSD1306_t dev;
    _init_oled(&dev);

    event_t event = {};
    time_t now;
    time_t const loopInSec = 10;  // how often the while-loop runs [sec]

    // init A/D converter
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL, ADC_ATTEN_DB_0));  // measures 0.10 to 0.95 Volts

    while (1) {

        int const brightness = adc1_get_raw(ADC1_CHANNEL);
        ESP_LOGI(TAG, "brightness=%u", brightness);

        toDisplayMsg_t msg;
        if (xQueueReceive(_ipc->toDisplayQ, &msg, (TickType_t)(loopInSec * 1000 / portTICK_PERIOD_MS)) == pdPASS) {

            (void)_json2event(msg.data, &now, &event); // translate from serialized JSON `msg` to `event`
            free(msg.data);

            ESP_LOGI(TAG, "Update tod");
            _setTime(now);
        } else {
            _getTime(&now);
        }

        _update_oled(&dev, now, &event);
        _update_buzzer(now, &event, _ipc);
    }
}