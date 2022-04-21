# CALclock

[![GitHub Discussions](https://img.shields.io/github/discussions/sandervonk/CALclock)](https://github.com/sandervonk/CALclock/discussions)
![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/sandervonk/CALclock?include_prereleases&logo=DocuSign&logoColor=%23fff)
![GitHub](https://img.shields.io/github/license/sandervonk/CALclock)

The CALclock runs on an Espressif EPS32 microcontroller and shows upcoming events on a LED circle incorporated in a clock faceplate.

It can be used as anything from a decorative/interactive art piece to a normal clock that can remind you of upcoming appointments in a fun and cleanly designed way.

I used this to remind me of upcoming appointments once thatschool moved online.

![Backward facing glass clock with LED circle](media/forward_facing_250px.jpg)
![Forward facing glass clock with LED circle](media/backward_facing_250px.jpg)

## Features:

  - [x] Shows calendar events in different colors using an LED circle placed behind the faceplate of a clock.
  - [x] Push notifications for timely updates to calendar changes. [^1]
  - [x] Over-the-air (OTA) updates [^1]
  - [x] WiFi provisioning using phone app [^1]
  - [x] Remote restart, and version information (using MQTT)
  - [x] Core dump over MQTT to aid debugging [^1]
  - [x] Open source!

[^1]: Available with the full install as described in [`FULL_INSTALL.md`](FULL_INSTALL.md)

The full fledged project installation method is described in the [`FULL_INSTALL.md`](FULL_INSTALL.md). Before you go down that road, you may want to give it a quick spin to see what it can do. The remainder of this README will walk you through this.

## Parts

Two approaches can be used when deciding the look of you clock. One of which is to have the ring fully visible, with the other being to use the leds as an artsy-backlight.

![Internals of Backward facing glass clock with LED circle](media/forward_facing_int_250px.jpg)
![Internals of Forward facing glass clock with LED circle](media/backward_facing_int_250px.jpg)

- [ ] RGB LED Pixel Ring containing 60 WS2812B SMD5050 addressable LEDs (e.g. "Chinly Addressable 60 Pixel LED Ring"). These WS2812B pixels are 5V, and draw about 60 mA each at full brightness. If you plan to use it in a bedroom, you probably want less bright LEDS such as WS2812 (without the "B").
- [ ] ESP32 board with 4 MByte flash memory, such as [ESP32-DevKitC-VB](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview), LOLIN32 or MELIFE ESP32.
- [ ] 5 Volt, 3 Amp power adapter
- [ ] Capacitor (470 uF / 16V)
- [ ] Resistor (470 Ohm)
- [ ] Analog clock with glass face plate (e.g. Tempus TC6065S Wall Clock with Glass Metal Frame or a Selko 11" Brushed Metal Wall Clock)
- [ ] Optional frosting spray (e.g. Rust-Oleum Frosted Glass Spray Paint)
- [ ] Glass glue (e.g. Loctite Glass Glue)
- [ ] Molex 2 Pin Connectors

The Data-in of the LED circle should be driven with 5V +/- 0.5V, but we seem to get away with using the 3.3V output from ESP32 with a 470 Ohms resistor in series. We didn't notice a diffeence when using a level shifter.

## Connect

> :warning: **THIS PROJECT IS OFFERED AS IS. IF YOU USE IT YOU ASSUME ALL RISKS. NO WARRENTIES.**

Connect the 5 Volt adapter to the ESP32 and LED strip. Connect the data from the ESP32 module to the LED circle as shown below. 

| ESP32 module | LED circle   |
|:-------------|:-------------|
| `GPIO#18`    | WS2812 DATA  |

## Google Apps Script

The software is a symbiosis between a Google Apps Script and firmware running on the ESP32. The script reads events from your Google Calendar and presents them as JSON to the ESP32 device.

The ESP32 microcontroller calls a [Google Apps Script](https://developers.google.com/apps-script/guides/web) that retrieve a list of upcoming events from your calendar and returns them as a JSON object. As we see later, the ESP32 will update the LEDs based on this JSON object.

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

The script is more involved as needed because also supports *Push Notifications*.

## Build

Clone the repository and its submodules to a local directory. The `--recursive` flag automatically initializes and updates the submodules in the repository,.

```bash
git clone --recursive https://github.com/sandervonk/CALclock.git
```

or using `ssh`
```bash
git clone --recursive git@github.com:sandervonk/CALclock.git
```

From within Microsoft Visual Code (VScode), add the [Microsoft's C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools). Then add the [Espressif IDF extension &ge;4.4](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension). ESP-IDF will automatically start its configuration

From VScode:

  * Change to the `CALclock/clock` folder.
  * Connect your ESP32 module, and configure the device and COM port (press the F1-key and select "ESP-IDF: Device configurion")
  * Edit the configuration (press the F1-key, select "ESP-IDF: SDK configuration editor" and scroll down to CALclock)
      * Select "Use hardcoded Wi-Fi credentials" and specify the SSID and password of your Wi-Fi access point.
      * If you have a MQTT broker set up, select "Use hardcoded MQTT URL" and specify the URL in the format `mqtt://username:passwd@host.domain:1883`
  * Start the build-upload-monitor cycle (press the F1-key and select "ESP-IDF: Build, Flash and start a monitor on your device").

The device will appear on your network segment as `calclock.local`. If MQTT is configured, it will publish MQTT messages.

## ESP32 Design

The functionality is divided into:
- `HTTPS Client Task`, that polls the Google Apps Script for calendar events
- `Display Task`, that uses the Remote Control Module on the ESP32 to drive the LED strip.
- `HTTP POST Server`, to listen to push notifications from Google.
- `OTA Task`, that check for updates upon reboot
- `Reset Task`, when GPIO#0 is low for 3 seconds, it erases the WiFi credentials so that the board can be re-provisioned using your phone.

The different parts communicate using FreeRTOS mailboxes.

## Using

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

Here `DEVNAME` is either a programmed device name, such as `esp32-1`, or `esp32_XXXX` where the `XXXX` are the last digits of the MAC address. Device names are assigned based on the BLE MAC address in `main/main.c`.

Messages can be sent to a specific device, or the whole group:
```
mosquitto_pub -h {BROKER} -u {USERNAME} -P {PASSWORD} -t "calclock/ctrl/esp32-1" -m "who"
mosquitto_pub -h {BROKER} -u {USERNAME} -P {PASSWORD} -t "calclock/ctrl" -m "who"
```

### MQTT

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

## Feedback

We love to hear from you. Please use the Github discussions to provide feedback.
