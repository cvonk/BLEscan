/**
 * @brief ESP32 BLE iBeacon scanner and advertising (ctrl/data over MQTT)
 * 
 * This file is part of BLEscan.
 * 
 * BLEscan is free software: you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 * 
 * BLEscan is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with BLEscan. 
 * If not, see <https://www.gnu.org/licenses/>.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2020-2022, Johan and Coert Vonk
 */

 #include <sdkconfig.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_ota_ops.h>
#include <esp_bt_device.h>
#include <esp_core_dump.h>

#include <wifi_connect.h>
#include <ota_update_task.h>
#include <factory_reset_task.h>
#include "ipc.h"
#include "mqtt_task.h"
#include "ble_task.h"

static char const * const TAG = "main";

typedef struct wifi_connect_priv_t {
    ipc_t * ipc;
} wifi_connect_priv_t;

static void
_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static esp_err_t
_wifi_connect_cb(void * const priv_void, esp_ip4_addr_t const * const ip)
{
    wifi_connect_priv_t * const priv = priv_void;
    ipc_t * const ipc = priv->ipc;

    // note the MAC and IP addresses
    snprintf(ipc->dev.ipAddr, WIFI_DEVIPADDR_LEN, IPSTR, IP2STR(ip));
	//board_name(ipc->dev.name, WIFI_DEVNAME_LEN);

    ipc->dev.count.wifiConnect++;
    return ESP_OK;
}

static esp_err_t
_wifi_disconnect_cb(void * const priv_void, bool const auth_err)
{
    wifi_connect_priv_t * const priv = priv_void;
    ipc_t * const ipc = priv->ipc;

    if (auth_err) {
        ipc->dev.count.wifiAuthErr++;
        // 2BD: should probably reprovision on repeated auth_err and return ESP_FAIL
    }
    ESP_LOGW(TAG, "Wifi disconnect connectCnt=%u, authErrCnt=%u", ipc->dev.count.wifiConnect, ipc->dev.count.wifiAuthErr);
    return ESP_OK;
}

static void
__attribute__((noreturn)) _delete_task()
{
    ESP_LOGI(TAG, "Exiting task ..");
    (void)vTaskDelete(NULL);

    while (1) {  // FreeRTOS requires that tasks never return
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

/*
 * Connect to WiFi accesspoint.
 * Register callbacks when connected or disconnected.
 */

static void
_connect2wifi(ipc_t * const ipc)
{
    static wifi_connect_priv_t priv = {};
    priv.ipc = ipc;

    wifi_connect_config_t wifi_connect_config = {
        .onConnect = _wifi_connect_cb,
        .onDisconnect = _wifi_disconnect_cb,
        .priv = &priv,
    };
    ESP_ERROR_CHECK(wifi_connect_init(&wifi_connect_config));

    wifi_config_t * wifi_config_addr = NULL;
#ifdef CONFIG_BLESCAN_HARDCODED_WIFI_CREDENTIALS
    if (strlen(CONFIG_BLESCAN_HARDCODED_WIFI_SSID)) {
        ESP_LOGW(TAG, "Using SSID from Kconfig");
        wifi_config_t wifi_config = {
            .sta = {
                .ssid = CONFIG_BLESCAN_HARDCODED_WIFI_SSID,
                .password = CONFIG_BLESCAN_HARDCODED_WIFI_PASSWD,
            }
        };
        wifi_config_addr = &wifi_config;
    } else
#endif
    {
        ESP_LOGW(TAG, "Using SSID from nvram");
    }

    esp_err_t err = wifi_connect_start(wifi_config_addr);
    if (err == ESP_ERR_WIFI_SSID) {
        ESP_LOGE(TAG, "Wi-Fi SSID/passwd not provisioned");
        _delete_task();
    }
    ESP_ERROR_CHECK(err);
}

void app_main() {

	_init_nvs();

    ESP_LOGI(TAG, "starting ..");
    xTaskCreate(&factory_reset_task, "factory_reset_task", 4096, NULL, 5, NULL);

    static ipc_t ipc = {};
    ipc.toBleQ = xQueueCreate(2, sizeof(ipc_to_ble_msg_t));
    ipc.toMqttQ = xQueueCreate(2, sizeof(ipc_to_mqtt_msg_t));
    assert(ipc.toBleQ && ipc.toMqttQ);

    _connect2wifi(&ipc);

	xTaskCreate(&ota_update_task, "ota_update_task", 2 * 4096, "scanner", 5, NULL);
    xTaskCreate(&ble_task, "ble_task", 2 * 4096, &ipc, 5, NULL);
    xTaskCreate(&mqtt_task, "mqtt_task", 2 * 4096, &ipc, 5, NULL);
}
