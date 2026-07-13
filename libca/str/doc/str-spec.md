# UTF-8 字符串 — 类型规范

> 路径：`libca/str/src/libca/str/`
> 命名空间 `ca::str`，UTF-8 是唯一内部编码。

## 类型体系

| 类型 | 所有权 | 可变 | \0 保证 | 用途 |
|------|--------|------|---------|------|
| `Utf8String` | 拥有 | 不可变 | 是 | 主要字符串类型，clone 为唯一复制方式 |
| `Utf8StringRef` | 非拥有（视图） | 不可变 | 否 | 零拷贝切片、参数传递 |
| `ZUtf8StringRef` | 非拥有（视图） | 不可变 | 是 | 字面量/全局常量的缓存视图 |
| `Utf8StringBuilder` | 拥有 | 可变 | — | 增量构建字符串 |
| `Utf8Iterator` | — | 前向迭代器 | — | 遍历码点，配合范围 for |
| `Utf8StringArena` | 拥有（整批） | — | 否 | 追加式去重池，整批共存共死 |
| `Utf8StringPool` | 拥有（按引用计数回收） | — | 否 | 引用计数字符串池，条目寿命各异 |
| `Utf8StringPooledPtr` | 共享（引用计数句柄） | 不可变 | 否 | `Utf8StringPool::intern()` 的返回，8 字节 |
| `Utf8Twine` | — | 不可变 | 否 | 惰性拼接二叉树，materialize 时一次性产出 |

> **所有权分层**：拥有型（`Utf8String`/池）→ 非拥有视图（`Utf8StringRef`/`ZUtf8StringRef`）→
> 惰性拼接（`Utf8Twine`）。详见各类型章节与 `utf8_string_design.md`。

## Utf8String — 拥有所有权的不可变字符串

### 构造

| 方法 | 说明 |
|------|------|
| `Utf8String()` | 空字符串 |
| `Utf8String(const u8* data, usize byte_len)` | 从字节构造，复制+校验 UTF-8 |
| `explicit Utf8String(const char* cstr)` | 从 C 字符串构造 |
| `Utf8String(Utf8String&&)` | 移动构造 |
| `clone() -> Utf8String` | 显式深拷贝（唯一复制方式） |
| `from_cstr(const char*) -> Utf8String` | 工厂：从 C 字符串 |
| `from_data(const u8*, usize, usize) -> Utf8String` | 工厂：从字节数据 |
| `from_code_point(u32) -> Utf8String` | 工厂：从单个码点 |

`Utf8String` 拷贝构造/赋值已删除，必须显式调用 `clone()`。

### 查询

| 方法 | 复杂度 | 说明 |
|------|--------|------|
| `length() -> usize` | O(1) | 码点个数 |
| `byte_length() -> usize` | O(1) | 字节长度 |
| `is_empty() -> bool` | O(1) | 是否为空 |
| `size() -> usize` | O(1) | STL 兼容别名，同 `length()` |
| `empty() -> bool` | O(1) | STL 兼容别名，同 `is_empty()` |
| `data() -> const u8*` | O(1) | 原始字节指针（\0 结尾） |
| `c_str() -> const char*` | O(1) | C 风格字符串（\0 保证） |

### 访问

| 方法 | 复杂度 | 说明 |
|------|--------|------|
| `byte_at(usize index) -> u8` | O(1) | 按字节下标（不边界检查） |
| `code_point_at(usize index) -> u32` | O(n) | 按码点下标（不边界检查） |

### 切片（返回 `Utf8StringRef`，零拷贝）

| 方法 | 说明 |
|------|------|
| `ref() -> Utf8StringRef` | 整个字符串的视图 |
| `slice(byte_start, byte_end) -> Utf8StringRef` | 按字节区间 |
| `slice_by_cp(cp_start, cp_count) -> Utf8StringRef` | 按码点区间 |

### 子串（返回 `Utf8String`，分配）

| 方法 | 说明 |
|------|------|
| `substr(cp_start, cp_count) -> Utf8String` | 按码点取子串 |

### 变换

| 方法 | 返回 | 说明 |
|------|------|------|
| `to_lower()` | `Utf8String` | 小写转换 |
| `to_upper()` | `Utf8String` | 大写转换 |
| `replace_all(from, to)` | `Utf8String` | 全局替换 |

