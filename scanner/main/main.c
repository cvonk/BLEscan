/**
 * @brief ESP32 BLE iBeacon scanner and advertising (ctrl/data over MQTT)
 **/
// Copyright © 2020, Johan and Coert Vonk
// SPDX-License-Identifier: MIT
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
#include "ipc_msgs.h"
#include "mqtt_task.h"
#include "ble_task.h"

static char const * const TAG = "main";

void _init_nvs_flash(void)
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
    ipc_t * const ipc = priv_void;
    ipc->dev.connectCnt.wifi++;
    snprintf(ipc->dev.ipAddr, WIFI_DEVIPADDR_LEN, IPSTR, IP2STR(ip));

    ESP_LOGI(TAG, "%s / %u", ipc->dev.ipAddr, ipc->dev.connectCnt.wifi);
    return ESP_OK;
}

static esp_err_t
_wifi_disconnect_cb(void * const priv_void, bool const auth_err)
{
    // should probably reprovision on repeated auth_err
    return ESP_OK;
}

static void
_connect2wifi(ipc_t * const ipc)
{
    wifi_connect_config_t wifi_connect_config = {
        .onConnect = _wifi_connect_cb,
        .onDisconnect = _wifi_disconnect_cb,
        .priv = ipc,
    };
    ESP_ERROR_CHECK(wifi_connect_init(&wifi_connect_config));

#if defined(CONFIG_WIFI_CONNECT_SSID) && defined(CONFIG_WIFI_CONNECT_PASSWD)
    if (strlen(CONFIG_WIFI_CONNECT_SSID)) {
        ESP_LOGW(TAG, "Using SSID from Kconfig");
        wifi_config_t wifi_config = {
            .sta = {
                .ssid = CONFIG_WIFI_CONNECT_SSID,
                .password = CONFIG_WIFI_CONNECT_PASSWD,
            }
        };
        ESP_ERROR_CHECK(wifi_connect_start(&wifi_config));
    } else
#endif
    {
        ESP_LOGW(TAG, "Using SSID from flash");
        wifi_connect_start(NULL);
    }
}

void app_main() {

	_init_nvs_flash();

    ESP_LOGI(TAG, "starting ..");
    xTaskCreate(&factory_reset_task, "factory_reset_task", 4096, NULL, 5, NULL);

    static ipc_t ipc;
    ipc.toBleQ = xQueueCreate(2, sizeof(toBleMsg_t));
    ipc.toMqttQ = xQueueCreate(2, sizeof(toMqttMsg_t));
    ipc.dev.connectCnt.wifi = 0;
    ipc.dev.connectCnt.mqtt = 0;
    assert(ipc.toBleQ && ipc.toMqttQ);

    _connect2wifi(&ipc);

	xTaskCreate(&ota_update_task, "ota_update_task", 2 * 4096, NULL, 5, NULL);
    xTaskCreate(&ble_task, "ble_task", 2 * 4096, &ipc, 5, NULL);
    xTaskCreate(&mqtt_task, "mqtt_task", 2 * 4096, &ipc, 5, NULL);
}
