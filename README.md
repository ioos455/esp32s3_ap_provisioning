# ESP32-S3 物联网 AP 配网系统

基于 **ESP32-S3 + ESP-IDF** 实现的无线配网方案：设备启动后开放热点，手机连接后在浏览器中完成配网，**终端用户无需安装任何 APP**。

配套硬件为正点原子 DNESP32S3 类开发板（SPI LCD、XL9555 扩展 IO、ES8311 音频等板载外设）。

---

## 功能特性

| 功能 | 状态 |
| --- | --- |
| Wi-Fi STA 连接（可配置 SSID / 密码） | ✅ 已完成 |
| 按键触发进入配网模式 | ✅ 已完成 |
| SoftAP 热点，手机浏览器访问配置页 | ✅ 已完成 |
| HTML 配置页存于 SPIFFS 分区下发 | ✅ 已完成 |
| 自建 WebSocket 服务器，JSON 双向通信 | ✅ 已完成 |
| 提交 SSID / 密码后切换 STA 连接 | ✅ 已完成 |
| LCD 显示连接状态与 IP | ✅ 已完成 |
| 扫描周边 Wi-Fi 并推送列表到手机 | ⚠️ **有缺陷，见「已知问题」#2** |

---

## 硬件与引脚

| 外设 | 接口 | 引脚 |
| --- | --- | --- |
| SPI LCD | SPI | 见 `components/BSP/MYSPI`、`SPILCD` |
| XL9555 扩展 IO（按键 / LCD 控制） | I2C0 | SDA=GPIO38, SCL=GPIO48 |
| LED0 / LED1 | GPIO | GPIO43 / GPIO44 |
| 按键 KEY1（触发配网） | XL9555 P1.4 | 经 I2C 读取 |

配网热点：`SSID = ESP32S3_AP`，设备 IP = `192.168.100.1`

---

## 目录结构

```
.
├── main/
│   └── main.c                      # 入口：初始化各模块 + 轮询按键触发配网
├── components/
│   └── BSP/
│       ├── CMakeLists.txt          # 统一注册各 BSP 子目录
│       ├── HTML/
│       │   └── apcfg.html          # 配网页面（含前端 WebSocket 逻辑）
│       ├── SERVER/
│       │   ├── ws_server.c/.h      # HTTP + WebSocket 服务器
│       ├── WIFISTA/
│       │   ├── wifista.c/.h        # Wi-Fi STA 模式、扫描任务
│       │   ├── ap_wifi.c/.h        # 配网流程、SPIFFS、JSON 收发
│       │   ├── wifi_secrets.h.example  # 凭据模板（此文件入库）
│       │   └── wifi_secrets.h      # 真实凭据（已被 .gitignore 忽略）
│       ├── XL9555/                 # 扩展 IO 驱动
│       ├── MYIIC/                  # I2C 驱动（XL9555 用）
│       ├── MYSPI/ SPILCD/          # SPI 与 LCD 驱动
│       └── LED/
├── partitions_webserver.csv        # 含 SPIFFS 分区的分区表
└── sdkconfig
```

---

## 配网流程

```
上电
  │
  ├─ 初始化 LED / SPI / I2C / XL9555 / LCD / Wi-Fi(STA)
  │
  └─ 主循环轮询 KEY1
        │
        └─ 按下 KEY1
              │
              ├─ ① 切换 Wi-Fi 为 AP+STA 模式
              │     配置 AP 静态 IP 192.168.100.1
              │     （改 IP 前必须先停 DHCP 服务，否则不生效）
              │
              ├─ ② 启动 HTTP + WebSocket 服务器
              │     浏览器访问 http://192.168.100.1/  → 返回 SPIFFS 中的配置页
              │     页面 JS 建立 ws://192.168.100.1/ws
              │
              ├─ ③ 手机端操作
              │     点「重新扫描」 → 发 {"scan":"start"}   → 设备扫描并推送 WiFi 列表
              │     填 SSID/密码  → 发 {"ssid":"...","password":"..."}
              │
              └─ ④ 收到凭据
                    停掉 Web 服务 → 切回 STA 模式 → 连接目标 WiFi
                    LCD 显示连接状态与获取到的 IP
```

