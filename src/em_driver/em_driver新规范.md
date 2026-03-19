# em_driver 新规范（rule 源码注入模式）

本规范用于统一 em_driver 的组织和用户接入方式。

## 1. 设计目标

- 不使用驱动聚合目标（不提供 all/selected 聚合规则）。
- 每个驱动以独立 rule 形式注入到用户 target。
- 保证驱动源码与用户 target 使用同一套平台、架构、工具链与编译选项。

## 2. 目录组织

每个驱动采用独立目录：

```text
src/em_driver/
  xxx/
    xxx.h
    xxx.c
    port_xxx.c   # 可选：默认 weak 实现
    xmake.lua
    xxx.md       # 使用说明
```

## 3. 统一接入方式

用户通过 add_rules 按需启用驱动：

```lua
target("mtester")
    set_kind("binary")
    add_rules("libca.em_driver.led")
```

建议 include 写法：

```c
#include <em_driver/led/led.h>
```

rule 内部必须保证为用户 target 添加 src 头文件搜索路径，以支持上述 include 方式。

## 4. rule 参数规范

每个驱动 rule 支持统一参数：

- mode: 注入模式，取值 extern 或 dynamic，默认 extern。
- port: 可选列表，仅在 extern 模式下生效，格式为 {"1.c", "2.c"}。

示例：

```lua
add_rules("libca.em_driver.led", {mode = "extern"})
add_rules("libca.em_driver.led", {mode = "dynamic"})
add_rules("libca.em_driver.led", {mode = "extern", port = {"board/port_led.c", "board/port_led_extra.c"}})
```

## 5. port 注入规则（强约束）

- mode = extern 且未传 port：
  - 自动注入驱动目录中的默认 weak port 文件（如 port_led.c）。
- mode = extern 且传入 port 列表：
  - 仅注入用户传入的 port 文件列表。
  - 不再注入默认 weak port 文件。
- mode = dynamic：
  - 无论是否传入 port，都不注入任何 port 源码文件。

## 6. 实现要求

- 不在 rule 中使用 *.c 全量扫描；优先显式添加核心源码，避免误编译测试文件或样例文件。
- 对 mode 做合法性检查，非法值应直接报错。
- 对 port 做类型检查，要求为列表（table）；非法类型应直接报错。
- 用户传入相对路径时，按项目根目录解析为绝对路径后再添加。

## 7. 适用范围

本规范适用于 src/em_driver 下所有新旧驱动；新驱动必须遵循该规范，旧驱动逐步迁移。
