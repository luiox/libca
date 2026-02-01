# 02_design_principle — 设计原则与最佳实践

本文件对驱动开发中的设计决策给出具体、可操作的原则与示例，便于实现一致性与可维护性。适用于 em_driver 下的新驱动设计与现有驱动重构。

## 目录
1. OOP 风格使用
2. Port 层设计原则
3. 错误处理模式
4. 内存管理
5. 代码组织与文件结构
6. 调试与日志约定
7. 常见陷阱与防御
8. 性能考虑
9. Init 接口约定
10. Reviewer 快速检查清单

---

## 1. OOP 风格使用
- 何时使用：当设备需要维护状态、支持多实例或涉及多种配置时采用 OOP 风格（每个对象用 `xxx_t` 表示）。
- 何时不使用：纯计算函数、无状态工具函数或单一全局资源不应使用 OOP。
- 要求：所有对外 API 的第一个参数必须为 `xxx_t* self`。

示例：
```c
void led_init(led_t* self, void* gpio, u16 pin, u8 active_level);
i32  bh1750_read(bh1750_t* self, f32* lux);
```

设计提示：将设备的硬件句柄放在结构体中（`void* gpio`, `void* hi2c` 等），将配置与状态分组，状态变量放在结构体的末尾。

## 2. Port 层设计原则
- 最小化硬件抽象：Port（`xxx_port_t`）只包含驱动运行所需的最少函数。避免把所有 HAL 接口全部暴露到 Port 中。
- 使用 `void*` 作为 handle：使驱动与具体平台 HAL 解耦。
- 分组相关函数：把 GPIO、I2C、时序函数等逻辑分组到同一 Port 结构内。
- 单一全局 Port：默认采用单一全局 Port 指针并提供 `xxx_bind_port()` / `xxx_port_is_registered()`。
- Access 宏：通过宏封装 Port 调用，便于修改与阅读，例如 `#define XXX_WRITE(self, v) g_xxx_port->write((self)->gpio, (self)->pin, (v))`。

示例：
```c
typedef struct dht11_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8   (*read_pin)(void* gpio, u16 pin);
    void (*delay_us)(u32 us);
} dht11_port_t;

static const dht11_port_t* g_dht11_port = NULL;
```

例外情况：若确实需要多个独立 Port，可在 API 中显式传递 Port 指针，但需在文档中说明理由。

## 3. Port Init 接口约定

- 规则：当 `xxx_t` 的成员较多（建议阈值 > 4）时，硬件相关字段应由使用者在外部初始化，`xxx_init()` 仅接收配置类参数并进行验证与最小初始化。

示例：

```c
// 使用者在外部初始化硬件字段
bh1750_t sensor = { .hi2c = &hi2c1, .dev_addr = 0x46 };
// 仅通过 init 传入配置参数
bh1750_init(&sensor, BH1750_MODE_CONTINUOUS);
```

实现建议：在 `xxx_init()` 内使用 `param_check(self != NULL)` 并校验关键配置，避免重复初始化。

## 4. 错误处理模式

- 参数检查：针对 `self` 使用 `param_check(self != NULL)`（契约式），其他参数根据上下文使用 if 返回错误码。
- 初始化检查：在运行时检查 `self->initialized` 或 `xxx_port_is_registered()` 并返回模块错误码（负数）。
- 超时与重试：在时序或通信操作中实现超时检查并返回明确错误码，例如 `*_ERR_TIMEOUT`。
- 错误码约定：以 `MODULE_ERR_*` 或 `MODULENAME_ERR_*` 命名，错误值必须为负；成功用 `MODULE_OK`（0）。

示例：
```c
i32 xxx_read(xxx_t* self, u8* data) {
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(g_xxx_port != NULL);

    if (!self->initialized) return XXX_ERR_NOT_INITIALIZED;
    // ...
}
```

## 5. 内存管理
- 禁止动态分配：驱动中应避免使用 `malloc` / `free`，以确保确定性与低内存碎片。
- 临时缓冲区优先使用栈分配，调用者为较大缓冲区分配内存。

## 6. 代码组织与文件结构
- 头文件（`xxx.h`）包含类型定义（port 与 device）、公共 API、错误码与 doxygen 注释。
- 源文件（`xxx.c`）包含 Port 绑定、Access 宏、API 实现与私有静态函数。
- 私有函数使用 `static` 修饰。
- 头文件禁止使用 `#pragma once`，使用 include guard（`LIBCA_EM_DRIVER_XXX_H` 风格）。

推荐结构：
```
xxx.h  // 类型、API、错误码
xxx.c  // 实现、静态函数、单元测试（#if TEST_ENABLE）
```

## 7. 调试与日志约定
- 禁止使用 `stdio.h` 的 `printf/scanf` 等输出调试信息。日志必须使用 `em_base/debug.h` 提供的接口（例如 `debug_print`）。
- 日志等级：使用适当的等级与模块前缀，例如 `[bh1750] info: ...`，避免在常规执行路径打印大量日志。

示例：
```c
#include "../em_base/debug.h"

debug_print("[xxx] error: port not registered\n");
```

## 8. 常见陷阱与防御
- 时序敏感代码：在 bit-banging 实现中严格遵守 datasheet，避免在中断中进行大量延时。
- Port 注册未检查：始终在 API 开始处检查 `g_xxx_port` 是否已注册。
- 忽略边界条件：所有长度/索引等使用 `usize` 并检查上界。
- 全局变量冲突：全局变量需 `g_` 前缀并在头文件注明可见性。

## 9. 性能考虑
- 对热路径避免使用昂贵的浮点操作；必要时使用整数替代或预计算表。
- 小函数可用 `static inline` 优化，但谨慎使用以免代码膨胀。
- 避免在中断上下文中执行复杂计算或阻塞调用。

