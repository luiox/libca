# Driver Header 模板

```c
/**
 * @file xxx.h
 * @brief 简要描述功能
 * @author Your Name
 * @date YYYY-MM-DD
 */

#ifndef LIBCA_EM_DRIVER_XXX_H
#define LIBCA_EM_DRIVER_XXX_H

#include "../em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief port 层接口（硬件相关函数指针）
 */
typedef struct xxx_port {
    void (*write)(void* gpio, u16 pin, u8 val);
    u8   (*read)(void* gpio, u16 pin);
    void* context; // 可选的硬件上下文
} xxx_port_t;

/**
 * @brief 绑定 port
 */
void xxx_bind_port(const xxx_port_t* port);
bool xxx_port_is_registered(void);

/**
 * @brief 设备对象
 */
typedef struct xxx {
    u16 mode;
    void* gpio;
    u16 pin;
} xxx_t;

/**
 * @brief 初始化设备对象
 * @param self 设备对象指针
 * @param mode 初始化模式
 */
void xxx_init(xxx_t* self, u16 mode);

/**
 * @brief 示例操作
 * @param self 设备对象指针
 */
void xxx_do_something(xxx_t* self);

#ifdef __cplusplus
}
#endif

#endif // LIBCA_EM_DRIVER_XXX_H
```

说明：请替换 `xxx` 为模块名，`@brief` 等 doxygen 注释为中文说明，并使用头文件保护宏（禁止使用 `#pragma once`）。
