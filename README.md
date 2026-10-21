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
| 扫描周边 Wi-Fi 并推送列表到手机 | ✅ 已完成（失败/掉线均可恢复） |

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
> 扫描失败时设备回传 `{"wifi_list":[],"error":"ESP_ERR_xxx"}`，前端直接提示失败原因；
> 若推送时客户端恰好掉线，结果会缓存在设备上，客户端重连（或刷新页面）后立即补发。

> ⚠️ **手机侧注意**：ESP32 热点没有外网，Android/iOS 检测到「无互联网」后可能自动切回移动数据，
> 此时浏览器保留着已加载的页面，但 WebSocket 已经无法连到 `192.168.100.1`，表现为点了扫描没反应。
> 解决：在 Wi-Fi 设置里选择「仍然保持连接 / 使用此网络」，并关闭「移动数据自动切换」（Android 的
> “自动切换到移动数据” / iOS 的“无线局域网助理”）。

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

### ✅ #1 WebSocket 一直连不上（已修复）

**现象**：页面能正常打开，但 WebSocket 始终连不上（前端不停重连）；点击扫描没有任何反馈。

**根因**：`HTTPD_DEFAULT_CONFIG()` 的 `max_req_hdr_len` 取自 `CONFIG_HTTPD_MAX_REQ_HDR_LEN`，本工程为 **512 字节**。
普通 `GET /` 的请求头勉强能塞下，但浏览器发起 WebSocket 升级时还会附加
`Upgrade` / `Origin` / `Sec-WebSocket-Key` / `Sec-WebSocket-Version` / `Sec-WebSocket-Extensions` 等字段，
总长度必然超过 512，httpd 直接判定请求超长并丢弃 —— 握手请求根本不会派发到 `/ws` 的处理函数
（开启 `DEBUG` 模块后访问 `/debug` 可以看到 `ws_handshakes` 始终为 0，而 HTTP 页面却访问正常）。

**修复**：`web_server_start()` 中显式配置 `max_req_hdr_len = 1024`，
并打开 `lru_purge_enable`（会话被浏览器 keep-alive 占满时回收最久未用的连接），
同时把 httpd 栈从默认 4096 提到 6144。

**原问题（已一并修掉）**：`ws_get_req()` 把 `httpd_ws_recv_frame()` 的所有错误都 `return` 给 httpd，
而 IDF 的分发逻辑是「handler 返回非 `ESP_OK` 就关闭 socket」，读到半包/超时时整条连接会被关掉。
现在失败只记日志并返回 `ESP_OK`，`web_server_send()` 发送前也会用 `httpd_ws_get_fd_info()` 校验连接。

---

### ✅ #2 Wi-Fi 扫描结果送不到前端（已修复）

**原问题**：扫描结果只推给全局保存的 `client_fds`。AP+STA 模式下扫描会让 SoftAP 短暂离开本信道，手机可能掉线或刷新页面，旧 fd 失效 → 结果丢失 → 前端等到 20 秒超时，表现为「扫描不到任何 Wi-Fi」。

**修复**：
1. `ap_wifi.c` 用互斥锁缓存最近一次扫描 JSON，WebSocket 新建连接（含重连/刷新）时通过 `connect_cb` 立即补发；
2. 扫描失败也回包 `{"wifi_list":[],"error":"ESP_ERR_xxx"}`，前端立即提示原因而不是空等；
3. `wifi_scan_task()` 使用显式扫描配置：`show_hidden = true`、`home_chan_dwell_time = 120ms`（扫描过程中周期性回到 AP 信道，避免手机掉线）；
4. 扫描返回 `ESP_ERR_WIFI_STATE`（STA 正在重连）时，先暂停重连再重试一次。

---

### 🟠 #3 WiFi 凭据曾硬编码在源码中（已修复）

**原问题**：`wifista.h` 中直接写入了真实 SSID 与密码，若直接推送到公开仓库等于公开凭据。

