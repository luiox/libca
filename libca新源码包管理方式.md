# libca 新源码包管理方式

## 1. 目标与背景

当前希望解决的问题是：

1. 提供对用户友好的源码包接入方式，做到一键注入当前 target。
2. 避免先编译成中间库（.o/.a）再链接导致的编译配置不一致。
3. 将路径解析、模块启用、驱动参数、依赖补齐统一到一套可扩展接口。
4. 避免依赖 includes 作用域细节，统一采用 import 作为脚本域入口。

本方案采用 import 模块化方式，入口为 `libca.em`，并引入 target 级配置中心（values）存储状态。

## 2. 用户侧目标语法

建议用户使用如下语法：

```lua
target("demo_led_extern")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")

        em.setup(target, {
            root = "path/to/libca_root"
        })

        em.add_libs(target, "em_driver", {
            led = {
                mode = "extern",
                port = {"board/port_led.c"}
            }
        })
    end)
```

设计原则：

1. `setup` 只做上下文初始化。
2. `add_libs` 做模块注入和参数处理。
3. 所有信息写入 target values，避免散落状态。

## 3. values 数据模型

以 `libca.em` 为命名空间根，建议结构如下：

```lua
{
    root = "path/to/libca_root",
    em_base = {
        enable = true
    },
    em_util = {
        enable = false
    },
    em_driver = {
        enable = true,
        led = {
            mode = "extern",
            port = {"board/port_led.c"}
        }
    }
}
```

目标设计上，建议将此结构存于 target 的 `values` 中，使用固定 key，例如：

- key: `libca.em.state`

当前落地版本说明：

1. 现阶段实现先采用模块内 `_states` 表按 target 名称缓存状态。
2. 这样可以先保证接口可用与行为稳定。
3. 后续可再切换到 target values 持久化（不改变对外 API）。

读取/更新策略：

1. `setup` 初始化根结构。
2. 每次 `add_libs` 做增量合并。
3. 写回时保持幂等，不重复注入同一文件。

## 4. 模块目录建议

为降低 src 目录混乱，建议把导出模块统一放到根目录 `xmake` 文件夹：

```text
xmake/
  modules/
    libca/
      em.lua              # 用户 import("libca.em") 的主入口
      em_core.lua         # 状态管理、合并、校验
      em_inject.lua       # 源码注入工具函数
      em_registry.lua     # 模块/驱动描述注册表
      tool/
        package.lua       # libca.tool 的基础工具
        validate.lua
        path.lua
```

说明：

1. src 目录仅保留源码、测试、内部构建对象。
2. xmake/modules 专门承载对外复用逻辑。
3. 用户工程通过 add_moduledirs 指向 `.../xmake/modules`。

## 5. libca.em 对外接口设计

建议对外仅暴露两个函数：

1. `setup(target, opts)`
2. `add_libs(target, name, opts)`

### 5.1 setup(target, opts)

职责：

1. 校验 target。
2. 解析并归一化 root。
3. 初始化 `libca.em.state`。

要求：

1. root 必填或可推导（推荐必填，避免歧义）。
2. root 统一绝对路径。

### 5.2 add_libs(target, name, opts)

职责：

1. 写入模块配置到 state。
2. 按 name 调用对应注入器。
3. 自动补齐依赖模块。

name 建议支持：

- `em_base`
- `em_util`
- `em_driver`
- 后续可扩展更多 em_xxx。

`em_driver` 的 opts 建议为 map：

```lua
{
    led = {mode = "extern", port = {"board/port_led.c"}},
    ds18b20 = {mode = "dynamic"}
}
```

## 6. 依赖自动补齐策略

核心规则：用户声明功能，不需要手动记依赖。

例如：

1. `add_libs(..., "em_driver", {...})` 自动补 `em_base`。
2. 某些驱动依赖 `em_util` 时按注册表自动补齐。
3. 去重注入，避免重复 add_files。

这能回答 "em_base/em_util 怎么办" 的问题：

- 由 `libca.em` 统一管理，不由用户手工拼依赖。

## 7. 源码注入实现要点

### 7.1 路径策略

1. 统一基于 root 解析，不依赖多层 includes 的 scriptdir。
2. 用户传入 port 路径统一转为绝对路径。
3. 所有 add_files/add_includedirs 走归一化入口。

### 7.2 extern/dynamic 策略

以 led 为例：

1. mode=extern 且未传 port：注入默认 weak port。
2. mode=extern 且传 port 列表：仅注入用户 port。
3. mode=dynamic：不注入 port，设置动态宏并要求用户绑定函数表。

### 7.3 幂等与去重

建议维护内部去重集合，至少覆盖：

1. files
2. includedirs
3. defines

避免多次调用 `add_libs` 导致重复注入。

### 7.4 错误信息

错误必须包含三段信息：

1. 模块名
2. 参数名
3. 期望格式

示例：

- `libca.em: em_driver.led.port must be list(table)`

## 8. libca.tool 设计建议

目标：快速封装任意源码包，减少重复劳动。

建议提供：

1. `tool.register(spec)`
2. `tool.inject(target, spec, opts)`
3. `tool.merge_state(old, patch)`
4. `tool.normalize_paths(root, paths)`
5. `tool.validate(schema, opts)`

这样 em_driver/em_util/em_protocol 都能复用同一套工具链。

## 9. 迁移与兼容策略

当前阶段不迁移既有 `libca_em.lua`，采用并行方案：

1. 保留现有规则入口，保障存量用户。
2. 新用户推荐 import 入口。
3. 在 demo 中持续验证 import 方案。

后续稳定后再评估：

1. 是否将规则入口内部改成转调 import。
2. 是否逐步标记旧规则为兼容层。

## 10. 第一阶段落地范围

建议首批仅覆盖：

1. `setup`
2. `add_libs(target, "em_base")`
3. `add_libs(target, "em_driver", {led = {...}})`
4. `extern` 模式（含 port 列表）

阶段目标：

1. 用户工程可以最小接入 em_base + led。
2. 路径与模式行为可预测。
3. 配置结构可向后扩展，不破坏已有调用。

## 11. 用户工程最小接入示例

```lua
set_project("user-app")
set_languages("c99")
add_rules("mode.debug", "mode.release")

add_moduledirs("third_party/libca/xmake/modules")

target("app")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {root = "third_party/libca"})
        em.add_libs(target, "em_driver", {
            led = {mode = "extern", port = {"board/port_led.c"}}
        })
    end)
```

## 12. 总结

本方案的核心价值：

1. 用 import 替代 includes 函数透传，避免作用域歧义。
2. 用 setup + add_libs + state 统一管理注入行为。
3. 用自动依赖补齐解决 em_base/em_util 的一致性问题。
4. 用 xmake/modules 目录分离导出能力与 src 内部实现，降低维护复杂度。
