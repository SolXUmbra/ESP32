# 硬件规格（已实测 / 推断 / 待实测 分列）

## 1. 已实测
| 项 | 值 | 证据 |
| --- | --- | --- |
| MCU | ESP32-D0WD-V3 rev 3.1，经典 ESP32（非 S3），无原生 USB CDC | `esptool chip_id` |
| Flash / PSRAM | 4 MB / 无 | `flash_id` / `ESP.getPsramSize()` |
| USB 串口 | CH340，VID_1A86 / PID_7523，当前 COM5，115200 8N1 | PnP 实测 |
| 同机其他串口 | COM3 / COM4（VID_36B7，状态 Unknown），不是本设备 | PnP 实测；Bridge 必须按 VID/PID 发现 |
| 屏幕 | 2.8" ST7789，物理 240×320 | 面板扫描卡 |
| A 方案显示方向 | `setRotation(0)`，逻辑 240×320 竖屏；`LV_HOR_RES_MAX=240`/`LV_VER_RES_MAX=320` | `config.h` / `lv_conf.h` |
| SPI | HSPI：SCLK=14 / MOSI=13 / MISO=12 / DC=2 / CS=15 / RST=-1，写 20 MHz / 读 10 MHz，mode 0 | `config.h` |
| 面板参数（冻结） | `offset_x=0`、`offset_y=0`、`invert=false`、`rgb_order=false` | `HARDWARE_CONFIG.md` 16 张编号卡目视验证 |
| 像素字节序 | `LV_COLOR_16_SWAP=0`，`writePixels(..., swap=true)`；`LCD_PIXEL_SWAP=true` | `lv_conf.h` / `display_manager.cpp`。运行时探测日志：`pixel order: probe=11AA asLE=AA11 -> swap=1` |
| 照片花屏根因 | LovyanGFX 16bit sprite 用 `swap565_t/rgb565_2Byte`，LVGL 期望小端 RGB565；需运行时字节序探测 + 原地逐像素校正 | 11-07 实测修复 |
| 背光 | GPIO21，PWM 5 kHz / 8 bit，默认 220/255 | `config.h` |
| SD | CS=GPIO5，VSPI（与 LCD HSPI 分离），约 7851 MB FAT，根 `/SUNISLE/` | `HARDWARE_CONFIG.md` |
| LVGL 缓冲 | 16 bit，24 KB 内存池，`240×12` 部分缓冲 = 5,760 B，单缓冲 | `lv_conf.h` |
| app 分区 | `partitions_sunisle.csv`，app0 @ `0x10000`，`0x140000` = 1,310,720 B | 分区表 |
| Windows 计划任务 | `Register-ScheduledTask` 可用 | PowerShell 实测 |

## 2. 源码层已实现、硬件未验证（G3 待实测）
- 触摸：`lgfx::Touch_XPT2046` 挂同一 `HSPI_HOST`，`bus_shared=true`，1 MHz；`TOUCH_CS=33`（**best-guess**）、`TOUCH_IRQ=36`、`x_min=300/x_max=3900`、`y_min=400/y_max=3900`、`offset_rotation=0`；`_touch.init()` 成功才 `setTouch()` 并注册 LVGL pointer indev。
- 诊断命令：`TOUCH`（ready/pressed/x/y）、`TOUCHPROBE`（依次试 33/32/34，或 `TOUCHPROBE=34`）。
- 状态：`TODO(待实测)` — CS 引脚、校准值、启用后是否白屏、`1ab7ea3` 新固件烧录结果。**结论写入本文件 §4，并由 CHANGELOG 记录。**

## 3. 推断（不得当事实）
- 横屏需 `setRotation(1)` 或 `(3)` 才得到 320×240；旋转后 `offset_x/y`/`invert`/`rgb_order` 是否需同步调整未经实测，触摸 `offset_rotation` 与校准值必须重测。
- 背光 GPIO21 说明来自旧工程而非本板实测。

