#include "ws_server.h"
#include "esp_log.h"
#include "esp_http_server.h"

#define TAG "ws_server"

/* 网页HTML内容缓存，用于返回浏览器首页 */
static const char *http_html = NULL;

/* WebSocket接收数据回调函数，收到消息后由外部模块处理 */
static web_receive_cb receive_cb = NULL;

/* HTTPD服务实例句柄，唯一标识当前已启动的Web服务器 */
static httpd_handle_t server_handle = NULL;

/* 当前已建立WebSocket连接的客户端socket fd，用于向客户端发送数据 */
static int client_fds = 0;

/*
 * @brief 处理HTTP GET请求，返回网页HTML内容
 *
 * 当浏览器访问根路径 " / " 时，会调用此函数向客户端发送网页代码，
 * 让浏览器加载前端界面。
 *
 * @param r 当前HTTP请求对象
 * @return 成功返回 ESP_OK，失败返回对应错误码
 */
esp_err_t http_get_req(httpd_req_t *r)
{
    return httpd_resp_send(r, http_html, HTTPD_RESP_USE_STRLEN); // 发送网页内容给浏览器
}

/*
 * @brief 处理WebSocket握手和数据帧接收
 *
 * 该函数同时处理两种情况：
 * 1. HTTP GET握手请求：建立WebSocket连接，并保存客户端socket fd；
 * 2. WebSocket数据帧：读取客户端发送来的文本消息，并回调外部 receive_cb。
 *
 * @param r 当前HTTP/WebSocket请求对象
 * @return 成功返回 ESP_OK，失败返回对应错误码
 */
esp_err_t ws_get_req(httpd_req_t *r)
{
    if(r->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "WebSocket handshake received");
        client_fds = httpd_req_to_sockfd(r);                        // 保存当前客户端socket fd
        return ESP_OK;                                              // 握手成功，建立WebSocket连接
    }

    httpd_ws_frame_t ws_pkt;                                            // WebSocket数据包结构体
    esp_err_t ret;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));                       // 初始化结构体
    ret = httpd_ws_recv_frame(r, &ws_pkt, 0);                           // 先读取帧头信息，获取payload长度等参数
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame info");
        return ret;
    }
    uint8_t *buf = (uint8_t *)malloc(ws_pkt.len + 1);                   // 分配接收缓冲区
    if (buf == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for websocket payload");
        return ESP_ERR_NO_MEM;
    }

    ws_pkt.payload = buf;                                               // 设置payload指向缓冲区
    ret = httpd_ws_recv_frame(r, &ws_pkt, ws_pkt.len);                  // 实际读取数据载荷
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get payload");
        free(buf);
        return ret;
    }
    buf[ws_pkt.len] = '\0';
    if (ret == ESP_OK)
    {
        if(ws_pkt.type == HTTPD_WS_TYPE_TEXT)
        {
            if(receive_cb != NULL)
            {
                ESP_LOGI(TAG, "Received websocket text message: %s", ws_pkt.payload);
                receive_cb(ws_pkt.payload, (uint16_t)ws_pkt.len);                 // 调用外部回调处理收到的文本消息
            }
        }
    }
    free(buf);                                                          // 释放临时缓冲区
    return ESP_OK;
}

/*
 * @brief 启动Web服务器，并注册网页和WebSocket URI处理函数
 *
 * 该函数完成以下工作：
 * 1. 保存网页HTML内容和接收回调函数；
 * 2. 初始化并启动HTTPD服务；
 * 3. 注册根路径 "/" 对应网页页面；
 * 4. 注册 "/ws" 对应WebSocket连接，供浏览器建立实时通信。
 *
 * @param cfg Web服务器配置结构体，包含 HTML代码和回调函数
 * @return 成功返回 ESP_OK，参数错误返回 ESP_FAIL
 */
esp_err_t web_server_start(ws_cfg_t *cfg)
{
    if(cfg == NULL) return ESP_FAIL;
    http_html = cfg->html_code;                                         // 设置网页内容
    receive_cb = cfg->receive_cb;                                       // 设置消息接收回调

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();                     // 获取默认HTTPD配置
    esp_err_t ret = httpd_start(&server_handle, &config);               // 启动HTTPD服务
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(ret));
        server_handle = NULL;
        return ret;
    }

    httpd_uri_t uri_http = {
        .uri       = "/",                                             // 网页首页路径
        .method    = HTTP_GET,                                         // 只允许GET请求
        .handler   = http_get_req,                                     // 处理首页访问
        .user_ctx  = NULL,
    };
    ret = httpd_register_uri_handler(server_handle, &uri_http);         // 注册网页访问处理函数
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "HTTP URI registration failed: %s", esp_err_to_name(ret));
        httpd_stop(server_handle);
        server_handle = NULL;
        return ret;
    }

    httpd_uri_t uri_ws = {
        .uri       = "/ws",                                           // WebSocket连接路径
        .method    = HTTP_GET,                                          // WebSocket建立时使用GET握手
        .handler   = ws_get_req,                                        // 处理WebSocket消息
        .user_ctx  = NULL,
        .is_websocket = true,                                           // 标记为WebSocket URI
    };
    ret = httpd_register_uri_handler(server_handle, &uri_ws);           // 注册WebSocket处理函数
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "WebSocket URI registration failed: %s", esp_err_to_name(ret));
        httpd_stop(server_handle);
        server_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "HTTP server started, WebSocket endpoint: /ws");

    return ESP_OK;
}

/*
 * @brief 停止已启动的Web服务器
 *
 * 该函数在需要关闭网页服务、重启服务或切换页面时调用，用于释放
 * HTTPD 监听线程和套接字资源，避免再次启动时发生冲突。
 *
 * 逻辑：检查 server_handle 是否已存在；若存在则调用 httpd_stop 关闭
 * 服务，并将句柄置空，表示当前没有可用的HTTPD服务实例。
 *
 * @return 始终返回 ESP_OK，表示停止操作已完成
 */
esp_err_t web_server_stop(void)
{
    if(server_handle)
    {
        httpd_stop(server_handle);                                      // 停止 httpd 服务
        server_handle = NULL;                                            // 清空服务句柄，避免重复关闭或重用
    }
    return ESP_OK;
}

/*
 * @brief 向已连接的WebSocket客户端发送文本消息
 *
 * 该函数将待发送数据封装成 WebSocket 文本帧，并通过 httpd_ws_send_frame_async
 * 发送给当前已建立连接的客户端。
 *
 * @param data 要发送的文本数据缓冲区
 * @param len 数据长度
 * @return 成功返回 ESP_OK，失败返回对应错误码
 */
esp_err_t web_server_send(uint8_t *data, uint16_t len)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));                       // 初始化WebSocket帧结构体
    ws_pkt.payload = data;                                              // 设置发送数据缓冲区
    ws_pkt.len = len;                                                   // 设置数据长度
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;                                   // 设置为文本类型消息
    
    return httpd_ws_send_frame_async(server_handle, client_fds, &ws_pkt);
}