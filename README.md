# ESP32 Calendar Clock

[![GitHub Discussions](https://img.shields.io/github/discussions/sandervonk/ESP32_Calendar-clock)](https://github.com/sandervonk/ESP32_Calendar-clock/discussions)
![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/sandervonk/ESP32_Calendar-clock?include_prereleases&logo=DocuSign&logoColor=%23fff)
![GitHub](https://img.shields.io/github/license/sandervonk/ESP32_Calendar-clock)

This program runs on an Espressif EPS32 microcontroller and shows upcoming events on a LED circle incorporated in a clock faceplate.

It can be used as anything from a decorative/interactive art piece to a normal clock that can remind you of upcoming appointments in a fun and cleanly designed way.

I used this to remind me of upcoming appointments now that school moved online.

![Backward facing glass clock with LED circle](media/forward_facing_250px.jpg)
![Forward facing glass clock with LED circle](media/backward_facing_250px.jpg)

## Features:

- Shows calendar events in different colors using an LED circle placed behind the faceplate of a clock.
- Optional push notifications for timely updates to calendar changes.
- Optional Over-the-air (OTA) updates
- Optional WiFi provisioning using phone app
- Optional remote restart, and version information (using MQTT)
- Optional core dump over MQTT to aid debugging

## Google Apps Script

The ESP32 microcontroller calls a [Google Apps Script](https://developers.google.com/apps-script/guides/web) that retrieve a list of upcoming events from your calendar and returns them as a JSON object.  As we see later, the ESP32 will update the LEDs based on this JSON object.

To create the Webapp:
  - Create a new project on [script.google.com](https://script.google.com);
  - Copy and paste the code from `script\Code.gs`
  - Resources > Advanced Google Services > enable the `Calendar API`
  - File > Manage Versions, Save New Version
  - Publish > Deploy as web app
      - version = select the version you just saved
      - execute as = `Me`
      - who has access = `anyone`, even anonymous (make sure you understand what the script does!)
      - You will get a warning, because the app has not been verified by Google
      - Once you clock `Deploy`, it presents you with two URLs
          - one that ends in `/exec`, the published version, based on the version you chose.
          - one that ends in `/dev`, the most recent saved code, intended for quick testing during development.
      - Copy the `main/Kconfig.example` to `main/Kconfig` and copy the URL that ends in `/exec` to `main/Kconfig` under `CLOCK_GAS_CALENDAR_URL`.

The script is more involved as needed because also supports /Push Notifications/, as described at the end of this document.

## ESP32 Firmware

The functionality is divided into:
- `HTTPS Client Task`, that polls the Google Apps Script for calendar events
- `Display Task`, that uses the Remote Control Module on the ESP32 to drive the LED strip.
- `HTTP POST Server`, to listen to push notifications from Google.
- `OTA Task`, that check for updates upon reboot
- `Reset Task`, when GPIO#0 is low for 3 seconds, it erases the WiFi credentials so that the board can be re-provisioned using your phone.

The different parts communicate using FreeRTOS mailboxes.

### Clone

The Git repository contains submodules.  To clone these submodules as well, use the `--recursive` flag.
```
git clone --recursive https://github.com/sandervonk/ESP32_Calendar-clock
cd ESP32_Calendar-clock
git submodule init
copy main\Kconfig-example main\Kconfig
copy components\ota_update_task\Kconfig.example components\ota_update_task\Kconfig
```

Update using
```bash
git pull
git submodule update --recursive --remote
```

### Bill of Materials

Two approaches can be used when deciding the look of you clock. One of which is to have the ring fully visible, with the other being to use the leds as an artsy-backlight.

![Internals of Backward facing glass clock with LED circle](media/forward_facing_int_250px.jpg)
![Internals of Forward facing glass clock with LED circle](media/backward_facing_int_250px.jpg)

- RGB LED Pixel Ring containing 60 WS2812B SMD5050 addressable LEDs (e.g. "Chinly Addressable 60 Pixel LED Ring").  These WS2812B pixels are 5V, and draw about 60 mA each at full brightness.  If you plan to use it in a bedroom, you probably want less bright LEDS such as WS2812 (without the "B").
- ESP32 board with 4 MByte flash memory, such as [ESP32-DevKitC-VB](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview), LOLIN32 or MELIFE ESP32.
- 5 Volt, 3 Amp power adapter
- Capacitor (470 uF / 16V)
- Resistor (470 Ohm)
- Analog clock with glass face plate (e.g. Tempus TC6065S Wall Clock with Glass Metal Frame or a Selko 11" Brushed Metal Wall Clock)
- Optional frosting spray (e.g.  Rust-Oleum Frosted Glass Spray Paint)
- Glass glue (e.g. Loctite Glass Glue)
- Molex 2 Pin Connectors

The Data-in of the LED circle should be driven with 5V +/- 0.5V, but we seem to get away with using the 3.3V output from ESP32 with 470 Ohms in series. To be safe, you should use a level shifter.

The software is a symbiosis between a Google Apps Script and firmware running on the ESP32.  The script reads events from your Google Calendar and presents them as JSON to the ESP32 device.

### System Development Kit (SDK)

The software relies on the cutting edge (master) of the ESP-IDF System Development Kit (SDK), currently `v4.3-dev-472-gcf056a7d0`.  Install this SDK according to its [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/), or follow a third party guide such as [ESP32 + VSCode](https://github.com/cvonk/vscode-starters/blob/master/ESP32/README.md).

### Configuration

In the main directory, copy `Kconfig-example.projbuild` to `Kconfig.projbuild`.  From visual studio code, use `ctrl-shift-p` >> `ESP-IDF: launch gui configuration tool` to change the values.
- `WIFI_SSID`: Name of the WiFi access point to connect to.  Leave blank when provisioning using BLE and a phone app.
- `WIFI_PASSWD`: Password of the WiFi access point to connect to.  Leave blank when provisioning using BLE and a phone app.
- `CLOCK_WS2812_PIN`: Transmit GPIO# on ESP32 that connects to DATA on the LED circle.
- `CLOCK_GAS_CALENDAR_URL`: Public URL of the Google Apps script that supplies calendar events as JSON.
- `CLOCK_MQTT_URL`, Optional URL of the MQTT broker.  For authentication include the username and password, e.g. `mqtt://user:passwd@host.local:1883`
- `OTA_FIRMWARE_URL`, Optional over-the-air URL that hosts the firmware image (`.bin`)
- `RESET_PIN`, Optonal RESET input GPIO number on ESP32 that connects to a pull down switch (default 0)

### Configure

1. Either update the defaults in the `Kconfig.projbuild` file directly, or use the "ESP-IDF: launch gui configuration tool".
2. Delete `sdkconfig` so the build system will recreate it.

### Build

The firmware loads in two stages:
  1. `factory.bin` configures the WiFi using phone app (except when WiFi credentials are hard code in `Kconfig.projbuild`)
  2. `calclock.bin`, the main application

Compile `calclock.bin` first, by opening the folder in Microsoft Visual Code and starting the build (`ctrl-e b`).

When using OTA updates, the resulting `build/calclock.bin` should be copied to the OTA Update file server (pointed to by `OTA_UPDATE_FIRMWARE_URL`).

### Provision WiFi credentials

If you set your WiFi SSID and password using `Kconfig.projbuild` you're all set and can simply flash the application and skip the remainder of this section.

To provision the WiFi credentials using a phone app, this `factory` app advertises itself to the phone app.  The Espressif BLE Provisioning app is available as source code or from the [Android](https://play.google.com/store/apps/details?id=com.espressif.provble) and [iOS](https://apps.apple.com/in/app/esp-ble-provisioning/id1473590141) app stores.

On the ESP32 side, we need a `factory` app that advertises itself to the phone app.  Open the folder `factory` in Microsft Visual Code and start a debug session (`ctrl-e d`).  This will compile and flash the code, and connects to the serial port to show the debug messages.

Using the Espressif BLE Provisioning phone app, `scan` and connect to the ESP32.  use the app to specify the WiFi SSID and password. Depending on the version of the app, you may first have to change `_ble_device_name_prefix` to `PROV_` in `test/main/main.c`, and change the `config.service_uuid` in `ble_prov.c`.
(Personally, I still use an older customized version of the app.)

This stores the WiFi SSID and password in flash memory and triggers a OTA download of the application itself.  IAlternatively, don't supply the OTA path and flash the `calclock.bin` application using the serial port.

To erase the WiFi credentials, pull `GPIO# 0` down for at least 3 seconds and release.  This I/O is often connected to a button labeled `BOOT` or `RESET`.

### OTA download

Besides connecting to WiFi, one of the first things the application does is check for OTA updates.  Upon completion, the device resets to activate the downloaded code.

## Using the application

To easily see what version of the software is running on the device, or what WiFi network it is connected to, the firmware contains a MQTT client.
> MQTT stands for MQ Telemetry Transport. It is a publish/subscribe, extremely simple and lightweight messaging protocol, designed for constrained devices and low-bandwidth, high-latency or unreliable networks. [FAQ](https://mqtt.org/faq)

One the clock is on the wall, we can still keep a finger on the pulse using
  - Remote restart, and version information (using MQTT)
  - Core dump over MQTT to aid debugging

Control messages are:
- `who`, can be used for device discovery when sent to the group topic
- `restart`, to restart the ESP32 (and check for OTA updates)
- `int N`, to change scan/adv interval to N milliseconds
- `mode`, to report the current scan/adv mode and interval

Control messages can be sent:
- `calclock/ctrl`, a group topic that all devices listen to, or
- `calclock/ctrl/DEVNAME`, only `DEVNAME` listens to this topic.

Here `DEVNAME` is either a programmed device name, such as `esp32-1`, or `esp32_XXXX` where the `XXXX` are the last digits of the MAC address.  Device names are assigned based on the BLE MAC address in `main/main.c`.

Messages can be sent to a specific device, or the whole group:
```
mosquitto_pub -h {BROKER} -u {USERNAME} -P {PASSWORD} -t "calclock/ctrl/esp-1" -m "who"
mosquitto_pub -h {BROKER} -u {USERNAME} -P {PASSWORD} -t "calclock/ctrl" -m "who"
```

### Replies to Control msgs, debug output and coredumps

Both replies to control messages and unsolicited data such as debug output and coredumps are reported using MQTT topic `calclock/data/SUBTOPIC/DEVNAME`.

Subtopics are:
- `who`, response to `who` control messages,
- `restart`, response to `restart` control messages,
- `dbg`, general debug messages, and
- `coredump`, GDB ELF base64 encoded core dump.

E.g. to listen to all data, use:
```
mosquitto_sub -h {BROKER} -u {USERNAME} -P {PASSWORD} -t "calclock/data/#" -v
```
where `#` is a the MQTT wildcard character.

## Receive push notification from Google calendar changes

To improve response time we have the option of using the [Push Notifications API](https://developers.google.com/calendar/v3/push):
> Allows you to improve the response time of your application. It allows you to eliminate the extra network and compute costs involved with polling resources to determine if they have changed. Whenever a watched resource changes, the Google Calendar API notifies your application.
> To use push notifications, you need to do three things:
> 1. Register the domain of your receiving URL.
> 2. Set up your receiving URL, or "Webhook" callback receiver.
> 3. Set up a notification channel for each resource endpoint you want to watch.

For the first requirement, the Google push notification need to be able to traverse your access router and reach your ESP32 device.  This requires a SSL certificate and a reverse proxy.  Please refer to [Traversing your access router](https://coertvonk.com/sw/embedded/turning-on-the-light-the-hard-way-26806#traverse) for more details.  On the Google Console end, visit APIs and Services > Domain Verification > Add domain.

The second requirement is met by `http_post_server.c`.  Note that the reverse proxy forwards the HTTPS request from Google as HTTP to the device.

The last requirement is met by extending the Google Apps Script as shown in `scripts/push-notifications.gs`.

To give the script the necessary permissions, we need to switch it from /default/ GCP (Apps Script–managed Cloud Platform project) to a /standard/ GCP project.  Then give it permissions to the Calendar API and access your domain.
  - in [Google Script](https://script.google.com/) > Resources > Cloud Platform project > associate with (new) Cloud project number
      - it will ask you to configure an OAuth consent screen
  - in Google Cloud Console for that (new) project > OAuth consent screen
      - select `External`
      - add scope = Google Calendar API ()../auth/calendar.readonly)
      - press `Save` (not submit)
  - In that same Google Clouse Console UI
      - on the left, domain verification > add/allow domain > add the external domain for your ESP32.

## Feedback

We love to hear from you.  Please use the Github discussions to provide feedback.
