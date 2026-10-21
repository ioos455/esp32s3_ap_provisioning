#ifndef __WIFISTA_H_
#define __WIFISTA_H_

#include "esp_wifi.h"
#include "esp_err.h"
#include "wifi_secrets.h"               /* 敏感配置：SSID / 密码，见 wifi_secrets.h.example */

/* 以下 4 个宏来自 wifi_secrets.h（该文件已被 .gitignore 忽略）
 * 首次编译时由 CMake 自动从 wifi_secrets.h.example 生成 */
#define DEFAULT_WIFI_SSID           WIFI_SECRET_STA_SSID
#define DEFAULT_WIFI_PASSWORD       WIFI_SECRET_STA_PASSWORD
#define DEFAULT_WIFI_AP_SSID        WIFI_SECRET_AP_SSID
#define DEFAULT_WIFI_AP_PASSWORD    WIFI_SECRET_AP_PASSWORD


/*
 * @brief 扫描回调函数
 * @param num_networks WiFi数量
 * @param *ap_records WiFi信息
 * @param result 扫描结果，ESP_OK表示扫描成功（此时列表可能为空）
*/
typedef void (*p_wifi_scan_cb)(uint8_t num_networks, wifi_ap_record_t *ap_records, esp_err_t result);

/*
 * @brief 初始化Wi-Fi站点模式
 * @return void
 */
void wifi_sta_init(void);                       /* WIFI初始化 */

/*
 * @brief Wi-Fi扫描任务函数
 * @param pvParameters 任务参数
 * @return void
 */
esp_err_t wifi_manager_ap(void);                /* WIFI AP模式配置 */

/*
 * @brief 扫描Wi-Fi网络
 * @param cb 回调函数指针，用于处理扫描结果
 * @return esp_err_t 错误码
 */
esp_err_t wifi_manager_scan(p_wifi_scan_cb cb);  /* WIFI扫描 */

/** 连接wifi
 * @param ssid
 * @param password
 * @return 成功/失败
*/
esp_err_t wifi_manager_connect(const char* ssid,const char* password);

#endif
