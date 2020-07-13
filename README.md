# ESP32 Calender Clock

Shows upcoming events for the next 12 hours on a LED circle (incorporated in a clock faceplate)

Goal is to visualize calendar appointments on an analog 12 clock.
h
: add picture

![Glass clock with LED circle behind](media/photo.png)

## Features:

- Shows calendar events in different colors using an LED circle placed behind the faceplate of a clock.
- Remote restart, and version information (using MQTT)
- Can use push notifications to provide timely updates to calendar changes.
- Optional Over-the-air (OTA) updates
- Optional WiFi provisioning using phone app

## Usage

:
:
:


## Hardware

:
: add schematic
:

Parts:
- RGB LED Pixel Ring containing 60 WS2812B SMD5050 adressable LEDs (e.g. "Chinly Addressable 60 Pixel LED Ring").  These WS2812B pixels are 5V, and draw about 60 mA each at full brightness.  If you plan to use it in a bedroom, you probably want less bright LEDS such as WS2812 (without the "B").
- ESP32 board with 4 MByte flash memory, such as [ESP32-DevKitC-VB](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview), LOLIN32 or MELIFE ESP32.
- 5 Volt, 3 Amp power adapter
- Capacitor (470 uF / 16V)
- Resistor
- Analog clock with glass face plate (e.g. Tempus TC6065S Wall Clock with Glass Metal Frame)
- Optional frosting spray (e.g.  Rust-Oleum Frosted Glass Spray Paint)
- Glass glue (e.g. Loctite Glass Glue)

The Data-in of the LED circle should be driven with 5V +/- 0.5V, but we seem to get away with using the 3.3V output from ESP32 with 470 Ohms in series. To be safe, you should use a level shifter.

The software is a symbiosis betweeen a Google Apps Script and firmware running on the ESP32.  The script reads events from your Google Calendar and presents them as JSON to the ESP32 device.

## Pulls calendar data using Google script

See `script\doGet.gs`

:
: explain what it does and how to deploy
:

## Firmware for the ESP32

The ESP32 polls the Google Script and updates the LEDs based on the calendar events.  The functionality is divided into:
- `HTTPS Client Task`, that polls the Google Apps Script for calendar events
- `Display Task`, that uses the Remote Control Module on the ESP32 to drive the LED strip.
- `HTTP POST Server`, to listen to push notifications from Google.
- `OTA Task`, that check for updates upon reboot
- `Reset Task`, when GPIO#0 is low for 3 seconds, it erases the WiFi credentials so that the board can be reprovisioned using your phone.
The different parts communicate using FreeRTOS mailboxes.

### Configuration

In the main directory, copy `Kconfig-example.projbuild` to `Kconfig.projbuild`.  From visual studio code, use `ctrl-shift-p` >> `ESP-IDF: launch gui configuration tool` to change the values.
- `WIFI_SSID`: Name of the WiFi access point to connect to.  Leave blank when provisioning using BLE and a phone app.
- `WIFI_PASSWD`: Password of the WiFi access point to connect to.  Leave blank when provisioning using BLE and a phone app.
- `CLOCK_WS2812_PIN`: Transmit GPIO# on ESP32 that connects to DATA on the LED circle.
- `CLOCK_GAS_CALENDAR_URL`: Public URL of the Google Apps script that supplies calendar events as JSON.
- `CLOCK_MQTT_URL`, Optional URL of the MQTT broker.  For authentication include the username and password, e.g. `mqtt://user:passwd@host.local:1883`
- `OTA_FIRMWARE_URL`, Optional over-the-air URL that hosts the firmware image (`.bin`)
- `RESET_PIN`, Optonal RESET input GPIO number on ESP32 that connects to a pull down switch (default 0)

### Compile

