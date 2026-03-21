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
