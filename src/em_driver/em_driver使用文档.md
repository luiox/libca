# em_driver 使用文档

## 快速接入

```lua
add_moduledirs(path.join(os.scriptdir(), "..", "xmake", "modules"))

target("app")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {root = path.join(os.scriptdir(), "..")})
        em.add_libs(target, "em_driver", {
            led = {
                mode = "extern",
                port = {path.join(os.scriptdir(), "board", "port_led.c")}
            }
        })
    end)
```

头文件建议：

```c
#include <em_driver/led/led.h>
```

## 参数说明

在 `em.add_libs(target, "em_driver", {...})` 中，驱动名作为 key。

示例：

```lua
em.add_libs(target, "em_driver", {
    ds18b20 = {
        mode = "extern",
        port = {path.join(os.scriptdir(), "board", "port_ds18b20.c")}
    }
})
```

通用参数：

1. `mode`：来自驱动 manifest 的 `port_config.mode`。
2. `port`：端口文件绝对路径列表。

## 端口规则

1. 不传 `port`：用驱动默认端口。
2. 默认端口未配置：自动扫描 `port_*.c`。
3. 传了 `port`：只用用户传入文件，不再注入默认端口。

## 常见问题

1. 重复符号（LNK2005）：通常是默认端口和自定义端口同时编译，检查是否误传了重复源文件。
2. 找不到 manifest：检查路径是否为 `src/em_driver/<driver>/<driver>.lua`。
3. 找不到头文件：确认 `em.setup` 的 `root` 指向 libca 根目录。
