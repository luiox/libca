# 组件开发示例分析

本文档分析 libca 项目中现有的 em_util/em_base 组件实现，展示不同类型组件的设计模式、实现细节和测试策略。

---

## 示例 1: RingBuffer（环形缓冲区容器）

### 特点
- 经典的数据结构容器
- 使用位运算实现高效的环形索引
- 提供多种数据类型的便捷访问函数
- 完整的单元测试覆盖

### 数据结构

```c
typedef struct ringbuffer {
    uint8_t* buffer;      // 缓冲区指针
    position_size_t size;  // 缓冲区大小（必须是2的幂次方）
    position_size_t used;  // 已使用大小
    position_size_t read;  // 读指针
    position_size_t write; // 写指针
} ringbuffer_t;
```

**设计要点**：
- `size` 必须是2的幂次方，用于位运算优化
- 使用 `used` 计数而非指针比较判断满/空
- `position_size_t` 类型支持不同平台的大小

### 关键实现 - 初始化

```c
void ringbuffer_init(ringbuffer_t* rb, uint8_t* buffer, position_size_t size)
{
    param_check(rb != NULL);
    param_check(buffer != NULL);
    rb->buffer = buffer;
    rb->size   = size;
    rb->used   = 0;
    rb->read   = 0;
    rb->write  = 0;
}
```

**最佳实践**：
1. 使用 `param_check` 进行参数检查
2. 所有成员变量显式初始化
3. 假设调用者保证 `size` 是2的幂次方

### 关键实现 - 写入

```c
position_size_t ringbuffer_write(ringbuffer_t* rb, const uint8_t* data, position_size_t size)
{
    param_check(rb != NULL);
    param_check(data != NULL);

    position_size_t free_size = ringbuffer_free(rb);
    if (size > free_size) {
        size = free_size;  // 只写入能写入的部分
    }

    // 写入数据
    for (position_size_t i = 0; i < size; i++) {
        rb->buffer[rb->write] = data[i];
        rb->write = (rb->write + 1) & (rb->size - 1);  // 位运算取模
    }
    rb->used += size;
    return size;
}
```

**关键技巧**：
1. 检查可用空间，自动截断
2. 使用位运算 `(rb->write + 1) & (rb->size - 1)` 实现循环
   - 如果 `size` 是8（二进制 `1000`），则 `size - 1` 是 `0111`
   - `(write + 1) & 0111` 相当于 `(write + 1) % 8`
3. 返回实际写入的大小

### 关键实现 - 读取

```c
position_size_t ringbuffer_read(ringbuffer_t* rb, uint8_t* buf, position_size_t size)
{
    param_check(rb != NULL);
    param_check(buf != NULL);

    position_size_t used_size = ringbuffer_used(rb);
    if (size > used_size) {
        size = used_size;  // 只读取能读取的部分
    }

    // 读取数据
    for (position_size_t i = 0; i < size; i++) {
        buf[i] = rb->buffer[rb->read];
        rb->read = (rb->read + 1) & (rb->size - 1);
    }
    rb->used -= size;
    return size;
}
```

**设计要点**：
1. 只读取实际可用的数据量
2. 读取后更新 `used` 计数
3. 读指针也使用位运算循环

### 工具函数

```c
u8 ringbuffer_read_u8(ringbuffer_t* rb) {
    u8 val = 0;
    ringbuffer_read(rb, &val, 1);
    return val;
}

void ringbuffer_write_u8(ringbuffer_t* rb, u8 value) {
    ringbuffer_write(rb, &value, 1);
}
```

**设计要点**：
- 提供类型安全的便捷访问函数
- 避免调用者手动处理类型转换
- 支持 u8/u16/i16/i32/u32/i32/float 等类型

### 单元测试

