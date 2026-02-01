# 驱动开发示例分析

本文档分析 libca 项目中现有的驱动实现，展示不同类型驱动的设计模式和最佳实践。

---

## 示例 1: LED 驱动（简单 GPIO 输出）

### 特点
- 最简单的驱动模式
- 只涉及 GPIO 写入操作
- 支持高/低电平有效配置
- 维护 LED 状态

### Port 设计

```c
typedef struct led_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
} led_port_t;
```

**设计要点**：
- 只需要一个写函数
- 使用 `void*` 避免具体 GPIO 句柄类型
- 参数：GPIO 句柄、引脚编号、写入值（0 或 1）

### 设备对象

```c
typedef struct led {
    void* gpio;          // GPIO 句柄
    u16 pin;             // 引脚编号
    u8 valid;            // 有效电平（1=高电平亮，0=低电平亮）
    led_state state;     // 当前 LED 状态
} led_t;
```

**设计要点**：
- 硬件资源在前（gpio, pin）
- 配置参数（valid）在后
- 状态变量（state）最后

### 关键实现

```c
void led_on(led_t* self) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(g_led_port != NULL);

    if (self->valid) {
        g_led_port->write_pin(self->gpio, self->pin, 1);
    } else {
        g_led_port->write_pin(self->gpio, self->pin, 0);
    }
    self->state = led_state_on;
}
```

**最佳实践**：
1. 先检查 port 是否注册
2. 使用 `debug_print` 输出错误信息
3. 根据 `valid` 参数决定输出电平
4. 更新状态变量

### 适用场景
- LED 指示灯
- 蜂鸣器
- 继电器控制
- 其他简单开关控制

---

## 示例 2: BH1750 光照传感器（I2C 总线）

### 特点
- 典型的 I2C 总线设备
- 使用访问宏简化代码
- 数据格式转换（原始值 → Lux）
- 错误处理和重试机制

### Port 设计

```c
typedef struct bh1750_port {
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr,
                     u16 mem_addr_size, u8* data,
                     u16 data_size, u32 timeout);
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr,
                    u16 mem_addr_size, u8* data,
                    u16 data_size, u32 timeout);
} bh1750_port_t;
```

**设计要点**：
- 使用标准 I2C HAL 接口签名
- `dev_addr` 包含读/写位
- `mem_addr` 和 `mem_addr_size` 用于寄存器地址（BH1750 不使用）
- `timeout` 防止 I2C 操作卡死

### 访问宏

```c
#define bh1750_send_cmd(self, cmd) \
    g_bh1750_port->i2c_write(self->hi2c, BH1750_ADDR_WRITE, 0, 0,
                              (uint8_t*)&cmd, 1, 0xFFFF)
#define bh1750_read_dat(self, dat) \
    g_bh1750_port->i2c_read(self->hi2c, BH1750_ADDR_READ, 0, 0, dat, 2, 0xFFFF)
```

**设计要点**：
- 使用宏避免函数调用开销
- 固定参数（如地址、大小）硬编码
- 提高代码可读性

### 关键实现

```c
i32 bh1750_start(bh1750_t* self, bh1750_mode_t mode) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(g_bh1750_port != NULL);

    i32 ret = bh1750_send_cmd(self, mode);
    if (ret != 0) {
        debug_print("[bh1750] i2c write fail, ret:%d\n", ret);
        return BH1750_ERR_I2C_FAIL;
    }

    return BH1750_OK;
}
```

**最佳实践**：
1. 检查 port 注册状态
2. 使用访问宏调用 I2C 函数
3. 检查返回值并转换错误码
4. 使用 `debug_print` 记录失败

### 数据转换

```c
static u16 bh1750_dat2lux(uint8_t* dat) {
    u32 raw = ((u16)dat[0] << 8) | dat[1];
    return (u16)(raw * 5 / 6);
}
```

**设计要点**：
- 使用静态函数（不暴露给外部）
- 数据格式转换在驱动内部完成
- 返回物理单位（Lux）

### 适用场景
- I2C 传感器（温度、湿度、压力等）
- I2C EEPROM/Flash
- I2C 扩展芯片

---