### 查找

| 方法 | 返回 | 说明 |
|------|------|------|
| `index_of(needle) -> usize` | 码点下标 | 子串首次出现，未找到返回 `npos` |
| `index_of(needle, start_cp) -> usize` | 码点下标 | 从指定码点开始查找 |
| `index_of(code_point) -> usize` | 码点下标 | 码点首次出现 |
| `index_of(code_point, start_cp) -> usize` | 码点下标 | 从指定码点开始查找 |
| `contains(needle) -> bool` | — | 是否包含子串 |

### 前缀/后缀/修剪

| 方法 | 返回 | 说明 |
|------|------|------|
| `starts_with(prefix) -> bool` | — | 前缀检查（字节级） |
| `ends_with(suffix) -> bool` | — | 后缀检查（字节级） |
| `trim()` | `Utf8StringRef` | 首尾空白修剪（视图） |
| `trim_start()` | `Utf8StringRef` | 开头空白修剪 |
| `trim_end()` | `Utf8StringRef` | 结尾空白修剪 |

### 拆分

| 方法 | 返回 | 说明 |
|------|------|------|
| `split(delimiter)` | `vector<Utf8StringRef>` | 按分隔符拆分为视图列表 |

### 比较

| 方法/运算符 | 说明 |
|-------------|------|
| `compare(other) -> int` | 逐字节字典序 |
| `equals(other) -> bool` | 内容相等 |
| `operator==` / `operator!=` | 支持 `Utf8String` / `Utf8StringRef` 互相比较 |
| `operator<` / `operator>` / `operator<=` / `operator>=` | 按 UTF-8 字节字典序排序 |

比较运算符以 `Utf8StringRef` 作为公共视图层：排序重载定义为非成员函数，
`Utf8String` 通过隐式构造为 `Utf8StringRef` 参与比较。这样 `Utf8String`、
`Utf8StringRef` 和 `const char*` 的常用组合都能复用同一组视图比较逻辑，避免在拥有型
字符串和视图类型上重复展开成员排序重载。

### 转换

| 方法 | 说明 |
|------|------|
| `to_std_string() -> std::string` | 创建标准库字符串（分配） |
| `operator std::string_view() -> std::string_view` | 零拷贝转换为标准库视图 |

## Utf8StringRef — 非拥有字符串视图

### 构造

| 方法 | 说明 |
|------|------|
| `Utf8StringRef()` | 空视图 |
| `Utf8StringRef(data, byte_len, length)` | 从字节+码点个数（不校验） |
| `Utf8StringRef(const Utf8String&)` | 从 `Utf8String` 构造 |
| `from_cstr(cstr) -> Utf8StringRef` | 从 C 字符串（O(n) 计算码点） |
| `from_data(data, byte_len, cp_len) -> Utf8StringRef` | 从字节数据 |
| `from_string_view(sv) -> Utf8StringRef` | 从 `std::string_view`（不复制，O(n) 计算码点） |

### 方法

与 `Utf8String` 共享相同的查询/访问/切片/查找/比较接口（见上），区别：

- **不提供** `c_str()` — 视图不保证 \0 终止
- `slice()` / `substr()` 均以视图形式返回
- `substr()` 返回 `Utf8String`（分配）

### 标准库互操作

| 方法 | 说明 |
|------|------|
| `operator std::string_view() -> std::string_view` | 零拷贝转换；视图不保证 \0，但 `string_view` 只看 `[data, data+size)` 无需 \0 |
| `to_std_string() -> std::string` | 创建标准库字符串（分配） |

### 常量

```
static constexpr usize npos = usize(-1);  // 查找未找到
```

## ZUtf8StringRef — \0 终止的字符串视图

用于字面量和全局常量场景，保证 \0 终止以支持 `c_str()`。

| 方法 | 说明 |
|------|------|
| `from_static(const char*) -> ZUtf8StringRef` | 从字面量/全局常量（有全局缓存优化） |
| `from_utf8_string(const Utf8String&) -> ZUtf8StringRef` | 从 `Utf8String`（保证 \0） |
| `from_std_string(const string&) -> ZUtf8StringRef` | 从 `std::string`（自行保证内容） |
| `data() -> const u8*` | 原始字节 |
| `c_str() -> const char*` | C 风格字符串（\0 保证） |
| `byte_length() -> usize` | 字节长度 |
| `length() -> usize` | 码点个数 |
| `operator std::string_view()` | 零拷贝转换 |
| `operator Utf8StringRef()` | 隐式降级为普通视图（丢弃 \0 保证） |

