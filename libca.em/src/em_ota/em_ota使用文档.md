---
version: 1.0
update: 
2026-02-28 - 完成第一版的使用文档编写
---

## em_ota 使用文档

em_ota 模块提供 Flash 分区管理功能，支持分区定义、读写擦除和流式写入，适用于 OTA 升级场景。

---

## 模块组成

| 文件 | 说明 |
|------|------|
| `partition.h` | 分区管理器接口定义 |
| `partition.c` | 分区管理器实现 |

---

## 核心概念

### 分区（Partition）

分区是 Flash 的逻辑划分，每个分区包含：
- **名称**：字符串标识，用于查找
- **起始地址**：绝对地址
- **大小**：字节数
- **属性标志**：可读/可写/可擦除

### 流式写入（Stream）

流式写入专为 OTA 升级设计，支持：
- 分块写入数据
- 写入进度跟踪
- 回调通知
- 大小校验

---

## API 概览

### Port 注册

```c
void partition_register_port(const partition_port_t *port);
bool partition_port_is_registered(void);
```

### 分区查找

```c
const partition_t* partition_find(const partition_t *table, usize count, const char *name);
```

### 基础操作

```c
i32 partition_read(const partition_t *part, u32 offset, u8 *buf, u32 len);
i32 partition_write(const partition_t *part, u32 offset, const u8 *data, u32 len);
i32 partition_erase(const partition_t *part);
i32 partition_erase_range(const partition_t *part, u32 offset, u32 len);
```

### 流式写入

```c
i32 partition_stream_open(partition_stream_t *stream, const partition_t *part, u32 total_size);
i32 partition_stream_write(partition_stream_t *stream, const u8 *data, u32 len);
i32 partition_stream_close(partition_stream_t *stream);
u32 partition_stream_offset(const partition_stream_t *stream);
u32 partition_stream_remaining(const partition_stream_t *stream);
```

---

## 使用示例

### 1. 定义分区表

```c
#include "partition.h"

/* 用户定义的静态分区表 */
static const partition_t my_partitions[] = {
    {"bootloader", 0x08000000, 0x00010000, PARTITION_FLAG_READONLY},
    {"app",        0x08010000, 0x00040000, PARTITION_FLAG_READABLE | PARTITION_FLAG_WRITABLE | PARTITION_FLAG_ERASEABLE},
    {"download",   0x08050000, 0x00020000, PARTITION_FLAG_READABLE | PARTITION_FLAG_WRITABLE | PARTITION_FLAG_ERASEABLE},
    {"params",     0x08070000, 0x00010000, PARTITION_FLAG_READABLE | PARTITION_FLAG_WRITABLE | PARTITION_FLAG_ERASEABLE},
};

#define PARTITION_COUNT (sizeof(my_partitions) / sizeof(my_partitions[0]))
```

### 2. 注册底层 Port

```c
/* 底层 Flash 操作实现 */
static i32 flash_read(u32 addr, u8 *buf, u32 len)
{
    // 调用 HAL 或驱动读取 Flash
    // ...
    return 0;  // 成功返回 0
}

static i32 flash_write(u32 addr, const u8 *data, u32 len)
{
    // 调用 HAL 或驱动写入 Flash
    // ...
    return 0;
}

static i32 flash_erase(u32 addr, u32 len)
{
    // 调用 HAL 或驱动擦除 Flash
    // ...
    return 0;
}

static const partition_port_t flash_port = {
    .read  = flash_read,
    .write = flash_write,
    .erase = flash_erase,
};

/* 初始化时注册 */
void ota_init(void)
{
    partition_register_port(&flash_port);
}
```

### 3. 分区读写

```c
void read_app_header(void)
{
    const partition_t *app = partition_find(my_partitions, PARTITION_COUNT, "app");
    if (app == NULL) {
        return;  // 分区不存在
    }

    u8 header[16];
    i32 ret = partition_read(app, 0, header, sizeof(header));
    if (ret != PARTITION_OK) {
        // 读取失败
        return;
    }

    // 处理 header...
}
```

