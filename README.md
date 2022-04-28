# BLE iBeacon scanner and advertizer

[![GitHub Discussions](https://img.shields.io/github/discussions/cvonk/BLEscan)](https://github.com/cvonk/BLEscan/discussions)
![GitHub tag (latest by date)](https://img.shields.io/github/v/tag/cvonk/BLEscan)
![GitHub package.json dependency version (prod)](https://img.shields.io/github/package-json/dependency-version/cvonk/BLEscan/esp-idf) 
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

This program runs on an Espressif EPS32 microcontroller and advertizes or scan for iBeacons.

One of my sons used this as a tool to research the behavior of Bluetooth Low-Energy (BLE) signals in relation to contact tracing for MIT PACT.

![ESP32 statues scattered around the yard](media/photo.jpg)

## Features:

  - [x] Supports both BLE advertiser and scan modes
  - [x] Advertize interval is configurable
  - [x] Controlled and data presented through MQTT
  - [x] Supports over-the-air updates [^1]
  - [x] Easily one-time provisioning from an Android phone [^1]

[^1]: Available with the full install as described in [`FULL_INSTALL.md`](FULL_INSTALL.md)

The full fledged project installation method is described in the [`FULL_INSTALL.md`](FULL_INSTALL.md). Before you go down that road, you may want to give it a quick spin to see what it can do. The remainder of this README will walk you through this.

The device interfaces using the MQTT protocol.
> MQTT stands for MQ Telemetry Transport. It is a publish/subscribe, extremely simple and lightweight messaging protocol, designed for constrained devices and low-bandwidth, high-latency or unreliable networks. [FAQ](https://mqtt.org/faq)

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
```
git clone --recursive https://github.com/cvonk/BLEscan
cd BLEscan
cp scanner/Kconfig.example scanner/Kconfig
cp factory/Kconfig.example factory/Kconfig
```

### ESP-IDF 

If you haven't installed ESP-IDF, I recommend the Microsoft Visual Studio Code IDE (vscode). From vscode, add the [Microsoft's C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools). Then add the [Espressif IDF extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension) and follow its configuration to install ESP-IDF 4.4.

### ESP32 Device

In `menuconfig`, scroll down to BLEscan and select "Use hardcoded Wi-Fi credentials" and specify the SSID and password of your Wi-Fi access point. Then select "Use hardcoded MQTT URL" and specify the URL to the MQTT broker in the form `mqtt://mqtt:passwd@host.domain:1883`.

```bash
cd scanner
idf.py set-target esp32
idf.py menuconfig
idf.py flash
```

## Using the application

Both replies to control messages and scan results are reported using MQTT topic `blescan/data/SUBTOPIC/DEVNAME`.

Subtopics are:
- `scan`, BLE scan results,
- `mode`, response to `mode` and `int` control messages,
- `who`, response to `who` control messages,
- `restart`, response to `restart` control messages,
- `dbg`, general debug messages

> The easiest way for running the Mosquitto MQTT client under Microsoft Windows is by using Windows Subsystem for Linux.

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

### Other controls

Other control messages are:
- `who`, can be used for device discovery when sent to the group topic
- `restart`, to restart the ESP32 (and check for OTA updates)
- `int N`, to change scan/adv interval to N milliseconds
- `mode`, to report the current scan/adv mode and interval

The easiest way to run a Mosquitto MQTT client under Microsoft Windows is through the Windows Subsystem for Linux.

Messages can be sent to a specific device, or the whole group:
```
mosquitto_pub -t "blescan/ctrl/esp-1" -m "who"
mosquitto_pub -t "blescan/ctrl" -m "who"
```

## Feedback

We love to hear from you. Please use the Github channels to provide feedback.