## 示例 3: DHT11 温湿度传感器（时序敏感型）

### 特点
- 需要精确的时序控制（微秒级）
- 双向 GPIO 操作（输出 → 输入）
- 位级别的通信协议
- 复杂的错误处理

### Port 设计

```c
typedef struct dht11_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8   (*read_pin)(void* gpio, u16 pin);
    void (*set_output_mode)(void* gpio, u16 pin);
    void (*set_input_mode)(void* gpio, u16 pin);
    void (*delay_us)(u32 us);
    void (*delay_ms)(u32 ms);
} dht11_port_t;
```

**设计要点**：
- 需要双向 GPIO 操作
- 需要切换 GPIO 模式（输出/输入）
- 需要精确延时函数（微秒级）

### 访问宏

```c
#define DHT11_WRITE(self, v)    g_dht11_port->write_pin((self)->gpio, (self)->pin, (v))
#define DHT11_READ(self)        g_dht11_port->read_pin((self)->gpio, (self)->pin)
#define DHT11_OUTPUT_MODE(self) g_dht11_port->set_output_mode((self)->gpio, (self)->pin)
#define DHT11_INPUT_MODE(self)  g_dht11_port->set_input_mode((self)->gpio, (self)->pin)
#define DHT11_DELAY_US(us)      g_dht11_port->delay_us(us)
#define DHT11_DELAY_MS(ms)      g_dht11_port->delay_ms(ms)
```

**设计要点**：
- 宏命名清晰：`DHT11_WRITE`, `DHT11_READ` 等
- 参数使用 `(self)` 确保优先级正确
- 所有硬件操作通过宏访问

### 关键实现 - 复位和启动

```c
static i32 dht11_reset_and_start(dht11_t* self) {
    if (!g_dht11_port) return DHT11_ERR_PORT_NOT_REGISTERED;

    DHT11_OUTPUT_MODE(self);
    DHT11_WRITE(self, 0);
    DHT11_DELAY_MS(DHT11_START_LOW_MS);  // 拉低 >=18ms
    DHT11_WRITE(self, 1);
    DHT11_DELAY_US(30);  // 等待 30us

    DHT11_INPUT_MODE(self);
    return DHT11_OK;
}
```

**时序说明**：
1. 主机拉低 >=18ms 发送启动信号
2. 拉高 30us 等待
3. 切换为输入模式等待响应

### 关键实现 - 响应检测

```c
static i32 dht11_check_response(dht11_t* self) {
    u32 retry = 0;

    // 等待响应低电平
    while (DHT11_READ(self)) {
        DHT11_DELAY_US(1);
        retry++;
        if (retry >= DHT11_ACK_WAIT_US_MAX) break;
    }
    if (retry >= DHT11_ACK_WAIT_US_MAX) return DHT11_ERR_NO_RESPONSE;

    // 等待响应高电平
    retry = 0;
    while (!DHT11_READ(self)) {
        DHT11_DELAY_US(1);
        retry++;
        if (retry >= DHT11_ACK_WAIT_US_MAX) break;
    }
    if (retry >= DHT11_ACK_WAIT_US_MAX) return DHT11_ERR_BAD_ACK1;

    // 等待响应结束（回到低电平）
    retry = 0;
    while (DHT11_READ(self)) {
        DHT11_DELAY_US(1);
        retry++;
        if (retry >= DHT11_ACK_WAIT_US_MAX) break;
    }
    if (retry >= DHT11_ACK_WAIT_US_MAX) return DHT11_ERR_BAD_ACK2;

    return DHT11_OK;
}
```

**最佳实践**：
1. 使用重试机制防止死循环
2. 为每个错误场景定义不同的错误码
3. 使用宏定义时序常量

### 关键实现 - 位读取

```c
static u8 dht11_read_bit(dht11_t* self) {
    u32 retry = 0;
    while (!DHT11_READ(self)) {
        DHT11_DELAY_US(1);
        retry++;
        if (retry >= DHT11_BIT_WAIT_LOW_US_MAX) break;
    }
    retry = 0;
    while (DHT11_READ(self)) {
        DHT11_DELAY_US(1);
        retry++;
        if (retry >= DHT11_BIT_WAIT_LOW_US_MAX) break;
    }
    DHT11_DELAY_US(40);  // 延时 40us 后采样
    return (DHT11_READ(self) ? 1 : 0);
}
```

