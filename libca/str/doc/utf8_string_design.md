---
version: 1.0
update: 2026-05-31
---

# libca/str UTF-8 字符串设计文档

## 1. 核心思想

提供不可变的 UTF-8 字符串类型 `Utf8String`（持有所有权）和 `Utf8StringRef`（非拥有引用），
命名空间 `ca::str`。底层基于 `u8`（`uint8_t`）类型存储字节数据，
支持码点（code point）访问和迭代。

### 设计动机

在 libca 体系中，字符编码统一为 UTF-8。现有的 `ca::String` 是可变的，
而许多场景只需要只读的字符串视图或不可变字符串。
`Utf8String` / `Utf8StringRef` 填补了这一空白。

### 不可变性

- `Utf8String` 一旦构造完成内容不可修改，所有"修改"操作（substr 等）返回新实例
- 无需考虑 COW（Copy-on-Write）、扩容、写时复制等可变字符串的复杂度
- 天然线程安全（无竞争条件）

## 2. 实现细节

### 2.1 数据结构

```cpp
// Utf8String — 拥有所有权的不可变字符串
struct Utf8String {
    u8*   data_;       // 堆分配的 UTF-8 字节数据，始终以 '\0' 结尾
    usize byteLength_; // 字节长度（不含结尾 '\0'）
    usize length_;     // 码点个数（缓存，O(1) 访问）
};

// Utf8StringRef — 非拥有引用的字符串视图
struct Utf8StringRef {
    const u8* data_;       // 指向外部 UTF-8 字节数据
    usize     byteLength_; // 字节长度
    usize     length_;     // 码点个数（缓存）
};
```

### 2.2 关键接口

**Utf8String**
| 接口 | 说明 |
|------|------|
| `Utf8String()` | 构造空字符串 |
| `Utf8String(const u8*, usize)` | 从字节数组构造（复制 + 校验 UTF-8 合法性） |
| `Utf8String(const char*)` | 从 C 字符串构造 |
| `Utf8String(const Utf8String&)` | 拷贝构造 |
| `Utf8String(Utf8String&&)` | 移动构造 |
| `fromCodePoint(u32)` | 从单个码点构造 |
| `fromUtf8(const u8*, usize)` | 工厂方法 |
| `length()` | 码点个数（O(1)） |
| `byteLength()` | 字节长度（O(1)） |
| `isEmpty()` | 是否为空 |
| `data()` | 原始字节指针 |
| `cStr()` | C 风格字符串（O(1)，始终 null-terminated） |
| `byteAt(index)` | 字节下标访问（O(1)） |
| `codePointAt(index)` | 码点下标访问（O(n) 扫描） |
| `ref()` | 获取 Utf8StringRef 视图 |
| `slice(byteStart, byteEnd)` | 按字节区间切片 → Utf8StringRef |
| `sliceByCp(cpStart, cpCount)` | 按码点区间切片 → Utf8StringRef |
| `substr(cpStart, cpCount)` | 按码点取子串 → Utf8String（新分配） |
| `compare(other)` | 逐字节字典序比较 |
| `equals(other)` | 内容相等判断 |
| `operator== / !=` | 相等/不等比较 |

**Utf8StringRef**
| 接口 | 说明 |
|------|------|
| `Utf8StringRef()` | 空视图 |
| `Utf8StringRef(const u8*, usize, usize)` | 从字节数据+码点数量构造 |
| `Utf8StringRef(const Utf8String&)` | 从 Utf8String 构造视图 |
| 查询/访问 | 同 Utf8String（data 返回 const u8*） |
| slice/sliceByCp | 返回新的 Utf8StringRef |
| substr | 返回 Utf8String（新分配） |

**工具函数**
| 函数 | 说明 |
|------|------|
| `utf8CodePointBytes(firstByte)` | 根据首字节获取码点字节数 |
| `utf8DecodeCodePoint(bytes)` | 解码 UTF-8 字节 → u32 码点值 |
| `utf8EncodeCodePoint(cp, out)` | 编码码点 → UTF-8 字节，返回写入字节数 |
| `utf8CountCodePoints(data, len)` | 统计码点数量，遇非法序列返回 0 |
| `utf8IsValid(data, len)` | 校验 UTF-8 合法性 |

### 2.3 内存布局

```
Utf8String 内部存储:
┌──────────┬──────────────┬──────────┬──────┐
│  data_   │ byteLength_  │ length_  │ ...  │
│ (u8*)    │ (usize)      │ (usize)  │      │
└────┬─────┴──────────────┴──────────┴──────┘
     │
     ▼   byteLength_ bytes          1 byte
  ┌────┬────┬────┬────┬────┬────┬────┬────┐
  │ b0 │ b1 │ b2 │... │ bn │\0  │    │    │
  └────┴────┴────┴────┴────┴────┴────┴────┘
  allocated: byteLength_ + 1 字节

Utf8StringRef 内部存储:
┌──────────┬──────────────┬──────────┐
│  data_   │ byteLength_  │ length_  │
│ (const   │ (usize)      │ (usize)  │
│  u8*)    │              │          │
└──────────┴──────────────┴──────────┘
  指向外部数据，不持有所有权
```

