# em_driver编写规范

假定我们当前写的驱动对应的硬件模块名字叫做xxx。

任何一个需要外部硬件操作的函数，必须以port的方式告知使用者。对于头文件，一般来说按照下面这样子编写。一定要先port的内容先定义，然后再定义驱动的部分。不要因为都是struct而把port和驱动的结构体定义放一起。

```c
/**
 * @file xxx.h
 * @author 作者名字 (邮箱)
 * @brief 说明
 * @version 版本（一般是2位x.x）
 * @date 时间（yyyy-mm-dd）
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_XXX_H
#define LIBCA_EM_DRIVER_XXX_H

#include "../em_base/datatype.h"

// port
typedef struct xxx_port{
    // 通用接口层，尽可能使用void*代替具体句柄定义
    // ...
}xxx_port_t;

// 绑定port
void xxx_bind_port(const xxx_port_t* port);
bool xxx_port_is_registered(void);

typedef struct xxx
{
    mode_t mode;
    void* gpio;
    u16 pin;
    void* hi2c;
    // 其他需要的硬件资源或者需要存储的数据
    // ...
} xxx_t;

// 初始化
void xxx_init(xxx_t* self, mode_t mode);
// 功能
void xxx_func(xxx_t* self);

#endif // !LIBCA_EM_DRIVER_XXX_H
```

要求必须有`xxx_t`和`xxx_init`，如果没有硬件接口依赖，那么不需要定义一个`xxx_port_t`、`xxx_bind_port`、`xxx_port_is_registered`。如果`xxx_init`里面确实没什么要做的，可以像下面这样子写。

```c
void xxx_init(key_t* self)
{
    // nothing to do
}
```

对于驱动的`xxx_func`这种功能类型的函数，也包括`xxx_init`的第一个参数必须是`xxx_t* self`，只要OOP封装就必须统一命名。

驱动的打印使用`em_base`下的`debug`。例如下面这样子。

```c
#include "../em_base/debug.h"

static void xxx_func(i32 type)
{
    // ...
    debug_print("[xxx] error: unsupport type, type:%d\n", type);
}
```

对于错误处理，如果要返回错误码，就必须以`DS18B20_ERR_NO_PRESENCE`这样子，`模块名字_ERR_错误含义`的格式来定义宏，而且错误码必须是负数。`模块名字_OK`这个值如果要定义，那么必须是0。

对于驱动的使用者，如果不需要关心的模块内部寄存器信息，可以放在.c里面去定义。

em_driver 的驱动通常不编写单元测试，因为驱动与硬件耦合紧密，很多行为无法在纯软件环境中复现或断言。**但要求在驱动发布时提供明确的硬件验证计划**（包含测试步骤、必要的测试设备或夹具、测试向量与验收标准）。如果驱动中存在纯逻辑的部分（例如数据解码、校验、状态机等），应将这些逻辑抽离到无硬件依赖的模块，并为这些模块编写单元测试（遵循 `prompt/em_code_rule.md` 的单元测试规范，使用 `TEST_ENABLE` 和 `em_test` 的 mock 能力）。同时建议在 PR 中附上集成或硬件测试的复现步骤与结果截图/日志，以便 review。

参考模板（驱动头文件模板、驱动实现模板、硬件验证计划模板、PR 检查清单）已放在 `.codebuddy/skills/em-driver-dev/assets`，供开发者直接复用或拷贝。

如果不需要对象参与的部分，不应该OOP风格，比如通用的计算工具函数这种，不依赖于OOP的驱动对象。

对于`xxx_init`的接口设计，如果是硬件的配置，不要用传参的形式进行初始化，而是要求用户自己用大括号法直接初始化好传入，而具体什么模式之类的配置再通过参数传入初始化。