**已处理**：拆分为 `wifi_secrets.h`（真实凭据，已 `.gitignore`）+ `wifi_secrets.h.example`（模板，入库），并由 CMake 在首次编译时自动生成。

**待办**：如果这些凭据曾出现在任何已推送的提交中，建议更换密码。

---

### 🟠 #4 `client_fds` 只支持单个客户端

**位置**：`components/BSP/SERVER/ws_server.c`

```c
static int client_fds = -1;                             // 单变量
...
client_fds = httpd_req_to_sockfd(r);                    // 每次握手都覆盖
```

**问题**：
- 只记录最后一次握手的 socket fd。若同时存在多个连接（如开多个标签页），数据只会发到最后握手的那一个；
- ~~未校验有效性~~：已修复，`web_server_send()` 现在先用 `httpd_ws_get_fd_info()` 校验 fd 是否仍是有效 WebSocket，无效时返回 `ESP_ERR_INVALID_STATE`（上层会缓存结果并在下次连接时补发）。

**待办**：改为数组 + 计数管理所有客户端，实现多标签页同时刷新列表。

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

## 调试技巧（没有串口日志时）

本项目 `LED0/LED1` 使用了 **GPIO43/44**，而这正是 UART0 控制台的 TX/RX 引脚，`led_init()` 执行后
控制台会被 GPIO 复用覆盖，**运行时日志基本看不到**。为此固件预留了一个可选的诊断模块：

```
components/BSP/DEBUG/
├── bsp_debug.c
└── bsp_debug.h      # 开关 BSP_DEBUG_ENABLE，默认 0（禁用）
```

**默认是关闭的**：此时 `bsp_debug.c` 整个文件不参与编译，`ws_server.c` 里的
`bsp_debug_ws_handshake()` / `bsp_debug_ws_frame()` / `bsp_debug_ws_send()` 都是头文件里的空内联函数，
不注册 URI、不计数、不占 RAM（实测关闭时固件小约 1.4KB）。

需要排查时，把 `bsp_debug.h` 里的默认值改成 1 重新编译：

```c
#ifndef BSP_DEBUG_ENABLE
#define BSP_DEBUG_ENABLE 1      /* 1=启用 /debug 诊断页，0=禁用（默认） */
#endif
```

然后访问：

```
http://192.168.100.1/debug
```

返回内容示例：

```json
{"reset_reason":1,"uptime_ms":73769,"free_heap":224852,"wifi_mode":3,
 "sta_connected":false,"ws_fd":5,"ws_connected":true,
 "ws_handshakes":1,"ws_frames":3,"ws_send_ok":3,"ws_send_fail":0}
```

| 字段 | 含义 |
| --- | --- |
| `reset_reason` | 复位原因（1=POWERON，4/5=看门狗，6=PANIC） |
| `uptime_ms` | 开机毫秒数，变小说明设备偷偷重启了 |
| `wifi_mode` | 1=STA，2=AP，3=AP+STA |
| `ws_handshakes` | WebSocket 握手成功次数；为 0 说明浏览器请求根本没到达设备 |
| `ws_frames` / `ws_send_ok` / `ws_send_fail` | 收到的帧数、发送成功/失败次数 |

> 该诊断页曾用于定位「页面能打开但 WebSocket 一直连不上」的问题：`ws_handshakes` 恒为 0，
> 最终查出是 httpd 默认 `max_req_hdr_len=512` 装不下浏览器的 WS 升级请求头。

---

## 后续改进方向

1. 支持多 WebSocket 客户端（#4）；
2. 为 I2C 增加并发保护（#9）；
3. 增加配网超时退出机制（如 3 分钟无人连接则关闭 AP 以省电）；
4. 增加配网失败回退（连不上目标网络时重新开放 AP，而非无限重连）；
5. 将按键处理改为独立任务（#10）。

---

## 许可

个人学习项目，供参考。板载外设驱动（SPILCD / MYIIC / MYSPI / XL9555）基于厂商 BSP 整理。
