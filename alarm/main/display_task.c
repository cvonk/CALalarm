/**
  * @brief display_task, received JSON, parses and drive LED circle accordingly
  *
  * CLOSED SOURCE, NOT FOR PUBLIC RELEASE
  * (c) Copyright 2020, Sander and Coert Vonk
  * All rights reserved. Use of copyright notice does not imply publication.
  * All text above must be included in any redistribution
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

#include "display_task.h"
#include "ipc/ipc.h"
#include "ssd1306.h"
#include "font8x8_basic.h"

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
_update_oled(SSD1306_t * const dev)
{
	ssd1306_display_text(dev, 0, "SSD1306 128x32", 14, false);
	ssd1306_display_text(dev, 1, "Hello World!!", 13, false);
	ssd1306_clear_line(dev, 2, true);
	ssd1306_clear_line(dev, 3, true);
	ssd1306_display_text(dev, 2, "SSD1306 128x32", 14, true);
	ssd1306_display_text(dev, 3, "Hello World!!", 13, true);

#if 0
	// Fade Out
	for(int contrast=0xff;contrast>0;contrast=contrast-0x20) {
		ssd1306_contrast(dev, contrast);
		vTaskDelay(40);
	}
#endif

/*
    NtpTime::ntptime_t const time = ntpTime.getTime();
    GoogleCalEvent::alarm_t const alarm = googleCalEvent.getAlarm();

    oled.clearDisplay();
    oled.setCursor(0, 0);

    if (time.status != NtpTime::timeNeedsSync) {
        oled.drawBitmap(SSD1306_LCDWIDTH - 16, 0, timeSyncBitmap, 16, 8, WHITE);
    }

    if (alarm.status != GoogleCalEvent::alarmNeedsSync) {
        oled.drawBitmap(SSD1306_LCDWIDTH - 16, 8, alarmSyncBitmap, 16, 8, WHITE);
    }

    if (time.status == NtpTime::timeNotSet) {
        oled.print("Waiting for time");
    } else {
        oled.setTextSize(3);
        if (time.hour12 < 10) {
            oled.print(" ");
        }
        oled.print(time.hour12); oled.print(":");
        if (time.minute < 10) {
            oled.print("0");
        }
        oled.print(time.minute);
        oled.setTextSize(1);
        oled.print(time.hour12pm ? "PM" : "AM");
    }

    int const alarmHour = alarm.alarmTime / 60;
    int const alarmMinute = alarm.alarmTime % 60;

    oled.setCursor(0, SSD1306_LCDHEIGHT - 8);
    if (alarm.status == GoogleCalEvent::alarmNotSet) {
        oled.print("No alarm set");
    } else {
        oled.print(alarmHour); oled.print(":");
        if (alarmMinute < 10) {
            oled.print("0");
        }
        oled.print(alarmMinute);
        oled.print(" ");
        oled.print(alarm.title);
    }

    int const light = analogRead(0);
    oled.dim(light < 50);

    if (time.status != NtpTime::timeNotSet &&
        alarm.status != GoogleCalEvent::alarmNotSet &&
        alarmHour == time.hour24 && alarmMinute == time.minute && time.second == 0 && buzzer.isOff())
    {
        buzzer.start();
    }

    oled.display();  // write the cached commands to display
*/
}

void
display_task(void * ipc_void)
{
    _ipc = ipc_void;

    event_t * const event = (event_t *) malloc(sizeof(event_t));
    assert(event);

    SSD1306_t dev;
    _init_oled(&dev);


/*
    buzzer.begin();

    pinMode(BUTTON_B_GPIO, INPUT_PULLUP);
    pinMode(BUTTON_C_GPIO, INPUT_PULLUP);
    pinMode(HAPTIC_GPIO, OUTPUT);        
*/

    _update_oled(&dev);

    time_t now;
    time_t const loopInSec = 60;  // how often the while-loop runs [msec]
    while (1) {

        // if there was an calendar update then apply it

        toDisplayMsg_t msg;
        if (xQueueReceive(_ipc->toDisplayQ, &msg, (TickType_t)(loopInSec * 1000 / portTICK_PERIOD_MS)) == pdPASS) {
            (void)_json2event(msg.data, &now, event); // translate from serialized JSON "msg" to C representation "events"
            free(msg.data);
            ESP_LOGI(TAG, "Update");
            _setTime(now);
        } else {
            _getTime(&now);
        }
    }
}