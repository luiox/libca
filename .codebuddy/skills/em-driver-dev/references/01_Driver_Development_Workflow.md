# Driver 开发流程

本文档提供完整的 em_driver 硬件驱动开发流程，以及维护时理解驱动框架设计的方法。它是新驱动开发和现有驱动分析的主要指南。

## 目录

1. [流程概览](#流程概览)
2. [Step 1: 理解硬件需求](#step-1-理解硬件需求)
3. [Step 2: 定义驱动结构](#step-2-定义驱动结构)
4. [Step 3: 实现驱动逻辑](#step-3-实现驱动逻辑)
5. [Step 4: 集成到构建系统](#step-4-集成到构建系统)
6. [Step 5: 测试方法](#step-5-测试方法)
7. [维护工作流程](#维护工作流程)

---

## 流程概览

```
┌─────────────────────────────────────────────────────────────┐
│                    Driver 开发周期                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   1. 分析硬件              ───┐                           │
│      └─ 通信接口                  │                           │
│      └─ Port 函数              │                           │
│      └─ 配置参数                │                           │
│                               │                           │
│   2. 定义结构             <──┤                           │
│      └─ Port 层                  │                           │
│      └─ Device object           │                           │
│      └─ API 函数                │                           │
│                               │                           │
│   3. 实现逻辑          <──┤                           │
│      └─ Debug 系统               │                           │
│      └─ Port 检查               │                           │
│      └─ 设备操作             │                           │
│                               │                           │
│   4. 集成构建         <──┤                           │
│      └─ xmake.lua                │                           │
│                               │                           │
│   5. 测试              <──┘                           │
│      └─ MCU 环境                                        │
│      └─ 模拟测试                                       │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Step 1: 理解硬件需求

### 1.1 识别通信接口

分析设备如何与 MCU 通信：

| 接口类型 | 特性 | 示例设备 |
|---------|------|----------|
| **GPIO** | 直接引脚控制，简单开关 | LED, 蜂鸣器, 继电器 |
| **I2C** | 两线串行，地址寻址 | 传感器, EEPROM, RTC |
| **SPI** | 四线高速串行 | Flash, 显示屏, 射频模块 |
| **UART** | 异步串行 | GPS 模块, 蓝牙 |
| **Custom Timing** | 精准时序的 Bit-banging | DHT11, 单线设备 |

### 1.2 确定需要的 Port 函数

为每种接口类型，识别所需的最少硬件操作：

**GPIO Interface**:
```c
void (*write_pin)(void* gpio, u16 pin, u8 value);
u8  (*read_pin)(void* gpio, u16 pin);
void (*set_mode)(void* gpio, u16 pin, u8 mode);  // output/input
```

**I2C Interface**:
```c
i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr,
                 u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr,
                u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
```

**Timing-Sensitive Interface**:
```c
void (*delay_us)(u32 us);
void (*delay_ms)(u32 ms);
```

### 1.3 识别设备配置参数

从 datasheet 中提取所有可配置方面：

- **硬件资源**: GPIO 引脚, I2C 地址, SPI 片选引脚
- **工作模式**: 测量范围, 分辨率, 电源模式
- **校准数据**: 偏移, 比例因子, 灵敏度
- **时序约束**: 最小延时, 超时值

**示例 - BH1750 光传感器**:
```c
// 硬件
void* hi2c;
u16 dev_addr;  // 0x46 或 0x47，取决于 ADDR 引脚

// 配置
u8 measurement_mode;  // 连续/单次, 分辨率
u8 mt_reg;           // 测量时间寄存器
```

---

## Step 2: 定义驱动结构

### 2.1 Port 层（硬件抽象）

定义 `xxx_port_t`，包含硬件操作的函数指针。

**设计规则**:
- 使用 `void*` 作为 handle 类型，避免平台依赖
- 只包含最少必要的函数
- 逻辑分组相关函数
- 保持 Port 结构小巧专注

**示例 - 简单 GPIO 驱动**:
```c
typedef struct led_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
} led_port_t;
```

**示例 - I2C 传感器驱动**:
```c
typedef struct bh1750_port {
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr,
                     u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr,
                    u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
} bh1750_port_t;
```

**示例 - 时序敏感驱动**:
```c
typedef struct dht11_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8  (*read_pin)(void* gpio, u16 pin);
    void (*set_output_mode)(void* gpio, u16 pin);
    void (*set_input_mode)(void* gpio, u16 pin);
    void (*delay_us)(u32 us);
    void (*delay_ms)(u32 ms);
} dht11_port_t;
```

### 2.2 Port 绑定函数

如果存在 Port 层，始终包含这两个函数：

```c
void xxx_bind_port(const xxx_port_t* port);
bool xxx_port_is_registered(void);
```

**实现模式**:
```c
// 全局 Port 指针
static const xxx_port_t* g_xxx_port = NULL;

// 注册 Port
void xxx_bind_port(const xxx_port_t* port) {
    g_xxx_port = port;
}

// 检查 Port 是否已注册
bool xxx_port_is_registered(void) {
    return g_xxx_port != NULL;
}
```

**为什么用这个模式?**:
- 单一全局 Port 对大部分驱动足够（共享硬件接口）
- 允许使用模拟 Port 轻松测试
- 通过 `port_is_registered()` 防止 NULL 解引用

### 2.3 Device Object (`xxx_t`)

定义设备结构，包含硬件资源和状态。

**成员组织**:
```c
typedef struct xxx {
    // 1. 硬件 handles（始终在前面）
    void* gpio;       // GPIO handle
    u16 pin;          // 引脚编号
    void* hi2c;       // I2C handle（如果需要）

    // 2. 配置参数
    u8 mode;          // 工作模式
    u16 timeout;      // 超时时间（毫秒）

    // 3. 状态变量（始终在后面）
    u8 initialized;
    u8 last_error;
} xxx_t;
```

**示例 - LED 驱动**:
```c
typedef struct led {
    void* gpio;       // 硬件: GPIO handle
    u16 pin;          // 硬件: 引脚编号
    u8 active_level;  // 配置: 有效电平（0 或 1）
} led_t;
```

**示例 - DHT11 传感器**:
```c
typedef struct dht11 {
    void* gpio;       // 硬件: GPIO handle
    u16 pin;          // 硬件: 引脚编号
    u8 last_valid;    // 状态: 上次读取有效
} dht11_t;
```

### 2.4 API 函数

遵循以下命名和设计约定：

#### 2.4.1 函数签名

**所有函数必须将 `xxx_t* self` 作为第一个参数**（OOP 风格）:

```c
void xxx_init(xxx_t* self, /* config params */);
i32  xxx_read(xxx_t* self, u8* data);
void xxx_reset(xxx_t* self);
```

#### 2.4.2 Init 函数

**始终包含 `xxx_init()`** - 即使它什么都不做：

```c
void xxx_init(xxx_t* self, void* gpio, u16 pin) {
    // 如果所有状态都在 self 中，无需初始化
    // 但函数必须存在以保持一致性
}
```

**如果需要初始化**:
```c
void xxx_init(xxx_t* self, void* gpio, u16 pin, u8 mode) {
    self->gpio = gpio;
    self->pin = pin;
    self->mode = mode;
    self->initialized = 1;
}
```

#### 2.4.3 错误码

定义错误码（如果适用）：

```c
#define XXX_OK                         0
#define XXX_ERR_PORT_NOT_REGISTERED   (-1)
#define XXX_ERR_INVALID_PARAM         (-2)
#define XXX_ERR_NOT_INITIALIZED       (-3)
#define XXX_ERR_TIMEOUT               (-4)

// 所有错误码必须为负数！
```

#### 2.4.4 Access 宏

定义 Access 宏以简化代码：

```c
// 用于基于 GPIO 的驱动
#define XXX_WRITE(self, v)    g_xxx_port->write_pin((self)->gpio, (self)->pin, (v))
#define XXX_READ(self)        g_xxx_port->read_pin((self)->gpio, (self)->pin)

// 用于时序
#define XXX_DELAY_US(us)      g_xxx_port->delay_us(us)

// 用于 I2C
#define XXX_I2C_READ(reg, buf, len) \
    g_xxx_port->i2c_read((self)->hi2c, (self)->dev_addr, (reg), \
                         1, (buf), (len), 1000)
```

**优势**:
- 实现代码更简洁
- 需要时易于修改 Port 调用
- 自文档化代码

---

## Step 3: 实现驱动逻辑

### 3.1 包含 Debug 系统

```c
#include "../em_base/debug.h"

// 使用 debug_print 记录日志
debug_print("[xxx] error: port not registered\n");

// 驱动内所有参数检查都用 param_check
param_check(self != NULL);
param_check(g_xxx_port != NULL);
```

### 3.2 检查 Port 注册

使用前始终验证 Port 已注册：

```c
i32 xxx_read(xxx_t* self, u8* data) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(g_xxx_port != NULL);

    // 实现
    // ...
}
```

### 3.3 实现设备操作

通过 Access 宏使用 Port 函数：

```c
i32 xxx_read_data(xxx_t* self, u8* buffer) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(buffer != NULL);
    param_check(g_xxx_port != NULL);

    // 向设备写入命令
    XXX_WRITE(self, 1);   // 触发读取

    // 等待设备
    XXX_DELAY_US(10);

    // 读取数据
    *buffer = XXX_READ(self);

    return XXX_OK;
}
```

### 3.4 错误处理模式

```c
i32 xxx_operation(xxx_t* self, u8* data) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(g_xxx_port != NULL);

    // 检查初始化状态（不是参数检查，可以用 if）
    if (!self->initialized) {
        debug_print("[xxx] error: not initialized\n");
        return XXX_ERR_NOT_INITIALIZED;
    }

    // 实现操作
    // ... 驱动特定逻辑

    return XXX_OK;
}
```

---

## Step 4: 集成到构建系统

### 4.1 将驱动添加到 xmake.lua

编辑 `src/em_driver/xmake.lua` 以包含新驱动：

```lua
-- 添加驱动 target
target("libca.em_driver.xxx")
    add_files("xxx.c")
    add_headerfiles("xxx.h", {prefixdir = "libca/em_driver"})

-- 确保包含在主 em_driver target 中
-- （如果使用组合库）
```

### 4.2 构建验证

```bash
# 构建驱动
xmake build libca.em_driver.xxx

# 构建整个 em_driver 模块
xmake build libca.em_driver
```

### 4.3 头文件组织

确保 `xxx.h` 格式正确：

```c
#ifndef LIBCA_EM_DRIVER_XXX_H
#define LIBCA_EM_DRIVER_XXX_H

#include "em_base/datatype.h"

// 前向声明
typedef struct xxx_port xxx_port_t;
typedef struct xxx xxx_t;

// Port 定义
struct xxx_port {
    // 函数指针...
};

// Device 定义
struct xxx {
    // 成员...
};

// Port 绑定
void xxx_bind_port(const xxx_port_t* port);
bool xxx_port_is_registered(void);

// API 函数
void xxx_init(xxx_t* self, /* params */);
i32  xxx_read(xxx_t* self, u8* data);

// 错误码
#define XXX_OK       0
// ...

#endif // LIBCA_EM_DRIVER_XXX_H
```

---

## Step 5: 测试方法

### 5.1 重要：驱动不需要单元测试

**em_driver 模块不需要单元测试**，因为它们依赖硬件。

取而代之，通过以下方式测试：
1. **MCU 环境**: 绑定真实 HAL 函数，在硬件上测试
2. **模拟环境**: 绑定模拟函数以验证逻辑

### 5.2 模拟测试

创建模拟环境以在无硬件情况下测试驱动逻辑：

```c
// 模拟 Port 函数
static u8 simulated_pin_state = 0;

void sim_write_pin(void* gpio, u16 pin, u8 value) {
    simulated_pin_state = value;
    printf("[SIM] GPIO %c%d = %d\n", (char)gpio, pin, value);
}

u8 sim_read_pin(void* gpio, u16 pin) {
    printf("[SIM] GPIO %c%d read -> %d\n", (char)gpio, pin, simulated_pin_state);
    return simulated_pin_state;
}

void sim_delay_us(u32 us) {
    printf("[SIM] delay_us(%d)\n", us);
}

// 绑定模拟 Port
xxx_port_t sim_port = {
    .write_pin = sim_write_pin,
    .read_pin = sim_read_pin,
    .delay_us = sim_delay_us
};

// 测试驱动
xxx_t my_device;
xxx_init(&my_device, (void*)"A", 5);
xxx_bind_port(&sim_port);

// 测试操作
xxx_write(&my_device, 1);
u8 value = xxx_read(&my_device);
```

### 5.3 硬件测试

在实际 MCU 上：

```c
// 绑定真实 HAL 函数
xxx_port_t hal_port = {
    .write_pin = HAL_GPIO_WritePin,
    .read_pin = HAL_GPIO_ReadPin,
    .delay_us = HAL_Delay_us
};
xxx_bind_port(&hal_port);

// 使用真实设备测试
xxx_t sensor;
xxx_init(&sensor, GPIOA, GPIO_PIN_5);
xxx_read(&sensor, &data);
```

### 5.4 测试清单

- [ ] Port 注册检查
- [ ] 使用 `param_check` 验证参数
- [ ] 所有 API 函数正常工作
- [ ] 恰当返回错误码
- [ ] 设备在 datasheet 规范内工作
- [ ] 多实例工作（如果支持）

---

## 维护工作流程

维护或分析现有驱动时：

### 1. 理解设计

使用此工作流程理解任何 em_driver：

1. **检查 Port 结构** (`xxx_port_t`):
   - 需要哪些硬件操作？
   - 使用什么通信接口？

2. **检查 Device object** (`xxx_t`):
   - 需要哪些硬件资源？
   - 存在哪些配置选项？
   - 维护什么状态？

3. **查看 API 函数**:
   - 支持哪些操作？
   - 如何进行错误处理？
   - 有任何 Access 宏吗？

4. **研究实现**:
   - 如何使用 Port 层？
   - 有什么时序要求？
   - 有任何状态机吗？

### 2. 常见维护任务

#### 添加新功能

遵循此工作流程的 Step 1-4：
1. 分析新硬件需求
2. 更新设备结构和 API
3. 实现新逻辑
4. 先模拟测试，再硬件测试

#### 调试问题

1. **检查 Port 注册**: `xxx_port_is_registered()` 是否返回 true？
2. **验证参数**: 所有 `param_check` 调用是否通过？
3. **审查时序**: 延时是否足够？
4. **检查状态**: 查看内部状态变量

#### 移植到新平台

1. 使用新 HAL 实现 Port 函数
2. 用 `xxx_bind_port()` 绑定新 Port
3. 在新硬件上测试

---

## 快速参考

### 常见文件结构

```
xxx.h:
  - 前向声明
  - Port 结构（xxx_port_t）
  - Device 结构（xxx_t）
  - Port 绑定函数
  - API 声明
  - 错误码

xxx.c:
  - 全局 Port 指针
  - Port 绑定实现
  - Access 宏
  - API 实现
```

### 核心模式

```c
// Port 模式
static const xxx_port_t* g_xxx_port = NULL;

void xxx_bind_port(const xxx_port_t* port) {
    g_xxx_port = port;
}

// 驱动内所有参数检查都用 param_check
param_check(self != NULL);
param_check(data != NULL);
param_check(g_xxx_port != NULL);

// Access 模式
XXX_WRITE(self, 1);
value = XXX_READ(self);
```

### 相关文档

- [Common Driver Patterns](./02_Common_Driver_Patterns.md) - 详细示例
- [Design Principles](./03_Design_Principles.md) - 最佳实践和指南
- [Driver Examples](./04_Driver_Examples.md) - 现有驱动分析
- [中文规范文档](./05_Specification_CN.md) - 完整中文规范
