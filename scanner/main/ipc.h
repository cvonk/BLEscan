#pragma once

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#endif
#define ALIGN( type ) __attribute__((aligned( __alignof__( type ) )))
#define PACK( type )  __attribute__((aligned( __alignof__( type ) ), packed ))
#define PACK8  __attribute__((aligned( __alignof__( uint8_t ) ), packed ))
#ifndef MIN
#define MIN(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
#endif
#ifndef MAX
#define MAX(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#endif
#ifndef ELEM_AT
# define ELEM_AT(a, i, v) ((uint) (i) < ARRAY_SIZE(a) ? (a)[(i)] : (v))
#endif
#ifndef ELEM_POS
# define ELEM_POS(a, s) \
    do { \
      for (uint_least8_t ii = 0; ii < ARRAY_SIZE(a); ii++) { \
	    if (strcasecmp(s, a[ii]) == 0) { \
	      return ii; \
	    } \
      } \
      return -1; \
    } while(0)
#endif

#define BLE_DEVNAME_LEN (32)
#define BLE_DEVMAC_LEN (6 * 3)
#define WIFI_DEVNAME_LEN (32)
#define WIFI_DEVIPADDR_LEN (16)

typedef struct ipc_t {
    QueueHandle_t toBleQ;
    QueueHandle_t toMqttQ;
    struct dev {
        char bda[BLE_DEVMAC_LEN];
        char ipAddr[WIFI_DEVIPADDR_LEN];
        char name[WIFI_DEVNAME_LEN];
        struct count {
            uint wifiAuthErr;
            uint wifiConnect;
            uint mqttConnect;
        } count;
    } dev;
} ipc_t;

// to MQTT

typedef enum ipc_to_mqtt_typ_t {
    IPC_TO_MQTT_IPC_DEV_AVAILABLE,
    IPC_TO_MQTT_MSGTYPE_SCAN,
    IPC_TO_MQTT_MSGTYPE_RESTART,
    IPC_TO_MQTT_MSGTYPE_WHO,
    IPC_TO_MQTT_MSGTYPE_MODE,
    IPC_TO_MQTT_MSGTYPE_DBG
} ipc_to_mqtt_typ_t;

typedef struct ipc_to_mqtt_msg_t {
    ipc_to_mqtt_typ_t  dataType;
    char *             data;  // must be freed by recipient
} ipc_to_mqtt_msg_t;

// to BLE

typedef enum ipc_to_ble_typ_t {
    IPC_TO_BLE_TYP_CTRL
} ipc_to_ble_typ_t;

typedef struct ipc_to_ble_msg_t {
    ipc_to_ble_typ_t  dataType;
    char *            data;  // must be freed by recipient
} ipc_to_ble_msg_t;

void sendToBle(ipc_to_ble_typ_t const dataType, char const * const data, size_t const data_len, ipc_t const * const ipc);
void sendToMqtt(ipc_to_mqtt_typ_t const dataType, char const * const data, ipc_t const * const ipc);