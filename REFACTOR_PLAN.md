# REFACTOR_PLAN.md — 晴屿 SUNISLE 重构与分阶段实施计划

> 前置：`AUDIT_REPORT.md` 已完成，备份已完成，独立目录 `D:/ESP32/sunisle/` 已建立。
> 原则：**不覆盖旧工程**、**每阶段单独编译 / 烧录 / 测试 / 记录**、
> **不擅自更换 LVGL 主版本与显示库**、**不用模拟数据冒充真实功能**。

---

## 1. 产品与工程目标

把「ATLAS MOMENTS 开发板演示工程」重构为「晴屿 SUNISLE 桌面电子相框」：
照片为视觉主体，时间天气为陪衬；默认断网也能用；视觉精致、不像开发板项目。
功能严格限定为**照片相册 / 天气 / 时钟 / 精致 UI**，不做蓝牙、音乐、播放器。

## 2. 架构（模块边界，逻辑不写在 main.cpp）

```
sunisle/
├─ include/sunisle/config.h          # 冻结引脚、时序、调色板、SD 路径（非机密）
├─ include/sunisle/state.h           # SystemState 枚举 + 快照 + 互斥发布
├─ include/secrets.h                 # gitignored，Wi-Fi / 设备 Token（留空待用户填）
├─ include/fonts/sunisle_cn.h        # 中文子集字库声明（Pillow 生成）
├─ include/hardware/
│   ├─ display_manager.h             # LovyanGFX 设备 + LVGL disp_drv + flush
│   ├─ backlight_manager.h           # GPIO21 PWM
│   ├─ storage_manager.h             # SD 挂载降级、目录、原子写、索引恢复
│   └─ touch_manager.h               # 明确 HELD OFF（阶段 1 不注册 indev）
├─ include/services/
│   ├─ time_service.h                # 时间模块：未校时 / 已校时 / 断网续走
│   ├─ photo_service.h               # 照片索引、扫描、轮播、异常跳过
│   ├─ weather_service.h             # 真实天气、缓存、离线标识
│   ├─ wifi_manager.h                # 模式 A/B 的 Wi-Fi 生命周期
│   └─ sync_service.h                # 同步编排 + 安全清理四重条件
├─ include/network/
│   ├─ nas_api_adapter.h             # 六个约定接口（仅适配层，不假设已实现）
│   ├─ weather_api_adapter.h         # Open-Meteo 解析
│   └─ http_client.h                 # 原生 WiFiClient 受控缓冲
├─ include/models/                   # photo_model.h / weather_model.h / device_config.h
├─ include/ui/
│   ├─ ui_theme.h  ui_icons.h        # 调色板 + LVGL 原生绘制天气图标
│   ├─ ui_manager.h                  # 三页切换、启动动画编排
│   ├─ ui_boot.h / ui_home.h / ui_photos.h / ui_settings.h
├─ include/utils/ logger.h  memory_monitor.h
└─ src/**.cpp                        # 与 include 一一对应
```

**任务边界**
- LVGL 只在 `loop()`（core 1）里访问；其它任务只写 `state` 快照（mutex 保护），
  由 UI 在 tick 里读取后自行刷新控件。
- 网络请求、SD 读写、JPEG 解码一律不放在 LVGL 回调里；阶段 3 起放到 core 0 任务。
- 阶段 1 无网络任务，所有操作在 `setup()` 完成或由串口命令触发，主循环只跑 LVGL。

## 3. 任务状态机（任务书第 9 节）

`BOOT → LOCAL_INIT → HOME_READY`（阶段 1 到此）
阶段 3 追加 `WIFI_CONNECTING → WEATHER_UPDATING / SYNCING → HOME_READY`
异常分支 `OFFLINE / ERROR_RECOVERY`：SD 缺失、索引损坏、网络失败都降级而非崩溃。

## 4. 视觉规范（任务书第 6 节，最高优先级）

| 角色 | 色值 |
| --- | --- |
| 奶油白（底） | `#F7F4EC` |
| 浅沙色（照片占位 / 分隔块） | `#EEE8DC` |
| 深暖灰（主文字） | `#353A36` |
| 次级灰褐（辅助文字） | `#858477` |
| 日光橙（强调 / 太阳） | `#E9A86D` |
| 鼠尾草绿 | `#9EAA91` |
| 浅雾蓝 | `#B9CBD0` |
| 分隔线 | `#E5E0D5` |

- 竖屏 240×320：顶部 ~50px（日期 / 星期 / 天气图标 + 温度），
  中部 ~196px（**照片主体**，保持宽高比，禁止拉伸），底部 ~68px（大号时间 / 城市 / 状态）。
- 留白优先；**不用**大面积纯黑、科技蓝、荧光色；不用密集圆角卡片、阴影、边框堆叠。
- 天气图标：LVGL 原生绘制（线 + 圆弧），低饱和，统一风格；**不用 Emoji、不引图标字体**。
- 启动页：奶油白 + 「晴屿 / SUNISLE / 把喜欢的时光，留在身边。/ A Little Island of Sunshine.」，
  轻量渐显 1.8s，不加载图片资源。
- 三页：Home / Photos / Settings；默认 Home；触摸不可用时自动运行 + 串口命令切页。