```c
#if TEST_ENABLE

#include "../em_test/test.h"

TEST_CASE(ringbuffer_basic)
{
    uint8_t      buf[16];
    ringbuffer_t rb;
    uint8_t      data_to_write[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t      data_to_read[4] = {0};

    ringbuffer_init(&rb, buf, 16);

    // 测试初始状态
    TEST_ASSERT_EQUAL_INT(0, ringbuffer_used(&rb));
    TEST_ASSERT_EQUAL_INT(16, ringbuffer_free(&rb));

    // 测试写入
    position_size_t written = ringbuffer_write(&rb, data_to_write, 4);
    TEST_ASSERT_EQUAL_INT(4, written);
    TEST_ASSERT_EQUAL_INT(4, ringbuffer_used(&rb));
    TEST_ASSERT_EQUAL_INT(12, ringbuffer_free(&rb));

    // 测试读取
    position_size_t read = ringbuffer_read(&rb, data_to_read, 4);
    TEST_ASSERT_EQUAL_INT(4, read);
    TEST_ASSERT_EQUAL_INT(0, ringbuffer_used(&rb));
    TEST_ASSERT_EQUAL_INT(0x01, data_to_read[0]);
    TEST_ASSERT_EQUAL_INT(0x04, data_to_read[3]);
}

TEST_CASE(ringbuffer_wrap_around)
{
    uint8_t      buf[8];
    ringbuffer_t rb;
    uint8_t      data1[] = {1, 2, 3, 4, 5, 6};
    uint8_t      data2[] = {7, 8};
    uint8_t      read_buf[8];

    ringbuffer_init(&rb, buf, 8);

    // 写入6字节
    ringbuffer_write(&rb, data1, 6);
    // 读取4字节，此时 read=4, write=6, used=2
    ringbuffer_read(&rb, read_buf, 4);
    TEST_ASSERT_EQUAL_INT(2, ringbuffer_used(&rb));

    // 再次写入4字节，会发生回环 (6+4=10, 10%8=2)
    position_size_t written = ringbuffer_write(&rb, data2, 4);
    TEST_ASSERT_EQUAL_INT(4, written);
    TEST_ASSERT_EQUAL_INT(6, ringbuffer_used(&rb));

    // 读取所有数据验证正确性
    ringbuffer_read(&rb, read_buf, 6);
    TEST_ASSERT_EQUAL_INT(5, read_buf[0]);
    TEST_ASSERT_EQUAL_INT(6, read_buf[1]);
    TEST_ASSERT_EQUAL_INT(7, read_buf[2]);
    TEST_ASSERT_EQUAL_INT(8, read_buf[3]);
}

#endif
```

**测试策略**：
1. `ringbuffer_basic`: 基本读写功能
2. `ringbuffer_wrap_around`: 回环场景（关键测试）
3. `ringbuffer_u8/u16/i16/u32/i32/float`: 各种数据类型测试
4. `ringbuffer_checksum`: 校验和功能测试

---

## 示例 2: CRC 算法（校验算法）

### 特点
- 实现多种 CRC 算法（CRC32/IEEE、CRC16/MODBUS、CRC16/XMODEM）
- 提供查找表和朴素实现两种版本
- 标准测试向量验证
- 高性能优化（查找表）

### CRC-32/IEEE 实现（查找表）

```c
// CRC-32/IEEE 802.3 查找表 (多项式: 0x04C11DB7)
static const u32 g_crc32_ieee_table[256] = {
    0x00000000U, 0x77073096U, 0xee0e612cU, 0x990951baU,
    // ... 完整的256项查找表
};

u32 crc32_ieee_fast(const void* data, usize size)
{
    uint32_t crc = 0xFFFFFFFFU;
    const u8* p = (const u8*)(data);

    for (usize i = 0; i < size; i++) {
        crc = g_crc32_ieee_table[(crc ^ (u32)p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}
```

**设计要点**：
1. 初始化 CRC 为 `0xFFFFFFFF`
2. 使用查找表加速计算（每次查表代替8次移位）
3. 最终异或 `0xFFFFFFFF` 取反
4. 查找表预计算并定义为 `static const`

### CRC-16/MODBUS 实现（朴素版）

