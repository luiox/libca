# libca 导入导出机制设计与使用文档

## 1. 文档目标

本文档用于说明 libca 在源码包场景下的导入导出机制，覆盖：

1. 设计目标与约束。
2. 当前实现架构。
3. 用户侧接入方式。
4. 模块与驱动的扩展方式。
5. 与旧规则体系的兼容关系。

本文档已删除历史阶段中未落地或已废弃的方案描述，内容与当前代码实现保持一致。

## 2. 设计目标

当前机制的核心目标：

1. 用户通过 import 方式按需注入源码，不依赖预编译聚合库。
2. 保证驱动源码与用户 target 使用同一套编译配置。
3. 统一模块依赖管理，避免用户手工记忆依赖链。
4. 保证 add_files/add_includedirs/add_defines 幂等去重。
5. 支持 em_driver 的数据驱动解释模式，降低维护成本。

## 3. 总体架构

### 3.1 入口

用户脚本入口为：

- import("libca.em")

导出 API 位于：

- xmake/modules/libca/em.lua

主要函数：

1. setup(target, opts)
2. add_libs(target, name, opts)
3. get_state(target)
4. register_module(name, handler)
5. register_driver(name, handler)

### 3.2 内部模块

内部由以下组件组成：

1. em_core: 状态管理、模块分发、依赖展开。
2. em_registry: 模块处理器注册与按需加载。
3. em_inject: add_files/add_includedirs/add_defines 去重注入。
4. em_modules/*: 各模块 handler。
5. em_driver_interpreter: em_driver 的驱动清单解释器。

### 3.3 当前状态存储

状态目前存储于 em_core 内部的按 target 键控缓存中，不依赖 target values。

状态中包含：

1. root
2. modules 启用记录
3. options 累积参数
4. injected 去重桶（files/includedirs/defines）

## 4. 导入机制（import）

### 4.1 用户最小接入

```lua
add_moduledirs("third_party/libca/xmake/modules")

target("app")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {root = "third_party/libca"})
        em.add_libs(target, "em_driver", {
            led = {
                mode = "extern",
                port = {path.join(os.scriptdir(), "board", "port_led.c")}
            }
        })
    end)
```

### 4.2 setup(target, opts)

职责：

1. 校验 target 对象。
2. 解析 root 为绝对路径。
3. 初始化当前 target 的源码注入状态。

要求：

1. opts.root 必须可解析到有效目录。
2. setup 必须先于 add_libs 调用。

### 4.3 add_libs(target, name, opts)

职责：

1. 按模块名分发到对应 handler。
2. 自动展开依赖并先注入依赖模块。
3. 合并同模块多次调用参数。
4. 触发注入去重机制。

当前支持模块：

1. em_base
2. em_util
3. em_shell
4. em_protocol
5. em_log
6. em_driver

## 5. 导出机制（库能力导出）

libca 的源码包能力通过 xmake module 导出，外部工程通过 add_moduledirs + import 获取。

导出层级：

1. 对外稳定 API：libca.em
2. 对内可扩展接口：register_module/register_driver

扩展导出示例：

```lua
local em = import("libca.em")

em.register_module("em_custom", {
    deps = {"em_base"},
    handle = function (target, state, opts)
        local src_root = path.join(state.root, "src")
        target:add("includedirs", src_root)
        target:add("files", path.join(src_root, "em_custom", "custom.c"))
    end
})
```

## 6. 模块分发器设计

### 6.1 注册与加载

em_registry 的行为：

1. 启动时扫描 xmake/modules/libca/em_modules 下的文件。
2. add_libs 请求模块时按名字延迟加载对应 handler。
3. handler 需导出 get_handler() 并返回 table。

handler 要求：

1. deps: 依赖模块名列表，可为空。
2. handle(target, state, opts, registry): 模块处理函数。

### 6.2 依赖管理

依赖由 handler.deps 声明，em_core 递归展开。

示例：

1. em_driver deps = {"em_base"}
2. em_protocol deps = {"em_base", "em_util"}

### 6.3 注入幂等

em_inject 对以下项做去重：

1. files
2. includedirs
3. defines

同一 target 重复 add_libs 不会重复注入同一路径/宏。

## 7. em_driver 解释器设计

### 7.1 驱动清单位置

解释器按以下顺序查找清单：

1. src/em_driver/<driver>/<driver>.lua（推荐）
2. src/em_driver/<driver>.lua（兼容）

### 7.2 驱动清单语法

驱动清单必须返回 table。

推荐字段：

1. name: 驱动名。
2. dir: 相对 em_driver 根目录的驱动目录。
3. src: 核心源码列表。
4. default_port_src: 默认 port 源码列表（可选）。
5. port_config: 配置项到宏定义的映射（可选）。

示例：

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

### 7.3 用户调用语法

```lua
em.add_libs(target, "em_driver", {
    led = {
        mode = "extern",
        port = {path.join(os.scriptdir(), "board", "port_led.c")}
    }
})
```

### 7.4 解释规则

1. src 中列出的源码会被注入，并检查文件存在。
2. port_config 中每个配置项必须有 default 和 values。
3. 用户未传 port 时：
   - 若 default_port_src 非空，注入该列表。
   - 否则自动扫描驱动目录下 port_*.c 作为默认 port。
4. 用户传 port 时：
   - 仅注入用户传入的 port 列表。
   - 每个路径必须是绝对路径且文件存在。
5. mode 等配置项通过 port_config 转换为 defines。

## 8. 已有模块行为约定

### 8.1 em_base

注入：

1. src/em_base/datatype.c
2. src/em_base/debug.c
3. src/em_base/compiler_compat.c

### 8.2 em_util

注入 src/em_util 下 c 源码，过滤 test- 前缀文件。

### 8.3 em_shell

注入 src/em_shell/shell.c。

### 8.4 em_protocol

注入 src/em_protocol 下 c 源码，过滤 test- 前缀文件。

### 8.5 em_log

1. backend 选项类型为 string。
2. 默认 backend 为 simple_logger。
3. 注入：
   - src/em_log/log.c
   - src/em_log/<backend>.c

## 9. 兼容与迁移

当前采用并行机制：

1. 保留旧规则入口 libca_em.lua，保障存量用户。
2. 新工程推荐使用 import("libca.em")。
3. 可逐步将旧规则内部改造成转调 import 机制。

## 10. Demo 与验证

参考 demo 目录：

1. demo_led_extern: extern + 用户 port。
2. demo_led_dynamic: dynamic 模式。
3. demo_led_default_port: extern + 默认 port。
4. demo_driver_manifests_check: 驱动清单覆盖检查。

建议命令：

```bash
xmake -P demo build demo_led_extern
xmake -P demo run demo_led_extern
xmake -P demo build demo_led_dynamic
xmake -P demo run demo_led_dynamic
xmake -P demo build demo_led_default_port
xmake -P demo run demo_led_default_port
xmake -P demo build demo_driver_manifests_check
xmake -P demo run demo_driver_manifests_check
```

## 11. 常见错误与排查

1. call setup first:
   - 原因：未先调用 setup。
2. unsupported module:
   - 原因：模块无对应 handler 或名称拼写错误。
3. manifest must return table:
   - 原因：驱动清单格式不合法。
4. source not found:
   - 原因：src/default_port_src 列表与实际文件不一致。
5. port item must be absolute path:
   - 原因：用户 port 传了相对路径。

## 12. 维护建议

1. 新增模块时，优先在 em_modules 下新增独立 handler 文件。
2. 新增驱动时，优先在 src/em_driver/<name>/<name>.lua 中维护清单。
3. 尽量显式维护 src 列表，避免误编译测试/示例源码。
4. 对外 API 保持 setup/add_libs 稳定，避免破坏用户工程。
