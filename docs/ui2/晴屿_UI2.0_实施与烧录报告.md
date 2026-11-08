# 晴屿 UI 2.0 · 实施与烧录报告

> 状态：**已编译 → 已烧录 → 串口验证通过**。真机目视结论仍需人眼确认。
> 时间：2026-11-08 08:36 / 固件 `QINGYU 2.0.0-ui2`

## 1. 决策采纳

§11 的 D1–D7 全部按建议执行：四张设计图与 Token 通过；BOOT 1.5 s；GUEST 深石墨；触摸维持禁用；时间数字用 `INK`、品牌蓝留给任务数字；执行瘦身；`TITLE` 复用 `sunisle_cn_16`。

## 2. 代码改动

| 文件 | 变更 |
|---|---|
| `tools/gen_cn_font.py` | 补 11 个缺字 `伴侣动后启完情桌确详面`；`sunisle_cn_16` 由 288 字缩到 64 字静态集；删除 `sunisle_cn_boot`；新增 `sunisle_num_34`（Segoe UI Light、15 字形、比例 advance）；生成器支持 ASCII 字形与 `adv_map` |
| `src/ui/sunisle_cn.cpp`、`include/fonts/*.h` | 重新生成；14 px 面 300 字（白名单来源） |
| `include/lv_conf.h` | 关闭 `LV_FONT_MONTSERRAT_18/28` |
| `include/ui/qingyu_ui.h`、`src/ui/qingyu_ui.cpp` | 重写为 BOOT / HOME / MEMOS / GUEST 四画面；删 TASKS / REMINDERS；新增比例数字时钟、卡片、进度条、分页点、按钮；新增 `qy_ui_set_boot_progress()`、`qy_ui_set_status()`、`qy_ui_set_work()`、`qy_ui_action()` |
| `src/ui/ui_theme.*` | 色板换成 UI 2.0 Token |
| `src/ui/ui_bridge.cpp/.h` | 四画面数据适配；新增 `DEMO` 示意数据开关、`ACT` 动作、`setBootProgress()`；保留 `sanitizeDynamic()` 与访客丢文本逻辑 |
| `src/ui/ui_manager.cpp` | 200 ms 淡入 + 1.5 s BOOT 单次线性进度推进；页面枚举收敛为 2 |
| `src/main.cpp` | `PAGE=home|memos|guest|boot`；新增 `DEMO=on|off`、`ACT=ack|later|detail|task` |
| `include/sunisle/state.h` | `Page` 收敛为 HOME/MEMOS/GUEST/BOOT |
| `include/sunisle/config.h` | `BOOT_FADE_MS=200`、`BOOT_HOLD_MS=1500`、`BOOT_TOTAL_MS=1500`、版本号 |

**未改动**：GPIO / SPI / `setRotation(1)` / USB 115200 / 分区表 / 服务层；触摸仍不注册 indev；上电强制 BOOT → GUEST，私有 setter 在 Guest/Boot 丢弃文本。

## 3. 容量账（app0 = 1,310,720 B）

| 项 | 字节 | 占比 | 余量 |
|---|---:|---:|---:|
| 旧 `build_g`（阶段 C） | 1,294,768 | 98.78% | 15,952 |
| **UI 2.0** | **1,202,117** | **91.70%** | **108,603** |

净省 92,651 B，为阶段 D 的 Bridge / JSONL 留出 ≥ 40 KB 目标已满足。

## 4. 串口验证摘要（`esp32_ui2_verify_log.txt`）

- `QINGYU / UI 2.0`，`UI 2.0 READY heap=201612 B flush=20`；无白屏 / 无 LVGL 错误。
- `[ui] UI 2.0 BOOT (1500 ms) -> GUEST` → `BOOT done -> GUEST (default, no private text cached)`。
- NTP 校时成功 `time=08:36`；Wi-Fi `HOME` 上线；天气真实 `阴 8 C`；每日一言开机轮换 #8/30。
- `PRIVACY=OWNER` / `PAGE=home` / `PAGE=memos` / `NEXT` / `ACT=ack` / `DEMO=on|off` 全部按预期响应；heap 稳定 180 KB / min 142 KB。
- 设备当前停在 **HOME（OWNER，DEMO=on）**。

