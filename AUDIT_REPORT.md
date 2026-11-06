# AUDIT_REPORT.md — 晴屿 SUNISLE 阶段 0 审计

> 审计时间：2026-11-06 17:16–17:24
> 审计对象：`D:/ESP32/atlas-moments/`（旧工程，183 个文件）
> 依据：`TEST_REPORT.md`、`HARDWARE_REPORT.md`、`BUILD_REPORT.md`、`README.md`、
> `NAS_API_REPORT.md`、`platformio.ini`、`partitions_atlas.csv`、全部 `src/` 与 `include/` 源码，
> 以及本机实际安装的 PlatformIO 依赖库源码。
> **本文件所有结论均来自实际文件与实测日志，未引用任何网上 CYD 引脚表。**
> 审计阶段**没有修改旧工程任何文件**。

---

## 0. 备份与隔离状态（前置条件已完成）

| 项 | 值 |
| --- | --- |
| 旧工程完整备份 | `D:/ESP32/backup/atlas-moments_pre_sunisle_20261106/` |
| 备份内容 | 182 个文件 / 6.4 MB，含全部源码、报告、v3–v9 快照、`include/secrets.h`、`.local/` 固件 |
| 备份清单 | `BACKUP_MANIFEST_SHA256.txt`（182 行 SHA256） |
| 排除项 | 仅 `.pio/`（可重新生成的构建缓存）与 `build.log`（日志） |
| 新工程 | `D:/ESP32/sunisle/`，独立目录，**与旧工程无文件级关联** |
| 旧工程状态 | **原样保留、可恢复**；板上当前仍运行旧工程 v9 阶段 B 固件 |
| 整片 Flash 备份 | `D:/ESP32/backup/full_flash_20261106.bin`（SHA256 `28cc552e…acb5f`），未改动 |

⚠️ 备份目录含 `include/secrets.h`（Wi-Fi 口令），**仅本地保存，不可对外分享**。

---

## 1. 任务书第 3 节逐项检查

| # | 检查项 | 结论 | 证据 / 位置 |
| --- | --- | --- | --- |
| 1 | 完整项目目录 | ✅ 已通读 | 183 文件；`src/{hal,ui,net,diag}` + `include/{hal,ui,net,diag}` + 7 份报告 + 7 个版本快照 |
| 2 | `TEST_REPORT.md` | ✅ 已通读全文 285 行 | 阶段 0 / A / B 全部记录；含 16 张测试卡对照表、白屏根因、阶段 B 实弹日志 |
| 3 | 构建环境 | ✅ 见 §2 | `platformio.ini`、`partitions_atlas.csv`、PlatformIO Core 6.2.0、esptool 5.6.0 |
| 4 | LVGL v8 初始化 | ✅ 见 §3 | `src/hal/display.cpp:66-72`、`include/lv_conf.h` |
| 5 | ST7789 初始化与参数 | ✅ 见 §4 | `include/hal/gfx_device.h`、`include/hal/pins.h`、`HARDWARE_REPORT.md §10` |
| 6 | SD 卡初始化 / 挂载 / 读写 | ⚠️ **只做过存在性探测，从未实测读写** | `src/hal/hw_probe.cpp:325-340`：`SD.begin(cs)` → `cardSize()` → `SD.end()`。**无 mkdir / open / read / write** |
| 7 | JPEG 解码库及内存 | ⚠️ **旧工程完全没有** | 全库 grep 无 `jpeg/jpg/TJpg`；`lib/` 只有 README；未安装任何解码库。**但 LovyanGFX 自带 TJpgD，见 §6** |
| 8 | 中文字体资源 | ⚠️ **旧工程完全没有** | `lv_conf.h` 只启用 Montserrat 12/14/20/28/48；`TEST_REPORT.md` 明确写「中文子集字库在阶段 F」 |
| 9 | 可复用时钟 UI | ⚠️ **有布局，无产品调性** | `src/ui/clock_ui.cpp`：`ATLAS MOMENTS` / 大号 `--:--` / 卡片 / PREV-NEXT 按钮 / HEAP 调试行，属开发板演示风格，**不可直接用于产品** |
| 10 | 触摸驱动与背光 | ⚠️ 触摸**主动禁用**；背光 GPIO21 | `src/hal/touch.cpp:83-90`：检测到与 LCD 共用 HSPI 即 `HELD OFF`；`src/hal/display.cpp:50-55`：`ledcSetup(0,5000,8)` + `ledcAttachPin(21)` + 亮度 220 |

