#ifndef __WS_SERVER_H_
#define __WS_SERVER_H_

#include "esp_err.h"

//web接收回调函数
typedef void (*web_receive_cb)(uint8_t *data, uint16_t len);


//web结构体
typedef struct {
    web_receive_cb receive_cb;                                          //websocket接收回调函数
    const char *html_code;                                              //网页内容
}ws_cfg_t;

/*
 * @brief 启动Web服务器，并注册网页和WebSocket URI处理函数
 * @param cfg Web服务器配置结构体，包含 HTML代码和回调函数
 * @return 成功返回 ESP_OK，参数错误返回 ESP_FAIL
 */
esp_err_t web_server_start(ws_cfg_t *cfg);

/*
 * @brief 停止已启动的Web服务器
 * @return 始终返回 ESP_OK，表示停止操作已完成
 */
esp_err_t web_server_stop(void);

/*
 * @brief 向已连接的WebSocket客户端发送文本消息
 * @param data 要发送的文本数据缓冲区
 * @param len 数据长度
 * @return 成功返回 ESP_OK，失败返回对应错误码
 */
esp_err_t web_server_send(uint8_t *data, uint16_t len);



#endif // __WS_SERVER_H_