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
| `to_std_string() -> std::string` | 创建标准库字符串 |

## Utf8StringRef — 非拥有字符串视图

### 构造

| 方法 | 说明 |
|------|------|
| `Utf8StringRef()` | 空视图 |
| `Utf8StringRef(data, byte_len, length)` | 从字节+码点个数（不校验） |
| `Utf8StringRef(const Utf8String&)` | 从 `Utf8String` 构造 |
| `from_cstr(cstr) -> Utf8StringRef` | 从 C 字符串（O(n) 计算码点） |
| `from_data(data, byte_len, cp_len) -> Utf8StringRef` | 从字节数据 |

### 方法

与 `Utf8String` 共享相同的查询/访问/切片/查找/比较接口（见上），区别：

- **不提供** `c_str()` — 视图不保证 \0 终止
- `slice()` / `substr()` 均以视图形式返回
- `substr()` 返回 `Utf8String`（分配）

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

## 非成员函数

| 函数 | 说明 |
|------|------|
| `split(str, delimiter) -> vector<Utf8StringRef>` | 按分隔符拆分 |
| `join(parts, separator) -> Utf8String` | 用分隔符连接 |
| `operator<<(os, s) -> ostream&` | 流输出 |

## 设计决策

- `Utf8String` 不可变 — 线程安全，Rust `String` 语义对齐
- `clone()` 显式 — 避免意外深拷贝
- UTF-8 校验在构造时一次性完成
- 所有码点下标访问为 O(n)（需扫描），字节下标访问为 O(1)
