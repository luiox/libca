# em_util 使用文档

## 快速接入

```lua
add_moduledirs(path.join(os.scriptdir(), "..", "xmake", "modules"))

target("app")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {root = path.join(os.scriptdir(), "..")})
        em.add_libs(target, "em_util")
    end)
```

## 头文件示例

```c
#include <em_util/crc.h>
#include <em_util/queue.h>
#include <em_util/pid.h>
```

## 常见组合

1. 协议场景：若同时使用 `em_protocol` 与 `em_util`，请在工程中显式添加两者及其依赖。
2. 裸工具场景：只做算法和容器时，直接引入 `em_util` 即可。

## 常见问题

1. 链接数学函数失败：按工具链需求补系统数学库。
2. 找不到头文件：确认已执行 `em.setup`，且 `root` 路径正确。
