//
// @brief 不可变 UTF-8 字符串 (Utf8String) 及引用视图 (Utf8StringRef)
// @author Canrad
// @date 2026/05/31
// @note 命名空间 ca::str，基于 u8 类型存储，支持码点访问
//

#ifndef LIBCA_STR_UTF8_STRING_HPP
#define LIBCA_STR_UTF8_STRING_HPP

#include "libca/core/datatype.hpp"

#include "utf8_util.hpp"

#include <cstddef>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

namespace ca::str {

// ============================================================================
// 前向声明
// ============================================================================

class Utf8String;
class Utf8Iterator;


// ============================================================================
// Utf8StringRef — 非拥有引用的不可变 UTF-8 字符串视图
// ============================================================================

class Utf8StringRef {
public:
    // 默认构造：空视图
    Utf8StringRef() noexcept;

    // 从字节数据 + 码点个数构造（不校验，不复制）
    // data: UTF-8 字节数据指针
    // byteLength: 字节长度
    // length: 码点个数
    Utf8StringRef(const u8* data, usize byteLength, usize length) noexcept;

    // 从 Utf8String 构造视图
    Utf8StringRef(const Utf8String& str) noexcept;

    // 从 C 字符串构造视图（O(n) 计算码点个数）
    static Utf8StringRef fromCStr(const char* cstr) noexcept;

    // 特殊常量
    static constexpr usize npos = usize(-1);

    // ---- 查询 ----

    // 码点个数（O(1)）
    usize length() const noexcept;

    // 字节长度（O(1)）
    usize byteLength() const noexcept;

    // 是否为空字符串
    bool isEmpty() const noexcept;

    // 原始字节数据指针（非空终止）
    const u8* data() const noexcept;

    // ---- 访问 ----

    // 按字节下标访问（O(1)），不进行边界检查
    u8 byteAt(usize index) const;

    // 按码点下标访问（O(n)），不进行边界检查
    u32 codePointAt(usize index) const;

    // ---- 切片（返回 Utf8StringRef，不分配内存） ----

    // 按字节区间 [byteStart, byteEnd) 切片
    Utf8StringRef slice(usize byteStart, usize byteEnd) const;

    // 按码点区间切片：从第 cpStart 个码点开始，取 cpCount 个码点
    Utf8StringRef sliceByCp(usize cpStart, usize cpCount) const;

    // ---- 子串（返回 Utf8String，分配内存） ----

    // 按码点取子串：从第 cpStart 个码点开始，取 cpCount 个码点
    Utf8String substr(usize cpStart, usize cpCount) const;

    // ---- 前缀/后缀检查（字节级，O(n)）----

    bool startsWith(const Utf8StringRef& prefix) const noexcept;
    bool endsWith(const Utf8StringRef& suffix) const noexcept;

    // ---- 修剪（返回视图，不分配） ----

    Utf8StringRef trim() const noexcept;
    Utf8StringRef trimStart() const noexcept;
    Utf8StringRef trimEnd() const noexcept;

    // ---- 拆分 ----

    std::vector<Utf8StringRef> split(const Utf8StringRef& delimiter) const;

    // ---- 大小写转换（返回 Utf8String，分配） ----

    Utf8String toLower() const;
    Utf8String toUpper() const;

    // ---- 替换 ----

    Utf8String replaceAll(const Utf8StringRef& from, const Utf8StringRef& to) const;

    // ---- 查找 ----

    // 查找子串首次出现的码点下标，未找到返回 npos
    usize indexOf(const Utf8StringRef& needle) const noexcept;
    usize indexOf(const Utf8StringRef& needle, usize startCp) const noexcept;
    usize indexOf(u32 codePoint) const noexcept;
    usize indexOf(u32 codePoint, usize startCp) const noexcept;

    // 是否包含子串
    bool contains(const Utf8StringRef& needle) const noexcept;

    // ---- 迭代器 ----

    Utf8Iterator begin() const noexcept;
    Utf8Iterator end() const noexcept;

    // ---- 比较 ----

    // 逐字节字典序比较
    int compare(const Utf8StringRef& other) const noexcept;

    // 内容相等判断（逐字节比较）
    bool equals(const Utf8StringRef& other) const noexcept;

    bool operator==(const Utf8StringRef& other) const noexcept;
    bool operator!=(const Utf8StringRef& other) const noexcept;

private:
    const u8* data_;
    usize     byteLength_;
    usize     length_;   // 码点个数
};


// ============================================================================
// Utf8String — 拥有所有权的不可变 UTF-8 字符串
// ============================================================================

class Utf8String {
public:
    // ---- 构造 / 析构 ----

