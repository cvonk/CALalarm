# ESP32 Calender Clock

Shows upcoming events for the next 12 hours on LED circle (incorporated in a clock faceplate)

## Syncs with Google script

See `script` directory

## Hardware

Uses "RGB LED Pixel Ring" with 60 WS2812B SMD5050 LEDs, e.g. Chinly 60 LEDs WS2812B 5050 RGB LED Pixel Ring Addressable DC5V ([shop](https://www.amazon.com/gp/product/B0794YVW3T)).

These WS2812B pixels are 5V, and draw about 60 mA each at full brightness.
The Data-in should be driven with 5V +/- 0.5V, but we seem to get away with using the 3.3V output from ESP32 with 470 Ohms in series.  To be safe, you should use a level shifter.

## Configuration

Use `menuconfig` to configure:
- CLOCK_WS2812_PIN: Transmit GPIO# on ESP32 that connects to DATA on the LED circle.
- CLOCK_GAS_CALENDAR_URL: Public URL of the Google Apps script that supplies calendar events as JSON.
- CLOCK_OTA_FIRMWARE_URL: Optional public URL of server that hosts the firmware image (`.bin`).

## WiFi provisioning

Load the "factory.bin" image, and use a the the Espressif BLE Provisioning app from
- [Android](https://play.google.com/store/apps/details?id=com.espressif.provble)
- [iOS](https://apps.apple.com/in/app/esp-ble-provisioning/id1473590141)

This stores the WiFi SSID and password in flash memory and triggers a OTA download of the application itself.

To erase the WiFi credentials, pull `GPIO# 0` down for at least 3 seconds.

## Over-the-air Updates (OTA)

If not using OTA updates, just provision and load the application `.bin` over the serial port.

## Development environment

The software relies on the SDK version "ESP-IDF 4.1-beta2" and matching tools.

For development environment:(GNU toolchain, ESP-IDF, JTAG/OpenOCD, VSCode) refer to https://github.com/cvonk/vscode-starters/tree/master/ESP32

