# 知识变更记录（追加，不静默覆盖）

## 2026-11-08 06:41 — v0.1.0 建立
- 由阶段 A 审计（以 GitHub `origin/main = 1ab7ea3` 为基线）生成；只写已确认事实，推断与未实测项分别标注。
- 用户确认门禁 G1–G5：横屏 320×240 / 新建独立 `B_QINGYU/` / 烧录 `1ab7ea3` 实测触摸 / 去照片模块其余保留 / 启用本地 stdio MCP。
- 设备仍运行 `9779c6e` 旧固件（1,284,752 B，11-07 02:28 烧录）；触摸与瘦身只在源码层。
- 本轮创建 10 个知识文件，未修改任何生产代码、未烧录、未创建 MCP/Bridge。
- 待补：G3 设备实测结论写入 `HARDWARE_SPEC.md §4`；本机复编译尺寸写入 `SOFTWARE_ARCHITECTURE.md`；Skill 命中验证结果。
- 上述三项已于同日 06:50 补齐，见下条。

## 2026-11-08 06:50 — G3 设备验证 + 阶段 B 实测通过
- **Skill 实测调用成功**：`Skill(qingyu-developer)` 返回本目录 `SKILL.md` 全文，触发词命中；10 个知识文件齐全，源副本在 `B_QINGYU/skill/`。
- **编译卡住根因确认**：工作区 safe-delete shim 拦截 SCons 删除已有 `.o`（`sitecustomize.py::_safe_remove`），在旧构建目录里编译到 `lv_draw_rect.c.o` 即挂起；`--project-option` 在 PlatformIO 6.2.0 不存在。改用**全新空构建目录**（独立工程）后 118.36 s 编译成功。
- **独立工程 `B_QINGYU/` 已建立**：从 `A方案_源码快照_1ab7ea3.zip` 解压 64 文件，补 `include/secrets.h`、复制 `.pio/libdeps`（219 MB），零源码修改即编译通过 → 等同 `1ab7ea3` 固件。
- **G3 烧录与实测完成**：整片备份 `esp32_flash_backup_4MB_20261108_pre_g3.bin`（4,194,304 B，460800 波特 99 秒）；只写 app0 成功且校验通过；开机、LVGL、Wi-Fi、NTP、天气、SD、每日一言全部正常；触摸驱动 init 与 LVGL indev 注册成功，但 **CS 引脚与校准仍未确认**（`TOUCHPROBE` 对 33/32/34 都返回 `init=OK read=no`，无法区分；需按压实测）。
- 设备现运行 `1ab7ea3` 等价固件（不再是 `9779c6e`）。`ESP32/` 与 A 归档全程零修改；未创建 MCP/Bridge；未向 GitHub 写入。
- 下一步：阶段 C（横屏 320×240 + 去照片模块 + 六页 UI 集成），横屏改动需目视编号卡验证与触摸重校准。

## 2026-11-08 06:53 — **更正上一条：G3 判定失败并已回滚**
- 用户目视反馈：**白屏、按压无反应**。上一条中「日志层排除白屏」的推断**错误，作废**；flush 计数、STATE/MEM、照片解码 checksum 都不能替代人眼看屏。
- 诊断 `white_screen_diag.py` → `esp32_white_screen_diag.txt`：固件仍在跑（`flush=6806`、`heap=109444 B`、`DIAG` 已下发、`UI` 已恢复），但显示不可用。
- 已整片回滚 `0x0 ← esp32_flash_backup_4MB_20261108_pre_g3.bin`（`Hash of data verified`），回滚后开机日志 `build Nov 7 2026 02:27:58`（`9779c6e`）、`READY ST7789 240x320`、Wi-Fi HOME、照片轮播正常。等待用户目视确认恢复。
- **B 方案基线改为 `9779c6e`（已目视验证），禁用 `1ab7ea3`**；`B_QINGYU/` 需按新基线重建。触摸在任何情况下都不得先假定可用。
- 白屏最可疑改动：`e3f5cb2` 的 XPT2046 共享 HSPI + LVGL 30 ms 轮询（CS 未确认、最后一次 probe 改成 34）、`ui_sunisle.cpp` 全屏 `touch_catcher` 覆盖层。**未二分验证，属假设。**
- 新增文件：`white_screen_diag.py`、`touch_press_probe.py`、`esp32_white_screen_diag.txt`、`esp32_rollback_boot_log.txt`、`B方案_阶段B_G3执行报告.md`。

