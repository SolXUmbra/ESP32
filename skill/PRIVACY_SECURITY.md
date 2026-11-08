# 隐私与安全规则

## 模式
- OWNER / GUEST 白名单；**上电默认 Guest**。
- Guest 公开范围：品牌、欢迎语、时间日期、经明确 `public` 标记的公开信息。
- Guest 禁止显示：私人任务、备忘、摘要、通知历史、财务结果、对话内容、账号状态详情。
- 状态优先级：Guest > Reminder overlay > Current owner page；任何未知状态默认 Guest。

## 切换规则（阶段 G）
| 事件 | 结果 |
| --- | --- |
| 上电 / 复位 / USB 重连 / Bridge 重启 / 会话无法确认 | 保持 Guest |
| Windows 锁屏 | 自动 Guest |
| 解锁 | 需 Bridge 已验证会话令牌授权后才可回 Owner |
| Guest 下普通触摸或简单字符串 | 不得切到 Owner |

授权依赖 Bridge 会话令牌 + 有效期；公开信息必须显式 `public` 标记。

## 组件侧必修缺陷
- `qy_ui_set_privacy(GUEST)` 只清 task/memo/reminder 标签与 `home_summary/next`；**未清** `home_greeting`、`home_date`、`home_counts`、`page_title`，也未清静态 `counts[96]` / `summary[160]` 缓冲 → 不满足「清理私人文本缓存而不只是隐藏控件」。
- `qy_ui_set_privacy(OWNER)` 无任何授权校验 → 必须加已验证会话令牌校验。
- 要求：切换 100 次后抓取 LVGL 对象树文本核对，无私人文本残留。

## 凭据
- `include/secrets.h` gitignored；Wi-Fi 密码、设备令牌不入库、不进归档、不写日志。
- Bridge 日志不得含私人内容；SQLite 每日备份并版本化 schema migration。
- 回滚：固件整片 flash 备份 + 数据库备份；停 Bridge 授权即强制 Guest。

## 待确认
`TODO(待实测)`：Windows 锁屏/解锁事件检测方案（本机 `Add-Type` 编译 .NET 曾被安全策略拒绝，`TermService` Stopped）；令牌有效期与存储位置。
