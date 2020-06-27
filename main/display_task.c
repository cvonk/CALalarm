/**
  * @brief display_task, received JSON, parses and drive LED circle accordingly
  *
  * CLOSED SOURCE, NOT FOR PUBLIC RELEASE
  * (c) Copyright 2020, Sander and Coert Vonk
  * All rights reserved. Use of copyright notice does not imply publication.
  * All text above must be included in any redistribution
 **/

#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/rmt.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <led_strip.h>
#include <cJSON.h>

#include "display_task.h"
#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))
#define ALIGN( type ) __attribute__((aligned( __alignof__( type ) )))
#define PACK( type )  __attribute__((aligned( __alignof__( type ) ), packed ))
#define PACK8  __attribute__((aligned( __alignof__( uint8_t ) ), packed ))

static const char * TAG = "display_task";

typedef struct {
    uint16_t first, last;
    uint16_t hue;
} PACK8 event_t;

enum MAX_EVENTS { MAX_EVENTS = 30 };
typedef event_t events_t[MAX_EVENTS];

static void  // far from ideal, but it gets the job done
_updateEventDetail(char const * const key, uint const value, event_t * const event )
{
    if (strcmp(key, "first") == 0) {
        event->first = value;
    } else if (strcmp(key, "last") == 0) {
        event->last = value;
    } else if (strcmp(key, "hue") == 0) {
        event->hue = value;
    } else {
        ESP_LOGE(TAG, "%s: unrecognized key(%s) ", __func__, key);
    }
}

static uint
_parseJson(char *serializedJson, events_t events, struct tm * time_info)
{
    uint len = 0;

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

    // e.g. 2020-06-25T22:30:16.329Z
    strptime(jsonTime->valuestring, "%Y-%m-%dT%H:%M:%S", time_info);

    cJSON const *const jsonEvents = cJSON_GetObjectItem(jsonRoot, "events");
    if (!jsonEvents || jsonEvents->type != cJSON_Array) {
        ESP_LOGE(TAG, "JSON.events is missing or not an Array");
        return 0;
    }

    enum DETAIL_COUNT { DETAIL_COUNT = 3 };
    static char const * const detailNames[DETAIL_COUNT] = {"first", "last", "hue"};

    event_t * event = events;
    for (int ii = 0; ii < cJSON_GetArraySize(jsonEvents) && ii < MAX_EVENTS; ii++, len++) {

        cJSON const *const jsonEvent = cJSON_GetArrayItem(jsonEvents, ii);
        if (!jsonEvent || jsonEvent->type != cJSON_Object) {
            ESP_LOGE(TAG, "JSON.events[%d] is missing or not an Object", ii);
            return 0;
        }
        for (uint vv = 0; vv < ARRAYSIZE(detailNames); vv++) {
            cJSON const *const jsonObj = cJSON_GetObjectItem(jsonEvent, detailNames[vv]);
            if (!jsonObj || jsonObj->type != cJSON_Number) {
                ESP_LOGE(TAG, "JSON.event[%d].%s is missing or not a Number", ii, detailNames[vv]);
                return 0;
            }
            _updateEventDetail(detailNames[vv], jsonObj->valueint, event);
        }
        if (event->last < event->first || event->last >= CONFIG_CLOCK_WS2812_COUNT) {
            ESP_LOGE(TAG, "JSON.events[%d] illegal start/end value", ii);
            return 0;
        }
        event++;
    }
    cJSON_Delete(jsonRoot);
    return len;
}

// https://en.wikipedia.org/wiki/HSL_and_HSV
static void
_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360;
    uint32_t const rgb_max = v * 2.55f;
    uint32_t const rgb_min = rgb_max * (100 - s) / 100.0f;
    uint32_t const i = h / 60;
    uint32_t const diff = h % 60;
    uint32_t const rgb_adj = (rgb_max - rgb_min) * diff / 60;  // RGB adjustment amount by hue

    switch (i) {
        case 0:
            *r = rgb_max;
            *g = rgb_min + rgb_adj;
            *b = rgb_min;
            break;
        case 1:
            *r = rgb_max - rgb_adj;
            *g = rgb_max;
            *b = rgb_min;
            break;
        case 2:
            *r = rgb_min;
            *g = rgb_max;
            *b = rgb_min + rgb_adj;
            break;
        case 3:
            *r = rgb_min;
            *g = rgb_max - rgb_adj;
            *b = rgb_max;
            break;
        case 4:
            *r = rgb_min + rgb_adj;
            *g = rgb_min;
            *b = rgb_max;
            break;
        default:
            *r = rgb_max;
            *g = rgb_min;
            *b = rgb_max - rgb_adj;
            break;
    }
}

static void
_setTime(struct tm * tm)
{
    struct timeval tv = { .tv_sec = mktime(tm), .tv_usec = 0};
    settimeofday(&tv, NULL);
}

static void
_getTime(struct tm * tm)
{
        time_t now;
        time(&now);
        localtime_r(&now, tm);
}

void
display_task(void * ipc_void)
{
    display_task_ipc_t * ipc = ipc_void;
    QueueHandle_t jsonQ = ipc->jsonQ;

    // install ws2812 driver
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(CONFIG_CLOCK_WS2812_PIN, RMT_CHANNEL_0);
    config.clk_div = 2;  // set counter clock to 40MHz
    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));
    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(CONFIG_CLOCK_WS2812_COUNT, (led_strip_dev_t)config.channel);
    led_strip_t * const strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!strip) { ESP_LOGE(TAG, "Can't install WS2812 driver"); return; }

    event_t * const events = (event_t *)malloc(sizeof(events_t));
    if (!events) { ESP_LOGE(TAG, "No memory for events"); return; }

    ESP_ERROR_CHECK(strip->clear(strip, 100));  // turn off all LEDs

    uint len = 0;
    struct tm tm;
    while (1) {
        char * msg;
        if (xQueueReceive(jsonQ, &msg, (TickType_t)(10000L / portTICK_PERIOD_MS)) == pdPASS) {

            len = _parseJson(msg, events, &tm); // translate from serialized JSON "msg" to C representation "events"
            free(msg);
            _setTime(&tm);
        } else {
            _getTime(&tm);
        }

        uint hourHandPixel = tm.tm_hour%12 * CONFIG_CLOCK_WS2812_COUNT / 12 + tm.tm_min * CONFIG_CLOCK_WS2812_COUNT / (12 * 60);

        for (int pp = 0; pp < CONFIG_CLOCK_WS2812_COUNT; pp++) {
            ESP_ERROR_CHECK(strip->set_pixel(strip, pp, 0, 0, 0));
        }
        event_t const * event = events;
        for (uint ee = 0; ee < len; ee++, event++) {
            for (uint pp = event->first; pp <= event->last; pp++) {
                uint r, g, b;
                uint const s = 100 - 100 * ((pp - hourHandPixel) % 60) / CONFIG_CLOCK_WS2812_COUNT;
                //ESP_LOGI(TAG, "pxl%02d: %03d", pp, s);
                _hsv2rgb(event->hue, 100, s, &r, &g, &b);
                ESP_ERROR_CHECK(strip->set_pixel(strip, pp, r, g, b));
            }
        }
        ESP_ERROR_CHECK(strip->refresh(strip, 100));
    }
}