## 2026-11-08 07:14 — 白屏二分结论 + 阶段 C 起步
- 二分完成（用户目视）：实验 A（`1ab7ea3` 全量 + 关闭 XPT2046 init）**显示正常** → 白屏元凶 = `e3f5cb2` 触摸层；`1ab7ea3` 的瘦身/字库改动不导致白屏。实验 B（`9779c6e` + 触摸层）已编译、不必烧录。
- **新约束**：实验 A 目视「貌似有字体丢失」→ `1ab7ea3` 的 4bpp CJK 与 `sunisle_latin` 同名符号子集不得复用；`verify_fonts.py ALL CHECKS PASSED` 只是字节级自洽，不证明真机字体可用。阶段 C 字库按 B 六页文案重生成并真机目视确认。
- 阶段 C 不启用触摸：不 init XPT2046、不注册 LVGL indev、不留 `touch_catcher`；触摸 CS/校准/可用性维持「未确认」。
- `B_QINGYU/` 重建为 `9779c6e`（零改动 1,284,752 B / 98.02% / 余 25,968 B）；横屏驱动层改完并编译通过（1,284,736 B，**未烧录**）。
- 设备已整片回滚 `0x0 ← esp32_flash_backup_4MB_20261108_pre_g3.bin`，`Hash of data verified`。

## 2026-11-08 07:33 — 阶段 C 代码/编译/烧录完成（目视未确认）
- 用户确认字库策略 ①+③：静态文案全量 8bpp 字库 + 动态文本白名单降级，Bridge 栅格化下发留 TODO 到阶段 D。
- `B_QINGYU/` 完成：横屏 320×240、去照片模块、六页 UI（BOOT/HOME/TASKS/MEMOS/REMINDERS/GUEST）、`qingyu_ui` C++ 移植并修 4 类缺陷、上电默认 GUEST。
- 容量：`build_e` 编译 SUCCESS 113.52 s，固件 1,164,928 B / app0 88.88% / 余 145,792 B（比 9779c6e 少 119,824 B）。
- 烧录 `0x10000` Hash verified；串口日志与六页 PAGE/NEXT/PRIVACY 全部正常；**屏幕目视未确认，禁止声称显示正常**。
- 整片备份 `esp32_flash_backup_4MB_20261108_pre_stageC.bin`。详见《B方案_阶段C_改造说明.md》。

## 2026-11-08 07:54 — 阶段 C 第一次目视通过 + 字体/品牌修订
- 用户目视 build_e（阶段 C 首版）：**屏幕显示正常** → 横屏 `setRotation(1)` 方向正确、无白屏、无缺字、卡片无越界。§8 目视门禁对该固件通过。
- 用户反馈两点：英文 logo 没见、字体偏小 → build_f 修订：`SUNISLE` 字标加到全部 6 页；字号 12→14（正文）/16（主文案）/18（Guest 时钟 Latin）；两档 CJK 字库各覆盖 288 字 8bpp；全部页面坐标重排；无 indev 的按钮改为页码。
- 编译挂起根因**确定**：SCons 收尾删除 288 个 `tmp*.tmp` 触发 `sitecustomize.py` 批量删除确认；单条命令加 `CODEBUDDY_SAFE_DELETE_ENABLED=0` 即解，85.53 s SUCCESS。不必再每次换全新 build_dir。
- 固件 1,235,829 B / app0 94.3% / 余 74,891 B；app0 备份 `esp32_app0_backup_20261108_pre_typofix.bin`；烧录 Hash verified；串口与六页轮播全通过，停在 GUEST。**build_f 目视未确认**。
- 详见《B方案_阶段C_改造说明.md》§9。

## 2026-11-08 08:03 — 第二次目视反馈修订（build_g，目视未确认）
- 用户目视 build_f：SUNISLE 已出现（§9 门禁通过）；新反馈：删「私人工作内容隐藏」、每日一言显示不全、字体单一全横排、要设计感。
- **每日一言截断双重根因**：`Snapshot.quote[48]` 装不下 21 汉字（63 B UTF-8）+ `label()` 全局 `LV_LABEL_LONG_DOT` 单行打点。改为 `quote[96]` + 两处 quote label 用 `LV_LABEL_LONG_WRAP` 固定文本框。日志已出现完整 17 字句。
- 排版层次：Guest 左对齐品牌 + 右上 28 px 大字时钟 + 竖排「每日一言」栏目标签（Kai 16）+ 橙色竖线 + 横排换行正文；新增 `sunisle_cn_kai_16`（54 字 / 13,824 B，含中文标点）用于标题/主文案；卡片交替缩进；页码 18 px。不新增装饰图案。
- 白名单仍 288 码点未变（被删文案仍留在 B_UI_STRINGS 仅为保持覆盖）。
- `build_g` + `CODEBUDDY_SAFE_DELETE_ENABLED=0`，SUCCESS 85.07 s；**固件 1,294,768 B / app0 98.78% / 余 15,952 B**；app0 备份 `esp32_app0_backup_20261108_pre_typo2.bin`；烧录 Hash verified；串口与六页轮播全通过，停在 GUEST。**目视未确认**。
- 详见《B方案_阶段C_改造说明.md》§10。