```c
u16 crc16_modbus(const void* data, usize size)
{
    u16 crc = 0xFFFF;
    const u8* p = (const u8*)data);
    for (usize i = 0; i < size; i++) {
        crc ^= (u16)p[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
```

**设计要点**：
1. 初始化 CRC 为 `0xFFFF`
2. 每个字节处理8次（逐位处理）
3. 多项式 `0xA001` 是 `0x8005` 的位反转版本
4. 适合代码空间受限的场景

### 滚动计算版本

```c
u16 crc16_modbus_ex(const void* data, usize size, u16 previous_crc)
{
    u16 crc = previous_crc;
    const u8* p = (const u8*)data;
    for (usize i = 0; i < size; i++) {
        crc ^= (u16)p[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
```

**设计要点**：
- 接受 `previous_crc` 参数
- 适用于流式数据（数据分块到达）
- 可以持续计算，不需要一次性获得所有数据

### 单元测试

```c
#if TEST_ENABLE
#include "../em_test/test.h"
#include <string.h>

TEST_CASE(crc)
{
    const char* data = "123456789";
    usize       len  = strlen(data);

    // CRC32 IEEE
    u32 c32_fast = crc32_ieee_fast(data, len);
    u32 c32_slow = crc32_ieee(data, len);
    TEST_ASSERT(c32_fast == 0xCBF43926);  // 标准测试向量
    TEST_ASSERT(c32_slow == 0xCBF43926);

    // CRC16 Modbus
    u16 c16_modbus_fast = crc16_modbus_fast(data, len);
    u16 c16_modbus_slow = crc16_modbus(data, len);
    TEST_ASSERT(c16_modbus_fast == 0x4B37);  // 标准测试向量
    TEST_ASSERT(c16_modbus_slow == 0x4B37);

    // CRC16 XMODEM
    u16 c16_xmodem_fast = crc16_xmodem_fast(data, len);
    u16 c16_xmodem_slow = crc16_xmodem(data, len);
    TEST_ASSERT(c16_xmodem_fast == 0x31C3);  // 标准测试向量
    TEST_ASSERT(c16_xmodem_slow == 0x31C3);
}
#endif
```

**测试策略**：
1. **标准测试向量**：使用 "123456789" 作为标准输入
2. **对比验证**：快速版和慢速版结果必须一致
3. **期望值硬编码**：标准算法的已知结果

---

## 示例 3: PID 控制器（控制算法）

### 特点
- 实现位置式和增量式两种 PID
- 浮点数计算
- 详细的注释说明公式
- 简洁的单元测试

### 数据结构

```c
typedef struct pid_position {
    float target;       // 目标值
    float kp;           // 比例系数
    float ki;           // 积分系数
    float kd;           // 微分系数
    float last_error;   // 上一次误差
    float prev_error;   // 前一次误差（用于增量式）
    float sum_error;    // 误差积分
} pid_position_t;

typedef struct pid_incremental {
    float target;
    float kp;
    float ki;
    float kd;
    float last_error;
    float prev_error;
} pid_incremental_t;
```

### 位置式 PID 实现

```c
float pid_position_calculate(pid_position_t* pid, float current_value)
{
    // 离散的位置式PID计算公式
    // u(k)=Kp*e(k)+Ki*∑e(j)+Kd[e(k)-e(k-1)]

    float error = pid->target - current_value;   // 当前误差 = 目标值 - 当前值
    pid->sum_error += error;                     // 误差积分
    float d_error = error - pid->last_error;     // 误差微分

    pid->prev_error = pid->last_error;   // 更新上一次的误差
    pid->last_error = error;             // 更新当前最新的误差

    return pid->kp * error              // 比例项
           + pid->ki * pid->sum_error   // 积分项
           + pid->kd * d_error;         // 微分项
}
```

**设计要点**：
1. 误差计算：`error = target - current_value`
2. 积分项累加：`sum_error += error`
3. 微分项：`d_error = error - last_error`
4. 更新历史状态（prev_error, last_error）

