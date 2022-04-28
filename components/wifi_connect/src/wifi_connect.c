/**
 * @brief ESP32 Component to connect to WiFi Access Point
 **/
// Copyright © 2020, Coert Vonk
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <string.h>
#include <sdkconfig.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include "wifi_connect.h"

static char const * const TAG = "wifi_connect";
static EventGroupHandle_t _event_group = NULL;

typedef enum {
    WIFI_EVENT_CONNECTED = BIT0
} my_wifi_event_t;

static void
_wifiStaStart(void * arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    //wifi_connect_config_t * const arg = arg_void;
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static void
_wifiConnectHandler(void * arg_void, esp_event_base_t event_base,  int32_t event_id, void * event_data)
{
    if (_event_group) {
        xEventGroupSetBits(_event_group, WIFI_EVENT_CONNECTED);
    }
    wifi_connect_config_t * const cfg = arg_void;
    if (cfg->onConnect) {
        wifi_connect_config_t * const cfg = arg_void;
        ip_event_got_ip_t const * const event = (ip_event_got_ip_t *) event_data;
        cfg->onConnect(cfg->priv, &event->ip_info.ip);
    }
}

static void
_wifiDisconnectHandler(void * arg_void, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
    if (_event_group) {
        xEventGroupClearBits(_event_group, WIFI_EVENT_CONNECTED);
    }
    wifi_connect_config_t * const cfg = arg_void;

    wifi_event_sta_disconnected_t const * const disconn = (wifi_event_sta_disconnected_t *) event_data;
    bool auth_err = false;
    switch (disconn->reason) {
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_BEACON_TIMEOUT:
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_ASSOC_FAIL:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            auth_err = true;
            break;
        case WIFI_REASON_NO_AP_FOUND:
        default:
            break;
    }
    if (cfg->onDisconnect) {
        if (cfg->onDisconnect(cfg->priv, auth_err) != ESP_OK) {
            return;  // no reconnect
        }
    }
    vTaskDelay(10000L / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(esp_wifi_connect());
}

esp_err_t
wifi_connect_init(wifi_connect_config_t * const config)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();  // init WiFi with configuration from non-volatile storage
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &_wifiStaStart, config));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &_wifiDisconnectHandler, config));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &_wifiConnectHandler, config));
    return ESP_OK;
}

esp_err_t
wifi_connect_start(wifi_config_t * const wifi_config)
{
    _event_group = xEventGroupCreate();
    assert(_event_group);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    if (wifi_config) {
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config));
    }
    wifi_config_t wifi_cfg;
    if (esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg) != ESP_OK ||
        *wifi_cfg.sta.ssid == 0) {
        ESP_LOGE(TAG, "No SSID/Passwd");
        return ESP_ERR_WIFI_SSID;
    }
    ESP_ERROR_CHECK(esp_wifi_start());

    // wait until connected to AP
    assert(xEventGroupWaitBits(_event_group, WIFI_EVENT_CONNECTED, pdFALSE, pdFALSE, portMAX_DELAY));
    vEventGroupDelete(_event_group);
    _event_group = NULL;
    return ESP_OK;
}