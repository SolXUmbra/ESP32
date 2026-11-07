# 晴屿 SUNISLE B

> 把喜欢的时光，留在身边。 / *A Little Island of Sunshine.*

一台桌面**备忘录提醒**设备：备忘列表、到点弹窗提醒、日期时间与天气，
并接收 WorkBuddy 下发的 `CMD|` / `SKILL|` / `TASK|` 指令。相册模块已移除。

> **版本说明**：本分支 `b-version` 为 B 版。A 版（相册）完整保留在 `main`
> 分支与 `vA` 标签（`D:/ESP32/sunisle/`），源码、构建产物与本地配置均未改动。

---

## B 版速览（2026-11-08）

| 能力 | 说明 |
| --- | --- |
| 备忘录 | `memos.tsv` 本地持久化、软删除 + `.bak` 备份、到点触发、稍后提醒、每日/每周重复 |
| 提醒交互 | 屏幕弹卡片，轻触即完成；30 秒自动收起 |
| 技能配置 | `SKILL|SET|memo.lead_min=10` 等，存 `skills.tsv`，`display.brightness` 即时生效 |
| 综合任务 | `TASK|ADD|name=..|steps=a;b;c`，每推进一步生成一条备忘提醒 |
| 下达指令 | 串口与可选 HTTP 轮询共用 `commandService::execute()`，统一 `ACK|` / `ERR|` 回执 |
| 网络下发 | 默认关闭；在 `include/secrets.h` 填 `SUNISLE_CMD_HOST` 即启用（纯 HTTP，无 TLS） |
| 中文字库 | `sunisle_cn_memo`：414 个日常常用字子集；超出字符需加进 `MEMO_CHARS` 重新生成 |

编译：`Flash 90.3% (1,184,157 B)` / `RAM 27.8%`，余量约 123.6 KB。
设备尚未实机烧录验证（COM5 未接入）。

串口速试：
```
CMD|help
CMD|add|text=开会|at=09:30
CMD|add|text=喝水|in=10
CMD|list
TASK|ADD|name=晨间流程|steps=起床;喝水;出门
SKILL|GET
```

---

## A 版历史状态（2026-11-06 19:05 实测，仅存档）

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
