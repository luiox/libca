# Bytes / BytesMut — 类型规范

> 路径：`libca/core/src/libca/core/bytes.hpp`
> 对齐 Rust [`bytes`](https://docs.rs/bytes/latest/bytes/) crate，代替旧 `ByteBuffer`。

## 类型体系

| 类型 | 所有权 | 可变 | 存储 | 用途 |
|------|--------|------|------|------|
| `Bytes` | 共享（引用计数） | 不可变 | `shared_ptr<u8>` | 零拷贝切片、只读数据传递 |
| `BytesMut` | 唯一 | 可变 | `unique_ptr<u8[]>` | 缓冲区构建、序列化 |

`BytesMut::freeze()` → `Bytes`（转移所有权，零拷贝）。

## Bytes — 不可变字节序列

### 工厂

| 方法 | 说明 |
|------|------|
| `Bytes()` | 空序列 |
| `from_static(data, len)` | 引用静态/全局数据（不复制，无 storage） |
| `copy_from_slice(data, len)` | 从字节数组复制 |

拷贝构造/赋值可用（浅增引用计数）。

### 查询

| 方法 | 说明 |
|------|------|
| `len() -> usize` | 字节长度 |
| `is_empty() -> bool` | 是否为空 |
| `as_ptr() -> const u8*` | 指向当前读位置的指针 |

### 读游标

| 方法 | Rust 对应 | 说明 |
|------|-----------|------|
| `remaining() -> usize` | `remaining()` | 剩余可读字节数 |
| `advance(cnt)` | `advance()` | 前进游标，跳过 cnt 字节 |

### 切片（零拷贝）

| 方法 | 说明 |
|------|------|
| `slice(begin, end) -> Bytes` | 返回共享同一 storage 的切片 |

### 类型化读（前进游标）

| 方法 | 端序 |
|------|------|
| `get_u8()` | — |
| `get_u16()` / `get_u16_le()` | 大端 / 小端 |
| `get_u32()` / `get_u32_le()` | 大端 / 小端 |
| `get_u64()` / `get_u64_le()` | 大端 / 小端 |
| `get_i16()` / `get_i16_le()` | 大端 / 小端 |
| `get_i32()` / `get_i32_le()` | 大端 / 小端 |
| `get_i64()` / `get_i64_le()` | 大端 / 小端 |
| `get_f32()` / `get_f64()` | 大端（网络序） |

后缀 `_le` = 小端，无后缀 = 大端（网络字节序）。

### 批量读

| 方法 | 说明 |
|------|------|
| `copy_to_slice(dst, len)` | 复制 `len` 字节到目标缓冲区，前进游标 |

越界访问抛出 `std::out_of_range`。

## BytesMut — 可变字节缓冲区

### 工厂

| 方法 | 说明 |
|------|------|
| `BytesMut()` | 空缓冲区（无分配） |
| `with_capacity(cap)` | 预分配指定容量 |

拷贝构造/赋值可用（深拷贝），移动构造/赋值可用（转移所有权）。

### 查询

| 方法 | 说明 |
|------|------|
| `len() -> usize` | 已写入字节数 |
| `is_empty() -> bool` | 是否为空 |
| `as_ptr() -> const u8*` | 指向当前读位置的指针 |
| `as_mut_ptr() -> u8*` | 指向当前读位置的可写指针 |

### 读游标

| 方法 | 说明 |
|------|------|
| `remaining() -> usize` | 剩余可读字节数（`len - pos`） |
| `advance(cnt)` | 前进读游标 |

### 写剩余空间

| 方法 | Rust 对应 | 说明 |
|------|-----------|------|
| `remaining_mut() -> usize` | `remaining_mut()` | 剩余可写字节数（`capacity - len`） |

### 容量管理

| 方法 | Rust 对应 | 说明 |
|------|-----------|------|
| `reserve(additional)` | `reserve()` | 确保至少还可写 `additional` 字节 |
| `clear()` | `clear()` | 清空内容，游标归零（不释放内存） |
| `truncate(len)` | `truncate()` | 截断到指定长度 |

扩容策略：容量翻倍，最小 32 字节，不小于请求值。

### 批量写

| 方法 | 说明 |
|------|------|
| `put_slice(data, len)` | 从字节数组追加写入 |

### 类型化写（追加写入，前进游标）

| 方法 | 端序 |
|------|------|
| `put_u8(val)` | — |
| `put_u16(val)` / `put_u16_le(val)` | 大端 / 小端 |
| `put_u32(val)` / `put_u32_le(val)` | 大端 / 小端 |
| `put_u64(val)` / `put_u64_le(val)` | 大端 / 小端 |
| `put_i16(val)` / `put_i16_le(val)` | 大端 / 小端 |
| `put_i32(val)` / `put_i32_le(val)` | 大端 / 小端 |
| `put_i64(val)` / `put_i64_le(val)` | 大端 / 小端 |
| `put_f32(val)` / `put_f64(val)` | 大端（网络序） |

### 类型化读（前进游标）

与 `Bytes` 接口相同：`get_u8()`、`get_u16()`、`get_u32()` 等（见上表）。

### 冻结

| 方法 | Rust 对应 | 说明 |
|------|-----------|------|
| `freeze() -> Bytes` | `freeze()` | 将所有权转移为共享 `Bytes`，自身清零（零拷贝） |

### 比较

| 方法/运算符 | 说明 |
|-------------|------|
| `equals(other) -> bool` | 内容相等（仅剩余部分） |
| `operator==` / `operator!=` | 委托 `equals()` |

## 与旧 ByteBuffer 的差异

| 旧 ByteBuffer | Bytes / BytesMut |
|---------------|------------------|
| `flip()` | ❌ 移除 — 读写分离通过独立游标实现 |
| `position()` setter | ❌ 移除 |
| `capacity()` 查询 | ❌ 移除（BytesMut 仅 `remaining_mut()`） |
| `ByteOrder` 状态枚举 | ❌ 移除 — 端序显式在方法名 |
| 绝对位置读写 | ❌ 移除 |
| 单个可变类型 | ✅ 拆为 `Bytes`（不可变）+ `BytesMut`（可变） |
| 不可变序列零拷贝切片 | ✅ `Bytes::slice()` |

## 设计决策

- `shared_ptr<u8>` + `std::default_delete<u8[]>()` 作为 `Bytes` 共享存储
- `from_static` 时 `storage_` 为 `nullptr`，`slice()` 通过 `storage_` 有无决定是否共享
- 无后缀 = 大端（网络字节序），`_le` = 小端，对齐业界惯例
- 所有 throw 异常为 `std::out_of_range`（Result 体系推广后改为 `Result`）
