# 晴屿 SUNISLE · B 方案（QINGYU）

> 把喜欢的时光，留在身边。 / *A Little Island of Sunshine.*

针对同一块 ESP32-D0WD-V3 + ST7789 320×240 硬件的 **B 方案**：保留 A 方案的显示/SD/天气/校时底座，把显示层整体换成 **LVGL 8.3 + 四画面 AI 桌面助手 UI（UI 2.3 简约中式）**。

> **分支说明**：本分支 `b-version` 为 B 方案，工程在分支根目录；A 方案（纯 LovyanGFX 照片相框）完整保留在 `main` 分支与 `vA` 标签。
> **实现沿革**：分支上一版是「备忘提醒 + WorkBuddy `CMD|`/`TASK|`/`SKILL|` 指令协议」（commit `da0e758`，未上机验证）；本版按「四画面 AI 桌面助手」重做显示层，那套指令协议与 `memo_service` 未沿用，需要时可从 `da0e758` 取回。

---

## 当前状态（2026-11-08 10:40 烧录实测）

| 项 | 状态 |
| --- | --- |
| 硬件基线 | ESP32-D0WD-V3 / 4 MB flash / 无 PSRAM / CH340 `COM5` |
| 显示 | ST7789 240×320 物理，横屏逻辑 320×240，`setRotation(1)`，HSPI 20 MHz，swap=1 |
| UI | **UI 2.3 简约中式**：BOOT / HOME / MEMOS / GUEST 四画面，8 px 栅格、16 px 页边、1 px 描边卡 |
| 中文字体 | 自研 LVGL 8bpp 子集面：14 px 动态（303 字白名单）/ 16 px 静态 / 28 px 楷品牌 / 34、56 px Light 数字 |
| 固件 | **1,228,481 B / 93.7%（余 82,239 B）**，RAM 97,556 B / 29.8% |
| 烧录 | 仅写 `0x10000`（app0），`Hash of data verified`，干净重启停在 GUEST |
| 隐私 | 上电强制 GUEST；私有文本不缓存、不清空即不残留；GUEST 忽略点击，无法进入 Owner |
| 触摸 | ⛔ 未打通：四种探测均取不到坐标，缺板子原理图/引脚；交互走串口等价命令 |
| 网络 | Wi-Fi → NTP → Open-Meteo 真实天气（渑池），取数后关射频 |

**启动实测（`esp32_boot_log_cn23.txt`）**

```
[display] READY ST7789 320x240 HSPI 20MHz swap=1 buf=7680B
[touch] HELD OFF at boot - no GPIO touched (panel stays safe)
[ui] UI 2.0 BOOT (1500 ms); lock state mirrors the Windows session
[net] Wi-Fi up: ssid=HOME ip=192.168.0.126 rssi=-50 dBm
[time] SYNCED epoch=1794105343
[weather] LIVE temp=11 C wmo=2 -> 多云
[boot] UI 2.0 READY heap=199640 B flush=20
```

零缺字警告（`glyph dsc. not found`）、零 LVGL 报错。

---

## 目录

| 路径 | 内容 |
| --- | --- |
| `src/ui/qingyu_ui.cpp` | 四画面全部布局，坐标与设计图一一对应 |
| `src/ui/sunisle_cn.cpp` | 生成的 CJK 子集字模（约 690 KB，随仓库提交以便直接编译） |
| `src/ui/ui_bridge.cpp` | 状态 → 文案绑定，动态字超出白名单降级为 `?` |
| `src/ui/ui_theme.cpp` | 宣纸 / 墨 / 朱砂 / 青瓷 token |
| `src/hardware/touch_manager.cpp` | 安全触摸层：不注册 LVGL indev、开机不碰触摸 GPIO |
| `tools/gen_cn_font.py` | 字模生成（统一 TTF 基线 + 真实 ink bbox，勿改成逐字居中） |
| `docs/ui2/` | UI 2.x 规范、实施与烧录报告、四张 320×240 设计图 + `index.html` |
| `skill/` | 本项目开发知识库（硬件参数、UI 规范、串口协议、禁止事项） |

## 串口命令（115200，等价于触控）

```
PAGE=home|memos|guest     切换页面
PRIVACY=owner|guest       切换隐私状态
ACT=ack|later|detail|task 确认 / 稍后 / 查看详情 / 完成任务
NEXT                      跳下一条
TOUCH=on|off / TOUCHSCAN  触摸探测（安全层，不注册 indev）
REBOOT                    重启
```

## 编译与烧录

```bash
python -m platformio run -e esp32dev
python -m esptool --chip esp32 --port COM5 --baud 921600 write_flash \
  --flash_mode dio --flash_freq 40m --flash_size 4MB 0x10000 .pio/build_g/esp32dev/firmware.bin
```

