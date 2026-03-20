# led 驱动使用说明

## 1. 头文件

```c
#include <em_driver/led/led.h>
```

## 2. 推荐接入方式（import）

```lua
target("app")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {root = "path/to/libca"})
        em.add_libs(target, "em_driver", {
            led = {
                mode = "extern",
                port = {path.join(os.scriptdir(), "board", "port_led.c")}
            }
        })
    end)
```

## 3. 参数说明

- mode：由 led.lua 的 port_config.mode 控制。
  - extern（默认）
  - dynamic
- port：可选列表，必须是绝对路径字符串列表。

## 4. 行为规则

- extern + 未传 port：
  - 注入默认 port（default_port_src 或自动扫描 port_*.c）。
- extern + 传入 port：
  - 仅注入用户 port，不注入默认 port。
- dynamic：
  - 通过宏切换行为；若未传 port，不注入默认 port 以外文件。

## 5. 驱动清单

led 清单位于：

- src/em_driver/led/led.lua

清单字段决定：

1. 源码注入列表（src）
2. 默认 port 源码（default_port_src）
3. 配置项到宏（port_config）