## 4. G3 设备实测记录（2026-11-08 06:49，真实设备；日志 `esp32_g3_touch_test.txt`）
- 固件：`B_QINGYU/` 从 `1ab7ea3` 快照**零源码修改**编译，1,137,776 B（app0 86.81%，余 172,944 B），SHA256 `b741b820967304cd6e2921f6b5133e44337354d847ff0a0fc020bf1de5a9f4c6`；只写 `0x10000`，esptool `Hash of data verified`，hard reset 后正常启动。
- 开机日志关键行：`[display] TOUCH ready (XPT2046, shared HSPI, cs=33)`、`[display] LVGL pointer indev registered`、`[display] READY ST7789 240x320 HSPI 20MHz swap=1 buf=5760B`、`[boot] STAGE 1 READY heap=125816 B flush=27`；后续 `flush=1412/1598`、`flush=6806`、`pixels sampled=3221 nonSand=99%`。**但用户目视结果是白屏、按压无反应 —— 日志层"正常"不能证明显示正常，这是本项目的实测教训。**
- `STATE`/`MEM`：heap 109,412 B / min 69,140 B；Wi-Fi `ssid=HOME`；NTP SYNCED；天气 HTTP 200；SD 7851 MB；照片 3 张。
- `TOUCH` → `ready=YES pressed=no x=0 y=0 (cs=33)`（未按压，符合预期）。
- `TOUCHPROBE` → cs=33/32/34 全部 `init=OK read=no`；`TOUCHPROBE=32`、`=34` 同样。**结论：XPT2046 `init()` 不校验 CS 是否真接到控制器，`init=OK` 不能证明引脚正确；未按压时 `read=no` 也无法区分。`TOUCH_CS=33` 仍是 best-guess。**
- `TODO(待实测)`：按压屏幕时抓 `TOUCH` 的 `pressed/x/y`，确认 CS 与校准值；横屏后 `offset_rotation` 与校准值需重测。

### 4.1 G3 判定失败与回滚（2026-11-08 06:53，用户目视）
- 用户反馈：**屏幕白屏、按压无反应**。诊断脚本 `white_screen_diag.py` 抓到的日志仍显示 `state=HOME_READY`、`flush=6806`、`heap=109444 B`、`DIAG` 图案已下发、`UI` 已恢复 —— 固件在跑，但显示不可用。
- 已整片回滚 `0x0 ← esp32_flash_backup_4MB_20261108_pre_g3.bin`（`Wrote 4194304 bytes … Hash of data verified`），回滚后开机日志为 `build Nov 7 2026 02:27:58`（`9779c6e`），`READY ST7789 240x320`、Wi-Fi HOME、照片轮播正常。**请用户目视确认恢复。**
- **`1ab7ea3` 不得作为 B 方案基线**：它在源码/日志层看起来正常，但真机白屏。B 基线改为已目视验证的 `9779c6e`。
- 白屏最可疑的两处改动（`e3f5cb2`）：① XPT2046 挂共享 HSPI + LVGL indev 每 30 ms 轮询 `getTouch()`，CS=33 未确认且最后一次 `TOUCHPROBE=34` 把 CS 改成了 34；② `ui_sunisle.cpp` 新增全屏 `touch_catcher` 覆盖层（`lv_obj_remove_style_all` + `CLICKABLE`）。**二者均未二分验证，不得写成定论。**
- 教训：本项目的目视编号卡验证不可省略；串口日志、flush 计数、照片解码 checksum 都只覆盖软件层，不能替代人眼看屏。

## 5. 禁止改动（冻结）
GPIO 分配、SPI 主机与频率、`offset_x/y`、`invert`、`rgb_order`、`swap`、LovyanGFX 版本 1.2.36、LVGL 版本 8.3.11。任何改动前必须整片备份并重跑目视编号卡验证。

### 4.2 07:14 二分结论（以此为准）
- 实验 A = `1ab7ea3` 全量 + `_touch.init()` 强制关闭（`if (false && _touch.init())`）→ 用户目视「画面显示正常，貌似有字体丢失」。实验 B = `9779c6e` + 7 个触摸层文件，编译成功、未烧录。
- **白屏根因确定：触摸层**（XPT2046 共享 HSPI + LVGL pointer indev 轮询 + 全屏 `touch_catcher`）。`1ab7ea3` 的瘦身/字库不背锅。
- 阶段 C 硬件决策：不 init XPT2046、不注册 LVGL indev、不留 `touch_catcher`。`TOUCH_CS=33` 仍未实测，CS/校准/可用性一律「未确认」，不得写「已支持触摸」。
- 横屏 `setRotation(1)`（逻辑 320×240，物理 `PANEL_W/H=240/320`）只是代码值，**未目视验证**；横屏后必须重跑编号卡验证。
- 已整片回滚到 `9779c6e`（完整 8bpp CJK + LVGL 内置 Montserrat），`Hash of data verified`。

### 4.3 07:36 横屏目视结论
- 用户目视阶段 C 首版固件：**屏幕显示正常**。横屏逻辑 320×240 + `setRotation(1)` 在这块板上方向正确，不再是「代码值未验证」。
- 触摸仍未启用（白屏根因），CS/校准/可用性一律「未确认」，不得写「已支持触摸」。

