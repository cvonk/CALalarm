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
    char * pushId;
    time_t alarm, start, stop;
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
_set_time(time_t const time)
{
    struct timeval tv = { .tv_sec = time, .tv_usec = 0};
    settimeofday(&tv, NULL);
}

static time_t
_get_time(time_t * time_)
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
        ESP_LOGE(TAG, "JSON err");
        return 0;
    }
    cJSON const *const jsonTime = cJSON_GetObjectItem(jsonRoot, "time");
    if (!jsonTime || jsonTime->type != cJSON_String) {
        ESP_LOGE(TAG, "JSON.time err");
        return 0;
    }
    *time = _str2time(jsonTime->valuestring);

    cJSON const *const jsonPushId = cJSON_GetObjectItem(jsonRoot, "pushId");
    if (!jsonPushId || jsonPushId->type != cJSON_String) {
        ESP_LOGW(TAG, "JSON.pushId is missing (or not a String)");
        free(event->pushId);
        event->pushId = NULL;
    } else {
        free(event->pushId);
        event->pushId = strdup(jsonPushId->valuestring);
    }

    cJSON const *const jsonEvents = cJSON_GetObjectItem(jsonRoot, "events");
    if (!jsonEvents || jsonEvents->type != cJSON_Array) {
        ESP_LOGE(TAG, "JSON.events err");
        return 0;
    }

    if (cJSON_GetArraySize(jsonEvents) > 0) {

        cJSON const *const jsonEvent = cJSON_GetArrayItem(jsonEvents, 0);
        if (!jsonEvent || jsonEvent->type != cJSON_Object) {
            ESP_LOGE(TAG, "JSON.events[0] err");
            return 0;
        }

        cJSON const *const jsonTitleObj = cJSON_GetObjectItem(jsonEvent, "title");
        cJSON const *const jsonAlarmObj = cJSON_GetObjectItem(jsonEvent, "alarm");
        cJSON const *const jsonStartObj = cJSON_GetObjectItem(jsonEvent, "start");
        cJSON const *const jsonStopObj = cJSON_GetObjectItem(jsonEvent, "stop");

        if (!jsonTitleObj || jsonTitleObj->type != cJSON_String) {
            ESP_LOGE(TAG, "JSON.event[0].title err");
            return 0;
        }
        if (!jsonAlarmObj || jsonAlarmObj->type != cJSON_String) {
            ESP_LOGE(TAG, "JSON.event[0].alarm err");
            return 0;
        }
        if (!jsonStartObj || jsonStartObj->type != cJSON_String) {
            ESP_LOGE(TAG, "JSON.event[0].start err");
            return 0;
        }
        if (!jsonStopObj || jsonStopObj->type != cJSON_String) {
            ESP_LOGE(TAG, "JSON.event[0].end err");
            return 0;
        }

        free(event->title);
        event->valid = true;
        event->title = strdup(jsonTitleObj->valuestring);
        event->alarm = _str2time(jsonAlarmObj->valuestring);
        event->start = _str2time(jsonStartObj->valuestring);
        event->stop = _str2time(jsonStopObj->valuestring);
    }

    cJSON_Delete(jsonRoot);
    return len;
}

void
_oled_init(SSD1306_t * const dev)
{
    i2c_master_init(dev, CONFIG_SSD1306_SDA_GPIO, CONFIG_SSD1306_SCL_GPIO, CONFIG_SSD1306_RESET_GPIO);
	ssd1306_init(dev, 128, 32);
	ssd1306_clear_screen(dev, false);
	ssd1306_contrast(dev, 0xff);
}

static void
_oled_set_brightness(SSD1306_t * const dev, int const brightness)
{
    //ESP_LOGI(TAG, "brightness=%u", brightness);
    ssd1306_contrast(dev, brightness);  // 2nd arg is uint8_t
}

static void
_oled_set_status(SSD1306_t * const dev, char const * const src, bool const show_link)
{

    char str[18];
    char const * const link_symb = "\x03\x04";
    snprintf(str, sizeof(str), "%-13s %2s", src, show_link ? link_symb : "");
    ssd1306_clear_line(dev, 3, false);
    ssd1306_display_text(dev, 3, (char *)str, strlen(str), false);
}

static void
_oled_set_ampm(SSD1306_t * const dev, bool const is_pm)
{
    char str[] = "               A";
    if (is_pm) {
        str[15] = 'P';
    }
    ssd1306_display_text(dev, 0, str, strlen(str), false);
    str[15] = 'M';
    ssd1306_display_text(dev, 1, str, strlen(str), false);
}

static void
_oled_update(SSD1306_t * const dev, time_t const now, event_t const * const event)
{
    struct tm nowTm;
    localtime_r(&now, &nowTm);
    uint8_t const hr = (nowTm.tm_hour % 12 == 0) ? 12 : nowTm.tm_hour % 12;

    // show am/pm
    _oled_set_ampm(dev, nowTm.tm_hour >= 12);

    // show time
    char str[7];
    snprintf(str, sizeof(str), "%2d:%02d", hr, nowTm.tm_min);
    ssd1306_display_text_x3(dev, 0, str, strlen(str), false);

    // show status (clears the screen, so we do this first)
    char const * const status = event->valid ? event->title : "no alarm set";
    _oled_set_status(dev, status, event->pushId);
}

void
_buzzer_update(time_t const now, event_t const * const event, ipc_t * ipc)
{
    time_t last = 0;
    struct tm nowTm, alarmTm;
    localtime_r(&now, &nowTm);
    localtime_r(&event->alarm, &alarmTm);

    if (event->valid && nowTm.tm_hour == alarmTm.tm_hour && nowTm.tm_min == alarmTm.tm_min) {
        if (difftime(last, now) > 60) {  //  make sure it doesn't go off multiple times in same minute
            sendToBuzzer(TO_BUZZER_MSGTYPE_START, ipc);
            last = now;
        }
    }
}

void
display_task(void * ipc_void)
{
    _ipc = ipc_void;

    // init OLED display
    SSD1306_t dev;
    _oled_init(&dev);

    event_t event = {};
    time_t now = 0;
    time_t const loopInSec = 10;  // how often the while-loop runs [sec]

    // init A/D converter
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL, ADC_ATTEN_DB_0));  // measures 0.10 to 0.95 Volts

    while (1) {

        toDisplayMsg_t msg;
        if (xQueueReceive(_ipc->toDisplayQ, &msg, (TickType_t)(loopInSec * 1000 / portTICK_PERIOD_MS)) == pdPASS) {

            switch(msg.dataType) {
                case TO_DISPLAY_MSGTYPE_JSON:
                    (void)_json2event(msg.data, &now, &event); // translate from serialized JSON `msg` to `event`
                    _set_time(now);
                    break;
                case TO_DISPLAY_MSGTYPE_STATUS:
                    _oled_set_status(&dev, msg.data, false);
                    break;
            }
            free(msg.data);
        } else {
            _get_time(&now);
        }

        if (now) {  // tod is initialized
            _oled_update(&dev, now, &event);
            _buzzer_update(now, &event, _ipc);
        }
        _oled_set_brightness(&dev, adc1_get_raw(ADC1_CHANNEL));
    }
}