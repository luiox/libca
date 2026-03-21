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
        em.add_libs(target, "em_xxx")
    end)
```

其中`setup`需要指定libca的root目录，而debug选项也是必须实现的，这个是一个可选性，默认为false，为true时候需要输出调试信息。

而`add_libs`是添加组件的方式，必须显式添加依赖所有需要的依赖，要求如果没有添加依赖的module的时候终止构建，并且打印提示，是什么module缺乏依赖的什么module的添加。module的添加顺序应该是无关的，而且重复添加源文件时候应该去重。

重复调用`add_libs`仅保留最后一次添加的module对应的配置。

另外，`includedirs`的注入策略固定为仅注入`src_root`（即`<root>/src`）。这是有意为之，目的是保证用户以`<em_xxx/xxx.h>`形式统一包含头文件。

## em_driver

em_driver的使用大致如下。`port`是可选的，如果没有的话，那就什么都不做。

```c
em.add_libs(target, "em_driver", {
	led = {
		mode = "extern",
		port = {path.join(os.scriptdir(), "board", "port_led.c")}
	}
})
```

针对em_driver的各种驱动，每个驱动必须定义类似于下面这样子的内容的一个文件。这个文件通常跟驱动名字对应，例如`em_driver/<name>/<name>.lua`。需要返回一个返回配置表的函数，这个函数的要求是参数是ctx，目前暂时不用。返回的配置表有要求。

```lua
return function(ctx)
    local config = {
        -- 基本信息
        name = "led",
        dir = "led", -- 相对于 em_driver 的路径

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

这里要求这个返回的配置表是静态描述数据，不能依赖运行时扫描目录得到`src`或`port_config`。

所有的driver应该都是一样的，name就是driver的名字，这个name是在这个里面作为key用。

然后第二个就是dir，这个用于定位后面的driver源码的目录。这个路径，我们就要求是相对em_driver目录的就行。

其次就是源码，我们要求是`src = { "led.c" },`这样子的一个字符串列表，解释器注意检查文件是否存在，不存在要输出不存在的文件路径。

然后是`port_config`，这个是一个复杂的`map`，以宏为配置，`mode`这个是必须的，其他都是类似的，都应该是default+values的这样子的组合，values定义了不同的选项和对应的宏定义。`extra_cfg`仅仅是作为一个例子，可以没有。

`port`是可选参数，如果用户没有传入`port`，解释器不注入任何port源文件，也不自动扫描默认port文件。

错误信息格式不强制固定，但建议采用`module/dependency/target`三元组，方便日志定位。

