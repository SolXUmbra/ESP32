# SD 卡目录与文件说明

根目录统一为 `/SUNISLE/`。首次启动由 `storage_manager.cpp::ensureDirectories()` 自动创建。
**只允许在该命名空间下新增内容，绝不删除或覆盖卡上已有的任何文件。**

```
/SUNISLE/
├─ config/
│   ├─ device.json       设备标识 / 固件版本 / schema
│   └─ settings.json     亮度、轮播间隔、城市、经纬度、时区、同步计划、裁切策略
├─ photos/
│   ├─ index.json        照片索引（阶段 2/4）
│   ├─ thumbnails/       服务端建议提供 240×320 适配版本
│   ├─ .trash/           阶段 4 软删除回收站（不直接 remove）
│   └─ *.jpg / *.jpeg    真实照片
├─ weather/
│   └─ cache.json        最后一次**成功获取的真实**天气 + 时间戳
└─ system/
    ├─ sync.json         上次同步结果
    └─ sync.log          追加式日志（只 append，不 truncate）
```

## 当前各文件状态（阶段 1）

| 文件 | 状态 | 内容 |
| --- | --- | --- |
| `config/device.json` | 首次启动自动创建（已存在则不动） | `{"product":"SUNISLE","firmware":"...","schema":1}` |
| `config/settings.json` | 首次启动自动创建（已存在则不动） | 亮度 220、轮播 15000ms、城市留空、经纬度留空、时区 CST-8 |
| `photos/index.json` | **尚未创建**（阶段 2） | — |
| `weather/cache.json` | **尚未创建**（阶段 3） | 城市未配置，固件不发起任何网络请求 |
| `system/sync.json` | 首次启动自动创建 | `{"last_sync":0,"result":"NEVER"}` |
| `system/sync.log` | 追加式，按需创建 | `[millis] [tag] message` |

> `settings.json` 的 `city / lat / lon` 由用户在阶段 3 填写。
> 本阶段修改亮度 / 间隔时会**读回并原样保留** `lat / lon / timezone` 等本阶段不管理的字段，
> 不会把它们清空。

## index.json 字段契约（阶段 2/4 实现）

至少记录：远程照片 ID、本地文件名、文件大小、校验信息（sha256）、
远程更新时间、下载时间、照片显示状态。清单结构见 `NAS_API_ADAPTER.md §3`。

## 写入纪律

1. `writeIfAbsent()`：文件已存在则直接返回，**不覆盖**。
2. `atomicReplace()`：仅允许 `/SUNISLE/` 下的托管文件；写 `*.tmp` → 校验大小 →
   替换 → 重命名。校验失败保留原文件。
3. 照片下载（阶段 4）同样用 `.tmp` + sha256 校验 + 原子替换；下载中断不破坏已有照片。
4. 预留空间 `SD_MIN_FREE_BYTES = 64 MiB`，不足时停止下载。
5. 索引损坏 → 重新扫描 `photos/` 重建，不删文件。
6. SD 缺失 / 挂载失败 → 降级运行（UI、时间照常），可用串口 `RESCAN` 触发重挂载。

## 照片清理（阶段 4，任务书强制）

必须**同时**满足：① 服务端返回完整清单 ② 清单格式有效 ③ 同步任务确认成功
④ 照片确实已不在收藏列表。远程异常 / 认证失效 / 空清单 / 解析失败 → **禁止批量删除**。
实现优先移入 `.trash/` 软删除并保留索引标记，可回滚。

## 往卡里放照片（PC 无读卡器）

设备是唯一的写卡者：`python tools/upload_photo.py <图片> --name 01_xxx.jpg`
（`PUT` 协议 + cover 裁切 + 1/2 缩放解码）。协议、失败码、已踩的 RX 溢出坑
见 `docs/PHOTO_UPLOAD.md`。上传脚本只新增、**从不覆盖**已存在的照片。
