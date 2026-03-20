# em_driver 文档总览

本目录的驱动采用 import + 解释器模式接入，不再推荐 add_rules 方式。

## 推荐接入

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

## 驱动清单规范

每个驱动目录下应提供同名清单：

- src/em_driver/<driver>/<driver>.lua

清单示例：

```lua
return {
    name = "led",
    dir = "led",
    src = {"led.c"},
    default_port_src = {"port_led.c"},
    port_config = {
        mode = {
            default = "extern",
            values = {
                extern = "LIBCA_LED_PORT_MODE=1",
                dynamic = "LIBCA_LED_PORT_MODE=2"
            }
        }
    }
}
```

## 参数约定

在 `em.add_libs(target, "em_driver", {...})` 中，驱动名作为 key：

```lua
em.add_libs(target, "em_driver", {
    ds18b20 = {
        mode = "extern",
        port = {path.join(os.scriptdir(), "board", "port_ds18b20.c")}
    }
})
```

通用字段：

1. mode: 由驱动 `port_config.mode` 决定可选值。
2. port: 绝对路径列表。传入时仅使用用户 port，不注入默认 port。

## 默认 port 规则

1. 未传 `port`：使用 `default_port_src`。
2. 若 `default_port_src` 为空：自动扫描 `port_*.c`。
3. 传入 `port`：仅注入用户传入文件。