### 4.4 09:27 触摸硬件实测（结论：无可用数据通路）
- 安全触摸层已重建并烧录：**不注册 LVGL indev、不 `spi::init()`、不 `setTouch()`、开机不碰任何触摸 GPIO**；仅 `TOUCH=on` 后 1 MHz 低频事务自读 XPT2046，按下→抬起经回调分派与 `ACT=`/`NEXT` 相同的 UI 动作；GUEST 忽略所有 tap。实测显示稳定（flush 持续增长、heap 178–185 KB、无白屏）。
- SPI：8 个候选 CS（33/32/25/26/27/16/17/4）+ 1750 帧采样，MISO **恒 `00 00 00`**，`x/y/z1/z2` 全 0；补 `SPI_MISO_DLEN_REG` 后仍全 0。
- I²C：7 组 SDA/SCL 组合全地址扫描 `devices=0`，排除 GT911/FT6236/CST816 电容触摸。
- **结论（已被 §4.5 修订）**：按常规引脚定义读不到触摸数据；软件无法单独启用触摸。**用户 09:31 明确确认「按压过屏幕、板屏一体化、触摸确定可用」**，因此不得再写成「板上没有触摸硬件」或把「没按压」当原因；正确的是「引脚定义未知」。
- 三个坑（**硬规则**）：① 旧层 `pin_int=36` 令 `Touch_XPT2046::getTouchRaw()` 在 IRQ 高时直接返回 0 → 永远检测不到按压，新层不用 IRQ；② GPIO34/35/36/39 只输入，不能当 CS；③ `lgfx::spi::readBytes()` 只设 `SPI_MOSI_DLEN_REG`，ESP32 全双工必须显式设 `SPI_MISO_DLEN_REG` 才能读到 MISO。
- 设备当前 `TOUCH=OFF`（开机默认 OFF）、停在 HOME（OWNER/DEMO on）。报告 `晴屿_触摸硬件诊断报告.md`；回滚 `esp32_app0_backup_20261108_pre_touch.bin` + 快照 `B_QINGYU_pre_touch_snapshot.tar.gz`。
- 命令：`TOUCH | TOUCH=on|off | TOUCHPROBE[=pin] | TOUCHSCAN[=ms] | TOUCHCS=<pin> | TOUCHMAP=0..3 | I2CPROBE`。**不得写「已支持触摸」**。

### 4.5 09:55 触摸第四/五轮（独立路径，结论：引脚定义未知）
- 用户确认按压有效且硬件确定可用 → 排查对象改为**探测方法**。新增四条路径，均不注册 indev、不碰 LCD_CS(15)/DC(2)/SD_CS(5)/BL(21)：
  - `TOUCHSPI[=<cs>,<ms>]`：Arduino `SPIClass(HSPI)` 全双工 `transfer()`，同组 14/12/13 引脚，绕开 `lgfx::spi::readBytes()` 面板 ID 助手。
  - `TOUCHMISO[=<cs>]`：`spiAttachMISO()` 经 GPIO matrix 换 MISO 输入脚，扫 12/22/25/26/27/32/33/34/35/36/39/4/16/17。
  - `TOUCHMATRIX`：14 MISO × 9 CS = 117 组合，每格 4 次读。
  - `TOUCHIRQ[=<ms>]`：只读 GPIO 电平监控 + 同窗口 SPI 采样，一次按压拿两类证据。
- 实测：CS 扫描多为 `00 00 00`、16/17/4 为 `FF FF FF`；cs=33 连续 3 s 在 `00↔FF` 振荡并出现一次 `D1 00 00`（MOSI 回环特征）；117 组合 67 个非平凡**全为 `FF FF FF`**；15 s 按压窗口 `spi33 frames=150 changes=1`；GPIO16 翻转 107 次 / GPIO39 翻转 267 次，互补且约 18 次/秒 → 判为浮空噪声，非 PENIRQ。**始终没有 12 位 ADC 中间值**。
- **硬规则**：MISO 候选禁止包含 SD 的 VSPI 引脚 19/23，`spiAttachMISO()` 会把它们强制 INPUT 并打挂 SD 卡直到重启（首版触发 `sdCommand(): Card Failed`，已剔除并验证 `SD ready=YES`、重启 `MOUNT OK`）。
- **当前判决**：软件层完备可逆、全程无白屏，但常规 MISO/CS 组合都读不到控制器 → 拿不到坐标，不得声称触摸可用。缺的是**板子型号 / 触摸控制器 CS、DOUT(MISO)、PENIRQ 实际 GPIO**。拿到后 `TOUCHCS=<pin>`（必要时改 `LCD_MISO`）+ `TOUCH=on` 直接用现有层。
- 同轮交付 GUEST 2.1 多巴胺版；最终固件 1,228,045 B / 93.7%，干净重启后停在 GUEST。回滚 `esp32_app0_backup_20261108_pre_guest21.bin` + `B_QINGYU_pre_guest21_snapshot.tar.gz`。报告 `UI2_DESIGN/晴屿_GUEST2.1_多巴胺版交付说明.md` 与 `晴屿_触摸硬件诊断报告.md` 补记。
