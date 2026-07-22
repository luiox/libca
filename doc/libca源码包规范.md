# libca源码包规范

为了彻底解决libca需要面对的复杂的嵌入式环境带来的编译选项不一致问题，最终选择源码包注入的方案。本文档是所有相关的源码包定义的唯一最终可信来源。对于整个源码包管理，只限定用户端的使用情况和库创建时候一些必须的描述文件定义，其余实现可以自由选择。

## modules

各种modules的使用，不限定具体实现，但是必须满足下面这样子的使用方式。

```lua
add_moduledirs("<path/to/libca/xmake/modules>")

target("app")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = "<path/to/libca>",
            debug = true
        })
        em.add_libs(target, {
            em_protocol = {},
            em_util = {},
            em_base = {}
        })
    end)
```

其中`setup`需要指定libca的root目录，而debug选项也是必须实现的，这个是一个可选性，默认为false，为true时候需要输出调试信息。

而`add_libs`是添加组件的方式。推荐传入以module名为key、module配置为value的table；无配置时value可以是空table或`true`。兼容的单module形式是`em.add_libs(target, "em_xxx", opts)`，但不允许用多个字符串冒充可变参数。

必须显式添加所有依赖，不会隐式注入。管理器在target的`after_load`阶段统一校验整组请求并按依赖拓扑顺序注入，因此table内顺序以及多次`add_libs`调用的先后顺序都无关。缺少依赖时必须终止构建，并打印缺少依赖的module、dependency和target。重复添加的源文件、include目录和宏必须去重。

重复调用`add_libs`仅保留最后一次添加的module对应的配置。

另外，`includedirs`的注入策略固定为仅注入`src_root`（即`<root>/libca.em/src`）。这是有意为之，目的是保证用户以`<em_xxx/xxx.h>`形式统一包含头文件。

内置源码包module及其显式依赖如下：

| module | 依赖 |
|--------|------|
| `em_base` | 无 |
| `em_bus` / `em_crypto` / `em_dstream` / `em_format` / `em_motion` / `em_ota` / `em_platform` / `em_shell` / `em_test` / `em_util` | `em_base` |
| `em_component` / `em_mpool` / `em_protocol` | `em_base`, `em_util` |
| `em_driver` | `em_base`，以及所选driver描述文件声明的`deps` |
| `em_log` | `em_base`, `em_dstream`, `em_format`, `em_platform` |

`em_eimui`当前包含SDL mock和项目级路由/handler，是宿主侧原型，不属于可注入MCU源码包。调用`em.list_modules()`可以获取当前支持的module清单。

## em_driver

em_driver的使用大致如下。`port`是可选的，如果没有的话，那就什么都不做。

```lua
em.add_libs(target, {
    em_driver = {
        led = {
            mode = "extern",
            port = {path.join(os.scriptdir(), "board", "port_led.c")}
        }
    },
    em_base = {}
})
```

针对em_driver的各种驱动，每个驱动必须定义类似于下面这样子的内容的一个文件。这个文件通常跟驱动名字对应，例如`em_driver/<name>/<name>.lua`。需要返回一个返回配置表的函数，这个函数的要求是参数是ctx，目前暂时不用。返回的配置表有要求。

```lua
return function(ctx)
    local config = {
        -- 基本信息
        name = "led",
        dir = "led", -- 相对于 em_driver 的路径
        deps = {}, -- 除em_base之外的module依赖

        -- 源码处理逻辑（约定）
        src = { "led.c" },

        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_LED_PORT_MODE=1",
                    dynamic= "LIBCA_LED_PORT_MODE=2"
                }
            },
            extra_cfg = {
                default = "feature_a",
                values = {
                    feature_a = "ENABLE_FEATURE_A=1",
                    feature_b = "ENABLE_FEATURE_B=1"
                }
            }
        }
    }
    return config
end
```

这里要求这个返回的配置表是静态描述数据，不能依赖运行时扫描目录得到`src`、`deps`或`port_config`。

所有的driver应该都是一样的，name就是driver的名字，这个name是在这个里面作为key用。

然后第二个就是dir，这个用于定位后面的driver源码的目录。这个路径，我们就要求是相对em_driver目录的就行。

其次就是源码，我们要求是`src = { "led.c" },`这样子的一个字符串列表，解释器注意检查文件是否存在，不存在要输出不存在的文件路径。

`deps`是可选的字符串列表，只声明driver除`em_base`外的module依赖。用户仍必须在`add_libs`中显式添加这些module；例如`tofxxf`声明并要求`em_util`。

然后是`port_config`，这个是一个复杂的`map`，以宏为配置，`mode`这个是必须的，其他都是类似的，都应该是default+values的这样子的组合，values定义了不同的选项和对应的宏定义。`extra_cfg`仅仅是作为一个例子，可以没有。

`port`是可选参数，如果用户没有传入`port`，解释器不注入任何port源文件，也不自动扫描默认port文件。

错误信息格式不强制固定，但建议采用`module/dependency/target`三元组，方便日志定位。

