# libca.em组件索引与功能概览

面向嵌入式工程的 em_ 组件清单，包含主要功能与常见使用入口。



## 目录

- [模块总览](#模块总览)
- [em_base](#em_base)
- [em_bus](#em_bus)
- [em_component](#em_component)
- [em_driver](#em_driver)
- [em_eimui](#em_eimui)
- [em_log](#em_log)
- [em_ota](#em_ota)
- [em_platform](#em_platform)
- [em_protocol](#em_protocol)
- [em_shell](#em_shell)
- [em_test](#em_test)
- [em_util](#em_util)

---

## 模块总览

| 模块 | 主要功能 |
|------|----------|
| em_base | 基础类型与通用基础设施（类型、内存、调试等） |
| em_bus | 总线相关基础组件（如 I2C/SPI 等的抽象/配套） |
| em_component | 通用基础组件集合 |
| em_driver | 常见传感器/外设驱动集合 |
| em_eimui | EIMUI 相关组件 |
| em_log | 日志相关组件 |
| em_ota | OTA 相关组件 |
| em_platform | 平台适配层 |
| em_protocol | 传输协议栈（XMODEM/YMODEM 等） |
| em_shell | 交互式命令行 shell |
| em_test | 单元测试框架与工具 |
| em_util | 通用工具库 |

---

## em_base: 基础组件

| 模块                | 描述             | 关键特性                                        |
| ------------------- | ---------------- | ----------------------------------------------- |
| `datatype.h`        | 基础数据类型定义 | 跨平台类型别名（u8/u16/u32/i8/i16/i32/f32/f64） |
| `macro_util.h`      | 宏工具集合       | 字符串转换、安全命名、编译期检查工具            |
| `compiler_compat.h` | 编译器兼容性     | MSVC/GCC/Clang 兼容层，section 属性支持         |
| `memory_util.h`     | 内存操作工具     | 安全的内存操作宏，未使用参数标记                |
| `string_util.h`     | 字符串工具       | 字符串长度计算、格式化辅助                      |
| `debug.h`           | 调试打印系统     | 调试输出、断言宏、参数检查、可配置输出缓冲区    |



## em_test: 单元测试组件

断言

| 断言类型   | 宏名                         | 说明                          |
| ---------- | ---------------------------- | ----------------------------- |
| 通用断言   | `TEST_ASSERT(cond)`          | 基础条件检查                  |
| 8位整数    | `TEST_EXPECT_EQ_U8/I8`       | 8位无符号/有符号整数比较      |
| 16位整数   | `TEST_EXPECT_EQ_U16/I16`     | 16位整数比较                  |
| 32位整数   | `TEST_EXPECT_EQ_U32/I32`     | 32位整数比较                  |
| 浮点数     | `TEST_EXPECT_EQ_F32/F64`     | 带精度的浮点比较（默认 1e-6） |
| 自定义精度 | `TEST_EXPECT_EQ_F32_E/F64_E` | 自定义 epsilon 的浮点比较     |
| 布尔值     | `TEST_EXPECT_EQ_BOOL`        | 布尔值比较                    |
| 指针       | `TEST_EXPECT_NULL/NOT_NULL`  | 空指针检查                    |
| 字符串     | `TEST_EXPECT_EQ_STR`         | 字符串比较                    |
| 内存       | `TEST_EXPECT_EQ_MEM`         | 内存块比较                    |
| 真值       | `TEST_EXPECT_EQ_TRUE/FALSE`  | 显式真假值检查                |



## em_util: 通用组件

### crc: CRC 校验库

| 算法          | 函数                | 多项式     | 用途        |
| ------------- | ------------------- | ---------- | ----------- |
| CRC-32/IEEE   | `crc32_ieee/fast`   | 0x04C11DB7 | 以太网、ZIP |
| CRC-16/MODBUS | `crc16_modbus/fast` | 0x8005     | Modbus RTU  |
| CRC-16/XMODEM | `crc16_xmodem/fast` | 0x1021     | XMODEM 协议 |
| CRC-16/YMODEM | `crc16_ymodem/fast` | 0x1021     | YMODEM 协议 |
| Checksum      | `checksum_calc_u8`  | -          | 简单校验和  |

**特性**:

- 提供朴素实现和查表快速实现（`_fast` 后缀）
- 支持滚动计算（`_ex` 后缀）

### soft_timer: 软件定时器

| 类型                | 描述       | 典型应用               |
| ------------------- | ---------- | ---------------------- |
| `timeout_timer_t`   | 超时定时器 | 超时检测、看门狗       |
| `acumulate_timer_t` | 累计定时器 | 性能统计、运行时间记录 |

**特性**:

- 纯内联实现，零开销
- 32位时间戳，溢出安全
- 支持手动和自动时间基准

### mem_view: 内存视图工具

支持绑定一个内存缓冲区，以声明式的方式操作。不拥有缓冲区所有权的情况下，进行数据解析。

## em_driver: 模块驱动组件

| 模块型号 | 模块名字                                   | 是否已实物验证 |
| -------- | ------------------------------------------ | -------------- |
| ADS1115  | 16位ADC驱动                                | 待确认         |
| AS5600   | 磁编码器驱动                               | 待确认         |
| AT24CXX  | EEPROM驱动（AT24C01-AT24C512系列）         | 待确认         |
| BH1750   | 数字型光照强度传感器                       | 待确认         |
| BMC050   | 三轴加速度+磁力计驱动                      | 待确认         |
| BME280   | 温湿度压力传感器                           | 待确认         |
| BMP180   | 压力传感器                                 | 待确认         |
| BMP280   | 气压计传感器                               | 待确认         |
| DHT11    | 温湿度传感器                               | 已验证         |
| DHT22    | 温湿度传感器                               | 待确认         |
| DS1302   | 时钟芯片驱动                               | 待确认         |
| DS18B20  | 温度传感器                                 | 待确认         |
| EC11     | 旋转编码器驱动                             | 待确认         |
| HC_SR04  | 超声波测距驱动                             | 待确认         |
| HTS221   | 温湿度传感器                               | 待确认         |
| ILLUME   | 光敏电阻传感器                             | 待确认         |
| IR_TRACK | 反射式寻迹传感器                           | 待确认         |
| JY61P    | 姿态传感器（Wit协议）                      | 已验证         |
| KEY      | 硬件按键驱动                               | 待确认         |
| LCD1602  | 液晶显示屏驱动                             | 待确认         |
| LED      | LED驱动                                    | 待确认         |
| MAX30102 | 脉搏血氧仪和心率监测                       | 待确认         |
| MOTOR    | 直流有刷电机驱动                           | 待确认         |
| MQ_X     | MQ系列气体传感器（MQ-2/3/4/5/6/7/8/9/135） | 待确认         |
| NRF24    | NRF24L01 2.4G无线模块                      | 待确认         |
| SGP30    | 空气质量传感器                             | 待确认         |
| TOFXXF   | 激光测距模块                               | 已验证         |
| W25QXX   | SPI Flash驱动（W25Q系列）                  | 已验证         |



