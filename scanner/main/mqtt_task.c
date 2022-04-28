/**
 * @brief mqtt_client_task, fowards scan results to and control messages from MQTT broker
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
#include <stdlib.h>
#include <string.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>
#include <nvs.h>

#include "ipc.h"
#include "mqtt_task.h"

static char const * const TAG = "mqtt_task";

static EventGroupHandle_t _mqttEventGrp = NULL;
typedef enum {
	MQTT_EVENT_CONNECTED_BIT = BIT0
} mqttEvent_t;

static struct {
    char * ctrl;
    char * ctrlGroup;
} _topic;

static esp_mqtt_client_handle_t _connect2broker(ipc_t const * const ipc);  // forward decl

void
sendToMqtt(ipc_to_mqtt_typ_t const dataType, char const * const data, ipc_t const * const ipc)
{
    ipc_to_mqtt_msg_t msg = {
        .dataType = dataType,
        .data = strdup(data)
    };
    assert(msg.data);
    if (xQueueSendToBack(ipc->toMqttQ, &msg, 0) != pdPASS) {
        ESP_LOGE(TAG, "toMqttQ full");
        free(msg.data);
    }
}

static esp_err_t
_mqtt_event_cb(esp_mqtt_event_handle_t event) {

     ipc_t * const ipc = event->user_context;

	switch (event->event_id) {

        case MQTT_EVENT_DISCONNECTED:  // indicates that we got disconnected from the MQTT broker

            xEventGroupClearBits(_mqttEventGrp, MQTT_EVENT_CONNECTED_BIT);
            ESP_LOGW(TAG, "Broker disconnected");
        	// reconnect is part of the SDK
            break;

        case MQTT_EVENT_CONNECTED:  // indicates that we're connected to the MQTT broker

            xEventGroupSetBits(_mqttEventGrp, MQTT_EVENT_CONNECTED_BIT);
            ipc->dev.count.mqttConnect++;
            ESP_LOGI(TAG, "Broker connected");

            esp_mqtt_client_subscribe(event->client, _topic.ctrl, 1);
            esp_mqtt_client_subscribe(event->client, _topic.ctrlGroup, 1);
            ESP_LOGI(TAG, "Subscribed to \"%s\", \"%s\"", _topic.ctrl, _topic.ctrlGroup);
            break;

        case MQTT_EVENT_DATA:  // indicates that data is received on the MQTT control topic
        
            if (event->topic && event->data_len == event->total_data_len) {  // quietly ignores chunked messaegs

                if (strncmp("restart", event->data, event->data_len) == 0) {

                    sendToMqtt(IPC_TO_MQTT_MSGTYPE_RESTART, "{ \"response\": \"restarting\" }", ipc);
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    esp_restart();

                } else if (strncmp("who", event->data, event->data_len) == 0) {

                    esp_partition_t const * const running_part = esp_ota_get_running_partition();
                    esp_app_desc_t running_app_info;
                    ESP_ERROR_CHECK(esp_ota_get_partition_description(running_part, &running_app_info));

                    wifi_ap_record_t ap_info;
                    ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));

                    char * payload;
                    int const payload_len = asprintf(&payload,
                        "{ \"ble\": {\"name\": \"%s\", \"address\": \"%s\"}, \"firmware\": { \"version\": \"%s.%s\", \"date\": \"%s %s\" }, \"wifi\": { \"connect\": %u, \"address\": \"%s\", \"SSID\": \"%s\", \"RSSI\": %d }, \"mqtt\": { \"connect\": %u }, \"mem\": { \"heap\": %u } }",
                        ipc->dev.name, ipc->dev.bda,
                        running_app_info.project_name, running_app_info.version,
                        running_app_info.date, running_app_info.time,
                        ipc->dev.count.wifiConnect, ipc->dev.ipAddr, ap_info.ssid, ap_info.rssi,
                        ipc->dev.count.mqttConnect, heap_caps_get_free_size(MALLOC_CAP_8BIT));

                    assert(payload_len >= 0);
                    sendToMqtt(IPC_TO_MQTT_MSGTYPE_WHO, payload, ipc);
                    free(payload);

                } else {
                    sendToBle(IPC_TO_BLE_TYP_CTRL, event->data, event->data_len, ipc);
                }
            }
            break;

        default:
            break;
	}
	return ESP_OK;
}

/*
 * Connect to MQTT broker
 * The `MQTT_EVENT_CONNECTED_BIT` in `mqttEventGrp` indicates that we're connected to the MQTT broker.
 * This bit is controlled by the `mqtt_event_cb`
 */