---

## 2. 构建环境与依赖（实测）

| 项 | 值 |
| --- | --- |
| PlatformIO Core | **6.2.0** |
| platform | `espressif32@6.9.0`（framework-arduinoespressif32 `3.20017.241212`，即 Arduino core 2.0.17 / IDF 4.4） |
| 工具链 | `xtensa-esp32-elf` 8.4.0+2021r2-patch5，esptool 5.6.0（PlatformIO 内置 4.5.1 亦可用） |
| 板 / Flash | `esp32dev`，4MB，`dout`，`f_flash=40MHz`，upload 921600 |
| LVGL | **8.3.11**（锁定版本，不换主版本） |
| 显示库 | **LovyanGFX 1.2.36**（锁定版本） |
| 已装 framework 库 | SD / FS / SPIFFS / LittleFS / WiFi / HTTPClient / WiFiClientSecure / Preferences 等 |
| 分区表 | `nvs 0x9000/0x5000`、`otadata 0xE000/0x2000`、**`app0 0x10000/0x140000 (1310720 B)`**、`app1 0x150000/0x140000`、`spiffs 0x290000/0x170000` |
| 旧工程最后一次构建 | `1248085 B` = **app0 的 95.2%**，RAM 24.5%（80324 B），仅剩约 63 KB |

**已知构建坑（必须继承规避）**：分区 CSV 不能带 UTF-8 BOM；`lv_conf.h` 必须有 `LV_CONF_H` 守卫；
`LV_COLOR_CHROMA_KEY` 需用函数式宏；LovyanGFX 用 `writePixels` 而非废弃的 `pushColors`；
`RTC_DATA_ATTR` 在 `ESP.restart()` 会清零，须用 `RTC_NOINIT_ATTR` + magic；
`HTTPClient.cpp` 无条件链接 `WiFiClientSecure`（mbedTLS ~90KB），明文 HTTP 除外。

---

## 3. LVGL v8 初始化方式（可直接沿用）

```text
lv_init()
lv_disp_draw_buf_init(&buf, buf1, nullptr, 240*12)   // 单部分缓冲，5760 B
lv_disp_drv_init / hor_res=240 / ver_res=320 / flush_cb / draw_buf
lv_disp_drv_register
loop(): lv_timer_handler() + delay(5)
```

- `lv_conf.h`：`LV_COLOR_DEPTH=16`、`LV_COLOR_16_SWAP=0`、`LV_MEM_SIZE=24KB`、
  `LV_TICK_CUSTOM=1` 用 `millis()`、`LV_USE_ANIM=1`、`LV_USE_FS_* = 0`。
- flush：`startWrite → setAddrWindow → writePixels(uint16*, w*h, swap=true) → endWrite → lv_disp_flush_ready`。
  `swap=true` 是把 LVGL 原生小端 RGB565 转成 SPI 字节序，**实测正确，不得更改**。
- **无 indev 注册**（触摸 HELD OFF）。新工程阶段 1 同样不注册，避免再次白屏。
- 开机有一段纯 LovyanGFX 自检卡（1.2s），绕过 LVGL 用于隔离故障。

## 4. ST7789 已验证参数（冻结，不得修改）

见 `HARDWARE_CONFIG.md`。来源：编号测试卡 03 / 07 / 16 目视清晰、其余 13 张花屏；
LCD MISO 未接线故 ID 寄存器读不回，固件日志始终标 `VERIFIED by panel-scan VISUAL test`。

## 5. SD 卡现状（重要缺口）

- 引脚：**CS=GPIO5 / VSPI**（板载 MicroSD 约 7851 MB，`SD.begin(5)` 实测成功）。
- 与 LCD 的 **HSPI 不同总线**，无冲突 —— 这是本板可以照片 + 显示并行的基础。
- 旧工程只做了 `begin → cardSize → end`，**没有验证过挂载稳定性、目录创建、读写、掉卡恢复、
  索引损坏重建、原子替换**。阶段 1/2 必须从头实测。
