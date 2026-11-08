# HARDWARE_CONFIG.md — 晴屿 SUNISLE 硬件配置（冻结）

> **本文件是 SUNISLE 工程引脚与显示参数的唯一真源。**
> 所有数值来自 `D:/ESP32/atlas-moments/HARDWARE_REPORT.md §10` 与 16 张编号测试卡
> 的**用户目视实测**（03 / 07 / 16 清晰，其余花屏），不是网上 CYD 引脚表。
> 任务书第 2 节要求「不得擅自修改已经验证的参数」→ 以下参数**一律冻结**。
> 代码对应位置：`include/sunisle/config.h` 与 `src/hardware/display_manager.cpp`。

---

## 1. 主控与存储

| 项 | 实测值 | 来源 |
| --- | --- | --- |
| MCU | **ESP32-D0WD-V3 rev 3.1**，双核 240MHz，经典 ESP32（**不是 S3**） | `esptool chip-id` / `esp_chip_info` |
| Flash | **4 MB**（0xc4 / 3.3V） | `esptool flash-id` |
| **PSRAM** | **无 / 0 bytes** | `ESP.getPsramSize()` 实机返回 0 |
| Secure Boot / Flash 加密 | 均未启用 | `espefuse summary` |
| 串口 | COM5，CH340（VID_1A86 / PID_7523），115200 8N1 | Windows PnP |
| MicroSD | 约 **7851 MB**，FAT，`CS=GPIO5` | `SD.begin(5)` 实机 |
| 蓝牙 | 硅片支持 BR/EDR，但**本阶段不使用**（任务书明令不做蓝牙） | `esp_chip_info` |

## 2. 屏幕（冻结）

| 参数 | 值 |
| --- | --- |
| 尺寸 / 分辨率 | **2.8 英寸 / 240×320 竖屏**，rotation=0 |
| 驱动芯片 | **ST7789**（`lgfx::Panel_ST7789`） |
| 显示库 | **LovyanGFX 1.2.36**（见 `AUDIT_REPORT.md §8-1`，不使用 TFT_eSPI） |
| SPI 主机 | **HSPI**（`spi_host = HSPI_HOST`） |
| SCLK | **GPIO14** |
| MOSI | **GPIO13** |
| MISO | **GPIO12**（LCD 未接 MISO，ID 寄存器读不回，属正常） |
| DC | **GPIO2** |
| CS | **GPIO15** |
| RST | **-1（未接）** |
| SPI mode | 0 |
| `spi_3wire` | false |
| `use_lock` | true |
| `dma_channel` | 1 |
| **写时钟** | **20 MHz**（10 MHz 亦实测正常；40 MHz **未验证，禁止使用**） |
| 读时钟 | 10 MHz |
| `panel_width/height` | 240 / 320 |
| `offset_x / offset_y` | **0 / 0** |
| `invert` | **false** |
| `rgb_order` | **false**（RGB，非 BGR） |
| LVGL 刷屏字节序 | **`writePixels(..., swap=true)`**，`LV_COLOR_16_SWAP=0` |
| LVGL 色深 | 16 位 RGB565 |
| LVGL 部分缓冲 | `240 × 12` 像素 = **5760 B**（阶段 1 实测可再调） |

> **不得更改的四个高危项**：驱动 ST7789、HSPI 引脚组、20MHz、`swap=true`。
> 历史上把它们改成 ILI9341 / 40MHz / `swap=false` 分别导致花屏；把 14/13/12 改成
> bit-bang GPIO 导致白屏。任何改动都必须重跑 16 卡面板扫描并重新目视确认。

## 3. 背光（从旧工程确认，任务书明确要求）

| 参数 | 最终实际使用值 |
| --- | --- |
| GPIO | **21**（`kVerifiedLcdCandidate = 1`） |
| 备选 GPIO | 27（测试卡 07 亦清晰；硬件近似常亮，**不采用**） |
| PWM | `ledcSetup(channel=0, freq=5000 Hz, resolution=8 bit)` |
| 默认亮度 | **220 / 255** |
| 接口 | `backlight::set(uint8_t)` → `ledcWrite(0, value)` |

> 旧工程 `src/hal/display.cpp:50-55` 是唯一依据，未猜测。
> 设置页的「亮度」项直接调这个接口。

## 4. SD 卡（VSPI，与 LCD 的 HSPI 隔离）

