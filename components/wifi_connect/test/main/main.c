/**
  * @brief demonstrates the use of the "wifi_connect" component
  **/
// Copyright © 2020, Coert Vonk
// SPDX-License-Identifier: MIT

#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include <esp_netif.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_http_server.h>

#include <wifi_connect.h>

static char const * const TAG = "main";

typedef struct wifi_connect_priv_t {
    uint connectCnt;
    httpd_handle_t httpd_handle;
    httpd_uri_t * httpd_uri;
} wifi_connect_priv_t;

static void
_initNvsFlash(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static esp_err_t
_httpd_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "No such method");
        return ESP_FAIL;
    }
    const char * resp_str = "<html>\n"
        "<head>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>Welcome to device world</h1>\n"
        "</body>\n"
        "</html>";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

static esp_err_t
_wifi_connect_cb(void * const priv_void, esp_ip4_addr_t const * const ip)
{
    wifi_connect_priv_t * const priv = priv_void;
    priv->connectCnt++;
    ESP_LOGI(TAG, "WiFi connectCnt = %u", priv->connectCnt);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    esp_err_t err = httpd_start(&priv->httpd_handle, &config);
    if (err != ESP_OK) {
       ESP_LOGI(TAG, "Error starting server!");
    } else {
        httpd_register_uri_handler(priv->httpd_handle, priv->httpd_uri);
        ESP_LOGI(TAG, "Listening at http://" IPSTR "/", IP2STR(ip));
    }
    return ESP_OK;
}

static esp_err_t
_wifi_disconnect_cb(void * const priv_void)
{
    wifi_connect_priv_t * const priv = priv_void;
    httpd_stop(priv->httpd_handle);
    return ESP_OK;
}

void
app_main(void)
{
    _initNvsFlash();

    httpd_uri_t httpd_uri = {
        .uri       = "/*",
        .method    = HTTP_GET,
        .handler   = _httpd_handler,
        .user_ctx  = NULL
    };
    wifi_connect_priv_t priv = {
        .connectCnt = 0,
        .httpd_uri = &httpd_uri,
    };
    wifi_connect_config_t wifi_connect_config = {
        .onConnect = _wifi_connect_cb,
        .onDisconnect = _wifi_disconnect_cb,
        .priv = &priv,
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
    ESP_LOGI(TAG, "Connected to WiFi AP");
    while (1) {
        // do something
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}