---

## WebSocket 通信协议

前端与设备之间的消息格式（原 `main/参考.json` 中的协议说明，已整理至此）。

**连接地址**：`ws://192.168.100.1/ws`（由 `apcfg.html` 动态拼接 `window.location.hostname`）

### 下行：手机 → 设备

**① 请求扫描周边 Wi-Fi**

```json
{ "scan": "start" }
```

**② 提交要连接的 Wi-Fi 凭据**

```json
{
  "ssid": "你的WiFi名称",
  "password": "你的WiFi密码"
}
```

### 上行：设备 → 手机

**扫描结果**（`wifi_sacn_cb()` 组装后推送）

```json
{
  "wifi_list": [
    { "ssid": "test1", "rssi": -23, "encrypted": true },
    { "ssid": "test2", "rssi": -70, "encrypted": true }
  ]
}
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `ssid` | string | 网络名称 |
| `rssi` | int | 信号强度（dBm），越接近 0 越强 |
| `encrypted` | bool | `false` 表示开放网络（`WIFI_AUTH_OPEN`） |

> 即使扫描到 0 个网络，设备也会回传 `{"wifi_list":[]}`，前端据此结束等待。
> 但若 WebSocket 已断开（见「已知问题 #1」），该消息无法送达，前端会等到 20 秒超时。

---

## 技术要点

- **AP 段改 IP 的顺序**：必须 `esp_netif_dhcps_stop()` → `esp_netif_set_ip_info()` → `esp_netif_dhcps_start()`。直接改 IP 不生效。
- **WebSocket 用回调解耦业务**：`ws_server.c` 只负责连接管理与收发，业务通过 `receive_cb` 回调给 `ap_wifi.c`，服务器本身可复用。
- **扫描任务同步**：用二值信号量防止重复启动扫描，成功/失败/内存不足三条路径都会归还信号量。
- **WebSocket 帧的两步读取**：先以 `max_len=0` 读帧头拿到长度，分配缓冲后再读载荷。
- **SPIFFS 页面下发**：HTML 存于独立分区，运行时读入内存作为 Web 服务器首页，便于修改页面而无需重新烧写固件逻辑。

---

## 编译与烧写

### 1. 准备敏感配置

真实 WiFi 凭据不随仓库分发。首次编译时 CMake 会自动从模板生成：

```bash
components/BSP/WIFISTA/wifi_secrets.h.example  →  wifi_secrets.h
```

生成后请按自己的网络修改 `wifi_secrets.h`：

```c
#define WIFI_SECRET_STA_SSID        "你的WiFi名称"
#define WIFI_SECRET_STA_PASSWORD    "你的WiFi密码"
#define WIFI_SECRET_AP_SSID         "ESP32S3_AP"
#define WIFI_SECRET_AP_PASSWORD     "12345678"
```

> `wifi_secrets.h` 已被 `.gitignore` 忽略，不会被提交。

### 2. 编译烧写

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

---

## 已知问题与待改进

> 以下问题按严重程度排序，均已定位到具体代码位置。欢迎 PR。

### 🔴 #1 WebSocket 连接建立后会自动断开

**现象**：页面加载后 WebSocket 握手成功，但数秒后自动断开，前端反复显示「WebSocket连接断开，正在重连...」。

**位置**：`components/BSP/SERVER/ws_server.c` 的 `ws_get_req()`

**分析**：`httpd_config_t` 默认 `recv_wait_timeout = 5`（秒）。空闲时 `httpd_ws_recv_frame()` 会因接收超时返回错误，而当前实现把该错误直接 `return` 出去：

```c
ret = httpd_ws_recv_frame(r, &ws_pkt, 0);
if (ret != ESP_OK)
{
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame info");
    return ret;                      // ← 返回非 ESP_OK，IDF 会关闭该连接
}
```

IDF 的分发逻辑（`httpd_parse.c`）中：

```c
ret = sd->ws_handler(r);
if (ret != ESP_OK) {
    httpd_req_cleanup(r);            // 连接被清理
}
```

**待办**：区分「接收超时」与其他错误，超时不应导致连接关闭；或延长 `recv_wait_timeout`。

---

### 🔴 #2 Wi-Fi 扫描功能不可用

**现象**：点击「重新扫描Wi-Fi网络」后前端在 20 秒后提示「扫描Wi-Fi网络失败，请稍后重试」，始终不显示网络列表。

**位置**：`components/BSP/WIFISTA/ap_wifi.c` 的 `ws_receive_cb()` / `wifi_sacn_cb()`

**分析**：扫描结果通过 `web_server_send()` → `httpd_ws_send_frame_async()` 推送给前端。若此时 WebSocket 已因 **#1** 断开，推送失败且前端收不到任何数据（`{"wifi_list":[]}` 也收不到），于是只能等前端 20 秒超时。

**待办**：
1. 先修复 #1；
2. `wifi_sacn_cb()` 中增加对发送失败的处理，并考虑扫描失败时也回传明确错误信息；
3. 在 `web_server_send()` 中增加连接有效性校验（见 #4）。

---

### 🟠 #3 WiFi 凭据曾硬编码在源码中（已修复）

**原问题**：`wifista.h` 中直接写入了真实 SSID 与密码，若直接推送到公开仓库等于公开凭据。

**已处理**：拆分为 `wifi_secrets.h`（真实凭据，已 `.gitignore`）+ `wifi_secrets.h.example`（模板，入库），并由 CMake 在首次编译时自动生成。

**待办**：如果这些凭据曾出现在任何已推送的提交中，建议更换密码。

---

### 🟠 #4 `client_fds` 只支持单个客户端

**位置**：`components/BSP/SERVER/ws_server.c`

```c
static int client_fds = 0;                              // 单变量
...
client_fds = httpd_req_to_sockfd(r);                    // 每次握手都覆盖
```

**问题**：
- 只记录最后一次握手的 socket fd。若同时存在多个连接（如刷新页面后旧连接未清理、或开多个标签页），数据会发到错误的 socket；
- 未校验有效性：若握手尚未发生就调用 `web_server_send()`，会向 fd `0` 发送。

**待办**：改为数组 + 计数管理所有客户端；`web_server_send()` 增加 `server_handle != NULL && client_fds > 0` 校验；`web_server_stop()` 中清空 fd。

---

### 🟡 #5 `xl9555_key_scan()` 内部存在 100ms 阻塞延时

**位置**：`components/BSP/XL9555/xl9555.c:228`

```c
esp_rom_delay_us(100000);      /* 去抖动 */
```

**问题**：该函数为「扫描一次」设计，内部阻塞 100ms 且带静态状态机，不适合放入周期轮询的任务（会把任务周期拖长到 100ms 以上）。

**待办**：需要按键扫描时改用「一次读取 16 位 IO 状态 + 自行做边沿检测」的方式，或将该函数改造为非阻塞版本。

---

### 🟡 #6 `xl9555_write_byte()` 的 malloc 结果直接使用

**位置**：`components/BSP/XL9555/xl9555.c:31`

```c
uint8_t *buf = malloc(1 + len);
if (buf == NULL)
{
    ESP_LOGE(xl9555_tag, "%s memory failed", __func__);
    // ← 仅打印日志，未 return，随后 memcpy 会解引用 NULL
}
memcpy(buf + 1, data, len);
```

**待办**：`malloc` 失败时补充 `return ESP_ERR_NO_MEM;`

---

### 🟡 #7 Wi-Fi 断线重连无次数保护

**位置**：`components/BSP/WIFISTA/wifista.c` 的 `wifi_event_handler()`

```c
case WIFI_EVENT_STA_DISCONNECTED:
    ...
    esp_wifi_connect();        // 立即重连，无计数
    break;
