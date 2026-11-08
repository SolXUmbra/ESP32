# 照片上传（PC 无读卡器时的唯一通路）

SD 卡在设备里，电脑上没有读卡器，所以**由设备自己写卡**：固件暴露一个 `PUT`
命令，主机把原始字节流交给它，设备写入 `/SUNISLE/photos/`、校验大小、重新扫描并立刻解码显示。

## 一条命令搞定

```bash
cd D:/ESP32/sunisle
python tools/upload_photo.py "C:\path\to\photo.png" --name 01_together.jpg
```

脚本会：

1. 读图 → **cover 裁到 216:164**（与 `ui_sunisle.cpp` 的 `PHOTO_W/PHOTO_H` 一致），
   纵向默认 `--focus 0.55`（略偏上，人像不会切掉下巴）；
2. 放大到 **432×328** 存为 JPEG（q88、4:4:4、非渐进），
   这样 TJpgD 用 1/2 IDCT 直接落到 216×164 —— **原始大图从不整帧进 RAM**；
3. 串口 `PUT` 上传（1 KB 分块），等设备回 `[put] OK`。

可选参数：`--port COM5`、`--focus 0..1`、`--name`。

## 协议（固件侧实现）

```text
host -> PUT /SUNISLE/photos/01_together.jpg 32167\n
dev  -> [put] READY 32167 /SUNISLE/photos/01_together.jpg
host -> <恰好 32167 个原始字节>
dev  -> [put] OK 32167/32167 /SUNISLE/photos/01_together.jpg
```

安全规则（代码里硬性执行）：

| 规则 | 行为 |
| --- | --- |
| 路径必须以 `/SUNISLE/photos/` 开头 | 否则 `[put] ERR path...` |
| 目标文件已存在 | **拒绝覆盖**，`[put] ERR file exists` |
| 字节数不符 / 中途超时 | 删除半成品文件，`[put] FAIL x/y`，绝不留下"看起来像照片"的残片 |
| 写入后回读校验 | `chk.size() != expected` → 删除 |

## 已踩过的坑

**症状**：`[put] FAIL 30361/32167`，差 1806 B。

**原因**：Arduino-ESP32 默认串口 RX FIFO 只有 256 B，上传时 core-0 的网络任务
同时在打印（`Wi-Fi up` / `NTP` / `weather`），主机全速灌数据导致溢出丢字节。

**修复**（已固化）：

- `setup()` 里 `Serial.setRxBufferSize(8192)` **在 `Serial.begin()` 之前**调用；
- 主机侧 1 KB 分块 + 4 ms 间隔；
- 建议等 Wi-Fi 窗口结束（开机约 20 s 后）再传，减少总线与 CPU 竞争。

## 照片规格建议

- 格式只有 **JPEG**（`.jpg` / `.jpeg`）。PNG 请先转（上传脚本会自动转）。
- 建议**横构图**：画面比例越接近 216:164 ≈ 1.32:1，裁切越少。
  3:4 竖构图会被裁掉约 43% 的高度，脚本按人脸偏上的策略取景。
- 单张建议 ≤ 300 KB；`/SUNISLE/photos/` 上限 200 个文件（`MAX_PHOTO_ENTRIES`）。
- 文件名只用 ASCII（`[A-Za-z0-9_-]`），会显示在照片下方作为标题（下划线转空格，去掉扩展名）。

## 验证清单

上传后应看到：

```text
[sd] uploaded /SUNISLE/photos/xx.jpg (32167 B verified)
[put] OK 32167/32167 ...
[sd] photo scan: 1 real JPEG file(s)
[uibridge] viewport sprite 216x164 = 70848 B (no PSRAM, lazy)
[uibridge] pixels sampled=3221 nonSand=100% checksum=0674A53B
[uibridge] photo 432x328 -> zoom=500/1000 (...)
```

`nonSand` 接近 100% 才说明**像素真的画进了精灵**（而不是"解码成功但没写"）。