| 参数 | 值 |
| --- | --- |
| CS | **GPIO5** |
| 总线 | **VSPI**（Arduino `SD` 默认 18/23/19） |
| 初始化 | `SD.begin(5)`；旧工程只做存在性探测，**读写能力待阶段 1 实测** |
| 频率 | 首次挂载使用保守速率，实测后再调整 |
| 目录根 | `/SUNISLE/` |
| 冲突 | LCD 用 HSPI(14/13/12)，SD 用 VSPI(18/23/19)，**不同主机，无总线冲突** |

⚠️ **SD 卡是用户自己的数据载体**：本工程只在 `/SUNISLE/` 命名空间下**新增**文件与目录，
不删除、不覆盖任何已有文件。照片清理必须满足任务书的四重条件，且默认软删除。

## 5. 触摸（XPT2046，实测可用：bit-bang + 压力判定）

| 项 | 值 |
| --- | --- |
| 板型 | **ESP32-2432S028R「CYD2USB」双 USB 版**（micro-USB + USB-C，ST7789 面板），背面丝印 `ESP32-2432S028`，官方标注 `TP CLK:IO25 / TP CS:IO33 / TP DIN:IO32 / TP DOUT:IO39 / TP IRQ:IO36` |
| 控制器 | **XPT2046**（电阻屏）。实机原始读数：空闲 `z=0..7`、按住 `z=1200..2500` |
| 独立总线 | `clk=GPIO25, din=GPIO32, dout=GPIO39, cs=GPIO33`（与 LCD 的 14/13/12、SD 的 18/19/23 均不共享） |
| 传输方式 | **软件 bit-bang（必须，不是偏好）**，见下方「为什么不能用硬件 SPI」 |
| 按下判定 | **不用 PENIRQ**（GPIO36 在本版不可信）。`z = Z1 + 4095 - Z2`，`z>400` 判按下、`z<250` 判抬起（迟滞），松手才发 tap |
| 通道命令 | X=`0x91`、Y=`0xD1`、Z1=`0xB1`、Z2=`0xC1`（即 0x90/0xD0/0xB0/0xC0 置 PD=01 保持基准）。**写反通道或写错 Z 公式，全部真实读数都会像悬浮噪声** |
| 总线失效特征 | 若四个通道同时读到 `4095`（0xFFF），说明 DOUT 悬空、总线上没有芯片在应答。此时**必须判 z=0**，否则 4095 会被当成最大压力 → 永久「按下」→ 一个 tap 都发不出 |
| 坐标标定 | 四点标定存 NVS（`qytouch`/`cal`）；实测值 `x 235..3633`、`y 445..3663`，无轴互换。`TOUCHCAL` 重标、`TOUCHSETCAL=x0,x1,y0,y1[,swap]` 注入 |
| 与 LVGL 关系 | **不注册 LVGL indev**，仅通过回调上报 tap（避免 `B方案_白屏二分记录.md` 中的白屏路径） |
| 权限门控 | **锁屏状态镜像 Windows 桌面**（见下节）：Windows 锁屏 → GUEST（触摸全部丢弃），Windows 解锁 → HOME/OWNER（触摸立即可用）。设备本地不再自行上锁/解锁：无长按手势、无闲置自动锁。串口 `SESSION=lock|unlock` 是 Bridge 接入前的等价入口 |
| 调试命令 | `TOUCH` / `TOUCH=on|off` / `TOUCHMODE=hw|bb` / `TOUCHDBG[=ms]`（主循环视角，非阻塞）/ `TOUCHRAW[=i,p]` / `TOUCHLOG[=ms]`（独占视角，阻塞）/ `TOUCHCAL` / `TOUCHSETCAL` / `TOUCHSCAN` / `TOUCHPINS=clk,mosi,miso,cs,irq` |

### 锁屏模型：镜像 Windows 桌面（2026-11-08 定案）
| Windows 状态 | 设备状态 | 触摸 |
| --- | --- | --- |
| 锁屏 | GUEST（只显示公开信息） | 全部丢弃，日志 `GUEST tap ignored (desktop locked)` |
| 解锁进桌面 | HOME / OWNER | 正常翻页与命中区动作 |
| 未知（无 Bridge 链路） | 默认 OWNER | 正常可用 |

旧实现有三处**本地**锁屏权威，全部已移除：
1. 上电默认 GUEST —— 无 Bridge 时永远无法解锁，屏幕看起来"点了毫无反应"；
2. OWNER 长按 ≥2s 回锁 GUEST；
3. OWNER 空闲 60s 自动锁。
只保留一个入口 `applySessionLock(bool windowsLocked)`（main.cpp），`SESSION=lock|unlock` 与后续 Bridge 都走它，
设备自己不再判断"用户还在不在"。