## 2026-11-08 08:20 — UI 2.0 视觉方案交付（待用户确认，未改代码）
- 用户给出《晴屿 QINGYU UI 2.0 重构任务书》+ ChatGPT 正式设计交付稿，要求「先完成设计、先看四张 320×240 设计图，确认后才改 LVGL 代码」。本轮严格按此执行：**未修改 `B_QINGYU/` 任何产品代码、未编译、未烧录**。
- 新交付目录 `UI2_DESIGN/`：`晴屿_UI2.0_视觉设计规范.md`、`01_boot_320x240.png` / `02_home_320x240.png` / `03_memos_320x240.png` / `04_guest_320x240.png`（均实测 320×240）、`05_交互流程图.png`（1000×640）、`index.html` 预览、`render_ui2.py` 绘图脚本。
- 页面结构：6 页 → 4 画面（BOOT / HOME / MEMOS 合并页 / GUEST 覆盖状态）；正常工作仅 2 个可导航页面；删 TASKS/MEMOS/REMINDERS 三页导航。
- 色系：奶油米黄 → `#F4F6FA` 现代浅色科技体系；GUEST 深石墨 `#171D2B` 为唯一例外。已算 RGB565 量化值。
- 字体审计：现有 5 面 8bpp CJK 共 148,640 B 位图 / 288 码点白名单；UI 2.0 静态文案 85 字中缺 11 字（伴侣动后启完情桌确详面），补入 14/16 两档需 4,972 B；新增 34 px 数字子集约 16,184 B。
- 容量：当前 build_g 1,294,768 B / 98.78% / 余 15,952 B；实施前必须先瘦身（缩 16 px 字集省 30–40 KB）。
- **触摸门禁不变**：白屏根因仍是 XPT2046 共享 HSPI + LVGL indev；设计完整定义滑动/轻触/长按，落地先走串口/Bridge 等价映射，不得声称触摸可用。
- 下一步：等用户确认视觉方案与 §11 的 D1–D7 决策，再按规范第十节增量改代码。

## 2026-11-08 08:36 — UI 2.0 已实施并烧录（用户「干吧」解除门禁）
- D1–D7 全部按规范建议采纳：BOOT 1.5 s、GUEST 深石墨、触摸维持禁用、时间数字用 INK、执行瘦身、TITLE 复用 16 px。**设备当前烧录 UI 2.0，停在 HOME（OWNER / DEMO on）**。
- 字库：补 11 缺字（14 px 面 300 字，仍是 sanitizeDynamic 白名单来源）；`sunisle_cn_16` 288→64 字静态集；删 `sunisle_cn_boot`；新增 `sunisle_num_34`（Segoe UI Light、15 字形、比例 advance，生成器新增 `allow_ascii`/`adv_map`）；关 `LV_FONT_MONTSERRAT_18/28`。**所有动态 label 必须绑 14 px 面**，16 px 面只承载静态标题/按钮，否则瘦身后动态文案会缺字。
- UI 层：四画面 BOOT/HOME/MEMOS/GUEST；删 TASKS/REMINDERS；`Page` = HOME/MEMOS/GUEST/BOOT；新增 `qy_ui_action()`、`setBootProgress()`、`setStatus()`、`setWork()`。200 ms 淡入 + BOOT 单次线性进度推进。
- 命令：`PAGE=home|memos|guest|boot`、`DEMO=on|off`（默认 ON，仅 Owner 两页示意数据、顶栏标 DEMO；OFF 为真实空态）、`ACT=ack|later|detail|task`。上电仍强制 BOOT→GUEST，私有 setter 在 Guest/Boot 丢弃文本；GPIO/SPI/旋转/触摸禁用全不动。
- 编译 SUCCESS 69.19 s：**1,202,117 B / 91.70% / 余 108,603 B**（比 build_g 省 92,651 B，阶段 D ≥40 KB 已满足）。备份 `B_QINGYU_pre_UI2_snapshot.tar.gz` + `esp32_app0_backup_20261108_pre_UI2.bin`；烧录 `0x10000` Hash verified；串口 `esp32_ui2_verify_log.txt` 全通过（BOOT→GUEST、NTP、Wi-Fi、天气、页面 0/1、ACT、DEMO、heap 稳定）。
- **真机目视未确认**：34 px 数字面首次上板、中文无豆腐块、卡片/按钮/分页点无重叠、深石墨 GUEST 对比。串口日志不能代替看屏。
- 报告：`UI2_DESIGN/晴屿_UI2.0_实施与烧录报告.md`。未推送 GitHub、未做阶段 D。

