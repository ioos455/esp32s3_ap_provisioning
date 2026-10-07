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
#include <stdbool.h>
#include <stdio.h>


/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
    bool key1_released = true;

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
