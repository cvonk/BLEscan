idf_component_register( SRCS
                            "main.c"
                            "mqtt_task.c"
                            "ble_task.c"
                        INCLUDE_DIRS
                            "."
                            "../components/factory_reset_task/include"
                            "../components/ota_update_task/include"
                            "../components/wifi_connect/include"
                            "../components/esp_ibeacon_api/include"
)
