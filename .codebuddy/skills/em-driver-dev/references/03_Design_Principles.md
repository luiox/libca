# Design Principles and Best Practices

本文档提供 em_driver 开发的设计原则、命名规范和最佳实践。它是做出一致设计决策和避免常见陷阱的参考。

## 目录

1. [命名规范](#命名规范)
2. [OOP 风格使用](#oop-风格使用)
3. [Port 层设计原则](#port-层设计原则)
4. [错误处理模式](#错误处理模式)
5. [内存管理](#内存管理)
6. [代码组织](#代码组织)
7. [常见陷阱](#常见陷阱)
8. [性能考虑](#性能考虑)

---

## 命名规范

### 类型名称

| 实体 | 约定 | 示例 |
|-----|-------|------|
| Device object | `xxx_t` | `led_t`, `bh1750_t`, `dht11_t` |
| Port structure | `xxx_port_t` | `led_port_t`, `bh1750_port_t` |
| Handle types | 使用平台 handles | `void* gpio`, `void* hi2c` |

### 函数

| 函数类型 | 约定 | 示例 |
|---------|-------|------|
| Port 绑定 | `xxx_bind_port()` | `led_bind_port()` |
| Port 检查 | `xxx_port_is_registered()` | `led_port_is_registered()` |
| 初始化 | `xxx_init()` | `led_init()` |
| 操作 | `xxx_operation()` | `led_on()`, `bh1750_read()` |

**函数签名模式**:
```c
// 所有 API 函数以 prefix + action 开始
void xxx_init(xxx_t* self, ...);
i32  xxx_read(xxx_t* self, ...);
void xxx_reset(xxx_t* self);
```

### 宏

| 宏类型 | 约定 | 示例 |
|--------|-------|------|
| Access 宏 | `XXX_OPERATION()` | `XXX_WRITE()`, `XXX_READ()` |
| 错误码 | `XXX_ERR_NAME` | `XXX_ERR_TIMEOUT`, `XXX_ERR_NULL` |
| 成功码 | `XXX_OK` | `BH1750_OK` |

**宏全部使用大写！**

### 错误码

| 规则 | 示例 |
|------|------|
| 错误使用负数 | `#define XXX_ERR_TIMEOUT (-1)` |
| 成功使用 0 | `#define XXX_OK 0` |
| 分组相关错误 | `#define XXX_ERR_I2C_NACK (-2)` |
| 描述性命名 | `#define XXX_ERR_NOT_INITIALIZED (-3)` |

---

## OOP 风格使用

### 何时使用 OOP 风格

**在以下情况使用 OOP 风格**:
- 驱动管理设备状态（配置、模式、数据）
- 需要多个相同设备实例
- 设备具有关联的硬件资源（引脚、handles）

**示例**:
```c
// OOP 风格 - 适用于有状态设备
led_t led1, led2;
led_init(&led1, GPIOA, GPIO_PIN_5, 1);
led_init(&led2, GPIOA, GPIO_PIN_6, 0);
led_on(&led1);
led_off(&led2);

bh1750 sensor1, sensor2;
bh1750_init(&sensor1, &hi2c1, 0x46);
bh1750_init(&sensor2, &hi2c1, 0x47);
```

### 何时不使用 OOP 风格

**在以下情况不使用 OOP 风格**:
- 函数是纯工具函数（无状态）
- 单一全局资源（只能有一个实例）
- 计算/转换函数

**示例**:
```c
// 工具函数 - 不需要 OOP
u32 calculate_crc(const u8* data, u32 length) {
    u32 crc = 0xFFFFFFFF;
    for (u32 i = 0; i < length; i++) {
        crc ^= data[i];
        for (u8 j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return crc;
}

// 使用 - 直接调用
u32 crc_value = calculate_crc(data_buffer, data_length);
```

### OOP 函数设计

**所有 API 函数必须将 `xxx_t* self` 作为第一个参数**:

```c
// 正确 - OOP 风格
void xxx_function(xxx_t* self, u8 param1, u16 param2) {
    param_check(self != NULL);
    // 实现
}

// 不正确 - 过程式风格
void xxx_function(u8 param1, u16 param2) {
    // 实现
}
```

---

## Port 层设计原则

### 原则 1: 最小化硬件抽象

**规则**: Port 层中只包含最少必要的函数。

**原因**:
- 减少对特定 HAL 实现的耦合
- 使驱动测试更容易
- 保持 Port 结构简单

**示例 - 好**:
```c
typedef struct led_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);  // 只需要的功能
} led_port_t;
```

**示例 - 差**:
```c
typedef struct led_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    void (*read_pin)(void* gpio, u16 pin);            // 不需要！
    void (*toggle_pin)(void* gpio, u16 pin);          // 不需要！
    void (*init_gpio)(void* gpio);                    // 不需要！
} led_port_t;
```

### 原则 2: 使用 void* 作为 Handles

**规则**: 使用 `void*` 作为硬件 handles 以避免平台依赖。

**原因**:
- 驱动代码保持平台无关
- 可与不同 HAL 实现工作
- 更灵活

**示例**:
```c
typedef struct bh1750 {
    void* hi2c;       // 可用于任何 I2C handle 类型
    u16 dev_addr;
} bh1750_t;

// 使用 STM32 HAL
bh1750_init(&sensor, &hi2c1, 0x46);

// 使用 Arduino
bh1750_init(&sensor, &Wire, 0x46);

// 使用自定义实现
bh1750_init(&sensor, my_i2c_handle, 0x46);
```

### 原则 3: 分组相关函数

**规则**: 逻辑分组 Port 函数。

**示例**:
```c
typedef struct dht11_port {
    // GPIO 控制
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8  (*read_pin)(void* gpio, u16 pin);

    // GPIO 配置
    void (*set_output_mode)(void* gpio, u16 pin);
    void (*set_input_mode)(void* gpio, u16 pin);

    // 时序
    void (*delay_us)(u32 us);
    void (*delay_ms)(u32 ms);
} dht11_port_t;
```

### 原则 4: 单一全局 Port

**规则**: 大部分驱动使用单一全局 Port 指针。

**原因**:
- 实现更简单
- 减少内存使用
- 大部分设备共享相同的通信接口

**模式**:
```c
static const xxx_port_t* g_xxx_port = NULL;

void xxx_bind_port(const xxx_port_t* port) {
    g_xxx_port = port;
}

bool xxx_port_is_registered(void) {
    return g_xxx_port != NULL;
}
```

**例外**: 如果驱动真正需要多个独立的 Port（罕见），可以在每次函数调用时传递 port。

---

## 错误处理模式

### 模式 1: Port 未注册

使用前始终检查 Port 是否已注册：

```c
i32 xxx_read(xxx_t* self, u8* data) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(g_xxx_port != NULL);

    // 继续实现
    // ...
}
```

### 模式 2: 参数验证

**驱动内所有参数检查都用 `param_check`**：
- **self 指针**：`param_check(self != NULL)`
- **g_xxx_port**：`param_check(g_xxx_port != NULL)`
- **data/buffer 指针**：`param_check(data != NULL)`
- **length/size**：`param_check(length > 0)`

驱动就是要快，用户不正确调用直接让他死。

```c
i32 xxx_read(xxx_t* self, u8* data, u16 length) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(g_xxx_port != NULL);
    param_check(length > 0);

    // 继续实现
    // ...
}
```

### 模式 3: 初始化状态

检查设备是否已初始化：

```c
i32 xxx_read(xxx_t* self, u8* data) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(g_xxx_port != NULL);

    // 检查初始化状态（不是参数检查，可以用 if）
    if (!self->initialized) {
        debug_print("[xxx] error: not initialized\n");
        return XXX_ERR_NOT_INITIALIZED;
    }

    // 继续实现
    // ...
}
```

### 模式 4: 硬件错误

为硬件问题返回适当的错误码：

```c
i32 xxx_read(xxx_t* self, u8* data, u32 timeout_ms) {
    // ... 验证 ...

    // 执行硬件操作
    u32 start_time = get_tick();
    while (!xxx_is_data_ready()) {
        if (get_tick() - start_time > timeout_ms) {
            debug_print("[xxx] error: timeout\n");
            return XXX_ERR_TIMEOUT;
        }
    }

    *data = xxx_read_data();
    return XXX_OK;
}
```

### 错误处理清单

- [ ] Port 已注册？
- [ ] 参数有效？
- [ ] 设备已初始化？
- [ ] 硬件操作超时？
- [ ] 返回有意义的错误码？
- [ ] 使用 debug_print 记录错误？

---

## 内存管理

### 原则 1: 无动态分配

**规则**: 驱动中不要使用 `malloc`、`free` 或任何动态内存分配。

**原因**:
- 嵌入式系统堆有限
- 避免碎片问题
- 确定性行为

**正确**:
```c
// 调用者分配内存
u8 buffer[128];
xxx_read(&device, buffer, sizeof(buffer));
```

**不正确**:
```c
// 驱动分配内存 - 避免这样做
u8* xxx_read(void) {
    u8* buffer = malloc(128);  // 不要这样做！
    // ...
    return buffer;
}
```

### 原则 2: 栈分配

**规则**: 对临时缓冲区使用栈分配。

**示例**:
```c
i32 xxx_read_register(xxx_t* self, u8 reg, u8* value) {
    // 临时数据的栈分配
    u8 buffer[2] = {reg, 0};  // 寄存器地址 + dummy

    // 使用缓冲区
    xxx_write_bytes(self, buffer, 2);
    *value = buffer[1];

    return XXX_OK;
}
```

### 原则 3: 调用者提供内存

**规则**: 让调用者为数据提供缓冲区。

**示例**:
```c
// 函数签名 - 调用者提供缓冲区
i32 xxx_read_data(xxx_t* self, u8* buffer, u16 max_len);

// 使用 - 调用者管理内存
u8 data[256];
u16 len = xxx_read_data(&device, data, sizeof(data));
```

---

## 代码组织

### 文件结构

```
xxx.h:
  1. 许可证和头文件守卫
  2. Includes
  3. 前向声明
  4. Port 结构定义
  5. Device 结构定义
  6. Port 绑定函数
  7. API 函数声明
  8. 错误码
  9. 头文件守卫结束

xxx.c:
  1. Includes
  2. 静态全局 Port 指针
  3. Port 绑定实现
  4. Access 宏（如果有）
  5. API 函数实现
```

### 头文件模板

```c
#ifndef LIBCA_EM_DRIVER_XXX_H
#define LIBCA_EM_DRIVER_XXX_H

#include "em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

// 前向声明
typedef struct xxx_port xxx_port_t;
typedef struct xxx xxx_t;

// Port 结构
struct xxx_port {
    // 函数指针...
};

// Device 结构
struct xxx {
    // 成员...
};

// Port 绑定
void xxx_bind_port(const xxx_port_t* port);
bool xxx_port_is_registered(void);

// API 函数
void xxx_init(xxx_t* self, ...);
i32  xxx_read(xxx_t* self, ...);

// 错误码
#define XXX_OK       0
#define XXX_ERR_XXX (-1)

#ifdef __cplusplus
}
#endif

#endif // LIBCA_EM_DRIVER_XXX_H
```

### 实现文件模板

```c
#include "../em_base/debug.h"
#include "xxx.h"

// 全局 Port 指针
static const xxx_port_t* g_xxx_port = NULL;

// Access 宏（可选）
#define XXX_WRITE(v) g_xxx_port->write((v))

// Port 绑定
void xxx_bind_port(const xxx_port_t* port) {
    g_xxx_port = port;
}

bool xxx_port_is_registered(void) {
    return g_xxx_port != NULL;
}

// API 实现
void xxx_init(xxx_t* self, ...) {
    param_check(self != NULL);
    // ...
}
```

---

## 常见陷阱

### 陷阱 1: 忘记绑定 Port

**症状**: `port not registered` 错误，崩溃

**解决方案**: 使用驱动前始终调用 `xxx_bind_port()`

```c
// 不正确 - 忘记绑定
xxx_t device;
xxx_init(&device, ...);
xxx_read(&device, &data);  // 错误！

// 正确 - 使用前绑定
xxx_t device;
xxx_bind_port(&port);
xxx_init(&device, ...);
xxx_read(&device, &data);  // OK
```

### 陷阱 2: 使用错误的延时精度

**症状**: 时序敏感设备失败（DHT11, One-Wire）

**解决方案**: 使用微秒延时获得精确时序

```c
// 不正确 - 微秒时序使用毫秒延时
delay_ms(1);  // 对 DHT11 来说太粗糙

// 正确 - 微秒延时
delay_us(40);  // 适当精度
```

### 陷阱 3: 忽略错误码

**症状**: 静默失败，行为不正确

**解决方案**: 始终检查返回值

```c
// 不正确 - 忽略错误
xxx_read(&device, &data);
process(data);  // 数据可能无效

// 正确 - 检查错误
i32 result = xxx_read(&device, &data);
if (result == XXX_OK) {
    process(data);
} else {
    handle_error(result);
}
```

### 陷阱 4: 不验证 I2C 地址

**症状**: I2C NACK，通信失败

**解决方案**: 验证 7 位 vs 8 位地址

```c
// 不正确 - 使用 8 位地址与 HAL
u16 addr = 0xA0;  // 8 位地址
HAL_I2C_Master_Transmit(hi2c, addr, ...);  // 错误！

// 正确 - 转换为 7 位地址
u16 addr = 0xA0 >> 1;  // 7 位地址
HAL_I2C_Master_Transmit(hi2c, addr, ...);  // 正确
```

### 陷阱 5: 不检查数组边界

**症状**: 缓冲区溢出，崩溃

**解决方案**: 始终验证长度

```c
// 不正确 - 无边界检查
i32 xxx_write(xxx_t* self, u8* data, u16 len) {
    for (u16 i = 0; i < len; i++) {
        self->buffer[i] = data[i];  // 溢出风险！
    }
    return XXX_OK;
}

// 正确 - 检查边界（驱动内全部用 param_check）
i32 xxx_write(xxx_t* self, u8* data, u16 len) {
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(g_xxx_port != NULL);
    param_check(len <= self->buffer_size);

    for (u16 i = 0; i < len; i++) {
        self->buffer[i] = data[i];
    }
    return XXX_OK;
}
```

### 陷阱 6: 忘记写周期时间（EEPROM）

**症状**: 数据未正确写入，读回旧数据

**解决方案**: 等待写周期完成

```c
// 不正确 - 写入后无延时
at24c_write(&eeprom, addr, data, len);
at24c_read(&eeprom, addr, read_buf, len);  // 数据尚未写入！

// 正确 - 等待写周期
at24c_write(&eeprom, addr, data, len);
delay_ms(10);  // 等待写周期
at24c_read(&eeprom, addr, read_buf, len);  // OK
```

---

## 性能考虑

### 优化 I2C 操作

**问题**: 多次小的 I2C 事务很慢

**解决方案**: 尽可能组合操作

```c
// 慢 - 多次事务
xxx_write_register(device, REG_CONFIG, config);
xxx_write_register(device, REG_MODE, mode);
xxx_write_register(device, REG_THRESHOLD, threshold);

// 快 - 单次事务
u8 buffer[3] = {REG_CONFIG, REG_MODE, REG_THRESHOLD};
u8 values[3] = {config, mode, threshold};
xxx_write_multi(device, buffer, values, 3);
```

### 缓存计算值

**问题**: 重复计算浪费 CPU 时间

**解决方案**: 尽可能缓存结果

```c
// 慢 - 每次重新计算
f32 get_temperature(xxx_t* self) {
    u16 raw = read_raw_adc();
    return (raw * 3.3f / 4096.0f - 0.5f) * 100.0f;  // 复杂数学
}

// 快 - 缓存上次值
f32 get_temperature(xxx_t* self) {
    if (self->cache_valid) {
        return self->cached_temp;  // 使用缓存值
    }
    u16 raw = read_raw_adc();
    self->cached_temp = (raw * 3.3f / 4096.0f - 0.5f) * 100.0f;
    self->cache_valid = 1;
    return self->cached_temp;
}
```

### 最小化延时

**问题**: 不必要的延时阻塞执行

**解决方案**: 尽可能使用轮询代替固定延时

```c
// 慢 - 固定延时
xxx_start_conversion(device);
delay_ms(50);  // 始终等待 50ms
u8 data = xxx_read_data(device);

// 快 - 轮询就绪
xxx_start_conversion(device);
while (!xxx_is_ready(device));  // 只等待需要的时长
u8 data = xxx_read_data(device);
```

### 小操作使用内联函数

**问题**: 简单操作的函数调用开销

**解决方案**: 对小函数使用 inline 或宏

```c
// 慢 - 函数调用开销
void xxx_set_bit(xxx_t* self, u8 bit) {
    self->reg |= (1 << bit);
}

// 快 - inline
static inline void xxx_set_bit(xxx_t* self, u8 bit) {
    self->reg |= (1 << bit);
}

// 或使用宏
#define XXX_SET_BIT(self, bit) ((self)->reg |= (1 << (bit))))
```

---

## 设计清单

在完成驱动之前，验证：

**结构**:
- [ ] Port 结构只包含必要函数
- [ ] Device object 遵循成员组织（handles, config, state）
- [ ] 所有 API 函数将 `xxx_t* self` 作为第一个参数
- [ ] Port 绑定函数存在

**命名**:
- [ ] 类型使用 `xxx_t` 后缀
- [ ] 函数使用 `xxx_` 前缀
- [ ] 宏是大写
- [ ] 错误码为负数（成功为 0 除外）

**错误处理**:
- [ ] 使用前检查 Port 注册
- [ ] 使用 `param_check` 验证参数
- [ ] 检查初始化状态（如果适用）
- [ ] 返回有意义的错误码
- [ ] 使用 `debug_print` 记录错误

**内存**:
- [ ] 无动态分配（malloc/free）
- [ ] 临时缓冲区使用栈分配
- [ ] 调用者提供数据缓冲区

**文档**:
- [ ] 头文件有清晰的注释
- [ ] 函数参数已文档化
- [ ] 错误码已解释
- [ ] 提供使用示例

## 相关文档

- [Driver Development Workflow](./01_Driver_Development_Workflow.md) - 逐步指南
- [Common Driver Patterns](./02_Common_Driver_Patterns.md) - 详细示例
- [Driver Examples](./04_Driver_Examples.md) - 现有驱动分析
