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
_test_oled()
{
	SSD1306_t dev;
	int center, top, bottom;
	char lineChar[20];

#if CONFIG_SSD1306_I2C_INTERFACE
	ESP_LOGI(TAG, "INTERFACE is i2c");
	ESP_LOGI(TAG, "CONFIG_SSD1306_SDA_GPIO=%d",CONFIG_SSD1306_SDA_GPIO);
	ESP_LOGI(TAG, "CONFIG_SSD1306_SCL_GPIO=%d",CONFIG_SSD1306_SCL_GPIO);
	ESP_LOGI(TAG, "CONFIG_SSD1306_RESET_GPIO=%d",CONFIG_SSD1306_RESET_GPIO);
	i2c_master_init(&dev, CONFIG_SSD1306_SDA_GPIO, CONFIG_SSD1306_SCL_GPIO, CONFIG_SSD1306_RESET_GPIO);
#endif // CONFIG_SSD1306_I2C_INTERFACE

#if CONFIG_SSD1306_SPI_INTERFACE
	ESP_LOGI(TAG, "INTERFACE is SPI");
	ESP_LOGI(TAG, "CONFIG_SSD1306_MOSI_GPIO=%d",CONFIG_SSD1306_MOSI_GPIO);
	ESP_LOGI(TAG, "CONFIG_SSD1306_SCLK_GPIO=%d",CONFIG_SSD1306_SCLK_GPIO);
	ESP_LOGI(TAG, "CONFIG_SSD1306_CS_GPIO=%d",CONFIG_SSD1306_CS_GPIO);
	ESP_LOGI(TAG, "CONFIG_SSD1306_DC_GPIO=%d",CONFIG_SSD1306_DC_GPIO);
	ESP_LOGI(TAG, "CONFIG_SSD1306_RESET_GPIO=%d",CONFIG_SSD1306_RESET_GPIO);
	spi_master_init(&dev, CONFIG_SSD1306_MOSI_GPIO, CONFIG_SSD1306_SCLK_GPIO, CONFIG_SSD1306_CS_GPIO, CONFIG_SSD1306_DC_GPIO, CONFIG_SSD1306_RESET_GPIO);
#endif // CONFIG_SSD1306_SPI_INTERFACE

#if CONFIG_SSD1306_FLIP
	dev._flip = true;
	ESP_LOGW(tag, "Flip upside down");
#endif

#if CONFIG_SSD1306_128x64
	ESP_LOGI(TAG, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);
#endif // CONFIG_SSD1306_128x64
#if CONFIG_SSD1306_128x32
	ESP_LOGI(TAG, "Panel is 128x32");
	ssd1306_init(&dev, 128, 32);
#endif // CONFIG_SSD1306_128x32

	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);

#if CONFIG_SSD1306_128x64
	top = 2;
	center = 3;
	bottom = 8;
	ssd1306_display_text(&dev, 0, "SSD1306 128x64", 14, false);
	ssd1306_display_text(&dev, 1, "ABCDEFGHIJKLMNOP", 16, false);
	ssd1306_display_text(&dev, 2, "abcdefghijklmnop",16, false);
	ssd1306_display_text(&dev, 3, "Hello World!!", 13, false);
	ssd1306_clear_line(&dev, 4, true);
	ssd1306_clear_line(&dev, 5, true);
	ssd1306_clear_line(&dev, 6, true);
	ssd1306_clear_line(&dev, 7, true);
	ssd1306_display_text(&dev, 4, "SSD1306 128x64", 14, true);
	ssd1306_display_text(&dev, 5, "ABCDEFGHIJKLMNOP", 16, true);
	ssd1306_display_text(&dev, 6, "abcdefghijklmnop",16, true);
	ssd1306_display_text(&dev, 7, "Hello World!!", 13, true);
#elif CONFIG_SSD1306_128x32
	top = 1;
	center = 1;
	bottom = 4;
	ssd1306_display_text(&dev, 0, "SSD1306 128x32", 14, false);
	ssd1306_display_text(&dev, 1, "Hello World!!", 13, false);
	ssd1306_clear_line(&dev, 2, true);
	ssd1306_clear_line(&dev, 3, true);
	ssd1306_display_text(&dev, 2, "SSD1306 128x32", 14, true);
	ssd1306_display_text(&dev, 3, "Hello World!!", 13, true);
