# 软件架构

## A 工程（只读基线）
- 构建：PlatformIO 6.2.0 + Arduino framework，`board=esp32dev`，`platform=espressif32@6.9.0`，`framework-arduinoespressif32 3.20017.241212+sha.dcc1105b`，toolchain xtensa-esp32 8.4.0。
- 依赖锁定：`lovyan03/LovyanGFX@1.2.36`、`lvgl/lvgl@8.3.11`；`lib_ldf_mode = deep+`。
- `build_flags`：`-I include`、`CORE_DEBUG_LEVEL=1`、`LV_CONF_INCLUDE_SIMPLE=1`、`LV_LVGL_H_INCLUDE_SIMPLE=1`、`SUNISLE_STAGE=1`。
- 分区 `partitions_sunisle.csv`；`board_build.flash_mode = dout`、`f_flash = 40 MHz`、`flash_size = 4MB`。
- 目录：`src/{main.cpp,hardware/,services/,ui/}`、`include/{sunisle/,lv_conf.h,secrets.h,fonts/}`、`tools/{gen_cn_font.py,gen_latin_font.py,verify_fonts.py}`、`lib/`、`partitions_sunisle.csv`。
- `lv_conf.h`（`1ab7ea3`）：关闭 18 个 widget（ARC/METER/CALENDAR/KEYBOARD/MENU/TABVIEW/TILEVIEW/WIN/DROPDOWN/ROLLER/SPINBOX/SPINNER/SWITCH/CHECKBOX/BTNMATRIX/LIST/LED/COLORWHEEL）；`BTN`/`CONT`/`LABEL`/`IMG`/`LINE` 仍为 1 → `qingyu_ui.c` 的 `lv_btn_create` 可用。
- 字体：中文 5 档共 227 字（12/16/quote/28/boot），4bpp；拉丁 6 个 Montserrat 子集（去掉 FontAwesome 与 kerning，符号名不变以覆盖库成员）；`tools/verify_fonts.py` 逐字校验 11 个字体，本机实测 `ALL CHECKS PASSED`。
- 运行：`loop()` 内 `lv_timer_handler()` + `ui::tickFrame()`，1 s UI tick、10 s 心跳、`delay(5)`。
- 容量：远端 `1ab7ea3` 提交记录固件 1,290,809 → **1,131,209 B（86.3%，约 175 KB 余量）**；**本机复编译实测（2026-11-08，`B_QINGYU/` 零源码修改）：`[SUCCESS] 118.36 s`，`firmware.bin = 1,137,776 B`，app0 86.81%，余 172,944 B**。与远端数字差 6,567 B，属本机工具链/库路径差异，不是功能差异。
- **该固件真机白屏（用户目视），已回滚；尺寸数据只说明编译通过，不能说明可运行。B 基线改用已目视验证的 `9779c6e`**（见 `HARDWARE_SPEC.md §4.1`）。

## B 方案目标分层
```
WorkBuddy（自然语言 / Skill / 定时自动化）
        ↓ MCP tools（本地 stdio，G5 已放行）
Qingyu MCP Server（Python，与 Bridge 同项目、模块化）
        ↓
Windows Bridge：串口管理 / SQLite / 消息队列 / 定时提醒 / 同步状态 / 设备监控 / 日志
        ↓ USB 串口 UTF-8 JSON Lines（≤2 KB/条）
ESP32：LVGL 六页 UI / 触摸 / SD 缓存 / 本地提醒
```
ESP32 不跑 LLM；首期不以 Wi-Fi 作主通信链路。

## B 交付包组件（`qingyu_ui`）
- ESP-IDF `components/qingyu_ui/` + `idf_component_register(SRCS "qingyu_ui.c" INCLUDE_DIRS "." REQUIRES lvgl)` → **不能直接放进 PlatformIO 工程**，需把 `.c/.h` 纳入源树或封装为本地 library。
- 12 个 API：`qy_ui_init/set_privacy/get_privacy/set_dashboard/set_tasks/set_memos/set_reminders/show_boot/show_page` + 3 个回调 typedef + 4 个数据结构。
- 已确认缺陷：`build_tasks()` 的 `ys[]` 只有 3 元素却循环 5 次（越界）；全部 label 用 Montserrat 占位；`set_privacy(GUEST)` 未清 `home_greeting/date/counts/page_title` 与静态 `counts[96]`/`summary[160]`；`set_privacy(OWNER)` 无授权校验；`on_task_action`/`on_reminder_ack` 未绑定；无左右滑动；`show_boot()` 无启动页；`qy_ui_init()` 直接在 `lv_scr_act()` 建对象（与 A 冲突）；`style_page()` 硬编码 320×240。
- 中文文案对现有 227 字子集缺 46 字（个主件任位办务场备始完客工式录待忘想提摘早智暂条查检模法球确离稍篮终能让记设访请迎醒重页项首）→ 文案定稿后必须重新生成子集。