## 5. 屏幕操作

| 命令 | 效果 |
|---|---|
| `NEXT` | HOME ↔ MEMOS |
| `PAGE=guest` | 深石墨访客屏 |
| `PRIVACY=OWNER` | 回到工作页（本地调试，非 Bridge 授权） |
| `ACT=ack\|later\|detail\|task` | 等效轻触动作，改变提醒状态 / 展开备忘 |
| `DEMO=off` | 切回真实空态（Bridge 未接入，摘要显示「等待 Bridge 接入」） |
| `DEMO=on` | 设计稿示意数据，顶栏明确显示 `DEMO` |

示意数据只出现在两个 Owner 页且带 `DEMO` 标识；GUEST 始终只有公开的时钟、日期、每日一言。

## 6. 待目视确认（串口不能代替看屏）

- [ ] 34 px Light 数字时钟字形、字距是否正常
- [ ] 中文无豆腐块 / `?`，尤其 BOOT「AI 桌面工作伴侣」
- [ ] 卡片、按钮、分页点无越界与重叠
- [ ] GUEST 深石墨与浅色工作页的对比符合预期
- [ ] 横屏方向正确，彩色面积克制

## 7. 回滚

```bash
python -m esptool --port COM5 --baud 921600 write_flash \
  --flash_mode dout --flash_freq 40m --flash_size 4MB \
  0x10000 esp32_app0_backup_20261108_pre_UI2.bin
```

源码快照：`B_QINGYU_pre_UI2_snapshot.tar.gz`（排除 `.pio`）。烧录日志 `esp32_ui2_flash_log.txt`、编译日志 `ui2_build_log.txt`、验证日志 `esp32_ui2_verify_log.txt`。

## 8. 未完成（阶段 D）

Bridge / MCP / SQLite / JSONL 接入、动态中文栅格化位图通道、Owner 改为已验证 Bridge 会话、触摸硬件单独验证、页面切换淡入动画、72 h 压测。

---

## 9. v2 修订：字体基线修复（真机反馈「所有字体歪歪扭扭」）

### 9.1 根因

与屏幕、SPI、旋转、RGB565 无关，是 `tools/gen_cn_font.py` 自绘字库的栅格化逻辑错误：

```python
x0, y0, x1, y1 = font.getbbox(ch)          # 每个字自己的墨迹框
ox = (size - gw) // 2 - x0                 # 把墨迹逐字居中
oy = (size - gh) // 2 - y0                 # 每个字得到一条不同的基线
```

每个字符被独立居中进 `size × size` 方框，等于给每个字换一条基线：「一」「，」「。」「:」「1」的墨迹被抬到格子中央，全高汉字却在底部，整行上下乱跳。34 px 数字面还把 `box_w` 固定为 34、却给 `:` 设 12 px advance，位图与步进不匹配，进一步叠加成字距忽宽忽窄。中英混排时自绘面的 `line_height/base_line`（16/2）又与内置 Montserrat 14（16/3）差 1 px，ASCII 与中文再错一层。

`UI2_DESIGN/font_baseline_fix.png` 是按 LVGL `lv_draw_sw_letter()` 公式复现的 BEFORE / AFTER 对照：BEFORE 明显飘字，`09:41` 高低不齐；AFTER 全部落在同一基线、数字恢复比例字距。

### 9.2 修复