## 3. 算法设计

### 3.1 UTF-8 编解码

```
UTF-8 编码规则:
┌────────────┬──────────────────────┬──────────────────────────┐
│ 码点范围    │ 字节序列              │ 首字节特征                │
├────────────┼──────────────────────┼──────────────────────────┤
│ U+0000-007F│ 0xxxxxxx            │ 0xxx xxxx (0x00-0x7F)    │
│ U+0080-07FF│ 110xxxxx 10xxxxxx   │ 110x xxxx (0xC0-0xDF)    │
│ U+0800-FFFF│ 1110xxxx 10xxxxxx.. │ 1110 xxxx (0xE0-0xEF)    │
│ U+10000-10FFFF│ 11110xxx 10xxxxxx.. │ 1111 0xxx (0xF0-0xF7)    │
└────────────┴──────────────────────┴──────────────────────────┘

码点解码（decode）:
  1字节: byte_0
  2字节: ((b0 & 0x1F) << 6) | (b1 & 0x3F)
  3字节: ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F)
  4字节: ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F)

码点编码（encode）:
  U+0000-007F: byte_0 = cp
  U+0080-07FF: byte_0 = 0xC0 | (cp >> 6), byte_1 = 0x80 | (cp & 0x3F)
  U+0800-FFFF: byte_0 = 0xE0 | (cp >> 12), ...
  U+10000-10FFFF: byte_0 = 0xF0 | (cp >> 18), ...
```

### 3.2 码点下标访问

`codePointAt(index)` 需要从字节起点开始扫描，累计跳过的码点数量：
```
pos = 0, cpIdx = 0
while cpIdx < index AND pos < byteLength:
    len = utf8CodePointBytes(data_[pos])
    pos += len
    cpIdx++
return utf8DecodeCodePoint(data_ + pos)
```
时间复杂度 O(n)。码点个数已缓存，所以 `length()` 是 O(1)。

### 3.3 切片

切片操作返回 `Utf8StringRef`（非拥有），不分配内存：
```
slice(byteStart, byteEnd): 截取 data_[byteStart..byteEnd) 区间
sliceByCp(cpStart, cpCount): 先扫描到第 cpStart 个码点的字节位置，
                             再扫描 cpCount 个码点得到 byteEnd
```

## 4. 架构设计

### 4.1 架构概览

```
libca/core/src/libca/core/datatype.hpp    ← u8, usize 基础类型
          ↕
libca/str/src/utf8_string.hpp              ← Utf8String + Utf8StringRef 声明
libca/str/src/utf8_string.cpp              ← 实现
libca/str/src/str.hpp                      ← 聚合头文件
          ↕
libca/str/unittest/utf8_string_test.cpp    ← Google Test 测试
```

### 4.2 依赖关系

- `Utf8String` → `Utf8StringRef`（`ref()` 返回 ref）
- `Utf8StringRef` → `Utf8String`（从 `Utf8String` 构造）
- 交叉引用通过前向声明解决
- 跨模块引用使用 `<libca/core/datatype.hpp>` 风格

## 5. 性能考虑

### 5.1 时间复杂度

| 操作 | Utf8String | Utf8StringRef |
|------|-----------|--------------|
| 构造（从数据） | O(n) 校验+复制+计码点 | O(1) |
| length() | O(1) 缓存 | O(1) 缓存 |
| byteLength() | O(1) | O(1) |
| byteAt() | O(1) | O(1) |
| codePointAt() | O(n) 扫描 | O(n) 扫描 |
| slice() | O(1) | O(1) |
| sliceByCp() | O(n) 扫描到起点 | O(n) 扫描到起点 |
| compare() | O(n) 字节比较 | O(n) 字节比较 |
| substr() | O(n) 分配+复制 | O(n) 分配+复制 |

### 5.2 优化策略

- 码点个数在构造时缓存，避免反复扫描
- `cStr()` O(1) 返回，内部始终预留 `\0` 结尾空间
- `Utf8StringRef` 轻量（3 个指针/整数），推荐传值

## 6. 使用示例

```cpp
#include <libca/str/str.hpp>
#include <cstdio>

using namespace ca::str;

int main() {
    // 从 C 字符串构造
    Utf8String s("你好世界 Hello");
    
    printf("length: %zu\n", s.length());       // 码点个数
    printf("bytes:  %zu\n", s.byteLength());   // 字节个数
    
    // 码点访问
    u32 cp = s.codePointAt(0);  // '你' 的码点 U+4F60
    
    // 切片
    auto ref = s.sliceByCp(0, 2);  // "你好"
    
    // 子串
    auto sub = s.substr(2, 3);     // "世界 "
    
    // 比较
    if (s == sub) { /* ... */ }
    
    // C 字符串导出
    printf("%s\n", s.cStr());
    
    return 0;
}
```
