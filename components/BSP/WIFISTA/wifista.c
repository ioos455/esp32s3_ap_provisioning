#include "wifista.h"
#include "esp_wifi.h"
#include "spilcd.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/ip_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <string.h>

#define TAG "MY_WIFI_AP"

//sta最大连接次数
static int sta_connect_count = 0;

static esp_netif_t *esp_netif_ap = NULL;

static SemaphoreHandle_t wifi_scan_semaphore = NULL;  /*二值信号量，用于Wi-Fi扫描任务的同步*/

/* WIFI事件处理函数 */
void wifi_event_handler(void* event_handler_arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data)
{
    esp_netif_ip_info_t *ip_info;
    ip_info = (esp_netif_ip_info_t*)event_data;       /* 获取IP地址信息 */

    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_STA_START:
                spilcd_show_string(10, 120, 240, 16, 16, "Connecting...", RED);  /* 显示连接消息 */
                esp_wifi_connect();  /* 连接到Wi-Fi网络 */
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                spilcd_show_string(10, 120, 240, 16, 16, "Disconnected", RED);  /* 显示断开连接消息 */
                //连接上限
                if(sta_connect_count < 6)
                {
                    esp_wifi_connect();
                    sta_connect_count++;
                }
                else if(sta_connect_count >= 6)
                {
                    ESP_LOGI(TAG, "ESP32 disconnect ap");
                }
                break;
            case WIFI_EVENT_STA_CONNECTED:
                spilcd_show_string(10, 120, 240, 16, 16, "Connected   ", RED);  /* 显示连接消息 */
                break;

            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "Connected to AP");
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "Disconnected from AP");
                break;
            default:break;
        }
    }
    else if (event_base == IP_EVENT) 
    {
        spilcd_show_num(10, 160, esp_ip4_addr1_16(&ip_info->ip), 3, 16, GREEN);  /* 显示IP地址的第一个八位组 */
        spilcd_show_num(50, 160, esp_ip4_addr2_16(&ip_info->ip), 3, 16, GREEN);  /* 显示IP地址的第二个八位组 */
        spilcd_show_num(90, 160, esp_ip4_addr3_16(&ip_info->ip), 3, 16, GREEN);  /* 显示IP地址的第三个八位组 */
        spilcd_show_num(130, 160, esp_ip4_addr4_16(&ip_info->ip), 3, 16, GREEN);  /* 显示IP地址的第四个八位组 */
    }
}

/*初始化Wi-Fi为STA模式并注册事件处理函数*/
void wifi_sta_init(void)
{
    esp_netif_init();                                           /*初始化TCP/IP网络接口*/
    esp_event_loop_create_default();                            /*创建默认事件循环*/

    esp_netif_ap = esp_netif_create_default_wifi_ap();           //绑定ap接口
    esp_netif_create_default_wifi_sta();                         //绑定sta接口
    
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                &wifi_event_handler, NULL);     /*注册Wi-Fi事件处理函数*/
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                &wifi_event_handler, NULL);     /*注册IP事件处理函数*/

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();         /*获取默认Wi-Fi配置*/
    esp_wifi_init(&cfg);                                         /*使用默认配置初始化Wi-Fi*/
    esp_wifi_set_mode(WIFI_MODE_STA);                            /*设置Wi-Fi模式为站点*/


    wifi_scan_semaphore = xSemaphoreCreateBinary();              /* 创建二值信号量用于Wi-Fi扫描任务的同步 */
    xSemaphoreGive(wifi_scan_semaphore);                         /* 初始化信号量为可用状态 */

    esp_wifi_start();                                               /* 启动Wi-Fi驱动程序 */

}

/*
 * @brief 配置Wi-Fi为AP模式
 * @return esp_err_t 错误码
 */
esp_err_t wifi_manager_ap(void)
{
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);  /* 获取当前Wi-Fi模式 */

    if(mode == WIFI_MODE_APSTA)
    {
        return ESP_OK;  /* 如果当前模式已经是AP+STA模式，则直接返回成功 */
    }
    else
    {
        esp_wifi_disconnect();                  /* 断开当前的Wi-Fi连接 */
        esp_wifi_stop();                        /* 停止Wi-Fi驱动程序 */
        esp_wifi_set_mode(WIFI_MODE_APSTA);     /* 设置Wi-Fi模式为AP+STA */
    }

    wifi_config_t wifi_ap_config = 
    {
        .ap = 
        {
            .channel = 5,                       //wifi通道数量
            .max_connection = 4,                /* 设置最大连接数 */
            .authmode = WIFI_AUTH_WPA_WPA2_PSK  /* 设置认证模式 */
        },
    };

    //根据认证模式配置ssid和password
    snprintf((char*)wifi_ap_config.ap.ssid, 32, DEFAULT_WIFI_AP_SSID);
    wifi_ap_config.ap.ssid_len = strlen(DEFAULT_WIFI_AP_SSID);
    snprintf((char*)wifi_ap_config.ap.password, 64, DEFAULT_WIFI_AP_PASSWORD);

    esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);  /* 设置AP配置 */

    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 100, 1);            /* 设置AP的IP地址 */
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);       /* 设置AP的子网掩码 */
    IP4_ADDR(&ip_info.gw, 192, 168, 100, 1);            /* 设置AP的网关    */
    
    esp_netif_dhcps_stop(esp_netif_ap);                 /* 停止AP的DHCP服务器 */
    esp_netif_set_ip_info(esp_netif_ap, &ip_info);      /* 设置AP的IP信息 */
    esp_netif_dhcps_start(esp_netif_ap);                /* 启动AP的DHCP服务器 */

    return esp_wifi_start();  /* 启动Wi-Fi驱动程序 */
}


