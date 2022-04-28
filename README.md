# ESP32 BLE iBeacon scanner and advertizer (ctrl/data over MQTT)

[![LICENSE](https://img.shields.io/github/license/jvonk/pact)](LICENSE)

This program runs on an Espressif EPS32 microcontroller and advertizes or scan for iBeacons.

I used this as a tool to research the behavior of Bluetooth Low-Energy (BLE) signals in relation to contact tracing.

![ESP32 statues scattered around the yard](media/photo.jpg)

## Features:

- Supports both BLE advertiser and scan modes
- Controlled and data presented through MQTT
- Optional Over-the-air (OTA) updates
- Optional WiFi provisioning using phone app
- Optional core dump over MQTT to aid debugging

## Getting started

The Git repository contains submodules.  To clone these submodules as well, use the `--recursive` flag.
```
git clone --recursive https://github.com/jvonk/pact
cd pact
cp ESP32_PACT/main/Kconfig-example.projbuild ESP32_PACT/main/Kconfig.projbuild
cp ESP32_PACT/components/ota_update_task/Kconfig.example ESP32_PACT/components/ota_update_task/Kconfig
```

Update using
```bash
git pull
git submodule update --recursive --remote
```

### Bill of materials

- ESP32 development board, such as [ESP32-DevKitC-VB](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview), LOLIN32, MELIFE ESP32 or pretty much any ESP32 board with 4 Mbyte flash memory.
- 5 Volt, micro USB power adapter,
- Raspberry Pi or other device to run a MQTT Broker such as [Mosquitto](https://mosquitto.org/).

### System Development Kit (SDK)

The software relies on the cutting edge (master) of the ESP-IDF System Development Kit (SDK), currently `v4.3-dev-472-gcf056a7d0`.  Install this SDK according to its [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/), or follow a third party guide such as [ESP32 + VSCode](https://github.com/cvonk/vscode-starters/blob/master/ESP32/README.md).

### Configure

1. Either update the defaults in the `Kconfig*` files directly, or use the "ESP-IDF: launch gui configuration tool".
2. Delete `sdkconfig` so the build system will recreate it.

### Build

The firmware loads in two stages:
  1. `factory.bin` configures the WiFi using phone app (except when WiFi credentials are hard code in `Kconfig.projbuild`)
  2. `blescan.bin`, the main application

Compile `blescan.bin` first, by opening the `ESP32_PACT` folder in Microsft Visual Code and starting the build (`ctrl-e b`).

When using OTA updates, the resulting `build/blescan.bin` should be copied to the OTA Update file server (pointed to by `OTA_UPDATE_FIRMWARE_URL`).

### Provision WiFi credentials

If you set your WiFi SSID and password using `Kconfig.projbuild` you're all set and can simply flash the application and skip the remainder of this section.

To provision the WiFi credentials using a phone app, this `factory` app advertises itself to the phone app.  The Espressif BLE Provisioning app is available as source code or from the [Android](https://play.google.com/store/apps/details?id=com.espressif.provble) and [iOS](https://apps.apple.com/in/app/esp-ble-provisioning/id1473590141) app stores.

On the ESP32 side, we need a `factory` app that advertises itself to the phone app.  Open the folder `factory` in Microsft Visual Code and start a debug session (`ctrl-e d`).  This will compile and flash the code, and connects to the serial port to show the debug messages.

Using the Espressif BLE Provisioning phone app, `scan` and connect to the ESP32.  use the app to specify the WiFi SSID and password. Depending on the version of the app, you may first have to change `_ble_device_name_prefix` to `PROV_` in `test/main/main.c`, and change the `config.service_uuid` in `ble_prov.c`.
(Personally, I still use an older customized version of the app.)

This stores the WiFi SSID and password in flash memory and triggers a OTA download of the application itself.  IAlternatively, don't supply the OTA path and flash the `blescan.bin` application using the serial port.

To erase the WiFi credentials, pull `GPIO# 0` down for at least 3 seconds and release.  This I/O is often connected to a button labeled `BOOT` or `RESET`.

### OTA download

Besides connecting to WiFi, one of the first things the application does is check for OTA updates.  Upon completion, the device resets to activate the downloaded code.

## Using the application

The device interfaces using the MQTT protocol.
> MQTT stands for MQ Telemetry Transport. It is a publish/subscribe, extremely simple and lightweight messaging protocol, designed for constrained devices and low-bandwidth, high-latency or unreliable networks. [FAQ](https://mqtt.org/faq)

The device support three modes:
  - `adv`, the device advertises iBeacon messages
  - `scan`, the device scans for iBeacon messages and reports them using MQTT
  - `idle`, the device neither advertises or scans

To switch modes, sent a control message with the new mode to:
- `blescan/ctrl`, a group topic that all devices listen to, or
- `blescan/ctrl/DEVNAME`, only `DEVNAME` listens to this topic.

Here `DEVNAME` is either a programmed device name, such as `esp32-1`, or `esp32_XXXX` where the `XXXX` are the last digits of the MAC address.  Device names are assigned based on the BLE MAC address in `main/ble_task.c`.

Other control messages are:
- `who`, can be used for device discovery when sent to the group topic
- `restart`, to restart the ESP32 (and check for OTA updates)
- `int N`, to change scan/adv interval to N milliseconds
- `mode`, to report the current scan/adv mode and interval

Messages can be sent to a specific device, or the whole group:
```
mosquitto_pub -h {BROKER} -u {USERNAME} -P {PASSWORD} -t "blescan/ctrl/esp-1" -m "who"
mosquitto_pub -h {BROKER} -u {USERNAME} -P {PASSWORD} -t "blescan/ctrl" -m "who"
```

### Scan results and replies to Control msgs

Both replies to control messages and scan results are reported using MQTT topic `blescan/data/SUBTOPIC/DEVNAME`.

Subtopics are:
- `scan`, BLE scan results,
- `mode`, response to `mode` and `int` control messages,
- `who`, response to `who` control messages,
- `restart`, response to `restart` control messages,
- `dbg`, general debug messages, and
- `coredump`, GDB ELF base64 encoded core dump.

E.g. to listen to all scan results, use:
```
mosquitto_sub -h {BROKER} -u {USERNAME} -P {PASSWORD} -t "blescan/data/scan/#" -v
```
where `#` is a the MQTT wildcard character.

## Feedback

I love to hear from you.  Please use the Github channels to provide feedback.