- SD 卡插在板子上，PC 侧无法直接读写；**阶段 2 要求的 100 张真实 JPEG 需要用户用读卡器
  拷入 `/SUNISLE/photos/`，或通过固件提供的受控写入通道**。这是外部依赖，不能伪造。

## 6. JPEG 解码能力评估（关键结论：不需要新增依赖）

旧工程零解码库，但 **LovyanGFX 1.2.36 内置 TJpgD 流式解码器**：

| 证据 | 值 |
| --- | --- |
| `src/lgfx/utility/lgfx_tjpgd.h` | `JD_SZBUF = 512`（流式输入缓冲 512 B）、`JD_FORMAT = 0`、`JD_USE_SCALE = 1`（支持降采样） |
| `LGFXBase.hpp:972` | `LGFX_FUNCTION_GENERATOR(drawJpg, draw_jpg)` |
| `lgfx_filesystem_support.hpp:90-100` | `drawJpgFile(fs::FS &fs, const char *path, ...)` 直接从 `SD` 流式解码并画到屏幕 |

**结论**：阶段 2 直接用 `gfx.drawJpgFile(SD, path, x, y, w, h, offX, offY, scale)`
即可满足「不把高分辨率图整帧载入 RAM」「行/块缓冲」「按屏幕缩放」，
且符合任务书「先评估内存和兼容性，不盲目增加依赖」。
`JPEGDEC` / `TJpg_Decoder` 作为备选，**暂不引入**（省 Flash、省一套 API 适配）。
⚠️ 尚未实机验证缩放与内存峰值，阶段 2 必须实测并记录到 `MEMORY_BUDGET.md`。

## 7. 中文字体资源评估

- LVGL 内置只有 Montserrat 系列（无中文）；LovyanGFX 自带 `lgfx_efont_cn.c`
  是**完整 CJK 字库**，任务书明令禁止引入，故不用。
- 本机实测：**Pillow 12.3.0 可用**，`C:/Windows/Fonts/msyh.ttc`（微软雅黑）可用。
- 新工程方案：`tools/gen_cn_font.py` 从**真实 TTF** 光栅化 UI 实际用到的汉字，
  生成标准 LVGL `lv_font_fmt_txt` 子集字体（8bpp alpha）：
  - `sunisle_cn_16`：185 个汉字 / 47360 B 位图 / line_height 18
  - `sunisle_cn_28`：仅「晴屿」2 字 / 1568 B / line_height 30
  - ASCII 与数字走 `lv_font_montserrat_16 / 28` 回退，保持拉丁字形质量
  - 合计约 **49 KB Flash**，远小于完整中文字库，符合「仅保留 UI 实际使用字符」

## 8. 与任务书不一致之处（必须显式记录，不擅自「纠正」）

| # | 任务书 | 旧工程实测 | SUNISLE 处置 |
| --- | --- | --- | --- |
| 1 | 「屏幕驱动库：TFT_eSPI」 | **旧工程用 LovyanGFX 1.2.36**；TFT_eSPI 只出现在**原厂遗留固件**的字符串里（曾据此误判为 ESP-AT） | 按「以现有工程实际情况为准」+「不得擅自更换屏幕驱动库」→ **保留 LovyanGFX**，不回退 TFT_eSPI。理由：LovyanGFX 支持运行时选引脚/驱动，才有 16 卡面板扫描的可能；换成 TFT_eSPI 会让已验证参数全部失效 |
| 2 | 「背光 GPIO21 或 27，必须从旧工程确认」 | **最终用 21**（`kVerifiedLcdCandidate=1`），亮度 220 / PWM 5kHz / 8bit / channel 0；27 亦可 | 冻结为 **21**，写入 `HARDWARE_CONFIG.md` |
| 3 | 「如果现有触摸能力稳定，可采用轻量手势」 | 触摸 **NOT CONFIRMED 且主动 HELD OFF**（与 LCD 共用 HSPI，bit-bang 会白屏） | 按「触摸不稳定时优先保证自动运行」→ 阶段 1 **不注册 indev、不做触摸热区**，页面切换与设置通过串口命令；核心显示不受影响 |
| 4 | 「开发方式 Arduino / PlatformIO」 | PlatformIO + Arduino core 2.0.17 | 沿用 |
| 5 | 「当前地点尚未配置，等待用户填写」 | 旧工程 `config.h` 已写渑池坐标 `34.7673/111.7587`（上一轮用户给的），`secrets.h` 有真实 Wi-Fi 口令 | **新工程一律不擅自沿用**：`include/secrets.h` 留空模板，`config` 的城市/经纬度留空并标「未配置」。如需沿用旧值，必须由用户在新任务里重新确认。天气在配置前显示「天气未配置」，不取数、不伪造 |
| 6 | 「模式 A 默认关闭 Wi-Fi」 | 旧工程 WiFi 常连（阶段 B 状态机 10s 重连） | 新工程按任务书实现 `COMPANION` / `SYNC` 两模式，默认关闭 Wi-Fi；旧网络代码只作参考，不整段移植 |
| 7 | 「SD 卡原有配置 CS5 / VSPI」 | 一致 | 沿用 |
| 8 | 「不能假设 NAS 服务端已实现」 | `NAS_API_REPORT.md` 已审计：fnOS 相册需登录 Cookie，官方 skill 明确排除 Media API | 阶段 4 只做 Adapter，**不宣称联调成功**；缺失接口清单见 `NAS_API_ADAPTER.md` |

