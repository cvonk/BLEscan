#pragma once

#define BLE_DEVNAME_LEN (32)
#define BLE_DEVMAC_LEN (6 * 3)
#define WIFI_DEVIPADDR_LEN (16)

typedef struct ipc_t {
    QueueHandle_t toBleQ;
    QueueHandle_t toMqttQ;
    struct dev {
        char bda[BLE_DEVMAC_LEN];
        char name[BLE_DEVNAME_LEN];
        char ipAddr[WIFI_DEVIPADDR_LEN];
        struct connectCnt {
            uint wifi;
            uint mqtt;
        } connectCnt;
    } dev;

} ipc_t;

typedef enum toMqttMsgType_t {
    TO_MQTT_IPC_DEV_AVAILABLE,
    TO_MQTT_MSGTYPE_SCAN,
    TO_MQTT_MSGTYPE_RESTART,
    TO_MQTT_MSGTYPE_WHO,
    TO_MQTT_MSGTYPE_MODE,
    TO_MQTT_MSGTYPE_DBG,
} toMqttMsgType_t;

typedef struct toMqttMsg_t {
    toMqttMsgType_t dataType;
    char * data;  // must be freed by recipient
} toMqttMsg_t;

typedef enum toBleMsgType_t {
    TO_BLE_MSGTYPE_CTRL
} toBleMsgType_t;

typedef struct toBleMsg_t {
    toBleMsgType_t dataType;
    char * data;  // must be freed by recipient
} toBleMsg_t;

void sendToBle(toBleMsgType_t const dataType, char const * const data, ipc_t const * const ipc);
void sendToMqtt(toMqttMsgType_t const dataType, char const * const data, ipc_t const * const ipc);