### 4. OTA 升级流程（流式写入）

```c
/* OTA 进度回调 */
static void on_progress(u32 offset, const u8 *data, u32 len, void *userdata)
{
    // 更新进度条、计算校验和等
    printf("OTA progress: %u bytes written\n", offset + len);
}

i32 ota_update(const u8 *firmware, u32 size)
{
    const partition_t *download = partition_find(my_partitions, PARTITION_COUNT, "download");
    if (download == NULL) {
        return -1;
    }

    /* 先擦除分区 */
    i32 ret = partition_erase(download);
    if (ret != PARTITION_OK) {
        return ret;
    }

    /* 打开流 */
    partition_stream_t stream;
    ret = partition_stream_open(&stream, download, size);
    if (ret != PARTITION_OK) {
        return ret;
    }

    /* 设置进度回调 */
    stream.on_block_written = on_progress;

    /* 分块写入（模拟网络分块接收） */
    const u32 block_size = 4096;
    u32 remaining = size;
    const u8 *ptr = firmware;

    while (remaining > 0) {
        u32 to_write = (remaining > block_size) ? block_size : remaining;

        ret = partition_stream_write(&stream, ptr, to_write);
        if (ret != PARTITION_OK) {
            return ret;  // 写入失败
        }

        ptr += to_write;
        remaining -= to_write;
    }

    /* 关闭流（自动校验大小） */
    ret = partition_stream_close(&stream);
    if (ret != PARTITION_OK) {
        return ret;  // 大小不匹配
    }

    return 0;  // OTA 完成
}
```

### 5. 校验固件后写入 App 分区

```c
i32 verify_and_write_app(void)
{
    const partition_t *download = partition_find(my_partitions, PARTITION_COUNT, "download");
    const partition_t *app = partition_find(my_partitions, PARTITION_COUNT, "app");

    // 读取 download 分区数据校验
    // ...

    // 擦除 app 分区
    partition_erase(app);

    // 复制数据...
}
```

---

## 错误码

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `PARTITION_OK` | 0 | 成功 |
| `PARTITION_ERR_NOT_FOUND` | -1 | 分区未找到 |
| `PARTITION_ERR_INVALID_PARAM` | -2 | 无效参数 |
| `PARTITION_ERR_OUT_OF_RANGE` | -3 | 超出分区范围 |
| `PARTITION_ERR_READONLY` | -4 | 分区只读 |
| `PARTITION_ERR_READ_FAIL` | -5 | 读取失败 |
| `PARTITION_ERR_WRITE_FAIL` | -6 | 写入失败 |
| `PARTITION_ERR_ERASE_FAIL` | -7 | 擦除失败 |
| `PARTITION_ERR_PORT_NOT_SET` | -8 | 底层端口未注册 |
| `PARTITION_ERR_SIZE_MISMATCH` | -9 | 流式写入大小不匹配 |
| `PARTITION_ERR_NOT_OPEN` | -10 | 流未打开 |

---

## 分区属性标志

| 标志 | 说明 |
|------|------|
| `PARTITION_FLAG_READABLE` | 可读 |
| `PARTITION_FLAG_WRITABLE` | 可写 |
| `PARTITION_FLAG_ERASEABLE` | 可擦除 |
| `PARTITION_FLAG_READONLY` | 只读（等同于 `READABLE`） |

---

## 注意事项

1. **初始化顺序**：必须先调用 `partition_register_port()` 注册底层端口，再使用其他功能
2. **Flash 解锁**：如需 Flash 解锁/加锁，请在调用分区操作前后自行处理
3. **静态分配**：所有结构体由用户静态分配，无动态内存
4. **线程安全**：模块本身不保证线程安全，多线程环境需自行加锁

---

## 依赖

- `em_base`（datatype.h, debug.h, string_util.h, memory_util.h）