### 增量式 PID 实现

```c
float pid_incremental_calculate(pid_incremental_t* pid, float current_value)
{
    // 离散的增量式PID计算公式
    // △u(k)=Ae(k)-Be(k-1)+Ce(k-2)
    // 其中：A=Kp+Ki+Kd, B=Kp+2Kd, C=Kd
    // 展开：△u(k)=Kp[e(k)-e(k-1)]+Ki*e(k)+Kd[e(k)-2e(k-1)+e(k-2)]

    float error  = pid->target - current_value;
    float output = pid->kp * (error - pid->last_error)     // Kp[e(k)-e(k-1)]
                   + pid->ki * error                       // +Ki*e(k)
                   + pid->kd * (error - 2 * pid->last_error + pid->prev_error); // +Kd[e(k)-2e(k-1)+e(k-2)]

    pid->prev_error = pid->last_error;
    pid->last_error = error;

    return output;
}
```

**设计要点**：
1. 计算的是**增量** `△u(k)`，不是输出 `u(k)`
2. 需要保存两次历史误差（prev_error, last_error）
3. 适用于需要增量控制的场景

### 单元测试

```c
#if TEST_ENABLE

#include "../em_test/test.h"

TEST_CASE(pid_position_basic)
{
    pid_position_t pid;
    pid_position_init(&pid, 1.0f, 0.1f, 0.05f, 10.0f);   // 目标10
    float values[] = {0, 2, 5, 8, 9, 10, 11, 12};

    // 简单验证第一个计算结果
    // error = 10 - 0 = 10
    // sum_error = 10
    // d_error = 10 - 0 = 10
    // output = 1.0*10 + 0.1*10 + 0.05*10 = 10 + 1 + 0.5 = 11.5
    float u = pid_position_calculate(&pid, values[0]);
    TEST_ASSERT_EQUAL_FLOAT(11.5f, u);
}

TEST_CASE(pid_incremental_basic)
{
    pid_incremental_t pid;
    pid_incremental_init(&pid, 1.0f, 0.1f, 0.05f, 10.0f);
    float values[] = {0, 2, 5, 8, 9, 10, 11, 12};

    // 简单验证第一个增量结果
    // error = 10 - 0 = 10
    // last_error = 0, prev_error = 0
    // du = 1.0*(10-0) + 0.1*10 + 0.05*(10 - 2*0 + 0) = 10 + 1 + 0.5 = 11.5
    float du = pid_incremental_calculate(&pid, values[0]);
    TEST_ASSERT_EQUAL_FLOAT(11.5f, du);
}

#endif
```

**测试策略**：
1. 手动计算期望值（使用公式）
2. 使用 `TEST_ASSERT_EQUAL_FLOAT` 浮点数比较
3. 在注释中详细说明计算过程
4. 测试多种输入值序列

---

## 通用设计模式总结

### 1. 参数检查

**使用 param_check 进行参数验证**：
```c
void xxx_function(xxx_t* self, const u8* data, usize size) {
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(size > 0);
    // ... 实现
}
```

**说明**：
- `param_check`: 统一使用 `param_check` 进行参数验证（来自 em_base/debug.h）
- 不再使用 `assert.h` 的 `assert` 宏

### 2. 错误处理

**返回错误码**：
```c
#define XXX_OK              0
#define XXX_ERR_NULL_PTR   (-1)
#define XXX_ERR_INVALID    (-2)
#define XXX_ERR_OVERFLOW   (-3)

i32 xxx_function(...) {
    if (!param) return XXX_ERR_NULL_PTR;
    // ...
    return XXX_OK;
}
```

**设计原则**：
- 错误码必须为负数（成功为0）
- 为每种错误场景定义明确的错误码
- 使用宏定义避免魔术数字

### 3. 内存管理

