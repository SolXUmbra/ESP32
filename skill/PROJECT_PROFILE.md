# 项目档案

## 定位
桌面 ESP32 终端：把 WorkBuddy 侧的任务、备忘、提醒、摘要同步到屏上，带隐私访客模式。ESP32 不跑 LLM；首期不以 Wi-Fi 为主通信链路。

## 方案划分
| 方案 | 目录 | 基线 | 状态 |
| --- | --- | --- | --- |
| A（SUNISLE 相框） | `ESP32/` | GitHub `SolXUmbra/ESP32`，`origin/main = 1ab7ea3` | 只读基线，禁止修改 |
| A 归档 | `A_SUNISLE_BASELINE/`、`A_SUNISLE_BASELINE_20261108.zip` | `git archive 1ab7ea3`（81 文件，无 `secrets.h`/`.pio`） | 回滚用 |
| B 交付包 | `B_qingyu_ui_delivery/qingyu_ui_delivery/` | 用户上传 zip 的只读解压副本 | 审计完成，UI 组件未集成 |
| B 工程 | `B_QINGYU/` | 从 `1ab7ea3` 快照独立复制 | **待建立** |

## Git 与凭据
- 远端 `https://github.com/SolXUmbra/ESP32`，分支 `main`。
- `include/secrets.h` 被 `.gitignore` 忽略；Wi-Fi 用 `WiFiMulti` 多组 SSID；真实密码不入库、不进归档、不写日志。
- 本机 git 读写远端必须加 `-c http.schannelCheckRevoke=false`（Windows Credential Manager + 本地代理 + schannel 吊销检查）。

## 门禁决策（2026-11-08 06:41，用户确认）
| # | 决策 | 约束 |
| --- | --- | --- |
| G1 | 横屏 | B 目标 320×240；改 `setRotation` 后必须重跑目视编号卡验证并重新校准触摸；`offset_x/y`/`invert`/`rgb_order`/SPI/GPIO 不动 |
| G2 | 新建独立 `B_QINGYU/` | `ESP32/` 与 A 归档全程零修改 |
| G3 | 烧录 `1ab7ea3` 并实测触摸 | 整片备份 → 只写 app0 → 开机日志 → `TOUCH`/`TOUCHPROBE` |
| G4 | 去掉照片模块，其余保留 | 不集成照片轮播/解码与 `PHOTO`/`DEL`/`RESCAN`/`NEXT` 照片命令；保留开机画面、每日一言、天气、Wi-Fi/NTP、显示与触摸 |
| G5 | 启用本地 stdio MCP | 阶段 E 写 `~/.workbuddy/mcp.json`，由用户在连接器管理页「信任」后实测，实测前不声称可用 |

## 执行顺序
G3 设备验证 → 阶段 B 本技能 → 阶段 C 独立工程骨架（横屏 + 去照片）→ D Bridge/SQLite → E MCP → F JSONL 同步 → G 访客模式 → H 启动管理与测试。

## 权威文档（工作区根）
`晴屿项目工程审计报告.md`（阶段 A 事实与差异清单）、`晴屿项目_B方案实施计划.md`（阶段 B–H 与门禁）。

## 更正（2026-11-08 06:53）
- 上表「B 工程基线 `1ab7ea3`」**作废**：该版本真机白屏、按压无反应，已整片回滚。
- **B 基线改为 `9779c6e`**（11-07 目视验证正常的固件，整片快照 `esp32_flash_backup_4MB_20261108_pre_g3.bin`）；`B_QINGYU/` 需按新基线重建。
- G3 结论：失败（不是「已验证」）。触摸 CS、校准、可用性一概未确认；阶段 C 不得依赖触摸。
- 目视编号卡验证为强制项，任何显示/旋转改动后必须重跑。
