#ifndef __LED_H
#define __LED_H

#include "driver/gpio.h"

/* 引脚定义 */
#define LED0_GPIO_PIN    GPIO_NUM_43     /* LED0连接的GPIO端口 */
#define LED1_GPIO_PIN    GPIO_NUM_44     /* LED1连接的GPIO端口 */

/* LED0端口定义 */
#define LED0(x)          do { x ?                                \
                              gpio_set_level(LED0_GPIO_PIN, 1):  \
                              gpio_set_level(LED0_GPIO_PIN, 0);  \
                            } while(0)  /* LED0翻转 */

/* LED1端口定义 */
#define LED1(x)          do { x ?                                \
                              gpio_set_level(LED1_GPIO_PIN, 1):  \
                              gpio_set_level(LED1_GPIO_PIN, 0);  \
                            } while(0)  /* LED1翻转 */

/* LED取反定义 */
#define LED0_TOGGLE()    do { gpio_set_level(LED0_GPIO_PIN, !gpio_get_level(LED0_GPIO_PIN)); } while(0)  /* LED0翻转 */
#define LED1_TOGGLE()    do { gpio_set_level(LED1_GPIO_PIN, !gpio_get_level(LED1_GPIO_PIN)); } while(0)  /* LED1翻转 */

/* 函数声明*/
void led_init(void);     /* 初始化LED */

#endif  /* __LED_H */