**避免动态分配**：
```c
// ✅ 推荐：由调用者提供缓冲区
typedef struct xxx {
    u8* buffer;  // 指向调用者提供的内存
    usize size;
} xxx_t;

void xxx_init(xxx_t* self, u8* buffer, usize size);

// ❌ 避免：在组件内部分配内存
typedef struct xxx {
    u8* buffer;  // 需要组件内部分配
} xxx_t;
```

**原因**：
- 嵌入式系统内存受限
- 避免内存碎片
- 减少内存泄漏风险

### 4. 数据类型使用

**使用 em_base/datatype.h 类型**：
```c
#include "../em_base/datatype.h"

// 使用统一的类型
u8, u16, u32, u64   // 无符号整数
i8, i16, i32, i64   // 有符号整数
f32, f64            // 浮点数
usize               // 大小/索引类型
```

**好处**：
- 跨平台兼容
- 明确的类型语义
- 代码一致性

### 5. 测试用例设计

**原子化原则**：
```c
// ❌ 不好：一个测试做所有事情
TEST_CASE(ringbuffer_all) {
    ringbuffer_init(&rb, buf, 16);
    ringbuffer_write(&rb, data, 4);
    ringbuffer_read(&rb, buf_out, 4);
    ringbuffer_write_u16(&rb, 0x1234);
    // ... 太多内容
}

// ✅ 好：每个功能单独测试
TEST_CASE(ringbuffer_basic) {
    // 测试基本读写
}

TEST_CASE(ringbuffer_wrap_around) {
    // 测试回环场景
}

TEST_CASE(ringbuffer_u16) {
    // 测试 u16 类型
}
```

### 6. 边界测试

**必须测试的边界**：
```c
TEST_CASE(xxx_boundary_zero) {
    // 测试 size = 0
    xxx_init(&obj, buf, 0);
    TEST_ASSERT_EQUAL_INT(0, xxx_used(&obj));
}

TEST_CASE(xxx_boundary_one) {
    // 测试 size = 1
    xxx_init(&obj, buf, 1);
    xxx_write(&obj, data, 1);
    TEST_ASSERT_TRUE(xxx_is_full(&obj));
}

TEST_CASE(xxx_boundary_max) {
    // 测试 size = MAX_SIZE
    xxx_init(&obj, buf, MAX_SIZE);
    // ... 测试
}
```

### 7. 浮点数比较

**使用 `TEST_ASSERT_EQUAL_FLOAT`**：
```c
TEST_CASE(pid_float_comparison) {
    pid_position_t pid;
    pid_position_init(&pid, 1.0f, 0.1f, 0.05f, 10.0f);
    float output = pid_position_calculate(&pid, 0.0f);

    // ✅ 使用 TEST_ASSERT_EQUAL_FLOAT（内部使用 epsilon）
    TEST_ASSERT_EQUAL_FLOAT(11.5f, output);

    // ❌ 不要直接使用 ==
    if (output == 11.5f) {  // 不可靠
        // ...
    }
}
```

### 8. 命名约定

**类型命名**：
```c
typedef struct ringbuffer { ... } ringbuffer_t;   // 结构体：xxx_t
```

**函数命名**：
```c
void ringbuffer_init(ringbuffer_t* rb, ...);      // xxx_init
position_size_t ringbuffer_write(...);           // xxx_write
```

**宏命名**：
```c
#define RINGBUFFER_SIZE_MAX   1024               // XXX_CONSTANT
```

---

## 测试最佳实践

### 1. 测试组织

```c
/* — 实现 — */
void my_function(/* params */) { ... }

/* — 单元测试 — */
#if TEST_ENABLE

#include "../em_test/test.h"

// 测试用例在这里

#endif
```

**规则**：
1. 所有代码在同一 `.c` 文件中
2. 测试在文件底部
3. 使用 `#if TEST_ENABLE` 控制编译

### 2. 测试命名

**格式**：`<module>_<feature>` 或 `<module>_<scenario>`

