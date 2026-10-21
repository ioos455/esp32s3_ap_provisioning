#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "myiic.h"
#include "my_spi.h"
#include "spilcd.h"
#include "xl9555.h"
#include "wifista.h"
#include "ap_wifi.h"    
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include <stdbool.h>
#include <stdio.h>


/* 复位原因，便于区分 崩溃/看门狗/欠压/上电 */
static const char *reset_reason_str(esp_reset_reason_t reason)
{
    switch (reason)
    {
        case ESP_RST_POWERON:  return "POWERON";
        case ESP_RST_EXT:      return "EXT_PIN";
        case ESP_RST_SW:       return "SW(esp_restart)";
        case ESP_RST_PANIC:    return "PANIC(abort/assert/crash)";
        case ESP_RST_INT_WDT:  return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT:      return "WDT";
        case ESP_RST_DEEPSLEEP:return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT(供电)";
        case ESP_RST_SDIO:     return "SDIO";
        default:               return "UNKNOWN";
    }
}


/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
    bool key1_released = true;

    /* 必须先初始化NVS，否则Wi-Fi/PHY校准数据无法保存 */
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    ESP_LOGW("MAIN", "reset reason = %s, free heap = %u",
             reset_reason_str(esp_reset_reason()), esp_get_free_heap_size());

    ESP_LOGI("MAIN", "Initializing LED");
    led_init();                 /* LED初始化 */
    ESP_LOGI("MAIN", "Initializing SPI");
    my_spi_init();              /* SPI初始化 */
    ESP_LOGI("MAIN", "Initializing I2C");
    myiic_init();               /* IIC初始化 */  
    ESP_LOGI("MAIN", "Initializing XL9555");
    xl9555_init();              /* 初始化按键 */
    ESP_LOGI("MAIN", "Initializing LCD");
    spilcd_init();              /* LCD屏初始化 */
    ESP_LOGI("MAIN", "Initializing Wi-Fi");
    ap_wifi_init();             /* Wi-Fi初始化 */
    ESP_LOGI("MAIN", "Initialization complete");
    
    while (1)
    {
        //KEY按键按下触发ap配网，iic加互斥锁
        if(KEY1 == 0 && key1_released)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            if(KEY1 == 0)
            {
                ESP_LOGI("MAIN", "KEY1 pressed, starting Wi-Fi AP");
                
                ap_wifi_apcfg();
                key1_released = false;
            }
        }
        else if(KEY1 != 0)
        {
            key1_released = true;
        }
        
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
