# em_format 使用文档

## 目录

- [快速开始](#快速开始)
- [配置宏](#配置宏)
- [API 参考](#api-参考)
- [格式化行为说明](#格式化行为说明)
- [常见用例](#常见用例)
- [编译与测试](#编译与测试)

---

## 快速开始

### 1) 仅基础格式（默认，最省体积）

```c
#include "../em_format/format.h"

char buf[64];
fmt_snprintf(buf, sizeof(buf), "id=%u name=%s", 7U, "mcu");
// 输出: id=7 name=mcu
```

默认仅支持 `%d %u %s %%`。

### 2) 启用 `%x/%X`

在 xmake target 中加定义：

```lua
add_defines("FMT_ENABLE_HEX=1")
```

### 3) 启用 `%f` 与宽度/精度

```lua
add_defines("FMT_ENABLE_FLOAT=1")
add_defines("FMT_ENABLE_WIDTH_PRECISION=1")
add_defines("FMT_ENABLE_HEX=1")
add_defines("FMT_FLOAT_MODE=FMT_FLOAT_MODE_SIMPLE")
```

---

## 配置宏

| 宏 | 默认值 | 说明 |
|---|---:|---|
| `FMT_ENABLE_FLOAT` | `0` | 启用 `%f` |
| `FMT_ENABLE_WIDTH_PRECISION` | `0` | 启用 `%0Nd`、`%.Nf` |
| `FMT_ENABLE_HEX` | `0` | 启用 `%x/%X` |
| `FMT_FLOAT_MODE` | `FMT_FLOAT_MODE_FIXED` | 浮点处理策略 |
| `FMT_FIXED_DECIMALS` | `3U` | `FIXED` 模式固定小数位 |
| `FMT_DEFAULT_PRECISION` | `3U` | 未显式指定精度时的小数位 |

浮点模式：
- `FMT_FLOAT_MODE_FIXED`：固定 `FMT_FIXED_DECIMALS` 位，截断。
- `FMT_FLOAT_MODE_SIMPLE`：按请求/默认精度，截断。
- `FMT_FLOAT_MODE_NORMAL`：按请求/默认精度，舍入。

---

## API 参考

### 类型转换

```c
usize u32_to_str(char* buf, u32 val);
usize u32_to_str_safe(char* buf, usize buf_len, u32 val);

usize f32_to_str(char* buf, f32 val, u32 decimal_num);
usize f32_to_str_safe(char* buf, usize buf_len, f32 val, u32 decimal_num);

usize f64_to_str(char* buf, f64 val, u32 decimal_num);
usize f64_to_str_safe(char* buf, usize buf_len, f64 val, u32 decimal_num);
```

### 轻量格式化

```c
i32 fmt_vsnprintf(char* buf, usize buf_size, const char* fmt, va_list args);
i32 fmt_vsnprintf_fast(char* buf, usize buf_size, const char* fmt, va_list args);

i32 fmt_snprintf(char* buf, usize buf_size, const char* fmt, ...);
i32 fmt_snprintf_fast(char* buf, usize buf_size, const char* fmt, ...);
i32 fmt_sprintf(char* buf, const char* fmt, ...);
```

返回语义：
- `fmt_snprintf/fmt_vsnprintf`：标准 `snprintf` 语义（返回“本应写入长度”）。
- `fmt_snprintf_fast/fmt_vsnprintf_fast`：截断时返回“实际写入长度”（`buf_size-1`）。

---

## 格式化行为说明

- 不支持的格式符按“`%` + 该字符”原样输出。
- 当 `FMT_ENABLE_WIDTH_PRECISION=0` 时，`%02d` / `%.2f` 这类修饰不解析。
- 当 `FMT_ENABLE_FLOAT=0` 时，`%f` 视为不支持格式符。

---

## 常见用例

### 用例1：极简日志格式

```c
char line[80];
fmt_snprintf_fast(line, sizeof(line), "t=%u state=%d", tick, state);
```

### 用例2：启用浮点并保留 2 位小数

```c
// 编译宏开启 FMT_ENABLE_FLOAT=1, FMT_ENABLE_WIDTH_PRECISION=1
char line[80];
fmt_snprintf(line, sizeof(line), "v=%.2f", voltage);
```

### 用例3：固定 3 位小数

```lua
add_defines("FMT_ENABLE_FLOAT=1")
add_defines("FMT_FLOAT_MODE=FMT_FLOAT_MODE_FIXED")
add_defines("FMT_FIXED_DECIMALS=3")
```

---

## 编译与测试

```bash
xmake build test-format
xmake run test-format

xmake build test-format-float-fixed
xmake run test-format-float-fixed

xmake build test-format-float-simple
xmake run test-format-float-simple

xmake build test-format-float-normal
xmake run test-format-float-normal
```