#elif
#  error "NO PANNEL SELECTED"
#endif // CONFIG_SSD1306_128x32

	vTaskDelay(3000 / portTICK_PERIOD_MS);
	
	// Display Count Down
	uint8_t image[24];
	memset(image, 0, sizeof(image));
	ssd1306_display_image(&dev, top, (6*8-1), image, sizeof(image));
	ssd1306_display_image(&dev, top+1, (6*8-1), image, sizeof(image));
	ssd1306_display_image(&dev, top+2, (6*8-1), image, sizeof(image));
	for(int font=0x39;font>0x30;font--) {
		memset(image, 0, sizeof(image));
		ssd1306_display_image(&dev, top+1, (7*8-1), image, 8);
		memcpy(image, font8x8_basic_tr[font], 8);
		if (dev._flip) ssd1306_flip(image, 8);
		ssd1306_display_image(&dev, top+1, (7*8-1), image, 8);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	
	// Scroll Up
	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text(&dev, 0, "---Scroll  UP---", 16, true);
	//ssd1306_software_scroll(&dev, 7, 1);
	ssd1306_software_scroll(&dev, (dev._pages - 1), 1);
	for (int line=0;line<bottom+10;line++) {
		lineChar[0] = 0x01;
		sprintf(&lineChar[1], " Line %02d", line);
		ssd1306_scroll_text(&dev, lineChar, strlen(lineChar), false);
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
	vTaskDelay(3000 / portTICK_PERIOD_MS);
	
	// Scroll Down
	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text(&dev, 0, "--Scroll  DOWN--", 16, true);
	//ssd1306_software_scroll(&dev, 1, 7);
	ssd1306_software_scroll(&dev, 1, (dev._pages - 1) );
	for (int line=0;line<bottom+10;line++) {
		lineChar[0] = 0x02;
		sprintf(&lineChar[1], " Line %02d", line);
		ssd1306_scroll_text(&dev, lineChar, strlen(lineChar), false);
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
	vTaskDelay(3000 / portTICK_PERIOD_MS);

	// Page Down
	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text(&dev, 0, "---Page	DOWN---", 16, true);
	ssd1306_software_scroll(&dev, 1, (dev._pages-1) );
	for (int line=0;line<bottom+10;line++) {
		//if ( (line % 7) == 0) ssd1306_scroll_clear(&dev);
		if ( (line % (dev._pages-1)) == 0) ssd1306_scroll_clear(&dev);
		lineChar[0] = 0x02;
		sprintf(&lineChar[1], " Line %02d", line);
		ssd1306_scroll_text(&dev, lineChar, strlen(lineChar), false);
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
	vTaskDelay(3000 / portTICK_PERIOD_MS);

	// Horizontal Scroll
	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text(&dev, center, "Horizontal", 10, false);
	ssd1306_hardware_scroll(&dev, SCROLL_RIGHT);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	ssd1306_hardware_scroll(&dev, SCROLL_LEFT);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	ssd1306_hardware_scroll(&dev, SCROLL_STOP);
	
	// Vertical Scroll
	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text(&dev, center, "Vertical", 8, false);
	ssd1306_hardware_scroll(&dev, SCROLL_DOWN);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	ssd1306_hardware_scroll(&dev, SCROLL_UP);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	ssd1306_hardware_scroll(&dev, SCROLL_STOP);
	
	// Invert
	ssd1306_clear_screen(&dev, true);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text(&dev, center, "  Good Bye!!", 12, true);
	vTaskDelay(5000 / portTICK_PERIOD_MS);


	// Fade Out
	ssd1306_fadeout(&dev);
	
#if 0
	// Fade Out
	for(int contrast=0xff;contrast>0;contrast=contrast-0x20) {
		ssd1306_contrast(&dev, contrast);
		vTaskDelay(40);
	}
#endif
}

void
_init_oled()
{
/*
  	oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // I2C addr 0x3C (for the 128x32)
	oled.display();  // displays the splash screen
	oled.setTextSize(1);
	oled.setTextColor(WHITE);
  
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		oled.setCursor(0, 0);
		oled.clearDisplay();
		oled.print("Connecting to WiFi ");
		oled.print(progress[ii++ % sizeof(progress)]);
		oled.display();
		Serial.print(".");
*/

}


void
_update_oled() 
{
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

/*
    _init_oled();
    buzzer.begin();

    pinMode(BUTTON_B_GPIO, INPUT_PULLUP);
    pinMode(BUTTON_C_GPIO, INPUT_PULLUP);
    pinMode(HAPTIC_GPIO, OUTPUT);        
*/

    _test_oled();

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