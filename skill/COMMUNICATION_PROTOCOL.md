# 通信协议

## A 方案现状（`1ab7ea3`，仍为旧协议）
- 纯文本行命令：`HELP / STATE / MEM / SD / TIME / WX / PAGE / BL / INTERVAL / RESCAN / NEXT / PHOTO / DEL / PUT / DIAG / UI / REBOOT / TOUCH / TOUCHPROBE`。
- 命令缓冲 `String s_cmd`，`s_cmd.length() < 63`，超出直接丢弃字符 —— 远端两个提交**都没改这一行**。
- `while (Serial.available())` 在 `loop()` 内同步 `handleCommand()`；`PUT` 的 `writeFromStream()` 会阻塞 LVGL `lv_timer_handler()`。
- 无 JSON、无 ACK、无消息 ID、无版本号、无去重。
- SD 目录 `/SUNISLE/{config,photos,photos/thumbnails,weather,system}`；`PUT` 永不覆盖，`DEL` 限于 photos 直属 `.jpg`。
- Wi-Fi：`WiFiMulti` 多 SSID；NTP + Open-Meteo 明文 HTTP/1.0；HTTPS 因 mbedTLS 体积不可行。

## B 方案目标（阶段 F）
```
WorkBuddy → MCP tool → Bridge SQLite（SAVED）→ USB JSONL 下发（SENT）→ 设备 ACK（ACKED）→ Bridge 更新
```
- UTF-8 **JSON Lines**，字段：`version` / `id` / `type` / `event` / `timestamp` / `data`。
- ACK、超时重试、断线重连、队列补发、设备重启后全量同步；同 `id` 重复不产生重复记录（`id` 唯一键）。
- 修改/删除走明确 event；重要提醒本地保留，断连仍可触发。
- 首期单条 ≤ 2 KB；Bridge 侧先截断摘要为 ≤ 2 行。
- 固件侧必须把 63 字节 `s_cmd` 换成独立接收缓冲（≥ 2 KB 环形）→ 按行切分 → 非阻塞解析 → 事件分发；**LVGL 回调内禁止串口阻塞**。
- Bridge 端口按 VID_1A86/PID_7523 发现 CH340，**不写死 COM5**。

## 数据模型（阶段 D）
`messages(id, version, event, payload, state, created_at, sent_at, acked_at)`，三态 `SAVED / SENT / ACKED`。

## 三态铁律
**已保存 / 已发送 / 设备已确认接收**必须严格区分；未收到 ACK 不得声称内容已显示。任何工具返回都要正确体现当前状态。

## 待确认
`TODO(待实测)`：协议版本号、ACK 超时与重试次数、设备重启全量同步触发条件、SD 卡在 B 方案中的缓存用途与目录名、Windows 锁屏事件检测方案。