The ESP32 firmware relies on the ESP-IDF SDK version >= 4.1-beta2 and accompanying tools. For more information on how to setup the development environment in VSCode refer to [ESP32 vsCode Starter](https://github.com/cvonk/vscode-starters/tree/master/ESP32).

The firmware loads in two stages:
  1. `factory.bin` configures the WiFi using phone app (not used when WiFi credentials are set using `Kconfig`)
  2. `calclock.bin`, the main application

Compile `calclock.bin` first, by opening the top level folder in Microsft Visual Code and issuing "Terminal >> Run Task >> Build - Build the application".

When using OTA updates, the resulting `build/calclock.bin` should be copied to the OTA file server (`OTA_FIRMWARE_URL`).

### Provision WiFi credentials

If you set your WiFi SSID and password using `Kconfig` you're all set and can simply flash the application and skip the remainder of this section.

To provision the WiFi credentials using a phone app, we need a `factory` app that advertises itself to the phone app.  Open the folder `factory` in Microsft Visual Code and issue "Terminal >> Run Task >> Monitor - Start the monitor".  This will compile and flash the code.  After that it connects to the serial port to show the debug messages.  The serial rate is 115200 baud.

On your phone run the Espressif BLE Provisioning app.
- [Android](https://play.google.com/store/apps/details?id=com.espressif.provble)
- [iOS](https://apps.apple.com/in/app/esp-ble-provisioning/id1473590141)
Scan and connect to the ESP32.  Then specify the WiFi SSID and password.
(You probably have to change `_ble_device_name_prefix` to `PROV_` in `factory\main.c` and change the `config.service_uuid` in `ble_prov.c` to us the mobile apps.)

This stores the WiFi SSID and password in flash memory and triggers a OTA download of the application itself.  Alternatively, don't supply the OTA path and flash the `calclock.bin` application using the serial port.

(To erase the WiFi credentials, pull `GPIO# 0` down for at least 3 seconds.)

### OTA download

Besides connecting to WiFi, one of the first things the code does is check for OTA updates.  Upon completion, the device resets to activate the downloaded code.

We use the term "updates" loosly, because it can also be used to downgrade the firmware.  To determine if the currently running code is different as the code on the server, it compares the project name, version, date and time.  Note that these may not always updated by the SDK.

The OTA server should support either HTTP or HTTPS.  If you WiFi name and password is provisioned using `Kconfig` it will be part of the binary, so you should use a server on your local network.

## Receive push notification from Google calendar changes

To improve response time we have the option of using the [Push Notifications API](https://developers.google.com/calendar/v3/push):

> Allows you to improve the response time of your application. It allows you to eliminate the extra network and compute costs involved with polling resources to determine if they have changed. Whenever a watched resource changes, the Google Calendar API notifies your application.
> To use push notifications, you need to do three things:
> 1. Register the domain of your receiving URL.
> 2. Set up your receiving URL, or "Webhook" callback receiver.
> 3. Set up a notification channel for each resource endpoint you want to watch.

For the first requirement, the Google push notification need to be able to traverse your access router and reach your ESP32 device.  This requires a SSL certificate and a reverse proxy.  Please refer to [Traversing your access router](https://coertvonk.com/sw/embedded/turning-on-the-light-the-hard-way-26806#traverse) for more details.

The second requirement is met by `http_post_server.c`.  Note that the reverse proxy forwards the HTTPS request from Google as HTTP to the device.

The last requirement is met by extending the Google Apps Script as shown in `scripts/push-notifications.gs`

To give the script the nescesary permissions, we need to switch it /default/ GCP to a /standard/ GCP project.  Then give it permissions to the Calendar API and access your domain.
  - in this Google Script > Resources > Cloud Platform project > associate with (new) Cloud project
  - in Google Cloud Console for that (new) project > OAuth consent screen > make internal, add scope = Google Calendar API ../auth/calendar.readonly
  - in Google Developers Console > select your project > domain verification > add domain > ..

## Keeping an eye on it (MQTT)

To easily see what version of the software is running on the device, or what WiFi network it is connected to, the firmware contains a MQTT client.

To control the device, sent a control message to either MQTT topic:
- `calclock/ctrl`, all devices listen to this
- `calclock/ctrl/DEVNAME`, only `DEVAME` listens to this
Here `DEVNAME` is either a programmed device name, such as `esp32-1`, or `esp32_XXXX` where the `XXXX` are the last digits of the MAC address.

Control messages are:
- `who`, can be used for device discovery when sent to the group
- `restart`, to restart the ESP32 (and check for OTA updates)

Messages can be sent to a specific device, or the whole group:
```
mosquitto_pub -h {BROKER} -u {USERNAME} -P {PASSWORD} -t "calclock/ctrl/esp32_0123" -m "restart"
mosquitto_pub -h {BROKER} -u {USERNAME} -P {PASSWORD} -t "calclock/ctrl" -m "who"
```

Replies to control messages and calendar events are reported using MQTT topic `calclock/data/DEVNAME`.  E.g. to listen to all devices:
```
mosquitto_sub -h {BROKER} -u {USERNAME} -P {PASSWORD} -t "calclock/data/#" -v
```
where `#` is the MQTT wildcard character.

## License

Copyright (c) 2020 Sander and Coert Vonk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.