#include "ap_wifi.h"
#include "wifista.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "ws_server.h"
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "cJSON.h"


#define TAG "SPIFFS"
#define SPIFFS_MOUNT "/spiffs"              /* SPIFFS挂载点 */
#define HTML_PATH "/spiffs/apcfg.html"      /* HTML文件路径 */
#define APCFG_BIT (BIT0)                    /* 事件标志位 */


static EventGroupHandle_t apcfg_ev;         /* apcfg事件组 */
static char *html_code = NULL;  /* 用于存储HTML代码的缓冲区 */
static char current_ssid[32];
static char current_password[64];
static bool ap_started = false;

/*
 * @brief 初始化Web页面缓冲区
 * @return char* 缓冲区指针
 */
static char *init_web_page_buffer(void)
{
    esp_vfs_spiffs_conf_t conf = 
    {
        .base_path = SPIFFS_MOUNT,          /* SPIFFS挂载点 */
        .format_if_mount_failed = false,    /* 如果挂载失败，是否格式化文件系统 */
        .max_files = 3,                     /* 最大同时打开的文件数 */
        .partition_label = NULL,            /* 分区标签，设置为NULL表示使用第一个subtype=spiffs的分区 */
    };
    esp_vfs_spiffs_register(&conf);  /* 注册SPIFFS文件系统 */
    struct stat st;
    if (stat(HTML_PATH, &st) != 0)
    {
        ESP_LOGE(TAG, "Failed to stat file: %s", HTML_PATH);  /* 如果文件不存在，打印错误信息 */
        return NULL;
    }               
    char *buffer = (char *)malloc(st.st_size + 1);  /* 分配缓冲区 */
    if (buffer == NULL)
    ESP_LOGE(TAG, "Failed to allocate memory for file buffer");  /* 如果内存分配失败，打印错误信息 */
    
    memset(buffer, 0, st.st_size + 1);  /* 初始化缓冲区 */

    FILE *file = fopen(HTML_PATH, "r");  /* 只读模式打开HTML文件 */
    if (file)
    {
        if (fread(buffer, st.st_size, 1, file) == 0) /* 读取文件内容到缓冲区 */
        {
            ESP_LOGE(TAG, "Failed to read file: %s", HTML_PATH);  /* 如果读取失败，打印错误信息 */
            free(buffer);
            buffer = NULL;
        }
        fclose(file);                   /* 关闭文件 */
    }
    else
    {
        free(buffer);                   /* 释放内存 */
        buffer = NULL;
    }
    return buffer;  /* 返回缓冲区指针 */

}

//接受ssid，password
static void ap_wifi_task(void *param)
{
    EventBits_t ev;
    while(1)
    {
        ev = xEventGroupWaitBits(apcfg_ev, APCFG_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
        if(ev & APCFG_BIT)
        {
            web_server_stop();          /* 断开web服务 */
            wifi_manager_connect(current_ssid, current_password);     /* 连接WiFi */
        }
    }
}

//初始化函数
void ap_wifi_init(void)
{
    wifi_sta_init();                        /* WIFI初始化 */
    html_code = init_web_page_buffer();     /* 初始化HTML页面缓冲区 */
    if (html_code == NULL)
    {
        ESP_LOGE(TAG, "Web page is unavailable; AP page will not work");
    }
    apcfg_ev = xEventGroupCreate();         /* 创建任务时间组 */
    xTaskCreatePinnedToCore(ap_wifi_task, "apcfg", 4096, NULL, 3, NULL, 1);
}


void wifi_sacn_cb(uint8_t num_networks, wifi_ap_record_t *ap_records)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *wifilist_js = cJSON_AddArrayToObject(root, "wifi_list");
    for(int i = 0;i < num_networks;i++)
    {
        cJSON *wifi_js = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi_js, "ssid", (char*)ap_records[i].ssid);
        cJSON_AddNumberToObject(wifi_js, "rssi", ap_records[i].rssi);
        if(ap_records[i].authmode == WIFI_AUTH_OPEN)
            cJSON_AddBoolToObject(wifi_js, "encrypted", 0);
        else
            cJSON_AddBoolToObject(wifi_js, "encrypted", 1);
        cJSON_AddItemToArray(wifilist_js, wifi_js);
    }
    char *data = cJSON_Print(root);
    if (data == NULL)
    {
        ESP_LOGE(TAG, "Failed to serialize Wi-Fi scan result");
    }
    else
    {
        ESP_LOGI(TAG, "WS send %u networks", num_networks);
        esp_err_t ret = web_server_send((uint8_t*)data, strlen(data));
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to send Wi-Fi scan result: %s", esp_err_to_name(ret));
        }
        cJSON_free(data);
    }
    cJSON_Delete(root);
}

/* 解析JSON回调函数 */
static void ws_receive_cb(uint8_t *data, uint16_t len)
{
    ESP_LOGI(TAG, "WebSocket request received, length=%u", len);
    cJSON *root = cJSON_ParseWithLength((char *)data, len);
    if(root)
    {
        cJSON *scan_js = cJSON_GetObjectItem(root, "scan");
        cJSON *ssid_js = cJSON_GetObjectItem(root, "ssid");
        cJSON *password_js = cJSON_GetObjectItem(root, "password");
        if(scan_js)
        {
            char *scan_value = cJSON_GetStringValue(scan_js);
            if(strcmp(scan_value, "start") == 0)
            {
                //启动扫描
                ESP_LOGI(TAG, "Starting scan task");
                esp_err_t ret = wifi_manager_scan(wifi_sacn_cb);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to start Wi-Fi scan: %s", esp_err_to_name(ret));
                }
            }
        }
        if(ssid_js && password_js)
        {
            char *ssid_value = cJSON_GetStringValue(ssid_js);
            char *password_value = cJSON_GetStringValue(password_js);
            snprintf(current_ssid, sizeof(current_ssid), "%s", ssid_value);
            snprintf(current_password, sizeof(current_password), "%s", password_value);
            xEventGroupSetBits(apcfg_ev, APCFG_BIT);
            //wifi_manager_connect(ssid_value, password_value);         会切换成STA模式
        }
        cJSON_Delete(root);
    }
}

/*配置为AP+STA模式*/
void ap_wifi_apcfg(void)
{
    if (ap_started)
    {
        return;
    }

    esp_err_t ret = wifi_manager_ap();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start Wi-Fi AP: %s", esp_err_to_name(ret));
        return;
    }

    ws_cfg_t ws_cfg = 
    {
        .html_code = html_code,
        .receive_cb = ws_receive_cb,
    };                 
    ret = web_server_start(&ws_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(ret));
        return;
    }

    ap_started = true;
    ESP_LOGI(TAG, "Wi-Fi AP started, SSID: ESP32S3_AP, IP: 192.168.100.1");

}