- `render_glyph()` 改用 Pillow `anchor="ls"`（左-基线）统一基线，存**真实墨迹 bbox**，输出 `(box_w, box_h, ofs_x, ofs_y)`；`ofs_y = -y1`、`ofs_x = x0`，与 LVGL `box_bottom = line_top + H - ofs_y` 一致。等价于 `lv_font_conv` 的字体结构。
- `adv_w` 改由 `font.getlength(ch)` 取得（8.4 定点），删除硬编码 `NUM_ADV`；中文天然全宽，数字自然比例。
- 行度量对齐内置 Montserrat：14 px `16/3`（H=13）、16 px `18/3`（H=15）、28 px Kai `31/5`、34 px Light `30/3`，混排基线一致。
- 新增断言覆盖 `LV_FONT_FMT_TXT_LARGE=0` 的位域上限：`adv_w < 4096`、`box_w/box_h < 256`、`ofs_x/ofs_y ∈ int8`、`bitmap_index < 1 MB`。
- 位图总量 90.6 KB → 88.5 KB；固件 1,202,117 B → **1,182,537 B（90.2%，余 128,183 B）**。

### 9.3 按规格逐条复核

| 规格 | 落地 |
| --- | --- |
| A 开机 | 居中「晴屿」28 px Kai + 「AI 桌面工作伴侣」+ 品牌蓝细线 + 进度指示（简洁动态标识）；删掉 `QINGYU` 字标、`正在启动`、`2.0` 版本号；`BOOT 1500 ms` → 强制 GUEST，仅授权后可回工作页 |
| B 工作首页 | 顶栏「晴屿 / SUNISLE」+ `USB ON/OFF` 状态与状态点；左侧 34 px 数字时钟、其下中文日期星期；右侧今日任务数 / 完成进度条；中部 WORKBUDDY 摘要两行；底部下一项任务；无独立任务中心 |
| C 备忘与提醒 | 顶部标题 + 待处理数量；上半区智能提醒（时间 / 事项 / 状态 + 确认 / 稍后）；下半区 AI 备忘（标题 / 摘要 / 更新时间 / 查看详情）；`ACT=detail` 等效「查看更多」；无独立提醒页 / 备忘页 |
| D 访客模式 | 深石墨 `#171D2B` 覆盖层、明亮品牌与欢迎语、日期时间、每日一言、锁标识；不缓存任何私有文本；未注册触摸 indev，无法触摸进入 Owner |

旋转 `setRotation(1)`、GPIO、SPI、USB 串口协议均未改动；`sanitizeDynamic()` 白名单仍取 14 px 面（300 字），动态中文降级逻辑不变。

### 9.4 v2 烧录与验证

- 编译 71 s 通过；`0x10000` 单区写入，`Hash of data verified`；日志 `ui2_build_log_v2.txt` / `esp32_ui2_flash_log_v2.txt` / `esp32_ui2_verify_log_v2.txt`。
- 串口链路全部通过：BOOT 1500 ms → GUEST、NTP 08:47、Wi-Fi / 真实天气、页面 0↔1、`ACT=ack/detail`、GUEST↔OWNER、heap 稳定，无 `glyph dsc. not found`、无 LVGL 报错。
- 设备停在 **HOME（OWNER / DEMO on）**。设计图 `01_boot` 与 `index.html` 已同步为精简后的 BOOT。
- 回滚仍用 `esp32_app0_backup_20261108_pre_UI2.bin`；本次改动前的源码快照同为 `B_QINGYU_pre_UI2_snapshot.tar.gz` 之后的 `B_QINGYU/` 工作副本。

**仍需目视确认**：真机上中文基线是否整齐、34 px 数字是否不再忽高忽低、卡片内文字有无重叠。串口只能证明逻辑，不能代替看屏。

---

## 10. GUEST 2.1 多巴胺版（2026-11-08 09:31–09:55）

用户否决深石墨 GUEST，要求「放大时间和提示、重新考虑布局和色彩、要点多巴胺色彩」；同时确认按压有效、一体化屏触摸可用。

