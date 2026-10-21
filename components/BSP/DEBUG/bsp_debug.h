#ifndef __BSP_DEBUG_H_
#define __BSP_DEBUG_H_

#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"

/*
 * 调试诊断开关
 *   1 = 启用：注册 /debug 页面，并统计 WebSocket 握手/收帧/发送次数
 *   0 = 禁用（默认）：不注册页面、不统计、不占 RAM，所有接口编译为空操作
 *
 * 需要临时开启时，把下面的默认值改成 1 重新编译即可。
 */
#ifndef BSP_DEBUG_ENABLE
#define BSP_DEBUG_ENABLE 0
#endif

#if BSP_DEBUG_ENABLE

/* WebSocket 握手成功一次 */
void bsp_debug_ws_handshake(void);

/* 收到一帧 WebSocket 数据 */
void bsp_debug_ws_frame(void);

/* 发送一次 WebSocket 数据，ok 表示是否成功 */
void bsp_debug_ws_send(bool ok);

/* 向 httpd 注册 /debug 诊断页（由 web_server_start 内部调用） */
esp_err_t bsp_debug_register_uri(httpd_handle_t server);

#else /* BSP_DEBUG_ENABLE == 0：空实现，调用处无需写 #if */

static inline void bsp_debug_ws_handshake(void) {}
static inline void bsp_debug_ws_frame(void) {}
static inline void bsp_debug_ws_send(bool ok) { (void)ok; }

static inline esp_err_t bsp_debug_register_uri(httpd_handle_t server)
{
    (void)server;
    return ESP_OK;
}

#endif /* BSP_DEBUG_ENABLE */

#endif /* __BSP_DEBUG_H_ */
