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

### 4. 单元测试政策和验证要求
- 驱动实现通常不包含单元测试，因为驱动与硬件紧耦合，许多硬件行为难以在纯软件环境中复现。为避免误导，本 Skill 保持“驱动无需单元测试”的策略，但要求对每个驱动提供明确的验证计划（见下）。

- 验证要求：
  - 在驱动文档中包含**硬件验证计划**（测试步骤、需要的硬件/夹具、测试数据、验收标准）。
  - 对驱动中可拆分的无硬件依赖逻辑，应抽离到独立模块并为该模块编写单元测试（遵循 `prompt/em_code_rule.md` 的单元测试规范）。
  - 在 PR 中附上集成测试/硬件测试的复现步骤与结果（日志或截图）。

#### 合规检查清单（reviewer 快速核对）
- 使用 `datatype.h` 中的固定宽度类型（如 `u8/u16/u32` 等），避免直接使用 `int/size_t` 等。
- 公共 API 必须有 doxygen `@brief/@param/@return` 注释。
- 头文件必须使用项目约定的 include guard（禁止 `#pragma once`）。
- 对于 `xxx_t *self` 等 self 指针，关键函数应使用 `param_check` 进行契约式检查。
- 错误码以 `MODULE_ERR_*` 命名并为负数，成功返回 `MODULE_OK`（0）。
- 缩进 4 空格，行长不超过 120 字符，K&R 花括号风格。
- 避免在驱动中使用全局非前缀变量（如需全局，使用 `g_` 前缀）。

> 注：此清单为快速核对项，详细规则以 `prompt/em_code_rule.md` 为准。

## 快速开始

### 开发新驱动

1. **分析硬件**（通信接口、port 函数、配置）
2. **定义结构**（port 层、device object、API）
3. **实现逻辑**（port 绑定、错误处理、设备操作）
4. **集成构建**（添加到 xmake.lua）
5. **测试**（先模拟，后硬件）

详见：[03_develop_workflow.md](./references/03_develop_workflow.md)

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

### 详细文档（已重组）

1. **[01_code_rule.md](./references/01_code_rule.md)**
   - 驱动/模块的编码规范摘要（语言、类型、命名、注释、单元测试策略）

2. **[02_design_principle.md](./references/02_design_principle.md)**
   - 设计原则、Port 层设计与错误处理、内存管理、OOP 使用指南

3. **[03_develop_workflow.md](./references/03_develop_workflow.md)**
   - 驱动开发流程（从硬件分析到集成与测试）、Init 约定与测试策略

> 说明：详细文档已整理为 `01_code_rule.md`、`02_design_principle.md`、`03_develop_workflow.md`。示例驱动参考见 `04_Driver_Examples.md`。 

**Assets 使用说明**：请在创建驱动或提交 PR 时复用 `assets/` 下的模板（头文件、实现、单元测试样例、硬件验证计划与 PR 清单），并在 PR 描述中附上填写后的 `driver_testing_plan.md`。

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

详细故障排查见 [常见陷阱](./references/02_design_principle.md#常见陷阱与防御)。

## 不使用 OOP 的场景

对于无状态管理的工具函数：
```c
// 无 OOP - 纯计算
u32 calculate_crc(const u8* data, u32 length) {
    // 实现
    return crc_value;
}
```

详见 [设计原则](./references/02_design_principle.md#oop-风格使用)。

---

## Assets 与 模板
本 Skill 提供若干模板和检查单，位于 `assets/` 目录：
- `driver_header_template.h.md`：驱动头文件模板（doxygen 注释、include guard、`extern "C"` 等）。
- `driver_c_template.c.md`：驱动 C 文件模板（`param_check`、错误码、Access 宏示例）。
- `unit_test_template.c`：无硬件依赖模块的单元测试样例（用于被抽离的可测试逻辑）。
- `driver_testing_plan.md`：硬件验证计划模板（应随 PR 提交或在 PR 描述中链接）。
- `pr_checklist.md`：PR 审查快速清单（建议 reviewer 使用）。

**注意**：本 Skill 保持“驱动无需单元测试”的策略，驱动需要在真实硬件或模拟环境中测试；请在 PR 中附上硬件验证计划或说明为何不适合单元测试。

