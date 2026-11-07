# SD 卡目录与文件说明（B 版）

根目录统一为 `/SUNISLE/`。首次启动由 `storage_manager.cpp::ensureDirectories()` 自动创建。
**只允许在该命名空间下新增内容，绝不删除或覆盖卡上已有的任何文件。**

A 版（相册）的布局见 `main` 分支 / `vA` 标签；B 版不再有 `photos/`、`thumbnails/` 与 `index.json`。

```
/SUNISLE/
├─ config/
│   ├─ device.json       设备标识 / 固件版本 / schema
│   ├─ settings.json     亮度、城市、经纬度、时区、同步计划
│   └─ skills.tsv        WorkBuddy 技能配置（skill.key \t value）
├─ memos/
│   ├─ memos.tsv         备忘录表（软删除回收站同在此文件）
│   ├─ memos.bak         每次改写前的上一版备份
│   └─ tasks.tsv         综合任务表
├─ weather/
│   └─ cache.json        最后一次成功获取的真实天气 + 时间戳
└─ system/
    ├─ sync.json         上次同步结果
    └─ sync.log          追加式日志（只 append，不 truncate）
```

## 表结构

`memos.tsv`（Tab 分隔，首行 `#` 为表头）：

```
id \t state \t repeat \t dueEpoch \t text
```

- `state`：0 = 生效，1 = 已完成，2 = 已删除（**软删除，行保留 = 回收站**）
- `repeat`：0 = 一次，1 = 每天，2 = 每周
- `dueEpoch`：Unix 秒；0 = 无时间便签，永不触发
- `text`：UTF-8，最多 64 字节；禁止 `\t` `\n` `\r` `|`（写入时自动替换为空格）

`tasks.tsv`：`id \t state \t step \t total \t name \t steps`，其中 `steps` 用 `;` 分隔。
`skills.tsv`：`skill.key \t value`。

## 写入纪律（数据安全铁律）

1. `writeIfAbsent()`：文件已存在则直接返回，**不覆盖**。
2. `atomicReplace()`：仅允许 `/SUNISLE/` 下的托管文件；写 `*.tmp` → 校验大小 →
   替换 → 重命名。校验失败保留原文件。
3. **备忘录删除只改 `state=2`，绝不物理删除行**；配套 `CMD|restore|id=N` 可恢复。
4. 每次改写 `memos.tsv` 前，先把旧内容完整复制到 `memos.bak`。
5. `settings.json` 的 `city / lat / lon / timezone` 不属于 B 版管理范围，
   保存时**读回并原样保留**，不会清空用户已填写的值。
6. 预留空间 `SD_MIN_FREE_BYTES = 64 MiB`。
7. SD 缺失 / 挂载失败 → 降级运行：备忘录留在 RAM（重启丢失，日志明确告警），
   UI、时钟、指令解析照常；可用串口 `SDPROBE` 诊断。

## 指令来源

备忘录既可由串口写入，也可由 WorkBuddy 网络下发。两条通道都进入同一个
`commandService::execute()` 解析器（协议见 `include/sunisle/protocol.h`），
因此不存在两套数据模型。网络通道默认关闭，在 gitignored 的 `include/secrets.h`
里设置 `SUNISLE_CMD_HOST` 后启用。
