# em_driver编写规范

假定我们当前写的驱动对应的硬件模块名字叫做xxx。

任何一个需要外部硬件操作的函数，必须以port方式告知使用者。当前统一支持两种注入方式：

1. 隐式注入（默认模式，推荐给快速接入用户）
2. 显式注入（推荐给复杂驱动或需要精细控制的场景）

默认模式必须是最简单模式，因此默认采用隐式注入。显式注入作为增强能力保留，不强制所有驱动都实现双模式。

对于头文件，一般来说按照下面这样子编写。一定要先定义 port 相关内容，再定义驱动对象本体。不要因为都是 struct 而把 port 和驱动结构体定义混排。

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

#include <em_base/datatype.h>

// 注入模式定义（默认隐式注入）
#define LIBCA_XXX_PORT_MODE_EXTERN 1
#define LIBCA_XXX_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_XXX_PORT_MODE
#define LIBCA_XXX_PORT_MODE LIBCA_XXX_PORT_MODE_EXTERN
#endif

#if (LIBCA_XXX_PORT_MODE == LIBCA_XXX_PORT_MODE_EXTERN)

// 隐式注入：由用户在 port_xxx.c 中实现
extern void port_xxx_func(/* ... */);

#elif (LIBCA_XXX_PORT_MODE == LIBCA_XXX_PORT_MODE_DYNAMIC)

// 显式注入：由用户传入函数表
typedef struct xxx_port{
    // 通用接口层，尽可能使用void*代替具体句柄定义
    // ...
}xxx_port_t;

// 绑定port
void xxx_bind_port(const xxx_port_t* port);
bool xxx_port_is_registered(void);

#else
#error "Invalid XXX port mode"
#endif

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

要求必须有`xxx_t`和`xxx_init`。如果没有硬件接口依赖，那么不需要定义`xxx_port_t`、`xxx_bind_port`、`xxx_port_is_registered`。如果`xxx_init`里面确实没什么要做的，可以像下面这样子写。

```c
void xxx_init(key_t* self)
{
    // nothing to do
}
```

对于驱动的`xxx_func`这种功能类型的函数，也包括`xxx_init`的第一个参数必须是`xxx_t* self`，只要OOP封装就必须统一命名。

驱动的打印使用`em_base`下的`debug`。例如下面这样子。

```c
#include <em_base/debug.h>

static void xxx_func(i32 type)
{
    // ...
    debug_print("[xxx] error: unsupport type, type:%d\n", type);
}
```

对于错误处理，如果要返回错误码，就必须以`DS18B20_ERR_NO_PRESENCE`这样子，`模块名字_ERR_错误含义`的格式来定义宏，而且错误码必须是负数。`模块名字_OK`这个值如果要定义，那么必须是0。

对于驱动的使用者，如果不需要关心的模块内部寄存器信息，可以放在.c里面去定义。

## 目录组织规则（新增）

建议采用“每个驱动一个目录”的组织方式：

```text
src/em_driver/
    xxx/
        xxx.h
        xxx.c
        port_xxx.c        # 可选，隐式注入默认弱符号实现
        README.md         # 可选
        hw_test_plan.md   # 可选，建议提交硬件验证计划
```

兼容历史平铺结构（`src/em_driver/xxx.c`），但新驱动默认按目录方式组织，便于复杂驱动维护与定位。

## include 规则（新增）

禁止在新增代码中使用层级回溯 include（例如 `../`、`../../`）。统一使用工程根相对路径：

```c
#include <em_base/datatype.h>
#include <em_base/debug.h>
```

同模块内部优先使用本地头文件：

```c
#include "xxx.h"
```

说明：xmake 已统一配置 `src`、`src/em_base`、`src/em_driver` 的 include 搜索路径，以降低目录重构时的维护成本。

## 模式选择默认策略（新增）

1. 默认使用隐式注入（`LIBCA_XXX_PORT_MODE_EXTERN`）。
2. 当驱动硬件交互复杂、需要运行期替换端口、或需要更强可测性时，启用显式注入（`LIBCA_XXX_PORT_MODE_DYNAMIC`）。
3. 对外文档必须明确该驱动支持的注入模式与默认模式。
4. PR 中需说明为何选择该模式（默认隐式无需额外说明，选择显式建议补充理由）。

em_driver 的驱动通常不编写单元测试，因为驱动与硬件耦合紧密，很多行为无法在纯软件环境中复现或断言。**但要求在驱动发布时提供明确的硬件验证计划**（包含测试步骤、必要的测试设备或夹具、测试向量与验收标准）。如果驱动中存在纯逻辑的部分（例如数据解码、校验、状态机等），应将这些逻辑抽离到无硬件依赖的模块，并为这些模块编写单元测试（遵循 `prompt/em_code_rule.md` 的单元测试规范，使用 `TEST_ENABLE` 和 `em_test` 的 mock 能力）。同时建议在 PR 中附上集成或硬件测试的复现步骤与结果截图/日志，以便 review。

参考模板（驱动头文件模板、驱动实现模板、硬件验证计划模板、PR 检查清单）已放在 `.codebuddy/skills/em-driver-dev/assets`，供开发者直接复用或拷贝。

如果不需要对象参与的部分，不应该OOP风格，比如通用的计算工具函数这种，不依赖于OOP的驱动对象。

对于 `xxx_init` 的接口设计，如果是硬件的配置，不要用传参的形式进行初始化，而是要求用户自己用大括号法直接初始化好传入，而具体什么模式之类的配置再通过参数传入初始化。**重要规则**：当 `xxx_t` 的成员数量较多（例如 > 4）时，硬件资源相关的成员（如 `void* gpio`、`u16 pin`、`void* hi2c` 等）应由使用者在外部初始化（结构体字面量或逐项赋值），`xxx_init()` 应仅接收配置类型参数（例如 `u16 mode`）并负责对这些配置进行验证与必要的最小化初始化。这有助于避免过长的函数参数列表并提高 API 的可读性与一致性。
