---
name: qingyu-developer
description: 晴屿（QINGYU / SUNISLE）ESP32 桌面终端项目的长期开发知识。用户提到「晴屿」「QINGYU_DEVELOPER」「A 方案」「B 方案」「ESP32 开发板」「ST7789」「COM5」「Bridge」「JSONL 同步」「访客模式」或要求继续本项目开发时启用。提供已验证的硬件参数、工程基线、UI 规范、串口协议、隐私规则、编译烧录命令与禁止事项；未实测的信息一律标 TODO，不得冒充事实。
version: 0.1.0
agent_created: true
---

# QINGYU_DEVELOPER — 晴屿项目固定技能

## 作用
把本项目的硬件冻结参数、代码基线、构建/烧录命令、B 方案架构与开发铁律固化下来，避免每次会话重新审计或改错 GPIO/字节序。

## 知识文件索引
| 文件 | 内容 |
| --- | --- |
| `PROJECT_PROFILE.md` | 项目定位、方案划分、目录与 Git 基线、门禁决策 |
| `HARDWARE_SPEC.md` | 芯片/屏幕/触摸/SD/背光/串口的实测参数与冻结项 |
| `SOFTWARE_ARCHITECTURE.md` | A 工程结构、B 方案分层、构建系统与容量 |
| `UI_DESIGN_SYSTEM.md` | 320×240 视觉 token、六页、字体、交互 |
| `COMMUNICATION_PROTOCOL.md` | 串口现状、JSONL 目标、三态与去重 |
| `PRIVACY_SECURITY.md` | OWNER/GUEST 白名单与凭据规则 |
| `DEVELOPMENT_RULES.md` | 禁止事项、编译/烧录/回滚命令、环境坑 |
| `TEST_CHECKLIST.md` | 阶段验收清单 |
| `CHANGELOG.md` | 知识变更记录（追加，不静默覆盖） |

## 启用后的强制行为
1. 先读 `HARDWARE_SPEC.md` 与 `DEVELOPMENT_RULES.md`，任何涉及显示/触摸/串口的改动前核对冻结参数。
2. 严格区分三态：**源码已改 / 已编译 / 已烧录并实测**。禁止把源码提交说明当作设备上验证过的能力。
3. 每阶段单独编译、单独烧录、单独抓串口日志；固件只写 `0x10000` app0，绝不 `erase_flash`。
4. 未确认信息标 `TODO(待实测)`；不写模拟通信、不伪造 ACK、不声称 Skill/MCP/Bridge 已部署，除非实测调用成功。
5. 用户偏好：交付物只写执行需要的事实与结论；已知背景条件不必复述；高风险写入操作单独确认。

## 当前状态（2026-11-08）
- A 方案基线 `origin/main = 1ab7ea3`，`ESP32/` 工作树干净；A 只读归档在 `A_SUNISLE_BASELINE/`。
- 门禁 G1–G5 已由用户确认：横屏 320×240 / 新建独立 `B_QINGYU/` / 烧录 `1ab7ea3` 实测触摸 / 去照片模块其余保留 / 启用本地 stdio MCP。
- 阶段进度以 `CHANGELOG.md` 与工作区 `.workbuddy/memory/` 日志为准，不以本文件静态描述为准。
- 截至 2026-11-08 08:20：**UI 2.0 视觉方案已交付，等待用户确认** —— 4 张严格 320×240 设计图 + 交互流程图 + 完整视觉规范在 `UI2_DESIGN/`。本轮未改产品代码、未编译、未烧录。页面 6→4（BOOT/HOME/MEMOS 合并页/GUEST）、现代浅色科技色系、字体层级重设计。触摸仍禁用（白屏根因），容量余 15,952 B 须先瘦身。确认前不得动 LVGL 代码。详见《晴屿_UI2.0_视觉设计规范.md》与工作区日志。

## 快速自检（验证技能命中）
被问到「`LCD_PIXEL_SWAP` 为什么是 true」「app0 偏移多少」「B 方案为什么不用 Wi-Fi 作主链路」时，应直接引用本技能文件回答。