`include/secrets.h` 保持 gitignored，从 `include/secrets.h.example` 复制后填 Wi-Fi。

---

## A 方案阶段状态（沿用底座，2026-11-06 实测）

| 阶段 | 状态 |
| --- | --- |
| 0 审计与备份 | ✅ 完成：`AUDIT_REPORT.md` / `HARDWARE_CONFIG.md` / `MEMORY_BUDGET.md` / `REFACTOR_PLAN.md` + 旧工程完整备份 |
| 1 基础系统 | ✅ 已实机跑通（UI / 中文 / 照片 / 天气 / 校时）；**30 分钟连续运行验收需真实时间，未做完不写 PASS** |
| 2 照片系统 | 🟡 单张真实 JPEG 已跑通端到端（上传 → SD → TJpgD 流式解码 → LVGL 显示）；批量与 2 小时压测待做 |
| 3 天气与时间 | ✅ 已实机跑通：Wi-Fi → NTP → Open-Meteo 真实数据 → 中文 WMO 文案 → 关射频 |
| 4 NAS 同步 | ⏸ 阻塞：SUNISLE Sync Server 未实现，见 `NAS_API_ADAPTER.md` |
| 5 整体优化 | ⏸ 依赖 2–4 |

**实机证据**（`verify_photo.log`）：

```
[sd] photo scan: 1 real JPEG file(s)
[weather] cache restored: 11 C wmo=0 (晴, offline)
[net] Wi-Fi up: ssid=ETC ip=192.168.31.225 rssi=-19 dBm
[time] SYNCED epoch=1793963116
[weather] HTTP 200, 692 B body, current.temperature_2m=11.2 weather_code=0
[weather] LIVE temp=11 C wmo=0 -> 晴
[uibridge] viewport sprite 216x164 = 70848 B (no PSRAM, lazy)
[uibridge] pixels sampled=3221 nonSand=100% checksum=0674A53B
[uibridge] photo 432x328 -> zoom=500/1000 (/SUNISLE/photos/01_together.jpg)
[net] radio OFF -> COMPANION mode (offline frame)
missing-glyph warnings: 0
```

界面：**用户提供的 `ui_sunisle.cpp` 为唯一界面**（单屏，240×320），旧三页 mock-up
已移入 `retired/stage1_three_page_ui/`（不编译、未删除）。

**诚实边界**：没有真实照片时仍是明确的空状态插画（不是假照片）；断网时显示缓存
并标注「离线数据」；缓存也没有时显示「更新中 / 获取失败」，绝不编造温度；
触摸主动禁用（与 LCD 共用 HSPI，bit-bang 会白屏）。

---

## 与任务书的偏差（已审计，不擅自「纠正」）

| 任务书 | 实测 / 处置 |
| --- | --- |
| 屏幕驱动库 TFT_eSPI | 旧工程实际用 **LovyanGFX 1.2.36**；TFT_eSPI 只出现在原厂遗留固件字符串里。按「不得擅自更换屏幕驱动库」→ 保留 LovyanGFX |
| 背光 GPIO21 或 27 | 从旧工程 `display.cpp:50-55` 确认最终用 **21**，亮度 220 / PWM 5kHz / 8bit |
| 触摸手势切页 | 触摸 **NOT CONFIRMED 且 HELD OFF**；按「触摸不稳定时优先保证自动运行」→ 用串口命令切页，不影响核心显示 |
| 当前地点尚未配置 | 已于 2026-11-06 由用户确认：**渑池 / 34.7673N / 111.7587E / CST-8**，写入 `include/secrets.h`（gitignored）。凭据从不进入报告或快照 |
| 三页 UI（Home/Photos/Settings） | 用户后续提供了完整的 `ui_sunisle.cpp/.h` 并指定为正式 UI → **以该单屏 UI 为准**；旧三页 mock-up 移入 `retired/`，不编译、未删除 |

详见 `AUDIT_REPORT.md §8`。

---

## 构建与烧录

```bash
cd D:/ESP32/sunisle
python -m platformio run -e esp32dev

# 只写 app0，不动 bootloader / 分区表 / otadata / app1 / spiffs
python -m esptool --port COM5 --baud 921600 write_flash \
  --flash_mode dout --flash_freq 40m --flash_size 4MB \
  0x10000 .pio/build/esp32dev/firmware.bin
```

- 平台：`espressif32@6.9.0`（Arduino core 2.0.17）、LVGL **8.3.11**、LovyanGFX **1.2.36**，版本锁定不换。
- 分区表 `partitions_sunisle.csv` 与板上 `0x8000` 读出的原厂布局**完全一致**。
- **不 `erase_flash`**；整片备份 `D:/ESP32/backup/full_flash_20261106.bin` 可回滚。
- 首次编译若卡在 CDN 下载，可从 `D:/ESP32/backup/atlas-moments_pre_sunisle_20261106/` 或旧工程
  `.pio/libdeps/esp32dev/` 复制已锁定的 LovyanGFX / lvgl 到本地 `.pio/libdeps/esp32dev/`。

