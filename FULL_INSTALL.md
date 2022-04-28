# Full Install

[![GitHub Discussions](https://img.shields.io/github/discussions/cvonk/BLEscan)](https://github.com/cvonk/BLEscan/discussions)
![GitHub tag (latest by date)](https://img.shields.io/github/v/tag/cvonk/BLEscan)
![GitHub package.json dependency version (prod)](https://img.shields.io/github/package-json/dependency-version/cvonk/BLEscan/esp-idf) 
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

This program runs on an Espressif EPS32 microcontroller and advertizes or scan for iBeacons.

One of my sons used this as a tool to research the behavior of Bluetooth Low-Energy (BLE) signals in relation to contact tracing for MIT PACT.

![ESP32 statues scattered around the yard](media/photo.jpg)

Our test setup had about 20 devices. To make this project scale, we used over-the-air updates and MQTT for the control and data channels.

## Features:

  - [x] Supports both BLE advertiser and scan modes
  - [x] Advertize interval is configurable
  - [x] Controlled and data presented through MQTT
  - [x] Supports over-the-air updates
  - [x] Easily one-time provisioning from an Android phone

## Hardware

No soldering required.

> :warning: **THIS PROJECT IS OFFERED AS IS. IF YOU USE IT YOU ASSUME ALL RISKS. NO WARRENTIES.**

### Bill of materials

| Name          | Description                                                       | Sugggested mfr/part#       |
|---------------|-------------------------------------------------------------------|----------------------------|
| ESP32BRD      | ESP32 development board                                          | [ESP32-DevKitC-VB](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview)
| BROKER        | Device to run MQTT Broker such as [Mosquitto](https://mosquitto.org/) | [Raspberry Pi 4](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/)

## Software

Clone the repository and its submodules to a local directory. The `--recursive` flag automatically initializes and updates the submodules in the repository,.

```bash
git clone --recursive https://github.com/cvonk/BLEscan
cd BLEscan
cp scanner/Kconfig.example scanner/Kconfig
cp factory/Kconfig.example factory/Kconfig
```

### ESP-IDF 

If you haven't installed ESP-IDF, I recommend the Microsoft Visual Studio Code IDE (vscode). From vscode, add the [Microsoft's C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools). Then add the [Espressif IDF extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension) and follow its configuration to install ESP-IDF 4.4.

### Boot process

As usual, the `bootloader` image does some minimum initializations. If it finds a valid `ota` image, it passes control over to that image. If not, it starts the `factory` image.

  - The `factory` image takes care of provisioning Wi-Fi and MQTT credentials with the help of a phone app. These credentials are stored in the `nvs` partition. It then downloads the `ota` image, and restarts the device.
  - We refer to the `ota` image as the `scanner`, as it provides the core of the functionality of the OPNpool device.

To host your `scanner` image, you will need to place it on your LAN or on the Web. Specify the "Firmware upgrade url endpoint" using menuconfig.

```bash
cd BLEscan/scanner
idf.py set-target esp32
idf.py menuconfig
idf.py flash
scp build/scanner.bin host.domain:~/path/to/scanner.bin
```

We will build the `factory` image and provision it using an Android phone app.

> If you have an iPhone, or you have problems running the Android app, you can extend `esp_prov.py` to include `mqtt_url` similar to what is shown [here](https://github.com/espressif/esp-idf-provisioning-android/issues/11#issuecomment-586973381). Sorry, I don't have the iOS development environment.

Specify the same "Firmware upgrade url endpoint" using menuconfig.

```bash
cd BLEscan/factory
idf.py set-target esp32
idf.py menuconfig
idf.py erase-flash
idf.py flash
idf.py monitor
```

In the last step of provisioning, this `factory` image will download the `scanner` image from your site.

Using an Android phone (we reuse the OPNpool app):

  * Install and run the OPNpool app from the [Play Store](https://play.google.com/store/apps/details?id=com.coertvonk.opnpool).
  * Using the overflow menu, select "Provision device".
  * Click on the "Provision" button and grant it access [^2].
  * Click on the name of the BLEscan device one it is detected (`POOL*`).
  * Select the Wi-Fi SSID to connect to and give it the password.
  * Specify the MQTT broker URL in the format `mqtt://username:passwd@host.domain:1883`.
  * Wait a few minutes for the provisioning to complete.

[^2]: Precise location permission is needed to find and connect to the OPNpool device using Bluetooth LE.

## Using the devices

Both replies to control messages and scan results are reported using MQTT topic `blescan/data/SUBTOPIC/DEVNAME`.

Subtopics are:
- `scan`, BLE scan results,
- `mode`, response to `mode` and `int` control messages,
- `who`, response to `who` control messages,
- `restart`, response to `restart` control messages,
- `dbg`, general debug messages

> The easiest way for running the Mosquitto MQTT client under Microsoft Windows is by using Windows Subsystem for Linux.

```bash
sudo apt-get update.
sudo apt-get install mosquitto.
sudo apt-get install mosquitto-clients.
```

E.g. to listen to all scan results, use:
```
mosquitto_sub -t "blescan/data/scan/#" -v
```
where `#` is a the MQTT wildcard character.

### Modes

The device support three modes:
  - `adv`, the device advertises iBeacon messages
  - `scan`, the device scans for iBeacon messages and reports them using MQTT
  - `idle`, the device neither advertises or scans

To switch modes, sent a control message with the new mode to:
- `blescan/ctrl`, a group topic that all devices listen to, or
- `blescan/ctrl/DEVNAME`, only `DEVNAME` listens to this topic.

Here `DEVNAME` is either a programmed device name, such as `esp32-1`, or `esp32_XXXX` where the `XXXX` are the last digits of the MAC address. Device names are assigned based on the BLE MAC address in `main/ble_task.c`.

| `mosquitto_pub -t "blescan/ctrl" -m SEE_BELOW` |  `mosquitto_sub -t "blescan/data/#"` | 
|----------------|-----------------------|
| `mode`         | `{ "response": { "mode": "ADV", "interval": 40 } }`
| `scan`         | `{ "response": { "mode": "SCAN", "interval": 40 } }`
| `int 100`      | `{ "response": { "mode": "SCAN", "interval": 100 } }`
| `adv`          | `{ "response": { "mode": "ADV", "interval": 100 } }`
| `idle`         | `{ "response": { "mode": "IDLE", "interval": 100 } }`

### Other controls

Other control messages are:
- `who`, can be used for device discovery when sent to the group topic
- `restart`, to restart the ESP32 (and check for OTA updates)
- `int N`, to change scan/adv interval to N milliseconds (40 .. 1000 msec)
- `mode`, to report the current scan/adv mode and interval

### Multiple devices

Messages can be sent to a specific device, or the whole group:
```
mosquitto_pub -t "blescan/ctrl/esp-1" -m "who"
mosquitto_pub -t "blescan/ctrl" -m "who"
```

In one terminal listen for the reponses from all devices

```bash
mosquitto_sub -t "blescan/data/#" -v
```

In another terminal sent the command to all devices

```
mosquitto_pub -t "blescan/ctrl" -m who
```

The first terminal will show the scan results
```
blescan/data/who/esp32-1 { "ble": {"name": "esp32-1", "address": "30:ae:a4:cc:24:6a"}, "firmware": { "version": "scanner.v1.0", "date": "Apr 28 2022 16:20:28" }, "wifi": { "connect": 1, "address": "10.1.1.120", "SSID": "Guest Barn", "RSSI": -51 }, "mqtt": { "connect": 1 }, "mem": { "heap": 115692 } }
blescan/data/who/esp32-4 { "ble": {"name": "esp32-4", "address": "ac:67:b2:53:7f:22"}, "firmware": { "version": "scanner.v1.0", "date": "Apr 28 2022 16:20:28" }, "wifi": { "connect": 1, "address": "10.1.1.123", "SSID": "Guest Barn", "RSSI": -66 }, "mqtt": { "connect": 1 }, "mem": { "heap": 115636 } }
blescan/data/who/esp32-3 { "ble": {"name": "esp32-3", "address": "ac:67:b2:53:82:8a"}, "firmware": { "version": "scanner.v1.0", "date": "Apr 28 2022 16:20:28" }, "wifi": { "connect": 1, "address": "10.1.1.122", "SSID": "Guest Barn", "RSSI": -58 }, "mqtt": { "connect": 1 }, "mem": { "heap": 115636 } }
blescan/data/who/esp32-2 { "ble": {"name": "esp32-2", "address": "30:ae:a4:cc:32:4e"}, "firmware": { "version": "scanner.v1.0", "date": "Apr 28 2022 16:20:28" }, "wifi": { "connect": 1, "address": "10.1.1.121", "SSID": "Guest Barn", "RSSI": -65 }, "mqtt": { "connect": 1 }, "mem": { "heap": 115716 } }
```


### Scan example

In one terminal listen for the reponses

```bash
mosquitto_sub -t "blescan/data/#" -v
```

In another terminal sent the commands.  First put all devices in advertizing mode, then put one device in scanning mode.

```
mosquitto_pub -t "blescan/ctrl" -m adv
mosquitto_pub -t "blescan/ctrl/esp32-2" -m scan
```

The first terminal will show the scan results
```
blescan/data/mode/esp32-3 { "response": { "mode": "ADV", "interval": 40 } }
blescan/data/mode/esp32-2 { "response": { "mode": "ADV", "interval": 40 } }
blescan/data/mode/esp32-4 { "response": { "mode": "ADV", "interval": 40 } }
blescan/data/mode/esp32-1 { "response": { "mode": "ADV", "interval": 40 } }
blescan/data/mode/esp32-1 { "response": { "mode": "SCAN", "interval": 40 } }
blescan/data/scan/esp32-1 { "name": "esp32-3", "address": "ac:67:b2:53:82:8a", "txPwr": -59, "RSSI": -40 }
blescan/data/scan/esp32-1 { "name": "esp32-4", "address": "ac:67:b2:53:7f:22", "txPwr": -59, "RSSI": -38 }
blescan/data/scan/esp32-1 { "name": "esp32-2", "address": "30:ae:a4:cc:32:4e", "txPwr": -59, "RSSI": -37 }
blescan/data/scan/esp32-1 { "name": "esp32-2", "address": "30:ae:a4:cc:32:4e", "txPwr": -59, "RSSI": -39 }
blescan/data/scan/esp32-1 { "name": "esp32-3", "address": "ac:67:b2:53:82:8a", "txPwr": -59, "RSSI": -39 }
blescan/data/scan/esp32-1 { "name": "esp32-3", "address": "ac:67:b2:53:82:8a", "txPwr": -59, "RSSI": -38 }
blescan/data/scan/esp32-1 { "name": "esp32-4", "address": "ac:67:b2:53:7f:22", "txPwr": -59, "RSSI": -37 }
blescan/data/scan/esp32-1 { "name": "esp32-3", "address": "ac:67:b2:53:82:8a", "txPwr": -59, "RSSI": -36 }
blescan/data/scan/esp32-1 { "name": "esp32-4", "address": "ac:67:b2:53:7f:22", "txPwr": -59, "RSSI": -37 }
blescan/data/scan/esp32-1 { "name": "esp32-2", "address": "30:ae:a4:cc:32:4e", "txPwr": -59, "RSSI": -40 }
blescan/data/scan/esp32-1 { "name": "esp32-2", "address": "30:ae:a4:cc:32:4e", "txPwr": -59, "RSSI": -37 }
blescan/data/scan/esp32-1 { "name": "esp32-3", "address": "ac:67:b2:53:82:8a", "txPwr": -59, "RSSI": -36 }
blescan/data/scan/esp32-1 { "name": "esp32-4", "address": "ac:67:b2:53:7f:22", "txPwr": -59, "RSSI": -37 }
blescan/data/scan/esp32-1 { "name": "esp32-2", "address": "30:ae:a4:cc:32:4e", "txPwr": -59, "RSSI": -39 }
blescan/data/scan/esp32-1 { "name": "esp32-4", "address": "ac:67:b2:53:7f:22", "txPwr": -59, "RSSI": -38 }
```

## Feedback

We love to hear from you. Please use the Github channels to provide feedback.
