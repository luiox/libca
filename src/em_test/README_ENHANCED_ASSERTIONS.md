# em_test 精确类型断言宏

## 概述

本模块为 `em_test` 测试框架新增了精确类型断言宏，解决了 C 语言整型提升（Integer Promotion）导致的类型比较问题。

## 问题背景

在 C 语言中，当比较 `int8_t` 和 `uint8_t` 时，会发生整型提升：

```c
int8_t  signed_val   = -1;   // 二进制: 0xFF，提升为 int: -1
uint8_t unsigned_val = 255;  // 二进制: 0xFF，提升为 int: 255

// 传统断言（有问题的写法）
TEST_ASSERT_EQUAL_INT(signed_val, unsigned_val);  // 失败！比较 -1 != 255

// 虽然它们的二进制表示完全相同（都是 0xFF）
```

## 解决方案

### 方案一：精确类型断言

为每种具体类型提供独立的断言宏，强制按指定类型比较：

```c
// 无符号类型
TEST_ASSERT_EQUAL_U8(expected, actual)   // uint8_t
TEST_ASSERT_EQUAL_U16(expected, actual)  // uint16_t
TEST_ASSERT_EQUAL_U32(expected, actual)  // uint32_t

// 有符号类型
TEST_ASSERT_EQUAL_I8(expected, actual)   // int8_t
TEST_ASSERT_EQUAL_I16(expected, actual)  // int16_t
TEST_ASSERT_EQUAL_I32(expected, actual)  // int32_t
```

**示例：**
```c
uint8_t value = 0xAB;
TEST_ASSERT_EQUAL_U8(0xAB, value);  // 正确！

int8_t signed_val = -42;
TEST_ASSERT_EQUAL_I8(-42, signed_val);  // 正确！
```

### 方案二：位宽比较宏（处理符号混用）

当你需要比较两个可能符号不同的值，但只关心它们的二进制内容时使用：

```c
TEST_ASSERT_EQUAL_U8_BITS(value1, value2)   // 按 8 位无符号比较
TEST_ASSERT_EQUAL_U16_BITS(value1, value2)  // 按 16 位无符号比较
TEST_ASSERT_EQUAL_U32_BITS(value1, value2)  // 按 32 位无符号比较
```

**典型场景：**
```c
// 通信协议：计算出的校验和（有符号）与接收到的字节（无符号）比较
int8_t checksum_calc = calculate_checksum();  // 可能返回 -1
uint8_t checksum_recv = packet[3];             // 从网络接收，值为 255

// 两者二进制相同（都是 0xFF），应该认为相等
TEST_ASSERT_EQUAL_U8_BITS(checksum_calc, checksum_recv);  // 成功！
```

## 使用建议

| 场景 | 推荐宏 | 原因 |
|------|--------|------|
| 明确知道类型 | `TEST_ASSERT_EQUAL_U8/I8` 等 | 类型安全，输出精确 |
| 比较二进制数据 | `TEST_ASSERT_EQUAL_U8_BITS` 等 | 忽略符号，只比较位模式 |
| 混合符号类型 | `TEST_ASSERT_EQUAL_U8_BITS` 等 | 避免整型提升陷阱 |
| 一般整数比较 | 保留原有 `TEST_ASSERT_EQUAL_INT` | 简单场景够用 |

## 错误输出示例

当断言失败时，新宏会输出详细的类型信息：

```
Test failed: test.c:25, expected u8 171 (0xAB), got 170 (0xAA)
Test failed: test.c:30, 8-bit mismatch: 0xFF != 0xFE 
    (decimal: 255 != 254, signed: -1 != -2)
```

## 完整示例

```c
#include "test.h"

TEST_CASE(test_protocol_validation)
{
    // 协议头部字节验证
    uint8_t header[] = {0xAA, 0x55, 0x01, 0xFF};
    
    TEST_ASSERT_EQUAL_U8(0xAA, header[0]);  // 同步字节1
    TEST_ASSERT_EQUAL_U8(0x55, header[1]);  // 同步字节2
    TEST_ASSERT_EQUAL_U8(0x01, header[2]);  // 命令字节
    
    // 校验和可能是从有符号运算得到
    int8_t expected_checksum = -1;  // 计算值
    TEST_ASSERT_EQUAL_U8_BITS(expected_checksum, header[3]);  // 比较原始字节
}
```

## 兼容性

- 需要 C99 或更高版本（使用 `<stdint.h>`）
- 向后兼容：原有 `TEST_ASSERT_EQUAL_INT/UINT` 等宏保持不变
- 零运行时开销：纯宏实现，无函数调用

## 实现文件

- `test.h` - 断言宏定义
- `test_enhanced_assertions.c` - 使用示例和测试用例
