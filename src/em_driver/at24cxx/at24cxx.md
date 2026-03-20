# at24cxx 驱动使用说明

## 1. 头文件

```c
#include <em_driver/at24cxx/at24cxx.h>
```

## 2. 推荐接入方式

```lua
target("app")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {root = path.join(os.scriptdir(), "..")})
        em.add_libs(target, "em_driver", {
            at24cxx = {
                mode = "extern"
            }
        })
    end)
```

## 3. 常用参数

- mode: 由驱动清单中的 `port_config.mode` 控制（常见为 extern/dynamic）。
- port: 可选，绝对路径列表。传入后只使用用户 port 文件。

示例：

```lua
em.add_libs(target, "em_driver", {
    at24cxx = {
        mode = "extern",
        port = {path.join(os.scriptdir(), "board", "port_at24cxx.c")}
    }
})
```

## 4. 默认 port 行为

- 未传 `port` 时，解释器会优先使用清单中的 `default_port_src`。
- 若清单未配置 `default_port_src`，解释器会自动扫描驱动目录中的 `port_*.c`。

## 5. 相关文件

- 清单：`src/em_driver/at24cxx/at24cxx.lua`
- 源码目录：`src/em_driver/at24cxx/`
