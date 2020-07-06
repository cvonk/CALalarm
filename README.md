# ESP32 Calender Clock

Shows upcoming events for the next 12 hours on a LED circle (incorporated in a clock faceplate)

## Hardware

Requires an ESP32 board with 4 MByte flash memory, e.g. [ESP32-DevKitC-VB](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview)

Uses "RGB LED Pixel Ring" with 60 WS2812B SMD5050 LEDs, e.g. Chinly 60 LEDs WS2812B 5050 RGB LED Pixel Ring Addressable DC5V ([shop](https://www.amazon.com/gp/product/B0794YVW3T)).

These WS2812B pixels are 5V, and draw about 60 mA each at full brightness.
The Data-in should be driven with 5V +/- 0.5V, but we seem to get away with using the 3.3V output from ESP32 with 470 Ohms in series.  To be safe, you should use a level shifter.

## Software

Symbiosis betweeen a Google Script and firmware running on the ESP32.

### Syncs with Google script

See `script` directory

### ESP32

The software relies on the ESP-IDF SDK version >= 4.1-beta2 and accompanying tools.

For development environment:(GNU toolchain, ESP-IDF, JTAG/OpenOCD, VSCode) refer to https://github.com/cvonk/vscode-starters/tree/master/ESP32

Copy `Kconfig-example.projbuild` to `Kconfig.projbuild`, and delete `sdkconfig` so the build system will recreate it.

Use `menuconfig` to configure:
- `CLOCK_WS2812_PIN`: Transmit GPIO# on ESP32 that connects to DATA on the LED circle.
- `CLOCK_GAS_CALENDAR_URL`: Public URL of the Google Apps script that supplies calendar events as JSON.
- `CLOCK_OTA_FIRMWARE_URL`: Optional public URL of server that hosts the firmware image (`.bin`).
- `CLOCK_MQTT_URL`, URL of the MQTT broker.  For authentication include the username and password, e.g. `mqtt://user:passwd@host.local:1883`
- `CLOCK_MQTT_TOPIC`, MQTT topic for iBeacons received over BLE
- `CLOCK_MQTT_CTRL_TOPIC`, MQTT topic for control messages
- `RESET_PIN`, RESET input GPIO number on ESP32 that connects to a pull down switch (default 0)
- `RESET_SECONDS`, Number of seconds that RESET needs to be hold low before erasing WiFi credentials (default 3)
- `OTA_FIRMWARE_URL`, Optional over-the-air URL that hosts the firmware image (`.bin`)
- `OTA_RECV_TIMEOUT`, Over-the-air receive timeout (default 5 sec)

Load the "factory.bin" image using UART, and use a the the Espressif BLE Provisioning app from
- [Android](https://play.google.com/store/apps/details?id=com.espressif.provble)
- [iOS](https://apps.apple.com/in/app/esp-ble-provisioning/id1473590141)
You probably have to change `_ble_device_name_prefix` to `PROV_` in `factory\main.c` and change the `config.service_uuid` in `ble_prov.c` to us the mobile apps.

This stores the WiFi SSID and password in flash memory and triggers a OTA download of the application itself.  Aternatively, don't supply the OTA path and load the "calendarclock.bin" application using UART.

To erase the WiFi credentials, pull `GPIO# 0` down for at least 3 seconds.

### Over-the-air Updates (OTA)

If not using OTA updates, just provision and load the application `.bin` over the serial port.

OTA Updates can be used to upgrade or downgrade the code.  To determine if the currently running code is different as the code on the server, it compares the project name, version, date and time.  Note that these may not always updated by the SDK.

## Usage

:
:
:

### MQTT

To control the device, sent a control message to either MQTT topic:
- `calendarclock/ctrl`, all devices listen to this
- `calendarclock/ctrl/DEVNAME`, where `DEVAME` is the device specific name, e.g. `clock_0123`

Control messages are:
- `restart`, to restart the ESP32 (and check for OTA updates)
- `who`, can be used for device discovery when sent to the group

Messages can be sent to a specific device, or the whole group:
```
mosquitto_pub -h {MQTTADDR} -u {USERNAME} -P {PASSWORD} -t "calendarclock/ctrl/clock_0123" -m "who"
mosquitto_pub -h {MQTTADDR} -u {USERNAME} -P {PASSWORD} -t "calendarclock/ctrl" -m "who"
```

Results are reported on MQTT topic:
- `calendarclock/data/DEVNAME`, where `DEVNAME` is either:
   - the device name, such as `esp32-1`, or
   - `esp32_XXXX` where the `XXXX` are the last digits of the BLE hardware address.

To listen to all scan results, use e.g.
```
mosquitto_sub -h {MQTTADDR} -u {USERNAME} -P {PASSWORD} -t "calendarclock/data/#" -v
```
where `#` is a wildcard character.

## License

Copyright (c) 2020 Sander Vonk

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