### 为什么必须用 bit-bang（2026-11-08 定案）
ESP32 只有 HSPI / VSPI 两个可用 SPI 外设，而本板**两个都已被占**：HSPI 归 LCD（LovyanGFX，14/13/12），VSPI 归 SD 卡（18/19/23/5）。

用 `SPIClass(VSPI)` 驱动触摸在「SD 不工作」时能读到正确数据（这正是 `TOUCHLOG` 曾经"看起来成功"的原因），
但一旦存储初始化，SD 库会调用 `SPI.begin()` 把 VSPI 的引脚改回 18/19/23 —— 触摸的 MISO(39) 随即悬空，
四个通道全读 `4095`，驱动锁定「一直按下」，**主循环里再也产生不了一个 tap**。

`TOUCHDBG` 是分辨这一层的关键：`TOUCHLOG` 阻塞主循环、独占总线，看到的是「好数据」；
`TOUCHDBG` 不阻塞，看到的是主循环真实视角 —— 在旧固件上是 `z=4095 恒定`，在 bit-bang 下是 `z=0..7`。

### 误判史（三次结论全错，根因都在驱动）
1.「触摸禁用」→ `readChannel` 把 CS 写死 33，`probe()`/`TOUCHCS=` 从未真正切过 CS；
2.「触摸已确认 / 板上无触摸通路」→ **X/Y 通道写反（0xD1/0x91）+ Z 公式错**，真实 ADC 值被当成噪声；
3.「硬件 SPI 能用」→ 见上，VSPI 与 SD 冲突，隔离测试通过、整机运行时全 4095。

按官方引脚 + 上游语义改正后实测（COM5）：空闲 `z=0..7`、按住 `z=1200~2500`、连续 tap 正常，
`HOME -> MEMOS` 跳转日志正常。**后续任何「触摸无反应」先跑 `TOUCHDBG=20000` 看 z：
空闲接近 0、按住 >400 即硬件与驱动都正常；若恒为 4095，就是总线被抢或引脚不对。**

## 6. 分区表（沿用原厂，不擅自修改）

`partitions_sunisle.csv` 与板上 `0x8000` 处读出的原厂布局完全一致：

| Name | Type | SubType | Offset | Size |
| --- | --- | --- | --- | --- |
| nvs | data | nvs | 0x9000 | 0x5000 |
| otadata | data | ota | 0xE000 | 0x2000 |
| **app0** | app | ota_0 | **0x10000** | **0x140000 (1310720 B)** |
| app1 | app | ota_1 | 0x150000 | 0x140000 |
| spiffs | data | spiffs | 0x290000 | 0x170000 |

- 烧录**只写 `0x10000`（app0）**，不擦整片、不动 bootloader / 分区表 / otadata / app1 / spiffs。
- 改分区表会令 `app1` 与 `spiffs` 失效，**必须事先得到用户明确确认才能做**。
- 旧工程阶段 B 固件已占 app0 的 95.2%；容量逐阶段实测见 `MEMORY_BUDGET.md`。

## 7. SD 卡目录约定（任务书第 8 节）

```
/SUNISLE/
├─ config/
│   ├─ device.json          # 设备标识、固件版本
│   └─ settings.json        # 亮度、轮播间隔、城市、天气配置、同步计划
├─ photos/
│   ├─ index.json           # 远程ID / 本地文件名 / 大小 / 校验 / 更新时间 / 下载时间 / 显示状态
│   ├─ thumbnails/          # 服务端建议提供 240×320 适配版本
│   └─ *.jpg                # 真实照片
├─ weather/
│   └─ cache.json           # 最后一次成功获取的真实天气 + 时间戳
└─ system/
    ├─ sync.json            # 同步状态
    └─ sync.log             # 追加式日志
```

规则：首次启动自动创建目录；文件不存在才写默认值（**绝不覆盖**）；
下载用 `.tmp` + 校验通过后原子替换；索引损坏可重新扫描；SD 缺失时降级运行。

## 8. 配置与机密

- `include/config.h`：非敏感编译期常量（引脚、时序、调色板、路径）。
- `include/secrets.h`：**gitignored**，Wi-Fi SSID/密码、设备 Token。
  SUNISLE 新工程**留空模板**，不复制旧工程的真实凭据，等待用户重新确认。
- 城市 / 经纬度 / 时区：任务书明确「当前地点尚未配置」，故 `settings.json` 留空，
  UI 显示「天气未配置」，**不取数、不伪造**。