```c
TEST_CASE(ringbuffer_basic);        // ringbuffer + basic
TEST_CASE(ringbuffer_wrap_around);  // ringbuffer + wrap_around (scenario)
TEST_CASE(ringbuffer_u16);          // ringbuffer + u16 (feature)
TEST_CASE(pid_position_basic);      // pid_position + basic
TEST_CASE(crc_standard_vector);    // crc + standard_vector
```

### 3. 测试结构

**标准结构**：
```c
TEST_CASE(ringbuffer_basic)
{
    // 1. 准备（Setup）
    uint8_t buf[16];
    ringbuffer_t rb;
    ringbuffer_init(&rb, buf, 16);

    // 2. 执行（Execute）
    u8 data = 0xAB;
    ringbuffer_write_u8(&rb, data);

    // 3. 断言（Check）
    TEST_ASSERT_EQUAL_INT(0xAB, ringbuffer_read_u8(&rb));
}
```

### 4. 测试覆盖率

**必需的测试场景**：
1. ✅ 基本功能（正常路径）
2. ✅ 边界条件（0, 1, MAX_SIZE）
3. ✅ 错误处理（如果适用）
4. ✅ 标准向量（算法类）
5. ✅ 特殊场景（如回环）

### 5. 辅助函数

**测试助手函数**：
```c
#if TEST_ENABLE

// 辅助函数：填充缓冲区
static void fill_buffer(ringbuffer_t* rb, u8 value, usize size) {
    for (usize i = 0; i < size; i++) {
        ringbuffer_write_u8(rb, value);
    }
}

// 使用辅助函数
TEST_CASE(ringbuffer_full) {
    u8 buf[8];
    ringbuffer_t rb;
    ringbuffer_init(&rb, buf, 8);

    fill_buffer(&rb, 0xAA, 8);  // 使用辅助函数
    TEST_ASSERT_TRUE(ringbuffer_is_full(&rb));
}

#endif
```

---

## 常见问题

### Q1: 如何进行参数检查和错误处理？

**A**:
- 统一使用 `param_check` 进行参数验证
- 返回错误码用于外部参数验证失败的情况

```c
i32 xxx_function(xxx_t* self, const u8* data, usize size) {
    param_check(self != NULL);              // 参数检查：self 不能为 NULL
    param_check(self->initialized);         // 参数检查：必须已初始化

    if (!data) return XXX_ERR_NULL_PTR;  // 错误码：外部参数可能为 NULL
    if (size == 0) return XXX_ERR_EMPTY;

    // ... 实现
    return XXX_OK;
}
```

### Q2: 组件需要线程安全吗？

**A**: libca 项目不要求线程安全，但需要考虑：
- 避免全局变量（使用对象封装）
- 函数应该是可重入的
- 如果需要共享状态，由调用者负责加锁

### Q3: 如何测试浮点数精度？

**A**: 使用 `TEST_ASSERT_EQUAL_FLOAT`，内部使用 epsilon 比较容差：

```c
float f_in = 3.14159f;
float f_out = calculate_pi();

// TEST_ASSERT_EQUAL_FLOAT 使用默认 epsilon（通常是 0.00001）
TEST_ASSERT_EQUAL_FLOAT(f_in, f_out);
```

### Q4: 如何处理循环引用？

**A**: 避免循环依赖，通过以下方式：
- 使用前向声明（`struct xxx;`）
- 通过指针传递
- 拆分成更小的模块

### Q5: 何时提供 `_ex` 扩展函数？

**A**:
- 当函数有额外的可配置参数时
- 当需要保留原始函数的简单接口时
- 当提供滚动/流式处理能力时

```c
// 基本函数
u16 crc16_modbus(const void* data, usize size);

// 扩展函数（支持滚动计算）
u16 crc16_modbus_ex(const void* data, usize size, u16 previous_crc);
```

---

## 参考资料

- **单元测试规范.md**: 完整的测试编写规范
- **RingBuffer 实现**: `src/em_util/ringbuffer.c`
- **CRC 实现**: `src/em_util/crc.c`
- **PID 实现**: `src/em_util/pid.c`
- **em_base/datatype.h**: 统一类型定义
