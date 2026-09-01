/// @file utf16_string.hpp
/// @brief Java 风格 UTF-16 字符串：Char16、Utf16StringRef、Utf16String、Utf16StringBuilder。
/// @note length() 返回 UTF-16 code unit 数量；char_at() O(1)；code_point_at() 感知代理对。

#pragma once

#include "libca/core/datatype.hpp"

#include "utf8_string.hpp"

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace ca::str {

/// @brief UTF-16 码元值类型，内部固定为 ca::u16，不使用标准 char16_t 作为存储类型。
struct Char16
{
    /// @brief 原始 UTF-16 code unit。
    ca::u16 value;

    constexpr Char16() noexcept
        : value(0)
    {}

    /// @brief 用原始 UTF-16 code unit 构造。
    constexpr explicit Char16(ca::u16 unit) noexcept
        : value(unit)
    {}

    /// @brief 返回原始 UTF-16 code unit。
    constexpr ca::u16  unit() const noexcept { return value; }
    constexpr explicit operator ca::u16() const noexcept { return value; }

    /// @brief 判断是否位于 UTF-16 surrogate 区间。
    constexpr bool is_surrogate() const noexcept { return value >= 0xD800 && value <= 0xDFFF; }

    /// @brief 判断是否为 UTF-16 high surrogate。
    constexpr bool is_lead_surrogate() const noexcept { return value >= 0xD800 && value <= 0xDBFF; }

    /// @brief 判断是否为 UTF-16 low surrogate。
    constexpr bool is_trail_surrogate() const noexcept
    {
        return value >= 0xDC00 && value <= 0xDFFF;
    }

    /// @brief 判断是否为非 surrogate 的 BMP code unit。
    constexpr bool is_bmp() const noexcept { return value <= 0xD7FF || value >= 0xE000; }

    constexpr bool operator==(Char16 other) const noexcept { return value == other.value; }
    constexpr bool operator!=(Char16 other) const noexcept { return value != other.value; }
    constexpr bool operator<(Char16 other) const noexcept { return value < other.value; }
    constexpr bool operator>(Char16 other) const noexcept { return value > other.value; }
    constexpr bool operator<=(Char16 other) const noexcept { return value <= other.value; }
    constexpr bool operator>=(Char16 other) const noexcept { return value >= other.value; }
};

static_assert(sizeof(Char16) == sizeof(ca::u16), "Char16 must stay a single UTF-16 unit");

class Utf16String;

/// @brief 非拥有 UTF-16 字符串视图，按 Java char[] 语义暴露 code unit。
///
/// `Utf16StringRef` 不拥有底层内存，调用方必须保证 `data()` 生命周期覆盖视图使用期。
/// 所有下标均为 UTF-16 code unit 下标；`code_point_at()` 会在当前位置是有效
/// surrogate pair 起点时合并为 Unicode code point。
class Utf16StringRef
{
public:
    /// @brief 查找失败时返回的哨兵值。
    static constexpr ca::usize npos = ca::usize(-1);

    constexpr Utf16StringRef() noexcept
        : data_(nullptr)
        , length_(0)
    {}
    constexpr Utf16StringRef(const Char16* data, ca::usize length) noexcept
        : data_(data)
        , length_(length)
    {}
    Utf16StringRef(const Utf16String& str) noexcept;

    /// @brief 从原始 u16 数组创建非拥有视图。
    static Utf16StringRef from_data(const ca::u16* data, ca::usize length) noexcept;

    /// @brief 从 `std::u16string` 创建非拥有视图。
    static Utf16StringRef from_std_u16string(const std::u16string& str) noexcept;

    /// @brief 返回 UTF-16 code unit 数量。
    constexpr ca::usize     length() const noexcept { return length_; }
    constexpr ca::usize     size() const noexcept { return length_; }
    constexpr bool          is_empty() const noexcept { return length_ == 0; }
    constexpr bool          empty() const noexcept { return length_ == 0; }
    constexpr const Char16* data() const noexcept { return data_; }
    /// @brief 返回原始 code unit 指针。
    const ca::u16* raw_data() const noexcept;

    /// @brief 返回 index 位置的 UTF-16 code unit；越界返回 0。
    Char16 char_at(ca::usize index) const noexcept;

    /// @brief 返回 index 位置的 Unicode code point；有效 surrogate pair 会被合并。
    ca::u32 code_point_at(ca::usize index) const noexcept;

    /// @brief 返回 `[begin, end)` 的非拥有子视图；非法或空范围返回空视图。
    Utf16StringRef slice(ca::usize begin, ca::usize end) const noexcept;

    /// @brief 复制 `[begin, end)` 范围为新的拥有型字符串。
    Utf16String substring(ca::usize begin, ca::usize end) const;

    /// @brief 查找 code unit 或子串，未找到返回 npos。
    ca::usize index_of(Char16 ch) const noexcept;
    ca::usize index_of(Char16 ch, ca::usize from) const noexcept;
    ca::usize index_of(const Utf16StringRef& needle) const noexcept;
    ca::usize index_of(const Utf16StringRef& needle, ca::usize from) const noexcept;
    /// @brief 从后向前查找 code unit 或子串，未找到返回 npos。
    ca::usize last_index_of(Char16 ch) const noexcept;
    ca::usize last_index_of(Char16 ch, ca::usize from) const noexcept;
    ca::usize last_index_of(const Utf16StringRef& needle) const noexcept;
    ca::usize last_index_of(const Utf16StringRef& needle, ca::usize from) const noexcept;
    /// @brief 判断是否以 prefix 开头。
    bool starts_with(const Utf16StringRef& prefix) const noexcept;

    /// @brief 判断 offset 位置是否以 prefix 开头。
    bool starts_with(const Utf16StringRef& prefix, ca::usize offset) const noexcept;

    /// @brief 判断是否以 suffix 结尾。
    bool ends_with(const Utf16StringRef& suffix) const noexcept;

    /// @brief 判断是否包含 needle。
    bool contains(const Utf16StringRef& needle) const noexcept;

    /// @brief 按 UTF-16 code unit 字典序比较。
    int compare(const Utf16StringRef& other) const noexcept;

    /// @brief `compare()` 的 Java 命名语义别名。
    int compare_to(const Utf16StringRef& other) const noexcept { return compare(other); }

    /// @brief 判断两个 UTF-16 code unit 序列是否完全相等。
    bool equals(const Utf16StringRef& other) const noexcept;

    /// @brief 返回 Java `String.hash_code()` 兼容的 32-bit hash。
    ca::i32 hash_code() const noexcept;

    /// @brief 拼接当前视图和 other，返回新的拥有型字符串。
    Utf16String concat(const Utf16StringRef& other) const;

    /// @brief 复制当前视图为拥有型字符串。
    Utf16String to_string() const;

    /// @brief 复制为 `std::u16string`。
    std::u16string to_std_u16_string() const;

    /// @brief 转换为 UTF-8 字符串。
    /// @throws std::runtime_error 当底层 UTF-16 序列非法。
    Utf8String to_utf8_string() const;

    bool operator==(const Utf16StringRef& other) const noexcept { return equals(other); }
    bool operator!=(const Utf16StringRef& other) const noexcept { return !equals(other); }

private:
    const Char16* data_;
    ca::usize     length_;
};

/// @brief 拥有所有权、不可变的 UTF-16 字符串。
///
/// `Utf16String` 复制输入 code unit 到自有存储，不共享外部缓冲区。复制语义通过
/// `clone()` 显式表达，类型本身只支持移动，避免隐式大拷贝。
class Utf16String
{
public:
    Utf16String() noexcept;
    Utf16String(const Char16* data, ca::usize length);
    Utf16String(const ca::u16* data, ca::usize length);
    explicit Utf16String(const std::u16string& str);

    Utf16String(const Utf16String&)            = delete;
    Utf16String& operator=(const Utf16String&) = delete;

    Utf16String(Utf16String&& other) noexcept;
    Utf16String& operator=(Utf16String&& other) noexcept;
    ~Utf16String();

    /// @brief 深拷贝当前字符串。
    Utf16String clone() const;

    /// @brief 从 UTF-8 视图转换为 UTF-16 字符串。
    /// @throws std::runtime_error 当输入 UTF-8 非法。
    static Utf16String from_utf8_string(const Utf8StringRef& str);

    /// @brief 从单个 Unicode code point 构造 UTF-16 字符串。
    /// @throws std::runtime_error 当 code point 非法。
    static Utf16String from_code_point(ca::u32 code_point);

    /// @brief 按 Java `String.valueOf` 常用语义构造 UTF-16 字符串。
    static Utf16String value_of(bool value);
    static Utf16String value_of(Char16 value);
    static Utf16String value_of(ca::i32 value);
    static Utf16String value_of(ca::i64 value);
    static Utf16String value_of(ca::f32 value);
    static Utf16String value_of(ca::f64 value);
    static Utf16String value_of(const char* utf8);
    static Utf16String value_of(const Utf8StringRef& utf8);
    static Utf16String value_of(const Utf16StringRef& value);

    ca::usize      length() const noexcept { return length_; }
    ca::usize      size() const noexcept { return length_; }
    bool           is_empty() const noexcept { return length_ == 0; }
    bool           empty() const noexcept { return length_ == 0; }
    const Char16*  data() const noexcept { return data_; }
    const ca::u16* raw_data() const noexcept;

    Char16  char_at(ca::usize index) const noexcept { return ref().char_at(index); }
    ca::u32 code_point_at(ca::usize index) const noexcept { return ref().code_point_at(index); }

    Utf16StringRef ref() const noexcept { return Utf16StringRef(data_, length_); }
    Utf16StringRef slice(ca::usize begin, ca::usize end) const noexcept
    {
        return ref().slice(begin, end);
    }
    Utf16String substring(ca::usize begin, ca::usize end) const
    {
        return ref().substring(begin, end);
    }

    ca::usize index_of(Char16 ch) const noexcept { return ref().index_of(ch); }
    ca::usize index_of(Char16 ch, ca::usize from) const noexcept
    {
        return ref().index_of(ch, from);
    }
    ca::usize index_of(const Utf16StringRef& needle) const noexcept
    {
        return ref().index_of(needle);
    }
    ca::usize index_of(const Utf16StringRef& needle, ca::usize from) const noexcept
    {
        return ref().index_of(needle, from);
    }
    ca::usize last_index_of(Char16 ch) const noexcept { return ref().last_index_of(ch); }
    ca::usize last_index_of(Char16 ch, ca::usize from) const noexcept
    {
        return ref().last_index_of(ch, from);
    }
    ca::usize last_index_of(const Utf16StringRef& needle) const noexcept
    {
        return ref().last_index_of(needle);
    }
    ca::usize last_index_of(const Utf16StringRef& needle, ca::usize from) const noexcept
    {
        return ref().last_index_of(needle, from);
    }
    bool starts_with(const Utf16StringRef& prefix) const noexcept
    {
        return ref().starts_with(prefix);
    }
    bool starts_with(const Utf16StringRef& prefix, ca::usize offset) const noexcept
    {
        return ref().starts_with(prefix, offset);
    }
    bool ends_with(const Utf16StringRef& suffix) const noexcept { return ref().ends_with(suffix); }
    bool contains(const Utf16StringRef& needle) const noexcept { return ref().contains(needle); }

    int     compare(const Utf16StringRef& other) const noexcept { return ref().compare(other); }
    int     compare_to(const Utf16StringRef& other) const noexcept { return compare(other); }
    bool    equals(const Utf16StringRef& other) const noexcept { return ref().equals(other); }
    ca::i32 hash_code() const noexcept { return ref().hash_code(); }

    Utf16String    concat(const Utf16StringRef& other) const { return ref().concat(other); }
    Utf16String    to_string() const { return clone(); }
    std::u16string to_std_u16_string() const { return ref().to_std_u16_string(); }
    Utf8String     to_utf8_string() const { return ref().to_utf8_string(); }

    bool operator==(const Utf16StringRef& other) const noexcept { return equals(other); }
    bool operator!=(const Utf16StringRef& other) const noexcept { return !equals(other); }
    bool operator==(const Utf16String& other) const noexcept { return equals(other.ref()); }
    bool operator!=(const Utf16String& other) const noexcept { return !equals(other.ref()); }

private:
    Char16*   data_;
    ca::usize length_;
};

/// @brief 可变 UTF-16 字符串构建器。
///
/// `Utf16StringBuilder` 使用 UTF-16 code unit 作为索引单位。它不承担 Java 对象
/// 布局或异常策略：越界读取返回空值，部分越界修改是 no-op 或夹到末尾；需要
/// Java 异常时应由翻译器 runtime 外层包装。
class Utf16StringBuilder
{
public:
    Utf16StringBuilder() = default;

    /// @brief 追加 Char16、数字、布尔、UTF-8 或 UTF-16 文本。
    Utf16StringBuilder& append(Char16 ch);
    Utf16StringBuilder& append(bool value);
    Utf16StringBuilder& append(ca::i32 value);
    Utf16StringBuilder& append(ca::i64 value);
    Utf16StringBuilder& append(ca::f32 value);
    Utf16StringBuilder& append(ca::f64 value);
    Utf16StringBuilder& append(const char* utf8);
    Utf16StringBuilder& append(const Utf8StringRef& utf8);
    Utf16StringBuilder& append(const Utf16StringRef& str);
    Utf16StringBuilder& append(const Utf16String& str);
    Utf16StringBuilder& append(const ca::u16* data, ca::usize length);
    /// @brief 追加 Unicode code point；非法 code point 返回 false。
    bool append_code_point(ca::u32 code_point);

    /// @brief 返回 index 位置的 code unit；越界返回 0。
    Char16 char_at(ca::usize index) const noexcept;

    /// @brief 返回 index 位置的 code point；有效 surrogate pair 会被合并。
    ca::u32 code_point_at(ca::usize index) const noexcept;

    /// @brief 设置 index 位置的 code unit；越界时 no-op。
    Utf16StringBuilder& set_char_at(ca::usize index, Char16 ch) noexcept;

    /// @brief 在 index 位置前插入文本；index 大于长度时插入到末尾。
    Utf16StringBuilder& insert(ca::usize index, Char16 ch);
    Utf16StringBuilder& insert(ca::usize index, bool value);
    Utf16StringBuilder& insert(ca::usize index, ca::i32 value);
    Utf16StringBuilder& insert(ca::usize index, ca::i64 value);
    Utf16StringBuilder& insert(ca::usize index, ca::f32 value);
    Utf16StringBuilder& insert(ca::usize index, ca::f64 value);
    Utf16StringBuilder& insert(ca::usize index, const char* utf8);
    Utf16StringBuilder& insert(ca::usize index, const Utf8StringRef& utf8);
    Utf16StringBuilder& insert(ca::usize index, const Utf16StringRef& str);
    Utf16StringBuilder& insert(ca::usize index, const Utf16String& str);
    /// @brief 删除 `[begin, end)` code unit 范围；越界范围会被夹到当前长度。
    Utf16StringBuilder& delete_range(ca::usize begin, ca::usize end);

    /// @brief 删除单个 code unit；越界时 no-op。
    Utf16StringBuilder& delete_char_at(ca::usize index);

    /// @brief 原地反转 code unit 序列，并尽量保持有效 surrogate pair 不被拆开。
    Utf16StringBuilder& reverse();

    /// @brief 预留容量，不改变长度。
    void reserve(ca::usize capacity);

    /// @brief 截断到 length；length 大于当前长度时不增长。
    void truncate(ca::usize length) noexcept;

    /// @brief 调整 code unit 长度；增长部分用 fill 填充。
    void resize(ca::usize length, Char16 fill = Char16());

    /// @brief 返回当前 UTF-16 code unit 数量。
    ca::usize length() const noexcept { return static_cast<ca::usize>(buffer_.size()); }
    ca::usize capacity() const noexcept { return static_cast<ca::usize>(buffer_.capacity()); }
    bool      is_empty() const noexcept { return buffer_.empty(); }
    /// @brief 清空 builder 内容。
    void clear() noexcept { buffer_.clear(); }

    /// @brief 构造不可变拥有型字符串快照。
    Utf16String build() const;

    /// @brief `build()` 的命名别名。
    Utf16String to_string() const { return build(); }

private:
    std::vector<Char16> buffer_;
};

bool operator==(const Utf16StringRef& lhs, const Utf16String& rhs) noexcept;
bool operator!=(const Utf16StringRef& lhs, const Utf16String& rhs) noexcept;
bool operator<(const Utf16StringRef& lhs, const Utf16StringRef& rhs) noexcept;
bool operator>(const Utf16StringRef& lhs, const Utf16StringRef& rhs) noexcept;
bool operator<=(const Utf16StringRef& lhs, const Utf16StringRef& rhs) noexcept;
bool operator>=(const Utf16StringRef& lhs, const Utf16StringRef& rhs) noexcept;

std::ostream& operator<<(std::ostream& os, Char16 ch);
std::ostream& operator<<(std::ostream& os, const Utf16StringRef& str);
std::ostream& operator<<(std::ostream& os, const Utf16String& str);

}   // namespace ca::str

namespace std {

template<>
struct hash<ca::str::Char16>
{
    size_t operator()(ca::str::Char16 ch) const noexcept;
};

template<>
struct hash<ca::str::Utf16StringRef>
{
    size_t operator()(const ca::str::Utf16StringRef& str) const noexcept;
};

template<>
struct hash<ca::str::Utf16String>
{
    size_t operator()(const ca::str::Utf16String& str) const noexcept;
};

}   // namespace std
