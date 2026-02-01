# Common Driver Patterns

本文档提供 libca em_driver 模块中常见驱动模式的详细示例。每种模式都包含 Port 设计理由、Device object 结构、关键实现细节和使用示例。

## 目录

1. [Simple GPIO Driver (LED)](#1-simple-gpio-driver-led)
2. [I2C Sensor Driver (BH1750)](#2-i2c-sensor-driver-bh1750)
3. [Timing-Sensitive Driver (DHT11)](#3-timing-sensitive-driver-dht11)
4. [State Machine Driver (EC11 Encoder)](#4-state-machine-driver-ec11-encoder)
5. [EEPROM Driver (AT24CXX)](#5-eeprom-driver-at24cxx)

---

## 1. Simple GPIO Driver (LED)

### 概述

Simple GPIO 驱动控制单个引脚的输出状态。这是最简单的驱动模式，是理解 em_driver 框架的良好起点。

### Port 设计

```c
typedef struct led_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
} led_port_t;
```

**设计理由**:
- 只需要向 GPIO 引脚写入
- 无需读取（仅输出设备）
- 无时序要求
- 最小化硬件抽象

### Device Object

```c
typedef struct led {
    void* gpio;       // GPIO handle（平台特定）
    u16 pin;          // 引脚编号
    u8 active_level;  // 有效电平（0=低电平有效，1=高电平有效）
} led_t;
```

**成员说明**:
- `gpio`: 平台特定的 GPIO port handle（如 `GPIOA` 指针）
- `pin`: 引脚编号（如 `GPIO_PIN_5`）
- `active_level`: LED 用 HIGH 还是 LOW 信号打开

### API 函数

```c
void led_init(led_t* self, void* gpio, u16 pin, u8 active_level);
void led_on(led_t* self);
void led_off(led_t* self);
void led_toggle(led_t* self);
```

### 实现

```c
#include "../em_base/debug.h"

static const led_port_t* g_led_port = NULL;

void led_bind_port(const led_port_t* port) {
    g_led_port = port;
}

bool led_port_is_registered(void) {
    return g_led_port != NULL;
}

void led_init(led_t* self, void* gpio, u16 pin, u8 active_level) {
    self->gpio = gpio;
    self->pin = pin;
    self->active_level = active_level;
}

void led_on(led_t* self) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(g_led_port != NULL);
    g_led_port->write_pin(self->gpio, self->pin, self->active_level);
}

void led_off(led_t* self) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(g_led_port != NULL);
    g_led_port->write_pin(self->gpio, self->pin, !self->active_level);
}

void led_toggle(led_t* self) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(g_led_port != NULL);
    // 读取当前状态并切换
    // 注意：这需要 read_pin 功能，而 LED 没有
    // 替代方案：在 led_t 中跟踪状态
}
```

### 使用示例

```c
// 在 STM32 上使用 HAL
led_t status_led;
led_bind_port(&(led_port_t){
    .write_pin = HAL_GPIO_WritePin
});

led_init(&status_led, GPIOA, GPIO_PIN_5, 1);  // 高电平有效

led_on(&status_led);   // 打开
HAL_Delay(500);
led_off(&status_led);  // 关闭
```

### 关键要点

- 最简单的驱动模式
- 演示基本 Port 绑定
- 展示 `param_check` 使用
- 有效电平抽象提高灵活性

---

## 2. I2C Sensor Driver (BH1750)

### 概述

BH1750 是通过 I2C 通信的数字光传感器。它演示了如何处理 I2C 通信、设备寻址和基于寄存器的配置。

### Port 设计

```c
typedef struct bh1750_port {
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr,
                     u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr,
                    u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
} bh1750_port_t;
```

**设计理由**:
- 需要 I2C 读写操作
- 使用标准 HAL I2C 函数签名
- 支持多个 I2C 地址（0x46 或 0x47）
- 不需要时序函数（I2C 处理）

### Device Object

```c
typedef struct bh1750 {
    void* hi2c;       // I2C handle
    u16 dev_addr;     // I2C 设备地址（0x46 或 0x47）
} bh1750_t;
```

**成员说明**:
- `hi2c`: I2C 外设 handle（如 `&hi2c1`）
- `dev_addr`: 7 位 I2C 地址（取决于 ADDR 引脚状态）

### Access 宏

```c
#define BH1750_I2C_WRITE(reg, data, size) \
    g_bh1750_port->i2c_write((self)->hi2c, (self)->dev_addr, (reg), 1, \
                             (data), (size), 1000)

#define BH1750_I2C_READ(reg, buf, size) \
    g_bh1750_port->i2c_read((self)->hi2c, (self)->dev_addr, (reg), 1, \
                            (buf), (size), 1000)
```

### API 函数

```c
void bh1750_init(bh1750_t* self, void* hi2c, u16 dev_addr);
i32  bh1750_read_light(bh1750_t* self, f32* lux);
```

### 实现

```c
#include "../em_base/debug.h"
#include <math.h>

static const bh1750_port_t* g_bh1750_port = NULL;

void bh1750_bind_port(const bh1750_port_t* port) {
    g_bh1750_port = port;
}

void bh1750_init(bh1750_t* self, void* hi2c, u16 dev_addr) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(g_bh1750_port != NULL);

    self->hi2c = hi2c;
    self->dev_addr = dev_addr;

    // 上电并设置测量模式
    u8 cmd = 0x01;  // 上电
    g_bh1750_port->i2c_write(hi2c, dev_addr, 0, 0, &cmd, 1, 1000);
}

i32 bh1750_read_light(bh1750_t* self, f32* lux) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(lux != NULL);
    param_check(g_bh1750_port != NULL);
        debug_print("[bh1750] error: port not registered\n");
        return -1;
    }

    // 开始连续测量（高分辨率模式）
    u8 cmd = 0x10;
    g_bh1750_port->i2c_write(self->hi2c, self->dev_addr, 0, 0, &cmd, 1, 1000);

    // 等待测量
    //（I2C 轮询延迟由 I2C 实现处理）

    // 读取结果（2 字节）
    u8 data[2];
    g_bh1750_port->i2c_read(self->hi2c, self->dev_addr, 0, 0, data, 2, 1000);

    // 转换为 lux
    u16 raw = (data[0] << 8) | data[1];
    *lux = raw / 1.2f;  // datasheet 中的转换因子

    return 0;
}
```

### 使用示例

```c
bh1750 light_sensor;
bh1750_bind_port(&(bh1750_port_t){
    .i2c_write = HAL_I2C_Master_Transmit,
    .i2c_read = HAL_I2C_Master_Receive
});

bh1750_init(&light_sensor, &hi2c1, 0x46);  // ADDR 引脚低

f32 lux;
bh1750_read_light(&light_sensor, &lux);
printf("Light intensity: %.1f lux\n", lux);
```

### 关键要点

- 演示 I2C 通信模式
- 展示基于寄存器的设备控制
- 使用 Access 宏简化代码
- 从原始数据转换为物理单位

---

## 3. Timing-Sensitive Driver (DHT11)

### 概述

DHT11 是温度和湿度传感器，需要精确时序（微秒级延时）。它演示了 bit-banging 通信和双向 GPIO 使用。

### Port 设计

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

**设计理由**:
- **双向 GPIO**: 需要在输出和输入模式间切换
- **精确时序**: 协议需要微秒级延时
- **引脚控制**: 写入用于起始信号，读取用于数据位
- **毫秒延时**: 测量之间（传感器需要恢复时间）

### Device Object

```c
typedef struct dht11 {
    void* gpio;       // GPIO handle
    u16 pin;          // 引脚编号
    u8 last_valid;    // 上次读取有效
} dht11_t;
```

### Access 宏

```c
#define DHT11_OUTPUT_MODE(self) \
    g_dht11_port->set_output_mode((self)->gpio, (self)->pin)

#define DHT11_INPUT_MODE(self) \
    g_dht11_port->set_input_mode((self)->gpio, (self)->pin)

#define DHT11_WRITE(self, v) \
    g_dht11_port->write_pin((self)->gpio, (self)->pin, (v))

#define DHT11_READ(self) \
    g_dht11_port->read_pin((self)->gpio, (self)->pin)

#define DHT11_DELAY_US(us) \
    g_dht11_port->delay_us(us)

#define DHT11_DELAY_MS(ms) \
    g_dht11_port->delay_ms(ms)
```

### API 函数

```c
void dht11_init(dht11_t* self, void* gpio, u16 pin);
i32  dht11_read(dht11_t* self, u8* temp, u8* humi);
```

### 实现

```c
#include "../em_base/debug.h"

static const dht11_port_t* g_dht11_port = NULL;

void dht11_bind_port(const dht11_port_t* port) {
    g_dht11_port = port;
}

void dht11_init(dht11_t* self, void* gpio, u16 pin) {
    self->gpio = gpio;
    self->pin = pin;
    self->last_valid = 0;
}

i32 dht11_read(dht11_t* self, u8* temp, u8* humi) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(temp != NULL);
    param_check(humi != NULL);
    param_check(g_dht11_port != NULL);
        debug_print("[dht11] error: port not registered\n");
        return -1;
    }

    // 1. 发送起始信号（主机拉低 >18ms）
    DHT11_OUTPUT_MODE(self);
    DHT11_WRITE(self, 0);
    DHT11_DELAY_MS(20);

    // 2. 拉高并等待响应
    DHT11_WRITE(self, 1);
    DHT11_INPUT_MODE(self);

    // 等待传感器拉低（应该 ~20-40us）
    u32 timeout = 100;
    while (DHT11_READ(self) && timeout-- > 0) DHT11_DELAY_US(1);
    if (timeout == 0) {
        debug_print("[dht11] timeout waiting for sensor response\n");
        return -1;
    }

    // 等待传感器拉高（应该 ~80us）
    timeout = 100;
    while (!DHT11_READ(self) && timeout-- > 0) DHT11_DELAY_US(1);
    if (timeout == 0) {
        debug_print("[dht11] timeout waiting for sensor high\n");
        return -1;
    }

    // 3. 读取 40 位数据
    u8 data[5] = {0};
    for (u8 i = 0; i < 5; i++) {
        for (u8 j = 0; j < 8; j++) {
            // 等待位起始（低）
            timeout = 100;
            while (DHT11_READ(self) && timeout-- > 0) DHT11_DELAY_US(1);
            timeout = 100;
            while (!DHT11_READ(self) && timeout-- > 0) DHT11_DELAY_US(1);

            // 测量位持续时间
            DHT11_DELAY_US(40);  // 等待到位周期中途

            // 如果引脚高，是 '1'（长脉冲）
            if (DHT11_READ(self)) {
                data[i] |= (1 << (7 - j));
                // 等待剩余高时间
                timeout = 100;
                while (DHT11_READ(self) && timeout-- > 0) DHT11_DELAY_US(1);
            }
        }
    }

    // 4. 验证校验和
    if (data[4] != (data[0] + data[1] + data[2] + data[3])) {
        debug_print("[dht11] checksum error\n");
        return -1;
    }

    *humi = data[0];  // 湿度整数部分
    *temp = data[2];  // 温度整数部分
    self->last_valid = 1;

    return 0;
}
```

### 使用示例

```c
dht11 temp_sensor;
dht11_bind_port(&(dht11_port_t){
    .write_pin = HAL_GPIO_WritePin,
    .read_pin = HAL_GPIO_ReadPin,
    .set_output_mode = HAL_GPIO_SetOutputMode,
    .set_input_mode = HAL_GPIO_SetInputMode,
    .delay_us = HAL_Delay_us,
    .delay_ms = HAL_Delay
});

dht11_init(&temp_sensor, GPIOA, GPIO_PIN_6);

u8 temp, humi;
if (dht11_read(&temp_sensor, &temp, &humi) == 0) {
    printf("Temperature: %d°C, Humidity: %d%%\n", temp, humi);
}
```

### 关键要点

- 演示双向 GPIO 使用
- 展示微秒延时的精确时序控制
- 实现协议级位读取
- 包含校验和验证

---

## 4. State Machine Driver (EC11 Encoder)

### 概述

EC11 是需要状态机解码旋转方向的旋转编码器。它演示了如何维护内部状态和处理异步事件。

### Port 设计

```c
typedef struct ec11_port {
    u8 (*read_a)(void* gpio, u16 pin_a);  // 读取引脚 A
    u8 (*read_b)(void* gpio, u16 pin_b);  // 读取引脚 B
    void (*set_mode)(void* gpio, u16 pin_a, u16 pin_b, u8 mode);
} ec11_port_t;
```

**设计理由**:
- 两个输入引脚（A 和 B）带正交编码
- 只需要读操作（输入设备）
- 无时序要求（基于轮询）
- 状态机跟踪旋转方向

### Device Object

```c
typedef struct ec11 {
    void* gpio;       // GPIO handle
    u16 pin_a;        // 引脚 A
    u16 pin_b;        // 引脚 B
    u8 state;         // 当前状态（0-3）
    i16 count;        // 旋转计数器
} ec11_t;
```

**成员说明**:
- `gpio`: 两个引脚的 GPIO handle
- `pin_a`, `pin_b`: 正交信号的引脚编号
- `state`: 正交序列中的当前状态（0-3）
- `count`: 累积旋转计数（CW 正，CCW 负）

### API 函数

```c
void ec11_init(ec11_t* self, void* gpio, u16 pin_a, u16 pin_b);
void ec11_update(ec11_t* self);  // 定期调用
i16  ec11_get_count(ec11_t* self);
void ec11_reset_count(ec11_t* self);
```

### 实现

```c
#include "../em_base/debug.h"

static const ec11_port_t* g_ec11_port = NULL;

void ec11_bind_port(const ec11_port_t* port) {
    g_ec11_port = port;
}

void ec11_init(ec11_t* self, void* gpio, u16 pin_a, u16 pin_b) {
    self->gpio = gpio;
    self->pin_a = pin_a;
    self->pin_b = pin_b;
    self->state = 0;
    self->count = 0;

    // 配置引脚为输入
    if (g_ec11_port) {
        g_ec11_port->set_mode(gpio, pin_a, pin_b, 1);  // 1 = input
    }
}

void ec11_update(ec11_t* self) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(g_ec11_port != NULL);

    // 读取两个引脚的当前状态
    u8 a = g_ec11_port->read_a(self->gpio, self->pin_a);
    u8 b = g_ec11_port->read_b(self->gpio, self->pin_b);

    // 正交解码状态机
    u8 new_state = (a << 1) | b;

    // CW 旋转的状态转换：
    // 00 -> 01 -> 11 -> 10 -> 00
    // CCW 旋转的状态转换：
    // 00 -> 10 -> 11 -> 01 -> 00

    // 转换矩阵 [old_state][new_state]
    // -1: CCW, 0: 无效/无变化, +1: CW
    static const i8 transition_table[4][4] = {
        // new state: 00  01  11  10
        /* 00 */ { 0, -1,  0,  1},
        /* 01 */ { 1,  0, -1,  0},
        /* 11 */ { 0,  1,  0, -1},
        /* 10 */ {-1,  0,  1,  0}
    };

    i8 change = transition_table[self->state][new_state];
    self->count += change;
    self->state = new_state;
}

i16 ec11_get_count(ec11_t* self) {
    param_check(self != NULL);
    return self->count;
}

void ec11_reset_count(ec11_t* self) {
    param_check(self != NULL);
    self->count = 0;
}
```

### 使用示例

```c
ec11 rotary_encoder;
ec11_bind_port(&(ec11_port_t){
    .read_a = HAL_GPIO_ReadPin,
    .read_b = HAL_GPIO_ReadPin,
    .set_mode = HAL_GPIO_SetInputMode
});

ec11_init(&rotary_encoder, GPIOA, GPIO_PIN_7, GPIO_PIN_8);

// 在主循环中
while (1) {
    ec11_update(&rotary_encoder);
    i16 count = ec11_get_count(&rotary_encoder);
    if (count != 0) {
        printf("Encoder count: %d\n", count);
    }
    HAL_Delay(10);  // 以 100Hz 轮询
}
```

### 关键要点

- 演示状态机模式
- 展示内部状态维护
- 基于轮询的事件处理
- 正交信号解码

---

## 5. EEPROM Driver (AT24CXX)

### 概述

AT24CXX 是具有不同容量的 I2C EEPROM 系列。它演示了 I2C 通信与地址分页和写周期处理。

### Port 设计

```c
typedef struct at24c_port {
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr,
                     u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr,
                    u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    void (*delay_ms)(u32 ms);
} at24c_port_t;
```

**设计理由**:
- 标准的 I2C 读写操作
- `delay_ms` 需要写周期时间（EEPROM 需要 ~5-10ms 完成写入）
- 读取无时序要求

### Device Object

```c
typedef struct at24c {
    void* hi2c;       // I2C handle
    u16 dev_addr;     // 基本 I2C 地址
    u16 capacity;     // 总容量（字节）
    u16 page_size;    // 写入的页大小（如 32 字节）
} at24c_t;
```

### API 函数

```c
void at24c_init(at24c_t* self, void* hi2c, u16 dev_addr, u16 capacity, u16 page_size);
i32  at24c_read(at24c_t* self, u16 addr, u8* data, u16 len);
i32  at24c_write(at24c_t* self, u16 addr, const u8* data, u16 len);
```

### 实现

```c
#include "../em_base/debug.h"

static const at24c_port_t* g_at24c_port = NULL;

void at24c_bind_port(const at24c_port_t* port) {
    g_at24c_port = port;
}

void at24c_init(at24c_t* self, void* hi2c, u16 dev_addr, u16 capacity, u16 page_size) {
    self->hi2c = hi2c;
    self->dev_addr = dev_addr;
    self->capacity = capacity;
    self->page_size = page_size;
}

i32 at24c_read(at24c_t* self, u16 addr, u8* data, u16 len) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(g_at24c_port != NULL);
    param_check(len > 0);
    param_check(addr + len <= self->capacity);
        debug_print("[at24c] error: port not registered\n");
        return -1;
    }

    // 处理大 EEPROM 的 I2C 地址选择
    u16 i2c_addr = self->dev_addr;
    if (self->capacity > 256 * 1024) {
        // 使用地址位用于 >256KB 的设备
        i2c_addr |= (addr >> 16) & 0x07;
        addr &= 0xFFFF;
    } else if (self->capacity > 32 * 1024) {
        i2c_addr |= (addr >> 15) & 0x07;
        addr &= 0x7FFF;
    }

    return g_at24c_port->i2c_read(self->hi2c, i2c_addr, addr, 2, data, len, 1000);
}

i32 at24c_write(at24c_t* self, u16 addr, const u8* data, u16 len) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(g_at24c_port != NULL);
    param_check(len > 0);
    param_check(addr + len <= self->capacity);
        debug_print("[at24c] error: port not registered\n");
        return -1;
    }

    u16 written = 0;
    while (written < len) {
        // 计算页对齐的写入长度
        u16 page_start = (addr + written) & ~(self->page_size - 1);
        u16 page_end = page_start + self->page_size;
        u16 write_len = page_end - (addr + written);
        if (write_len > (len - written)) {
            write_len = len - written;
        }

        // 写入页
        g_at24c_port->i2c_write(self->hi2c, self->dev_addr, addr + written,
                                 2, (u8*)(data + written), write_len, 1000);

        // 等待写周期完成
        g_at24c_port->delay_ms(10);

        written += write_len;
    }

    return 0;
}
```

### 使用示例

```c
at24c eeprom;
at24c_bind_port(&(at24c_port_t){
    .i2c_write = HAL_I2C_Master_Transmit,
    .i2c_read = HAL_I2C_Master_Receive,
    .delay_ms = HAL_Delay
});

at24c_init(&eeprom, &hi2c1, 0xA0, 32768, 32);  // AT24C256, 32KB, 32-byte 页

// 写入一些数据
u8 write_data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
at24c_write(&eeprom, 0x1000, write_data, 10);

// 读回
u8 read_data[10];
at24c_read(&eeprom, 0x1000, read_data, 10);
```

### 关键要点

- 演示基于页的写入处理
- 展示大 EEPROM 的地址选择
- 包含写周期时序
- 处理边界条件（页对齐，容量检查）

---

## 模式选择指南

| 需求 | 推荐模式 | 示例 |
|------|-----------|------|
| 单输出控制 | Simple GPIO | LED, 继电器, 蜂鸣器 |
| 基于寄存器的传感器 | I2C Sensor | BH1750, MPU6050 |
| 精确时序协议 | Timing-Sensitive | DHT11, One-Wire |
| 正交输入 | State Machine | EC11 编码器 |
| 非易失存储 | EEPROM | AT24CXX |
| 复杂状态机 | State Machine + Timing | 射频模块 |

## 常见问题和解决方案

### 问题 1: Port 未注册

**症状**: `port not registered` 错误消息

**解决方案**: 使用任何 API 函数前始终调用 `xxx_bind_port()`

### 问题 2: 时序问题

**症状**: 读写失败，数据不正确

**解决方案**:
- 验证延时函数准确（尽可能使用示波器）
- 检查 datasheet 的最小/最大时序要求
- 考虑 MCU 间的硬件时序变化

### 问题 3: I2C 地址问题

**症状**: I2C 总线上 NACK

**解决方案**:
- 验证设备地址（7 位 vs 8 位）
- 检查硬件上的地址引脚
- 使用 I2C 扫描器检测总线上的设备

### 问题 4: 状态机毛刺

**症状**: 旋转方向错误或计数丢失

**解决方案**:
- 以适当频率轮询（太慢：丢失事件，太快：抖动）
- 如有必要添加去抖动
- 验证信号质量（示波器）

## 相关文档

- [Driver Development Workflow](./01_Driver_Development_Workflow.md) - 逐步指南
- [Design Principles](./03_Design_Principles.md) - 最佳实践
- [Driver Examples](./04_Driver_Examples.md) - 更详细分析