> 建议将 `ZUtf8StringRef` 声明为 `static` / 全局变量，避免重复计算码点。

## Utf8StringBuilder — 可变构建器

| 方法 | 说明 |
|------|------|
| `append(str/buffer/data)` | 追加数据 |
| `append_code_point(u32 cp) -> bool` | 追加单个码点 |
| `reserve(byte_capacity)` | 预分配 |
| `capacity() -> usize` | 当前容量 |
| `byte_length() -> usize` | 已写入字节数 |
| `is_empty() -> bool` | 是否为空 |
| `clear()` | 清空 |
| `build() -> Utf8String` | 构建（可能抛异常） |
| `build_or_empty() -> Utf8String` | 构建，失败返回空串 |

## Utf8Iterator — 码点迭代器

```
iterator_category: forward_iterator_tag
value_type:        u32
```

支持范围 for 和标准算法：

```cpp
for (u32 cp : str) {
    // 遍历每个码点
}
```

| 方法 | 说明 |
|------|------|
| `operator*() -> u32` | 当前码点 |
| `operator++()` | 前进一个码点 |
| `byte_ptr() -> const u8*` | 当前位置的字节指针 |

## Utf8StringArena — 追加式去重池

> **Provisional**，**非线程安全**。固定块链式扩展，内存不挪动。

### 构造

| 方法 | 说明 |
|------|------|
| `Utf8StringArena()` | 空池，分配初始 64KB chunk |
| `Utf8StringArena(Utf8StringArena&&)` | 移动构造 |
| 拷贝构造/赋值 | 已删除 |

### intern（复制入池、去重，返回池内副本视图）

| 方法 | 返回 | 说明 |
|------|------|------|
| `intern(const u8*, usize)` | `Utf8StringRef` | 校验 UTF-8 复制入池；空/非法返回空视图 |
| `intern(const char* cstr)` | `Utf8StringRef` | intern C 字符串 |
| `intern(const Utf8StringRef&)` | `Utf8StringRef` | intern 视图内容 |
| `intern(const Utf8String&)` | `Utf8StringRef` | intern 拥有字符串内容 |

### 统计 / 管理

| 方法 | 说明 |
|------|------|
| `size() -> usize` | 唯一字符串数量 |
| `total_bytes() -> usize` | 已分配 chunk 总容量（含未用空间） |
| `clear()` | 释放所有 chunk，回到空池（此后所有 ref 失效） |

> **生命周期契约**：返回的 `Utf8StringRef` 绑定 arena——arena 析构 / `clear()` / 移动赋值后全部失效。
> 选型：一批字符串有共同死亡点用 arena；寿命各异用 `Utf8StringPool`。

## Utf8StringPool — 引用计数字符串池

> **Provisional**，**非线程安全**。条目堆分配、指针稳定，按引用计数**真删**（非墓碑）。

### 构造

| 方法 | 说明 |
|------|------|
| `Utf8StringPool()` | 空池 |
| `Utf8StringPool(Utf8StringPool&&)` | 移动构造 |
| 拷贝构造/赋值 | 已删除 |

### intern（复制入池、去重，返回引用计数句柄）

| 方法 | 返回 | 说明 |
|------|------|------|
| `intern(const u8*, usize)` | `Utf8StringPooledPtr` | 校验 UTF-8 复制入池；空/非法返回空句柄 |
| `intern(const char* cstr)` | `Utf8StringPooledPtr` | intern C 字符串 |
| `intern(const Utf8StringRef&)` | `Utf8StringPooledPtr` | intern 视图内容 |

### 查找（不分配、不改 ref_count）

| 方法 | 说明 |
|------|------|
| `find(const Utf8StringRef&) -> Utf8StringPooledPtr` | 命中返回持有该条目的句柄（refcount++），未命中返回空句柄 |

### 统计 / 管理