    // 默认构造：空字符串
    Utf8String() noexcept;

    // 从字节数组构造（复制 + 校验 UTF-8 合法性）
    // data: UTF-8 字节数据（不必以 '\0' 结尾）
    // byteLength: 字节长度
    Utf8String(const u8* data, usize byteLength);

    // 从 C 风格字符串构造（null-terminated）
    explicit Utf8String(const char* cstr);

    // 拷贝构造（已删除，请使用 clone()）
    Utf8String(const Utf8String& other) = delete;

    // 移动构造
    Utf8String(Utf8String&& other) noexcept;

    // 析构
    ~Utf8String();

    // 拷贝赋值（已删除，请使用 clone()）
    Utf8String& operator=(const Utf8String& other) = delete;

    // 移动赋值
    Utf8String& operator=(Utf8String&& other) noexcept;

    // 显式克隆（唯一复制方式）
    Utf8String clone() const;

    // ---- 工厂方法 ----

    // 从单个码点创建 UTF-8 字符串
    static Utf8String fromCodePoint(u32 cp);

    // 从字节数组创建（同构造函数语义）
    static Utf8String fromUtf8(const u8* data, usize byteLength);

    // ---- 查询 ----

    // 码点个数（O(1)）
    usize length() const noexcept;

    // 字节长度（O(1)）
    usize byteLength() const noexcept;

    // 是否为空字符串
    bool isEmpty() const noexcept;

    // STL 兼容别名
    usize size() const noexcept { return length_; }
    bool empty() const noexcept { return byteLength_ == 0; }

    // 原始字节数据指针（内部存储以 '\0' 结尾）
    const u8* data() const noexcept;

    // C 风格字符串（O(1)，内部已有 '\0' 终止符）
    const char* cStr() const noexcept;

    // ---- 访问 ----

    // 按字节下标访问（O(1)），不进行边界检查
    u8 byteAt(usize index) const;

    // 按码点下标访问（O(n) 扫描），不进行边界检查
    u32 codePointAt(usize index) const;

    // ---- 视图 / 切片 ----

    // 获取整个字符串的 Utf8StringRef 视图
    Utf8StringRef ref() const noexcept;

    // 按字节区间 [byteStart, byteEnd) 切片，返回非拥有视图
    Utf8StringRef slice(usize byteStart, usize byteEnd) const;

    // 按码点区间切片：从第 cpStart 个码点开始，取 cpCount 个码点
    Utf8StringRef sliceByCp(usize cpStart, usize cpCount) const;

    // ---- 迭代器 ----

    Utf8Iterator begin() const noexcept;
    Utf8Iterator end() const noexcept;

    // ---- 子串 ----

    // 按码点取子串：从第 cpStart 个码点开始，取 cpCount 个码点
    Utf8String substr(usize cpStart, usize cpCount) const;

    // ---- 前缀/后缀 ----

    bool startsWith(const Utf8StringRef& prefix) const noexcept;
    bool endsWith(const Utf8StringRef& suffix) const noexcept;

    // ---- 修剪 ----

    Utf8StringRef trim() const noexcept;
    Utf8StringRef trimStart() const noexcept;
    Utf8StringRef trimEnd() const noexcept;

    // ---- 拆分 ----

    std::vector<Utf8StringRef> split(const Utf8StringRef& delimiter) const;

    // ---- 大小写转换 ----

    Utf8String toLower() const;
    Utf8String toUpper() const;

    // ---- 替换 ----

    Utf8String replaceAll(const Utf8StringRef& from, const Utf8StringRef& to) const;

    // ---- 查找 ----

    usize indexOf(const Utf8StringRef& needle) const noexcept;
    usize indexOf(const Utf8StringRef& needle, usize startCp) const noexcept;
    usize indexOf(u32 codePoint) const noexcept;
    usize indexOf(u32 codePoint, usize startCp) const noexcept;
    bool contains(const Utf8StringRef& needle) const noexcept;

    // ---- 比较 ----

    int compare(const Utf8StringRef& other) const noexcept;
    int compare(const Utf8String& other) const noexcept;
    bool equals(const Utf8StringRef& other) const noexcept;
    bool operator==(const Utf8String& other) const noexcept;
    bool operator==(const Utf8StringRef& other) const noexcept;
    bool operator!=(const Utf8String& other) const noexcept;
    bool operator!=(const Utf8StringRef& other) const noexcept;

private:
    u8*   data_;
    usize byteLength_;
    usize length_;   // 码点个数（缓存）