- GUEST 改为纵向四段高饱和撞色：紫品牌条 / 黄时间卡 / 粉欢迎卡 / 薄荷绿每日一言。时间 34→**56 px** 新增 `sunisle_num_56`（H=41/BL=3，12,798 B），主提示改用 28 px 楷「欢迎来访」，副提示与日期升到 16 px；删英文锁行。HOME/MEMOS/BOOT 未动，隐私逻辑、GPIO/SPI/旋转/串口协议未动。
- 16 px 面补 GUEST 静态字与「未校时」（81 字 / 18,944 B），修掉真机首版 `glyph dsc. not found for U+672A / U+6821`；14 px 面 303 字白名单不变。
- 最终固件 **1,228,045 B / 93.7%，余 82,675 B**，`0x10000` Hash verified，开机零缺字警告，设备干净重启后停在 **GUEST**。
- 触摸新增四条独立探测路径（Arduino SPI 全双工、GPIO matrix 换 MISO、MISO×CS 117 组合、IRQ 电平+SPI 同窗采样）；详见 `晴屿_GUEST2.1_多巴胺版交付说明.md` §2 与 `晴屿_触摸硬件诊断报告.md`。仍未取到控制器坐标，等板子型号 / 引脚定义。
- 回滚：`esp32_app0_backup_20261108_pre_guest21.bin` + `B_QINGYU_pre_guest21_snapshot.tar.gz`。

---

## 11. 极简收口（2026-11-08 10:20–10:40）：GUEST 2.2/2.3 去标签 + 重居中

### 11.1 变更链

1. 多巴胺方案被否（「还是简约中式吧，注意页面布局符合互联网大厂产品风格」）→ 全站回到单一宣纸/墨/朱砂/青瓷 token，GUEST 与 BOOT/HOME/MEMOS 同一设计系统，不再是独立"皮肤"。
2. 连续三轮删冗余文案：「现在时间」「每日一言」→「访客模式」→「欢迎来访」。判断标准：**该文字是否只是把画面已经表达的信息再写一遍**。时钟不必标注是时间，引文不必标注是每日一言，访客屏不必自我声明是访客模式。
3. 删掉 28 px 楷「欢迎来访」后主视觉失去一层，整段上移重排（不是简单留白）。

### 11.2 GUEST 2.3 坐标（现行）

| 元素 | 坐标 / 样式 |
| --- | --- |
| 顶栏 | 朱砂印 16,10,6,6；楷「晴屿」28,7；`SUNISLE` 66,11 字距3；细线 16,34,288,1 |
| 时间 | `TOP_MID y=68`，ink 69..109，56 px Light 比例数字面 |
| 日期 | `TOP_MID y=116`，ink 116..131，16 px 墨 |
| 一线朱砂 | 136,144,48,2 |
| 提示 | `TOP_MID y=156`，ink 156..172，16 px 淡墨 |
| 每日一言卡 | 16,204,288,30，1 px 描边；引文 26,212,268，居中 `LV_LABEL_LONG_DOT` 单行 |
| 说明性标签 | 无 |

纵向重心核验：可用区 34..204，ink 上留 35 px、下留 32 px，重心落在几何中线；ink 带彼此不相交（69..109 / 116..131 / 144 / 156..172 / 204..）。设计图 `04_guest_320x240.png` 与固件逐像素同源（同一组坐标常量）。

### 11.3 验证

- 编译 **1,228,481 B / 93.7%（余 82,239 B）**，RAM 97,556 B / 29.8%；`0x10000` 单区写入，`Hash of data verified`。
- 串口 `esp32_boot_log_cn23.txt`：SD 3 张 JPEG、backlight、ST7789 320×240 20 MHz、touch HELD OFF、BOOT 1500 ms → **GUEST**、Wi-Fi `HOME` 192.168.0.126、NTP `SYNCED`、真实天气 11 ℃ 多云、HELP/heap 199,640 B；**零 `glyph dsc. not found`、零 LVGL 报错**。
- 回滚：`esp32_app0_backup_20261108_pre_cn23.bin`（本轮前 app0 原样回读）。
- 未解决：触摸控制器仍未取到坐标（缺板子原理图/引脚），交互继续走串口等价命令 `PAGE=` / `ACT=` / `PRIVACY=`。
