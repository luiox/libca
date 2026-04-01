# em_base 使用文档

## 快速接入

```lua
add_moduledirs(path.join(os.scriptdir(), "..", "xmake", "modules"))

target("app")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {root = path.join(os.scriptdir(), "..")})
        em.add_libs(target, "em_base")
    end)
```

## 实现版本配置

`em_base` 支持分别配置 `memory_util` 与 `string_util` 的实现版本：

- `std`：标准库内联实现（默认）
- `custom`：内置实现（适用于无标准库环境）

### 默认配置（可省略）

```lua
on_load(function (target)
    local em = import("libca.em")
    em.setup(target, {root = path.join(os.scriptdir(), "..")})
    em.add_libs(target, "em_base") -- 等价于 memory_util=std, string_util=std
end)
```

### 独立配置示例

```lua
on_load(function (target)
    local em = import("libca.em")
    em.setup(target, {root = path.join(os.scriptdir(), "..")})
    em.add_libs(target, "em_base", {
        memory_util = "custom",
        string_util = "std"
    })
end)
```

### 选项说明

| 选项 | 值 | 默认值 | 对应宏 |
|------|----|--------|--------|
| `memory_util` | `std` / `custom` | `std` | `USE_CUSTOM_MEMORY_UTIL_IMPL` |
| `string_util` | `std` / `custom` | `std` | `USE_CUSTOM_STRING_UTIL_IMPL` |

若传入非法值，将回退到 `std` 并输出 warning。

## 头文件

```c
#include <em_base/datatype.h>
#include <em_base/debug.h>
#include <em_base/compiler_compat.h>
```

## 典型用法

```c
#include <em_base/datatype.h>
#include <em_base/debug.h>

void app_demo(void)
{
    u32 value = 123;
    debug_printf("value=%u\n", value);
}
```

## 说明

1. `em_base` 是基础依赖模块，需要由用户在工程中显式添加。
2. 如果你只用基础类型和调试接口，可单独引入 `em_base`。