    // 内部初始化：从 src 复制 byteLen 字节，校验 UTF-8，计算码点个数
    void init(const u8* src, usize byteLen);
};


// ============================================================================
// Utf8StringBuilder — 用于构建 Utf8String 的可变构建器
// ============================================================================

class Utf8StringBuilder {
public:
    Utf8StringBuilder() noexcept;
    Utf8StringBuilder(Utf8StringBuilder&& other) noexcept;
    ~Utf8StringBuilder();

    Utf8StringBuilder& operator=(Utf8StringBuilder&& other) noexcept;

    Utf8StringBuilder(const Utf8StringBuilder&) = delete;
    Utf8StringBuilder& operator=(const Utf8StringBuilder&) = delete;

    Utf8StringBuilder& append(const Utf8StringRef& str);
    Utf8StringBuilder& append(const Utf8String& str);
    Utf8StringBuilder& append(const char* cstr);
    Utf8StringBuilder& append(const u8* data, usize byteLength);
    bool appendCodePoint(u32 cp);

    void reserve(usize byteCapacity);
    usize capacity() const noexcept;
    usize byteLength() const noexcept;
    bool isEmpty() const noexcept;
    void clear() noexcept;

    Utf8String build() const;
    Utf8String buildOrEmpty() const noexcept;

private:
    u8*   buffer_;
    usize byteLength_;
    usize capacity_;
    static constexpr usize kDefaultCapacity = 64;
    void grow(usize minCapacity);
};


// ============================================================================
// Utf8Iterator — 前向迭代器，O(1) 步进，配合范围 for 使用
// ============================================================================

class Utf8Iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = u32;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const u32*;
    using reference         = u32;

    Utf8Iterator() noexcept : pos_(nullptr), end_(nullptr) {}
    Utf8Iterator(const u8* pos, const u8* end) noexcept : pos_(pos), end_(end) {}

    u32 operator*() const noexcept {
        return utf8DecodeCodePoint(pos_);
    }

    Utf8Iterator& operator++() noexcept {
        pos_ += utf8CodePointBytesSafe(*pos_);
        return *this;
    }

    Utf8Iterator operator++(int) noexcept {
        Utf8Iterator tmp = *this;
        pos_ += utf8CodePointBytesSafe(*pos_);
        return tmp;
    }

    bool operator==(const Utf8Iterator& other) const noexcept {
        return pos_ == other.pos_;
    }

    bool operator!=(const Utf8Iterator& other) const noexcept {
        return pos_ != other.pos_;
    }

    const u8* bytePtr() const noexcept { return pos_; }

private:
    const u8* pos_;
    const u8* end_;
};


// ============================================================================
// 非成员比较运算符（对称比较）
// ============================================================================

bool operator==(const Utf8StringRef& lhs, const Utf8String& rhs) noexcept;
bool operator!=(const Utf8StringRef& lhs, const Utf8String& rhs) noexcept;

// ============================================================================
// 自由函数
// ============================================================================

namespace literals {

inline Utf8StringRef operator""_utf8_ref(const char* str, usize len) noexcept {
    return Utf8StringRef(reinterpret_cast<const u8*>(str), len,
                         utf8CountCodePoints(reinterpret_cast<const u8*>(str), len));
}

inline Utf8String operator""_utf8(const char* str, usize len) {
    return Utf8String(reinterpret_cast<const u8*>(str), len);
}

}  // namespace literals

/// 按分隔符拆分为视图列表
std::vector<Utf8StringRef> split(const Utf8StringRef& str,
                                 const Utf8StringRef& delimiter);

/// 用分隔符连接多个字符串
Utf8String join(const std::vector<Utf8StringRef>& parts,
                const Utf8StringRef& separator);

/// 流输出
std::ostream& operator<<(std::ostream& os, const Utf8StringRef& s);
std::ostream& operator<<(std::ostream& os, const Utf8String& s);

}  // namespace ca::str


// ============================================================================
// std::hash 特化
// ============================================================================

namespace std {

template <>
struct hash<ca::str::Utf8String> {
    size_t operator()(const ca::str::Utf8String& s) const noexcept;
};

template <>
struct hash<ca::str::Utf8StringRef> {
    size_t operator()(const ca::str::Utf8StringRef& s) const noexcept;
};

}  // namespace std

#endif  // LIBCA_STR_UTF8_STRING_HPP
