---
name: em-driver-dev
description: 用于开发 em_driver 硬件驱动，适用于嵌入式 MCU 系统。用于创建新的传感器/外设驱动、实现 port 层抽象、遵循 libca 项目的 OOP 风格驱动模式。触发场景：如"开发 xxx 驱动"或"为某设备实现传感器驱动"或"重构 xxx 驱动"。
---

# EM Driver 开发指南

## 概述

本 Skill 指导 em_driver 硬件驱动的开发，采用标准化的 OOP 风格和 port 层抽象，确保代码结构和硬件独立性。

## 核心原则

### 1. OOP 风格
- 所有 API 函数第一个参数为 `xxx_t* self`
- Device object 管理硬件资源和状态
- Port 层抽象硬件操作

### 2. Port 层抽象
- 大部分驱动使用单一全局 port 指针
- 函数指针封装硬件操作
- 使用 `void*` 作为平台无关的 handle

### 3. 统一命名
- 类型：`xxx_t`, `xxx_port_t`
- 函数：`xxx_bind_port()`, `xxx_init()`, `xxx_operation()`
- 宏：`XXX_WRITE()`, `XXX_READ()`, `XXX_OK`, `XXX_ERR_*`

### 4. 无需单元测试
- 驱动依赖硬件
- 在真实 MCU 或模拟 port 上测试

## 快速开始

### 开发新驱动

1. **分析硬件**（通信接口、port 函数、配置）
2. **定义结构**（port 层、device object、API）
3. **实现逻辑**（port 绑定、错误处理、设备操作）
4. **集成构建**（添加到 xmake.lua）
5. **测试**（先模拟，后硬件）

详见：[01_Driver_Development_Workflow.md](./references/01_Driver_Development_Workflow.md)

### 常见驱动模式

| 模式 | 适用场景 | 示例 |
|------|----------|------|
| Simple GPIO | 输出控制 (LED, relay) | [LED](./references/04_Driver_Examples.md#led) |
| I2C Sensor | 寄存器型传感器 | [BH1750](./references/04_Driver_Examples.md#bh1750) |
| Timing-Sensitive | 时序敏感协议 | [DHT11](./references/04_Driver_Examples.md#dht11) |
| State Machine | 正交编码器 | [EC11](./references/04_Driver_Examples.md#ec11) |
| EEPROM | 非易失存储 | [AT24CXX](./references/04_Driver_Examples.md#at24cxx) |

详见：[02_Common_Driver_Patterns.md](./references/02_Common_Driver_Patterns.md)

## 核心模式

### Port 绑定
```c
static const xxx_port_t* g_xxx_port = NULL;

void xxx_bind_port(const xxx_port_t* port) {
    g_xxx_port = port;
}

bool xxx_port_is_registered(void) {
    return g_xxx_port != NULL;
}
```

### 错误处理
```c
i32 xxx_read(xxx_t* self, u8* data) {
    // 驱动内所有参数检查都用 param_check
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(g_xxx_port != NULL);

    // ... implementation
    return XXX_OK;
}
```

### Access 宏
```c
#define XXX_WRITE(self, v)    g_xxx_port->write((self)->gpio, (self)->pin, (v))
#define XXX_READ(self)        g_xxx_port->read((self)->gpio, (self)->pin)
```

## 参考资料

### 详细文档

1. **[01_Driver_Development_Workflow.md](./references/01_Driver_Development_Workflow.md)**
   - 完整的驱动开发流程
   - 硬件需求分析
   - 结构定义模式
   - 构建系统集成
   - 测试方法
   - 维护工作流程

2. **[02_Common_Driver_Patterns.md](./references/02_Common_Driver_Patterns.md)**
   - 5 种常见驱动模式及示例
   - LED, BH1750, DHT11, EC11, AT24CXX
   - Port 设计理由
   - 实现细节
   - 使用示例

3. **[03_Design_Principles.md](./references/03_Design_Principles.md)**
   - 命名规范
   - OOP 使用指南
   - Port 层设计原则
   - 错误处理模式
   - 内存管理规则
   - 常见问题和解决方案

4. **[04_Driver_Examples.md](./references/04_Driver_Examples.md)**
   - 代码库中现有驱动分析
   - 实际实现示例
   - 代码片段和说明

5. **[05_Specification_CN.md](./references/05_Specification_CN.md)**
   - 完整中文规范文档
   - 详细编码标准
   - 最佳实践和示例

### 快速参考

#### 文件结构
```
xxx.h:  Port, Device, API 声明, 错误码
xxx.c:  Port 绑定, Access 宏, 实现
```

#### 命名速查
```
类型：     xxx_t, xxx_port_t
函数：     xxx_bind_port(), xxx_init(), xxx_read()
宏：       XXX_WRITE(), XXX_OK, XXX_ERR_TIMEOUT
```

#### 错误码模式
```
#define XXX_OK                     0
#define XXX_ERR_PORT_NOT_REGISTERED (-1)
#define XXX_ERR_INVALID_PARAM       (-2)
#define XXX_ERR_TIMEOUT             (-3)
// 所有错误码为负数！
```

## 常见问题

| 问题 | 解决方案 |
|------|----------|
| Port 未注册 | 使用前调用 `xxx_bind_port()` |
| 时序错误 | 使用微秒级延时提高精度 |
| I2C NACK | 验证 7 位 vs 8 位地址 |
| 数据损坏 | 等待写周期 (EEPROM) |
| 数据类型错误 | 检查 datasheet 数据格式 |

详细故障排查见 [常见陷阱](./references/03_Design_Principles.md#common-pitfalls)。

## 不使用 OOP 的场景

对于无状态管理的工具函数：
```c
// 无 OOP - 纯计算
u32 calculate_crc(const u8* data, u32 length) {
    // 实现
    return crc_value;
}
```

详见 [设计原则](./references/03_Design_Principles.md#oop-style-usage)。

---

**注意**：本 Skill 不需要单元测试，驱动在真实硬件或模拟环境中测试。