```

**问题**：若 SSID 或密码错误，会陷入「连接失败 → 立即重连」的无限循环，日志刷屏并持续占用 CPU，用户表现为设备卡死。

**备注**：文件顶部已声明 `static int sta_connect_count = 0;`，但仅在 `wifi_manager_connect()` 中清零，未用于计数。

**待办**：增加重连次数上限，超限后停止重连（可回到配网模式或提示错误）。

---

### 🟡 #8 `ap_wifi.c` 中 cJSON 解析未判空

**位置**：`components/BSP/WIFISTA/ap_wifi.c:139`

```c
cJSON *root = cJSON_ParseWithLength((char *)data, len);
if(root)
{
    ...
}
```

**说明**：此处已有 `if(root)` 保护，但内部 `cJSON_GetStringValue()` 的返回值未校验。若收到的 JSON 字段类型不符，会返回 NULL 并传入 `strcmp()` 造成异常。

**待办**：对 `cJSON_GetStringValue()` 的返回值做判空。

---

### 🟡 #9 I2C 总线缺少互斥保护

**位置**：`components/BSP/MYIIC/myiic.c`

**问题**：`xl9555.c` 通过该总线读 IO，若后续有多个任务同时访问 I2C（如新增传感器读取），会出现总线数据错乱（I2C 非线程安全）。当前单一访问路径下暂未暴露。

**待办**：为 I2C 访问增加互斥锁（`xSemaphoreCreateMutex`），或统一由单个任务发起所有 I2C 事务。

---

### 🟡 #10 按键检测采用主循环轮询

**位置**：`main/main.c`

```c
while (1)
{
    if(KEY1 == 0 && key1_released) { ... }
    vTaskDelay(pdMS_TO_TICKS(50));
}
```

**问题**：轮询方式占用主任务，且 `KEY1` 宏每次调用都会发起一次完整 I2C 读事务（`xl9555_pin_read()` 内部读 2 字节），一次判断最多产生数次 I2C 通信。

**待办**：可改为独立按键任务 + 一次读取 16 位 IO 后本地按位判断，提升响应速度并减少总线占用。

---

### ⚪ #11 其他代码质量项

| 位置 | 问题 |
| --- | --- |
| `ap_wifi.c` | `if (buffer == NULL)` 后缺少大括号，可读性差 |
| `wifista.c` | `esp_wifi_scan_get_ap_num()` 返回值未检查 |
| `wifista.c` | IP 事件回调中对 `event_data` 直接按 `esp_netif_ip_info_t*` 强转，未校验事件类型 |
| `ws_server.c` | `web_server_send()` 的 `len` 参数为 `uint16_t`，而 `strlen()` 返回 `size_t`，超长数据会被截断 |
| `main.c` | `nvs_flash_init()` 失败时未处理（如 NVS 分区满或版本不匹配应擦除重试） |
| 全局 | 部分文件缺少统一日志 TAG，排查问题时难以区分来源 |

---

## 后续改进方向

1. 修复 #1 / #2，使扫描功能可用；
2. 支持多 WebSocket 客户端（#4）；
3. 为 I2C 增加并发保护（#9）；
4. 增加配网超时退出机制（如 3 分钟无人连接则关闭 AP 以省电）；
5. 增加配网失败回退（连不上目标网络时重新开放 AP，而非无限重连）；
6. 将按键处理改为独立任务（#10）。

---

## 许可

个人学习项目，供参考。板载外设驱动（SPILCD / MYIIC / MYSPI / XL9555）基于厂商 BSP 整理。