## 2026-11-08 08:47 — v2：字体基线修复 + 规格复核（用户反馈「所有字体歪歪扭扭」）
- **根因**：`tools/gen_cn_font.py::render_glyph()` 用 `font.getbbox(ch)` 把每个字按自身墨迹框居中进 `size×size`，等于每字一条基线 →「一，。:1」飘到格子中央；34 px 面 `box_w=34` 却给 `:` 设 12 px advance，位图与步进不匹配；自绘面 `line_height/base_line` 与内置 Montserrat 不一致，混排再错一层。与屏幕 / SPI / 旋转无关。
- **修复（以后改字库的硬规则，禁止再回到逐字居中）**：改 Pillow `anchor="ls"` 统一基线 + 真实 ink bbox，输出 `box_w/box_h/ofs_x/ofs_y`，`ofs_y=-y1`；`adv_w` 用 `font.getlength()`，删 `NUM_ADV`；行度量对齐 Montserrat（14:16/3、16:18/3、28:31/5、34:30/3）；新增位域上限断言（`LV_FONT_FMT_TXT_LARGE=0`：adv<4096、box<256、ofs int8、bitmap_index<1 MB）。
- 规格复核：BOOT 精简为「晴屿」+「AI 桌面工作伴侣」+ 品牌蓝细线 + 进度（删 `QINGYU`/`正在启动`/`2.0`）；顶栏状态改 `USB ON/OFF`；A/B/C/D 逐条见报告 §9.3。设计图 `01_boot` 与 `index.html` 已同步。
- 编译 71 s SUCCESS：**1,182,537 B / 90.2% / 余 128,183 B**；`0x10000` Hash verified；串口 v2 全通过，无 `glyph dsc. not found`。设备停在 HOME（OWNER / DEMO on）。
- 对比图 `UI2_DESIGN/font_baseline_fix.png`、脚本 `UI2_DESIGN/verify_font_fix.py`、报告 §9。**09:09 用户目视「目前尚可」→ 字体基线修复通过真机确认**。

## 2026-11-08 09:27 — 触摸诊断（用户「触摸屏不管用」）
- 重建安全触摸层：**不注册 LVGL indev / 不 `spi::init()` / 不 `setTouch()` / 开机不碰触摸 GPIO**，只在 `TOUCH=on` 后 1 MHz 共享 HSPI 低频事务自读 XPT2046，按下→抬起经回调映射 320×240 并分派与 `ACT=`/`NEXT` 相同的动作；GUEST 忽略 tap。新增 `TOUCH/TOUCHPROBE/TOUCHSCAN/TOUCHCS/TOUCHMAP/I2CPROBE` 命令。
- 三轮实测：SPI 8 个 CS（33/32/25/26/27/16/17/4）+ 1750 帧采样 MISO 恒 `00 00 00`；I²C 7 组引脚全地址扫描 `devices=0`。**结论：板上无可用触摸数据通路，软件无法单独启用触摸**，需硬件接线信息。详见 `晴屿_触摸硬件诊断报告.md` 与 HARDWARE_SPEC §4.4。
- 三个已修的坑：旧层 `pin_int=36` 导致 `getTouchRaw()` 提前返回、GPIO34 只输入不能当 CS、`lgfx::spi::readBytes()` 需显式设 `SPI_MISO_DLEN_REG`。
- 编译 1,187,345 B / 90.6%，`0x10000` Hash verified；全程显示稳定、无白屏。设备当前 `TOUCH=OFF`、HOME（OWNER/DEMO on）。回滚 `esp32_app0_backup_20261108_pre_touch.bin`、快照 `B_QINGYU_pre_touch_snapshot.tar.gz`。

