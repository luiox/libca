# em_driver 设计文档

## 目标

`em_driver` 负责把驱动源码按需注入到用户 target，避免用户手工维护大量 `add_files`。

核心目标：

1. 驱动接入统一：全部通过 `import("libca.em") + em.add_libs(..., "em_driver", ...)`。
2. 驱动扩展简单：新增驱动只需要补一个 manifest 文件。
3. 平台一致性：驱动源码在用户 target 内编译，天然继承用户编译参数。

## 架构

`em_driver` 由两层组成：

1. 模块分发层：`xmake/modules/libca/em_modules/em_driver.lua`
2. 解释器层：`xmake/modules/libca/em_driver_interpreter.lua`

分发层负责读取用户传入的驱动配置；解释器层负责解析 manifest 并做源码/宏/端口注入。

## Manifest 规范

推荐路径：`src/em_driver/<driver>/<driver>.lua`。

最小示例：

```lua
return {
    name = "led",
    dir = "led",
    src = {"led.c"}
}
```

常用扩展字段：

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

## 解释器规则

1. manifest 查找顺序：优先目录模式，再兼容历史平铺模式。
2. `src` 为必填，且文件必须存在。
3. 用户传 `port` 时，只注入用户端口文件。
4. 用户不传 `port` 时，先注入 `default_port_src`，没有则扫描 `port_*.c`。
5. `port_config` 中每个配置项必须有 `default` 与 `values`，解释器按用户值映射到 `add_defines`。

## 扩展新驱动流程

1. 新建目录 `src/em_driver/<name>/`。
2. 放置 `<name>.h`、`<name>.c`、可选 `port_<name>.c`。
3. 新建 `<name>.lua` manifest。
4. 可选：新增 `<name>.md` 使用说明。
5. 在 demo 或业务 target 里通过 `em.add_libs(..., "em_driver", {...})` 验证。

## 兼容性说明

- 推荐目录 manifest：`src/em_driver/<name>/<name>.lua`。
- 历史平铺 manifest 仍可兼容，但不建议新增使用。