## 9. 可复用 / 需重写判定

| 模块 | 处置 | 原因 |
| --- | --- | --- |
| `gfx_device.h`（运行时选 Bus/Panel） | **裁剪复用** | 参数来源唯一且已验证；新工程去掉 4 种驱动的探测分支，只留 ST7789 |
| `display.cpp` flush / `writePixels(swap=true)` | **复用** | 字节序与频率是踩坑换来的，绝不重写 |
| `display.cpp` `selfTest()` 彩色棋盘卡 | **不复用** | 开发板诊断风格，与品牌启动页冲突；改为独立 RAW 诊断命令，正常开机不显示 |
| `hw_probe.cpp` 的 6 组候选 / bit-bang ID 扫描 | **不复用** | 已完成历史使命；新工程直接用冻结参数，避免再次扰动面板 |
| `hw_probe.cpp` `SD.begin(5)` | **复用并扩展** | 需补挂载降级 / 目录创建 / 读写 / 原子替换 |
| `clock_ui.cpp` | **不用** | 演示风格（HEAP 调试行、PREV/NEXT 灰按钮、卡片），与「不像开发板演示项目」冲突；仅借鉴 240×320 排版经验 |
| `net/network.cpp` core0 任务 + mutex 快照 | **架构复用** | 异步不卡 UI 已实机验证；阶段 3 重写为「按需同步、完成关 WiFi」 |
| `net/weather.cpp` 原生 HTTP/1.0 客户端 | **阶段 3 复用** | 免密钥 Open-Meteo、避开 mbedTLS、NVS 缓存已实测；但缓存要按任务书改存 `/SUNISLE/weather/cache.json` |
| `touch.cpp` | **不复用（保持禁用）** | 见 §8-3 |
| `versions/` 全部快照 | **保留在备份中** | 可回滚参考 |

## 10. 阶段 0 结论与阻塞项

**审计完成，允许进入实施。** 但在开始阶段 1 前必须确认下列外部依赖（不阻塞编码，阻塞验收）：

| 阻塞项 | 影响阶段 | 需要什么 |
| --- | --- | --- |
| 100 张不同尺寸真实 JPEG | 2 | 用户用读卡器写入 SD `/SUNISLE/photos/`，或确认由固件受控写入通道生成 |
| SUNISLE Sync Server 未实现 | 4 | 服务端地址、设备 Token、六个约定接口的真实实现；只能先交 Adapter + 缺失清单 |
| 城市 / 经纬度 / 时区 / Wi-Fi 凭据 | 3 | 用户在新工程里重新确认（旧值不擅自沿用） |
| 30 分钟 / 2 小时连续运行 | 1 / 2 / 5 | 真实时间，不能加速、不能伪造 |
| 触摸 | 1–5 | 需要真人按压取 raw 值 + 共用 `Bus_SPI` 重写；在此之前核心显示不受影响 |
| app0 容量 | 2–4 | 旧工程已占 95.2%；叠加照片/天气/Wi-Fi 后可能溢出，需按 `MEMORY_BUDGET.md` 逐阶段实测，必要时请求用户确认是否允许改分区表（会破坏 app1/spiffs） |
