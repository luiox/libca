# led 驱动使用说明

## 1. 头文件包含

```c
#include <em_driver/led/led.h>
```

## 2. 启用规则

在用户 target 中添加：

```lua
add_rules("libca.em_driver.led")
```

等价于：

```lua
add_rules("libca.em_driver.led", {mode = "extern"})
```

## 3. mode 参数

- extern: 默认模式。
- dynamic: 动态注入模式。

示例：

```lua
add_rules("libca.em_driver.led", {mode = "dynamic"})
```

## 4. port 参数（可选列表）

port 仅在 extern 模式下生效，必须是列表：

```lua
add_rules("libca.em_driver.led", {mode = "extern", port = {"board/port_led.c", "board/port_led_extra.c"}})
```

行为如下：

- extern + 未传 port:
  - 自动编译 led 默认 weak port 实现（port_led.c）。
- extern + 传入 port 列表:
  - 仅编译用户传入的 port 文件，不再编译默认 port_led.c。
- dynamic:
  - 不编译任何 port 文件，用户应通过 led_bind_port() 完成端口函数绑定。

## 5. 最小示例（extern + 自定义 port）

```lua
target("mtester")
    set_kind("binary")
    set_plat("cross")
    set_arch("arm")
    set_toolchains("arm-none-eabi-custom")

    add_rules("stm32.f1xx", {mcu = "STM32F103xE"})
    add_rules("libca.em_driver.led", {
        mode = "extern",
        port = {"board/port_led.c"}
    })

    add_files("app/*.c")
```

## 6. 默认 port 的意义

默认 port_led.c 只提供 weak 空实现，主要用于编译占位，实际硬件行为由用户提供的 port 实现决定。