**位时序解析**：
- 等待低电平结束
- 等待高电平（数据位 '0' 为 ~26us，'1' 为 ~70us）
- 延时 40us 后采样（区分 '0' 和 '1'）

### 适用场景
- 单总线传感器（DHT11/DHT22）
- 1-Wire 协议设备（DS18B20）
- 自定义时序协议
- 软件模拟 SPI/I2C

---

## 示例 4: EC11 旋转编码器（状态机式）

### 特点
- 多 GPIO 输入（CLK、DT、SW）
- 状态机式的扫描检测
- 保持历史状态
- 方向检测逻辑

### Port 设计

```c
typedef struct ec11_port {
    u8 (*read_pin)(void* gpio, u16 pin);
} ec11_port_t;
```

**设计要点**：
- 只需要读操作（输入引脚）
- 单一函数处理所有引脚

### 设备对象

```c
typedef struct ec11 {
    void* clk_gpio;
    u16   clk_pin;
    void* dt_gpio;
    u16   dt_pin;
    void* sw_gpio;
    u16   sw_pin;

    u8 last_clk_state;
    u8 last_dt_state;
    u8 last_sw_state;
    u8 sw_when_down_state;

    i32 rotation_count;
    ec11_rotation last_item;
} ec11_t;
```

**设计要点**：
- 3 个引脚（CLK、DT、SW）
- 保存上一次的状态（用于跳变检测）
- 累计计数值
- 记录最后一次旋转方向

### 初始化

```c
void ec11_init(ec11_t* self, ...) {
    // ... 参数赋值
    self->rotation_count = 0;
    self->last_item      = EC11_ROTATION_NONE;

    if (g_port) {
        self->last_clk_state = g_port->read_pin(self->clk_gpio, self->clk_pin);
        self->last_dt_state  = g_port->read_pin(self->dt_gpio, self->dt_pin);
        self->last_sw_state  = g_port->read_pin(self->sw_gpio, self->sw_pin);
    } else {
        debug_print("[ec11] warning: port not registered...\n");
        self->last_clk_state = 1;  // 假设空闲高电平
        self->last_dt_state  = 1;
        self->last_sw_state  = 1;
    }
}
```

**最佳实践**：
1. 即使 port 未注册也设置默认值
2. 输出警告信息
3. 根据硬件特性设置默认状态

### 扫描逻辑

```c
ec11_rotation ec11_scan(ec11_t* self) {
    if (!g_port) {
        debug_print("[ec11] error: port not registered\n");
        return EC11_ROTATION_NONE;
    }

    u8 clk_state = g_port->read_pin(self->clk_gpio, self->clk_pin);
    u8 dt_state  = g_port->read_pin(self->dt_gpio, self->dt_pin);
    self->last_sw_state = g_port->read_pin(self->sw_gpio, self->sw_pin);

    ec11_rotation result = EC11_ROTATION_NONE;

    // 检测 CLK 跳变
    if (clk_state != self->last_clk_state) {
        // 当 CLK 和 DT 电平不同时为正转，相同时为反转
        if (clk_state != dt_state) {
            result = EC11_ROTATION_RIGHT;
            self->rotation_count++;
        } else {
            result = EC11_ROTATION_LEFT;
            self->rotation_count--;
        }
        self->last_item = result;
    }

    self->last_clk_state = clk_state;
    self->last_dt_state  = dt_state;

    return result;
}
```

**检测原理**：
1. 检测 CLK 引脚的跳变
2. 比较 CLK 和 DT 的电平关系：
   - 不同 → 正转
   - 相同 → 反转
3. 更新历史状态和计数

### 适用场景
- 旋转编码器
- 增量式编码器
- 需要状态保持的传感器
- 事件驱动型输入设备

---

## 通用设计模式总结

### 1. Port 层设计原则

✅ **DO**:
- 使用 `void*` 避免具体硬件类型
- 只包含最小必要的函数
- 函数命名清晰（`write_pin`, `read_pin`, `delay_us` 等）