/* Wi-Fi扫描任务函数 */
void wifi_scan_task(void *pvParameters)
{
    p_wifi_scan_cb cb = (p_wifi_scan_cb)pvParameters;  /* 获取回调函数指针 */
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    ESP_LOGI(TAG, "Starting Wi-Fi scan, mode=%d", mode);
    esp_err_t ret = esp_wifi_scan_start(NULL, true);  /* 启动Wi-Fi扫描，阻塞直到扫描完成 */
    //启动扫描任务失败
    if (ret != ESP_OK)                                  /* 出错删除任务 */
    {
        ESP_LOGE(TAG, "Wi-Fi scan failed: %s", esp_err_to_name(ret));
        //判断回调函数是否存在
        if (cb != NULL)
        {
            //存在就清除
            cb(0, NULL);
        }
        xSemaphoreGive(wifi_scan_semaphore);            /* 获取二值信号量 */
        vTaskDelete(NULL);
        return;
    }

    //获取WiFi数量，并分配内存
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);            /* 获取找到的接入点数量 */

    wifi_ap_record_t *ap_records = ap_count > 0 ?
        (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count) : NULL;  /* 为接入点记录分配内存 */
    if (ap_count > 0 && ap_records == NULL) /* 出错删除任务 */
    {
        ESP_LOGE(TAG, "Failed to allocate memory for AP records");
        if (cb != NULL)
        {
            cb(0, NULL);
        }
        xSemaphoreGive(wifi_scan_semaphore);
        vTaskDelete(NULL);                          /* 删除任务并返回 */
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi scan finished, AP count=%d", ap_count);

    //将获取到的WiFi保存在maclloc分配的内存
    if (ap_count > 0)
    {
        ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);  /* 获取接入点记录 */
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to get Wi-Fi scan records: %s", esp_err_to_name(ret));
            ap_count = 0;
        }
    }
    if (cb != NULL)
    {
        cb(ap_count, ap_records);               /* 调用回调函数，传入网络数量和接入点记录 */
    }

    free(ap_records);                           /* 释放为接入点记录分配的内存 */
    xSemaphoreGive(wifi_scan_semaphore);        /* 如果信号量被获取，则将其归还 */
    vTaskDelete(NULL);                          /* 在完成后删除任务 */
}

/*
 * @brief 扫描Wi-Fi网络
 * @param cb 回调函数指针，用于处理扫描结果
 * @return esp_err_t 错误码
 */
esp_err_t wifi_manager_scan(p_wifi_scan_cb cb)
{
    if(pdTRUE == xSemaphoreTake(wifi_scan_semaphore, 0))  /* 尝试获取信号量，如果成功则表示没有其他扫描任务在运行 */
    {
        esp_wifi_clear_ap_list();  /* 清除之前的扫描结果 */  
        BaseType_t task_result = xTaskCreatePinnedToCore(wifi_scan_task, "wifi_scan_task", 4096, (void*)cb, 5, NULL, 1);
        if (task_result != pdPASS)
        {
            //创建任务失败
            xSemaphoreGive(wifi_scan_semaphore);
            return ESP_ERR_NO_MEM;
        }
        return ESP_OK;
    }
    else
    {
        ESP_LOGW(TAG, "Wi-Fi scan task is already running");  
        return ESP_ERR_INVALID_STATE;  /* 返回错误码，表示扫描任务已经在运行 */
    }
 
}

/** 连接wifi
 * @param ssid
 * @param password
 * @return 成功/失败
*/
esp_err_t wifi_manager_connect(const char* ssid,const char* password)
{
    sta_connect_count = 0;
    wifi_config_t wifi_config = 
    {
        .sta = 
        {
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,   //加密方式
        },
    };
    snprintf((char*)wifi_config.sta.ssid,31,"%s",ssid);
    snprintf((char*)wifi_config.sta.password,63,"%s",password);
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if(mode != WIFI_MODE_STA)
    {
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        esp_wifi_start();
    }
    else
    {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        esp_wifi_connect();
    }
    return ESP_OK;
}