static esp_mqtt_client_handle_t
_connect2broker(ipc_t const * const ipc) {

    char * mqtt_url = NULL;

#ifdef CONFIG_BLESCAN_HARDCODED_MQTT_CREDENTIALS
    ESP_LOGW(TAG, "Using mqtt_url from Kconfig");
    mqtt_url = strdup(CONFIG_BLESCAN_HARDCODED_MQTT_URL);
#else
    ESP_LOGW(TAG, "Using mqtt_url from nvram");
    nvs_handle_t nvs_handle;
    size_t len;
    if (nvs_open("storage", NVS_READONLY, &nvs_handle) == ESP_OK &&
        nvs_get_str(nvs_handle, "mqtt_url", NULL, &len) == ESP_OK) {
            
        mqtt_url = (char *) malloc(len);
        ESP_ERROR_CHECK(nvs_get_str(nvs_handle, "mqtt_url", mqtt_url, &len));
    }
#endif
    if (mqtt_url == NULL || *mqtt_url == '\0') {
        return NULL;
    }

    ESP_LOGW(TAG, "read mqtt_url (%s)", mqtt_url);

    esp_mqtt_client_handle_t client;
    xEventGroupClearBits(_mqttEventGrp, MQTT_EVENT_CONNECTED_BIT);
    {
        const esp_mqtt_client_config_t mqtt_cfg = {
            .event_handle = _mqtt_event_cb,
            .user_context = (void *) ipc,
            .uri = mqtt_url
        };
        client = esp_mqtt_client_init(&mqtt_cfg);
        //ESP_ERROR_CHECK(esp_mqtt_client_set_uri(client, CONFIG_POOL_MQTT_URL));
        ESP_ERROR_CHECK(esp_mqtt_client_start(client));
    }
	assert(xEventGroupWaitBits(_mqttEventGrp, MQTT_EVENT_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY));
    free(mqtt_url);
    return client;
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

static char const *
_type2subtopic(ipc_to_mqtt_typ_t const type)
{
    struct mapping {
        ipc_to_mqtt_typ_t const type;
        char const * const subtopic;
    } mapping[] = {
        { IPC_TO_MQTT_MSGTYPE_SCAN, "scan" },
        { IPC_TO_MQTT_MSGTYPE_RESTART, "restart" },
        { IPC_TO_MQTT_MSGTYPE_WHO, "who" },
        { IPC_TO_MQTT_MSGTYPE_MODE, "mode" },
        { IPC_TO_MQTT_MSGTYPE_DBG, "dbg" },
    };
    for (uint ii = 0; ii < ARRAY_SIZE(mapping); ii++) {
        if (type == mapping[ii].type) {
            return mapping[ii].subtopic;
        }
    }
    return NULL;
}

static void
_wait4ipcDevAvail(ipc_t * ipc)
{
    // ble sends a msg when ipc->dev is initialized
    ipc_to_mqtt_msg_t msg;
    assert(xQueueReceive(ipc->toMqttQ, &msg, (TickType_t)(1000L / portTICK_PERIOD_MS)) == pdPASS);
    assert(msg.dataType == IPC_TO_MQTT_IPC_DEV_AVAILABLE);
    free(msg.data);
}

void
mqtt_task(void * ipc_void) {

    ESP_LOGI(TAG, "starting ..");
	ipc_t * ipc = ipc_void;

    _wait4ipcDevAvail(ipc);
    assert(asprintf(&_topic.ctrl, "%s/%s", CONFIG_BLESCAN_MQTT_CTRL_TOPIC, ipc->dev.name));
    assert(asprintf(&_topic.ctrlGroup, "%s", CONFIG_BLESCAN_MQTT_CTRL_TOPIC));

    // event group indicates that we're connected to the MQTT broker

	_mqttEventGrp = xEventGroupCreate();
    esp_mqtt_client_handle_t const client = _connect2broker(ipc);
    if (client == NULL) {
        ESP_LOGE(TAG, "MQTT not provisioned");
        _delete_task();
    }

	while (1) {
        ipc_to_mqtt_msg_t msg;
		if (xQueueReceive(ipc->toMqttQ, &msg, (TickType_t)(1000L / portTICK_PERIOD_MS)) == pdPASS) {

            char * topic;
            char const * const subtopic = _type2subtopic(msg.dataType);
            if (subtopic) {
                asprintf(&topic, "%s/%s/%s", CONFIG_BLESCAN_MQTT_DATA_TOPIC, subtopic, ipc->dev.name);
            } else {
                asprintf(&topic, "%s/%s", CONFIG_BLESCAN_MQTT_DATA_TOPIC, ipc->dev.name);
            }
            esp_mqtt_client_publish(client, topic, msg.data, strlen(msg.data), 1, 0);
            free(topic);
            free(msg.data);
		}
	}
}