## 目录结构

```
sunisle/
├─ include/
│  ├─ sunisle/config.h          # 冻结引脚 / 时序 / 调色板 / SD 路径
│  ├─ sunisle/state.h           # SystemState + mutex 快照
│  ├─ lv_conf.h  secrets.h(gitignored)
│  ├─ fonts/sunisle_cn.h        # 中文子集字库声明（工具生成）
│  ├─ hardware/  display_manager / backlight_manager / storage_manager / touch_manager
│  ├─ services/  time_service / photo_service / weather_service
│  ├─ ui/        ui_theme / ui_icons / ui_manager / ui_boot / ui_home / ui_photos / ui_settings
│  └─ utils/     logger / memory_monitor
├─ src/                          # 与 include 一一对应
├─ tools/gen_cn_font.py          # 用 Pillow + 系统 CJK 字体生成子集（12/16/28 px）
├─ tools/upload_photo.py         # 串口上传照片到 SD（PC 无读卡器时唯一通路）
├─ docs/                         # SD 目录说明、烧录回滚、照片上传
├─ retired/stage1_three_page_ui/ # 已退场的三页 mock-up（保留可恢复，不编译）
├─ AUDIT_REPORT.md  HARDWARE_CONFIG.md  MEMORY_BUDGET.md  REFACTOR_PLAN.md
├─ NAS_API_ADAPTER.md            # 接口契约 + 缺失清单（服务端未实现）
└─ TEST_REPORT.md  BUILD_REPORT.md
```

## 串口命令（115200，因触摸不可用而提供）

| 命令 | 作用 |
| --- | --- |
| `HELP` | 命令列表 |
| `STATE` | 打印完整状态快照（含 `city=渑池 weather=晴`） |
| `MEM` | 堆 / 最低水位 / 最大可分配块 / PSRAM |
| `SD` | SD 容量、已用、剩余、真实照片数 |
| `TIME` | 时间模块状态 |
| `WX` | 天气模块状态（configured / valid / cached / temp / wmo） |
| `PAGE=home\|photos\|settings` | 切换页面（单屏 UI 下仍保留入口） |
| `BL=<0..255>` | 背光亮度 |
| `INTERVAL=<3..300>` | 轮播间隔（秒） |
| `RESCAN` / `NEXT` | 重新扫描 SD 照片 / 切下一张 |
| `PUT <path> <bytes>` | 串口上传文件到 SD（随后发送原始字节） |
| `DIAG` / `UI` | RAW 显示自检（绕过 LVGL）/ 恢复 UI |
| `REBOOT` | 重启 |

## SD 卡目录

```
/SUNISLE/
├─ config/    device.json  settings.json
├─ photos/    index.json  thumbnails/  *.jpg
├─ weather/   cache.json
└─ system/    sync.json  sync.log
```

首次启动自动建目录；**只在 `/SUNISLE/` 下新增文件，绝不删除或覆盖卡上已有数据**。
配置写入用 `.tmp` + 校验 + 替换，且会保留本阶段不管理的 `lat/lon/timezone` 字段。
SD 缺失时降级运行（UI / 时间仍可用）。

## 视觉

调色板严格取自任务书：奶油白 `#F7F4EC`、浅沙 `#EEE8DC`、深暖灰 `#353A36`、
次级灰褐 `#858477`、日光橙 `#E9A86D`、鼠尾草绿 `#9EAA91`、浅雾蓝 `#B9CBD0`、
分隔线 `#E5E0D5`。照片是视觉主体（216×182），时间与天气是陪衬。
天气图标用 LVGL 原生圆/细条绘制，**不用 Emoji、不引图标字体**。
启动页 1.8s 渐显，只有文字与一条细分隔线，不加载图片资源。

## 资源与安全

- 4MB Flash / **无 PSRAM** / app0 单槽 1310720 B；逐阶段实测见 `MEMORY_BUDGET.md`。
- 中文子集字库：185 个 UI 实际使用的汉字 + 品牌 2 字，约 49KB；ASCII 走 Montserrat 回退。
  新增中文文案只需加进 `tools/gen_cn_font.py` 的 `UI_STRINGS` 并重跑脚本。
- JPEG 复用 LovyanGFX 内置 TJpgD（512B 流式缓冲），**不新增解码库**；阶段 2 实测。
- `include/secrets.h` 已 gitignore，当前为空模板；不存 NAS 密码、不处理登录 Cookie。
- 编译镜像会内嵌任何填入的凭据，分享工程前请扫描二进制，勿分享 `.pio/build/` 或 `.local/`。
