#include "bsp_debug.h"

#if BSP_DEBUG_ENABLE

#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include "ws_server.h"

#define TAG "bsp_debug"

/* 运行时计数器，通过 /debug 页面输出，用于在没有串口日志时定位问题 */
static uint32_t s_handshakes = 0;
static uint32_t s_frames = 0;
static uint32_t s_send_ok = 0;
static uint32_t s_send_fail = 0;

void bsp_debug_ws_handshake(void)
{
    s_handshakes++;
}

void bsp_debug_ws_frame(void)
{
    s_frames++;
}

void bsp_debug_ws_send(bool ok)
{
    if (ok)
    {
        s_send_ok++;
    }
    else
    {
        s_send_fail++;
    }
}

/*
 * @brief 运行时诊断页面 /debug
 *
 * 本项目 LED 占用 GPIO43/44，与控制台 UART0 冲突，串口基本看不到运行日志，
 * 因此把关键状态通过 HTTP 输出，方便用手机浏览器直接查看。
 */
static esp_err_t http_debug_req(httpd_req_t *r)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        return httpd_resp_send_500(r);
    }

    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_wifi_get_mode(&mode);

    wifi_ap_record_t ap_info;
    bool sta_connected = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);

    cJSON_AddNumberToObject(root, "reset_reason", esp_reset_reason());
    cJSON_AddNumberToObject(root, "uptime_ms", esp_log_timestamp());
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "wifi_mode", mode);
    cJSON_AddBoolToObject(root, "sta_connected", sta_connected);
    if (sta_connected)
    {
        cJSON_AddNumberToObject(root, "sta_rssi", ap_info.rssi);
    }
    cJSON_AddNumberToObject(root, "ws_fd", web_server_client_fd());
    cJSON_AddBoolToObject(root, "ws_connected", web_server_client_connected());
    cJSON_AddNumberToObject(root, "ws_handshakes", s_handshakes);
    cJSON_AddNumberToObject(root, "ws_frames", s_frames);
    cJSON_AddNumberToObject(root, "ws_send_ok", s_send_ok);
    cJSON_AddNumberToObject(root, "ws_send_fail", s_send_fail);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (out == NULL)
    {
        return httpd_resp_send_500(r);
    }

    httpd_resp_set_type(r, "application/json");
    esp_err_t ret = httpd_resp_send(r, out, HTTPD_RESP_USE_STRLEN);
    cJSON_free(out);
    return ret;
}

esp_err_t bsp_debug_register_uri(httpd_handle_t server)
{
    if (server == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_uri_t uri_debug = {
        .uri      = "/debug",
        .method   = HTTP_GET,
        .handler  = http_debug_req,
        .user_ctx = NULL,
    };

    esp_err_t ret = httpd_register_uri_handler(server, &uri_debug);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Debug URI registration failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

#endif /* BSP_DEBUG_ENABLE */
