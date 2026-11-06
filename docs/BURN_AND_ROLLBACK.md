# 烧录、使用与失败回滚

## 前置

- 板子：ESP32-D0WD-V3 / 4MB Flash / 无 PSRAM / COM5（CH340）
- 已冻结参数见 `HARDWARE_CONFIG.md`；**不要**改 ST7789 / HSPI 引脚 / 20MHz / `swap=true`
- 整片备份：`D:/ESP32/backup/full_flash_20261106.bin`
  （SHA256 `28cc552e78e33db41fa58816cc28e2d04b01cb1458366f021a2e6750809acb5f`）
- 旧工程完整备份：`D:/ESP32/backup/atlas-moments_pre_sunisle_20261106/`
  （含 `BACKUP_MANIFEST_SHA256.txt`）

## 编译

```bash
cd D:/ESP32/sunisle
python -m platformio run -e esp32dev
```

产物：`.pio/build/esp32dev/firmware.bin`。分区表与板上原厂布局完全一致。

## 烧录（只写 app0）

```bash
python -m esptool --port COM5 --baud 921600 write_flash \
  --flash_mode dout --flash_freq 40m --flash_size 4MB \
  0x10000 .pio/build/esp32dev/firmware.bin
```

必须看到 `Hash of data verified`。
**禁止** `erase_flash`、禁止写 `0x8000`（分区表）、禁止写 `0x150000`（app1）。

## 首次使用

1. 通电 → 奶油白品牌启动页渐显 1.8s → 自动进入 Home。
2. Home：照片区为明确标注的空状态插画；时间 `--:--`（未校时）；天气「天气未配置」。
   **这三项都是阶段 1 的诚实状态，不是故障**。
3. 串口 115200 输入 `HELP` 查看命令；`STATE` / `MEM` / `SD` 查看状态。
4. 把真实 JPEG 放进 SD 的 `/SUNISLE/photos/`（需读卡器），再发 `RESCAN`，
   `STATE` 会显示真实文件数；阶段 2 才会真正解码显示。
5. 用读卡器编辑 `/SUNISLE/config/settings.json` 填城市与经纬度（阶段 3 才会取天气）。

## 回滚

| 场景 | 操作 |
| --- | --- |
| SUNISLE 固件异常 / 白屏 | `esptool write_flash 0x10000 <旧工程 v9 固件>` 回退到上一版可运行固件 |
| 需要完全恢复出厂 | `esptool write_flash 0x0 D:/ESP32/backup/full_flash_20261106.bin` |
| 源码误改 | 从 `D:/ESP32/backup/atlas-moments_pre_sunisle_20261106/` 按清单校验恢复 |
| 分区表被误改（本工程不会做） | 整片回写备份 |
| SD 目录异常 | 固件只重建缺失目录；**删除 `/SUNISLE/` 须由用户明确确认后手动处理** |

## 白屏 / 花屏排障

1. 串口发 `DIAG` → 显示纯 LovyanGFX 彩色自检图（绕过 LVGL）。
   - 自检图正常 → 问题在 LVGL / UI 层。
   - 自检图也异常 → 问题在 SPI / 面板 / 背光。
2. `UI` 恢复 LVGL 并全屏重绘。
3. `STATE` 与 `MEM` 检查是否崩溃 / 内存耗尽。
4. 历史根因提醒：**不要**启用共享 HSPI 的 bit-bang 触摸（会白屏）；
   不要改 20MHz / `swap=true` / ST7789。

## 数据保护

- 只写 `app0`；不动 NVS 之外的用户分区。
- SD 上只在 `/SUNISLE/` 下新增，不删除、不覆盖已有文件；照片清理默认软删除。
- `include/secrets.h` 为空模板且 gitignored；编译镜像会内嵌填入的凭据，
  分享工程前请扫描二进制，勿分享 `.pio/build/` 与 `.local/`。