## 隔离原则
A 与 B 屏幕方向、产品文案、串口协议、LVGL 对象树都不同；`B_QINGYU/` 独立目录，共享的只有冻结硬件参数文档与字库生成工具思路。

## 字库与容量约束（2026-11-08 07:14）
- `1ab7ea3` 的 4bpp CJK + `sunisle_latin` 同名符号子集在真机上被目视发现缺字；`9779c6e` 的 8bpp CJK + LVGL 内置 Montserrat 目视正常。
- `gen_cn_font.py` 的 SPECS 字符集在 `9779c6e..1ab7ea3` **未变**（diff 仅 8bpp→4bpp 打包）；`BASE_CPS` 覆盖全部可打印 ASCII + `°`，源码无 `LV_SYMBOL` → 缺字原因未定位，**不得假定是字符集缺失**。
- `verify_fonts.py ALL CHECKS PASSED` 只校验生成字节与 stock 字体自洽，不覆盖真机渲染。
- 阶段 C **不复用 4bpp CJK / 拉丁子集**；字库按 B 六页文案重生成 + 真机目视。瘦身只复用 `lv_conf.h` 关闭未用 widget（与字体无关），并按六页实际需要重开。
- 容量账：`9779c6e` 基线 1,284,752 B（app0 98.02%，余 25,968 B）；横屏驱动层 1,284,736 B（余 25,984 B，未烧录）。去照片 + 关 widget + 字库重生成必须一起算，六页 UI 才能放下。

## 阶段 C 实测（2026-11-08 07:33）
- 字库：`sunisle_cn_12` 288 字 / 41,472 B、`sunisle_cn_16` 81 字 / 20,736 B，8bpp；`include/fonts/sunisle_cn_chars.h` 输出 288 码点白名单，`ui_bridge.cpp::sanitizeDynamic()` 逐码点降级超集汉字为 `?`，不缓存。
- `qingyu_ui` 移植为 C++（`src/ui/qingyu_ui.cpp`）以匹配生成字库的链接；头文件保留 `qy_ui_set_dynamic_bitmap()` TODO（阶段 D Bridge 栅格化）。
- 隐私：BOOT → GUEST 为默认；私有 setter 在 Guest/BOOT 直接丢弃，`PRIVACY=OWNER` 仅本地调试并在日志明确标注非 Bridge 验证会话。
- 容量账：9779c6e 1,284,752 B → 阶段 C 1,164,928 B（净减 119,824 B）。
- 编译坑：`LV_USE_IMG=0` 破坏 `lv_canvas.h`/`lv_animimg.h`（缺 `lv_img_t`）；`LV_USE_ARC=0` 必须同时 `LV_USE_SPINNER=0`；采用上游 1ab7ea3 已验证的 widget 组合。

## 字体与容量（07:54 修订，替代 07:33 数据）
- CJK 字库：`sunisle_cn_14` 288 字 / 56,448 B + `sunisle_cn_16` 288 字 / 73,728 B，均 8bpp、均覆盖全量 288 字（避免换字号就缺字）；`sunisle_cn_28` Kai 品牌、`sunisle_cn_boot` Kai 标语不变。`sunisle_cn_12` 已删除。
- `include/fonts/sunisle_cn_chars.h` 白名单 288 码点（取自 14 px），`sanitizeDynamic()` 逻辑不变。
- 容量账：9779c6e 1,284,752 B → 阶段 C 首版 1,164,928 B → 字号/品牌修订 **1,235,829 B（94.3%，余 74,891 B）**。阶段 D 代码需精打细算，必要时把 16 px 字库缩回「仅主文案字集」可省约 50 KB。
- `ui_bridge.cpp` 开机日志仍写死 `12/16px 8bpp CJK`，纯文案过时，阶段 D 编译时改。

## 容量硬约束（2026-11-08 08:03，阶段 D 阻塞项）
- 阶段 C 第二版固件 **1,294,768 B / app0 98.78% / 余仅 15,952 B**（build_f 1,235,829 B → +58,939 B）。增量：Montserrat 28 约 25 KB、`sunisle_cn_kai_16` 13,824 B 位图、字库数组与布局代码。
- 阶段 D 的 Bridge/JSONL 解析放不下，**开工前必须先瘦身**。候选见《B方案_阶段C_改造说明.md》§10.5：优先缩 `sunisle_cn_16` 到实际 16 px 用字（预计省 30–40 KB）；次选自制仅数字/冒号的 28 px Latin 子集；扩分区表暂不推荐。
- 字库：`sunisle_cn_14` 288 字 / 56,448 B、`sunisle_cn_16` 288 字 / 73,728 B、`sunisle_cn_28` Kai 2 字、`sunisle_cn_boot` Kai 12 字、`sunisle_cn_kai_16` Kai 54 字 / 13,824 B，全部 8bpp。`sunisle_cn_chars.h` 白名单 288 码点不变。
- `ui_bridge.cpp` 开机日志已改为 `14/16px 8bpp CJK + Kai 16`。