| 方法 | 说明 |
|------|------|
| `size() -> usize` | 唯一条目数（真删后 == 活条目数） |
| `active_entries() -> usize` | 活跃条目数（ref_count > 0） |
| `total_bytes() -> usize` | 活条目字节和 |
| `clear()` | disown_all fail-safe，回到空池（残留句柄转自管，不 UAF） |

> **生命周期契约（核心）**：
> - **真删**：`PooledPtr` refcount 归零即 delete 该 entry（非墓碑）。
> - **outlive 是软契约**：Pool 先于 PooledPtr 析构 / `clear()` / move-assign 时**不会 UAF**（走 disown fail-safe，存活句柄自管释放），但失去去重收益。
> - **Ref 由借用纪律保证**：`PooledPtr` 析构后，由它 `.ref()` 派生的 `Utf8StringRef` 失效。

## Utf8StringPooledPtr — 引用计数池化句柄

> 8 字节（一个 `Utf8PoolEntry*`）。拷贝/赋值 refcount++，析构 refcount--，归零真删或（被 disown 时）自管释放。

### 特殊成员函数

| 方法 | 说明 |
|------|------|
| 默认构造 | 空句柄 |
| 拷贝构造 / 赋值 | refcount++ |
| 移动构造 / 赋值 | 转移所有权，不改变 refcount |
| 析构 | refcount--，到 0 释放 |

### 访问 / 转换

| 方法 | 说明 |
|------|------|
| `data() -> const u8*` | 原始字节 |
| `byte_length() / length() / is_empty()` | 字节长度 / 码点数 / 是否空 |
| `explicit operator bool()` | 是否持有条目 |
| `ref() -> Utf8StringRef` | 转视图（PooledPtr 须存活） |
| `operator Utf8StringRef() const&` | 隐式转视图（仅左值；右值已 `delete` 防 UAF） |

### 比较

| 运算符 | 说明 |
|--------|------|
| `operator== / != (Utf8StringPooledPtr)` | 指针相等语义 |
| `operator== / != (Utf8StringRef)` | 内容比较（非成员对称重载） |

## Utf8Twine — 惰性拼接

> LLVM Twine 风格二叉拼接树，拼接零分配。**借用语义同 `Utf8StringRef`**：只在单表达式内作参数，绝不存成员、绝不跨语句；仅 `materialize()` / `to_string()` 时一次性产出。

### 叶子构造（隐式，使 `"a" + ref` 等表达式成立）

| 构造 | 说明 |
|------|------|
| `Utf8Twine()` | 空 |
| `Utf8Twine(const char*)` | C 字面量 |
| `Utf8Twine(const Utf8StringRef&)` | 视图（PooledPtr/ZUtf8StringRef 经隐式转换走这里） |
| `Utf8Twine(const Utf8String&)` | 取其视图 |

### 拼接 / 查询

| 方法 | 说明 |
|------|------|
| `concat(rhs) -> Utf8Twine` | 拼接（产生新节点） |
| `operator+(Utf8Twine, Utf8Twine)` | 非成员，两侧经隐式转换 |
| `is_empty() -> bool` | 是否为空 |
| `byte_length() -> usize` | 各片段字节和（O(片段数)） |

### 一次性产出

| 方法 | 返回 | 说明 |
|------|------|------|
| `to_string()` | `Utf8String` | 独立拥有者 |
| `materialize(Utf8StringArena&)` | `Utf8StringRef` | intern 进 arena |
| `materialize(Utf8StringPool&)` | `Utf8StringPooledPtr` | intern 进 pool（含单叶子快速路径） |

## 非成员函数

| 函数 | 说明 |
|------|------|
| `split(str, delimiter) -> vector<Utf8StringRef>` | 按分隔符拆分 |
| `join(parts, separator) -> Utf8String` | 用分隔符连接 |
| `operator<<(os, s) -> ostream&` | 流输出（Utf8StringRef / Utf8String） |
| `operator+(Utf8Twine, Utf8Twine) -> Utf8Twine` | Twine 拼接 |

## 设计决策

- `Utf8String` 不可变 — 线程安全，Rust `String` 语义对齐
- `clone()` 显式 — 避免意外深拷贝
- UTF-8 校验在构造时一次性完成
- 所有码点下标访问为 O(n)（需扫描），字节下标访问为 O(1)
