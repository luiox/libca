# em_driver 新规范（import 解释器模式）

本规范用于统一 em_driver 在源码包导入场景下的组织和接入方式。

## 1. 设计目标

- 通过 import("libca.em") + em.add_libs 注入驱动源码。
- 每个驱动由独立清单（manifest）描述，使用解释器解析。
- 保证驱动源码与用户 target 使用同一套平台、架构、工具链与编译参数。

## 2. 目录组织

每个驱动采用独立目录：

```text
src/em_driver/
  xxx/
    xxx.h
    xxx.c
    port_xxx.c      # 可选，默认 port
    xxx.lua         # 驱动清单（必须）
    xxx.md          # 使用说明（建议）
```

## 3. 用户侧统一接入方式

```lua
target("mtester")
    set_kind("binary")
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

建议 include 写法：

```c
#include <em_driver/led/led.h>
```

## 4. 驱动清单语法

驱动清单文件路径推荐：

- src/em_driver/<driver>/<driver>.lua

清单返回 table，核心字段：

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

## 5. 端口注入规则（强约束）

- 用户未传 port：
  - 先使用 default_port_src。
  - 若 default_port_src 为空，自动扫描驱动目录下 port_*.c。
- 用户传入 port 列表：
  - 仅注入用户传入的 port。
  - 不注入默认 port。
- port 要求为绝对路径列表，且文件必须存在。

## 6. 配置与宏规则

- port_config 下每个配置项必须包含：
  - default（string）
  - values（map: option -> define）
- 用户未传配置值时使用 default。
- 解释器会将选中项映射为 defines 注入到 target。

## 7. 适用范围

本规范适用于 src/em_driver 下所有驱动；新增驱动必须遵循该规范。