❌ **DON'T**:
- 不要在 port 中包含特定硬件类型
- 不要添加不必要的辅助函数
- 不要在 port 层做业务逻辑

### 2. 设备对象设计

**推荐顺序**：
```c
typedef struct xxx {
    // 1. 硬件资源句柄
    void* gpio;
    u16 pin;
    void* hi2c;

    // 2. 配置参数
    u8 mode;
    u16 timeout;

    // 3. 状态变量
    u8 initialized;
    // ...
} xxx_t;
```

### 3. 访问宏的使用

**何时使用访问宏**：
- 频繁调用的硬件操作
- 需要简化的重复代码
- 提高可读性的场景

**示例**：
```c
#define XXX_WRITE(self, v)    g_xxx_port->write_pin((self)->gpio, (self)->pin, (v))
#define XXX_READ(self)        g_xxx_port->read_pin((self)->gpio, (self)->pin)
```

### 4. 错误处理

**错误码定义**：
```c
#define XXX_OK                      0
#define XXX_ERR_PORT_NOT_REGISTERED (-1)
#define XXX_ERR_INVALID_PARAM       (-2)
#define XXX_ERR_TIMEOUT             (-3)
```

**错误处理模式**：
```c
i32 xxx_function(xxx_t* self, ...) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(g_xxx_port != NULL);

    // ... 操作

    return XXX_OK;
}
```

### 5. 调试输出

**使用 `debug_print`**：
```c
debug_print("[xxx] error: operation failed, ret:%d\n", ret);
```

**格式规范**：
- 使用 `[xxx]` 前缀标识模块
- 包含错误描述和参数
- 使用换行符 `\n`

### 6. 时序控制

**使用宏定义时序常量**：
```c
#define XXX_DELAY_US    10
#define XXX_TIMEOUT_MS  100
```

**避免魔术数字**：
```c
// ❌ 不好
DHT11_DELAY_US(80);

// ✅ 好
#define DHT11_BIT_1_WAIT_US   70
#define DHT11_SAMPLE_DELAY_US 40
DHT11_DELAY_US(DHT11_SAMPLE_DELAY_US);
```

---

## 常见问题

### Q1: 什么时候需要 port 层？

**A**: 任何时候驱动需要与硬件交互时都需要：
- GPIO 操作
- 总线通信（I2C/SPI/UART）
- 精确延时
- 中断处理（通过回调函数）

### Q2: 如果驱动不需要硬件依赖怎么办？

**A**: 可以不定义 port 层：
```c
// 不需要 port 层
typedef struct xxx {
    u32 data;
} xxx_t;

void xxx_init(xxx_t* self, u32 data) {
    self->data = data;
}
```

### Q3: 如何处理多实例？

**A**: 每个实例维护自己的状态：
```c
xxx_t dev1, dev2;
xxx_init(&dev1, gpio1, pin1);
xxx_init(&dev2, gpio2, pin2);

// 使用
xxx_read(&dev1, &data1);
xxx_read(&dev2, &data2);
```

### Q4: port 可以注册多次吗？

**A**: 不推荐。port 是全局的，通常在系统启动时绑定一次。如果需要切换硬件，重新绑定即可。

### Q5: 如何测试驱动？

**A**: 使用模拟 port：
```c
// 模拟 port
u8 simulated_value = 0;
void sim_write_pin(void* gpio, u16 pin, u8 value) {
    simulated_value = value;
    printf("[SIM] GPIO%c%d = %d\n", (char)gpio, pin, value);
}

// 绑定
xxx_port_t sim_port = { .write_pin = sim_write_pin };
xxx_bind_port(&sim_port);

// 测试
xxx_t dev;
xxx_init(&dev, (void*)'A', 5);
xxx_on(&dev);  // 输出: [SIM] GPIOA5 = 1
```

---

## 参考资料

- **em_driver编写规范.md**: 完整的开发规范
- **LED 驱动**: `src/em_driver/led.c`
- **BH1750 驱动**: `src/em_driver/bh1750.c`
- **DHT11 驱动**: `src/em_driver/dht11.c`
- **EC11 驱动**: `src/em_driver/ec11.c`
