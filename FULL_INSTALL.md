# CALalarm full install

[![GitHub Discussions](https://img.shields.io/github/discussions/sandervonk/CALalarm)](https://github.com/sandervonk/CALalarm/discussions)
![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/sandervonk/CALalarm?include_prereleases&logo=DocuSign&logoColor=%23fff)
![GitHub](https://img.shields.io/github/license/sandervonk/CALalarm)

Shows Google Calendar events on a LED circle incorporated in a clock faceplate.

It can be used as anything from a decorative/interactive art piece to a normal clock that can remind you of upcoming appointments in a fun and cleanly designed way. I used this to remind me of upcoming appointments once thatschool moved online.

![Backward facing glass clock with LED circle](media/forward_facing_250px.jpg) ![Forward facing glass clock with LED circle](media/backward_facing_250px.jpg)

## Features:

  - [x] Synchronizes with Google Calendar
  - [x] Shows calendar events on OLED screen
  - [x] Push notifications for timely updates to calendar changes.
  - [x] Over-the-air (OTA) updates
  - [x] WiFi provisioning using phone app
  - [x] Remote restart, and version information (using MQTT)
  - [x] Core dump over MQTT to aid debugging
  - [x] Open source!

## Hardware

Two approaches can be used when deciding the look of you clock. One of which is to have the ring fully visible, with the other being to use the leds as an artsy-backlight.

![Internals of Backward facing glass clock with LED circle](media/forward_facing_int_250px.jpg) ![Internals of Forward facing glass clock with LED circle](media/backward_facing_int_250px.jpg)

### Parts

- [ ] RGB LED Pixel Ring containing 60 WS2812B SMD5050 addressable LEDs (e.g. "Chinly Addressable 60 Pixel LED Ring"). These WS2812B pixels are 5V, and draw about 60 mA each at full brightness. If you plan to use it in a bedroom, you probably want less bright LEDS such as WS2812 (without the "B").
- [ ] ESP32 board with 4 MByte flash memory, such as [ESP32-DevKitC-VB](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview), LOLIN32 or MELIFE ESP32.
- [ ] 5 Volt, 3 Amp power adapter
- [ ] Capacitor (470 uF / 16V)
- [ ] Resistor (470 Ohm)
- [ ] Analog clock with glass face plate (e.g. Tempus TC6065S Wall Clock with Glass Metal Frame or a Selko 11" Brushed Metal Wall Clock)
- [ ] Optional frosting spray (e.g. Rust-Oleum Frosted Glass Spray Paint)
- [ ] Glass glue (e.g. Loctite Glass Glue)
- [ ] Molex 2 pin connector set

### Connect

> :warning: **THIS PROJECT IS OFFERED AS IS. IF YOU USE IT YOU ASSUME ALL RISKS. NO WARRENTIES.**

Connect the 5 Volt adapter to the ESP32 and LED strip.

Connect the data from the ESP32 module to the LED circle as shown below. 

| ESP32 module | xxxx           |
|:-------------|:---------------|
| `GPIO#18`    | xx             |

The `DATA-IN` of the LED circle should be driven with TTL signal levels, but we seem to get away with using the 3.3 Volt output from ESP32 in series with a 470 Ohms resistor. We didn't notice a diffeence when using a level shifter.

## Software

Clone the repository and its submodules to a local directory. The `--recursive` flag automatically initializes and updates the submodules in the repository.

```bash
git clone --recursive https://github.com/sandervonk/CALalarm.git
```

or using `ssh`
```bash
git clone --recursive git@github.com:sandervonk/CALalarm.git
```

From within Microsoft Visual Code (VScode), add the [Microsoft's C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools). Then add the [Espressif IDF extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension) and follow its configuration to install ESP-IDF 4.4.

### Google Apps Script

The software is a symbiosis between [Google Apps Script](https://developers.google.com/apps-script/guides/web) and firmware running on the ESP32. The script reads events from your Google Calendar and presents them as JSON to the ESP32 device.

To create the Web app:
  - Create a new project on [script.google.com](https://script.google.com);
  - Rename the project to e.g. `CALalarm-doGet`
  - Copy and paste the code from `script\Code.gs`
  - Add the `Google Calendar API` service .
  - Select the function `test` and click `Debug`. This will ask for permissions. Don't expect output.
  - Click `Deploy` and chose `New deployment`, choose
    - Service tye = `Web app`
    - Execute as = `Me`
    - Who has access = `Anyone`, make sure you understand what the script does!
    - Copy the Web app URL to the clipboard

Open the URL in a web browser. You should get a reply like
```json
{
    "time": "2022-04-20 12:56:37",
    "pushId": "some_id",
    "events": [
        { 
            "start": "2022-04-20 10:55:00",
            "end": "2022-04-20 15:45:00",
            "title: "school"
        }
    ]
}
```

Now, copy the `clock/main/Kconfig.example` to `clock/main/Kconfig` and paste the URL that ends in `/exec` to `clock/main/Kconfig` under `CLOCK_GAS_CALENDAR_URL`.

As we see in the next sections, the ESP32 does a `HTTP GET` on this URL. That way it retrieves a list of upcoming events from your calendar, and update the LEDs accordingly.

### ESP32 Device

The functionality is divided into:
- `HTTPS Client Task`, that polls the Google Apps Script for calendar events
- `Display Task`, that drives the LED strip.
- `HTTP POST Server`, that listens for push notifications from Google.
- `OTA Task`, that checks for updates upon reboot
- `Reset Task`, when GPIO#0 is held low for 3 seconds, it erases the Wi-Fi credentials so that the board can be re-provisioned using your phone.

These different parts communicate using FreeRTOS mailboxes.

#### Boot steps

As usual, the `bootloader` image does some minimum initializations. If it finds a valid `ota` image, it passes control over to that image. If not, it starts the `factory` image.

  - The `factory` image takes care of provisioning Wi-Fi and MQTT credentials with the help of a phone app. These credentials are stored in the `nvs` partition. It then downloads the `ota` image, and restarts the device.
  - We refer to the `ota` image as the `clock`, as it provides the core of the functionality of the CALalarm device.

#### The `clock` image

To host your own `clock` image, you will need to place it on your LAN or on the Web. If you're fine using my logiciel du jour, you can skip this section. In its current implementation it does **not** verify the Google Apps host certificate.

> To use HTTPS, you will need to add the server's public certificate to `clock/components/ota_update_task/CMakelists.txt`, and uncomment some lines in `ota_update_task.c` with `server_cert_pem`.

From VScode:

  * Open the `CALalarm/clock` folder (`File > Open`).
  * Connect your ESP32 module, and select the serial port using `ctrl-e p`.
  * Edit the SDK configuration (`ctrl-e g`) and scroll down to CALalarm and specify your "Firmware upgrade url endpoint" (e.g. http://host.domain/path/to/clock.bin).
  * Start the build cycle using `ctrl-e b`.
  * Upload `CALalarm/clock/build/clock.bin` to your site.

#### The `factory` image

We will build the `factory` image and provision it using the [OPNpool Android phone app](https://play.google.com/store/apps/details?id=com.coertvonk.opnpool).

> If you have an iPhone, or you have problems running the Android app, you can extend `esp_prov.py` to include `mqtt_url` similar to what is shown [here](https://github.com/espressif/esp-idf-provisioning-android/issues/11#issuecomment-586973381). Sorry, I don't have the iOS development environment.

In the last step of provisioning, this `factory` image will download the `clock` image from an external site.

From VScode:

  * Open the `CALalarm/factory` folder.
  * Connect your ESP32 module, and select the serial port using `ctrl-e p`.
  * Erase the NVRAM using `ctrl-e r`.
  * If you built and host your own `clock` image, you need to specify the path by editing the SDK configuration (`ctrl-e g`) and scroll down to CALalarm and specify your "Firmware upgrade url endpoint" (e.g. http://host.domain/path/clock.bin).
  * Start the build-upload-monitor cycle using `ctrl-e d`.

Using an Android phone:

  * Install and run the OPNpool app from the [Play Store](https://play.google.com/store/apps/details?id=com.coertvonk.opnpool).
  * Using the overflow menu, select "Provision device".
  * Click on the `Provision` button and grant it access [^2].
  * Click on the name of the CALalarm device once it is detected (`POOL*`).
  * Select the Wi-Fi SSID to connect to and give it the password.
  * If you don't have a MQTT broker press `Skip`.  Otherwise, specify the broker URL in the format `mqtt://username:passwd@host.domain:1883`.
  * Wait a few minutes for the provisioning to complete.

[^2]: Precise location permission is needed to find and connect to the CALalarm device using Bluetooth LE.

The device will appear on your network segment as `calalarm.local`. If MQTT is configured, it will publish MQTT messages.

*Your clock is now functional!*

## Push notification from Google

Normally, the devices polls for changes in the Google Calendar every 2 minutes. We can improve this response time by pushing notifications from Google to your device. You should only enable this, if your router has a SSL certificate and you're familiar with configuring a reverse proxy on your router.

The [Push Notifications API](https://developers.google.com/calendar/v3/push) documentation says:
> Allows you to improve the response time of your application. It allows you to eliminate the extra network and compute costs involved with polling resources to determine if they have changed. Whenever a watched resource changes, the Google Calendar API notifies your application. To use push notifications, you need to do three things:
> 1. Set up your receiving URL, or "Webhook" callback receiver.
> 2. Set up a notification channel for each resource endpoint you want to watch.

To meet the first requirement, the push notification need to be able to traverse your access router to reach your ESP32 device. This requires a SSL certificate and a reverse proxy. This implies you need to configure a reverse proxy (Nginx or Pound) on your router.

Setup this reverse proxy so that it forwards HTTPS request from Google, as HTTP to the device. On the device, the module `http_post_server.c` is the endpoint for these push notifications. 

The second requirement is already met by the Google apps script that we installed earlier.

## Behind the scenes

To easily see what version of the software is running on the CALalarm device, or what Wi-Fi network it is connected to, the firmware contains a MQTT client.
> MQTT stands for MQ Telemetry Transport. It is a publish/subscribe, extremely simple and lightweight messaging protocol, designed for constrained devices and low-bandwidth, high-latency or unreliable networks.

It supports the following control messages:
- `who`, can be used for device discovery when sent to the group topic
- `restart`, to restart the ESP32 (and check for OTA updates)
- `int N`, to change scan/adv interval to *N* milliseconds
- `mode`, to report the current scan/adv mode and interval

Control messages can be sent to:
- `calalarm/ctrl`, a group topic that all devices listen to, or
- `calalarm/ctrl/DEVNAME`, only `DEVNAME` listens to this topic.

Here `DEVNAME` is either a programmed device name, such as `esp32-1`, or `esp32_XXXX` where the `XXXX` are the last digits of the MAC address. Device names are assigned based on the BLE MAC address in `main/main.c`.

Messages can be sent to a specific device, or the whole group:
```
mosquitto_pub -h BROKER -u USERNAME -P PASSWORD -t "calalarm/ctrl/esp32-1" -m "who"
mosquitto_pub -h BROKER -u USERNAME -P PASSWORD -t "calalarm/ctrl" -m "who"
```

Both replies to such control messages and debug output (and coredumps) are reported using MQTT topic `calalarm/data/SUBTOPIC/DEVNAME`.

Subtopics are:
- `who`, response to `who` control messages,
- `restart`, response to `restart` control messages,
- `dbg`, general debug messages, and
- `coredump`, GDB ELF base64 encoded core dump.

E.g. to listen to all data, use:
```
mosquitto_sub -h {BROKER} -u {USERNAME} -P {PASSWORD} -t "calalarm/data/#" -v
```
where `#` is a the MQTT wildcard character.

## Feedback

We love to hear from you. Please use the Github discussions to provide feedback.




## OLD REMOVE !!

((OLD To give the script the necessary permissions, we need to switch it from /default/ GCP (Apps Script–managed Cloud Platform project) to a /standard/ GCP project. Then give it permissions to the Calendar API and access your domain.
  - in [Google Script](https://script.google.com/) > Resources > Cloud Platform project > associate with (new) Cloud project number
      - it will ask you to configure an OAuth consent screen
  - in Google Cloud Console for that (new) project > OAuth consent screen
      - select `External`
      - add scope = Google Calendar API ()../auth/calendar.readonly)
      - press `Save` (not submit)
  - In that same Google Clouse Console UI
      - on the left, domain verification > add/allow domain > add the external domain for your ESP32.))