## 2026-11-08 09:55 — GUEST 2.1 多巴胺版 + 触摸第四/五轮
- 用户否决深石墨 GUEST（「布局不行、要放大时间和提示、要点多巴胺色彩」），并确认**按压过屏幕、板屏一体化、触摸确定可用**（§4.4 的「是否真的按压」疑问就此结案，不得再当作原因）。
- GUEST 重做为纵向四段高饱和撞色：紫 `#7C4DFF` 品牌条 / 黄 `#FFD93D` 时间卡 / 粉 `#FF4FA3` 欢迎卡 / 薄荷绿 `#2ED9A8` 每日一言条，暖奶油 `#FFF6E0` 底 + 蓝/橙装饰圆。时间 34→**56 px**（新增 `sunisle_num_56`，H=41/BL=3，12,798 B），主提示 16→**28 px 楷**「欢迎来访」，副提示/日期 14→16 px；删英文锁行。HOME/MEMOS/BOOT 未动，隐私逻辑/GPIO/SPI/旋转/串口协议未动。
- 16 px 面补 GUEST 静态字 +「未校时」（81 字 / 18,944 B），修掉首版真机 `glyph dsc. not found for U+672A/U+6821`；14 px 面 303 字白名单不变。设计图 `04_guest_320x240.png` 与 `index.html` 同步重渲染。
- 触摸新增四条独立路径：`TOUCHSPI`（Arduino `SPIClass(HSPI)` 全双工 `transfer()`）、`TOUCHMISO`（`spiAttachMISO()` 经 GPIO matrix 换 MISO 脚）、`TOUCHMATRIX`（14 MISO × 9 CS = 117 组合 ×4 读）、`TOUCHIRQ`（只读 GPIO 电平 + 同窗口 SPI 采样）。全部不注册 indev、不碰 LCD_CS/DC/SD_CS/BL。
- 结果：全组合只出现 `00 00 00` / `FF FF FF`（浮空/上拉），`D1 00 00` 是 MOSI 回环特征，**无任何 12 位 ADC 值**；15 s 按压窗口 `spi33 frames=150 changes=1`，GPIO16/39 高频互补翻转判为噪声非 PENIRQ。软件层完备，但仍取不到坐标 → 缺的是**板子型号/引脚定义**，请用户提供背面丝印或控制器 CS/DOUT/IRQ 接线。首版 MISO 候选误含 SD 的 VSPI 19/23 导致 `sdCommand(): Card Failed`，已剔除并验证 `SD ready=YES`。
- 最终固件 **1,228,045 B / 93.7%，余 82,675 B**，编译 50 s，`0x10000` Hash verified，开机零缺字警告，干净重启后停在 **GUEST**。回滚 `esp32_app0_backup_20261108_pre_guest21.bin` + `B_QINGYU_pre_guest21_snapshot.tar.gz`。交付 `UI2_DESIGN/晴屿_GUEST2.1_多巴胺版交付说明.md`。GitHub 推送、阶段 D 仍未做。

## 2026-11-08 10:20–10:40 · UI 2.2/2.3 简约中式收口
- 多巴胺 GUEST 被否，全站回到单一「宣纸 / 墨 / 朱砂 / 青瓷」token：`#F5F3ED` 底、`#FFFFFF` 卡面、`#222A27` 墨、`#717A74` 淡墨、`#B23A2E` 朱砂（唯一强调色）、`#6E8B7E` 青瓷、`#E2DFD6` 细线。8 px 栅格 / 16 px 页边距（内容 288）/ 1 px 描边卡片 / 右侧状态对齐。
- 连续三轮删冗余文案：「现在时间」「每日一言」→「访客模式」→「欢迎来访」。判据固化进规范：**一句话若只是复述画面已表达的信息，就不写**。GUEST 不再自我声明是访客模式。
- 删 28 px 楷行后主视觉重排（非留白）：时间 `TOP_MID y=68`（ink 69..109）、日期 y=116、朱砂线 136,144,48,2、提示 y=156（ink 156..172）、引文卡 16,204,288,30。可用区 34..204 内上留 35 / 下留 32，重心落几何中线。
- 设计图 `04_guest_320x240.png` 逐像素与固件对齐（细线实测 x=136..184，中心 160）；`index.html` 文案同步。
- 编译 **1,228,481 B / 93.7%（余 82,239 B）**，RAM 97,556 B / 29.8%，`0x10000` Hash verified；串口零缺字警告、零 LVGL 报错、BOOT 1500 ms → GUEST、NTP 同步、真实天气正常。回滚 `esp32_app0_backup_20261108_pre_cn23.bin`。