## 5. 阶段拆分与验收口径

| 阶段 | 交付 | 编译/烧录 | 验收 | 本轮状态 |
| --- | --- | --- | --- | --- |
| **0 审计与备份** | 4 份文档 + 完整备份 + 独立目录 | 不编译 | 旧工程零改动、备份可恢复 | ✅ 已完成 |
| **1 基础系统** | 显示/LVGL/SD 降级/启动页/三页 UI/本地时间模块/基础设置 | 单独编译 + 仅写 app0 | 30 分钟不花屏不死机 | 🔄 进行中（编译与短时实机验证；30 分钟需真实时间，未做完不写 PASS） |
| **2 照片系统** | SD 扫描 / JPEG 流式解码 / 适配 / 轮播 / 淡入 / 索引 / 异常跳过 | 单独编译 | 100 张不同尺寸真实 JPEG + 2 小时压测 | ⏸ 阻塞：需用户提供真实照片（SD 在板上，PC 无法直接写） |
| **3 天气与时间** | Wi-Fi / NTP / 真实天气 / 矢量图标 / SD 缓存 / 断网显示 | 单独编译 | 联网取真实天气；关 WiFi 后照片时间正常 | ⏸ 阻塞：需用户重新确认城市/经纬度/Wi-Fi 凭据 |
| **4 NAS 同步** | Adapter / 设备 Token / 清单 / 增量 / 校验 / 安全清理 | 单独编译 | 真实服务联调 | ⏸ 阻塞：SUNISLE Sync Server 未实现，只交 Adapter + 缺失清单，不宣称成功 |
| **5 整体优化** | UI / 流畅度 / 启动 / 内存 / SD / 断网恢复 | 单独编译 | 2 小时连续运行 | ⏸ 依赖 2–4 |

## 6. 关键实施决策（有据可依，不拍脑袋）

1. **保留 LovyanGFX，不换 TFT_eSPI**：任务书写 TFT_eSPI，但已验证工程用 LovyanGFX；
   任务书同时要求「不得擅自更换屏幕驱动库」「以现有工程实际情况为准」。见 `AUDIT_REPORT.md §8-1`。
2. **冻结全部显示参数**：见 `HARDWARE_CONFIG.md §2`，四个高危项不得改。
3. **背光 GPIO21**：从旧工程 `display.cpp:50-55` 确认，非猜测。
4. **触摸 HELD OFF**：与 LCD 共用 HSPI，bit-bang 会白屏。按任务书「触摸不稳定时优先保证自动运行」，
   阶段 1 不注册 indev、不做热区，页面/设置走串口命令；核心显示不受影响。
5. **JPEG 不新增依赖**：LovyanGFX 内置 TJpgD（512B 流式缓冲 + 降采样），
   `drawJpgFile(SD, path, ...)` 直接可用。阶段 2 实测后再决定是否换 JPEGDEC。
6. **中文子集字库**：Pillow + 微软雅黑生成 185 字 / 约 49KB，ASCII 走 Montserrat 回退。
   生成脚本可重复执行，新增 UI 文案只需加进 `UI_STRINGS` 并重跑。
7. **不沿用旧工程凭据**：新 `secrets.h` 留空；城市/经纬度留空并标「未配置」，
   天气不取数、UI 明确显示「天气未配置」。需要用户在新任务里重新确认。
8. **SD 数据只读安全**：只在 `/SUNISLE/` 下新增，不删不覆盖已有文件；
   照片清理必须同时满足「完整清单 / 格式有效 / 同步成功 / 确已取消收藏」四条，
   远程异常、认证失效、空清单、解析失败一律禁止批量删除；优先软删除 + 回收站。
9. **默认关闭 Wi-Fi**：`COMPANION` 模式不启 Wi-Fi、不发起任何网络请求；
   只有 `SYNC` 模式（定时或手动）才连接，完成后关闭。与旧工程常连架构不同。
10. **烧录纪律**：只写 `app0 0x10000`；不 `erase_flash`；每次 `Hash of data verified`；
    整片备份 `28cc552e…acb5f` 与旧工程备份都可回滚。

## 7. 串口命令（阶段 1，因触摸不可用而提供）

`HELP` / `STATE` / `MEM` / `PAGE=home|photos|settings` / `BL=<0..255>` /
`INTERVAL=<秒>` / `SD` / `TIME` / `RESCAN` / `REBOOT` / `DIAG`（RAW 显示自检，绕过 LVGL）。
命令只改变本机运行状态，不产生任何虚构数据。

## 8. 回滚方案

| 场景 | 操作 |
| --- | --- |
| SUNISLE 固件异常 | `esptool write_flash 0x10000 <备份中旧工程 v9 固件>`，或整片回写 `full_flash_20261106.bin` |
| 源码误改 | 从 `D:/ESP32/backup/atlas-moments_pre_sunisle_20261106/` 按 `BACKUP_MANIFEST_SHA256.txt` 校验恢复 |
| SD 目录异常 | 删除 `/SUNISLE/` 由用户确认后手动处理；固件只重建缺失目录，不覆盖已有文件 |
| 分区表被误改（本工程不会做） | 整片回写 `full_flash_20261106.bin` |
