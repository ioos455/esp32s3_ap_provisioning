#ifndef __WS_SERVER_H_
#define __WS_SERVER_H_

#include "esp_err.h"

typedef void (*web_receive_cb)(uint8_t *data, uint16_t len);

typedef struct {
    web_receive_cb receive_cb;                                          //websocket接收回调函数
    const char *html_code;                                              //网页内容
}ws_cfg_t;


esp_err_t web_server_start(ws_cfg_t *cfg);
esp_err_t web_server_stop(void);
esp_err_t web_server_send(uint8_t *data, uint16_t len);



#endif // __WS_SERVER_H_